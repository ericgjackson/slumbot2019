#include <stdio.h>
#include <stdlib.h>

#include <memory>
#include <string>

#include "betting_tree.h"
#include "canonical_cards.h"
#include "cfr_values.h"
#include "combined_eg_cfr.h"
#include "game.h"
#include "hand_tree.h"
#include "vcfr_state.h"

using std::string;
using std::unique_ptr;

CombinedEGCFR::CombinedEGCFR(const CardAbstraction &ca, const CardAbstraction &base_ca,
			     const BettingAbstraction &ba, const BettingAbstraction &base_ba,
			     const CFRConfig &cc, const CFRConfig &base_cc,
			     const Buckets &buckets, bool cfrs, bool zero_sum, int num_threads) :
  EGCFR(ca, base_ca, ba, base_ba, cc, base_cc, buckets,
	ResolvingMethod::COMBINED, cfrs, zero_sum, num_threads) {
}

// Simulate dummy root node with two succs.  Succ 0 corresponds to playing to
// the subgame.  Succ 1 corresponds to taking the T value.
// Use "villain" to mean the player who is not the target player.
void CombinedEGCFR::HalfIteration(BettingTree *subtree, int solve_bd, int p, int target_p,
				  VCFRState *state, double **reach_probs, double *opp_cvs) {
  const HandTree *hand_tree = state->GetHandTree();
  double *villain_reach_probs = reach_probs[target_p^1];
  int subtree_st = subtree->Root()->Street();
  int num_hole_card_pairs = Game::NumHoleCardPairs(subtree_st);
  int num_hole_cards = Game::NumCardsForStreet(0);
  int max_card1 = Game::MaxCard() + 1;
  int num_enc;
  if (num_hole_cards == 1) num_enc = max_card1;
  else                     num_enc = max_card1 * max_card1;
  double *villain_probs = new double[num_enc];
  const CanonicalCards *hands = hand_tree->Hands(subtree_st, 0);
  // bool nonneg = nn_regrets_ && regret_floors_[subtree_st] >= 0;
  double sum_villain_reach_probs = 0;
  for (int i = 0; i < num_hole_card_pairs; ++i) {
    const Card *cards = hands->Cards(i);
    Card hi = cards[0];
    int enc;
    if (num_hole_cards == 1) {
      enc = hi;
    } else {
      Card lo = cards[1];
      enc = hi * max_card1 + lo;
    }
    villain_probs[enc] = villain_reach_probs[enc];
    sum_villain_reach_probs += villain_probs[enc];
  }
}

void CombinedEGCFR::SolveSubgame(BettingTree *subtree, int solve_bd, double **reach_probs,
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

  unique_ptr<bool []> players(new bool[num_players]);
  for (int p = 0; p < num_players; ++p) {
    players[p] = p == target_p;
  }
  sumprobs_.reset(new CFRValues(players.get(), subtree_streets.get(), solve_bd,
				subtree_st, buckets_, subtree));
  sumprobs_->AllocateAndClearDoubles(subtree->Root(), -1);

  int num_hole_card_pairs = Game::NumHoleCardPairs(subtree_st);
  combined_regrets_.reset(new double[num_hole_card_pairs * 2]);
  for (int i = 0; i < num_hole_card_pairs; ++i) {
    combined_regrets_[i * 2] = 0;
    combined_regrets_[i * 2 + 1] = 0;
  }
  
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
      HalfIteration(subtree, solve_bd, p, target_p, initial_states[p],
		    reach_probs, opp_cvs);
    }
  }

  for (int p = 0; p < num_players; ++p) {
    delete initial_states[p];
  }
  delete [] initial_states;
  DeleteStreetBuckets(street_buckets);
}

