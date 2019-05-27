#ifndef _BOARD_TREE_H_
#define _BOARD_TREE_H_

#include <memory>

#include "cards.h"
#include "game.h"

class BoardTree {
public:
  static void Create(void);
  static void Delete(void);
  static const Card *Board(int st, int bd) {
    if (st == 0) return NULL;
    int num_board_cards = Game::NumBoardCards(st);
    return &boards_[st][bd * num_board_cards];
  }
  static int SuitGroups(int st, int bd) {return suit_groups_[st][bd];}
  // The number of isomorphic variants of the current street addition to the board.  For example,
  // for AcKcQc9d the number of variants is 3 because the 9d turns has three isomorphic variants
  // (9d, 9h, 9s).
  static int NumVariants(int st, int bd) {return board_variants_[st][bd];}
  static int LocalIndex(int root_st, int root_bd, int st, int gbd);
  static int GlobalIndex(int root_st, int root_bd, int st, int lbd);
  static int NumLocalBoards(int root_st, int root_bd, int st);
  static int NumBoards(int st) {return num_boards_[st];}
  static int SuccBoardBegin(int root_st, int root_bd, int st) {
    return succ_board_begins_[root_st][st][root_bd];
  }
  static int SuccBoardEnd(int root_st, int root_bd, int st) {
    return succ_board_ends_[root_st][st][root_bd];
  }
  static void CreateLookup(void);
  static void DeleteLookup(void);
  static int LookupBoard(const Card *board, int st);
  static void BuildBoardCounts(void);
  static void DeleteBoardCounts(void);
  static int BoardCount(int st, int bd) {return board_counts_[st][bd];}
  static void BuildPredBoards(void);
  static void DeletePredBoards(void);
  static int PredBoard(int msbd, int pst) {
    return pred_boards_[msbd * max_street_ + pst];
  }
private:
  BoardTree(void) {}
  
  static void Count(int st, const Card *prev_board, int prev_sg);
  static void Build(int st, const std::unique_ptr<Card []> &prev_board, int prev_sg);
  static void DealRawBoards(Card *board, int st);
  static void BuildPredBoards(int st, int *pred_bds);

  static int max_street_;
  static std::unique_ptr<int []> num_boards_;
  static int **board_variants_;
  static int ***succ_board_begins_;
  static int ***succ_board_ends_;
  static Card **boards_;
  static int **suit_groups_;
  static int *bds_;
  static int **lookup_;
  static int **board_counts_;
  static int *pred_boards_;
};

#endif
