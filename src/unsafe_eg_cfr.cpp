#include <stdio.h>
#include <stdlib.h>

#include <memory>
#include <string>

#include "betting_trees.h"
#include "cfr_values.h"
#include "game.h"
#include "hand_tree.h"
#include "reach_probs.h"
#include "unsafe_eg_cfr.h"
#include "vcfr_state.h"

using std::shared_ptr;
using std::string;
using std::unique_ptr;

UnsafeEGCFR::UnsafeEGCFR(const CardAbstraction &ca, const CardAbstraction &base_ca,
			 const BettingAbstraction &base_ba, const CFRConfig &cc,
			 const CFRConfig &base_cc, const Buckets &buckets, int num_threads) :
  EGCFR(ca, base_ca, base_ba, cc, base_cc, buckets, ResolvingMethod::UNSAFE, false, false,
	num_threads) {
}

void UnsafeEGCFR::SolveSubgame(BettingTrees *subtrees, int solve_bd, const ReachProbs &reach_probs,
			       const string &action_sequence, const HandTree *hand_tree,
			       double *opp_cvs, int target_p, bool both_players, int num_its) {
  int subtree_st = subtrees->Root()->Street();
  int num_players = Game::NumPlayers();
  int max_street = Game::MaxStreet();
  
  unique_ptr<bool []> subtree_streets(new bool[max_street + 1]);
  for (int st = 0; st <= max_street; ++st) {
    subtree_streets[st] = st >= subtree_st;
  }
  regrets_.reset(new CFRValues(nullptr, subtree_streets.get(), solve_bd, subtree_st, buckets_,
			       subtrees->GetBettingTree()));
  regrets_->AllocateAndClear(subtrees->GetBettingTree(), CFRValueType::CFR_DOUBLE, false, -1);

  // Should honor sumprobs_streets_

  sumprobs_.reset(new CFRValues(nullptr, subtree_streets.get(), solve_bd, subtree_st, buckets_,
				subtrees->GetBettingTree()));
  sumprobs_->AllocateAndClear(subtrees->GetBettingTree(), CFRValueType::CFR_DOUBLE, false, -1);

  for (it_ = 1; it_ <= num_its; ++it_) {
    // Go from high to low to mimic slumbot2017 code
    for (int p = (int)num_players - 1; p >= 0; --p) {
      HalfIteration(subtrees, p, reach_probs.Get(p^1), hand_tree, action_sequence);
    }
  }
}
