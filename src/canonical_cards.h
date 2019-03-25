#ifndef _CANONICAL_CARDS_H_
#define _CANONICAL_CARDS_H_

#include <memory>

#include "cards.h"

class CanonicalCards {
 public:
  CanonicalCards(void) {}
  CanonicalCards(int n, const Card *previous, int num_previous, int previous_suit_groups,
		 bool maintain_suit_groups);
  virtual ~CanonicalCards(void);
  void SortByHandStrength(const Card *board);
  static bool ToCanon2(const Card *cards, int num_cards,
		       int suit_groups, Card *canon_cards);
  static void ToCanon(const Card *cards, int num_cards,
		      int suit_groups, Card *canon_cards);
  int NumVariants(int i) const {return num_variants_[i];}
  int Canon(int i) const {return canon_[i];}
  int N(void) const {return n_;}
  int NumRaw(void) const {return num_raw_;}
  int NumCanon(void) const {return num_canon_;}
  const Card *Cards(int i) const {return &cards_[i * n_];}
  int HandValue(int i) const {return hand_values_[i];}
  int SuitGroups(int i) const {return suit_groups_[i];}
 protected:
  int NumMappings(const Card *cards, int n, int old_suit_groups);

  int n_;
  std::unique_ptr<Card []> cards_;
  std::unique_ptr<int []> hand_values_;
  std::unique_ptr<unsigned char []> num_variants_;
  std::unique_ptr<int []> canon_;
  int num_raw_;
  int num_canon_;
  std::unique_ptr<int []> suit_groups_;
};

void UpdateSuitGroups(const Card *cards, int num_cards, const int old_suit_groups,
		      int *new_suit_groups);

#endif
