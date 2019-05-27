// New implementation of run_approx_rgbr.  Difference is that PreResponder no longer is a subclass
// of VCFR.  Needs to maintain reach probs so that we can support endgame solving.
//
// Computes an approximate real-game best-response.  It is a sampling approximation of a lower
// bound to the true best-response value.
//
// Similar to a real best-response calculation, the code here computes the value a "responder" can
// achieve playing against a "target" player strategy.  There are two refinements:
// 1) The responder is limited in how he can respond (as compared to a true best-response).  So
// the value we get out is a lower bound on the true real-game best-response value.
// 2) We sample so that we only get an approximation of this lower bound value.
//
// run_approx_rgbr takes a street argument.  Prior to the given street the responder just plays
// according to the target strategy.  On the given street and later street the responder
// plays a best response.
//
// We also sample from the boards on the given street.
//
// We resolve twice as much as needed (when using unsafe method).

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h> // gettimeofday()

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "betting_abstraction.h"
#include "betting_abstraction_params.h"
#include "betting_tree.h"
#include "betting_trees.h"
#include "board_tree.h"
#include "buckets.h"
#include "canonical_cards.h"
#include "canonical.h"
#include "card_abstraction.h"
#include "card_abstraction_params.h"
#include "cfr_config.h"
#include "cfr_params.h"
#include "cfr_utils.h"
#include "cfr_values.h"
#include "constants.h"
#include "files.h"
#include "game.h"
#include "game_params.h"
#include "hand_tree.h"
#include "hand_value_tree.h"
#include "io.h"
#include "params.h"
#include "rand.h"
#include "reach_probs.h"
#include "sorting.h"
#include "subgame_utils.h"
#include "unsafe_eg_cfr.h"
#include "vcfr.h"
#include "vcfr_state.h"

using std::pair;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;

class PreResponder {
public:
  PreResponder(const CardAbstraction &ca, const BettingAbstraction &ba, const CFRConfig &cc,
	       const Buckets &buckets, int it, int street, int num_sampled_boards, bool quantize,
	       bool resolve, const CardAbstraction &s_ca, const BettingAbstraction &s_ba,
	       const CFRConfig &s_cc, int num_resolve_its);
  void Initialize(int responder_p);
  double Go(int responder_p);
private:
  void SetStreetBuckets(int st, int gbd);
  void Resolve(int gbd, const ReachProbs &reach_probs, const string &action_sequence,
	       const HandTree *hand_tree);
  shared_ptr<double []> Transition(Node *p0_node, Node *p1_node, const ReachProbs &reach_probs,
				   int gbd, const string &action_sequence);
  shared_ptr<double []> StreetInitial(Node *p0_node, Node *p1_node, const ReachProbs &reach_probs,
				      int gbd, const string &action_sequence);
  shared_ptr<double []> Process(Node *p0_node, Node *p1_node, const ReachProbs &reach_probs,
				int gbd, const string &action_sequence, int last_st);

  const CardAbstraction &card_abstraction_;
  const BettingAbstraction &betting_abstraction_;
  const CFRConfig &cfr_config_;
  const Buckets &buckets_;
  unique_ptr<Buckets> subgame_buckets_;
  int it_;
  int street_;
  bool quantize_;
  bool resolve_;
  const BettingAbstraction &subgame_betting_abstraction_;
  int num_resolve_its_;
  int responder_p_;
  shared_ptr<CFRValues> sumprobs_;
  unique_ptr<BettingTrees> betting_trees_;
  bool use_subgames_;
  unique_ptr<HandTree> trunk_hand_tree_;
  unique_ptr<int []> board_samples_;
  unique_ptr<unique_ptr<bool []> []> has_continuation_;
  unique_ptr<int []> street_buckets_;
  unique_ptr<VCFR> vcfr_;
  unique_ptr<EGCFR> eg_cfr_;
  unique_ptr<BettingTrees> subtrees_;
  int num_resolves_;
  double resolving_secs_;
};

PreResponder::PreResponder(const CardAbstraction &ca, const BettingAbstraction &ba,
			   const CFRConfig &cc, const Buckets &buckets, int it, int street,
			   int num_sampled_boards, bool quantize, bool resolve,
			   const CardAbstraction &s_ca, const BettingAbstraction &s_ba,
			   const CFRConfig &s_cc, int num_resolve_its) :
  card_abstraction_(ca), betting_abstraction_(ba), cfr_config_(cc), buckets_(buckets), it_(it),
  street_(street), quantize_(quantize), resolve_(resolve), subgame_betting_abstraction_(s_ba),
  num_resolve_its_(num_resolve_its) {
  int max_street = Game::MaxStreet();
  // If use_subgames_ is true then we will create a HandTree at each transition node that is
  // specific to the current board at that node.  Otherwise we will have a single global hand
  // tree for the entire game.
  if (resolve_) {
    use_subgames_ = true;
  } else {
    bool all_post_bucketed = true;
    for (int st = street_; st <= max_street; ++st) {
      if (buckets.None(st)) {
	all_post_bucketed = false;
	break;
      }
    }
    // If we are using buckets for every street >= street_, then we don't need the global board
    // index to compute offsets into the sumprobs.
    if (all_post_bucketed) {
      use_subgames_ = true;
    } else {
      use_subgames_ = false;
    }
  }
  if (use_subgames_) {
    trunk_hand_tree_.reset(new HandTree(0, 0, street_ - 1));
  } else {
    trunk_hand_tree_.reset(new HandTree(0, 0, max_street));
  }

  int num_boards = BoardTree::NumBoards(street_);
  board_samples_.reset(new int[num_boards]);
  if (street_ > 0) {
    has_continuation_.reset(new unique_ptr<bool []>[street_]);
    for (int st = 0; st < street_; ++st) {
      int num_boards = BoardTree::NumBoards(st);
      has_continuation_[st].reset(new bool[num_boards]);
    }
    has_continuation_[0][0] = true;
  }

  if (num_sampled_boards == 0 || num_sampled_boards > num_boards) {
    num_sampled_boards = num_boards;
  }
  if (num_sampled_boards == num_boards) {
    fprintf(stderr, "Sampling all boards on street %i\n", street_);
    for (int bd = 0; bd < num_boards; ++bd) {
      board_samples_[bd] = BoardTree::NumVariants(street_, bd);
    }
    for (int st = 1; st < street_; ++st) {
      int num_st_boards = BoardTree::NumBoards(st);
      for (int bd = 0; bd < num_st_boards; ++bd) {
	has_continuation_[st][bd] = true;
      }
    }
  } else {
    fprintf(stderr, "Sampling only some boards on street %i\n", street_);
    for (int bd = 0; bd < num_boards; ++bd) board_samples_[bd] = 0;
    for (int st = 1; st < street_; ++st) {
      int num_st_boards = BoardTree::NumBoards(st);
      for (int bd = 0; bd < num_st_boards; ++bd) {
	has_continuation_[st][bd] = false;
      }
    }
    struct drand48_data rand_buf;
    struct timeval time; 
    gettimeofday(&time, NULL);
    srand48_r((time.tv_sec * 1000) + (time.tv_usec / 1000), &rand_buf);
    vector< pair<double, int> > v;
    for (int bd = 0; bd < num_boards; ++bd) {
      int board_count = BoardTree::BoardCount(street_, bd);
      for (int i = 0; i < board_count; ++i) {
	double r;
	drand48_r(&rand_buf, &r);
	v.push_back(std::make_pair(r, bd));
      }
    }
    std::sort(v.begin(), v.end());
    for (int i = 0; i < num_sampled_boards; ++i) {
      int bd = v[i].second;
      // fprintf(stderr, "Sampling %i\n", bd);
      ++board_samples_[bd];
      if (street_ > 1) {
	const Card *board = BoardTree::Board(street_, bd);
	for (int st = 1; st < street_; ++st) {
	  int pbd = BoardTree::LookupBoard(board, st);
	  has_continuation_[st][pbd] = true;
	}
      }
    }
  }
  int max_num_hole_card_pairs = Game::NumHoleCardPairs(0);
  int num = (max_street + 1) * max_num_hole_card_pairs;
  street_buckets_.reset(new int[num]);

  if (resolve_) {
    subgame_buckets_.reset(new Buckets(s_ca, false));
    eg_cfr_.reset(new UnsafeEGCFR(s_ca, ca, s_ba, ba, s_cc, cc, *subgame_buckets_, 1));
  } else {
    vcfr_.reset(new VCFR(ca, ba, cc, buckets, 1));
    vcfr_->SetValueCalculation(true);
    for (int st = street_; st <= max_street; ++st) {
      vcfr_->SetBestResponseStreet(st, true);
    }
  }
  
  num_resolves_ = 0;
  resolving_secs_ = 0;
}

void PreResponder::SetStreetBuckets(int st, int gbd) {
  if (buckets_.None(st)) return;
  int num_board_cards = Game::NumBoardCards(st);
  const Card *board = BoardTree::Board(st, gbd);
  Card cards[7];
  for (int i = 0; i < num_board_cards; ++i) {
    cards[i + 2] = board[i];
  }

  const CanonicalCards *hands = trunk_hand_tree_->Hands(st, gbd);
  int max_street = Game::MaxStreet();
  int num_hole_card_pairs = Game::NumHoleCardPairs(st);
  int max_num_hole_card_pairs = Game::NumHoleCardPairs(0);
  int *street_buckets = street_buckets_.get() + st * max_num_hole_card_pairs;
  for (int i = 0; i < num_hole_card_pairs; ++i) {
    unsigned int h;
    if (st == max_street) {
      // Hands on final street were reordered by hand strength, but
      // bucket lookup requires the unordered hole card pair index
      const Card *hole_cards = hands->Cards(i);
      cards[0] = hole_cards[0];
      cards[1] = hole_cards[1];
      int hcp = HCPIndex(st, cards);
      h = ((unsigned int)gbd) * ((unsigned int)num_hole_card_pairs) + hcp;
    } else {
      h = ((unsigned int)gbd) * ((unsigned int)num_hole_card_pairs) + i;
    }
    street_buckets[i] = buckets_.Bucket(st, h);
  }
}

// Unfortunate replication of code from VCFR::StreetInitial()
static int *CreatePredCanons(HandTree *hand_tree, int pst, int pbd) {
  int pred_num_hole_card_pairs = Game::NumHoleCardPairs(pst);
  const CanonicalCards *pred_hands = hand_tree->Hands(pst, pbd);
  int max_card1 = Game::MaxCard() + 1;
  int num_enc = max_card1 * max_card1;
  int *pred_canons = new int[num_enc];
  for (int phcp = 0; phcp < pred_num_hole_card_pairs; ++phcp) {
    if (pred_hands->NumVariants(phcp) > 0) {
      const Card *pred_cards = pred_hands->Cards(phcp);
      int pred_encoding = pred_cards[0] * max_card1 + pred_cards[1];
      pred_canons[pred_encoding] = phcp;
    }
  }
  for (int phcp = 0; phcp < pred_num_hole_card_pairs; ++phcp) {
    if (pred_hands->NumVariants(phcp) == 0) {
      const Card *pred_cards = pred_hands->Cards(phcp);
      int pred_encoding = pred_cards[0] * max_card1 + pred_cards[1];
      int pc = pred_canons[pred_hands->Canon(phcp)];
      pred_canons[pred_encoding] = pc;
    }
  }
  return pred_canons;
}

void PreResponder::Resolve(int gbd, const ReachProbs &reach_probs,
			   const string &action_sequence, const HandTree *hand_tree) {
  struct timespec start, finish;
  clock_gettime(CLOCK_MONOTONIC, &start);
  eg_cfr_->SolveSubgame(subtrees_.get(), gbd, reach_probs, action_sequence, hand_tree, nullptr, -1,
			true, num_resolve_its_);
  clock_gettime(CLOCK_MONOTONIC, &finish);
  resolving_secs_ += (finish.tv_sec - start.tv_sec);
  resolving_secs_ += (finish.tv_nsec - start.tv_nsec) / 1000000000.0;
  ++num_resolves_;
}

shared_ptr<double []> PreResponder::Transition(Node *p0_node, Node *p1_node,
					       const ReachProbs &reach_probs, int pbd,
					       const string &action_sequence) {
  int nst = p0_node->Street();
  int pst = nst - 1;
  int pred_num_hole_card_pairs = Game::NumHoleCardPairs(pst);
  int max_card1 = Game::MaxCard() + 1;
  const CanonicalCards *pred_hands = trunk_hand_tree_->Hands(pst, pbd);
  unique_ptr<int []> pred_canons(CreatePredCanons(trunk_hand_tree_.get(), pst, pbd));
  shared_ptr<double []> vals(new double[pred_num_hole_card_pairs]);
  for (int i = 0; i < pred_num_hole_card_pairs; ++i) vals[i] = 0;
  // pbd is a global board index
  int ngbd_begin = BoardTree::SuccBoardBegin(pst, pbd, nst);
  int ngbd_end = BoardTree::SuccBoardEnd(pst, pbd, nst);
  int total_num_samples = 0;
  for (int ngbd = ngbd_begin; ngbd < ngbd_end; ++ngbd) {
    int num_samples = board_samples_[ngbd];
    if (num_samples == 0) continue;
    total_num_samples += num_samples;
    int nlbd;
    unique_ptr<VCFRState> next_state;
    unique_ptr<HandTree> subgame_hand_tree;
    const HandTree *next_hand_tree;
    if (use_subgames_) {
      // Create a hand tree just for this subgame.  The local board index is thus zero.
      subgame_hand_tree.reset(new HandTree(nst, ngbd, Game::MaxStreet()));
      next_hand_tree = subgame_hand_tree.get();
      nlbd = 0;
      next_state.reset(new VCFRState(responder_p_, reach_probs.Get(responder_p_^1), next_hand_tree,
				     action_sequence, ngbd, street_));
    } else {
      next_hand_tree = trunk_hand_tree_.get();
      nlbd = ngbd;
      next_state.reset(new VCFRState(responder_p_, reach_probs.Get(responder_p_^1),
				     next_hand_tree, action_sequence, 0, 0));
    }
    Node *node;
    if (responder_p_ == 0) {
      // If P0 is the best responder and P1 is the target, then want to use P1's tree.
      node = p1_node;
    } else {
      node = p0_node;
    }
    shared_ptr<double []> next_vals;
    if (resolve_) {
      subtrees_.reset(CreateSubtrees(nst, node->PlayerActing(), node->LastBetTo(), -1,
				     subgame_betting_abstraction_));
      fprintf(stderr, "Resolving P%i %s pbd %i ngbd %i\n", responder_p_, action_sequence.c_str(),
	      pbd, ngbd);
      Resolve(ngbd, reach_probs, action_sequence, next_hand_tree);
      eg_cfr_->SetStreetBuckets(nst, ngbd, *next_state);
      eg_cfr_->SetValueCalculation(true);
      int max_street = Game::MaxStreet();
      for (int st = street_; st <= max_street; ++st) {
	eg_cfr_->SetBestResponseStreet(st, true);
      }
      Node *subtree_root = subtrees_->Root();
      next_vals = eg_cfr_->Process(subtree_root, subtree_root, nlbd, *next_state, nst);
      eg_cfr_->SetValueCalculation(false);
      for (int st = street_; st <= max_street; ++st) {
	eg_cfr_->SetBestResponseStreet(st, false);
      }
    } else {
      vcfr_->SetStreetBuckets(nst, ngbd, *next_state);
      next_vals = vcfr_->Process(node, node, nlbd, *next_state, nst);
    }
    const CanonicalCards *hands;
    if (subgame_hand_tree.get()) {
      hands = subgame_hand_tree->Hands(nst, 0);
    } else {
      hands = trunk_hand_tree_->Hands(nst, ngbd);
    }
    int num_next_hands = hands->NumRaw();
    for (int nhcp = 0; nhcp < num_next_hands; ++nhcp) {
      const Card *cards = hands->Cards(nhcp);
      Card hi = cards[0];
      Card lo = cards[1];
      int enc = hi * max_card1 + lo;
      int pred_canon = pred_canons[enc];
      vals[pred_canon] += num_samples * next_vals[nhcp];
    }
  }
  if (total_num_samples > 0) {
    double d_total_num_samples = total_num_samples;
    int num_board_permutations = Game::StreetPermutations3(nst);
    double overweighting = num_board_permutations / d_total_num_samples;
    double scale_down = Game::StreetPermutations(nst);
    for (int ph = 0; ph < pred_num_hole_card_pairs; ++ph) {
      int pred_hand_variants = pred_hands->NumVariants(ph);
      if (pred_hand_variants > 0) {
	vals[ph] *= overweighting / (scale_down * pred_hand_variants);
      }
    }
    // Copy the canonical hand values to the non-canonical
    for (int ph = 0; ph < pred_num_hole_card_pairs; ++ph) {
      if (pred_hands->NumVariants(ph) == 0) {
	vals[ph] = vals[pred_canons[pred_hands->Canon(ph)]];
      }
    }
  }
  return vals;
}

shared_ptr<double []> PreResponder::StreetInitial(Node *p0_node, Node *p1_node,
						  const ReachProbs &reach_probs, int pbd,
						  const string &action_sequence) {
  int nst = p0_node->Street();
  int pst = nst - 1;
  int pred_num_hole_card_pairs = Game::NumHoleCardPairs(pst);
  int max_card1 = Game::MaxCard() + 1;
  const CanonicalCards *pred_hands = trunk_hand_tree_->Hands(pst, pbd);
  unique_ptr<int []> pred_canons(CreatePredCanons(trunk_hand_tree_.get(), pst, pbd));
  shared_ptr<double []> vals(new double[pred_num_hole_card_pairs]);
  for (int i = 0; i < pred_num_hole_card_pairs; ++i) vals[i] = 0;
  // pbd is a global board index
  int ngbd_begin = BoardTree::SuccBoardBegin(pst, pbd, nst);
  int ngbd_end = BoardTree::SuccBoardEnd(pst, pbd, nst);
  int total_num_samples = 0;
  for (int ngbd = ngbd_begin; ngbd < ngbd_end; ++ngbd) {
    if (! has_continuation_[nst][ngbd]) continue;
    int board_variants = BoardTree::NumVariants(nst, ngbd);
    total_num_samples += board_variants;
    SetStreetBuckets(nst, ngbd);
    auto next_vals = Process(p0_node, p1_node, reach_probs, ngbd, action_sequence, nst);
    const CanonicalCards *hands = trunk_hand_tree_->Hands(nst, ngbd);
    int num_next_hands = hands->NumRaw();
    for (int nhcp = 0; nhcp < num_next_hands; ++nhcp) {
      const Card *cards = hands->Cards(nhcp);
      Card hi = cards[0];
      Card lo = cards[1];
      int enc = hi * max_card1 + lo;
      int pred_canon = pred_canons[enc];
      // Aren't we going to be scaling by board variants at other StreetInitial nodes in
      // the post phase?
      vals[pred_canon] += board_variants * next_vals[nhcp];
    }
  }
  if (total_num_samples > 0) {
    double d_total_num_samples = total_num_samples;
    int num_board_permutations = Game::StreetPermutations3(nst);
    double overweighting = num_board_permutations / d_total_num_samples;
    double scale_down = Game::StreetPermutations(nst);
    for (int ph = 0; ph < pred_num_hole_card_pairs; ++ph) {
      int pred_hand_variants = pred_hands->NumVariants(ph);
      if (pred_hand_variants > 0) {
	vals[ph] *= overweighting / (scale_down * pred_hand_variants);
      }
    }
    // Copy the canonical hand values to the non-canonical
    for (int ph = 0; ph < pred_num_hole_card_pairs; ++ph) {
      if (pred_hands->NumVariants(ph) == 0) {
	vals[ph] = vals[pred_canons[pred_hands->Canon(ph)]];
      }
    }
  }
  return vals;
}

shared_ptr<double []> PreResponder::Process(Node *p0_node, Node *p1_node,
					    const ReachProbs &reach_probs, int gbd,
					    const string &action_sequence, int last_st) {
  int st = p0_node->Street();
  if (p0_node->Terminal()) {
    double sum_opp_probs;
    unique_ptr<double []> total_card_probs(new double[Game::MaxCard() + 1]);
    const CanonicalCards *hands = trunk_hand_tree_->Hands(st, gbd);
    CommonBetResponseCalcs(st, hands, reach_probs.Get(responder_p_^1).get(), &sum_opp_probs,
			   total_card_probs.get());
    return Fold(p0_node, responder_p_, hands, reach_probs.Get(responder_p_^1).get(), sum_opp_probs,
		total_card_probs.get());
  }
  int pa = p0_node->PlayerActing();
  Node *node = pa == 0 ? p0_node : p1_node;
  Node *responding_node = pa == 0 ? p1_node : p0_node;
  if (st > last_st) {
    if (st == street_) {
      return Transition(p0_node, p1_node, reach_probs, gbd, action_sequence);
    } else {
      return StreetInitial(p0_node, p1_node, reach_probs, gbd, action_sequence);
    }
  }
  int num_succs = node->NumSuccs();
  unique_ptr<int []> succ_mapping = GetSuccMapping(node, responding_node);
  shared_ptr<ReachProbs []> succ_reach_probs =
    ReachProbs::CreateSuccReachProbs(node, gbd, gbd, trunk_hand_tree_->Hands(st, gbd), buckets_,
				     sumprobs_.get(), reach_probs, false);
  int num_hole_card_pairs = Game::NumHoleCardPairs(st);
  Card max_card1 = Game::MaxCard() + 1;
  const CanonicalCards *hands = trunk_hand_tree_->Hands(st, gbd);
  bool our_choice = (pa == responder_p_);
  shared_ptr<double []> vals(new double[num_hole_card_pairs]);
  for (int s = 0; s < num_succs; ++s) {
    int p0_s = pa == 0 ? s : succ_mapping[s];
    int p1_s = pa == 0 ? succ_mapping[s] : s;
    string new_action_sequence = action_sequence + node->ActionName(s);
    auto succ_vals = Process(p0_node->IthSucc(p0_s), p1_node->IthSucc(p1_s), succ_reach_probs[s],
			     gbd, new_action_sequence, st);
    if (our_choice) {
      // Only need to scale values at our-choice nodes
      for (int i = 0; i < num_hole_card_pairs; ++i) {
	const Card *cards = hands->Cards(i);
	Card hi = cards[0];
	Card lo = cards[1];
	int enc = hi * max_card1 + lo;
	double prob;
	double pred_prob = reach_probs.Get(pa, enc);
	if (pred_prob == 0) {
	  prob = 0;
	} else {
	  prob = succ_reach_probs[s].Get(pa, enc) / pred_prob;
	}
	succ_vals[i] *= prob;
      }
    }
    if (s == 0) {
      vals = succ_vals;
    } else {
      for (int i = 0; i < num_hole_card_pairs; ++i) {
	vals[i] += succ_vals[i];
      }
    }
  }
  return vals;
}

void PreResponder::Initialize(int responder_p) {
  // For asymmetric, we need both betting trees
  betting_trees_.reset(new BettingTrees(betting_abstraction_));

  int max_street = Game::MaxStreet();
  unique_ptr<bool []> streets(new bool[max_street + 1]);
  if (resolve_) {
    for (int st = 0; st <= max_street; ++st) streets[st] = st < street_;
  } else {
    for (int st = 0; st <= max_street; ++st) streets[st] = true;
  }

  if (betting_abstraction_.Asymmetric()) {
    sumprobs_.reset(new CFRValues(nullptr, streets.get(), 0, 0, buckets_, *betting_trees_));
  } else {
    sumprobs_.reset(new CFRValues(nullptr, streets.get(), 0, 0, buckets_,
				  betting_trees_->GetBettingTree()));
  }

  char dir[500];
  sprintf(dir, "%s/%s.%u.%s.%u.%u.%u.%s.%s", Files::OldCFRBase(), Game::GameName().c_str(),
	  Game::NumPlayers(), card_abstraction_.CardAbstractionName().c_str(), Game::NumRanks(),
	  Game::NumSuits(), Game::MaxStreet(), betting_abstraction_.BettingAbstractionName().c_str(),
	  cfr_config_.CFRConfigName().c_str());
  if (betting_abstraction_.Asymmetric()) {
    sumprobs_->ReadAsymmetric(dir, it_, *betting_trees_, "x", -1, true, quantize_);
  } else {
    sumprobs_->Read(dir, it_, betting_trees_->GetBettingTree(), "x", -1, true, quantize_);
  }

  if (! resolve_) {
    vcfr_->SetSumprobs(sumprobs_);
  }
}

double PreResponder::Go(int responder_p) {
  responder_p_ = responder_p;
  int num_hole_card_pairs = Game::NumHoleCardPairs(0);
  int num_remaining = Game::NumCardsInDeck() - Game::NumCardsForStreet(0);
  int num_opp_hole_card_pairs = num_remaining * (num_remaining - 1) / 2;

  shared_ptr<double []> vals;
  if (street_ == 0) {
    // Need to invoke the post responder directly
    HandTree hand_tree(0, 0, Game::MaxStreet());
    VCFRState state(responder_p, &hand_tree);
    vcfr_->SetStreetBuckets(0, 0, state);
    vals = vcfr_->Process(betting_trees_->Root(), betting_trees_->Root(), 0, state, 0);
  } else {
    unique_ptr<ReachProbs> reach_probs(ReachProbs::CreateRoot());
    SetStreetBuckets(0, 0);
    vals = Process(betting_trees_->Root(0), betting_trees_->Root(1), *reach_probs, 0, "x", 0);
  }
  double sum = 0;
  for (int hcp = 0; hcp < num_hole_card_pairs; ++hcp) {
    sum += vals[hcp];
  }
  double overall = sum / (num_hole_card_pairs * num_opp_hole_card_pairs);
  printf("P%i BR val: %f\n", responder_p, overall);
  fprintf(stderr, "%.1f secs spent resolving\n", resolving_secs_);
  if (num_resolves_ > 0) {
    fprintf(stderr, "Avg %.2f secs per resolve (%i resolves)\n", resolving_secs_ / num_resolves_,
	    num_resolves_);
  }
#if 0
  if (responder_p == 1) {
    Card aa_cards[2];
    aa_cards[0] = MakeCard(12, 3);
    aa_cards[1] = MakeCard(12, 2);
    int aa_hcp = HCPIndex(0, aa_cards);
    double aa_p1_val = vals[aa_hcp] / num_opp_hole_card_pairs;
    printf("AA P1: %f (hcp %i)\n", aa_p1_val, aa_hcp);
  }
#endif
  return overall;
}

static void Usage(const char *prog_name) {
  fprintf(stderr, "USAGE: %s <game params> <card params> <betting abstraction params> <CFR params> "
	  "<it> [quantize|raw] <street> <num sampled boards> (<resolve card params> "
	  "<resolve betting params> <resolve CFR params> <resolve its>)\n", prog_name);
  fprintf(stderr, "\n");
  fprintf(stderr, "Specify 0 for <num sampled boards> to not sample\n");
  exit(-1);
}

int main(int argc, char *argv[]) {
  if (argc != 9 && argc != 13) Usage(argv[0]);
  Files::Init();
  unique_ptr<Params> game_params = CreateGameParams();
  game_params->ReadFromFile(argv[1]);
  Game::Initialize(*game_params);
  unique_ptr<Params> card_params = CreateCardAbstractionParams();
  card_params->ReadFromFile(argv[2]);
  unique_ptr<CardAbstraction>
    card_abstraction(new CardAbstraction(*card_params));
  unique_ptr<Params> betting_params = CreateBettingAbstractionParams();
  betting_params->ReadFromFile(argv[3]);
  unique_ptr<BettingAbstraction>
    betting_abstraction(new BettingAbstraction(*betting_params));
  unique_ptr<Params> cfr_params = CreateCFRParams();
  cfr_params->ReadFromFile(argv[4]);
  unique_ptr<CFRConfig>
    cfr_config(new CFRConfig(*cfr_params));
  int it;
  if (sscanf(argv[5], "%i", &it) != 1) Usage(argv[0]);
  bool quantize;
  string qa = argv[6];
  if (qa == "quantize") quantize = true;
  else if (qa == "raw") quantize = false;
  else                  Usage(argv[0]);
  int street, num_sampled_boards;
  if (sscanf(argv[7], "%i", &street) != 1)             Usage(argv[0]);
  if (sscanf(argv[8], "%i", &num_sampled_boards) != 1) Usage(argv[0]);

  bool resolve = false;
  unique_ptr<CardAbstraction> subgame_card_abstraction;
  unique_ptr<BettingAbstraction> subgame_betting_abstraction;
  unique_ptr<CFRConfig> subgame_cfr_config;
  int num_resolve_its = 0;
  if (argc == 12) {
    resolve = true;
    unique_ptr<Params> subgame_card_params = CreateCardAbstractionParams();
    subgame_card_params->ReadFromFile(argv[9]);
    subgame_card_abstraction.reset(new CardAbstraction(*subgame_card_params));
    unique_ptr<Params> subgame_betting_params = CreateBettingAbstractionParams();
    subgame_betting_params->ReadFromFile(argv[10]);
    subgame_betting_abstraction.reset(new BettingAbstraction(*subgame_betting_params));
    unique_ptr<Params> subgame_cfr_params = CreateCFRParams();
    subgame_cfr_params->ReadFromFile(argv[11]);
    subgame_cfr_config.reset(new CFRConfig(*subgame_cfr_params));
    if (sscanf(argv[12], "%i", &num_resolve_its) != 1) Usage(argv[0]);
  }
  
  int num_players = Game::NumPlayers();
  if (num_players != 2) {
    fprintf(stderr, "Only heads-up supported\n");
    exit(-1);
  }

  if (resolve && street == 0) {
    fprintf(stderr, "Cannot resolve if street is 0\n");
    exit(-1);
  }
  
  BoardTree::Create();
  BoardTree::CreateLookup();
  BoardTree::BuildBoardCounts();

  // Should get created when needed
  // HandValueTree::Create();

  Buckets buckets(*card_abstraction, false);
  PreResponder pre_responder(*card_abstraction, *betting_abstraction, *cfr_config, buckets, it,
			     street, num_sampled_boards, quantize, resolve,
			     *subgame_card_abstraction, *subgame_betting_abstraction,
			     *subgame_cfr_config, num_resolve_its);
  double gap = 0;
  for (int responder_p = 0; responder_p < 2; ++responder_p) {
    pre_responder.Initialize(responder_p);
    // post_responder.Initialize(responder_p);
    double val = pre_responder.Go(responder_p);
    gap += val;
  }
  printf("Gap: %f\n", gap);
  printf("Exploitability: %.2f mbb/g\n", ((gap / 2.0) / num_players) * 1000.0);
}
