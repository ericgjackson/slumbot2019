// These methods differ from the methods in canonical_cards.cpp in that
// you can provide raw cards for multiple streets and get back the canonical
// version of those cards.  The methods in canonical_cards.cpp like
// ToCanon() can only canonicalize the cards on a single street.
//
// We need to make sure we canonicalize in the same way as canonical_cards.cpp
// though.

#include <stdio.h>
#include <stdlib.h>

#include <memory>

#include "canonical.h"
#include "cards.h"
#include "constants.h"
#include "game.h"
#include "sorting.h"

using std::unique_ptr;

// There are 13 possible ranks (2...A).  For a given street we encode the
// suit with 13 bits indicating whether that rank is present in the given
// suit.
static int EncodeRanksOfRawSuitForStreet(int raw_suit, int street, const Card *raw_cards) {
  int street_code = 0;
  int num_cards_for_street = Game::NumCardsForStreet(street);
  for (int i = 0; i < num_cards_for_street; ++i) {
    Card raw_card = raw_cards[i];
    int this_raw_suit = Suit(raw_card);
    if (this_raw_suit == raw_suit) {
      int rank = Rank(raw_card);
      street_code |= (1 << rank);
    }
  }
  return street_code;
}

// Each street is encoded separately.  Then the codes for each street are
// concatenated into an overall code.  The most significant bits are dedicated
// to the flop, the next significant to the turn, etc., with the least
// significant dedicated to the hole cards.
//
// You can pass in nullptr for raw_hole_cards.
static unsigned long long int EncodeRanksOfRawSuit(int raw_suit, const Card *raw_board,
						   const Card *raw_hole_cards,
						   int max_street) {
  unsigned long long int code = 0;
  const Card *raw_board_ptr = raw_board;
  for (int st = 1; st <= max_street; ++st) {
    unsigned long long int street_code =
      EncodeRanksOfRawSuitForStreet(raw_suit, st, raw_board_ptr);
    code |= street_code << (16 * (max_street + 1 - st));
    int num_cards_for_street = Game::NumCardsForStreet(st);
    raw_board_ptr += num_cards_for_street;
  }
  if (raw_hole_cards) {
    unsigned long long int street_code =
      EncodeRanksOfRawSuitForStreet(raw_suit, 0, raw_hole_cards);
    code |= street_code;
  }
  return code;
}

void CanonicalizeCards(const Card *raw_board, const Card *raw_hole_cards, int max_street,
		       Card *canon_board, Card *canon_hole_cards, int *suit_mapping) {
  unsigned long long int suit_codes[4];
  for (unsigned int s = 0; s < 4; ++s) {
    suit_codes[s] = EncodeRanksOfRawSuit(s, raw_board, raw_hole_cards, max_street);
  }
  int sorted_suits[4];
  bool used[4];
  for (int s = 0; s < 4; ++s) used[s] = false;
  for (int pos = 0; pos < 4; ++pos) {
    int best_s = -1;
    unsigned long long int best_rank_code = 0;
    for (int s = 0; s < 4; ++s) {
      if (used[s]) continue;
      unsigned long long int rank_code = suit_codes[s];
      if (best_s == -1 || rank_code > best_rank_code) {
	best_rank_code = rank_code;
	best_s = s;
      }
    }
    sorted_suits[pos] = best_s;
    used[best_s] = true;
  }

  for (int i = 0; i < 4; ++i) {
    int raw_suit = sorted_suits[i];
    suit_mapping[raw_suit] = i;
  }

  // Canonicalize the cards.  Also sort each street's cards from high to low.
  int num_board_cards = Game::NumBoardCards(max_street);
  unique_ptr<Card []> canon_street_cards(new Card[num_board_cards]);
  int index = 0;
  for (int st = 1; st <= max_street; ++st) {
    int num_cards_for_street = Game::NumCardsForStreet(st);
    for (int i = 0; i < num_cards_for_street; ++i) {
      Card raw_card = raw_board[index + i];
      canon_street_cards[i] = MakeCard(Rank(raw_card), suit_mapping[Suit(raw_card)]);
    }
    SortCards(canon_street_cards.get(), num_cards_for_street);
    for (int i = 0; i < num_cards_for_street; ++i) {
      canon_board[index + i] = canon_street_cards[i];
    }
    index += num_cards_for_street;
  }

  if (raw_hole_cards) {
    int num_hole_cards = Game::NumCardsForStreet(0);
    for (int i = 0; i < num_hole_cards; ++i) {
      Card raw_card = raw_hole_cards[i];
      canon_hole_cards[i] = MakeCard(Rank(raw_card), suit_mapping[Suit(raw_card)]);
    }
    SortCards(canon_hole_cards, num_hole_cards);
  }
}

void CanonicalizeCards(const Card *raw_board, const Card *raw_hole_cards, int max_street,
		       Card *canon_board, Card *canon_hole_cards) {
  int suit_mapping[4];
  CanonicalizeCards(raw_board, raw_hole_cards, max_street, canon_board,
		    canon_hole_cards, suit_mapping);
}
