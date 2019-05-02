// Alberta has a notion of suit groups.  Suits that are equivalent are in the same group.
//
// I would like to incrementally add cards and not be limited to adding
// three cards.  For starters, should be able to calculate canonical flop
// boards by adding three cards to none.  And then canonical turn boards
// by adding one card to the canonical flops.
//
// Need canon mapping to map from non-canonical cards to (indices of) canonical
// cards.  Alberta actually just maintains map to n^2 encoding of canonical
// cards.
// Need weight for canonical cards which is number of raw card combinations
// that correspond to this canonical card combination.
// Need card mapping to map from indices to cards (or perhaps an encoding
// like hi * (max_card + 1) + lo).
// Alberta maintains a map from raw next street cards to prior street
// canonical indices.
//
// Does the canonicalization in ToCanon() need to be in sync with the
// canonicalization in AddNCards()?  ToCanon() will map
// AcKcQc/3s2h to AcKcQc/3d2h.  It gives the lower suit (diamonds) to the
// higher hole card.

#include <algorithm>
#include <vector>

#include "canonical_cards.h"
#include "cards.h"
#include "game.h"
#include "hand_value_tree.h"

using std::unique_ptr;
using std::vector;

CanonicalCards::CanonicalCards(int n, const Card *previous, int num_previous,
			       int previous_suit_groups, bool maintain_suit_groups) {
  n_ = n;
  suit_groups_.reset(nullptr);
  hand_values_.reset(nullptr);
  int max_card = Game::MaxCard();
  int index = 0;
  int num_remaining = Game::NumCardsInDeck() - num_previous;
  num_canon_ = 0;
  if (n == 1) {
    cards_.reset(new Card[num_remaining]);
    num_variants_.reset(new unsigned char[num_remaining]);
    canon_.reset(new int[num_remaining]);
    if (maintain_suit_groups) {
      suit_groups_.reset(new int[num_remaining]);
    }
    Card canon_cards[1];
    for (int c = 0; c <= max_card; ++c) {
      if (InCards(c, previous, num_previous)) continue;
      cards_[index] = c;
      int num_mappings = NumMappings(&cards_[index], n,
					      previous_suit_groups);
      num_variants_[index] = num_mappings;
      short encoding;
      if (num_mappings == 0) {
	ToCanon(&cards_[index], n, previous_suit_groups, canon_cards);
	encoding = canon_cards[0];
      } else {
	encoding = c;
	++num_canon_;
      }
      canon_[index] = encoding;
      if (maintain_suit_groups) {
	UpdateSuitGroups(&cards_[index], n_, previous_suit_groups,
			 &suit_groups_[index]);
      }
      ++index;
    }
    if (index != num_remaining) {
      fprintf(stderr, "Index too large: %u nr %u np %u mc %u "
	      "prev %i %i %i %i %i\n",
	      index, num_remaining, num_previous, max_card,
	      (int)previous[0], (int)previous[1], (int)previous[2],
	      (int)previous[3], (int)previous[4]);
      exit(-1);
    }
  } else if (n == 2) {
    int num = num_remaining * (num_remaining - 1) / 2;
    cards_.reset(new Card[2 * num]);
    num_variants_.reset(new unsigned char[num]);
    canon_.reset(new int[num]);
    if (maintain_suit_groups) {
      suit_groups_.reset(new int[num]);
    }
    
    Card canon_cards[2];
    for (int hi = 1; hi <= max_card; ++hi) {
      if (InCards(hi, previous, num_previous)) continue;
      for (int lo = 0; lo < hi; ++lo) {
	if (InCards(lo, previous, num_previous)) continue;
	cards_[index * 2] = hi;
	cards_[index * 2 + 1] = lo;
	int num_mappings = NumMappings(&cards_[index * 2], n,
						previous_suit_groups);
	num_variants_[index] = num_mappings;
	short encoding;
	if (num_mappings == 0) {
	  ToCanon(&cards_[index * 2], n, previous_suit_groups, canon_cards);
	  encoding = canon_cards[0] * (max_card + 1) + canon_cards[1];
	} else {
	  encoding = hi * (max_card + 1) + lo;
	  ++num_canon_;
	}
	canon_[index] = encoding;
	if (maintain_suit_groups) {
	  UpdateSuitGroups(&cards_[index * 2], n_, previous_suit_groups,
			   &suit_groups_[index]);
	}
	++index;
      }
    }
    if (index != num) {
      fprintf(stderr, "Index too large n 2 np %u: %u num %u\n",
	      num_previous, index, num);
      exit(-1);
    }
  } else if (n == 3) {
    int num =
      num_remaining * (num_remaining - 1) * (num_remaining - 2) / 6;
    cards_.reset(new Card[3 * num]);
    num_variants_.reset(new unsigned char[num]);
    canon_.reset(new int[num]);
    if (maintain_suit_groups) {
      suit_groups_.reset(new int[num]);
    }
    Card canon_cards[3];
    for (int hi = 2; hi <= max_card; ++hi) {
      if (InCards(hi, previous, num_previous)) continue;
      for (int mid = 1; mid < hi; ++mid) {
	if (InCards(mid, previous, num_previous)) continue;
	for (int lo = 0; lo < mid; ++lo) {
	  if (InCards(lo, previous, num_previous)) continue;
	  cards_[index * 3] = hi;
	  cards_[index * 3 + 1] = mid;
	  cards_[index * 3 + 2] = lo;
	  int num_mappings = NumMappings(&cards_[index * 3], n,
						  previous_suit_groups);
	  num_variants_[index] = num_mappings;
	  int encoding;
	  if (num_mappings == 0) {
	    ToCanon(&cards_[index * 3], n, previous_suit_groups, canon_cards);
	    encoding = canon_cards[0] * (max_card + 1) * (max_card + 1) +
	      canon_cards[1] * (max_card + 1) + canon_cards[2];
	  } else {
	    encoding = hi * (max_card + 1) * (max_card + 1) +
	      mid * (max_card + 1) + lo;
	    ++num_canon_;
	  }
	  canon_[index] = encoding;
	  if (maintain_suit_groups) {
	    UpdateSuitGroups(&cards_[index * 3], n_, previous_suit_groups,
			     &suit_groups_[index]);
	  }
	  ++index;
	}
      }
    }
    if (index != num) {
      fprintf(stderr, "Index too large n 3: %u num %u\n", index, num);
      exit(-1);
    }
  } else {
    fprintf(stderr, "CanonicalCards: n %u not supported\n", n);
    exit(-1);
  }
  num_raw_ = index;
}

CanonicalCards::~CanonicalCards(void) {
}

// This version does not resort the cards
// Returns true if a change was made
bool CanonicalCards::ToCanon2(const Card *cards, int num_cards, int suit_groups,
			      Card *canon_cards) {
  int num_suits = Game::NumSuits();
  bool change_made = false;

  if (num_cards <= 0) return false;

  int *rank_used = new int[num_suits];

  int nsg = suit_groups;

  for (int i = 0; i != num_suits; ++i) rank_used[i] = 0;

  if (canon_cards != cards) {
    for (int i = 0; i < num_cards; ++i) {
      canon_cards[i] = cards[i];
    }
  }

  for (int i = 0; i < num_cards; ++i) {
    /* Get the current suit of the card */
    int old_suit = Suit(canon_cards[i]);
    /* Get the new suit */
    int new_suit = ((char *)&nsg)[old_suit];

    int rank = Rank(canon_cards[i]);
    rank_used[new_suit] |= 1 << rank;
   
    if (new_suit != old_suit) {
      canon_cards[i] = MakeCard(rank, new_suit);
      change_made = true;

      /* Change every other card of the same suit */
      for (int j = i + 1; j < num_cards; ++j ) {
	if (Suit(canon_cards[j]) == old_suit) {
	  rank = Rank(canon_cards[j]);
	  canon_cards[j] = MakeCard(rank, new_suit);
	} else if (Suit(canon_cards[j]) == new_suit) {
	  rank = Rank(canon_cards[j]);
	  canon_cards[j] = MakeCard(rank, old_suit);
	}
      }
    }

    /* Update suit_groups */
    for (int m = 0; m < num_suits; ++m) {
      int n;
      for (n = 0; n < m; ++n) {
	if (((unsigned char *)&suit_groups)[n] ==
	    ((unsigned char *)&suit_groups)[m] &&
	    rank_used[n] == rank_used[m]) {
	  // Were the suits in the same group before, and they
	  // now have the same cards again?  If so, recombine them.
	  break;
	} else if (((unsigned char *)&nsg)[n] ==
		   ((unsigned char *)&nsg)[m] &&
		   rank_used[n] == rank_used[m]) {
	  // Have we found a new suit that is isomorphic, given the 
	  // cards we've seen so far?
	  break;
	}
      }
      ((unsigned char *)&nsg)[m] = n;
    }
  }

  delete [] rank_used;

  return change_made;
}

void CanonicalCards::ToCanon(const Card *cards, int num_cards, int suit_groups, Card *canon_cards) {
  bool change_made = ToCanon2(cards, num_cards, suit_groups, canon_cards);
  if (change_made) {
    if (num_cards == 1) {
    } else if (num_cards == 2) {
      if (canon_cards[0] < canon_cards[1]) {
	Card temp = canon_cards[0];
	canon_cards[0] = canon_cards[1];
	canon_cards[1] = temp;
      }
    } else if (num_cards == 3) {
      Card c0 = canon_cards[0];
      Card c1 = canon_cards[1];
      Card c2 = canon_cards[2];
      if (c0 >= c1 && c0 >= c2) {
	if (c1 >= c2) {
	  // Don't need to do anything
	} else {
	  canon_cards[1] = c2;
	  canon_cards[2] = c1;
	}
      } else if (c1 >= c0 && c1 >= c2) {
	if (c0 >= c2) {
	  canon_cards[0] = c1;
	  canon_cards[1] = c0;
	} else {
	  canon_cards[0] = c1;
	  canon_cards[1] = c2;
	  canon_cards[2] = c0;
	}
      } else {
	if (c0 >= c1) {
	  canon_cards[0] = c2;
	  canon_cards[1] = c0;
	  canon_cards[2] = c1;
	} else {
	  canon_cards[0] = c2;
	  canon_cards[2] = c0;
	}
      }
    } else {
      fprintf(stderr, "ToCanon() only supports at most three cards currently: "
	      "num_cards %u\n", num_cards);
      exit(-1);
    }
    // qsort(canon_cards, num_cards, sizeof(cards[0]), compareCardByIdx);
  }

}

struct Hand {
  int hv;
  int index;
};

struct HandLowerCompare {
  bool operator()(const Hand &h1, const Hand &h2) {
    if (h1.hv < h2.hv) {
      return true;
    } else if (h1.hv > h2.hv) {
      return false;
    } else if (h1.index < h2.index) {
      return true;
    } else {
      return false;
    }
  }
};

static HandLowerCompare g_hand_lower_compare;

static int g_ui_larger_compare(const void *p1, const void *p2) {
  Card ui1 = *(Card *)p1;
  Card ui2 = *(Card *)p2;
  if (ui1 > ui2)      return -1;
  else if (ui1 < ui2) return 1;
  else                return 0;
}

// board is not sorted from high to low
// Do not assume we can modify board
// Assume board is a max street board
// We don't update suit groups (unsafe, no?)
void CanonicalCards::SortByHandStrength(const Card *board) {
  int num_board_cards = Game::NumBoardCards(Game::MaxStreet());
  unique_ptr<Card []> sorted_board(new Card[num_board_cards]);
  for (int i = 0; i < num_board_cards; ++i) sorted_board[i] = board[i];
  qsort((void *)sorted_board.get(), (size_t)num_board_cards, sizeof(Card),
	g_ui_larger_compare);
#if 0
  for (int j = 0; j < num_board_cards; ++j) {
    for (int k = j + 1; k < num_board_cards; ++k) {
      if (sorted_board.get()[j] == sorted_board.get()[k]) {
	fprintf(stderr, "Illegal board\n");
	for (int l = 0; l < num_board_cards; ++l) {
	  fprintf(stderr, "%i ", (int)sorted_board.get()[l]);
	}
	fprintf(stderr, "\n");
	exit(-1);
      }
    }
  }
#endif
  vector<Hand> hands(num_raw_);
  Hand h;
  int hole_cards[2];
  for (int i = 0; i < num_raw_; ++i) {
    Card *cards = &cards_[i * n_];
    hole_cards[0] = cards[0];
    hole_cards[1] = cards[1];
    // We know hole cards are sorted
#if 0
    for (int j = 0; j < num_board_cards; ++j) {
      fprintf(stderr, "%i ", (int)sorted_board.get()[j]);
    }
    fprintf(stderr, "/ %i %i\n", hole_cards[0], hole_cards[1]);
#endif
    h.hv = HandValueTree::Val(sorted_board.get(), hole_cards);
    h.index = i;
    hands[i] = h;
  }
  std::sort(hands.begin(), hands.end(), g_hand_lower_compare);

  hand_values_.reset(new int[num_raw_]);
  Card *new_cards = new Card[num_raw_ * n_];
  unsigned char *new_num_variants = new unsigned char[num_raw_];
  int *new_canon = new int[num_raw_];

  for (int i = 0; i < num_raw_; ++i) {
    int hv = hands[i].hv;
    int index = hands[i].index;
    Card hi = cards_[index * n_];
    Card lo = cards_[index * n_ + 1];
    new_cards[i * n_] = hi;
    new_cards[i * n_ + 1] = lo;
    new_num_variants[i] = num_variants_[index];
    new_canon[i] = canon_[index];
    hand_values_[i] = hv;
  }

  cards_.reset(new_cards);
  num_variants_.reset(new_num_variants);
  canon_.reset(new_canon);
}

int NChooseK(int n, int k) {
  int numer = 1, denom = 1;

  for (int i = 0; i < k; ++i) {
    numer *= n - i;
    denom *= k - i;
  }
  return numer / denom;
}

// Code liberally borrowed from CFR+ implementation from University of
// Alberta.
// Assumes cards are sorted by rank
// Currently only supports adding N cards to nothing.
int CanonicalCards::NumMappings(const Card *cards, int num_cards, int old_suit_groups) {
  int group_used[4], suit_used[4], group_size[4];
  int num_suits = Game::NumSuits();
  // Start with all suits in group 0
  int new_suit_groups = old_suit_groups;
  for (int s = 0; s < num_suits; ++s) group_size[s] = 0;
  for (int s = 0; s < num_suits; ++s) {
    ++group_size[((unsigned char *)&new_suit_groups)[s]];
  }
  int start = 0;
  int num_mappings = 1;
  // group_size starts out with the number of cards in each group,
  // calculated over all of the input cards.
  while (start < num_cards) {
    // Each time through this loop we consume a sequence of cards with the
    // same rank.  group_used[g] is the number of cards in the sequence
    // with group g.  suit_used[s] is the number of cards in the sequence
    // with suit s.
    for (int s = 0; s < num_suits; ++s) {
      group_used[s] = 0;
      suit_used[s] = 0;
    }
    int start_rank = Rank(cards[start]);
    int end = start;
    do {
      Card c = cards[end];
      int s = Suit(c);
      ++suit_used[s];
      ++group_used[((unsigned char *)&new_suit_groups)[s]];
      ++end;
    } while (end < num_cards && Rank(cards[end]) == start_rank);

    for (int g = 0; g < num_suits; ++g) {
      int gu = group_used[g];
      if (gu == 0) continue;
      num_mappings *= NChooseK(group_size[g], gu);

      /* start updating group_size array */
      int t = group_size[g];
      group_size[g] = group_used[g];

      /* check that the smallest valued cards are chosen for each group */
      int s = g;
      while (1) {
	if (! suit_used[s]) {
	  // cards don't use the lowest valued suits - not canonical
	  return 0;
	}
	if (! --group_used[g]) {
	  break;
	}
	while (((unsigned char *)&new_suit_groups)[++s] != g);
      }

      /* finish updating the grouping information */
      for (++s; s != num_suits; ++s) {
	if (((unsigned char *)&new_suit_groups)[s] == g ) {
	  ((unsigned char *)&new_suit_groups)[s] = s;
	  group_size[s] = t - group_size[g];
	  t = s;
	  for (++s; s != num_suits; ++s) {
	    if (((unsigned char *)&new_suit_groups)[s] == g) {
	      ((unsigned char *)&new_suit_groups)[s] = t;
	    }
	  }
	  break;
	}
      }
    }

    start = end;
  }
  return num_mappings;
}

void UpdateSuitGroups(const Card *cards, int num_cards, int old_suit_groups, int *new_suit_groups) {
  int seen_ranks[4];
  int num_suits = Game::NumSuits();
  // Default of 0
  *new_suit_groups = 0;
  for (int s = 0; s < num_suits; ++s) seen_ranks[s] = 0;
  for (int i = 0; i < num_cards; ++i) {
    Card c = cards[i];
    char rank = Rank(c);
    int suit = Suit(c);
    seen_ranks[suit] |= (1 << rank);
  }

  // Suit group of the first suit (clubs) is always zero
  ((unsigned char *)new_suit_groups)[0] = 0;
  for (int s2 = 1; s2 < num_suits; ++s2) {
    int sr2 = seen_ranks[s2];
    int s1;
    for (s1 = 0; s1 < s2; ++s1) {
      if (((unsigned char *)&old_suit_groups)[s1] ==
	  ((unsigned char *)&old_suit_groups)[s2] &&
	  seen_ranks[s1] == sr2) {
	break;
      }
    }
    ((unsigned char *)new_suit_groups)[s2] = s1;
  }
}

#if 0
// Older inner loop above
if ((old_suit_groups == NULL ||
     old_suit_groups[s1] == old_suit_groups[s2]) &&
    seen_ranks[s1] == sr2) {
  break;
 }
#endif
