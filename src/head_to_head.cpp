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
#include <unistd.h>   // sleep()

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
  Player(const CardAbstraction &a_ca, const CardAbstraction &b_ca,
	 const BettingAbstraction &a_ba, const BettingAbstraction &b_ba,
	 const CFRConfig &a_cc, const CFRConfig &b_cc, int a_it, int b_it, int resolve_st,
	 bool resolve_a, bool resolve_b, const CardAbstraction &as_ca,
	 const BettingAbstraction &as_ba, const CFRConfig &as_cc,
	 const CardAbstraction &bs_ca, const BettingAbstraction &bs_ba,
	 const CFRConfig &bs_cc, bool a_quantize, bool b_quantize);
  ~Player(void) {}
  void Go(int num_sampled_max_street_boards, bool deterministic, int num_threads);
  const CardAbstraction &ACardAbstraction(void) const {return a_card_abstraction_;}
  const CardAbstraction &BCardAbstraction(void) const {return b_card_abstraction_;}
  const BettingAbstraction &ABettingAbstraction(void) const {return a_betting_abstraction_;}
  const BettingAbstraction &BBettingAbstraction(void) const {return b_betting_abstraction_;}
  const CFRConfig &ACFRConfig(void) const {return a_cfr_config_;}
  const CFRConfig &BCFRConfig(void) const {return b_cfr_config_;}
  shared_ptr<Buckets> ASubgameBuckets(void) const {return a_subgame_buckets_;}
  shared_ptr<Buckets> BSubgameBuckets(void) const {return b_subgame_buckets_;}
  const CardAbstraction &ASubgameCardAbstraction(void) const {return a_subgame_card_abstraction_;}
  const CardAbstraction &BSubgameCardAbstraction(void) const {return b_subgame_card_abstraction_;}
  const BettingAbstraction &ASubgameBettingAbstraction(void) const {
    return a_subgame_betting_abstraction_;
  }
  const BettingAbstraction &BSubgameBettingAbstraction(void) const {
    return b_subgame_betting_abstraction_;
  }
  const CFRConfig &ASubgameCFRConfig(void) const {return a_subgame_cfr_config_;}
  const CFRConfig &BSubgameCFRConfig(void) const {return b_subgame_cfr_config_;}
  shared_ptr<Buckets> ABaseBuckets(void) const {return a_base_buckets_;}
  shared_ptr<Buckets> BBaseBuckets(void) const {return b_base_buckets_;}
  const BettingTrees &ABettingTrees(void) const {return *a_betting_trees_;}
  const BettingTrees &BBettingTrees(void) const {return *b_betting_trees_;}
  shared_ptr<CFRValues> AProbs(void) const {return a_probs_;}
  shared_ptr<CFRValues> BProbs(void) const {return b_probs_;}
  int ResolveSt(void) const {return resolve_st_;}
  bool ResolveA(void) const {return resolve_a_;}
  bool ResolveB(void) const {return resolve_b_;}
private:
  void Compute(int num_sampled_max_street_boards, bool deterministic, int num_threads, double *sum,
	       double *sum_sqd);
  void Report(double sum, double sum_sqd, int num_sampled_max_street_boards);

  const CardAbstraction &a_card_abstraction_;
  const CardAbstraction &b_card_abstraction_;
  const BettingAbstraction &a_betting_abstraction_;
  const BettingAbstraction &b_betting_abstraction_;
  const CFRConfig &a_cfr_config_;
  const CFRConfig &b_cfr_config_;
  const CardAbstraction &a_subgame_card_abstraction_;
  const BettingAbstraction &a_subgame_betting_abstraction_;
  const CFRConfig &a_subgame_cfr_config_;
  const CardAbstraction &b_subgame_card_abstraction_;
  const BettingAbstraction &b_subgame_betting_abstraction_;
  const CFRConfig &b_subgame_cfr_config_;
  shared_ptr<Buckets> a_base_buckets_;
  shared_ptr<Buckets> b_base_buckets_;
  unique_ptr<BettingTrees> a_betting_trees_;
  unique_ptr<BettingTrees> b_betting_trees_;
  shared_ptr<CFRValues> a_probs_;
  shared_ptr<CFRValues> b_probs_;
  int resolve_st_;
  bool resolve_a_;
  bool resolve_b_;
  shared_ptr<Buckets> a_subgame_buckets_;
  shared_ptr<Buckets> b_subgame_buckets_;
  int num_board_samples_;
  unique_ptr<int []> board_samples_;
};

Player::Player(const CardAbstraction &a_ca, const CardAbstraction &b_ca,
	       const BettingAbstraction &a_ba, const BettingAbstraction &b_ba,
	       const CFRConfig &a_cc, const CFRConfig &b_cc, int a_it, int b_it, int resolve_st,
	       bool resolve_a, bool resolve_b, const CardAbstraction &as_ca,
	       const BettingAbstraction &as_ba, const CFRConfig &as_cc,
	       const CardAbstraction &bs_ca, const BettingAbstraction &bs_ba,
	       const CFRConfig &bs_cc, bool a_quantize, bool b_quantize) :
  a_card_abstraction_(a_ca), b_card_abstraction_(b_ca),
  a_betting_abstraction_(a_ba), b_betting_abstraction_(b_ba),
  a_cfr_config_(a_cc), b_cfr_config_(b_cc),
  a_subgame_card_abstraction_(as_ca), a_subgame_betting_abstraction_(as_ba),
  a_subgame_cfr_config_(as_cc), b_subgame_card_abstraction_(bs_ca),
  b_subgame_betting_abstraction_(bs_ba), b_subgame_cfr_config_(bs_cc) {
  int max_street = Game::MaxStreet();
  resolve_a_ = resolve_a;
  resolve_b_ = resolve_b;
  resolve_st_ = resolve_st;

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
  }
  if (resolve_b_) {
    b_subgame_buckets_.reset(new Buckets(bs_ca, false));
  }

  // Want to be able to randomly sample a board from board_samples_.
  // Sampling must be proportional to the frequency of the board.
  int num_max_street_boards = BoardTree::NumBoards(max_street);
  // Should be able to compute this more directly.
  // For full holdem, it's (52 * 51 * 50 / 6) * 49 * 48.
  num_board_samples_ = 0;
  for (int bd = 0; bd < num_max_street_boards; ++bd) {
    num_board_samples_ += BoardTree::BoardCount(max_street, bd);
  }
  fprintf(stderr, "num_board_samples_ %i\n", num_board_samples_);
  board_samples_.reset(new int[num_board_samples_]);
  int i = 0;
  for (int bd = 0; bd < num_max_street_boards; ++bd) {
    int bd_num_samples = BoardTree::BoardCount(max_street, bd);
    for (int j = 0; j < bd_num_samples; ++j) {
      board_samples_[i++] = bd;
    }
  }
}

class PlayerThread {
public:
  PlayerThread(const Player &player, int thread_index, int num_threads,
	       const vector<int> &sampled_boards);
  void Go(void);
  void Run(void);
  void Join(void);
  double Sum(void) const {return sum_;}
  double SumSqd(void) const {return sum_sqd_;}
  int NumResolves(void) const {return num_resolves_;}
  double ResolvingSecs(void) const {return resolving_secs_;}
private:
  void Showdown(Node *a_node, Node *b_node, const ReachProbs &reach_probs);
  void Fold(Node *a_node, Node *b_node, const ReachProbs &reach_probs);
  void Nonterminal(Node *a_node, Node *b_node, const string &action_sequence,
		   const ReachProbs &reach_probs);
  void Walk(Node *a_node, Node *b_node, const string &action_sequence,
	    const ReachProbs &reach_probs, int last_st);
  double ProcessMaxStreetBoard(int msbd);

  const Player &player_;
  int thread_index_;
  int num_threads_;
  unique_ptr<shared_ptr<CanonicalCards> []> street_hands_;
  unique_ptr<EGCFR> a_eg_cfr_;
  unique_ptr<EGCFR> b_eg_cfr_;
  // When we resolve a street, the board index may change.  This is why we have separate
  // a boards and b boards.  Only one player may be resolving.
  unique_ptr<int []> a_gbds_;
  unique_ptr<int []> a_lbds_;
  unique_ptr<int []> b_gbds_;
  unique_ptr<int []> b_lbds_;
  int msbd_;
  unique_ptr<BettingTrees> a_subtrees_;
  unique_ptr<BettingTrees> b_subtrees_;
  shared_ptr<HandTree> resolve_hand_tree_;
  int num_subgame_its_;
  int b_pos_;
  double sum_b_outcomes_;
  double sum_weights_;
  int num_resolves_;
  double resolving_secs_;
  double sum_;
  double sum_sqd_;
  const vector<int> &sampled_boards_;
  pthread_t pthread_id_;
};

PlayerThread::PlayerThread(const Player &player, int thread_index, int num_threads,
			   const vector<int> &sampled_boards) :
  player_(player), thread_index_(thread_index), num_threads_(num_threads),
  sampled_boards_(sampled_boards) {
  int max_street = Game::MaxStreet();
  street_hands_.reset(new shared_ptr<CanonicalCards>[max_street + 1]);
  b_pos_ = 0;
  sum_b_outcomes_ = 0;
  sum_weights_ = 0;
  num_resolves_ = 0;
  resolving_secs_ = 0;
  // Check for dups for buckets
  if (player_.ResolveA()) {
    const CardAbstraction &a_ca = player_.ACardAbstraction();
    const BettingAbstraction &a_ba = player_.ABettingAbstraction();
    const CFRConfig &a_cc = player_.ACFRConfig();
    const CardAbstraction &as_ca = player_.ASubgameCardAbstraction();
    const CFRConfig &as_cc = player_.ASubgameCFRConfig();
    shared_ptr<Buckets> a_subgame_buckets = player_.ASubgameBuckets();
    a_eg_cfr_.reset(new UnsafeEGCFR(as_ca, a_ca, a_ba, as_cc, a_cc, *a_subgame_buckets, 1));
  }
  if (player_.ResolveB()) {
    const CardAbstraction &b_ca = player_.BCardAbstraction();
    const BettingAbstraction &b_ba = player_.BBettingAbstraction();
    const CFRConfig &b_cc = player_.BCFRConfig();
    const CardAbstraction &bs_ca = player_.BSubgameCardAbstraction();
    const CFRConfig &bs_cc = player_.BSubgameCFRConfig();
    shared_ptr<Buckets> b_subgame_buckets = player_.BSubgameBuckets();
    b_eg_cfr_.reset(new UnsafeEGCFR(bs_ca, b_ca, b_ba, bs_cc, b_cc, *b_subgame_buckets, 1));
  }
  a_gbds_.reset(new int[max_street + 1]);
  a_lbds_.reset(new int[max_street + 1]);
  b_gbds_.reset(new int[max_street + 1]);
  b_lbds_.reset(new int[max_street + 1]);
  a_gbds_[0] = 0;
  a_lbds_[0] = 0;
  b_gbds_[0] = 0;
  b_lbds_[0] = 0;
  // Should expose as configurable parameter
  num_subgame_its_ = 200;
}


// Compute outcome from B's perspective
void PlayerThread::Showdown(Node *a_node, Node *b_node, const ReachProbs &reach_probs) {
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

  sum_b_outcomes_ += sum_our_vals;
  sum_weights_ += sum_joint_probs;
}
  
// Compute outcome from B's perspective
void PlayerThread::Fold(Node *a_node, Node *b_node, const ReachProbs &reach_probs) {
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

  sum_b_outcomes_ += sum_our_vals;
  sum_weights_ += sum_joint_probs;
}

void PlayerThread::Nonterminal(Node *a_node, Node *b_node, const string &action_sequence,
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
  if (pa == b_pos_) {
    // This doesn't support multiplayer yet
    const CFRValues *sumprobs;
    if (player_.ResolveB() && st >= player_.ResolveSt()) {
      sumprobs = b_eg_cfr_->Sumprobs().get();
    } else {
      sumprobs = player_.BProbs().get();
    }
    const Buckets &buckets =
      (player_.ResolveB() && st >= player_.ResolveSt()) ? *player_.BSubgameBuckets() : *player_.BBaseBuckets();
    const CanonicalCards *hands = street_hands_[st].get();
    succ_reach_probs = ReachProbs::CreateSuccReachProbs(b_node, b_gbds_[st], b_lbds_[st], hands,
							buckets, sumprobs, reach_probs, false);
  } else {
    const CFRValues *sumprobs;
    if (player_.ResolveA() && st >= player_.ResolveSt()) {
      sumprobs = a_eg_cfr_->Sumprobs().get();
    } else {
      sumprobs = player_.AProbs().get();
    }
    const Buckets &buckets =
      (player_.ResolveA() && st >= player_.ResolveSt()) ? *player_.ASubgameBuckets() : *player_.ABaseBuckets();
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
}
 
void PlayerThread::Walk(Node *a_node, Node *b_node, const string &action_sequence,
			const ReachProbs &reach_probs, int last_st) {
  int st = a_node->Street();
  if (st > last_st && st == player_.ResolveSt()) {
    const BettingAbstraction &a_ba = player_.ABettingAbstraction();
    Node *next_a_node, *next_b_node;
    if (player_.ResolveA() && a_node->LastBetTo() < a_ba.StackSize()) {
      const BettingAbstraction &as_ba = player_.ASubgameBettingAbstraction();
      a_subtrees_.reset(CreateSubtrees(st, a_node->PlayerActing(), a_node->LastBetTo(), -1,
				       as_ba));
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
    const BettingAbstraction &b_ba = player_.BBettingAbstraction();
    if (player_.ResolveB() && b_node->LastBetTo() < b_ba.StackSize()) {
      const BettingAbstraction &bs_ba = player_.BSubgameBettingAbstraction();
      b_subtrees_.reset(CreateSubtrees(st, b_node->PlayerActing(), b_node->LastBetTo(), -1,
				       bs_ba));
      int max_street = Game::MaxStreet();
      int root_bd;
      if (st == max_street) root_bd = msbd_;
      else                  root_bd = BoardTree::PredBoard(msbd_, st);
      fprintf(stderr, "Resolving %s b_pos_ %i id %i lbt %i\n", action_sequence.c_str(), b_pos_,
	      b_node->NonterminalID(), b_node->LastBetTo());
      struct timespec start, finish;
      clock_gettime(CLOCK_MONOTONIC, &start);
      b_eg_cfr_->SolveSubgame(b_subtrees_.get(), root_bd, reach_probs, action_sequence,
			      resolve_hand_tree_.get(), nullptr, -1, true, num_subgame_its_);
      clock_gettime(CLOCK_MONOTONIC, &finish);
      double secs = (finish.tv_sec - start.tv_sec) + (finish.tv_nsec - start.tv_nsec) / 1000000000.0;
      fprintf(stderr, "Resolved in %f secs\n", secs);
      resolving_secs_ += secs;
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
    if (player_.ResolveA()) {
      a_eg_cfr_->ClearSumprobs();
    }
    if (player_.ResolveB()) {
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

double PlayerThread::ProcessMaxStreetBoard(int msbd) {
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
  sum_b_outcomes_ = 0;
  sum_weights_ = 0;

  if (player_.ResolveA() || player_.ResolveB()) {
    int resolve_st = player_.ResolveSt();
    if (resolve_st < max_street) {
      resolve_hand_tree_.reset(new HandTree(resolve_st, BoardTree::PredBoard(msbd_, resolve_st),
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
  const BettingTrees &a_betting_trees = player_.ABettingTrees();
  const BettingTrees &b_betting_trees = player_.BBettingTrees();
  for (b_pos_ = 0; b_pos_ < num_players; ++b_pos_) {
    b_root = b_betting_trees.Root(b_pos_);
    a_root = a_betting_trees.Root(b_pos_^1);
    Walk(a_root, b_root, "x", *reach_probs, 0);
  }
  return sum_b_outcomes_ / sum_weights_;
}

void PlayerThread::Go(void) {
  // Iterate through the sampled boards processing those that "belong" to this thread.
  sum_ = 0;
  sum_sqd_ = 0;
  int num_samples = sampled_boards_.size();
  for (int i = thread_index_; i < num_samples; i += num_threads_) {
    int bd = sampled_boards_[i];
    double avg_b_outcome = ProcessMaxStreetBoard(bd);
    sum_ += avg_b_outcome;
    sum_sqd_ += avg_b_outcome * avg_b_outcome;
    fprintf(stderr, "Thread %i sample %i outcome %f mbb %f\n", thread_index_, i, avg_b_outcome,
	    (avg_b_outcome / 2.0) * 1000.0);
  }
}

static void *player_thread_run(void *v_t) {
  PlayerThread *t = (PlayerThread *)v_t;
  t->Go();
  return NULL;
}

void PlayerThread::Run(void) {
  pthread_create(&pthread_id_, NULL, player_thread_run, this);
}

void PlayerThread::Join(void) {
  pthread_join(pthread_id_, NULL); 
}

void Player::Compute(int num_sampled_max_street_boards, bool deterministic, int num_threads,
		     double *sum, double *sum_sqd) {
  struct drand48_data rand_buf;
  if (deterministic) {
    srand48_r(0, &rand_buf);
  } else {
    struct timeval time; 
    gettimeofday(&time, NULL);
    srand48_r((time.tv_sec * 1000) + (time.tv_usec / 1000), &rand_buf);
  }
  vector<int> sampled_boards;
  double r;
  for (int i = 0; i < num_sampled_max_street_boards; ++i) {
    drand48_r(&rand_buf, &r);
    // Choose a board by uniformly sampling from board_samples_.
    int s = r * num_board_samples_;
    int bd = board_samples_[s];
    sampled_boards.push_back(bd);
  }
  unique_ptr<PlayerThread * []> threads(new PlayerThread *[num_threads]);
  for (int i = 0; i < num_threads; ++i) {
    threads[i] = new PlayerThread(*this, i, num_threads, sampled_boards);
  }
  for (int i = 1; i < num_threads; ++i) {
    threads[i]->Run();
  }
  threads[0]->Go();
  for (int i = 1; i < num_threads; ++i) {
    threads[i]->Join();
  }
  *sum = 0;
  *sum_sqd = 0;
  int num_resolves = 0;
  double resolving_secs = 0;
  for (int i = 0; i < num_threads; ++i) {
    *sum += threads[i]->Sum();
    *sum_sqd += threads[i]->SumSqd();
    num_resolves += threads[i]->NumResolves();
    resolving_secs += threads[i]->ResolvingSecs();
  }
  if (num_resolves > 0) {
    fprintf(stderr, "Avg resolve time: %f (%f/%i)\n", resolving_secs / num_resolves,
	    resolving_secs, num_resolves);
  }
  for (int i = 0; i < num_threads; ++i) {
    delete threads[i];
  }
}

void Player::Report(double sum, double sum_sqd, int num_sampled_max_street_boards) {
  double mean = sum / num_sampled_max_street_boards;
  double mean_mbb = (mean / 2.0) * 1000.0;
  fprintf(stderr, "Mean B outcome: %f\n", mean);
  fprintf(stderr, "MBB mean: %f\n", mean_mbb);
  if (num_sampled_max_street_boards > 1) {
    // Variance is the mean of the squares minus the square of the means
    double var = sum_sqd / ((double)num_sampled_max_street_boards) - mean * mean;
    double stddev = sqrt(var);
    double sum_stddev = stddev * sqrt(num_sampled_max_street_boards);
    double sum_lower = sum - 1.96 * sum_stddev;
    double sum_upper = sum + 1.96 * sum_stddev;
    double mbb_lower = ((sum_lower / (num_sampled_max_street_boards)) / 2.0) * 1000.0;
    double mbb_upper = ((sum_upper / (num_sampled_max_street_boards)) / 2.0) * 1000.0;
    fprintf(stderr, "MBB confidence interval: %f-%f\n", mbb_lower, mbb_upper);
    double single_lower = mean - 1.96 * stddev;
    double single_upper = mean + 1.96 * stddev;
    double single_lower_mbb = (single_lower / 2.0) * 1000.0;
    double single_upper_mbb = (single_upper / 2.0) * 1000.0;
    fprintf(stderr, "MBB single confidence interval: %f-%f\n", single_lower_mbb, single_upper_mbb);
  }
}

void Player::Go(int num_sampled_max_street_boards, bool deterministic, int num_threads) {
  double sum, sum_sqd;
  Compute(num_sampled_max_street_boards, deterministic, num_threads, &sum, &sum_sqd);
  Report(sum, sum_sqd, num_sampled_max_street_boards);
}

static void Usage(const char *prog_name) {
  fprintf(stderr, "USAGE: %s <game params> <A card params> <B card params> "
	  "<A betting abstraction params> <B betting abstraction params> <A CFR params> "
	  "<B CFR params> <A it> <B it> <num sampled max street boards> <num threads> "
	  "[quantize|raw] [quantize|raw] [deterministic|nondeterministic] <resolve A> <resolve B> "
	  "(<resolve st>) (<A resolve card params> <A resolve betting params> "
	  "<A resolve CFR config>) (<B resolve card params> <B resolve betting params> "
	  "<B resolve CFR config>)\n", prog_name);
  fprintf(stderr, "\n");
  fprintf(stderr, "Specify 0 for <num sampled max street boards> to not sample\n");
  fprintf(stderr, "<resolve A> and <resolve B> must be \"true\" or \"false\"\n");
  exit(-1);
}

int main(int argc, char *argv[]) {
  if (argc != 17 && argc != 21 && argc != 24) Usage(argv[0]);
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

  int a_it, b_it, num_sampled_max_street_boards, num_threads;
  if (sscanf(argv[8], "%i", &a_it) != 1)                           Usage(argv[0]);
  if (sscanf(argv[9], "%i", &b_it) != 1)                           Usage(argv[0]);
  if (sscanf(argv[10], "%i", &num_sampled_max_street_boards) != 1) Usage(argv[0]);
  if (sscanf(argv[11], "%i", &num_threads) != 1)                   Usage(argv[0]);

  bool a_quantize = false, b_quantize = false;
  string qa = argv[12];
  if (qa == "quantize") a_quantize = true;
  else if (qa == "raw") a_quantize = false;
  else                  Usage(argv[0]);
  string qb = argv[13];
  if (qb == "quantize") b_quantize = true;
  else if (qb == "raw") b_quantize = false;
  else                  Usage(argv[0]);

  bool deterministic = false;
  string da = argv[14];
  if (da == "deterministic")         deterministic = true;
  else if (da == "nondeterministic") deterministic = false;
  else                               Usage(argv[0]);
  
  bool resolve_a = false;
  bool resolve_b = false;
  string ra = argv[15];
  if (ra == "true")       resolve_a = true;
  else if (ra == "false") resolve_a = false;
  else                    Usage(argv[0]);
  string rb = argv[16];
  if (rb == "true")       resolve_b = true;
  else if (rb == "false") resolve_b = false;
  else                    Usage(argv[0]);

  if (resolve_a && resolve_b && argc != 24)     Usage(argv[0]);
  if (resolve_a && ! resolve_b && argc != 21)   Usage(argv[0]);
  if (! resolve_a && resolve_b && argc != 21)   Usage(argv[0]);
  if (! resolve_a && ! resolve_b && argc != 17) Usage(argv[0]);

  int resolve_st = -1;
  if (resolve_a || resolve_b) {
    if (sscanf(argv[17], "%i", &resolve_st) != 1) Usage(argv[0]);
  }
  
  unique_ptr<CardAbstraction> a_subgame_card_abstraction, b_subgame_card_abstraction;
  unique_ptr<BettingAbstraction> a_subgame_betting_abstraction, b_subgame_betting_abstraction;
  unique_ptr<CFRConfig> a_subgame_cfr_config, b_subgame_cfr_config;
  if (resolve_a) {
    unique_ptr<Params> subgame_card_params = CreateCardAbstractionParams();
    subgame_card_params->ReadFromFile(argv[18]);
    a_subgame_card_abstraction.reset(new CardAbstraction(*subgame_card_params));
    unique_ptr<Params> subgame_betting_params = CreateBettingAbstractionParams();
    subgame_betting_params->ReadFromFile(argv[19]);
    a_subgame_betting_abstraction.reset(new BettingAbstraction(*subgame_betting_params));
    unique_ptr<Params> subgame_cfr_params = CreateCFRParams();
    subgame_cfr_params->ReadFromFile(argv[20]);
    a_subgame_cfr_config.reset(new CFRConfig(*subgame_cfr_params));
  }
  if (resolve_b) {
    int a = resolve_a ? 21 : 18;
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
  
  Player player(*a_card_abstraction, *b_card_abstraction, *a_betting_abstraction,
		*b_betting_abstraction, *a_cfr_config, *b_cfr_config, a_it, b_it, resolve_st,
		resolve_a, resolve_b, *a_subgame_card_abstraction, *a_subgame_betting_abstraction,
		*a_subgame_cfr_config, *b_subgame_card_abstraction, *b_subgame_betting_abstraction,
		*b_subgame_cfr_config, a_quantize, b_quantize);
  player.Go(num_sampled_max_street_boards, deterministic, num_threads);
}
