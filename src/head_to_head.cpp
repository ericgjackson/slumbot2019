// head_to_head assesses head to head performance between two systems with two nice features:
// 1) You can sample final street boards evaluated leading to drastic speedups at the expense
// of some accuracy.
// 2) You can tell either or both systems to resolve subgames.
//
// Unlike play we do not sample hands.  In the simplest scenario, we traverse the betting tree,
// tracking each player's range at each node.  At terminal nodes we evaluate range vs. range EV.
//
// This works nicely with resolving.  We only need to resolve a given subgame at most once.  If we
// are sampling, we only resolve the subgames needed.
//
// I don't think I can support multiplayer.
//
// We support asymmetric abstractions now, I believe.
//
// Is there much wasted work recomputing reach probs for streets prior to the final street?
// A bit, but maybe it's not significant.  May only be sampling some turn boards so it might
// be a waste to precompute turn reach probs for all turn boards.  Could do the preflop and maybe
// the flop.
//
// Leaking CanonicalCards objects?
//
// If we do turn resolving and sample all river boards then we get lots of redundant turn
// resolving.  For each river board we resolve every turn subtree - even though lots of river
// boards correspond to the same turn board.

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h> // gettimeofday()

#include <algorithm>
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
#include "subgame_utils.h" // CreateSubtrees()
#include "unsafe_eg_cfr.h"

using std::pair;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;

class Player {
public:
  Player(const BettingAbstraction &a_ba, const BettingAbstraction &b_ba,
	 const CardAbstraction &a_ca, const CardAbstraction &b_ca, const CFRConfig &a_cc,
	 const CFRConfig &b_cc, int a_it, int b_it, int resolve_st, bool resolve_a, bool resolve_b,
	 const CardAbstraction &as_ca, const BettingAbstraction &as_ba, const CFRConfig &as_cc,
	 const CardAbstraction &bs_ca, const BettingAbstraction &bs_ba, const CFRConfig &bc_cc,
	 bool a_quantize, bool b_quantize);
  ~Player(void) {}
  void Go(int num_sampled_max_street_boards, bool deterministic);
private:
  void Showdown(Node *a_node, Node *b_node, const ReachProbs &reach_probs);
  void Fold(Node *a_node, Node *b_node, const ReachProbs &reach_probs);
  void Nonterminal(Node *a_node, Node *b_node, const string &action_sequence,
		   const ReachProbs &reach_probs);
  void Walk(Node *a_node, Node *b_node, const string &action_sequence,
	    const ReachProbs &reach_probs, int last_st);
  void ProcessMaxStreetBoard(int msbd);

  const BettingAbstraction &a_betting_abstraction_;
  const BettingAbstraction &b_betting_abstraction_;
  const BettingAbstraction &a_subgame_betting_abstraction_;
  const BettingAbstraction &b_subgame_betting_abstraction_;
  // bool a_asymmetric_;
  // bool b_asymmetric_;
  unique_ptr<BettingTrees> a_betting_trees_;
  unique_ptr<BettingTrees> b_betting_trees_;
  shared_ptr<Buckets> a_base_buckets_;
  shared_ptr<Buckets> b_base_buckets_;
  shared_ptr<CFRValues> a_probs_;
  shared_ptr<CFRValues> b_probs_;
  int resolve_st_;
  bool resolve_a_;
  bool resolve_b_;
  // When we resolve a street, the board index may change.  This is why we have separate
  // a boards and b boards.  Only one player may be resolving.
  unique_ptr<int []> a_gbds_;
  unique_ptr<int []> a_lbds_;
  unique_ptr<int []> b_gbds_;
  unique_ptr<int []> b_lbds_;
  // The number of times we sampled this board.
  int num_samples_;
  int msbd_;
  int b_pos_;
  // shared_ptr<HandTree> hand_tree_;
  shared_ptr<HandTree> resolve_hand_tree_;
  unique_ptr<shared_ptr<CanonicalCards> []> street_hands_;
  double sum_b_outcomes_;
  double sum_p0_outcomes_;
  double sum_p1_outcomes_;
  double sum_weights_;
  shared_ptr<Buckets> a_subgame_buckets_;
  shared_ptr<Buckets> b_subgame_buckets_;
  unique_ptr<BettingTrees> a_subtrees_;
  unique_ptr<BettingTrees> b_subtrees_;
  unique_ptr<EGCFR> a_eg_cfr_;
  unique_ptr<EGCFR> b_eg_cfr_;
  int num_subgame_its_;
  int num_resolves_;
  double resolving_secs_;
};

Player::Player(const BettingAbstraction &a_ba, const BettingAbstraction &b_ba,
	       const CardAbstraction &a_ca, const CardAbstraction &b_ca, const CFRConfig &a_cc,
	       const CFRConfig &b_cc, int a_it, int b_it, int resolve_st, bool resolve_a,
	       bool resolve_b, const CardAbstraction &as_ca, const BettingAbstraction &as_ba,
	       const CFRConfig &as_cc, const CardAbstraction &bs_ca,
	       const BettingAbstraction &bs_ba, const CFRConfig &bs_cc, bool a_quantize,
	       bool b_quantize) :
  a_betting_abstraction_(a_ba), b_betting_abstraction_(b_ba),
  a_subgame_betting_abstraction_(as_ba), b_subgame_betting_abstraction_(bs_ba) {
  int max_street = Game::MaxStreet();
  a_gbds_.reset(new int[max_street + 1]);
  a_lbds_.reset(new int[max_street + 1]);
  b_gbds_.reset(new int[max_street + 1]);
  b_lbds_.reset(new int[max_street + 1]);
  a_gbds_[0] = 0;
  a_lbds_[0] = 0;
  b_gbds_[0] = 0;
  b_lbds_[0] = 0;
  sum_b_outcomes_ = 0;
  sum_p0_outcomes_ = 0;
  sum_p1_outcomes_ = 0;
  sum_weights_ = 0;
  resolve_a_ = resolve_a;
  resolve_b_ = resolve_b;
  resolve_st_ = resolve_st;
  num_subgame_its_ = 200;

  a_base_buckets_.reset(new Buckets(a_ca, false));
  if (a_ca.CardAbstractionName() == b_ca.CardAbstractionName()) {
    fprintf(stderr, "Sharing buckets\n");
    b_base_buckets_ = a_base_buckets_;
  } else {
    fprintf(stderr, "Not sharing buckets\n");
    b_base_buckets_.reset(new Buckets(b_ca, false));
  }
  BoardTree::Create();
  BoardTree::CreateLookup();
  BoardTree::BuildBoardCounts();
  BoardTree::BuildPredBoards();

  a_betting_trees_.reset(new BettingTrees(a_ba));
  b_betting_trees_.reset(new BettingTrees(b_ba));

  street_hands_.reset(new shared_ptr<CanonicalCards>[max_street + 1]);
  
  bool shared_probs = 
    (a_ca.CardAbstractionName().c_str() == b_ca.CardAbstractionName() &&
     a_ba.BettingAbstractionName().c_str() == b_ba.BettingAbstractionName() &&
     a_cc.CFRConfigName() == b_cc.CFRConfigName() && a_it == b_it);
  unique_ptr<bool []> a_streets(new bool[max_street + 1]);
  unique_ptr<bool []> b_streets(new bool[max_street + 1]);
  if (resolve_a_ && resolve_b_) {
    for (int st = 0; st <= max_street; ++st) {
      a_streets[st] = (st < resolve_st_);
      b_streets[st] = (st < resolve_st_);
    }
  } else if (! resolve_a_ && ! resolve_b_) {
    for (int st = 0; st <= max_street; ++st) {
      a_streets[st] = true;
      b_streets[st] = true;
    }
  } else {
    // One system is being resolved
    if (shared_probs) {
      for (int st = 0; st <= max_street; ++st) {
	a_streets[st] = true;
	b_streets[st] = true;
      }
    } else {
      for (int st = 0; st <= max_street; ++st) {
	a_streets[st] = (st < resolve_st_ || ! resolve_a_);
	b_streets[st] = (st < resolve_st_ || ! resolve_b_);
      }
    }
  }

  a_probs_.reset(new CFRValues(nullptr, a_streets.get(), 0, 0, *a_base_buckets_,
			       *a_betting_trees_));
  char dir[500];
  
  sprintf(dir, "%s/%s.%u.%s.%u.%u.%u.%s.%s", Files::OldCFRBase(),
	  Game::GameName().c_str(), Game::NumPlayers(),
	  a_ca.CardAbstractionName().c_str(),
	  Game::NumRanks(), Game::NumSuits(), Game::MaxStreet(),
	  a_ba.BettingAbstractionName().c_str(),
	  a_cc.CFRConfigName().c_str());
  if (a_ba.Asymmetric()) {
    a_probs_->ReadAsymmetric(dir, a_it, *a_betting_trees_, "x", -1, true, a_quantize);
  } else {
    a_probs_->Read(dir, a_it, a_betting_trees_->GetBettingTree(), "x", -1, true, a_quantize);
  }

  if (a_ca.CardAbstractionName().c_str() == b_ca.CardAbstractionName() &&
      a_ba.BettingAbstractionName().c_str() == b_ba.BettingAbstractionName() &&
      a_cc.CFRConfigName() == b_cc.CFRConfigName() && a_it == b_it ) {
    fprintf(stderr, "Sharing probs between A and B\n");
    b_probs_ = a_probs_;
  } else {
    fprintf(stderr, "A and B do not share probs\n");
    b_probs_.reset(new CFRValues(nullptr, b_streets.get(), 0, 0, *b_base_buckets_,
				 *b_betting_trees_));
    sprintf(dir, "%s/%s.%u.%s.%u.%u.%u.%s.%s", Files::OldCFRBase(), Game::GameName().c_str(),
	    Game::NumPlayers(), b_ca.CardAbstractionName().c_str(), Game::NumRanks(),
	    Game::NumSuits(), Game::MaxStreet(), b_ba.BettingAbstractionName().c_str(),
	    b_cc.CFRConfigName().c_str());
    if (b_ba.Asymmetric()) {
      b_probs_->ReadAsymmetric(dir, b_it, *b_betting_trees_, "x", -1, true, b_quantize);
    } else {
      b_probs_->Read(dir, b_it, b_betting_trees_->GetBettingTree(), "x", -1, true, b_quantize);
    }
  }

  // Check for dups for buckets
  if (resolve_a_) {
    a_subgame_buckets_.reset(new Buckets(as_ca, false));
    a_eg_cfr_.reset(new UnsafeEGCFR(as_ca, a_ca, a_ba, as_cc, a_cc, *a_subgame_buckets_, 1));
  }
  if (resolve_b_) {
    b_subgame_buckets_.reset(new Buckets(bs_ca, false));
    b_eg_cfr_.reset(new UnsafeEGCFR(bs_ca, b_ca, b_ba, bs_cc, b_cc, *b_subgame_buckets_, 1));
  }
}

// Compute outcome from B's perspective
void Player::Showdown(Node *a_node, Node *b_node, const ReachProbs &reach_probs) {
  Card max_card1 = Game::MaxCard() + 1;

  // double *a_probs = b_pos_ == 0 ? reach_probs[1].get() : reach_probs[0].get();
  // double *b_probs = b_pos_ == 0 ? reach_probs[0].get() : reach_probs[1].get();
  double *a_probs = reach_probs.Get(b_pos_^1).get();
  double *b_probs = reach_probs.Get(b_pos_).get();
  unique_ptr<double []> cum_opp_card_probs(new double[52]);
  unique_ptr<double []> total_opp_card_probs(new double[52]);
  for (Card c = 0; c < max_card1; ++c) {
    cum_opp_card_probs[c] = 0;
    total_opp_card_probs[c] = 0;
  }
  int max_street = Game::MaxStreet();
  const CanonicalCards *hands = street_hands_[max_street].get();
  int num_hole_card_pairs = Game::NumHoleCardPairs(max_street);
  double sum_opp_probs = 0;
  for (int hcp = 0; hcp < num_hole_card_pairs; ++hcp) {
    const Card *cards = hands->Cards(hcp);
    Card hi = cards[0];
    Card lo = cards[1];
    int enc = hi * max_card1 + lo;
    double opp_prob = a_probs[enc];
    total_opp_card_probs[hi] += opp_prob;
    total_opp_card_probs[lo] += opp_prob;
    sum_opp_probs += opp_prob;
    if (opp_prob > 1.0) {
      fprintf(stderr, "Showdown: opp_prob (a) %f hcp %u\n", opp_prob, hcp);
      exit(-1);
    }
  }

  double opp_cum_prob = 0;
  unique_ptr<double []> win_probs(new double[num_hole_card_pairs]);
  double half_pot = a_node->LastBetTo();
  double sum_our_vals = 0, sum_joint_probs = 0;

  int j = 0;
  while (j < num_hole_card_pairs) {
    int last_hand_val = hands->HandValue(j);
    int begin_range = j;
    // Make three passes through the range of equally strong hands
    // First pass computes win counts for each hand and finds end of range
    // Second pass updates cumulative counters
    // Third pass computes lose counts for each hand
    while (j < num_hole_card_pairs) {
      const Card *cards = hands->Cards(j);
      Card hi = cards[0];
      Card lo = cards[1];
      int hand_val = hands->HandValue(j);
      if (hand_val != last_hand_val) break;
      win_probs[j] = opp_cum_prob - cum_opp_card_probs[hi] - cum_opp_card_probs[lo];
      ++j;
    }
    // Positions begin_range...j-1 (inclusive) all have the same hand value
    for (int k = begin_range; k < j; ++k) {
      const Card *cards = hands->Cards(k);
      Card hi = cards[0];
      Card lo = cards[1];
      int enc = hi * max_card1 + lo;
      double opp_prob = a_probs[enc];
      if (opp_prob <= 0) continue;
      cum_opp_card_probs[hi] += opp_prob;
      cum_opp_card_probs[lo] += opp_prob;
      opp_cum_prob += opp_prob;
    }
    for (int k = begin_range; k < j; ++k) {
      const Card *cards = hands->Cards(k);
      Card hi = cards[0];
      Card lo = cards[1];
      int enc = hi * max_card1 + lo;
      double our_prob = b_probs[enc];
      double better_hi_prob = total_opp_card_probs[hi] - cum_opp_card_probs[hi];
      double better_lo_prob = total_opp_card_probs[lo] - cum_opp_card_probs[lo];
      double lose_prob = (sum_opp_probs - opp_cum_prob) - better_hi_prob - better_lo_prob;
      sum_our_vals += our_prob * (win_probs[k] - lose_prob) * half_pot;
      // This is the sum of all A reach probabilities consistent with B holding <hi, lo>.
      double sum_consistent_opp_probs = sum_opp_probs + a_probs[enc] -
	total_opp_card_probs[hi] - total_opp_card_probs[lo];
      sum_joint_probs += our_prob * sum_consistent_opp_probs;
    }
  }

  // Scale to account for frequency of board
  double wtd_sum_our_vals = sum_our_vals * (double)num_samples_;
  double wtd_sum_joint_probs = sum_joint_probs * (double)num_samples_;
  sum_b_outcomes_ += wtd_sum_our_vals;
  if (b_pos_ == 0) {
    sum_p0_outcomes_ += wtd_sum_our_vals;
    sum_p1_outcomes_ -= wtd_sum_our_vals;
  } else {
    sum_p0_outcomes_ -= wtd_sum_our_vals;
    sum_p1_outcomes_ += wtd_sum_our_vals;
  }
  sum_weights_ += wtd_sum_joint_probs;
}
  
// Compute outcome from B's perspective
void Player::Fold(Node *a_node, Node *b_node, const ReachProbs &reach_probs) {
  Card max_card1 = Game::MaxCard() + 1;

  double half_pot = a_node->LastBetTo();
  // Player acting encodes player remaining at fold nodes
  int rem_p = a_node->PlayerActing();
  // Outcomes are from B's perspective
  if (b_pos_ != rem_p) {
    // B has folded
    half_pot = -half_pot;
  }
  int max_street = Game::MaxStreet();
  // Note: we are going to iterate through max street hands even if this is a pre-max-street
  // node.
  const CanonicalCards *hands = street_hands_[max_street].get();
  double sum_our_vals = 0, sum_joint_probs = 0;

  // double *a_probs = b_pos_ == 0 ? reach_probs[1].get() : reach_probs[0].get();
  // double *b_probs = b_pos_ == 0 ? reach_probs[0].get() : reach_probs[1].get();
  double *a_probs = reach_probs.Get(b_pos_^1).get();
  double *b_probs = reach_probs.Get(b_pos_).get();
  unique_ptr<double []> cum_opp_card_probs(new double[52]);
  unique_ptr<double []> total_opp_card_probs(new double[52]);
  for (Card c = 0; c < max_card1; ++c) {
    cum_opp_card_probs[c] = 0;
    total_opp_card_probs[c] = 0;
  }

  double sum_opp_probs = 0;
  // Always iterate through hole card pairs consistent with the sampled *max street* board, even if
  // this is a pre-max-street node.
  int num_hole_card_pairs = Game::NumHoleCardPairs(max_street);
  for (int hcp = 0; hcp < num_hole_card_pairs; ++hcp) {
    const Card *cards = hands->Cards(hcp);
    Card hi = cards[0];
    Card lo = cards[1];
    int enc = hi * max_card1 + lo;
    double opp_prob = a_probs[enc];
    total_opp_card_probs[hi] += opp_prob;
    total_opp_card_probs[lo] += opp_prob;
    sum_opp_probs += opp_prob;
  }

  for (int i = 0; i < num_hole_card_pairs; ++i) {
    const Card *cards = hands->Cards(i);
    Card hi = cards[0];
    Card lo = cards[1];
    int enc = hi * max_card1 + lo;
    double our_prob = b_probs[enc];
    // This is the sum of all the A reach probabilities consistent with B holding <hi, lo>.
    double sum_consistent_opp_probs = sum_opp_probs + a_probs[enc] -
      total_opp_card_probs[hi] - total_opp_card_probs[lo];
    sum_our_vals += our_prob * half_pot * sum_consistent_opp_probs;
    sum_joint_probs += our_prob * sum_consistent_opp_probs;    
  }

  // Scale to account for frequency of board
  double wtd_sum_our_vals = sum_our_vals * num_samples_;
  double wtd_sum_joint_probs = sum_joint_probs * num_samples_;
  sum_b_outcomes_ += wtd_sum_our_vals;
  if (b_pos_ == 0) {
    sum_p0_outcomes_ += wtd_sum_our_vals;
    sum_p1_outcomes_ -= wtd_sum_our_vals;
  } else {
    sum_p0_outcomes_ -= wtd_sum_our_vals;
    sum_p1_outcomes_ += wtd_sum_our_vals;
  }
  sum_weights_ += wtd_sum_joint_probs;
}

void Player::Nonterminal(Node *a_node, Node *b_node, const string &action_sequence,
			 const ReachProbs &reach_probs) {
  int st = a_node->Street();
  int pa = a_node->PlayerActing();
  // A and B may have different numbers of succs.  I think this may only be the case if either
  // or both systems are asymmetric.  We need to map from succ indices in one space to succs
  // indices in the other space.
  Node *acting_node = pa == b_pos_ ? b_node : a_node;
  Node *opp_node = pa == b_pos_ ? a_node : b_node;
  int acting_num_succs = acting_node->NumSuccs();
  int opp_num_succs = opp_node->NumSuccs();
  // It's allowed for the opponent to have more succs than the acting player, but not vice versa.
  if (acting_num_succs > opp_num_succs) {
    fprintf(stderr, "acting_num_succs (%i) > opp_num_succs (%i)\n", acting_num_succs,
	    opp_num_succs);
    fprintf(stderr, "b_pos_ %i pa %i st %i\n", b_pos_, pa, st);
    fprintf(stderr, "a num succs %i nt %i pa %i\n", a_node->NumSuccs(), a_node->NonterminalID(),
	    a_node->PlayerActing());
    fprintf(stderr, "b num succs %i nt %i pa %i\n", b_node->NumSuccs(), b_node->NonterminalID(),
	    b_node->PlayerActing());
    exit(-1);
  }
  unique_ptr<int []> succ_mapping = GetSuccMapping(acting_node, opp_node);
  
  shared_ptr<ReachProbs []> succ_reach_probs;
  // shared_ptr<double []> **succ_reach_probs;
  if (pa == b_pos_) {
    // This doesn't support multiplayer yet
    const CFRValues *sumprobs;
    if (resolve_b_ && st >= resolve_st_) {
      sumprobs = b_eg_cfr_->Sumprobs().get();
    } else {
      sumprobs = b_probs_.get();
    }
    const Buckets &buckets =
      (resolve_b_ && st >= resolve_st_) ? *b_subgame_buckets_ : *b_base_buckets_;
    const CanonicalCards *hands = street_hands_[st].get();
    succ_reach_probs = ReachProbs::CreateSuccReachProbs(b_node, b_gbds_[st], b_lbds_[st], hands,
							buckets, sumprobs, reach_probs, false);
  } else {
    const CFRValues *sumprobs;
    if (resolve_a_ && st >= resolve_st_) {
      sumprobs = a_eg_cfr_->Sumprobs().get();
    } else {
      sumprobs = a_probs_.get();
    }
    const Buckets &buckets =
      (resolve_a_ && st >= resolve_st_) ? *a_subgame_buckets_ : *a_base_buckets_;
    const CanonicalCards *hands = street_hands_[st].get();
    succ_reach_probs = ReachProbs::CreateSuccReachProbs(a_node, a_gbds_[st], a_lbds_[st], hands,
							buckets, sumprobs, reach_probs, false);
  }
  for (int s = 0; s < acting_num_succs; ++s) {
    string action;
    Node *a_succ, *b_succ;
    if (pa == b_pos_) {
      b_succ = b_node->IthSucc(s);
      a_succ = a_node->IthSucc(succ_mapping[s]);
      action = b_node->ActionName(s);
    } else {
      a_succ = a_node->IthSucc(s);
      b_succ = b_node->IthSucc(succ_mapping[s]);
      action = a_node->ActionName(s);
    }
    Walk(a_succ, b_succ, action_sequence + action, succ_reach_probs[s], st);
  }
#if 0
  for (int s = 0; s < acting_num_succs; ++s) {
    delete [] succ_reach_probs[s];
  }
  delete [] succ_reach_probs;
#endif
}
 
void Player::Walk(Node *a_node, Node *b_node, const string &action_sequence,
		  const ReachProbs &reach_probs, int last_st) {
  int st = a_node->Street();
  if (st > last_st && st == resolve_st_) {
    Node *next_a_node, *next_b_node;
    if (resolve_a_ && a_node->LastBetTo() < a_betting_abstraction_.StackSize()) {
      a_subtrees_.reset(CreateSubtrees(st, a_node->PlayerActing(), a_node->LastBetTo(), -1,
				       a_subgame_betting_abstraction_));
      int max_street = Game::MaxStreet();
      int root_bd;
      if (st == max_street) root_bd = msbd_;
      else                  root_bd = BoardTree::PredBoard(msbd_, st);
      struct timespec start, finish;
      clock_gettime(CLOCK_MONOTONIC, &start);
      a_eg_cfr_->SolveSubgame(a_subtrees_.get(), root_bd, reach_probs, action_sequence,
			      resolve_hand_tree_.get(), nullptr, -1, true, num_subgame_its_);
      clock_gettime(CLOCK_MONOTONIC, &finish);
      resolving_secs_ += (finish.tv_sec - start.tv_sec);
      resolving_secs_ += (finish.tv_nsec - start.tv_nsec) / 1000000000.0;
      ++num_resolves_;
      next_a_node = a_subtrees_->Root();
      for (int st1 = st; st1 <= max_street; ++st1) {
	int gbd;
	if (st1 == max_street) gbd = msbd_;
	else                   gbd = BoardTree::PredBoard(msbd_, st1);
	a_lbds_[st1] = BoardTree::LocalIndex(st, root_bd, st1, gbd);
      }
    } else {
      next_a_node = a_node;
    }
    if (resolve_b_ && b_node->LastBetTo() < b_betting_abstraction_.StackSize()) {
      b_subtrees_.reset(CreateSubtrees(st, b_node->PlayerActing(), b_node->LastBetTo(), -1,
				       b_subgame_betting_abstraction_));
      int max_street = Game::MaxStreet();
      int root_bd;
      if (st == max_street) root_bd = msbd_;
      else                  root_bd = BoardTree::PredBoard(msbd_, st);
      printf("Resolving %s b_pos_ %i id %i lbt %i\n", action_sequence.c_str(), b_pos_,
	     b_node->NonterminalID(), b_node->LastBetTo());
      fflush(stdout);
      struct timespec start, finish;
      clock_gettime(CLOCK_MONOTONIC, &start);
      b_eg_cfr_->SolveSubgame(b_subtrees_.get(), root_bd, reach_probs, action_sequence,
			      resolve_hand_tree_.get(), nullptr, -1, true, num_subgame_its_);
      clock_gettime(CLOCK_MONOTONIC, &finish);
      resolving_secs_ += (finish.tv_sec - start.tv_sec);
      resolving_secs_ += (finish.tv_nsec - start.tv_nsec) / 1000000000.0;
      ++num_resolves_;
      next_b_node = b_subtrees_->Root();
      for (int st1 = st; st1 <= max_street; ++st1) {
	int gbd;
	if (st1 == max_street) gbd = msbd_;
	else                   gbd = BoardTree::PredBoard(msbd_, st1);
	b_lbds_[st1] = BoardTree::LocalIndex(st, root_bd, st1, gbd);
      }
    } else {
      next_b_node = b_node;
    }
    Walk(next_a_node, next_b_node, action_sequence, reach_probs, st);
    // Release the memory now.  And make sure stale sumprobs are not accidentally used later.
    if (resolve_a_) {
      a_eg_cfr_->ClearSumprobs();
    }
    if (resolve_b_) {
      b_eg_cfr_->ClearSumprobs();
    }
    return;
  }
  if (a_node->Terminal()) {
    if (! b_node->Terminal()) {
      fprintf(stderr, "A terminal B nonterminal?!?\n");
      exit(-1);
    }
    if (a_node->Showdown()) {
      Showdown(a_node, b_node, reach_probs);
    } else {
      Fold(a_node, b_node, reach_probs);
    }
  } else {
    if (b_node->Terminal()) {
      fprintf(stderr, "A nonterminal B terminal?!?\n");
      exit(-1);
    }
    Nonterminal(a_node, b_node, action_sequence, reach_probs);
  }
}

void Player::ProcessMaxStreetBoard(int msbd) {
  int max_street = Game::MaxStreet();
  msbd_ = msbd;
  a_gbds_[max_street] = msbd_;
  a_lbds_[max_street] = msbd_;
  b_gbds_[max_street] = msbd_;
  b_lbds_[max_street] = msbd_;
  for (int st = 1; st < max_street; ++st) {
    int pbd = BoardTree::PredBoard(msbd_, st);
    a_gbds_[st] = pbd;
    a_lbds_[st] = pbd;
    b_gbds_[st] = pbd;
    b_lbds_[st] = pbd;
  }

  if (resolve_a_ || resolve_b_) {
    if (resolve_st_ < max_street) {
      resolve_hand_tree_.reset(new HandTree(resolve_st_, BoardTree::PredBoard(msbd_, resolve_st_),
					    max_street));
    } else {
      resolve_hand_tree_.reset(new HandTree(max_street, msbd_, max_street));
    }
  }
  
  for (int st = 0; st <= max_street; ++st) {
    int num_board_cards = Game::NumBoardCards(st);
    int bd = st == max_street ? msbd_ : BoardTree::PredBoard(msbd_, st);
    const Card *board = BoardTree::Board(st, bd);
    int sg = BoardTree::SuitGroups(st, bd);
    shared_ptr<CanonicalCards> hands(new CanonicalCards(2, board, num_board_cards, sg, false));
    if (st == max_street) {
      hands->SortByHandStrength(board);
    }
    street_hands_[st] = hands;
  }
  int num_players = Game::NumPlayers();
  unique_ptr<ReachProbs> reach_probs(ReachProbs::CreateRoot());
  Node *a_root, *b_root;
  for (b_pos_ = 0; b_pos_ < num_players; ++b_pos_) {
    b_root = b_betting_trees_->Root(b_pos_);
    a_root = a_betting_trees_->Root(b_pos_^1);
    Walk(a_root, b_root, "x", *reach_probs, 0);
  }
}

void Player::Go(int num_sampled_max_street_boards, bool deterministic) {
  num_resolves_ = 0;
  resolving_secs_ = 0;
  int max_street = Game::MaxStreet();
  int num_max_street_boards = BoardTree::NumBoards(max_street);
  if (num_sampled_max_street_boards == 0 ||
      num_sampled_max_street_boards > num_max_street_boards) {
    num_sampled_max_street_boards = num_max_street_boards;
  }

  if (num_sampled_max_street_boards == num_max_street_boards) {
    fprintf(stderr, "Processing all max street boards\n");
    for (int bd = 0; bd < num_max_street_boards; ++bd) {
      num_samples_ = BoardTree::BoardCount(max_street, bd);
      ProcessMaxStreetBoard(bd);
    }
  } else {
    unique_ptr<int []> max_street_board_samples(new int[num_max_street_boards]);
    for (int bd = 0; bd < num_max_street_boards; ++bd) max_street_board_samples[bd] = 0;
    struct drand48_data rand_buf;
    if (deterministic) {
      srand48_r(0, &rand_buf);
    } else {
      struct timeval time; 
      gettimeofday(&time, NULL);
      srand48_r((time.tv_sec * 1000) + (time.tv_usec / 1000), &rand_buf);
    }
    vector< pair<double, int> > v;
    for (int bd = 0; bd < num_max_street_boards; ++bd) {
      int board_count = BoardTree::BoardCount(max_street, bd);
      for (int i = 0; i < board_count; ++i) {
	double r;
	drand48_r(&rand_buf, &r);
	v.push_back(std::make_pair(r, bd));
      }
    }
    std::sort(v.begin(), v.end());
    for (int i = 0; i < num_sampled_max_street_boards; ++i) {
      int bd = v[i].second;
      ++max_street_board_samples[bd];
    }

    int so_far = 0;
    for (int bd = 0; bd < num_max_street_boards; ++bd) {
      num_samples_ = max_street_board_samples[bd];
      if (num_samples_ == 0) continue;
      ProcessMaxStreetBoard(bd);
      so_far += num_samples_;
      fprintf(stderr, "Processed %i/%i\n", so_far, num_sampled_max_street_boards);
    }
  }

  double avg_b_outcome = sum_b_outcomes_ / sum_weights_;
  // avg_b_outcome is in units of the small blind
  double b_mbb_g = (avg_b_outcome / 2.0) * 1000.0;
  fprintf(stderr, "Avg B outcome: %f (%.1f mbb/g)\n", avg_b_outcome, b_mbb_g);
  double avg_p1_outcome = sum_p1_outcomes_ / sum_weights_;
  double p1_mbb_g = (avg_p1_outcome / 2.0) * 1000.0;
  fprintf(stderr, "Avg P1 outcome: %f (%.1f mbb/g)\n", avg_p1_outcome, p1_mbb_g);
  fprintf(stderr, "%.1f secs spent resolving\n", resolving_secs_);
  if (num_resolves_ > 0) {
    fprintf(stderr, "Avg %.2f secs per resolve (%i resolves)\n", resolving_secs_ / num_resolves_,
	    num_resolves_);
  }
}

static void Usage(const char *prog_name) {
  fprintf(stderr, "USAGE: %s <game params> <A card params> <B card params> "
	  "<A betting abstraction params> <B betting abstraction params> <A CFR params> "
	  "<B CFR params> <A it> <B it> <num sampled max street boards> [quantize|raw] "
	  "[quantize|raw] [deterministic|nondeterministic] <resolve A> <resolve B> "
	  "(<resolve st>) (<A resolve card params> <A resolve betting params> "
	  "<A resolve CFR config>) (<B resolve card params> <B resolve betting params> "
	  "<B resolve CFR config>)\n", prog_name);
  fprintf(stderr, "\n");
  fprintf(stderr, "Specify 0 for <num sampled max street boards> to not sample\n");
  fprintf(stderr, "<resolve A> and <resolve B> must be \"true\" or \"false\"\n");
  exit(-1);
}

int main(int argc, char *argv[]) {
  if (argc != 16 && argc != 20 && argc != 23) Usage(argv[0]);
  Files::Init();
  unique_ptr<Params> game_params = CreateGameParams();
  game_params->ReadFromFile(argv[1]);
  Game::Initialize(*game_params);
  unique_ptr<Params> a_card_params = CreateCardAbstractionParams();
  a_card_params->ReadFromFile(argv[2]);
  unique_ptr<CardAbstraction>
    a_card_abstraction(new CardAbstraction(*a_card_params));
  unique_ptr<Params> b_card_params = CreateCardAbstractionParams();
  b_card_params->ReadFromFile(argv[3]);
  unique_ptr<CardAbstraction>
    b_card_abstraction(new CardAbstraction(*b_card_params));
  unique_ptr<Params> a_betting_params = CreateBettingAbstractionParams();
  a_betting_params->ReadFromFile(argv[4]);
  unique_ptr<BettingAbstraction>
    a_betting_abstraction(new BettingAbstraction(*a_betting_params));
  unique_ptr<Params> b_betting_params = CreateBettingAbstractionParams();
  b_betting_params->ReadFromFile(argv[5]);
  unique_ptr<BettingAbstraction>
    b_betting_abstraction(new BettingAbstraction(*b_betting_params));
  unique_ptr<Params> a_cfr_params = CreateCFRParams();
  a_cfr_params->ReadFromFile(argv[6]);
  unique_ptr<CFRConfig>
    a_cfr_config(new CFRConfig(*a_cfr_params));
  unique_ptr<Params> b_cfr_params = CreateCFRParams();
  b_cfr_params->ReadFromFile(argv[7]);
  unique_ptr<CFRConfig>
    b_cfr_config(new CFRConfig(*b_cfr_params));

  int a_it, b_it, num_sampled_max_street_boards;
  if (sscanf(argv[8], "%i", &a_it) != 1)                           Usage(argv[0]);
  if (sscanf(argv[9], "%i", &b_it) != 1)                           Usage(argv[0]);
  if (sscanf(argv[10], "%i", &num_sampled_max_street_boards) != 1) Usage(argv[0]);

  bool a_quantize = false, b_quantize = false;
  string qa = argv[11];
  if (qa == "quantize") a_quantize = true;
  else if (qa == "raw") a_quantize = false;
  else                  Usage(argv[0]);
  string qb = argv[12];
  if (qb == "quantize") b_quantize = true;
  else if (qb == "raw") b_quantize = false;
  else                  Usage(argv[0]);

  bool deterministic = false;
  string da = argv[13];
  if (da == "deterministic")         deterministic = true;
  else if (da == "nondeterministic") deterministic = false;
  else                               Usage(argv[0]);
  
  bool resolve_a = false;
  bool resolve_b = false;
  string ra = argv[14];
  if (ra == "true")       resolve_a = true;
  else if (ra == "false") resolve_a = false;
  else                    Usage(argv[0]);
  string rb = argv[15];
  if (rb == "true")       resolve_b = true;
  else if (rb == "false") resolve_b = false;
  else                    Usage(argv[0]);

  if (resolve_a && resolve_b && argc != 23)     Usage(argv[0]);
  if (resolve_a && ! resolve_b && argc != 20)   Usage(argv[0]);
  if (! resolve_a && resolve_b && argc != 20)   Usage(argv[0]);
  if (! resolve_a && ! resolve_b && argc != 16) Usage(argv[0]);

  int resolve_st = -1;
  if (resolve_a || resolve_b) {
    if (sscanf(argv[16], "%i", &resolve_st) != 1) Usage(argv[0]);
  }
  
  unique_ptr<CardAbstraction> a_subgame_card_abstraction, b_subgame_card_abstraction;
  unique_ptr<BettingAbstraction> a_subgame_betting_abstraction, b_subgame_betting_abstraction;
  unique_ptr<CFRConfig> a_subgame_cfr_config, b_subgame_cfr_config;
  if (resolve_a) {
    unique_ptr<Params> subgame_card_params = CreateCardAbstractionParams();
    subgame_card_params->ReadFromFile(argv[17]);
    a_subgame_card_abstraction.reset(new CardAbstraction(*subgame_card_params));
    unique_ptr<Params> subgame_betting_params = CreateBettingAbstractionParams();
    subgame_betting_params->ReadFromFile(argv[18]);
    a_subgame_betting_abstraction.reset(new BettingAbstraction(*subgame_betting_params));
    unique_ptr<Params> subgame_cfr_params = CreateCFRParams();
    subgame_cfr_params->ReadFromFile(argv[19]);
    a_subgame_cfr_config.reset(new CFRConfig(*subgame_cfr_params));
  }
  if (resolve_b) {
    int a = resolve_a ? 20 : 17;
    unique_ptr<Params> subgame_card_params = CreateCardAbstractionParams();
    subgame_card_params->ReadFromFile(argv[a]);
    b_subgame_card_abstraction.reset(new CardAbstraction(*subgame_card_params));
    unique_ptr<Params> subgame_betting_params = CreateBettingAbstractionParams();
    subgame_betting_params->ReadFromFile(argv[a+1]);
    b_subgame_betting_abstraction.reset(new BettingAbstraction(*subgame_betting_params));
    unique_ptr<Params> subgame_cfr_params = CreateCFRParams();
    subgame_cfr_params->ReadFromFile(argv[a+2]);
    b_subgame_cfr_config.reset(new CFRConfig(*subgame_cfr_params));
  }

  HandValueTree::Create();
  
  Player player(*a_betting_abstraction, *b_betting_abstraction, *a_card_abstraction,
		*b_card_abstraction, *a_cfr_config, *b_cfr_config, a_it, b_it, resolve_st,
		resolve_a, resolve_b, *a_subgame_card_abstraction, *a_subgame_betting_abstraction,
		*a_subgame_cfr_config, *b_subgame_card_abstraction, *b_subgame_betting_abstraction,
		*b_subgame_cfr_config, a_quantize, b_quantize);
  player.Go(num_sampled_max_street_boards, deterministic);
}
