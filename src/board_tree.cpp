// Stores all the possible canonical boards for a game.  For example the
// 1,755 canonical three-card flops for Holdem.  Supports a global index for
// each board and a method Board() to get the cards corresponding to a
// particular global index.
//
// Also supports a dense local indexing for any subtree of boards.  For
// example, all the turn and river boards that derive from the flop AcKcQc.
// This is useful when we want to solve subgames independently (e.g., in
// subgame solving and in cfrp.cpp).

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <memory>

#include "board_tree.h"
#include "canonical_cards.h"
#include "canonical.h"
#include "cards.h"
#include "constants.h"
#include "game.h"
#include "sorting.h"

using std::unique_ptr;

int BoardTree::max_street_ = 0;
unique_ptr<int []> BoardTree::num_boards_;
int **BoardTree::board_variants_ = nullptr;
int ***BoardTree::succ_board_begins_ = nullptr;
int ***BoardTree::succ_board_ends_ = nullptr;
Card **BoardTree::boards_ = nullptr;
int **BoardTree::suit_groups_ = nullptr;
int *BoardTree::bds_ = nullptr;
int **BoardTree::lookup_ = nullptr;
int **BoardTree::board_counts_ = nullptr;
int *BoardTree::pred_boards_ = nullptr;

int BoardTree::LocalIndex(int root_st, int root_bd, int st, int gbd) {
  if (st == root_st)     return 0;
  else if (root_st == 0) return gbd;
  else                   return gbd - succ_board_begins_[root_st][st][root_bd];
}

int BoardTree::GlobalIndex(int root_st, int root_bd, int st, int lbd) {
  if (st == root_st)     return root_bd;
  else if (root_st == 0) return lbd;
  else                   return lbd + succ_board_begins_[root_st][st][root_bd];
}

int BoardTree::NumLocalBoards(int root_st, int root_bd, int st) {
  if (st == root_st) return 1;
  else               return succ_board_ends_[root_st][st][root_bd] -
		       succ_board_begins_[root_st][st][root_bd];
}

void BoardTree::Count(int st, const Card *prev_board, int prev_sg) {
  int num_prev_board_cards = Game::NumBoardCards(st - 1);
  int num_cards_for_street = Game::NumCardsForStreet(st);
  CanonicalCards boards(num_cards_for_street, prev_board, num_prev_board_cards,
			prev_sg, true);
  num_boards_[st] += boards.NumCanon();
  Card next_board[5];
  for (int i = 0; i < num_prev_board_cards; ++i) {
    next_board[i] = prev_board[i];
  }
  int num_raw_boards = boards.NumRaw();
  for (int i = 0; i < num_raw_boards; ++i) {
    if (boards.NumVariants(i) == 0) continue;
    if (st < max_street_) {
      const Card *incr_next_board = boards.Cards(i);
      for (int j = 0; j < num_cards_for_street; ++j) {
	next_board[num_prev_board_cards + j] = incr_next_board[j];
      }
      int next_sg = boards.SuitGroups(i);
      Count(st + 1, next_board, next_sg);
    }
  }
}

void BoardTree::Build(int st, const unique_ptr<Card []> &prev_board, int prev_sg) {
  int num_prev_board_cards = Game::NumBoardCards(st - 1);
  int num_next_board_cards = Game::NumBoardCards(st);
  int num_cards_for_street = Game::NumCardsForStreet(st);
  CanonicalCards boards(num_cards_for_street, prev_board.get(),
			num_prev_board_cards, prev_sg, true);
  int num_raw_boards = boards.NumRaw();
  unique_ptr<Card []> next_board(new Card[num_next_board_cards]);
  for (int i = 0; i < num_prev_board_cards; ++i) {
    next_board[i] = prev_board[i];
  }
  for (int i = 0; i < num_raw_boards; ++i) {
    int board_variants = boards.NumVariants(i);
    if (board_variants == 0) continue;
    int bd = bds_[st]++;
    board_variants_[st][bd] = board_variants;
    const Card *incr_next_board = boards.Cards(i);
    for (int j = 0; j < num_cards_for_street; ++j) {
      next_board[num_prev_board_cards + j] = incr_next_board[j];
    }
    Card *to = &boards_[st][bd * num_next_board_cards];
    for (int j = 0; j < num_next_board_cards; ++j) {
      to[j] = next_board[j];
    }
    int next_sg = boards.SuitGroups(i);
    suit_groups_[st][bd] = next_sg;
    if (st < max_street_) {
      unique_ptr<int []> starts(new int[max_street_ + 1]);
      for (int nst = st + 1; nst <= max_street_; ++nst) {
	starts[nst] = bds_[nst];
      }
      Build(st + 1, next_board, next_sg);
      for (int nst = st + 1; nst <= max_street_; ++nst) {
	succ_board_begins_[st][nst][bd] = starts[nst];
	succ_board_ends_[st][nst][bd] = bds_[nst];
      }
    }
  }
}

void BoardTree::Create(void) {
  // Prevent multiple initialization
  if (boards_ != nullptr) return;
  max_street_ = Game::MaxStreet();
  num_boards_.reset(new int[max_street_ + 1]);
  // Convenient to say there is one empty board on the preflop
  num_boards_[0] = 1;
  for (int st = 1; st <= max_street_; ++st) num_boards_[st] = 0;
  if (max_street_ >= 1) Count(1, nullptr, 0);
  board_variants_ = new int *[max_street_ + 1];
  succ_board_begins_ = new int **[max_street_];
  succ_board_ends_ = new int **[max_street_];
  for (int st = 0; st <= max_street_; ++st) {
    int num_boards = num_boards_[st];
    board_variants_[st] = new int[num_boards];
  }
  board_variants_[0][0] = 1;
  for (int st = 0; st < max_street_; ++st) {
    int num_boards = num_boards_[st];
    succ_board_begins_[st] = new int *[max_street_ + 1];
    succ_board_ends_[st] = new int *[max_street_ + 1];
    for (int nst = st + 1; nst <= max_street_; ++nst) {
      succ_board_begins_[st][nst] = new int[num_boards];
      succ_board_ends_[st][nst] = new int[num_boards];
    }
  }
  boards_ = new Card *[max_street_ + 1];
  for (int st = 0; st <= max_street_; ++st) {
    int num_boards = num_boards_[st];
    int num_board_cards = Game::NumBoardCards(st);
    boards_[st] = new Card[num_boards * num_board_cards];
  }
  suit_groups_ = new int *[max_street_ + 1];
  for (int st = 0; st <= max_street_; ++st) {
    int num_boards = num_boards_[st];
    suit_groups_[st] = new int[num_boards];
  }
  suit_groups_[0][0] = 0;
  bds_ = new int[max_street_ + 1];
  for (int st = 0; st <= max_street_; ++st) {
    bds_[st] = 0;
  }
  if (max_street_ >= 1) Build(1, nullptr, 0);
  for (int st = 1; st <= max_street_; ++st) {
    if (bds_[st] != num_boards_[st]) {
      fprintf(stderr, "Num boards mismatch\n");
      exit(-1);
    }
    succ_board_begins_[0][st][0] = 0;
    succ_board_ends_[0][st][0] = num_boards_[st];
  }
  delete [] bds_;
  bds_ = nullptr;
  lookup_ = nullptr;
}

void BoardTree::Delete(void) {
  for (int st = 0; st <= max_street_; ++st) {
    delete [] board_variants_[st];
  }
  delete [] board_variants_;
  for (int st = 0; st < max_street_; ++st) {
    for (int nst = st + 1; nst <= max_street_; ++nst) {
      delete [] succ_board_begins_[st][nst];
      delete [] succ_board_ends_[st][nst];
    }
    delete [] succ_board_begins_[st];
    delete [] succ_board_ends_[st];
  }
  delete [] succ_board_begins_;
  delete [] succ_board_ends_;
  delete [] bds_;
  for (int st = 0; st <= max_street_; ++st) {
    delete [] boards_[st];
    delete [] suit_groups_[st];
  }
  delete [] boards_;
  delete [] suit_groups_;
  num_boards_.reset(nullptr);
}

void BoardTree::CreateLookup(void) {
  if (lookup_) return;
  int max_card1 = Game::MaxCard() + 1;
  lookup_ = new int *[max_street_ + 1];
  lookup_[0] = new int[1];
  lookup_[0][0] = 0;
  for (int st = 1; st <= max_street_; ++st) {
    int num_board_cards = Game::NumBoardCards(st);
    int num_codes = pow(max_card1, num_board_cards);
    lookup_[st] = new int[num_codes];
    for (int i = 0; i < num_codes; ++i) {
      lookup_[st][i] = kMaxInt;
    }
    int num_boards = num_boards_[st];
    for (int bd = 0; bd < num_boards; ++bd) {
      const Card *board = Board(st, bd);
      int code = 0;
      for (int i = 0; i < num_board_cards; ++i) {
	code += board[i] * pow(max_card1, i);
      }
      lookup_[st][code] = bd;
    }
  }
}

void BoardTree::DeleteLookup(void) {
  if (lookup_) {
    for (int st = 0; st <= max_street_; ++st) {
      delete [] lookup_[st];
    }
    delete [] lookup_;
    lookup_ = nullptr;
  }
}

int BoardTree::LookupBoard(const Card *board, int st) {
  if (lookup_ == nullptr) {
    fprintf(stderr, "Must call BoardTree::CreateLookup()\n");
    exit(-1);
  }
  int max_card1 = Game::MaxCard() + 1;
  int num_board_cards = Game::NumBoardCards(st);
  int code = 0;
  for (int i = 0; i < num_board_cards; ++i) {
    code += board[i] * pow(max_card1, i);
  }
  int ret = lookup_[st][code];
  if (ret == kMaxInt) {
    fprintf(stderr, "BoardTree::LookupBoard() invalid board; st %i code %i\n", st, code);
    OutputNCards(board, num_board_cards);
    printf("\n");
    fflush(stdout);
    exit(-1);
  }
  return ret;
}

void BoardTree::DealRawBoards(Card *board, int st) {
  int max_street = Game::MaxStreet();
  if (st > 1) {
    Card canon_board[5];
    CanonicalizeCards(board, nullptr, st - 1, canon_board, nullptr);
    int canon_bd = BoardTree::LookupBoard(canon_board, st - 1);
    if (canon_bd >= num_boards_[st - 1]) {
      fprintf(stderr, "DealRawBoards: couldn't look up canonical board\n");
      OutputThreeCards(board);
      printf(" -> ");
      OutputThreeCards(canon_board);
      printf("\n");
      fflush(stdout);
      exit(-1);
    }
    ++board_counts_[st - 1][canon_bd];
    if (st == max_street + 1) return;
  }
  int num_street_cards = Game::NumCardsForStreet(st);
  int num_prev_board_cards = Game::NumBoardCards(st - 1);
  Card max_card = Game::MaxCard();
  if (num_street_cards == 1) {
    for (Card c = 0; c <= max_card; ++c) {
      if (InCards(c, board, num_prev_board_cards)) continue;
      board[num_prev_board_cards] = c;
      DealRawBoards(board, st + 1);
    }
  } else if (num_street_cards == 2) {
    for (Card hi = 1; hi <= max_card; ++hi) {
      if (InCards(hi, board, num_prev_board_cards)) continue;
      board[num_prev_board_cards] = hi;
      for (Card lo = 0; lo < hi; ++lo) {
	if (InCards(lo, board, num_prev_board_cards)) continue;
	board[num_prev_board_cards + 1] = lo;
	DealRawBoards(board, st + 1);
      }
    }
  } else if (num_street_cards == 3) {
    for (Card hi = 2; hi <= max_card; ++hi) {
      if (InCards(hi, board, num_prev_board_cards)) continue;
      board[num_prev_board_cards] = hi;
      for (Card mid = 1; mid < hi; ++mid) {
	if (InCards(mid, board, num_prev_board_cards)) continue;
	board[num_prev_board_cards + 1] = mid;
	for (Card lo = 0; lo < mid; ++lo) {
	  if (InCards(lo, board, num_prev_board_cards)) continue;
	  board[num_prev_board_cards + 2] = lo;
	  DealRawBoards(board, st + 1);
	}
      }
    }
  } else {
    fprintf(stderr, "Can't handle %u street cards\n", num_street_cards);
    exit(-1);
  }
}

void BoardTree::BuildBoardCounts(void) {
  if (board_counts_) return;
  // Should I delete this when I am done?
  BoardTree::CreateLookup();
  board_counts_ = new int *[max_street_ + 1];
  board_counts_[0] = new int [1];
  board_counts_[0][0] = 1;
  for (int st = 1; st <= max_street_; ++st) {
    int num_boards = num_boards_[st];
    board_counts_[st] = new int[num_boards];
    for (int bd = 0; bd < num_boards; ++bd) board_counts_[st][bd] = 0;
  }
  if (max_street_ >= 1) {
    Card board[5];
    DealRawBoards(board, 1);
  }
  // Test that we saw each canonical board at least once
  for (int st = 1; st <= max_street_; ++st) {
    int num_boards = num_boards_[st];
    // int sum_counts = 0;
    for (int bd = 0; bd < num_boards; ++bd) {
      if (board_counts_[st][bd] == 0) {
	fprintf(stderr, "Never saw st %u bd %u\n", st, bd);
	exit(-1);
      }
      // sum_counts += board_counts_[st][bd];
    }
    // fprintf(stderr, "St %u sum counts %u\n", st, sum_counts);
  }
}

void BoardTree::DeleteBoardCounts(void) {
  if (board_counts_) {
    for (int st = 0; st <= max_street_; ++st) {
      delete [] board_counts_[st];
    }
    delete [] board_counts_;
    board_counts_ = nullptr;
  }
}

void BoardTree::BuildPredBoards(int st, int *pred_bds) {
  int pst = st - 1;
  int pbd = pred_bds[pst];
  int nbd_begin = BoardTree::SuccBoardBegin(pst, pbd, st);
  int nbd_end = BoardTree::SuccBoardEnd(pst, pbd, st);
  for (int nbd = nbd_begin; nbd < nbd_end; ++nbd) {
    if (st == max_street_) {
      for (int st1 = 0; st1 < max_street_; ++st1) {
	pred_boards_[nbd * max_street_ + st1] = pred_bds[st1];
      }
    } else {
      pred_bds[st] = nbd;
      BuildPredBoards(st + 1, pred_bds);
    }
  }
}

void BoardTree::BuildPredBoards(void) {
  if (pred_boards_) return;
  if (max_street_ <= 0) return;
  int num_ms_boards = num_boards_[max_street_];
  int sz = num_ms_boards * max_street_;
  pred_boards_ = new int[sz];
  int *pred_bds = new int[max_street_];
  pred_bds[0] = 0;
  BuildPredBoards(1, pred_bds);
  delete [] pred_bds;
}

void BoardTree::DeletePredBoards(void) {
  delete [] pred_boards_;
  pred_boards_ = nullptr;
}

