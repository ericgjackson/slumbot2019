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
#include "resolving_method.h"
#include "vcfr_state.h"

using std::shared_ptr;
using std::string;
using std::unique_ptr;

CombinedEGCFR::CombinedEGCFR(const CardAbstraction &ca, const CardAbstraction &base_ca,
			     const BettingAbstraction &ba, const BettingAbstraction &base_ba,
			     const CFRConfig &cc, const CFRConfig &base_cc,
			     const Buckets &buckets, bool cfrs, bool zero_sum, int num_threads) :
  EGCFR(ca, base_ca, ba, base_ba, cc, base_cc, buckets, ResolvingMethod::COMBINED, cfrs, zero_sum,
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

// Simulate dummy root node with two succs.  Succ 0 corresponds to playing to
// the subgame.  Succ 1 corresponds to taking the T value.
// Use "villain" to mean the player who is not the target player.
void CombinedEGCFR::HalfIteration(BettingTree *subtree, int solve_bd, int p, int target_p,
				  VCFRState *state, shared_ptr<double []> *reach_probs,
				  double *opp_cvs) {
  const HandTree *hand_tree = state->GetHandTree();
  shared_ptr<double []> villain_reach_probs = reach_probs[target_p^1];
  int subtree_st = subtree->Root()->Street();
  int num_hole_card_pairs = Game::NumHoleCardPairs(subtree_st);
  int num_hole_cards = Game::NumCardsForStreet(0);
  int max_card1 = Game::MaxCard() + 1;
  int num_enc;
  if (num_hole_cards == 1) num_enc = max_card1;
  else                     num_enc = max_card1 * max_card1;
  shared_ptr<double []> villain_probs(new double[num_enc]);
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
  
  double sum_to_add = 0;
  double probs[2];
  for (int i = 0; i < num_hole_card_pairs; ++i) {
    const Card *cards = hands->Cards(i);
    Card lo, hi = cards[0];
    int enc;
    if (num_hole_cards == 1) {
      enc = hi;
    } else {
      lo = cards[1];
      enc = hi * max_card1 + lo;
    }
    double rem = 1.0 - villain_probs[enc];
    if (rem > 0) {
      double *regrets = &combined_regrets_[i * 2];
      RegretsToProbs(regrets, 2, 0, probs);
      sum_to_add += rem * probs[0];
    }
  }

  double scale = 1.0;
  // Experiment
  if (sum_villain_reach_probs > 0) {
    // 0.2 best so far
    double cfrd_cap = 0.2;
    if (sum_to_add > sum_villain_reach_probs * cfrd_cap) {
      scale = (sum_villain_reach_probs * cfrd_cap) / sum_to_add;
    }
  }
#if 0
  if (sum_villain_reach_probs < 10.0) {
    if (sum_to_add > sum_villain_reach_probs * 0.4) {
      scale = (sum_villain_reach_probs  * 0.4) / sum_to_add;
    }
  } else {
    if (sum_to_add > sum_villain_reach_probs * 0.1) {
      scale = (sum_villain_reach_probs * 0.1) / sum_to_add;
    }
  }
#endif
#if 0
  if (sum_to_add >= 4.0) scale = 4.0 / sum_to_add;
#endif
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
    double rem = 1.0 - villain_probs[enc];
    if (rem > 0) {
      double *regrets = &combined_regrets_[i * 2];
      RegretsToProbs(regrets, 2, 0, probs);
      villain_probs[enc] += rem * probs[0] * scale;
    }
  }

  double prob_mass = sum_villain_reach_probs + scale * sum_to_add;
  // Best: 0.1
  double uniform_add = 0.1 * prob_mass / num_hole_card_pairs;
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
    villain_probs[enc] += uniform_add;
    if (villain_probs[enc] > 1.0) villain_probs[enc] = 1.0;
  }

  if (p == target_p) {
    state->SetOppProbs(villain_probs);
    double *vals = EGCFR::HalfIteration(subtree, solve_bd, p, *state);
    delete [] vals;
  } else {
    // Opponent phase.  The target player plays his fixed range to the subgame.  The target
    // player's fixed range is embedded in the opp_probs in state.
    double *vals = EGCFR::HalfIteration(subtree, solve_bd, p, *state);
    for (int i = 0; i < num_hole_card_pairs; ++i) {
      double *regrets = &combined_regrets_[i * 2];
      const Card *cards = hands->Cards(i);
      Card lo, hi = cards[0];
      int enc;
      if (num_hole_cards == 1) {
	enc = hi;
      } else {
	lo = cards[1];
	enc = hi * max_card1 + lo;
      }
      double t_value = opp_cvs[i];
#if 0
      // Temporary
      // Add random noise to T value
      t_value += (RandZeroToOne() - 0.5) * subtree->Root()->PotSize() * 0.05 *
	sum_target_probs_[i];
#endif
      double val = villain_probs[enc] * vals[i] + (1.0 - villain_probs[enc]) * t_value;
      double delta0 = vals[i] - val;
      double delta1 = t_value - val;
      regrets[0] += delta0;
      regrets[1] += delta1;
      if (regrets[0] < 0) regrets[0] = 0;
      if (regrets[1] < 0) regrets[1] = 0;
    }
    delete [] vals;
  }
}

void CombinedEGCFR::SolveSubgame(BettingTree *subtree, int solve_bd,
				 shared_ptr<double []> *reach_probs, const string &action_sequence,
				 const HandTree *hand_tree, double *opp_cvs, int target_p,
				 bool both_players, int num_its) {
  int subtree_st = subtree->Root()->Street();
  int num_players = Game::NumPlayers();
  int max_street = Game::MaxStreet();
  
  unique_ptr<bool []> subtree_streets(new bool[max_street + 1]);
  for (int st = 0; st <= max_street; ++st) {
    subtree_streets[st] = st >= subtree_st;
  }
  regrets_.reset(new CFRValues(nullptr, subtree_streets.get(), solve_bd, subtree_st, buckets_,
			       subtree));
  regrets_->AllocateAndClearDoubles(subtree->Root(), -1);

  // Should honor sumprobs_streets_

  unique_ptr<bool []> players(new bool[num_players]);
  for (int p = 0; p < num_players; ++p) {
    players[p] = p == target_p;
  }
  sumprobs_.reset(new CFRValues(players.get(), subtree_streets.get(), solve_bd, subtree_st,
				buckets_, subtree));
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
    initial_states[p] = new VCFRState(reach_probs[p^1], hand_tree, 0, action_sequence, solve_bd,
				      subtree_st, street_buckets);
  }
  SetStreetBuckets(subtree_st, solve_bd, *initial_states[0]);
  for (it_ = 1; it_ <= num_its; ++it_) {
    // Go from high to low to mimic slumbot2017 code
    for (int p = (int)num_players - 1; p >= 0; --p) {
      HalfIteration(subtree, solve_bd, p, target_p, initial_states[p], reach_probs, opp_cvs);
    }
  }

  for (int p = 0; p < num_players; ++p) {
    delete initial_states[p];
  }
  delete [] initial_states;
  DeleteStreetBuckets(street_buckets);
}

