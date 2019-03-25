#include <stdio.h>
#include <stdlib.h>

#include <memory>
#include <string>
#include <vector>

#include "betting_abstraction.h"
#include "betting_tree.h"
#include "board_tree.h"
#include "buckets.h"
#include "canonical_cards.h"
#include "card_abstraction.h"
#include "cards.h"
#include "cfr_config.h"
#include "cfr_utils.h"
#include "cfr_values.h"
#include "files.h"
#include "game.h"
#include "hand_tree.h"
#include "io.h"
#include "mp_vcfr.h"

using std::string;
using std::unique_ptr;
using std::vector;

MPVCFR::MPVCFR(const CardAbstraction &ca, const BettingAbstraction &ba, const CFRConfig &cc,
	       const Buckets &buckets, const BettingTree *betting_tree, int num_threads) :
  card_abstraction_(ca), betting_abstraction_(ba), cfr_config_(cc),
  buckets_(buckets), betting_tree_(betting_tree) {
  num_threads_ = num_threads;

  int max_street = Game::MaxStreet();

  compressed_streets_ = new bool[max_street + 1];
  for (int st = 0; st <= max_street; ++st) {
    compressed_streets_[st] = false;
  }
  const vector<int> &csv = cfr_config_.CompressedStreets();
  int num_csv = csv.size();
  for (int i = 0; i < num_csv; ++i) {
    int st = csv[i];
    compressed_streets_[st] = true;
  }

  int num_players = Game::NumPlayers();
  sumprob_streets_ = new bool *[num_players];
  for (int p = 0; p < num_players; ++p) {
    sumprob_streets_[p] = new bool[max_street + 1];
    for (int st = 0; st <= max_street; ++st) {
      // Can't control this by player currently
      // sumprob_streets_[p][st] = cfr_config_.SumprobStreet(p, st);
      sumprob_streets_[p][st] = cfr_config_.SumprobStreets()[st];
    }
  }

  regret_floors_ = new int[max_street + 1];
  const vector<int> &fv = cfr_config_.RegretFloors();
  if (fv.size() == 0) {
    for (int s = 0; s <= max_street; ++s) {
      regret_floors_[s] = 0;
    }
  } else {
    if ((int)fv.size() < max_street + 1) {
      fprintf(stderr, "Regret floor vector too small\n");
      exit(-1);
    }
    for (int s = 0; s <= max_street; ++s) {
      if (fv[s] == 1) regret_floors_[s] = kMinInt;
      else            regret_floors_[s] = fv[s];
    }
  }

  best_response_streets_.reset(new bool[max_street + 1]);
  for (int st = 0; st <= max_street; ++st) {
    best_response_streets_[st] = false;
  }
}

double *MPVCFR::OurChoice(Node *node, int lbd, int last_bet_to, int *contributions,
			  double **opp_probs, int **street_buckets,
			  const string &action_sequence) {
  int st = node->Street();
  int num_succs = node->NumSuccs();
  int num_hole_card_pairs = Game::NumHoleCardPairs(st);
  double *vals = nullptr;
  double **succ_vals = new double *[num_succs];
  int csi = node->CallSuccIndex();
  for (int s = 0; s < num_succs; ++s) {
    string action = node->ActionName(s);
    int old_contribution = 0;
    if (s == csi) {
      old_contribution = contributions[p_];
      contributions[p_] = last_bet_to;
    }
    succ_vals[s] = Process(node->IthSucc(s), lbd, last_bet_to, contributions, opp_probs,
			   street_buckets, action_sequence + action, st);
    if (s == csi) {
      contributions[p_] = old_contribution;
    }
  }
  if (num_succs == 1) {
    vals = succ_vals[0];
    succ_vals[0] = nullptr;
  } else {
    vals = new double[num_hole_card_pairs];
    for (int i = 0; i < num_hole_card_pairs; ++i) vals[i] = 0;
    if (best_response_streets_[st]) {
      for (int i = 0; i < num_hole_card_pairs; ++i) {
	double max_val = succ_vals[0][i];
	for (int s = 1; s < num_succs; ++s) {
	  double sv = succ_vals[s][i];
	  if (sv > max_val) max_val = sv;
	}
	vals[i] = max_val;
      }
    } else {
      exit(-1);
    }
  }
  return vals;
}

double *MPVCFR::OppChoice(Node *node, int lbd, int last_bet_to, int *contributions,
			  double **opp_probs, int **street_buckets,
			  const string &action_sequence) {
  int st = node->Street();
  int num_succs = node->NumSuccs();
  int pa = node->PlayerActing();
  int num_hole_card_pairs = Game::NumHoleCardPairs(st);
  const CanonicalCards *hands = hand_tree_->Hands(st, lbd);
  int num_players = Game::NumPlayers();
    
  double **pa_succ_opp_probs = new double *[num_succs];
  if (num_succs == 1) {
    pa_succ_opp_probs[0] = opp_probs[pa];
  } else {
    int nt = node->NonterminalID();
    int num_hole_cards = Game::NumCardsForStreet(0);
    int max_card1 = Game::MaxCard() + 1;
    int num_enc;
    if (num_hole_cards == 1) num_enc = max_card1;
    else                     num_enc = max_card1 * max_card1;
    for (int s = 0; s < num_succs; ++s) {
      pa_succ_opp_probs[s] = new double[num_enc];
      for (int i = 0; i < num_enc; ++i) pa_succ_opp_probs[s][i] = 0;
    }

    // The "all" values point to the values for all hands.
    double *d_all_current_probs = nullptr;
    double *d_all_cs_vals = nullptr;
    int *i_all_cs_vals = nullptr;
    
    double explore;
    if (value_calculation_ && ! br_current_) explore = 0;
    else                                     explore = explore_;

    bool bucketed = ! buckets_.None(st) &&
      node->LastBetTo() < card_abstraction_.BucketThreshold(st);

    if (bucketed) {
      fprintf(stderr, "Not supported yet\n");
      exit(-1);
      // current_strategy_->Values(pa, st, nt, &d_all_current_probs);
    } else {
      // cs_vals are the current strategy values; i.e., the values we pass into
      // RegretsToProbs() in order to get the current strategy.  In VCFR, these
      // are regrets; in a best-response calculation, they are (normally)
      // sumprobs.

      // Such a mess!
      AbstractCFRStreetValues *cs_values;
      if (value_calculation_ && ! br_current_) {
	cs_values = sumprobs_->StreetValues(st);
      } else {
	cs_values = regrets_->StreetValues(st);
      }
      CFRStreetValues<double> *d_cs_values, *d_sumprob_values = nullptr;
      CFRStreetValues<int> *i_cs_values, *i_sumprob_values = nullptr;
      AbstractCFRStreetValues *sumprob_values = nullptr;
      if (sumprobs_ && sumprob_streets_[st]) {
	sumprob_values = sumprobs_->StreetValues(st);
	if (sumprob_values == nullptr) {
	  fprintf(stderr, "No sumprobs values for street %u?!?\n", st);
	  exit(-1);
	}
	if ((d_sumprob_values =
	     dynamic_cast<CFRStreetValues<double> *>(sumprob_values)) ==
	    nullptr) {
	  if ((i_sumprob_values =
	       dynamic_cast<CFRStreetValues<int> *>(sumprob_values)) ==
	      nullptr) {
	    fprintf(stderr, "Neither int nor double sumprobs?!?\n");
	    exit(-1);
	  }
	}
      }
      if ((d_cs_values =
	   dynamic_cast<CFRStreetValues<double> *>(cs_values))) {
	if (d_sumprob_values) {
	  ProcessOppProbs(node, lbd, hands, bucketed, street_buckets, opp_probs, succ_opp_probs,
			  *d_cs_values, dsi, it_, soft_warmup_, hard_warmup_, sumprob_scaling_[st],
			  d_sumprob_values);
	} else {
	  ProcessOppProbs(node, lbd, hands, bucketed, street_buckets, opp_probs, succ_opp_probs,
			  *d_cs_values, dsi, it_, soft_warmup_, hard_warmup_, sumprob_scaling_[st],
			  i_sumprob_values);
	}
      } else {
	i_cs_values = dynamic_cast<CFRStreetValues<int> *>(cs_values);
	if (i_cs_values == nullptr) {
	  fprintf(stderr, "Neither int nor double cs values?!?\n");
	  exit(-1);
	}
	if (d_sumprob_values) {
	  ProcessOppProbs(node, lbd, hands, bucketed, street_buckets, opp_probs, succ_opp_probs,
			  *i_cs_values, dsi, it_, soft_warmup_, hard_warmup_, sumprob_scaling_[st],
			  d_sumprob_values);
	} else {
	  ProcessOppProbs(node, lbd, hands, bucketed, street_buckets, opp_probs, succ_opp_probs,
			  *i_cs_values, dsi, it_, soft_warmup_, hard_warmup_, sumprob_scaling_[st],
			  i_sumprob_values);
	}
      }

#if 0
      // Old code from slumbot2017
      if (value_calculation_ && ! br_current_) {
	if (sumprobs_->Ints(pa, st)) {
	  sumprobs_->Values(pa, st, nt, &i_all_cs_vals);
	} else {
	  sumprobs_->Values(pa, st, nt, &d_all_cs_vals);
	}
      } else {
	if (regrets_->Ints(pa, st)) {
	  regrets_->Values(pa, st, nt, &i_all_cs_vals);
	} else {
	  regrets_->Values(pa, st, nt, &d_all_cs_vals);
	}
      }
#endif
    }

    // The "all" values point to the values for all hands.
    double *d_all_sumprobs = nullptr;
    int *i_all_sumprobs = nullptr;
    // sumprobs_->Players(pa) check is there because in asymmetric systems
    // (e.g., endgame solving with CFR-D method) we are only saving probs for
    // one player.
    if (! value_calculation_ && sumprob_streets_[pa][st] &&
	sumprobs_->Players(pa)) {
      if (sumprobs_->Ints(pa, st)) {
	sumprobs_->Values(pa, st, nt, &i_all_sumprobs);
      } else {
	sumprobs_->Values(pa, st, nt, &d_all_sumprobs);
      }
    }

    // These values will point to the values for the current board
    double *d_cs_vals = nullptr, *d_sumprobs = nullptr;
    int *i_cs_vals = nullptr, *i_sumprobs = nullptr;

    if (bucketed) {
      i_sumprobs = i_all_sumprobs;
      d_sumprobs = d_all_sumprobs;
    } else {
      if (i_all_cs_vals) {
	i_cs_vals = i_all_cs_vals + lbd * num_hole_card_pairs * num_succs;
      } else {
	d_cs_vals = d_all_cs_vals + lbd * num_hole_card_pairs * num_succs;
      }
      if (i_all_sumprobs) {
	i_sumprobs = i_all_sumprobs + lbd * num_hole_card_pairs * num_succs;
      }
      if (d_all_sumprobs) {
	d_sumprobs = d_all_sumprobs + lbd * num_hole_card_pairs * num_succs;
      }
    }

    bool nonneg;
    if (value_calculation_ && ! br_current_) {
      nonneg = true;
    } else {
      nonneg = nn_regrets_ && regret_floors_[st] >= 0;
    }
    // No sumprob update if a) doing value calculation (e.g., RGBR), b)
    // we have no sumprobs (e.g., in asymmetric CFR), or c) we are during
    // the hard warmup period
    if (bucketed) {
      if (d_sumprobs) {
	// Double sumprobs
	bool update_sumprobs =
	  ! (value_calculation_ || d_sumprobs == nullptr ||
	     (hard_warmup_ > 0 && it_ <= hard_warmup_));
	ProcessOppProbsBucketed(node, street_buckets, hands, nonneg, it_, soft_warmup_,
				hard_warmup_, update_sumprobs, opp_probs[pa], pa_succ_opp_probs,
				d_all_current_probs, d_sumprobs);
      } else {
	// Int sumprobs
	bool update_sumprobs =
	  ! (value_calculation_ || i_sumprobs == nullptr ||
	     (hard_warmup_ > 0 && it_ <= hard_warmup_));
	ProcessOppProbsBucketed(node, street_buckets, hands, nonneg, it_, soft_warmup_,
				hard_warmup_, update_sumprobs, sumprob_scaling_, opp_probs[pa],
				pa_succ_opp_probs, d_all_current_probs, i_sumprobs);
      }
    } else {
      if (i_cs_vals) {
	if (d_sumprobs) {
	  // Int regrets, double sumprobs
	  bool update_sumprobs =
	    ! (value_calculation_ || d_sumprobs == nullptr ||
	       (hard_warmup_ > 0 && it_ <= hard_warmup_));
	  ProcessOppProbs(node, hands, bucketed, street_buckets, nonneg, uniform_, explore, it_,
			  soft_warmup_, hard_warmup_, update_sumprobs, opp_probs[pa],
			  pa_succ_opp_probs, i_cs_vals, d_sumprobs);
	} else {
	  // Int regrets and sumprobs
	  bool update_sumprobs =
	    ! (value_calculation_ || i_sumprobs == nullptr ||
	       (hard_warmup_ > 0 && it_ <= hard_warmup_));
	  ProcessOppProbs(node, hands, bucketed, street_buckets, nonneg, uniform_, explore,
			  ProbMethod::REGRET_MATCHING, it_, soft_warmup_, hard_warmup_,
			  update_sumprobs, sumprob_scaling_, opp_probs[pa], pa_succ_opp_probs,
			  i_cs_vals, i_sumprobs);
	}
      } else {
	// Double regrets and sumprobs
	  bool update_sumprobs =
	    ! (value_calculation_ || d_sumprobs == nullptr ||
	       (hard_warmup_ > 0 && it_ <= hard_warmup_));
	  ProcessOppProbs(node, hands, bucketed, street_buckets, nonneg, uniform_, explore, it_,
			  soft_warmup_, hard_warmup_, update_sumprobs, opp_probs[pa],
			  pa_succ_opp_probs, d_cs_vals, d_sumprobs);
      }
    }
  }

  double *vals = nullptr;
  for (int s = 0; s < num_succs; ++s) {
    string action = node->ActionName(s);
    double **succ_opp_probs = new double *[num_players];
    for (int p = 0; p < num_players; ++p) {
      if (p == pa) {
	succ_opp_probs[p] = pa_succ_opp_probs[s];
      } else {
	succ_opp_probs[p] = opp_probs[p];
      }
    }
    double *succ_vals = Process(node->IthSucc(s), lbd, last_bet_to, contributions, succ_opp_probs,
				street_buckets, action_sequence + action, st);
    delete [] succ_opp_probs;
    if (vals == nullptr) {
      vals = succ_vals;
    } else {
      for (int i = 0; i < num_hole_card_pairs; ++i) {
	vals[i] += succ_vals[i];
      }
      delete [] succ_vals;
    }
  }
  if (vals == nullptr) {
    // This can happen if there were non-zero opp probs on the prior street,
    // but the board cards just dealt blocked all the opponent hands with
    // non-zero probability.
    vals = new double[num_hole_card_pairs];
    for (int i = 0; i < num_hole_card_pairs; ++i) vals[i] = 0;
  }
  
  if (num_succs > 1) {
    for (int s = 0; s < num_succs; ++s) {
      delete [] pa_succ_opp_probs[s];
    }
  }
  delete [] pa_succ_opp_probs;

  return vals;
}

class MPVCFRThread {
public:
  MPVCFRThread(MPVCFR *vcfr, int thread_index, int num_threads, Node *node, int last_bet_to,
	       int *contributions, const string &action_sequence, double **opp_probs,
	       int *prev_canons);
  ~MPVCFRThread(void);
  void Run(void);
  void Join(void);
  void Go(void);
  double *RetVals(void) const {return ret_vals_;}
private:
  MPVCFR *vcfr_;
  int thread_index_;
  int num_threads_;
  Node *node_;
  int last_bet_to_;
  int *contributions_;
  const string &action_sequence_;
  double **opp_probs_;
  int *prev_canons_;
  int **street_buckets_;
  double *ret_vals_;
  pthread_t pthread_id_;
};

MPVCFRThread::MPVCFRThread(MPVCFR *vcfr, int thread_index, int num_threads, Node *node,
			   int last_bet_to, int *contributions, const string &action_sequence,
			   double **opp_probs, int *prev_canons) :
  action_sequence_(action_sequence) {
  vcfr_ = vcfr;
  thread_index_ = thread_index;
  num_threads_ = num_threads;
  node_ = node;
  last_bet_to_ = last_bet_to;
  contributions_ = contributions;
  opp_probs_ = opp_probs;
  prev_canons_ = prev_canons;
  int max_street = Game::MaxStreet();
  street_buckets_ = new int *[max_street + 1];
  for (int st = 0; st <= max_street; ++st) {
    int num_hole_card_pairs = Game::NumHoleCardPairs(st);
    street_buckets_[st] = new int[num_hole_card_pairs];
  }
}

MPVCFRThread::~MPVCFRThread(void) {
  delete [] ret_vals_;
  int max_street = Game::MaxStreet();
  for (int st = 0; st <= max_street; ++st) {
    delete [] street_buckets_[st];
  }
  delete [] street_buckets_;
}

static void *mpvcfr_thread_run(void *v_t) {
  MPVCFRThread *t = (MPVCFRThread *)v_t;
  t->Go();
  return NULL;
}

void MPVCFRThread::Run(void) {
  pthread_create(&pthread_id_, NULL, mpvcfr_thread_run, this);
}

void MPVCFRThread::Join(void) {
  pthread_join(pthread_id_, NULL); 
}

void MPVCFRThread::Go(void) {
  int st = node_->Street();
  int num_boards = BoardTree::NumBoards(st);
  int num_hole_card_pairs = Game::NumHoleCardPairs(st);
  Card max_card1 = Game::MaxCard() + 1;
  HandTree *hand_tree = vcfr_->GetHandTree();
  ret_vals_ = new double[num_hole_card_pairs];
  for (int i = 0; i < num_hole_card_pairs; ++i) ret_vals_[i] = 0;
  for (int bd = thread_index_; bd < num_boards; bd += num_threads_) {
    // Wait, each thread will overwrite street buckets.
    // Initialize buckets for this street
    vcfr_->SetStreetBuckets(st, bd, street_buckets_);
    double *bd_vals = vcfr_->Process(node_, bd, last_bet_to_, contributions_,
				     opp_probs_, street_buckets_,
				     action_sequence_, st);
    const CanonicalCards *hands = hand_tree->Hands(st, bd);
    int board_variants = BoardTree::NumVariants(st, bd);
    int num_hands = hands->NumRaw();
    for (int h = 0; h < num_hands; ++h) {
      const Card *cards = hands->Cards(h);
      Card hi = cards[0];
      Card lo = cards[1];
      int enc = hi * max_card1 + lo;
      int prev_canon = prev_canons_[enc];
      ret_vals_[prev_canon] += board_variants * bd_vals[h];
    }
    delete [] bd_vals;
  }
}

// Same as VCFR implementation; should share
int **MPVCFR::InitializeStreetBuckets(void) {
  int max_street = Game::MaxStreet();
  int **street_buckets = new int *[max_street + 1];
  for (int st = 0; st <= max_street; ++st) {
    if (buckets_.None(st)) {
      street_buckets[st] = nullptr;
      continue;
    }
    int num_hole_card_pairs = Game::NumHoleCardPairs(st);
    street_buckets[st] = new int[num_hole_card_pairs];
  }

  if (! buckets_.None(0)) {
    int num_hole_card_pairs = Game::NumHoleCardPairs(0);
    if (max_street == 0) {
      HandTree preflop_hand_tree(0, 0, 0);
      const CanonicalCards *hands = preflop_hand_tree.Hands(0, 0);
      for (int i = 0; i < num_hole_card_pairs; ++i) {
	const Card *hole_cards = hands->Cards(i);
	int hcp = HCPIndex(0, hole_cards);
	street_buckets[0][i] = buckets_.Bucket(0, hcp);
      }
    } else {
      for (int i = 0; i < num_hole_card_pairs; ++i) {
	street_buckets[0][i] = buckets_.Bucket(0, i);
      }
    }
  }
  return street_buckets;
}

// Same as VCFR implementation; should share
void MPVCFR::DeleteStreetBuckets(int **street_buckets) {
  int max_street = Game::MaxStreet();
  for (int st = 0; st <= max_street; ++st) {
    delete [] street_buckets[st];
  }
  delete [] street_buckets;
}

// Same as VCFR implementation; should share
void MPVCFR::SetStreetBuckets(int st, int gbd, int **street_buckets) {
  if (buckets_.None(st)) return;
  int num_board_cards = Game::NumBoardCards(st);
  const Card *board = BoardTree::Board(st, gbd);
  Card cards[7];
  for (int i = 0; i < num_board_cards; ++i) {
    cards[i + 2] = board[i];
  }
  // Assume root_bd_st_ == 0
  int lbd = gbd;

  const CanonicalCards *hands = hand_tree_->Hands(st, lbd);
  int max_street = Game::MaxStreet();
  int num_hole_card_pairs = Game::NumHoleCardPairs(st);
  for (int i = 0; i < num_hole_card_pairs; ++i) {
    int h;
    if (st == max_street) {
      // Hands on final street were reordered by hand strength, but
      // bucket lookup requires the unordered hole card pair index
      const Card *hole_cards = hands->Cards(i);
      cards[0] = hole_cards[0];
      cards[1] = hole_cards[1];
      int hcp = HCPIndex(st, cards);
      h = gbd * num_hole_card_pairs + hcp;
    } else {
      h = gbd * num_hole_card_pairs + i;
    }
    street_buckets[st][i] = buckets_.Bucket(st, h);
  }
}

double *MPVCFR::StreetInitial(Node *node, int plbd, int last_bet_to, int *contributions,
			      double **opp_probs, int **street_buckets,
			      const string &action_sequence) {
  int nst = node->Street();
  int pst = nst - 1;
  int prev_num_hole_card_pairs = Game::NumHoleCardPairs(pst);
  const CanonicalCards *pred_hands = hand_tree_->Hands(pst, plbd);
  Card max_card = Game::MaxCard();
  int num_encodings = (max_card + 1) * (max_card + 1);
  int *prev_canons = new int[num_encodings];
  double *vals = new double[prev_num_hole_card_pairs];
  for (int i = 0; i < prev_num_hole_card_pairs; ++i) vals[i] = 0;
  for (int ph = 0; ph < prev_num_hole_card_pairs; ++ph) {
    if (pred_hands->NumVariants(ph) > 0) {
      const Card *prev_cards = pred_hands->Cards(ph);
      int prev_encoding = prev_cards[0] * (max_card + 1) +
	prev_cards[1];
      prev_canons[prev_encoding] = ph;
    }
  }
  for (int ph = 0; ph < prev_num_hole_card_pairs; ++ph) {
    if (pred_hands->NumVariants(ph) == 0) {
      const Card *prev_cards = pred_hands->Cards(ph);
      int prev_encoding = prev_cards[0] * (max_card + 1) +
	prev_cards[1];
      int pc = prev_canons[pred_hands->Canon(ph)];
      prev_canons[prev_encoding] = pc;
    }
  }

  if (nst == 1 && num_threads_ > 1) {
    // Currently only flop supported
    unique_ptr<MPVCFRThread * []> threads(new MPVCFRThread *[num_threads_]);
    for (int t = 0; t < num_threads_; ++t) {
      threads[t] = new MPVCFRThread(this, t, num_threads_, node, last_bet_to, contributions,
				    action_sequence, opp_probs, prev_canons);
    }
    for (int t = 1; t < num_threads_; ++t) {
      threads[t]->Run();
    }
    // Do first thread in main thread
    threads[0]->Go();
    for (int t = 1; t < num_threads_; ++t) {
      threads[t]->Join();
    }
    for (int t = 0; t < num_threads_; ++t) {
      double *t_vals = threads[t]->RetVals();
      for (int i = 0; i < prev_num_hole_card_pairs; ++i) {
	vals[i] += t_vals[i];
      }
      delete threads[t];
    }
  } else {
    // Assume root_bd_st == 0
    int pgbd = plbd;
    int ngbd_begin = BoardTree::SuccBoardBegin(pst, pgbd, nst);
    int ngbd_end = BoardTree::SuccBoardEnd(pst, pgbd, nst);
    for (int ngbd = ngbd_begin; ngbd < ngbd_end; ++ngbd) {
    // Assume root_bd_st == 0
      int nlbd = ngbd;
      const CanonicalCards *hands = hand_tree_->Hands(nst, nlbd);
    
      SetStreetBuckets(nst, ngbd, street_buckets);
      // I can pass unset values for sum_opp_probs and total_card_probs.  I
      // know I will come across an opp choice node before getting to a terminal
      // node.
      double *next_vals = Process(node, nlbd, last_bet_to, contributions, opp_probs, street_buckets,
				  action_sequence, nst);

      int board_variants = BoardTree::NumVariants(nst, ngbd);
      int num_next_hands = hands->NumRaw();
      for (int nh = 0; nh < num_next_hands; ++nh) {
	const Card *cards = hands->Cards(nh);
	Card hi = cards[0];
	Card lo = cards[1];
	int encoding = hi * (max_card + 1) + lo;
	int prev_canon = prev_canons[encoding];
	vals[prev_canon] += board_variants * next_vals[nh];
      }
      delete [] next_vals;
    }
  }
  // Scale down the values of the previous-street canonical hands
  double scale_down = Game::StreetPermutations(nst);
  for (int ph = 0; ph < prev_num_hole_card_pairs; ++ph) {
    int prev_hand_variants = pred_hands->NumVariants(ph);
    if (prev_hand_variants > 0) {
      // Is this doing the right thing?
      vals[ph] /= scale_down * prev_hand_variants;
    }
  }
  // Copy the canonical hand values to the non-canonical
  for (int ph = 0; ph < prev_num_hole_card_pairs; ++ph) {
    if (pred_hands->NumVariants(ph) == 0) {
      vals[ph] = vals[prev_canons[pred_hands->Canon(ph)]];
    }
  }

  delete [] prev_canons;
  return vals;
}

double *MPVCFR::Process(Node *node, int lbd, int last_bet_to, int *contributions,
			double **opp_probs, int **street_buckets,
			const string &action_sequence, int last_st) {
  int st = node->Street();
  if (node->Terminal()) {
    double *showdown_vals, *fold_vals;
    fprintf(stderr, "p_ %u TID %u lbd %u\n", p_, node->TerminalID(), lbd);
    MPTerminal(p_, hand_tree_->Hands(st, lbd), contributions, opp_probs,
	       &showdown_vals, &fold_vals);
    if (node->NumRemaining() == 1) {
      delete [] showdown_vals;
      return fold_vals;
    } else {
      delete [] fold_vals;
      return showdown_vals;
    }
  } else {
    if (st > last_st) {
      return StreetInitial(node, lbd, last_bet_to, contributions, opp_probs,
			   street_buckets, action_sequence);
    }
    if (node->PlayerActing() == p_) {
      return OurChoice(node, lbd, last_bet_to, contributions, opp_probs,
		       street_buckets, action_sequence);
    } else {
      return OppChoice(node, lbd, last_bet_to, contributions, opp_probs,
		       street_buckets, action_sequence);
    }
  }
}

void MPVCFR::SetCurrentStrategy(Node *node) {
  if (node->Terminal()) return;
  int num_succs = node->NumSuccs();
  int st = node->Street();
  int nt = node->NonterminalID();
  int default_succ_index = node->DefaultSuccIndex();
  int p = node->PlayerActing();

  if (current_strategy_->Players(p) && ! buckets_.None(st) &&
      node->LastBetTo() < card_abstraction_.BucketThreshold(st) &&
      num_succs > 1) {
    // In RGBR calculation, for example, only want to set for opp
    
    int num_buckets = buckets_.NumBuckets(st);
    int num_nonterminal_succs = 0;
    bool *nonterminal_succs = new bool[num_succs];
    for (int s = 0; s < num_succs; ++s) {
      if (node->IthSucc(s)->Terminal()) {
	nonterminal_succs[s] = false;
      } else {
	nonterminal_succs[s] = true;
	++num_nonterminal_succs;
      }
    }

    double *d_all_current_strategy;
    current_strategy_->Values(p, st, nt, &d_all_current_strategy);
    double *d_all_cs_vals = nullptr;
    int *i_all_cs_vals = nullptr;
    bool nonneg;
    double explore;
    if (value_calculation_ && ! br_current_) {
      // Use average strategy for the "cs vals"
      if (sumprobs_->Ints(p, st)) {
	sumprobs_->Values(p, st, nt, &i_all_cs_vals);
      } else {
	sumprobs_->Values(p, st, nt, &d_all_cs_vals);
      }
      nonneg = true;
      explore = 0;
    } else {
      // Use regrets for the "cs vals"
      if (regrets_->Ints(p, st)) {
	regrets_->Values(p, st, nt, &i_all_cs_vals);
      } else {
	regrets_->Values(p, st, nt, &d_all_cs_vals);
      }
      nonneg = nn_regrets_ && regret_floors_[st] >= 0;
      explore = explore_;
    }
    if (i_all_cs_vals) {
      for (int b = 0; b < num_buckets; ++b) {
	int *cs_vals = i_all_cs_vals + b * num_succs;
	double *probs = d_all_current_strategy + b * num_succs;
	RegretsToProbs(cs_vals, num_succs, nonneg, uniform_, default_succ_index,
		       explore, num_nonterminal_succs, nonterminal_succs,
		       probs);
      }
    } else {
      for (int b = 0; b < num_buckets; ++b) {
	double *cs_vals = d_all_cs_vals + b * num_succs;
	double *probs = d_all_current_strategy + b * num_succs;
	RegretsToProbs(cs_vals, num_succs, nonneg, uniform_, default_succ_index,
		       explore, num_nonterminal_succs, nonterminal_succs,
		       probs);
      }
    }
    delete [] nonterminal_succs;
  }
  for (int s = 0; s < num_succs; ++s) {
    SetCurrentStrategy(node->IthSucc(s));
  }
}
