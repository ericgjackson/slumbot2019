#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

#include "cards.h"
#include "hand_evaluator.h"

using std::string;

HandEvaluator *HandEvaluator::Create(const string &name) {
  if (! strncmp(name.c_str(), "leduc", 5)) {
    return new LeducHandEvaluator();
  } else {
    // Assume some form of Holdem if not leduc
    return new HoldemHandEvaluator();
  }
}

HandEvaluator::HandEvaluator(void) {
}

HandEvaluator::~HandEvaluator(void) {
}

LeducHandEvaluator::LeducHandEvaluator(void) : HandEvaluator() {
}

LeducHandEvaluator::~LeducHandEvaluator(void) {
}

int LeducHandEvaluator::Evaluate(Card *cards, int num_cards) {
  int r0 = Rank(cards[0]);
  int r1 = Rank(cards[1]);
  int hr, lr;
  if (r0 > r1) {hr = r0; lr = r1;}
  else         {hr = r1; lr = r0;}
  if (hr == 2) {
    if (lr == 2)      return 5;
    else if (lr == 1) return 2;
    else              return 1;
  } else if (hr == 1) {
    if (lr == 1)      return 4;
    else              return 0;
  } else {
                      return 3;
  }
}

HoldemHandEvaluator::HoldemHandEvaluator(void) : HandEvaluator() {
  ranks_ = new int[7];
  suits_ = new int[7];
  rank_counts_ = new int[13];
  suit_counts_ = new int[4];
}

HoldemHandEvaluator::~HoldemHandEvaluator(void) {
  delete [] suits_;
  delete [] ranks_;
  delete [] rank_counts_;
  delete [] suit_counts_;
}

// Return values between 0 and 90
int HoldemHandEvaluator::EvaluateTwo(Card *cards) {
  int r0 = Rank(cards[0]);
  int r1 = Rank(cards[1]);
  if (r0 == r1) {
    return 78 + r0;
  }
  int hr, lr;
  if (r0 > r1) {hr = r0; lr = r1;}
  else         {hr = r1; lr = r0;}
  if (hr == 1) {
    return 0;       // 0
  } else if (hr == 2) {
    return 1 + lr;  // 1-2
  } else if (hr == 3) {
    return 3 + lr;  // 3-5
  } else if (hr == 4) {
    return 6 + lr;  // 6-9
  } else if (hr == 5) {
    return 10 + lr; // 10-14
  } else if (hr == 6) {
    return 15 + lr; // 15-20
  } else if (hr == 7) {
    return 21 + lr; // 21-27
  } else if (hr == 8) {
    return 28 + lr; // 28-35
  } else if (hr == 9) {
    return 36 + lr; // 36-44
  } else if (hr == 10) {
    return 45 + lr; // 45-54
  } else if (hr == 11) {
    return 55 + lr; // 55-65
  } else { // hr == 12
    return 66 + lr; // 66-77
  }
}

// Returns values between 0 and 2378 (inclusive)
// 13 trips - 2366-2378
// 169 (13 * 13) pairs (some values not possible) - 2197 - 2365
// 2197 no-pairs (some values not possible) - 0...2196
int HoldemHandEvaluator::EvaluateThree(Card *cards) {
  unsigned int r0 = Rank(cards[0]);
  unsigned int r1 = Rank(cards[1]);
  unsigned int r2 = Rank(cards[2]);
  if (r0 == r1 && r1 == r2) {
    return 2366 + r0;
  } else if (r0 == r1 || r0 == r2 || r1 == r2) {
    unsigned pr_rank, kicker;
    if (r0 == r1) {
      pr_rank = r0;
      kicker = r2;
    } else if (r0 == r2) {
      pr_rank = r0;
      kicker = r1;
    } else {
      pr_rank = r1;
      kicker = r0;
    }
    return 2197 + pr_rank * 13 + kicker;
  } else {
    unsigned int hr, mr, lr;
    if (r0 > r1) {
      if (r1 > r2) {
	hr = r0; mr = r1; lr = r2;
      } else if (r0 > r2) {
	hr = r0; mr = r2; lr = r1;
      } else {
	hr = r2; mr = r0; lr = r1;
      }
    } else if (r0 > r2) {
      hr = r1; mr = r0; lr = r2;
    } else if (r2 > r1) {
      hr = r2; mr = r1; lr = r0;
    } else {
      hr = r1; mr = r2; lr = r0;
    }
    return hr * 169 + mr * 13 + lr;
  }
}

// Return values between 0 and 31108
// 31096...31108: quads
// 30927...31095: three-of-a-kind
// 30758...30926: two-pair
// 28561...30757: pair
// 0...28560:     no-pair
// Next 715 (?) for no-pair
int HoldemHandEvaluator::EvaluateFour(Card *cards) {
  for (int r = 0; r <= 12; ++r) rank_counts_[r] = 0;
  for (int i = 0; i < 4; ++i) {
    ++rank_counts_[Rank(cards[i])];
  }
  int pair_rank1 = -1, pair_rank2 = -1;
  for (int r = 12; r >= 0; --r) {
    if (rank_counts_[r] == 4) {
      return kH4Quads + r;
    } else if (rank_counts_[r] == 3) {
      int kicker = -1;
      for (int r = 12; r >= 0; --r) {
	if (rank_counts_[r] == 1) {
	  kicker = r;
	  break;
	}
      }
      return kH4ThreeOfAKind + 13 * r + kicker;
    } else if (rank_counts_[r] == 2) {
      if (pair_rank1 == -1) {
	pair_rank1 = r;
      } else {
	pair_rank2 = r;
	break;
      }
    }
  }
  if (pair_rank2 >= 0) {
    return pair_rank1 * 13 + pair_rank2 + kH4TwoPair;
  }
  if (pair_rank1 >= 0) {
    int kicker1 = -1, kicker2 = -1;
    for (int r = 12; r >= 0; --r) {
      if (rank_counts_[r] == 1) {
	if (kicker1 == -1) {
	  kicker1 = r;
	} else {
	  kicker2 = r;
	  break;
	}
      }
    }
    return pair_rank1 * 169 + kicker1 * 13 + kicker2 + kH4Pair;
  }
  int kicker1 = -1, kicker2 = -1, kicker3 = -1, kicker4 = -1;
  for (int r = 12; r >= 0; --r) {
    if (rank_counts_[r] == 1) {
      if (kicker1 == -1)      kicker1 = r;
      else if (kicker2 == -1) kicker2 = r;
      else if (kicker3 == -1) kicker3 = r;
      else                    kicker4 = r;
    }
  }
  return kicker1 * 2197 + kicker2 * 169 + kicker3 * 13 + kicker4;
}

int HoldemHandEvaluator::Evaluate(Card *cards, int num_cards) {
  if (num_cards == 2) {
    return EvaluateTwo(cards);
  } else if (num_cards == 3) {
    return EvaluateThree(cards);
  } else if (num_cards == 4) {
    return EvaluateFour(cards);
  }
  for (int r = 0; r <= 12; ++r) rank_counts_[r] = 0;
  for (int s = 0; s < 4; ++s)   suit_counts_[s] = 0;
  for (int i = 0; i < num_cards; ++i) {
    Card c = cards[i];
    int r = Rank(c);
    ranks_[i] = r;
    ++rank_counts_[r];
    int s = Suit(c);
    suits_[i] = s;
    ++suit_counts_[s];
  }
  int flush_suit = -1;
  for (int s = 0; s < 4; ++s) {
    if (suit_counts_[s] >= 5) {
      flush_suit = s;
      break;
    }
  }
  // Need to handle straights with ace as low
  int r = 12;
  int straight_rank = -1;
  while (true) {
    // See if there are 5 ranks (r, r-1, r-2, etc.) such that there is at
    // least one card in each rank.  In other words, there is an r-high
    // straight.
    int r1 = r;
    int end = r - 4;
    while (r1 >= end &&
	   ((r1 > -1 && rank_counts_[r1] > 0) ||
	    (r1 == -1 && rank_counts_[12] > 0))) {
      --r1;
    }
    if (r1 == end - 1) {
      // We found a straight
      if (flush_suit >= 0) {
	// There is a flush on the board
	if (straight_rank == -1) straight_rank = r;
	// Need to check for straight flush.  Count how many cards between
	// end and r are in the flush suit.
	int num = 0;
	for (int i = 0; i < num_cards; ++i) {
	  if (suits_[i] == flush_suit &&
	      ((ranks_[i] >= end && ranks_[i] <= r) ||
	       (end == -1 && ranks_[i] == 12))) {
	    // This assumes we have no duplicate cards in input
	    ++num;
	  }
	}
	if (num == 5) {
	  return kStraightFlush + r;
	}
	// Can't break yet - there could be a straight flush at a lower rank
	// Can only decrement r by 1.  (Suppose cards are:
	// 4c5c6c7c8c9s.)
	--r;
	if (r < 3) break;
      } else {
	straight_rank = r;
	break;
      }
    } else {
      // If we get here, there was no straight ending at r.  We know there
      // are no cards with rank r1.  Therefore r can jump to r1 - 1.
      r = r1 - 1;
      if (r < 3) break;
    }
  }
  int three_rank = -1;
  int pair_rank = -1;
  int pair2_rank = -1;
  for (int r = 12; r >= 0; --r) {
    int ct = rank_counts_[r];
    if (ct == 4) {
      int hr = -1;
      for (int i = 0; i < num_cards; ++i) {
	int r1 = ranks_[i];
	if (r1 != r && r1 > hr) hr = r1;
      }
      return kQuads + r * 13 + hr;
    } else if (ct == 3) {
      if (three_rank == -1) {
	three_rank = r;
      } else if (pair_rank == -1) {
	pair_rank = r;
      }
    } else if (ct == 2) {
      if (pair_rank == -1) {
	pair_rank = r;
      } else if (pair2_rank == -1) {
	pair2_rank = r;
      }
    }
  }
  if (three_rank >= 0 && pair_rank >= 0) {
    return kFullHouse + three_rank * 13 + pair_rank;
  }
  if (flush_suit >= 0) {
    int hr1 = -1, hr2 = -1, hr3 = -1, hr4 = -1, hr5 = -1;
    for (int i = 0; i < num_cards; ++i) {
      if (suits_[i] == flush_suit) {
	int r = ranks_[i];
	if (r > hr1) {
	  hr5 = hr4; hr4 = hr3; hr3 = hr2; hr2 = hr1; hr1 = r;
	} else if (r > hr2) {
	  hr5 = hr4; hr4 = hr3; hr3 = hr2; hr2 = r;
	} else if (r > hr3) {
	  hr5 = hr4; hr4 = hr3; hr3 = r;
	} else if (r > hr4) {
	  hr5 = hr4; hr4 = r;
	} else if (r > hr5) {
	  hr5 = r;
	}
      }
    }
    return kFlush + hr1 * 28561 + hr2 * 2197 + hr3 * 169 + hr4 * 13 + hr5;
  }
  if (straight_rank >= 0) {
    return kStraight + straight_rank;
  }
  if (three_rank >= 0) {
    int hr1 = -1, hr2 = -1;
    for (int i = 0; i < num_cards; ++i) {
      int r = ranks_[i];
      if (r != three_rank) {
	if (r > hr1) {
	  hr2 = hr1; hr1 = r;
	} else if (r > hr2) {
	  hr2 = r;
	}
      }
    }
    if (num_cards == 3) {
      // No kicker
      return kThreeOfAKind + three_rank * 169;
    } else if (num_cards == 4) {
      // Only one kicker
      return kThreeOfAKind + three_rank * 169 + hr1 * 13;
    } else {
      // Two kickers
      return kThreeOfAKind + three_rank * 169 + hr1 * 13 + hr2;
    }
  }
  if (pair2_rank >= 0) {
    int hr1 = -1;
    for (int i = 0; i < num_cards; ++i) {
      int r = ranks_[i];
      if (r != pair_rank && r != pair2_rank && r > hr1) hr1 = r;
    }
    if (num_cards < 5) {
      // No kicker
      return kTwoPair + pair_rank * 169 + pair2_rank * 13;
    } else {
      // Encode two pair ranks plus kicker
      return kTwoPair + pair_rank * 169 + pair2_rank * 13 + hr1;
    }
  }
  if (pair_rank >= 0) {
    int hr1 = -1, hr2 = -1, hr3 = -1;
    for (int i = 0; i < num_cards; ++i) {
      int r = ranks_[i];
      if (r != pair_rank) {
	if (r > hr1) {
	  hr3 = hr2; hr2 = hr1; hr1 = r;
	} else if (r > hr2) {
	  hr3 = hr2; hr2 = r;
	} else if (r > hr3) {
	  hr3 = r;
	}
      }
    }
    if (num_cards == 3) {
      // One kicker
      return kPair + pair_rank * 2197 + hr1 * 169;
    } else if (num_cards == 4) {
      // Two kickers
      return kPair + pair_rank * 2197 + hr1 * 169 + hr2 * 13;
    } else {
      // Three kickers
      return kPair + pair_rank * 2197 + hr1 * 169 + hr2 * 13 + hr3;
    }
  }

  int hr1 = -1, hr2 = -1, hr3 = -1, hr4 = -1, hr5 = -1;
  for (int i = 0; i < num_cards; ++i) {
    int r = ranks_[i];
    if (r > hr1) {
      hr5 = hr4; hr4 = hr3; hr3 = hr2; hr2 = hr1; hr1 = r;
    } else if (r > hr2) {
      hr5 = hr4; hr4 = hr3; hr3 = hr2; hr2 = r;
    } else if (r > hr3) {
      hr5 = hr4; hr4 = hr3; hr3 = r;
    } else if (r > hr4) {
      hr5 = hr4; hr4 = r;
    } else if (r > hr5) {
      hr5 = r;
    }
  }
  if (num_cards == 3) {
    // Encode top three ranks
    return kNoPair + hr1 * 28561 + hr2 * 2197 + hr3 * 169;
  } else if (num_cards == 4) {
    // Encode top four ranks
    return kNoPair + hr1 * 28561 + hr2 * 2197 + hr3 * 169 + hr4 * 13;
  } else {
    // Encode top five ranks
    return kNoPair + hr1 * 28561 + hr2 * 2197 + hr3 * 169 + hr4 * 13 + hr5;
  }
}
