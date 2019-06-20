#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <memory>

#include "betting_abstraction.h"
#include "betting_trees.h"
#include "board_tree.h"
#include "buckets.h"
#include "canonical_cards.h"
#include "card_abstraction.h"
#include "cards.h"
#include "cfr_config.h"
#include "cfr_utils.h"
#include "cfr_values.h"
#include "cfrp.h"
#include "constants.h"
#include "files.h"
#include "game.h"
#include "hand_tree.h"
#include "hand_value_tree.h"
#include "io.h"
#include "rgbr.h"
#include "split.h"
#include "vcfr_state.h"
#include "vcfr.h"

using std::shared_ptr;
using std::unique_ptr;

RGBR::RGBR(const CardAbstraction &ca, const CFRConfig &cc, const Buckets &buckets, bool current,
	   bool quantize, int num_threads, const bool *streets) :
  CFRP(ca, cc, buckets, num_threads) {
  br_current_ = current;
  quantize_ = quantize;
  value_calculation_ = true;

  int max_street = Game::MaxStreet();
  if (streets) {
    for (int st = 0; st <= max_street; ++st) {
      best_response_streets_[st] = streets[st];
    }
  } else {
    for (int st = 0; st <= max_street; ++st) {
      best_response_streets_[st] = true;
    }
  }

  BoardTree::Create();
}

RGBR::~RGBR(void) {
}

double RGBR::Go(int it, int p, const BettingAbstraction &ba) {
  it_ = it;
  // If P0 is the best-responder, then we will want sumprobs generated in
  // the P1 CFR run.
  target_p_ = p^1;

  asymmetric_ = ba.Asymmetric();
  if (asymmetric_) {
    betting_trees_.reset(new BettingTrees(ba, target_p_));
  } else {
    betting_trees_.reset(new BettingTrees(ba));
  }

  char dir[500];
  sprintf(dir, "%s/%s.%u.%s.%i.%i.%i.%s.%s", Files::OldCFRBase(),
	  Game::GameName().c_str(), Game::NumPlayers(),
	  card_abstraction_.CardAbstractionName().c_str(), Game::NumRanks(),
	  Game::NumSuits(), Game::MaxStreet(),
	  ba.BettingAbstractionName().c_str(),
	  cfr_config_.CFRConfigName().c_str());
  if (asymmetric_) {
    char buf[100];
    sprintf(buf, ".p%u", target_p_);
    strcat(dir, buf);
  }

  bool *streets = nullptr;
  int max_street = Game::MaxStreet();
  if (subgame_street_ >= 0 && subgame_street_ <= max_street) {
    streets = new bool[max_street + 1];
    for (int st = 0; st <= max_street; ++st) {
      streets[st] = st < subgame_street_;
    }
  }  

  bool all_streets = true;
  for (int st = 0; st <= max_street; ++st) {
    if (! best_response_streets_[st]) all_streets = false;
  }
  int num_players = Game::NumPlayers();
  unique_ptr<bool []> players(new bool[num_players]);
  for (int p1 = 0; p1 < num_players; ++p1) {
    players[p1] = p1 != p || ! all_streets;
  }
  if (br_current_) {
    regrets_.reset(new CFRValues(players.get(), streets, 0, 0, buckets_,
				 betting_trees_->GetBettingTree()));
    regrets_->Read(dir, it_, betting_trees_->GetBettingTree(), "x", -1, false, quantize_);
    sumprobs_.reset();
  } else {
    sumprobs_.reset(new CFRValues(players.get(), streets, 0, 0, buckets_,
				  betting_trees_->GetBettingTree()));
    sumprobs_->Read(dir, it_, betting_trees_->GetBettingTree(), "x", -1, true, quantize_);
    regrets_.reset();
  }

  unique_ptr<bool []> bucketed_streets(new bool[max_street + 1]);
  bucketed_ = false;
  for (int st = 0; st <= max_street; ++st) {
    bucketed_streets[st] = ! buckets_.None(st);
    if (bucketed_streets[st]) bucketed_ = true;
  }

  delete [] streets;

  // Some of this logic is replicated in CFRP::HalfIteration().

#if 0
  if (subgame_street_ >= 0 && subgame_street_ <= max_street) {
    // subgame_running_ should be false for all threads
    // active_subgames_ should be nullptr for all threads
    for (int t = 0; t < num_threads_; ++t) {
      int ret = sem_post(&available_);
      if (ret != 0) {
	fprintf(stderr, "sem_post failed\n");
	exit(-1);
      }
    }
  }
#endif

  // if (subgame_street_ >= 0 && subgame_street_ <= max_street) pre_phase_ = true;
  shared_ptr<double []> vals = ProcessRoot(betting_trees_.get(), p, hand_tree_.get());
#if 0
  if (subgame_street_ >= 0 && subgame_street_ <= max_street) {
    WaitForFinalSubgames();
    pre_phase_ = false;
    vals = ProcessRoot(betting_trees_.get(), p, hand_tree_.get());
  }
#endif
  
  int num_hole_card_pairs = Game::NumHoleCardPairs(0);

#if 0
  if (p == 1) {
    const CanonicalCards *hands = hand_tree_->Hands(0, 0);
    for (int i = 0; i < num_hole_card_pairs; ++i) {
      const Card *hole_cards = hands->Cards(i);
      OutputTwoCards(hole_cards);
      printf(" %f (i %u)\n", vals[i], i);
    }
    fflush(stdout);
  }
#endif
  
  int num_remaining = Game::NumCardsInDeck() -
    Game::NumCardsForStreet(0);
  int num_opp_hole_card_pairs = num_remaining * (num_remaining - 1) / 2;
  double sum = 0;
  for (int i = 0; i < num_hole_card_pairs; ++i) {
    sum += vals[i];
#if 0
    // I don't know why I had this hear.  Maybe when I was displaying
    // per hand values.
    vals[i] /= num_opp_hole_card_pairs;
#endif
  }
  double overall = sum / (num_hole_card_pairs * num_opp_hole_card_pairs);
#if 0
  if (p == 1) {
    Card aa_cards[2];
    aa_cards[0] = MakeCard(12, 3);
    aa_cards[1] = MakeCard(12, 2);
    int aa_hcp = HCPIndex(0, aa_cards);
    double aa_p1_val = vals[aa_hcp] / num_opp_hole_card_pairs;
    printf("AA P1: %f (hcp %i)\n", aa_p1_val, aa_hcp);
  }
#endif

  regrets_.reset();
  sumprobs_.reset();

  return overall;
}
