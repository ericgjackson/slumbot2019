#ifndef _GAME_H_
#define _GAME_H_

#include <memory>
#include <string>

#include "cards.h"

class Params;

class Game {
 public:
  static void Initialize(const Params &params);
  static const std::string &GameName(void) {return game_name_;}
  static int MaxStreet(void) {return max_street_;}
  static int NumPlayers(void) {return num_players_;}
  static int NumRanks(void) {return num_ranks_;}
  static int HighRank(void) {return num_ranks_ - 1;}
  static int NumSuits(void) {return num_suits_;}
  static Card MaxCard(void) {
    return MakeCard(num_ranks_ - 1, num_suits_ - 1);
  }
  static int FirstToAct(int st) {return first_to_act_[st];}
  static int SmallBlind(void) {return small_blind_;}
  static int BigBlind(void) {return big_blind_;}
  static int Ante(void) {return ante_;}
  static int NumCardsForStreet(int st) {
    return num_cards_for_street_[st];
  }
  static int NumHoleCardPairs(int st) {
    return num_hole_card_pairs_[st];
  }
  static int NumBoardCards(int st) {
    return num_board_cards_[st];
  }
  static int NumCardsInDeck(void) {return num_cards_in_deck_;}
#if 0
  // Commented out because we do not support multiplayer currently.
  // Who needs this anyhow?
  static unsigned long long int NumCardPermutations(void) {
    return num_card_permutations_;
  }
#endif
  static int StreetPermutations(int street);
  static int StreetPermutations2(int street);
  static int StreetPermutations3(int street);
  static int BoardPermutations(int street);
 private:
  static std::string game_name_;
  static int max_street_;
  static int num_players_;
  static int num_ranks_;
  static int num_suits_;
  static std::unique_ptr<int []> first_to_act_;
  static int small_blind_;
  static int big_blind_;
  static int ante_;
  static std::unique_ptr<int []> num_cards_for_street_;
  static int num_cards_in_deck_;
  static unsigned long long int num_card_permutations_;
  static std::unique_ptr<int []> num_hole_card_pairs_;
  static std::unique_ptr<int []> num_board_cards_;
};

#endif
