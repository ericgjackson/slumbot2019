#ifndef _CANONICAL_H_
#define _CANONICAL_H_

#include "cards.h"

void CanonicalizeCards(const Card *raw_board, const Card *raw_hole_cards, int max_street,
		       Card *canon_board, Card *canon_hole_cards, int *suit_mapping);
void CanonicalizeCards(const Card *raw_board, const Card *raw_hole_cards, int max_street,
		       Card *canon_board, Card *canon_hole_cards);

#endif
