#include <stdio.h>
#include <stdlib.h>

#include <memory>
#include <string>

#include "betting_trees.h"
#include "board_tree.h"
#include "buckets.h"
#include "canonical_cards.h"
#include "card_abstraction.h"
#include "cards.h"
#include "cfr_utils.h"
#include "cfr_values.h"
#include "dynamic_cbr.h"
#include "game.h"
#include "hand_tree.h"
#include "reach_probs.h"
#include "subgame_utils.h"
#include "vcfr_state.h"

using std::shared_ptr;
using std::string;

DynamicCBR::DynamicCBR(const CardAbstraction &ca, const CFRConfig &cc, const Buckets &buckets,
		       int num_threads) :
  VCFR(ca, cc, buckets, num_threads) {
  value_calculation_ = true;
  int max_street = Game::MaxStreet();
  for (int st = 0; st <= max_street; ++st) {
    best_response_streets_[st] = true;
  }
}

DynamicCBR::~DynamicCBR(void) {
}

// Note that we may be working with sumprobs that are specific to a subgame.
// If so, they will be for the subgame rooted at root_bd_st and root_bd.
// So we must map our global board index gbd into a local board index lbd
// whenever we access hand_tree or the probabilities inside sumprobs.
shared_ptr<double []> DynamicCBR::Compute(Node *node, int p, const shared_ptr<double []> &opp_probs,
					  int gbd, const HandTree *hand_tree) {
  int st = node->Street();
  // time_t start_t = time(NULL);
  int num_hole_card_pairs = Game::NumHoleCardPairs(st);
  const CanonicalCards *hands = hand_tree->Hands(st, gbd);
  // Should set this appropriately
  string action_sequence = "x";
  shared_ptr<double []> vals = ProcessSubgame(node, node, gbd, p, opp_probs, hand_tree,
					      action_sequence);
  // Temporary?  Make our T values like T values constructed by build_cbrs,
  // by casting to float.
  for (int i = 0; i < num_hole_card_pairs; ++i) {
    vals[i] = (float)vals[i];
  }
#if 0
  // EVs for our hands are summed over all opponent hole card pairs.  To
  // compute properly normalized EV, need to divide by that number.
  int num_hole_cards = Game::NumCardsForStreet(0);
  int num_cards_in_deck = Game::NumCardsInDeck();
  int num_remaining = num_cards_in_deck - num_hole_cards;
  int num_opp_hole_card_pairs;
  if (num_hole_cards == 1) {
    num_opp_hole_card_pairs = num_remaining;
  } else {
    num_opp_hole_card_pairs = num_remaining * (num_remaining - 1) / 2;
  }
  double sum = 0;
  for (int i = 0; i < num_hole_card_pairs; ++i) {
    sum += vals[i] / num_opp_hole_card_pairs;
  }
  double ev = sum / num_hole_card_pairs;
#endif

  FloorCVs(node, opp_probs.get(), hands, vals.get());
  return vals;
}

// target_p is the player who you want CBR values for.
// Things get confusing in subgame solving.  Suppose we are doing subgame
// solving for P0.  We might say cfr_target_p is 0.  Then I want T-values for
// P1.  So I pass in 1 to Compute(). We'll need the reach probs of P1's
// opponent, who is P0.
shared_ptr<double []> DynamicCBR::Compute(Node *node, const ReachProbs &reach_probs, int gbd,
					  const HandTree *hand_tree, int target_p, bool cfrs,
					  bool zero_sum, bool current, bool purify_opp) {
  cfrs_ = cfrs;
  br_current_ = current;
  if (purify_opp) {
    if (current) {
      prob_method_ = ProbMethod::FTL;
    } else {
      prob_method_ = ProbMethod::PURE;
    }
  } else {
    prob_method_ = ProbMethod::REGRET_MATCHING;
  }
  if (zero_sum) {
    shared_ptr<double []> p0_cvs = Compute(node, 0, reach_probs.Get(1), gbd, hand_tree);
    shared_ptr<double []> p1_cvs = Compute(node, 1, reach_probs.Get(0), gbd, hand_tree);
    int st = node->Street();
    int num_hole_card_pairs = Game::NumHoleCardPairs(st);
    const CanonicalCards *hands = hand_tree->Hands(st, gbd);
    ZeroSumCVs(p0_cvs.get(), p1_cvs.get(), num_hole_card_pairs, reach_probs, hands);
    if (target_p == 1) return p1_cvs;
    else               return p0_cvs;
  } else {
    return Compute(node, target_p, reach_probs.Get(target_p^1), gbd, hand_tree);
  }
}

