#include <stdio.h>
#include <stdlib.h>

#include <memory>

#include "board_tree.h"
#include "canonical.h"
#include "cards.h"
#include "game.h"
#include "game_params.h"
#include "hand_tree.h"
#include "params.h"

using std::unique_ptr;

static void ProcessBoard(const Card *raw_board) {
  int st = 3; // River
  unsigned int num_hole_card_pairs = Game::NumHoleCardPairs(st);
  Card raw_hole_cards[2], canon_board[5], canon_hole_cards[2];
  Card canon_cards[7];
  for (Card hi = 1; hi < 52; ++hi) {
    if (InCards(hi, raw_board, 5)) continue;
    raw_hole_cards[0] = hi;
    for (Card lo = 0; lo < hi; ++lo) {
      if (InCards(lo, raw_board, 5)) continue;
      raw_hole_cards[1] = lo;
      CanonicalizeCards(raw_board, raw_hole_cards, st, canon_board, canon_hole_cards);
      unsigned int bd = BoardTree::LookupBoard(canon_board, st);
      for (int i = 0; i < 5; ++i) {
	canon_cards[i+2] = canon_board[i];
      }
      for (int i = 0; i < 2; ++i) {
	canon_cards[i] = canon_hole_cards[i];
      }
      int hcp = HCPIndex(st, canon_cards);
      unsigned int h = bd * num_hole_card_pairs + hcp;
      printf("%u: ", h);
      OutputNCards(canon_cards, 7);
      printf(" (");
      OutputTwoCards(raw_hole_cards);
      printf(" / ");
      OutputFiveCards(raw_board);
      printf(")");
      printf("\n");
      fflush(stdout);
    }
  }
}

static void Usage(const char *prog_name) {
  fprintf(stderr, "USAGE: %s <game params>\n", prog_name);
  exit(-1);
}

int main(int argc, char *argv[]) {
  if (argc != 2) Usage(argv[0]);
  // Files::Init();
  unique_ptr<Params> game_params = CreateGameParams();
  game_params->ReadFromFile(argv[1]);
  Game::Initialize(*game_params);
  BoardTree::Create();
  BoardTree::CreateLookup();
  Card board[5];
  for (Card f0 = 2; f0 < 52; ++f0) {
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
	    ProcessBoard(board);
	  }
	}
      }
    }
  }
}
