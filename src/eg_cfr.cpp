#include <stdio.h>
#include <stdlib.h>

#include <memory>
#include <string>

#include "betting_trees.h"
#include "board_tree.h"
#include "eg_cfr.h"
#include "hand_value_tree.h"
#include "resolving_method.h"
#include "vcfr_state.h"
#include "vcfr.h"

using std::shared_ptr;
using std::string;

// Can we skip this if no opp hands reach?
// We assume a hand tree was created for this subgame.  (Note that we get the board, gbd, from
// the hand tree's root board.)  Is that safe?
shared_ptr<double []> EGCFR::HalfIteration(BettingTrees *subtrees, int p,
					   shared_ptr<double []> opp_probs,
					   const HandTree *hand_tree,
					   const string &action_sequence) {
  Node *subtree_root = subtrees->Root();
  int gbd = hand_tree->RootBd();
  return ProcessSubgame(subtree_root, subtree_root, gbd, p, opp_probs, hand_tree, action_sequence);
}

EGCFR::EGCFR(const CardAbstraction &ca, const CardAbstraction &base_ca,
	     const BettingAbstraction &base_ba, const CFRConfig &cc, const CFRConfig &base_cc,
	     const Buckets &buckets, ResolvingMethod method, bool cfrs, bool zero_sum,
	     int num_threads) :
  VCFR(ca, cc, buckets, num_threads),
  base_card_abstraction_(base_ca), base_betting_abstraction_(base_ba),
  base_cfr_config_(base_cc) {

  HandValueTree::Create();
  BoardTree::Create();
  it_ = 0;
}
