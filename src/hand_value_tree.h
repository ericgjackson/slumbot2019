#ifndef _HAND_VALUE_TREE_H_
#define _HAND_VALUE_TREE_H_

#include "cards.h"

class HandValueTree {
public:
  // Note: currently you need to make sure that this is called from only one thread.
  static void Create(void);
  static void Delete(void);
  static bool Created(void);
  // Does *not* assume cards are sorted
  static int Val(const Card *cards);
  // board and hole_cards should be sorted from high to low.
  static int Val(const int *board, const int *hole_cards);
  static int DiskRead(Card *cards);
private:
  HandValueTree(void) {}

  static void ReadOne(void);
  static void ReadTwo(void);
  static void ReadThree(void);
  static void ReadFour(void);
  static void ReadFive(void);
  static void ReadSix(void);
  static void ReadSeven(void);

  static int num_board_cards_;
  static int num_cards_;
  static int *tree1_;
  static int **tree2_;
  static int ***tree3_;
  static int ****tree4_;
  static int *****tree5_;
  static int ******tree6_;
  static int *******tree7_;
};

#endif
