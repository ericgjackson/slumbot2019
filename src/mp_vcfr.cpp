#include <math.h> // lrint()
#include <stdio.h>
#include <stdlib.h>

#include <memory>
#include <string>
#include <vector>

#include "betting_tree.h"
#include "board_tree.h"
#include "buckets.h"
#include "canonical_cards.h"
#include "card_abstraction.h"
#include "cfr_config.h"
#include "cfr_street_values.h"
#include "cfr_utils.h"
#include "cfr_values.h"
#include "hand_tree.h"
#include "mp_vcfr.h"
#include "vcfr_state.h"

using std::string;
using std::unique_ptr;
using std::vector;

template <>
void MPVCFR::UpdateRegrets<int>(Node *node, double *vals, double **succ_vals, int *regrets) {
  int st = node->Street();
  int num_succs = node->NumSuccs();
  int num_hole_card_pairs = Game::NumHoleCardPairs(st);

  int floor = regret_floors_[st];
  int ceiling = regret_ceilings_[st];
  if (nn_regrets_) {
    for (int i = 0; i < num_hole_card_pairs; ++i) {
      int *my_regrets = regrets + i * num_succs;
      for (int s = 0; s < num_succs; ++s) {
	double d = succ_vals[s][i] - vals[i];
	// Need different implementation for doubles
	int di = lrint(d * regret_scaling_[st]);
	int ri = my_regrets[s] + di;
	if (ri < floor) {
	  my_regrets[s] = floor;
	} else if (ri > ceiling) {
	  my_regrets[s] = ceiling;
	} else {
	  my_regrets[s] = ri;
	}
      }
    }
  } else {
    for (int i = 0; i < num_hole_card_pairs; ++i) {
      int *my_regrets = regrets + i * num_succs;
      bool overflow = false;
      for (int s = 0; s < num_succs; ++s) {
	double d = succ_vals[s][i] - vals[i];
	my_regrets[s] += lrint(d * regret_scaling_[st]);
	if (my_regrets[s] < -2000000000 || my_regrets[s] > 2000000000) {
	  overflow = true;
	}
      }
      if (overflow) {
	for (int s = 0; s < num_succs; ++s) {
	  my_regrets[s] /= 2;
	}
      }
    }
  }
}

// This implementation does not round regrets to ints, nor do scaling.
template <>
void MPVCFR::UpdateRegrets<double>(Node *node, double *vals, double **succ_vals, double *regrets) {
  int st = node->Street();
  int num_succs = node->NumSuccs();
  int num_hole_card_pairs = Game::NumHoleCardPairs(st);
  
  double floor = regret_floors_[st];
  double ceiling = regret_ceilings_[st];
  if (nn_regrets_) {
    for (int i = 0; i < num_hole_card_pairs; ++i) {
      double *my_regrets = regrets + i * num_succs;
      for (int s = 0; s < num_succs; ++s) {
	double newr = my_regrets[s] + succ_vals[s][i] - vals[i];
	if (newr < floor) {
	  my_regrets[s] = floor;
	} else if (newr > ceiling) {
	  my_regrets[s] = ceiling;
	} else {
	  my_regrets[s] = newr;
	}
      }
    }
  } else {
    for (int i = 0; i < num_hole_card_pairs; ++i) {
      double *my_regrets = regrets + i * num_succs;
      for (int s = 0; s < num_succs; ++s) {
	my_regrets[s] += succ_vals[s][i] - vals[i];
      }
    }
  }
}

// This is ugly, but I can't figure out a better way.
// Need a version for bucketed systems
void MPVCFR::UpdateRegrets(Node *node, int lbd, double *vals, double **succ_vals) {
  int pa = node->PlayerActing();
  int st = node->Street();
  int nt = node->NonterminalID();
  int num_hole_card_pairs = Game::NumHoleCardPairs(st);
  int num_succs = node->NumSuccs();
  CFRStreetValues<double> *d_street_values;
  CFRStreetValues<int> *i_street_values;
  AbstractCFRStreetValues *street_values = regrets_->StreetValues(st);
  if ((d_street_values =
       dynamic_cast<CFRStreetValues<double> *>(street_values))) {
    double *board_regrets = d_street_values->AllValues(pa, nt) +
      lbd * num_hole_card_pairs * num_succs;
    UpdateRegrets(node, vals, succ_vals, board_regrets);
  } else if ((i_street_values =
	      dynamic_cast<CFRStreetValues<int> *>(street_values))) {
    int *board_regrets = i_street_values->AllValues(pa, nt) +
      lbd * num_hole_card_pairs * num_succs;
    UpdateRegrets(node, vals, succ_vals, board_regrets);
  }
}

double *MPVCFR::OurChoice(Node *node, int lbd, const VCFRState &state) {
  int pa = node->PlayerActing();
  int st = node->Street();
  int num_succs = node->NumSuccs();
  int num_hole_card_pairs = Game::NumHoleCardPairs(st);
  int nt = node->NonterminalID();
  double *vals = nullptr;
  double **succ_vals = new double *[num_succs];
  for (int s = 0; s < num_succs; ++s) {
    VCFRState succ_state(state, node, s);
    succ_vals[s] = Process(node->IthSucc(s), lbd, succ_state, st);
  }
  if (num_succs == 1) {
    vals = succ_vals[0];
    succ_vals[0] = nullptr;
  } else {
    int **street_buckets = state.StreetBuckets();
    vals = new double[num_hole_card_pairs];
    for (int i = 0; i < num_hole_card_pairs; ++i) vals[i] = 0;
    if (best_response_streets_[st]) {
      for (int i = 0; i < num_hole_card_pairs; ++i) {
	double max_val = succ_vals[0][i];
	for (int s = 1; s < num_succs; ++s) {
	  double sv = succ_vals[s][i];
	  if (sv > max_val) {max_val = sv;}
	}
	vals[i] = max_val;
#if 0
	if (st == 0 && node->NonterminalID() == 0 && pa == 1) {
	  printf("Root maxs %u\n", max_s);
	}
#endif
      }
    } else {
      int dsi = node->DefaultSuccIndex();
      bool bucketed = ! buckets_.None(st) &&
	node->LastBetTo() < card_abstraction_.BucketThreshold(st);
      if (bucketed && ! value_calculation_) {
	// This is true when we are running CFR+ on a bucketed system.  We
	// don't want to get the current strategy from the regrets during the
	// iteration, because the regrets for each bucket are in an intermediate
	// state.  Instead we compute the current strategy once at the
	// beginning of each iteration.
	// current_strategy_ always contains doubles
	CFRStreetValues<double> *street_values =
	  dynamic_cast< CFRStreetValues<double> *>(
			current_strategy_->StreetValues(st));
	for (int i = 0; i < num_hole_card_pairs; ++i) {
	  int b = street_buckets[st][i];
	  double *current_probs =
	    street_values->AllValues(pa, nt) + b * num_succs;
	  for (int s = 0; s < num_succs; ++s) {
	    vals[i] += succ_vals[s][i] * current_probs[s];
	  }
	}
      } else {
	AbstractCFRStreetValues *street_values;
	if (value_calculation_) {
	  street_values = sumprobs_->StreetValues(st);
	} else {
	  street_values = regrets_->StreetValues(st);
	}
	unique_ptr<double []> current_probs(new double[num_succs]);
	if (bucketed) {
	  for (int i = 0; i < num_hole_card_pairs; ++i) {
	    int b = street_buckets[st][i];
	    street_values->RMProbs(pa, nt, b * num_succs, num_succs, dsi,
				   current_probs.get());
	    for (int s = 0; s < num_succs; ++s) {
	      vals[i] += succ_vals[s][i] * current_probs[s];
	    }
	  }
	} else {
	  for (int i = 0; i < num_hole_card_pairs; ++i) {
	    int offset = lbd * num_hole_card_pairs * num_succs +
	      i * num_succs;
	    street_values->RMProbs(pa, nt, offset, num_succs, dsi,
				   current_probs.get());
	    for (int s = 0; s < num_succs; ++s) {
	      vals[i] += succ_vals[s][i] * current_probs[s];
	    }
	  }
	}
      }
      if (! value_calculation_ && ! pre_phase_) {
	// Need values for current board if this is unabstracted system
	UpdateRegrets(node, lbd, vals, succ_vals);
      }
    }
  }

  for (int s = 0; s < num_succs; ++s) {
    delete [] succ_vals[s];
  }
  delete [] succ_vals;
  
  return vals;
}

double *MPVCFR::OppChoice(Node *node, int lbd, const VCFRState &state) {
  int st = node->Street();
  int num_succs = node->NumSuccs();
  int num_hole_card_pairs = Game::NumHoleCardPairs(st);
  const HandTree *hand_tree = state.GetHandTree();
  const CanonicalCards *hands = hand_tree->Hands(st, lbd);
  int num_hole_cards = Game::NumCardsForStreet(0);
  int max_card1 = Game::MaxCard() + 1;
  int num_enc;
  if (num_hole_cards == 1) num_enc = max_card1;
  else                     num_enc = max_card1 * max_card1;

  double *opp_probs = state.OppProbs();
  double **succ_opp_probs = new double *[num_succs];
  if (num_succs == 1) {
    succ_opp_probs[0] = new double[num_enc];
    for (int i = 0; i < num_enc; ++i) {
      succ_opp_probs[0][i] = opp_probs[i];
    }
  } else {
    int **street_buckets = state.StreetBuckets();
    for (int s = 0; s < num_succs; ++s) {
      succ_opp_probs[s] = new double[num_enc];
      for (int i = 0; i < num_enc; ++i) succ_opp_probs[s][i] = 0;
    }

    int dsi = node->DefaultSuccIndex();
    bool bucketed = ! buckets_.None(st) &&
      node->LastBetTo() < card_abstraction_.BucketThreshold(st);
    
    if (bucketed && ! value_calculation_) {
      // This is true when we are running CFR+ on a bucketed system.  We
      // don't want to get the current strategy from the regrets during the
      // iteration, because the regrets for each bucket are in an intermediate
      // state.  Instead we compute the current strategy once at the
      // beginning of each iteration.
      // current_strategy_ always contains doubles
    } else {
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
	  ProcessOppProbs(node, lbd, hands, bucketed, street_buckets,
			  opp_probs, succ_opp_probs, *d_cs_values, dsi, it_,
			  soft_warmup_, hard_warmup_, sumprob_scaling_[st],
			  d_sumprob_values);
	} else {
	  ProcessOppProbs(node, lbd, hands, bucketed, street_buckets,
			  opp_probs, succ_opp_probs, *d_cs_values, dsi, it_,
			  soft_warmup_, hard_warmup_, sumprob_scaling_[st],
			  i_sumprob_values);
	}
      } else {
	i_cs_values = dynamic_cast<CFRStreetValues<int> *>(cs_values);
	if (i_cs_values == nullptr) {
	  fprintf(stderr, "Neither int nor double cs values?!?\n");
	  exit(-1);
	}
	if (d_sumprob_values) {
	  ProcessOppProbs(node, lbd, hands, bucketed, street_buckets,
			  opp_probs, succ_opp_probs, *i_cs_values, dsi, it_,
			  soft_warmup_, hard_warmup_, sumprob_scaling_[st],
			  d_sumprob_values);
	} else {
	  ProcessOppProbs(node, lbd, hands, bucketed, street_buckets,
			  opp_probs, succ_opp_probs, *i_cs_values, dsi, it_,
			  soft_warmup_, hard_warmup_, sumprob_scaling_[st],
			  i_sumprob_values);
	}
      }
    }
  }

  double *vals = nullptr;
  double succ_sum_opp_probs;
  for (int s = 0; s < num_succs; ++s) {
    double *succ_total_card_probs = new double[max_card1];
    CommonBetResponseCalcs(st, hands, succ_opp_probs[s], &succ_sum_opp_probs,
			   succ_total_card_probs);
    if (prune_ && succ_sum_opp_probs == 0) {
      delete [] succ_total_card_probs;
      continue;
    }
    VCFRState succ_state(state, node, s, succ_opp_probs[s], succ_sum_opp_probs,
			 succ_total_card_probs);
    double *succ_vals = Process(node->IthSucc(s), lbd, succ_state, st);
    if (vals == nullptr) {
      vals = succ_vals;
    } else {
      for (int i = 0; i < num_hole_card_pairs; ++i) {
	vals[i] += succ_vals[i];
      }
      delete [] succ_vals;
    }
    delete [] succ_total_card_probs;
  }
  if (vals == nullptr) {
    // This can happen if there were non-zero opp probs on the prior street,
    // but the board cards just dealt blocked all the opponent hands with
    // non-zero probability.
    vals = new double[num_hole_card_pairs];
    for (int i = 0; i < num_hole_card_pairs; ++i) vals[i] = 0;
  }
  for (int s = 0; s < num_succs; ++s) {
    delete [] succ_opp_probs[s];
  }
  delete [] succ_opp_probs;

  return vals;
}

class MPVCFRThread {
public:
  MPVCFRThread(MPVCFR *mp_vcfr, int thread_index, int num_threads, Node *node,
	       const string &action_sequence, double *opp_probs, const HandTree *hand_tree,
	       int *prev_canons);
  ~MPVCFRThread(void);
  void Run(void);
  void Join(void);
  void Go(void);
  double *RetVals(void) const {return ret_vals_;}
private:
  MPVCFR *mp_vcfr_;
  int thread_index_;
  int num_threads_;
  Node *node_;
  string action_sequence_;
  double *opp_probs_;
  const HandTree *hand_tree_;
  int *prev_canons_;
  double *ret_vals_;
  pthread_t pthread_id_;
};

MPVCFRThread::MPVCFRThread(MPVCFR *mp_vcfr, int thread_index, int num_threads, Node *node,
			   const string &action_sequence, double *opp_probs,
			   const HandTree *hand_tree, int *prev_canons) :
  action_sequence_(action_sequence) {
  mp_vcfr_ = mp_vcfr;
  thread_index_ = thread_index;
  num_threads_ = num_threads;
  node_ = node;
  opp_probs_ = opp_probs;
  hand_tree_ = hand_tree;
  prev_canons_ = prev_canons;
}

MPVCFRThread::~MPVCFRThread(void) {
  delete [] ret_vals_;
}

static void *mp_vcfr_thread_run(void *v_t) {
  MPVCFRThread *t = (MPVCFRThread *)v_t;
  t->Go();
  return NULL;
}

void MPVCFRThread::Run(void) {
  pthread_create(&pthread_id_, NULL, mp_vcfr_thread_run, this);
}

void MPVCFRThread::Join(void) {
  pthread_join(pthread_id_, NULL); 
}

void MPVCFRThread::Go(void) {
  int st = node_->Street();
  int pst = node_->Street() - 1;
  int num_boards = BoardTree::NumBoards(st);
  int num_prev_hole_card_pairs = Game::NumHoleCardPairs(pst);
  Card max_card1 = Game::MaxCard() + 1;
  ret_vals_ = new double[num_prev_hole_card_pairs];
  for (int i = 0; i < num_prev_hole_card_pairs; ++i) ret_vals_[i] = 0;
  for (int bd = thread_index_; bd < num_boards; bd += num_threads_) {
    int **street_buckets = AllocateStreetBuckets();
    VCFRState state(opp_probs_, hand_tree_, bd, action_sequence_, 0, 0,
		    street_buckets);
    // Initialize buckets for this street
    mp_vcfr_->SetStreetBuckets(st, bd, state);
    double *bd_vals = mp_vcfr_->Process(node_, bd, state, st);
    const CanonicalCards *hands = hand_tree_->Hands(st, bd);
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
    DeleteStreetBuckets(street_buckets);
  }
}

// Divide work at a street-initial node between multiple threads.  Spawns
// the threads, joins them, aggregates the resulting CVs.
// Only support splitting on the flop for now.
// Ugly that we pass prev_canons in.
void MPVCFR::Split(Node *node, double *opp_probs, const HandTree *hand_tree,
		   const string &action_sequence, int *prev_canons, double *vals) {
  int nst = node->Street();
  int pst = nst - 1;
  int prev_num_hole_card_pairs = Game::NumHoleCardPairs(pst);
  for (int i = 0; i < prev_num_hole_card_pairs; ++i) vals[i] = 0;
  unique_ptr<MPVCFRThread * []> threads(new MPVCFRThread *[num_threads_]);
  for (int t = 0; t < num_threads_; ++t) {
    threads[t] = new MPVCFRThread(this, t, num_threads_, node, action_sequence,
				  opp_probs, hand_tree, prev_canons);
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
}

void MPVCFR::SetStreetBuckets(int st, int gbd, const VCFRState &state) {
  if (buckets_.None(st)) return;
  int num_board_cards = Game::NumBoardCards(st);
  const Card *board = BoardTree::Board(st, gbd);
  Card cards[7];
  for (int i = 0; i < num_board_cards; ++i) {
    cards[i + 2] = board[i];
  }
  int lbd = BoardTree::LocalIndex(state.RootBdSt(), state.RootBd(),
					   st, gbd);

  const HandTree *hand_tree = state.GetHandTree();
  const CanonicalCards *hands = hand_tree->Hands(st, lbd);
  int max_street = Game::MaxStreet();
  int num_hole_card_pairs = Game::NumHoleCardPairs(st);
  int **street_buckets = state.StreetBuckets();
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

double *MPVCFR::StreetInitial(Node *node, int plbd, const VCFRState &state) {
  int nst = node->Street();
  int pst = nst - 1;
  int prev_num_hole_card_pairs = Game::NumHoleCardPairs(pst);
#if 0
  // Move this to CFRP class
  if (nst == subgame_street_ && ! subgame_) {
    if (pre_phase_) {
      SpawnSubgame(node, plbd, state.ActionSequence(), state.OppProbs());
      // Code expects values to be returned so we return all zeroes
      double *vals = new double[prev_num_hole_card_pairs];
      for (int i = 0; i < prev_num_hole_card_pairs; ++i) vals[i] = 0;
      return vals;
    } else {
      int p = node->PlayerActing();
      int nt = node->NonterminalID();
      double *final_vals = final_vals_[p][nt][plbd];
      if (final_vals == nullptr) {
	fprintf(stderr, "No final vals for %u %u %u?!?\n", p, nt, plbd);
	exit(-1);
      }
      final_vals_[p][nt][plbd] = nullptr;
      return final_vals;
    }
  }
#endif
  const HandTree *hand_tree = state.GetHandTree();
  const CanonicalCards *pred_hands = hand_tree->Hands(pst, plbd);
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

  // Move this to CFRP class?
  if (nst == 1 && subgame_street_ == -1 && num_threads_ > 1) {
    // Currently only flop supported
    Split(node, state.OppProbs(), state.GetHandTree(), state.ActionSequence(),
	  prev_canons, vals);
  } else {
    int pgbd = BoardTree::GlobalIndex(state.RootBdSt(), state.RootBd(), pst, plbd);
    int ngbd_begin = BoardTree::SuccBoardBegin(pst, pgbd, nst);
    int ngbd_end = BoardTree::SuccBoardEnd(pst, pgbd, nst);
    for (int ngbd = ngbd_begin; ngbd < ngbd_end; ++ngbd) {
      int nlbd = BoardTree::LocalIndex(state.RootBdSt(),
						state.RootBd(), nst, ngbd);

      const CanonicalCards *hands = hand_tree->Hands(nst, nlbd);
    
      SetStreetBuckets(nst, ngbd, state);
      // I can pass unset values for sum_opp_probs and total_card_probs.  I
      // know I will come across an opp choice node before getting to a terminal
      // node.
      double *next_vals = Process(node, nlbd, state, nst);

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

double *MPVCFR::Process(Node *node, int lbd, const VCFRState &state, int last_st) {
  int st = node->Street();
  if (node->Terminal()) {
    if (node->NumRemaining() == 1) {
      return Fold(node, p_, state.GetHandTree()->Hands(st, lbd),
		  state.OppProbs(), state.SumOppProbs(),
		  state.TotalCardProbs());
    } else {
      return Showdown(node, state.GetHandTree()->Hands(st, lbd),
		      state.OppProbs(), state.SumOppProbs(),
		      state.TotalCardProbs());
    }
  }
  if (st > last_st) {
    return StreetInitial(node, lbd, state);
  }
  if (node->PlayerActing() == p_) {
    return OurChoice(node, lbd, state);
  } else {
    return OppChoice(node, lbd, state);
  }
}

void MPVCFR::SetCurrentStrategy(Node *node) {
  if (node->Terminal()) return;
  int num_succs = node->NumSuccs();
  int st = node->Street();
  int nt = node->NonterminalID();
  int dsi = node->DefaultSuccIndex();
  int p = node->PlayerActing();

  if (current_strategy_->StreetValues(st)->Players(p) && ! buckets_.None(st) &&
      node->LastBetTo() < card_abstraction_.BucketThreshold(st) &&
      num_succs > 1) {
    // In RGBR calculation, for example, only want to set for opp

    int num_buckets = buckets_.NumBuckets(st);
#if 0
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
#endif
    AbstractCFRStreetValues *street_values;
    if (value_calculation_ && ! br_current_) {
      street_values = sumprobs_->StreetValues(st);
    } else {
      street_values = regrets_->StreetValues(st);
    }
    CFRStreetValues<double> *d_current_strategy_vals =
      dynamic_cast<CFRStreetValues<double> *>(
	       current_strategy_->StreetValues(st));
    unique_ptr<double []> current_probs(new double[num_succs]);
    for (int b = 0; b < num_buckets; ++b) {
      street_values->RMProbs(p, nt, b * num_succs, num_succs, dsi,
			     current_probs.get());
      d_current_strategy_vals->Set(p, nt, b, num_succs, current_probs.get());
    }
    // delete [] nonterminal_succs;
  }
  for (int s = 0; s < num_succs; ++s) {
    SetCurrentStrategy(node->IthSucc(s));
  }
}

MPVCFR::MPVCFR(const CardAbstraction &ca, const BettingAbstraction &ba, const CFRConfig &cc,
	       const Buckets &buckets, int num_threads) :
  card_abstraction_(ca), betting_abstraction_(ba), cfr_config_(cc),
  buckets_(buckets) {
  num_threads_ = num_threads;
  regret_floors_ = nullptr;
  regret_ceilings_ = nullptr;
  regret_scaling_ = nullptr;
  sumprob_scaling_ = nullptr;
  subgame_street_ = cfr_config_.SubgameStreet();
  soft_warmup_ = cfr_config_.SoftWarmup();
  hard_warmup_ = cfr_config_.HardWarmup();
  nn_regrets_ = cfr_config_.NNR();
  br_current_ = false;
  prob_method_ = ProbMethod::REGRET_MATCHING;
  value_calculation_ = false;
  // Whether we prune branches if no opponent hand reaches.  Normally true,
  // but false when calculating CBRs.
  prune_ = true;
  pre_phase_ = false;

  int max_street = Game::MaxStreet();
  best_response_streets_ = new bool[max_street + 1];
  for (int st = 0; st <= max_street; ++st) {
    best_response_streets_[st] = false;
  }
  
  sumprob_streets_ = new bool[max_street + 1];
  const vector<int> &ssv = cfr_config_.SumprobStreets();
  int num_ssv = ssv.size();
  if (num_ssv == 0) {
    for (int st = 0; st <= max_street; ++st) {
      sumprob_streets_[st] = true;
    }
  } else {
    for (int st = 0; st <= max_street; ++st) {
      sumprob_streets_[st] = false;
    }
    for (int i = 0; i < num_ssv; ++i) {
      int st = ssv[i];
      sumprob_streets_[st] = true;
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

  regret_ceilings_ = new int[max_street + 1];
  const vector<int> &cv = cfr_config_.RegretCeilings();
  if (cv.size() == 0) {
    for (int s = 0; s <= max_street; ++s) {
      regret_ceilings_[s] = kMaxInt;
    }
  } else {
    if ((int)cv.size() < max_street + 1) {
      fprintf(stderr, "Regret ceiling vector too small\n");
      exit(-1);
    }
    for (int s = 0; s <= max_street; ++s) {
      if (cv[s] == 0) regret_ceilings_[s] = kMaxInt;
      else            regret_ceilings_[s] = cv[s];
    }
  }

  regret_scaling_ = new double[max_street + 1];
  sumprob_scaling_ = new double[max_street + 1];
  const vector<double> &rv = cfr_config_.RegretScaling();
  if (rv.size() == 0) {
    for (int s = 0; s <= max_street; ++s) {
      regret_scaling_[s] = 1.0;
    }
  } else {
    if ((int)rv.size() < max_street + 1) {
      fprintf(stderr, "Regret scaling vector too small\n");
      exit(-1);
    }
    for (int s = 0; s <= max_street; ++s) {
      regret_scaling_[s] = rv[s];
    }
  }
  const vector<double> &sv = cfr_config_.SumprobScaling();
  if (sv.size() == 0) {
    for (int s = 0; s <= max_street; ++s) {
      sumprob_scaling_[s] = 1.0;
    }
  } else {
    if ((int)sv.size() < max_street + 1) {
      fprintf(stderr, "Sumprob scaling vector too small\n");
      exit(-1);
    }
    for (int s = 0; s <= max_street; ++s) {
      sumprob_scaling_[s] = sv[s];
    }
  }
}

MPVCFR::~MPVCFR(void) {
  // delete [] compressed_streets_;
  delete [] regret_floors_;
  delete [] regret_ceilings_;
  delete [] regret_scaling_;
  delete [] sumprob_scaling_;
  delete [] sumprob_streets_;
  delete [] best_response_streets_;
}

