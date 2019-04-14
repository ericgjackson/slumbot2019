#include <stdio.h>
#include <stdlib.h>

#include <memory>

#include "betting_tree.h"
#include "board_tree.h"
#include "eg_cfr.h"
#include "hand_value_tree.h"
#include "resolving_method.h"
#include "vcfr_state.h"
#include "vcfr.h"

using std::shared_ptr;

// Can we skip this if no opp hands reach
shared_ptr<double []> EGCFR::HalfIteration(BettingTree *subtree, int solve_bd, 
					   const VCFRState &state) {
  Node *subtree_root = subtree->Root();
  return Process(subtree_root, 0, state, subtree_root->Street());
}

EGCFR::EGCFR(const CardAbstraction &ca, const CardAbstraction &base_ca,
	     const BettingAbstraction &ba, const BettingAbstraction &base_ba,
	     const CFRConfig &cc, const CFRConfig &base_cc, const Buckets &buckets, 
	     ResolvingMethod method, bool cfrs, bool zero_sum, int num_threads) :
  VCFR(ca, ba, cc, buckets, num_threads),
  base_card_abstraction_(base_ca), base_betting_abstraction_(base_ba),
  base_cfr_config_(base_cc) {
#if 0
  method_ = method;
  cfrs_ = cfrs;
  zero_sum_ = zero_sum;
#endif

  HandValueTree::Create();
  BoardTree::Create();
  it_ = 0;

#if 0
  hand_tree_ = nullptr;
  sumprobs_ = nullptr;
  regrets_ = nullptr;
#endif
}
