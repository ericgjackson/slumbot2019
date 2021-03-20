// Generate hands and map them to hand indices.
// Hand indices are generated using a semi-dense encoding.

#include <stdio.h>
#include <stdlib.h>

#include <memory>
#include <string>
#include <unordered_set>

#include "board_tree.h"
#include "canonical.h"
#include "cards.h"
#include "game.h"
#include "game_params.h"
#include "hand_tree.h"
#include "params.h"

using std::string;
using std::unique_ptr;
using std::unordered_set;

static void All(unsigned int bd, int st, unordered_set<unsigned int> *s) {
  const Card *canon_board = BoardTree::Board(st, bd);
  int num_board_cards = Game::NumBoardCards(st);
  unsigned int num_hole_card_pairs = Game::NumHoleCardPairs(st);
  Card cards[7];
  for (int i = 0; i < num_board_cards; ++i) cards[i+2] = canon_board[i];
  for (Card hi = 1; hi < 52; ++hi) {
    if (InCards(hi, canon_board, num_board_cards)) continue;
    cards[0] = hi;
    for (Card lo = 0; lo < hi; ++lo) {
      if (InCards(lo, canon_board, num_board_cards)) continue;
      cards[1] = lo;
      unsigned int hcp = HCPIndex(st, cards);
      unsigned int h = bd * num_hole_card_pairs + hcp;
      s->insert(h);
#if 0
      printf("%u: ", h);
      OutputNCards(cards, num_board_cards + 2);
      printf("\n");
      fflush(stdout);
#endif
    }
  }
}

static void Canonical(const Card *raw_board, int st, unordered_set<unsigned int> *s) {
  // Iterates through only canonical hands.  For the flop, the hand indices generated
  // still range between 0 and 2063879, but you will observe only 1286792 distinct values.
  int num_board_cards = Game::NumBoardCards(st);
  unsigned int num_hole_card_pairs = Game::NumHoleCardPairs(st);
  Card raw_hole_cards[2], canon_board[5], canon_hole_cards[2];
  Card canon_cards[7];
  for (Card hi = 1; hi < 52; ++hi) {
    if (InCards(hi, raw_board, num_board_cards)) continue;
    raw_hole_cards[0] = hi;
    for (Card lo = 0; lo < hi; ++lo) {
      if (InCards(lo, raw_board, num_board_cards)) continue;
      raw_hole_cards[1] = lo;
      CanonicalizeCards(raw_board, raw_hole_cards, st, canon_board, canon_hole_cards);
      unsigned int bd = BoardTree::LookupBoard(canon_board, st);
      for (int i = 0; i < num_board_cards; ++i) {
	canon_cards[i+2] = canon_board[i];
      }
      for (int i = 0; i < 2; ++i) {
	canon_cards[i] = canon_hole_cards[i];
      }
      unsigned int hcp = HCPIndex(st, canon_cards);
      unsigned int h = bd * num_hole_card_pairs + hcp;
      s->insert(h);
#if 0
      printf("%u: ", h);
      OutputNCards(canon_cards, num_board_cards + 2);
      printf(" (");
      OutputTwoCards(raw_hole_cards);
      printf(" / ");
      OutputNCards(raw_board, num_board_cards);
      printf(")");
      printf("\n");
      fflush(stdout);
#endif
    }
  }
}

// This will take quite a while and use a lot of memory.
static void CanonicalRiver(void) {
  unordered_set<unsigned int> s;
  Card board[5];
  for (Card f0 = 2; f0 < 52; ++f0) {
    fprintf(stderr, "f0 %i\n", f0);
    board[0] = f0;
    for (Card f1 = 1; f1 < f0; ++f1) {
      board[1] = f1;
      for (Card f2 = 0; f2 < f1; ++f2) {
	board[2] = f2;
	for (Card t = 0; t < 52; ++t) {
	  if (t == f0 || t == f1 || t == f2) continue;
	  board[3] = t;
	  for (Card r = 0; r < 52; ++r) {
	    if (r == f0 || r == f1 || r == f2 || r == t) continue;
	    board[4] = r;
	    Canonical(board, 3, &s);
	  }
	}
      }
    }
  }
}

static void CanonicalFlop(void) {
  // Iterate through raw boards, not just canonical boards
  unordered_set<unsigned int> s;
  Card board[3];
  for (Card f0 = 2; f0 < 52; ++f0) {
    board[0] = f0;
    for (Card f1 = 1; f1 < f0; ++f1) {
      board[1] = f1;
      for (Card f2 = 0; f2 < f1; ++f2) {
	board[2] = f2;
	Canonical(board, 1, &s);
      }
    }
  }
  fprintf(stderr, "%u unique hands\n", (unsigned int)s.size());
}

static void AllFlop(void) {
  // Iterate through canonical boards
  unordered_set<unsigned int> s;
  unsigned int num_boards = BoardTree::NumBoards(1);
  for (unsigned int bd = 0; bd < num_boards; ++bd) {
    All(bd, 1, &s);
  }
  fprintf(stderr, "%u unique hands\n", (unsigned int)s.size());
}

static void Usage(const char *prog_name) {
  fprintf(stderr, "USAGE: %s <game params> <street> [canonical|all]\n", prog_name);
  exit(-1);
}

int main(int argc, char *argv[]) {
  if (argc != 4) Usage(argv[0]);
  unique_ptr<Params> game_params = CreateGameParams();
  game_params->ReadFromFile(argv[1]);
  int st;
  if (sscanf(argv[2], "%i", &st) != 1) Usage(argv[0]);
  bool canonical;
  string a = argv[3];
  if (a == "canonical") canonical = true;
  else if (a == "all")  canonical = false;
  else                  Usage(argv[0]);
  Game::Initialize(*game_params);
  BoardTree::Create();
  BoardTree::CreateLookup();
  if (st == 1) {
    if (canonical) {
      CanonicalFlop();
    } else {
      AllFlop();
    }
  } else if (st == 3) {
    CanonicalRiver();
  }
}
