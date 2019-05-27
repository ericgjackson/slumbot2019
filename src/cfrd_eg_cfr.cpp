#include <stdio.h>
#include <stdlib.h>

#include <memory>
#include <string>

#include "betting_trees.h"
#include "canonical_cards.h"
#include "cfr_values.h"
#include "cfrd_eg_cfr.h"
#include "game.h"
#include "hand_tree.h"
#include "reach_probs.h"
#include "resolving_method.h"
#include "vcfr_state.h"

using std::shared_ptr;
using std::string;
using std::unique_ptr;

CFRDEGCFR::CFRDEGCFR(const CardAbstraction &ca, const CardAbstraction &base_ca,
		     const BettingAbstraction &ba, const BettingAbstraction &base_ba,
		     const CFRConfig &cc, const CFRConfig &base_cc,
		     const Buckets &buckets, bool cfrs, bool zero_sum, int num_threads) :
  EGCFR(ca, base_ca, ba, base_ba, cc, base_cc, buckets, ResolvingMethod::CFRD, cfrs, zero_sum,
	num_threads) {
}

static void RegretsToProbs(double *regrets, int num_succs, int dsi, double *probs) {
  double sum = 0;
  for (int s = 0; s < num_succs; ++s) {
    double r = regrets[s];
    if (r > 0) sum += r;
  }
  if (sum == 0) {
    for (int s = 0; s < num_succs; ++s) {
      probs[s] = s == dsi ? 1.0 : 0;
    }
  } else {
    for (int s = 0; s < num_succs; ++s) {
      double r = regrets[s];
      if (r > 0) probs[s] = r / sum;
      else       probs[s] = 0;
    }
  }
}

void CFRDEGCFR::HalfIteration(BettingTrees *subtrees, int target_p, VCFRState *state,
			      double *opp_cvs) {
  int p = state->P();
  const HandTree *hand_tree = state->GetHandTree();
  int subtree_st = subtrees->Root()->Street();
  int num_hole_card_pairs = Game::NumHoleCardPairs(subtree_st);
  int num_hole_cards = Game::NumCardsForStreet(0);
  int max_card1 = Game::MaxCard() + 1;
  int num_enc;
  if (num_hole_cards == 1) num_enc = max_card1;
  else                     num_enc = max_card1 * max_card1;
  shared_ptr<double []> villain_probs(new double[num_enc]);
  const CanonicalCards *hands = hand_tree->Hands(subtree_st, 0);
  // bool nonneg = nn_regrets_ && regret_floors_[subtree_st] >= 0;
  double probs[2];
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
    RegretsToProbs(cfrd_regrets_.get() + i * 2, 2, 0, probs);
    villain_probs[enc] = probs[0];
  }
  if (p == target_p) {
    state->SetOppProbs(villain_probs);
    EGCFR::HalfIteration(subtrees, *state);
  } else {
    // Opponent phase.  The target player plays his fixed range to the subgame.  The target
    // player's fixed range is embedded in the opp_probs in state.
    shared_ptr<double []> vals = EGCFR::HalfIteration(subtrees, *state);
    for (int i = 0; i < num_hole_card_pairs; ++i) {
      double *regrets = cfrd_regrets_.get() + i * 2;
      const Card *cards = hands->Cards(i);
      Card hi = cards[0];
      int enc;
      if (num_hole_cards == 1) {
	enc = hi;
      } else {
	Card lo = cards[1];
	enc = hi * max_card1 + lo;
      }
      double t_value = opp_cvs[i];
      double val = villain_probs[enc] * vals[i] + (1.0 - villain_probs[enc]) * t_value;
      double delta0 = vals[i] - val;
      double delta1 = t_value - val;
      regrets[0] += delta0;
      regrets[1] += delta1;
      // Assume non-neg
      if (regrets[0] < 0) regrets[0] = 0;
      if (regrets[1] < 0) regrets[1] = 0;
    }
  }
}

void CFRDEGCFR::SolveSubgame(BettingTrees *subtrees, int solve_bd, const ReachProbs &reach_probs,
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

  unique_ptr<bool []> players(new bool[num_players]);
  for (int p = 0; p < num_players; ++p) {
    players[p] = p == target_p;
  }
  sumprobs_.reset(new CFRValues(players.get(), subtree_streets.get(), solve_bd, subtree_st,
				buckets_, subtrees->GetBettingTree()));
  sumprobs_->AllocateAndClear(subtrees->GetBettingTree(), CFRValueType::CFR_DOUBLE, false, -1);
  
  int num_hole_card_pairs = Game::NumHoleCardPairs(subtree_st);
  cfrd_regrets_.reset(new double[num_hole_card_pairs * 2]);
  for (int i = 0; i < num_hole_card_pairs; ++i) {
    cfrd_regrets_[i * 2] = 0;
    cfrd_regrets_[i * 2 + 1] = 0;
  }
  
  VCFRState **initial_states = new VCFRState *[num_players];
  for (int p = 0; p < num_players; ++p) {
    initial_states[p] = new VCFRState(p, reach_probs.Get(p^1), hand_tree, action_sequence, solve_bd,
				      subtree_st);
    SetStreetBuckets(subtree_st, solve_bd, *initial_states[p]);
  }
  for (it_ = 1; it_ <= num_its; ++it_) {
    // Go from high to low to mimic slumbot2017 code
    for (int p = (int)num_players - 1; p >= 0; --p) {
      HalfIteration(subtrees, target_p, initial_states[p], opp_cvs);
    }
  }

  for (int p = 0; p < num_players; ++p) {
    delete initial_states[p];
  }
  delete [] initial_states;
}
