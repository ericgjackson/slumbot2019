#include <stdio.h>
#include <stdlib.h>

#include <memory>
#include <string>

#include "betting_tree.h"
#include "cfr_values.h"
#include "game.h"
#include "hand_tree.h"
#include "unsafe_eg_cfr.h"
#include "vcfr_state.h"

using std::string;
using std::unique_ptr;

UnsafeEGCFR::UnsafeEGCFR(const CardAbstraction &ca, const CardAbstraction &base_ca,
			 const BettingAbstraction &ba, const BettingAbstraction &base_ba,
			 const CFRConfig &cc, const CFRConfig &base_cc, const Buckets &buckets,
			 int num_threads) :
  EGCFR(ca, base_ca, ba, base_ba, cc, base_cc, buckets,
	ResolvingMethod::UNSAFE, false, false, num_threads) {
}

void UnsafeEGCFR::SolveSubgame(BettingTree *subtree, int solve_bd, double **reach_probs,
			       const string &action_sequence, const HandTree *hand_tree,
			       double *opp_cvs, int target_p, bool both_players, int num_its) {
  int subtree_st = subtree->Root()->Street();
  int num_players = Game::NumPlayers();
  int max_street = Game::MaxStreet();
  
  unique_ptr<bool []> subtree_streets(new bool[max_street + 1]);
  for (int st = 0; st <= max_street; ++st) {
    subtree_streets[st] = st >= subtree_st;
  }
  regrets_.reset(new CFRValues(nullptr, subtree_streets.get(), solve_bd,
			       subtree_st, buckets_, subtree));
  regrets_->AllocateAndClearDoubles(subtree->Root(), -1);

  // Should honor sumprobs_streets_

  sumprobs_.reset(new CFRValues(nullptr, subtree_streets.get(), solve_bd,
				subtree_st, buckets_, subtree));
  sumprobs_->AllocateAndClearDoubles(subtree->Root(), -1);

  VCFRState **initial_states = new VCFRState *[num_players];
  int **street_buckets = AllocateStreetBuckets();
  for (int p = 0; p < num_players; ++p) {
    initial_states[p] = new VCFRState(reach_probs[p^1], hand_tree, 0,
				      action_sequence, solve_bd, subtree_st,
				      street_buckets);
  }
  SetStreetBuckets(subtree_st, solve_bd, *initial_states[0]);
  for (it_ = 1; it_ <= num_its; ++it_) {
    // Go from high to low to mimic slumbot2017 code
    for (int p = (int)num_players - 1; p >= 0; --p) {
      double *vals = HalfIteration(subtree, solve_bd, p, *initial_states[p]);
      delete [] vals;
    }
  }

  for (int p = 0; p < num_players; ++p) {
    delete initial_states[p];
  }
  delete [] initial_states;
  DeleteStreetBuckets(street_buckets);
}

