#include <stdio.h>
#include <stdlib.h>

#include <string>

#include "betting_tree.h"
#include "cfr_utils.h" // CommonBetResponseCalcs()
#include "game.h"
#include "hand_tree.h"
#include "vcfr_state.h"

using std::string;

// Currently allocate for all streets.  Could be smarter and allocate just
// for streets that we have buckets for.  But we don't call this code often
// and is cheap both in terms of time and memory.
int **AllocateStreetBuckets(void) {
  int max_street = Game::MaxStreet();
  int **street_buckets = new int *[max_street + 1];
  for (int st = 0; st <= max_street; ++st) {
    int num_hole_card_pairs = Game::NumHoleCardPairs(st);
    street_buckets[st] = new int[num_hole_card_pairs];
  }

  return street_buckets;
}

void DeleteStreetBuckets(int **street_buckets) {
  int max_street = Game::MaxStreet();
  for (int st = 0; st <= max_street; ++st) {
    delete [] street_buckets[st];
  }
  delete [] street_buckets;
}

double *AllocateOppProbs(bool initialize) {
  int num_hole_cards = Game::NumCardsForStreet(0);
  int max_card1 = Game::MaxCard() + 1;
  int num_enc;
  if (num_hole_cards == 1) num_enc = max_card1;
  else                     num_enc = max_card1 * max_card1;
  double *opp_probs = new double[num_enc];
  if (initialize) {
    for (int i = 0; i < num_enc; ++i) opp_probs[i] = 1.0;
  }
  return opp_probs;
}

// Called at the root of the tree.  We need to initialize total_card_probs_
// and sum_opp_probs_ because an open fold is allowed.
VCFRState::VCFRState(double *opp_probs, int **street_buckets, const HandTree *hand_tree) {
  opp_probs_ = opp_probs;
  street_buckets_ = street_buckets;
  action_sequence_ = "x";
  hand_tree_ = hand_tree;
  root_bd_ = 0;
  root_bd_st_ = 0;
  int max_card1 = Game::MaxCard() + 1;
  total_card_probs_ = new double[max_card1];
  const CanonicalCards *hands = hand_tree_->Hands(0, 0);
  CommonBetResponseCalcs(0, hands, opp_probs_, &sum_opp_probs_,
			 total_card_probs_);
}
  
// Called at an internal street-initial node.  We initialize total_card_probs_
// to nullptr and sum_opp_probs_ to zero because we know we will come
// across an opp-choice node before we need those members.
VCFRState::VCFRState(double *opp_probs, const HandTree *hand_tree, int lbd,
		     const string &action_sequence, int root_bd, int root_bd_st,
		     int **street_buckets) {
  opp_probs_ = opp_probs;
  total_card_probs_ = nullptr;
  sum_opp_probs_ = 0;
  hand_tree_ = hand_tree;
  action_sequence_ = action_sequence;
  root_bd_ = root_bd;
  root_bd_st_ = root_bd_st;
  street_buckets_ = street_buckets;
}

// Called at an internal street-initial node.  Unlike the above constructor,
// we take a total_card_probs parameter and initialize it.
VCFRState::VCFRState(double *opp_probs, double *total_card_probs, const HandTree *hand_tree, int st,
		     int lbd, const string &action_sequence, int root_bd, int root_bd_st,
		     int **street_buckets) {
  opp_probs_ = opp_probs;
  total_card_probs_ = total_card_probs;
  hand_tree_ = hand_tree;
  action_sequence_ = action_sequence;
  root_bd_ = root_bd;
  root_bd_st_ = root_bd_st;
  street_buckets_ = street_buckets;
  const CanonicalCards *hands = hand_tree_->Hands(st, lbd);
  CommonBetResponseCalcs(root_bd_st_, hands, opp_probs_, &sum_opp_probs_,
			 total_card_probs_);
}

// Create a new VCFRState corresponding to taking an action of ours.
VCFRState::VCFRState(const VCFRState &pred, Node *node, int s) {
  opp_probs_ = pred.OppProbs();
  hand_tree_ = pred.GetHandTree();
  action_sequence_ = pred.ActionSequence() + node->ActionName(s);
  root_bd_ = pred.RootBd();
  root_bd_st_ = pred.RootBdSt();
  street_buckets_ = pred.StreetBuckets();
  total_card_probs_ = pred.TotalCardProbs();
  sum_opp_probs_ = pred.SumOppProbs();
}

// Create a new VCFRState corresponding to taking an opponent action.
VCFRState::VCFRState(const VCFRState &pred, Node *node, int s, double *opp_probs,
		     double sum_opp_probs, double *total_card_probs) {
  opp_probs_ = opp_probs;
  hand_tree_ = pred.GetHandTree();
  action_sequence_ = pred.ActionSequence() + node->ActionName(s);
  root_bd_ = pred.RootBd();
  root_bd_st_ = pred.RootBdSt();
  street_buckets_ = pred.StreetBuckets();
  sum_opp_probs_ = sum_opp_probs;
  total_card_probs_ = total_card_probs;
}

#if 0
// If I want to update total_card_probs_ and sum_opp_probs_, do this.
void VCFRState::SetOppProbs(double *opp_probs, int st, int lbd) {
  opp_probs_ = opp_probs;
  const CanonicalCards *hands = hand_tree_->Hands(st, lbd);
  CommonBetResponseCalcs(root_bd_st_, hands, opp_probs_, &sum_opp_probs_,
			 total_card_probs_);
}
#endif

// We don't own any of the arrays.  Caller must delete.
VCFRState::~VCFRState(void) {
}

