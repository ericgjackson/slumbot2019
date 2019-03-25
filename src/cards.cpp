#include <stdio.h>
#include <stdlib.h>

#include <string>

#include "cards.h"

using std::string;

void OutputRank(int rank) {
  if (rank < 8) {
    printf("%i", (int)rank + 2);
  } else if (rank == 8) {
    printf("T");
  } else if (rank == 9) {
    printf("J");
  } else if (rank == 10) {
    printf("Q");
  } else if (rank == 11) {
    printf("K");
  } else if (rank == 12) {
    printf("A");
  } else {
    fprintf(stderr, "Illegal rank %i\n", rank);
    exit(-1);
  }
}

void OutputCard(Card card) {
  int rank = Rank(card);
  int suit = Suit(card);

  OutputRank(rank);
  switch (suit) {
  case 0:
    printf("c"); break;
  case 1:
    printf("d"); break;
  case 2:
    printf("h"); break;
  case 3:
    printf("s"); break;
  default:
    fprintf(stderr, "Illegal suit\n");
    exit(-1);
  }
}

void CardName(Card c, string *name) {
  *name = "";
  int rank = Rank(c);
  int suit = Suit(c);

  if (rank < 8) {
    *name += rank + 50;
  } else if (rank == 8) {
    *name += "T";
  } else if (rank == 9) {
    *name += "J";
  } else if (rank == 10) {
    *name += "Q";
  } else if (rank == 11) {
    *name += "K";
  } else if (rank == 12) {
    *name += "A";
  }
  switch (suit) {
  case 0:
    *name += "c"; break;
  case 1:
    *name += "d"; break;
  case 2:
    *name += "h"; break;
  case 3:
    *name += "s"; break;
  default:
    fprintf(stderr, "Illegal suit\n");
    exit(-1);
  }
}

void OutputTwoCards(Card c1, Card c2) {
  OutputCard(c1);
  printf(" ");
  OutputCard(c2);
}

void OutputTwoCards(const Card *cards) {
  OutputTwoCards(cards[0], cards[1]);
}

void OutputThreeCards(Card c1, Card c2, Card c3) {
  OutputCard(c1);
  printf(" ");
  OutputCard(c2);
  printf(" ");
  OutputCard(c3);
}

void OutputThreeCards(const Card *cards) {
  OutputThreeCards(cards[0], cards[1], cards[2]);
}

void OutputFourCards(Card c1, Card c2, Card c3, Card c4) {
  OutputCard(c1);
  printf(" ");
  OutputCard(c2);
  printf(" ");
  OutputCard(c3);
  printf(" ");
  OutputCard(c4);
}

void OutputFourCards(const Card *cards) {
  OutputFourCards(cards[0], cards[1], cards[2], cards[3]);
}

void OutputFiveCards(Card c1, Card c2, Card c3, Card c4, Card c5) {
  OutputCard(c1);
  printf(" ");
  OutputCard(c2);
  printf(" ");
  OutputCard(c3);
  printf(" ");
  OutputCard(c4);
  printf(" ");
  OutputCard(c5);
}

void OutputFiveCards(const Card *cards) {
  OutputFiveCards(cards[0], cards[1], cards[2], cards[3], cards[4]);
}

void OutputSixCards(Card c1, Card c2, Card c3, Card c4, Card c5, Card c6) {
  OutputCard(c1);
  printf(" ");
  OutputCard(c2);
  printf(" ");
  OutputCard(c3);
  printf(" ");
  OutputCard(c4);
  printf(" ");
  OutputCard(c5);
  printf(" ");
  OutputCard(c6);
}

void OutputSixCards(const Card *cards) {
  OutputSixCards(cards[0], cards[1], cards[2], cards[3], cards[4], cards[5]);
}

void OutputSevenCards(Card c1, Card c2, Card c3, Card c4, Card c5,
		      Card c6, Card c7) {
  OutputCard(c1);
  printf(" ");
  OutputCard(c2);
  printf(" ");
  OutputCard(c3);
  printf(" ");
  OutputCard(c4);
  printf(" ");
  OutputCard(c5);
  printf(" ");
  OutputCard(c6);
  printf(" ");
  OutputCard(c7);
}

void OutputSevenCards(const Card *cards) {
  OutputSevenCards(cards[0], cards[1], cards[2], cards[3], cards[4], cards[5],
		   cards[6]);
}

void OutputNCards(const Card *cards, int n) {
  for (int i = 0; i < n; ++i) {
    if (i > 0) printf(" ");
    OutputCard(cards[i]);
  }
}

Card MakeCard(int rank, int suit) {
  return rank * Game::NumSuits() + suit;
}

Card ParseCard(const char *str) {
  char c = str[0];
  int rank;
  if (c >= '0' && c <= '9') {
    rank = (c - '0') - 2;
  } else if (c == 'A') {
    rank = 12;
  } else if (c == 'K') {
    rank = 11;
  } else if (c == 'Q') {
    rank = 10;
  } else if (c == 'J') {
    rank = 9;
  } else if (c == 'T') {
    rank = 8;
  } else {
    fprintf(stderr, "Couldn't parse card rank\n");
    fprintf(stderr, "Str %s\n", str);
    exit(-1);
  }
  c = str[1];
  if (c == 'c') {
    return MakeCard(rank, 0);
  } else if (c == 'd') {
    return MakeCard(rank, 1);
  } else if (c == 'h') {
    return MakeCard(rank, 2);
  } else if (c == 's') {
    return MakeCard(rank, 3);
  } else {
    fprintf(stderr, "Couldn't parse card suit\n");
    fprintf(stderr, "Str %s\n", str);
    exit(-1);
  }
}

void ParseTwoCards(const char *str, bool space_separated, Card *cards) {
  cards[0] = ParseCard(str);
  if (space_separated) {
    cards[1] = ParseCard(str + 3);
  } else {
    cards[1] = ParseCard(str + 2);
  }
}

void ParseThreeCards(const char *str, bool space_separated, Card *cards) {
  cards[0] = ParseCard(str);
  if (space_separated) {
    cards[1] = ParseCard(str + 3);
    cards[2] = ParseCard(str + 6);
  } else {
    cards[1] = ParseCard(str + 2);
    cards[2] = ParseCard(str + 4);
  }
}

void ParseFiveCards(const char *str, bool space_separated, Card *cards) {
  cards[0] = ParseCard(str);
  if (space_separated) {
    cards[1] = ParseCard(str + 3);
    cards[2] = ParseCard(str + 6);
    cards[3] = ParseCard(str + 8);
    cards[4] = ParseCard(str + 12);
  } else {
    cards[1] = ParseCard(str + 2);
    cards[2] = ParseCard(str + 4);
    cards[3] = ParseCard(str + 6);
    cards[4] = ParseCard(str + 8);
  }
}

bool InCards(Card c, const Card *cards, int num_cards) {
  for (int i = 0; i < num_cards; ++i) if (c == cards[i]) return true;
  return false;
}

int MaxSuit(Card *board, int num_board) {
  int max_suit = Suit(board[0]);
  for (int i = 1; i < num_board; ++i) {
    int s = Suit(board[i]);
    if (s > max_suit) max_suit = s;
  }
  return max_suit;
}

