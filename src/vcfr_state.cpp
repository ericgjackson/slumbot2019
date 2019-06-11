// A VCFRState contains elements of the current state during CFR+ processing.  For example,
// the probabilities of each opponent hand reaching the current state.  Some data is shared
// between different VCFRState objects.  For example, if we are at an "our choice" node then
// the current state and all the successor states will share the same opponent reach
// probabilities.  We use shared_ptrs to allow such data to be shared between states, and yet
// be cleaned up when no longer needed.

#include <stdio.h>
#include <stdlib.h>

#include <memory>
#include <string>

#include "betting_tree.h"
#include "cfr_utils.h" // CommonBetResponseCalcs()
#include "game.h"
#include "hand_tree.h"
#include "vcfr_state.h"

using std::shared_ptr;
using std::string;

static shared_ptr<int []> AllocateStreetBuckets(void) {
  int max_num_hole_card_pairs = Game::NumHoleCardPairs(0);
  int max_street = Game::MaxStreet();
  int num = (max_street + 1) * max_num_hole_card_pairs;
  shared_ptr<int []> street_buckets(new int[num]);
  return street_buckets;
}

static shared_ptr<double []> AllocateOppProbs(void) {
  int num_hole_cards = Game::NumCardsForStreet(0);
  int max_card1 = Game::MaxCard() + 1;
  int num_enc;
  if (num_hole_cards == 1) num_enc = max_card1;
  else                     num_enc = max_card1 * max_card1;
  shared_ptr<double []> opp_probs(new double[num_enc]);
  for (int i = 0; i < num_enc; ++i) opp_probs[i] = 1.0;
  return opp_probs;
}

void VCFRState::AllocateTotalCardProbs(void) {
  int max_card1 = Game::MaxCard() + 1;
  total_card_probs_.reset(new double[max_card1]);
}

// Called at the root of the tree.
VCFRState::VCFRState(int p, const HandTree *hand_tree) {
  p_ = p;
  opp_probs_ = AllocateOppProbs();
  street_buckets_ = AllocateStreetBuckets();
  action_sequence_ = "x";
  hand_tree_ = hand_tree;
  total_card_probs_ = nullptr;
  // Signifies opp data is uninitialized
  sum_opp_probs_ = -1;
#if 0
  const CanonicalCards *hands = hand_tree_->Hands(0, 0);
  // We need to initialize total_card_probs_ and sum_opp_probs_ because an open fold is allowed.
  CommonBetResponseCalcs(0, hands, opp_probs_.get(), &sum_opp_probs_, total_card_probs_.get());
#endif
}
  
// Called at an internal street-initial node.  We do not initialize total_card_probs_ (and set
// sum_opp_probs_ to zero) because we know we will come across an opp-choice node before we need
// those members.
VCFRState::VCFRState(int p, const shared_ptr<double []> &opp_probs, const HandTree *hand_tree,
		     const string &action_sequence) {
  p_ = p;
  opp_probs_ = opp_probs;
  total_card_probs_ = nullptr;
  // Signifies opp data is uninitialized
  sum_opp_probs_ = -1;
  hand_tree_ = hand_tree;
  action_sequence_ = action_sequence;
  street_buckets_ = AllocateStreetBuckets();
}

// Create a new VCFRState corresponding to taking an action of ours.
VCFRState::VCFRState(const VCFRState &pred, Node *node, int s) {
  p_ = pred.P();
  opp_probs_ = pred.OppProbs();
  hand_tree_ = pred.GetHandTree();
  action_sequence_ = pred.ActionSequence() + node->ActionName(s);
  street_buckets_ = pred.AllStreetBuckets();
  total_card_probs_ = pred.TotalCardProbs();
  sum_opp_probs_ = pred.SumOppProbs();
}

// Create a new VCFRState corresponding to taking an opponent action.
VCFRState::VCFRState(const VCFRState &pred, Node *node, int s,
		     const shared_ptr<double []> &opp_probs) {
  p_ = pred.P();
  opp_probs_ = opp_probs;
  hand_tree_ = pred.GetHandTree();
  action_sequence_ = pred.ActionSequence() + node->ActionName(s);
  street_buckets_ = pred.AllStreetBuckets();
  // Signifies opp data is uninitialized
  sum_opp_probs_ = -1;
  total_card_probs_ = nullptr;
}

int *VCFRState::StreetBuckets(int st) const {
  int max_num_hole_card_pairs = Game::NumHoleCardPairs(0);
  return street_buckets_.get() + st * max_num_hole_card_pairs;
}
