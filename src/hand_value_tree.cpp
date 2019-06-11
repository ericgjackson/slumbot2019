#include <stdio.h>

#include <algorithm>
#include <vector>

#include "cards.h"
#include "files.h"
#include "game.h"
#include "hand_value_tree.h"
#include "io.h"

using std::vector;

int HandValueTree::num_board_cards_ = 0;
int HandValueTree::num_cards_ = 0;
int *HandValueTree::tree1_ = NULL;
int **HandValueTree::tree2_ = NULL;
int ***HandValueTree::tree3_ = NULL;
int ****HandValueTree::tree4_ = NULL;
int *****HandValueTree::tree5_ = NULL;
int ******HandValueTree::tree6_ = NULL;
int *******HandValueTree::tree7_ = NULL;

// Note: currently you need to make sure that this is called from only one thread.
void HandValueTree::Create(void) {
  // Check if already created
  if (num_cards_ != 0) return;
  tree1_ = NULL;
  tree2_ = NULL;
  tree3_ = NULL;
  tree4_ = NULL;
  tree5_ = NULL;
  tree6_ = NULL;
  tree7_ = NULL;
  int max_street = Game::MaxStreet();
  num_board_cards_ = Game::NumBoardCards(max_street);
  num_cards_ = num_board_cards_ + Game::NumCardsForStreet(0);
  if (num_cards_ == 1)      ReadOne();
  else if (num_cards_ == 2) ReadTwo();
  else if (num_cards_ == 3) ReadThree();
  else if (num_cards_ == 4) ReadFour();
  else if (num_cards_ == 5) ReadFive();
  else if (num_cards_ == 6) ReadSix();
  else if (num_cards_ == 7) ReadSeven();
}

bool HandValueTree::Created(void) {
  return num_cards_ != 0;
}

#if 0
HandValueTree::HandValueTree(int st) {
  tree1_ = NULL;
  tree2_ = NULL;
  tree5_ = NULL;
  tree6_ = NULL;
  tree7_ = NULL;
  num_board_cards_ = Game::NumBoardCards(st);
  num_cards_ = num_board_cards_ + Game::NumCardsForStreet(0);
  if (num_cards_ == 1)      ReadOne();
  else if (num_cards_ == 2) ReadTwo();
  else if (num_cards_ == 3) ReadThree();
  else if (num_cards_ == 5) ReadFive();
  else if (num_cards_ == 6) ReadSix();
  else if (num_cards_ == 7) ReadSeven();
}
#endif

void HandValueTree::ReadOne(void) {
  int max_card = Game::MaxCard();
  char buf[500];
  sprintf(buf, "%s/hand_value_tree.%s.%i.%i.%i", Files::StaticBase(),
	  Game::GameName().c_str(), Game::NumRanks(), Game::NumSuits(),
	  num_cards_);
  Reader reader(buf);
  int num_cards = max_card + 1;
  tree1_ = new int[num_cards];
  for (int i = 0; i < num_cards; ++i) tree1_[i] = 0;
  for (int i1 = 0; i1 < num_cards; ++i1) {
    tree1_[i1] = reader.ReadIntOrDie();
  }
}

void HandValueTree::ReadTwo(void) {
  int max_card = Game::MaxCard();
  char buf[500];
  sprintf(buf, "%s/hand_value_tree.%s.%i.%i.%i", Files::StaticBase(),
	  Game::GameName().c_str(), Game::NumRanks(), Game::NumSuits(),
	  num_cards_);
  Reader reader(buf);
  int num_cards = max_card + 1;
  tree2_ = new int *[num_cards];
  for (int i = 0; i < num_cards; ++i) tree2_[i] = NULL;
  for (int i1 = 1; i1 < num_cards; ++i1) {
    int *tree1 = new int[i1];
    tree2_[i1] = tree1;
    for (int i2 = 0; i2 < i1; ++i2) {
      tree1[i2] = reader.ReadIntOrDie();
    }
  }
}

void HandValueTree::ReadThree(void) {
  int max_card = Game::MaxCard();
  char buf[500];
  sprintf(buf, "%s/hand_value_tree.%s.%i.%i.3", Files::StaticBase(),
	  Game::GameName().c_str(), Game::NumRanks(), Game::NumSuits());
  Reader reader(buf);
  int num_cards = max_card + 1;
  tree3_ = new int **[num_cards];
  for (int i = 0; i < num_cards; ++i) tree3_[i] = NULL;
  for (int i1 = 2; i1 < num_cards; ++i1) {
    int **tree1 = new int *[i1];
    tree3_[i1] = tree1;
    for (int i2 = 1; i2 < i1; ++i2) {
      int *tree2 = new int[i2];
      tree1[i2] = tree2;
      for (int i3 = 0; i3 < i2; ++i3) {
	tree2[i3] = reader.ReadIntOrDie();
      }
    }
  }
}

void HandValueTree::ReadFour(void) {
  int max_card = Game::MaxCard();
  char buf[500];
  sprintf(buf, "%s/hand_value_tree.%s.%i.%i.4", Files::StaticBase(),
	  Game::GameName().c_str(), Game::NumRanks(), Game::NumSuits());
  Reader reader(buf);
  int num_cards = max_card + 1;
  tree4_ = new int ***[num_cards];
  for (int i = 0; i < num_cards; ++i) tree4_[i] = NULL;
  for (int i1 = 3; i1 < num_cards; ++i1) {
    int ***tree1 = new int **[i1];
    tree4_[i1] = tree1;
    for (int i2 = 2; i2 < i1; ++i2) {
      int **tree2 = new int *[i2];
      tree1[i2] = tree2;
      for (int i3 = 1; i3 < i2; ++i3) {
	int *tree3 = new int[i3];
	tree2[i3] = tree3;
	for (int i4 = 0; i4 < i3; ++i4) {
	  tree3[i4] = reader.ReadIntOrDie();
	}
      }
    }
  }
}

void HandValueTree::ReadFive(void) {
  int max_card = Game::MaxCard();
  char buf[500];
  sprintf(buf, "%s/hand_value_tree.%s.%i.%i.5", Files::StaticBase(),
	  Game::GameName().c_str(), Game::NumRanks(), Game::NumSuits());
  Reader reader(buf);
  int num_cards = max_card + 1;
  tree5_ = new int ****[num_cards];
  for (int i = 0; i < num_cards; ++i) tree5_[i] = NULL;
  for (int i1 = 4; i1 < num_cards; ++i1) {
    int ****tree1 = new int ***[i1];
    tree5_[i1] = tree1;
    for (int i2 = 3; i2 < i1; ++i2) {
      int ***tree2 = new int **[i2];
      tree1[i2] = tree2;
      for (int i3 = 2; i3 < i2; ++i3) {
	int **tree3 = new int *[i3];
	tree2[i3] = tree3;
	for (int i4 = 1; i4 < i3; ++i4) {
	  int *tree4 = new int[i4];
	  tree3[i4] = tree4;
	  for (int i5 = 0; i5 < i4; ++i5) {
	    tree4[i5] = reader.ReadIntOrDie();
	  }
	}
      }
    }
  }
}

void HandValueTree::ReadSix(void) {
  int max_card = Game::MaxCard();
  char buf[500];
  sprintf(buf, "%s/hand_value_tree.%s.%i.%i.6", Files::StaticBase(),
	  Game::GameName().c_str(), Game::NumRanks(), Game::NumSuits());
  Reader reader(buf);
  int num_cards = max_card + 1;
  tree6_ = new int *****[num_cards];
  for (int i = 0; i < num_cards; ++i) tree6_[i] = NULL;
  for (int i1 = 5; i1 < num_cards; ++i1) {
    int *****tree1 = new int ****[i1];
    tree6_[i1] = tree1;
    for (int i2 = 4; i2 < i1; ++i2) {
      int ****tree2 = new int ***[i2];
      tree1[i2] = tree2;
      for (int i3 = 3; i3 < i2; ++i3) {
	int ***tree3 = new int **[i3];
	tree2[i3] = tree3;
	for (int i4 = 2; i4 < i3; ++i4) {
	  int **tree4 = new int *[i4];
	  tree3[i4] = tree4;
	  for (int i5 = 1; i5 < i4; ++i5) {
	    int *tree5 = new int[i5];
	    tree4[i5] = tree5;
	    for (int i6 = 0; i6 < i5; ++i6) {
	      tree5[i6] = reader.ReadIntOrDie();
	    }
	  }
	}
      }
    }
  }
}

void HandValueTree::ReadSeven(void) {
  int max_card = Game::MaxCard();
  char buf[500];
  sprintf(buf, "%s/hand_value_tree.%s.%i.%i.7", Files::StaticBase(),
	  Game::GameName().c_str(), Game::NumRanks(), Game::NumSuits());
  Reader reader(buf);
  int num_cards = max_card + 1;
  tree7_ = new int ******[num_cards];
  for (int i = 0; i < num_cards; ++i) tree7_[i] = NULL;
  for (int i1 = 6; i1 < num_cards; ++i1) {
    int ******tree1 = new int *****[i1];
    tree7_[i1] = tree1;
    for (int i2 = 5; i2 < i1; ++i2) {
      int *****tree2 = new int ****[i2];
      tree1[i2] = tree2;
      for (int i3 = 4; i3 < i2; ++i3) {
	int ****tree3 = new int ***[i3];
	tree2[i3] = tree3;
	for (int i4 = 3; i4 < i3; ++i4) {
	  int ***tree4 = new int **[i4];
	  tree3[i4] = tree4;
	  for (int i5 = 2; i5 < i4; ++i5) {
	    int **tree5 = new int *[i5];
	    tree4[i5] = tree5;
	    for (int i6 = 1; i6 < i5; ++i6) {
	      int *tree6 = new int[i6];
	      tree5[i6] = tree6;
	      for (int i7 = 0; i7 < i6; ++i7) {
		tree6[i7] = reader.ReadIntOrDie();
	      }
	    }
	  }
	}
      }
    }
  }
}

void HandValueTree::Delete(void) {
  int max_card = Game::MaxCard();
  if (num_cards_ == 1) {
    delete [] tree1_;
  } else if (num_cards_ == 2) {
    for (int i1 = 1; i1 <= max_card; ++i1) {
      delete [] tree2_[i1];
    }
    delete [] tree2_;
  } else if (num_cards_ == 3) {
    for (int i1 = 2; i1 <= max_card; ++i1) {
      int **tree1 = tree3_[i1];
      for (int i2 = 1; i2 < i1; ++i2) {
	delete [] tree1[i2];
      }
      delete [] tree1;
    }
    delete [] tree3_;
  } else if (num_cards_ == 4) {
    for (int i1 = 3; i1 <= max_card; ++i1) {
      int ***tree1 = tree4_[i1];
      for (int i2 = 2; i2 < i1; ++i2) {
	int **tree2 = tree1[i2];
	for (int i3 = 1; i3 < i2; ++i3) {
	  delete [] tree2[i3];
	}
	delete [] tree2;
      }
      delete [] tree1;
    }
    delete [] tree5_;
  } else if (num_cards_ == 5) {
    for (int i1 = 4; i1 <= max_card; ++i1) {
      int ****tree1 = tree5_[i1];
      for (int i2 = 3; i2 < i1; ++i2) {
	int ***tree2 = tree1[i2];
	for (int i3 = 2; i3 < i2; ++i3) {
	  int **tree3 = tree2[i3];
	  for (int i4 = 1; i4 < i3; ++i4) {
	    delete [] tree3[i4];
	  }
	  delete [] tree3;
	}
	delete [] tree2;
      }
      delete [] tree1;
    }
    delete [] tree5_;
  } else if (num_cards_ == 6) {
    for (int i1 = 5; i1 <= max_card; ++i1) {
      int *****tree1 = tree6_[i1];
      for (int i2 = 4; i2 < i1; ++i2) {
	int ****tree2 = tree1[i2];
	for (int i3 = 3; i3 < i2; ++i3) {
	  int ***tree3 = tree2[i3];
	  for (int i4 = 2; i4 < i3; ++i4) {
	    int **tree4 = tree3[i4];
	    for (int i5 = 1; i5 < i4; ++i5) {
	      delete [] tree4[i5];
	    }
	    delete [] tree4;
	  }
	  delete [] tree3;
	}
	delete [] tree2;
      }
      delete [] tree1;
    }
    delete [] tree5_;
  } else if (num_cards_ == 7) {
    for (int i1 = 6; i1 <= max_card; ++i1) {
      int ******tree1 = tree7_[i1];
      for (int i2 = 5; i2 < i1; ++i2) {
	int *****tree2 = tree1[i2];
	for (int i3 = 4; i3 < i2; ++i3) {
	  int ****tree3 = tree2[i3];
	  for (int i4 = 3; i4 < i3; ++i4) {
	    int ***tree4 = tree3[i4];
	    for (int i5 = 2; i5 < i4; ++i5) {
	      int **tree5 = tree4[i5];
	      for (int i6 = 1; i6 < i5; ++i6) {
		delete [] tree5[i6];
	      }
	      delete [] tree5;
	    }
	    delete [] tree4;
	  }
	  delete [] tree3;
	}
	delete [] tree2;
      }
      delete [] tree1;
    }
    delete [] tree7_;
  }
  tree1_ = NULL;
  tree2_ = NULL;
  tree3_ = NULL;
  tree4_ = NULL;
  tree5_ = NULL;
  tree6_ = NULL;
  tree7_ = NULL;
  // So HandValueTree::Create() will do something on next call
  num_cards_ = 0;
}

#if 0
// Just do a selection sort.  We assume num is small.
static void SortCards(Card *cards, int num) {
  if (num == 1) return;
  for (int i = 0; i < num - 1; ++i) {
    Card maxc = cards[i];
    int maxi = i;
    for (int j = i + 1; j < num; ++j) {
      Card c = cards[j];
      if (c > maxc) {
	maxc = c;
	maxi = j;
      }
    }
    if (i != maxi) {
      cards[maxi] = cards[i];
      cards[i] = maxc;
    }
  }
}
#endif

int HandValueTree::Val(const Card *cards) {
  if (num_cards_ == 1) {
    return tree1_[(int)cards[0]];
  } else if (num_cards_ == 2) {
    vector<int> v(2);
    v[0] = cards[0];
    v[1] = cards[1];
    std::sort(v.begin(), v.end());
    return tree2_[v[1]][v[0]];
  } else if (num_cards_ == 3) {
    vector<int> v(3);
    v[0] = cards[0];
    v[1] = cards[1];
    v[2] = cards[2];
    std::sort(v.begin(), v.end());
    return tree3_[v[2]][v[1]][v[0]];
  } else if (num_cards_ == 4) {
    vector<int> v(4);
    v[0] = cards[0];
    v[1] = cards[1];
    v[2] = cards[2];
    v[3] = cards[3];
    std::sort(v.begin(), v.end());
    return tree4_[v[3]][v[2]][v[1]][v[0]];
  } else if (num_cards_ == 5) {
    vector<int> v(5);
    v[0] = cards[0];
    v[1] = cards[1];
    v[2] = cards[2];
    v[3] = cards[3];
    v[4] = cards[4];
    std::sort(v.begin(), v.end());
    return tree5_[v[4]][v[3]][v[2]][v[1]][v[0]];
  } else if (num_cards_ == 6) {
    vector<int> v(6);
    v[0] = cards[0];
    v[1] = cards[1];
    v[2] = cards[2];
    v[3] = cards[3];
    v[4] = cards[4];
    v[5] = cards[5];
    std::sort(v.begin(), v.end());
    return tree6_[v[5]][v[4]][v[3]][v[2]][v[1]][v[0]];
  } else if (num_cards_ == 7) {
    vector<int> v(7);
    v[0] = cards[0];
    v[1] = cards[1];
    v[2] = cards[2];
    v[3] = cards[3];
    v[4] = cards[4];
    v[5] = cards[5];
    v[6] = cards[6];
    std::sort(v.begin(), v.end());
    return tree7_[v[6]][v[5]][v[4]][v[3]][v[2]][v[1]][v[0]];
#if 0
    Card s[7];
    s[0] = cards[0];
    s[1] = cards[1];
    s[2] = cards[2];
    s[3] = cards[3];
    s[4] = cards[4];
    s[5] = cards[5];
    s[6] = cards[6];
    SortCards(s, 7);
    return tree7_[s[0]][s[1]][s[2]][s[3]][s[4]][s[5]][s[6]];
#endif
  } else {
    fprintf(stderr, "HandValueTree::Val: unexpected number of cards: %u\n",
	    num_cards_);
    exit(-1);
  }
}

// board and hole_cards should be sorted from high to low.
int HandValueTree::Val(const int *board, const int *hole_cards) {
  if (num_cards_ == 1) {
    return tree1_[hole_cards[0]];
  } else if (num_cards_ == 2 && num_board_cards_ == 1) {
    int b = board[0];
    int h = hole_cards[0];
    if (b < h) {
      return tree2_[h][b];
    } else {
      return tree2_[b][h];
    }
  } else if (num_cards_ == 2 && num_board_cards_ == 0) {
    int h0 = hole_cards[0];
    int h1 = hole_cards[1];
    if (h0 < h1) {
      return tree2_[h1][h0];
    } else {
      return tree2_[h0][h1];
    }
  } else if (num_cards_ == 3) {
    int a[3];
    int i = 0, j = 0, k = 0;
    int b = board[i];
    int h = hole_cards[j];
    while (i < 1 || j < 2) {
      if (b > h) {
	a[k++] = b;
	++i;
	b = -1;
      } else {
	a[k++] = h;
	++j;
	if (j < 2) h = hole_cards[j];
	else       h = -1;
      }
    }
    return tree3_[a[0]][a[1]][a[2]];
  } else if (num_cards_ == 4) {
    int a[4];
    int i = 0, j = 0, k = 0;
    int b = board[i];
    int h = hole_cards[j];
    while (i < 2 || j < 2) {
      if (b > h) {
	a[k++] = b;
	++i;
	if (i < 2) b = board[i];
	else       b = -1;
      } else {
	a[k++] = h;
	++j;
	if (j < 2) h = hole_cards[j];
	else       h = -1;
      }
    }
    return tree4_[a[0]][a[1]][a[2]][a[3]];
  } else if (num_cards_ == 5) {
    int a[5];
    int i = 0, j = 0, k = 0;
    int b = board[i];
    int h = hole_cards[j];
    while (i < 3 || j < 2) {
      if (b > h) {
	a[k++] = b;
	++i;
	if (i < 3) b = board[i];
	else       b = -1;
      } else {
	a[k++] = h;
	++j;
	if (j < 2) h = hole_cards[j];
	else       h = -1;
      }
    }
    return tree5_[a[0]][a[1]][a[2]][a[3]][a[4]];
  } else if (num_cards_ == 6) {
    int a[6];
    int i = 0, j = 0, k = 0;
    int b = board[i];
    int h = hole_cards[j];
    while (i < 4 || j < 2) {
      if (b > h) {
	a[k++] = b;
	++i;
	if (i < 4) b = board[i];
	else       b = -1;
      } else {
	a[k++] = h;
	++j;
	if (j < 2) h = hole_cards[j];
	else       h = -1;
      }
    }
    return tree6_[a[0]][a[1]][a[2]][a[3]][a[4]][a[5]];
  } else if (num_cards_ == 7) {
    int a[7];
    int i = 0, j = 0, k = 0;
    int b = board[i];
    int h = hole_cards[j];
    while (i < 5 || j < 2) {
      if (b > h) {
	a[k++] = b;
	++i;
	if (i < 5) b = board[i];
	else       b = -1;
      } else {
	a[k++] = h;
	++j;
	if (j < 2) h = hole_cards[j];
	else       h = -1;
      }
    }
#if 0
    printf("    %u %u %u %u %u %u %u\n", a[0], a[1], a[2], a[3], a[4], a[5],
	   a[6]);
    fflush(stdout);
#endif
    return tree7_[a[0]][a[1]][a[2]][a[3]][a[4]][a[5]][a[6]];
  } else {
    fprintf(stderr, "HandValueTree::Val: unexpected number of cards: %u\n",
	    num_cards_);
    exit(-1);
  }
}

// Returns the number of ordered tuples of n cards of which the first card
// is <= c.
// Suppose we want the number of n-tuples <6 or less, ?>.
// It's the sum of:
//   Number of n-1 tuples <5 or less>
//   Number of n-1 tuples <4 or less>
//   Number of n-1 tuples <3 or less>
//   Number of n-1 tuples <2 or less>
//   Number of n-1 tuples <1 or less>
//   Number of n-1 tuples <0 or less>
static long long int GetNumCombinations(int c, int num_cards) {
  if (num_cards == 1) {
    return c + 1;
  } else {
    long long int sum = 0;
    for (int c1 = 1; c1 <= c; ++c1) {
      sum += GetNumCombinations(c1-1, num_cards - 1);
#if 0
      if (sum > 0) {
	fprintf(stderr, "num_cards %i c1 %i c %i sum now %lli\n", num_cards,
		c1, c, sum);
      }
#endif
    }
    return sum;
  }
}

int HandValueTree::DiskRead(Card *cards) {
  int max_street = Game::MaxStreet();
  int num_cards = 0;
  for (int s = 0; s <= max_street; ++s) {
    num_cards += Game::NumCardsForStreet(s);
  }
  char buf[500];
  sprintf(buf, "%s/hand_value_tree.%s.%i.%i.%i", Files::StaticBase(), 
	  Game::GameName().c_str(), Game::NumRanks(), Game::NumSuits(),
	  num_cards);
  Reader reader(buf);
  if (num_cards == 7) {
    vector<int> v(7);
    v[0] = cards[0];
    v[1] = cards[1];
    v[2] = cards[2];
    v[3] = cards[3];
    v[4] = cards[4];
    v[5] = cards[5];
    v[6] = cards[6];
    std::sort(v.begin(), v.end());
    // What is the index for <x1, x2, x3, x4, x5, x6, x7>?
    // Sum of:
    //   Number of tuples <y1, ...> for y1 < x1
    //   Number of tuples <x1, y2, ...> for y2 < x2
    // Suppose cards (from high to low) are 10 8 6 4
    long long int loc = 0LL;
    loc += GetNumCombinations(v[6] - 1, 7);
    loc += GetNumCombinations(v[5] - 1, 6);
    loc += GetNumCombinations(v[4] - 1, 5);
    loc += GetNumCombinations(v[3] - 1, 4);
    loc += GetNumCombinations(v[2] - 1, 3);
    loc += GetNumCombinations(v[1] - 1, 2);
    if (v[0] > 0) loc += v[0];
    loc *= sizeof(int);
    reader.SeekTo(loc);
    return reader.ReadIntOrDie();
  } else {
    fprintf(stderr, "num_cards %i not supported\n", num_cards);
    exit(-1);
  }
}
