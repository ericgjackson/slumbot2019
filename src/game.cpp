#include <stdio.h>
#include <stdlib.h>

#include <memory>
#include <string>
#include <vector>

#include "game.h"
#include "params.h"
#include "split.h"

using std::string;
using std::unique_ptr;
using std::vector;

string Game::game_name_ = "";
int Game::max_street_ = 0;
int Game::num_players_ = 0;
int Game::num_ranks_ = 0;
int Game::num_suits_ = 0;
unique_ptr<int []> Game::first_to_act_;
int Game::small_blind_ = 0;
int Game::big_blind_ = 0;
int Game::ante_ = 0;
unique_ptr<int []> Game::num_cards_for_street_;
int Game::num_cards_in_deck_ = 0;
unsigned long long int Game::num_card_permutations_ = 0;
unique_ptr<int []> Game::num_hole_card_pairs_;
unique_ptr<int []> Game::num_board_cards_;

static int Factorial(int n) {
  if (n == 0) return 1;
  if (n == 1) return 1;
  return n * Factorial(n - 1);
}

void Game::Initialize(const Params &params) {
  game_name_ = params.GetStringValue("GameName");
  max_street_ = params.GetIntValue("MaxStreet");
  num_players_ = params.GetIntValue("NumPlayers");
  num_ranks_ = params.GetIntValue("NumRanks");
  if (num_ranks_ == 0) {
    fprintf(stderr, "There must be at least one rank\n");
    exit(-1);
  }
  num_suits_ = params.GetIntValue("NumSuits");
  if (num_suits_ == 0) {
    fprintf(stderr, "There must be at least one suit\n");
    exit(-1);
  }
  num_cards_in_deck_ = num_ranks_ * num_suits_;

  vector<int> ftav;
  ParseInts(params.GetStringValue("FirstToAct"), &ftav);
  if ((int)ftav.size() != max_street_ + 1) {
    fprintf(stderr, "Expected %i values in FirstToAct\n", max_street_ + 1);
    exit(-1);
  }
  first_to_act_.reset(new int[max_street_ + 1]);
  for (int st = 0; st <= max_street_; ++st) {
    first_to_act_[st] = ftav[st];
  }
  
  num_cards_for_street_.reset(new int[max_street_ + 1]);
  num_cards_for_street_[0] = params.GetIntValue("NumHoleCards");
  if (max_street_ >= 1) {
    if (! params.IsSet("NumFlopCards")) {
      fprintf(stderr, "NumFlopCards param needs to be set\n");
      exit(-1);
    }
    num_cards_for_street_[1] = params.GetIntValue("NumFlopCards");
  }
  if (max_street_ >= 2) num_cards_for_street_[2] = 1;
  if (max_street_ >= 3) num_cards_for_street_[3] = 1;
  ante_ = params.GetIntValue("Ante");
  small_blind_ = params.GetIntValue("SmallBlind");
  big_blind_ = params.GetIntValue("BigBlind");

  // Calculate num_card_permutations_, the number of ways of dealing out
  // the cards to both players and the board.
  // This assumes a two player game.
  num_card_permutations_ = 1ULL;
  int num_cards_left = num_cards_in_deck_;
  for (int p = 0; p < 2; ++p) {
    int num_hole_cards = num_cards_for_street_[0];
    int multiplier = 1;
    for (int n = (num_cards_left - num_hole_cards) + 1;
	 n <= num_cards_left; ++n) {
      multiplier *= n;
    }
    num_card_permutations_ *= multiplier / Factorial(num_hole_cards);
    num_cards_left -= num_hole_cards;
  }
  for (int s = 1; s <= max_street_; ++s) {
    int num_street_cards = num_cards_for_street_[s];
    int multiplier = 1;
    for (int n = (num_cards_left - num_street_cards) + 1;
	 n <= num_cards_left; ++n) {
      multiplier *= n;
    }
    num_card_permutations_ *= multiplier / Factorial(num_street_cards);
    num_cards_left -= num_street_cards;
  }

  num_hole_card_pairs_.reset(new int[max_street_ + 1]);
  num_board_cards_.reset(new int[max_street_ + 1]);
  int num_board_cards = 0;
  for (int st = 0; st <= max_street_; ++st) {
    if (st >= 1) num_board_cards += num_cards_for_street_[st];
    num_board_cards_[st] = num_board_cards;
    // Num cards left in deck after all board cards dealt
    int num_remaining = num_cards_in_deck_ - num_board_cards;
    if (num_cards_for_street_[0] == 2) {
      num_hole_card_pairs_[st] = (num_remaining * (num_remaining - 1)) / 2;
    } else if (num_cards_for_street_[0] == 1) {
      num_hole_card_pairs_[st] = num_remaining;
    } else {
      fprintf(stderr, "Game::Game: Expected 1 or 2 hole cards\n");
      exit(-1);
    }
  }
}

// Assume the hole cards for each player have been dealt out and the board
// cards for any street prior to the given street.  How many ways of dealing
// out the next street are there?
int Game::StreetPermutations(int street) {
  int num_cards_left = num_cards_in_deck_;
  num_cards_left -= 2 * num_cards_for_street_[0];
  for (int s = 1; s < street; ++s) {
    num_cards_left -= num_cards_for_street_[s];
  }
  int num_street_cards = num_cards_for_street_[street];
  int multiplier = 1;
  for (int n = (num_cards_left - num_street_cards) + 1; n <= num_cards_left; ++n) {
    multiplier *= n;
  }
  return multiplier / Factorial(num_street_cards);
}

// Assume the hole cards for *ourselves only* have been dealt out and the board
// cards for any street prior to the given street.  How many ways of dealing
// out the next street are there?
int Game::StreetPermutations2(int street) {
  int num_cards_left = num_cards_in_deck_;
  num_cards_left -= num_cards_for_street_[0];
  for (int s = 1; s < street; ++s) {
    num_cards_left -= num_cards_for_street_[s];
  }
  int num_street_cards = num_cards_for_street_[street];
  int multiplier = 1;
  for (int n = (num_cards_left - num_street_cards) + 1; n <= num_cards_left; ++n) {
    multiplier *= n;
  }
  return multiplier / Factorial(num_street_cards);
}

// Assume only that the board cards for any street prior to the given street.  How many ways of
// dealing out the next street are there?
int Game::StreetPermutations3(int street) {
  int num_cards_left = num_cards_in_deck_;
  for (int s = 1; s < street; ++s) {
    num_cards_left -= num_cards_for_street_[s];
  }
  int num_street_cards = num_cards_for_street_[street];
  int multiplier = 1;
  for (int n = (num_cards_left - num_street_cards) + 1; n <= num_cards_left; ++n) {
    multiplier *= n;
  }
  return multiplier / Factorial(num_street_cards);
}

// Assume the hole cards for each player have been dealt out and the board
// cards for any street prior to the given street.  How many ways of dealing
// out the remainder of the board are there?
int Game::BoardPermutations(int street) {
  int num_cards_left = num_cards_in_deck_;
  num_cards_left -= 2 * num_cards_for_street_[0];
  for (int s = 1; s < street; ++s) {
    num_cards_left -= num_cards_for_street_[s];
  }
  int num_permutations = 1;
  for (int s = street; s <= max_street_; ++s) {
    int num_street_cards = num_cards_for_street_[s];
    int multiplier = 1;
    for (int n = (num_cards_left - num_street_cards) + 1; n <= num_cards_left; ++n) {
      multiplier *= n;
    }
    num_permutations *= multiplier / Factorial(num_street_cards);
    num_cards_left -= num_street_cards;
  }
  return num_permutations;
}
