#include "cards.h"
#include "sorting.h"

PDILowerCompare g_pdi_lower_compare;
PIILowerCompare g_pii_lower_compare;
PDSHigherCompare g_pds_higher_compare;
PFUILowerCompare g_pfui_lower_compare;

// Only handles n <= 3
void SortCards(Card *cards, unsigned int n) {
  if (n == 1) {
    return;
  } else if (n == 2) {
    if (cards[0] < cards[1]) {
      Card temp = cards[0];
      cards[0] = cards[1];
      cards[1] = temp;
    }
  } else if (n == 3) {
    Card c0 = cards[0];
    Card c1 = cards[1];
    Card c2 = cards[2];
    if (c0 > c1) {
      if (c1 > c2) {
	// c0, c1, c2
      } else if (c0 > c2) {
	// c0, c2, c1
	cards[1] = c2;
	cards[2] = c1;
      } else {
	// c2, c0, c1
	cards[0] = c2;
	cards[1] = c0;
	cards[2] = c1;
      }
    } else {
      if (c0 > c2) {
	// c1, c0, c2
	cards[0] = c1;
	cards[1] = c0;
      } else if (c1 > c2) {
	// c1, c2, c0
	cards[0] = c1;
	cards[1] = c2;
	cards[2] = c0;
      } else {
	// c2, c1, c0
	cards[0] = c2;
	cards[1] = c1;
	cards[2] = c0;
      }
    }
  } else {
    fprintf(stderr, "Don't know how to sort %u cards\n", n);
    exit(-1);
  }
}
