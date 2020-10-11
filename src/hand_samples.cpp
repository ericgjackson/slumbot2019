// Keep track of how many hands on prior streets are consistent with a set of sampled boards
// for a later street.

#include <stdio.h>
#include <stdlib.h>

#include <memory>

#include "board_tree.h"
#include "canonical_cards.h"
#include "cards.h"
#include "game.h"
#include "hand_samples.h"
#include "hand_tree.h"

using std::unique_ptr;

HandSamples::HandSamples(int sample_st) {
  sample_st_ = sample_st;
  counts_ = new int **[sample_st_];
  for (int st = 0; st < sample_st_; ++st) {
    int num_boards = BoardTree::NumBoards(st);
    int num_hole_card_pairs = Game::NumHoleCardPairs(st);
    counts_[st] = new int *[num_boards];
    for (int bd = 0; bd < num_boards; ++bd) {
      counts_[st][bd] = new int[num_hole_card_pairs];
      for (int i = 0; i < num_hole_card_pairs; ++i) {
	counts_[st][bd][i] = 0;
      }
    }
  }
}

HandSamples::~HandSamples(void) {
  for (int st = 0; st < sample_st_; ++st) {
    int num_boards = BoardTree::NumBoards(st);
    for (int bd = 0; bd < num_boards; ++bd) {
      delete [] counts_[st][bd];
    }
    delete [] counts_[st];
  }
  delete [] counts_;
}

void HandSamples::AddBoard(int bd, int num_samples) {
  int num_board_cards = Game::NumBoardCards(sample_st_);
  const Card *board = BoardTree::Board(sample_st_, bd);
  int sg = BoardTree::SuitGroups(sample_st_, bd);
  unique_ptr<CanonicalCards> hands(new CanonicalCards(2, board, num_board_cards, sg, false));
  int num_hole_card_pairs = Game::NumHoleCardPairs(sample_st_);
  for (int st = 0; st < sample_st_; ++st) {
    int pbd = BoardTree::PredBoard(bd, st);
    for (int hcp = 0; hcp < num_hole_card_pairs; ++hcp) {
      const Card *cards = hands->Cards(hcp);
      // Card hi = cards[0];
      // Card lo = cards[1];
      int phcp = HCPIndex(st, board, cards);
      counts_[st][pbd][phcp] += num_samples;
    }
  }
}

int HandSamples::Count(int st, int bd, int hcp) const {
  return counts_[st][bd][hcp];
}
