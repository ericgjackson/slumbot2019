#ifndef _HAND_EVALUATOR_H_
#define _HAND_EVALUATOR_H_

#include <string>

#include "cards.h"

class HandEvaluator {
public:
  HandEvaluator(void);
  virtual ~HandEvaluator(void);
  static HandEvaluator *Create(const std::string &name);
  virtual int Evaluate(Card *cards, int num_cards) = 0;

private:
};

class LeducHandEvaluator : public HandEvaluator {
 public:
  LeducHandEvaluator(void);
  ~LeducHandEvaluator(void);
  int Evaluate(Card *cards, int num_cards);
};

class HoldemHandEvaluator : public HandEvaluator {
 public:
  HoldemHandEvaluator(void);
  ~HoldemHandEvaluator(void);
  int Evaluate(Card *cards, int num_cards);

  static const int kMaxHandVal = 775905;
  static const int kStraightFlush = 775892;
  static const int kQuads = 775723;
  static const int kFullHouse = 775554;
  static const int kFlush = 404261;
  static const int kStraight = 404248;
  static const int kThreeOfAKind = 402051;
  static const int kTwoPair = 399854;
  static const int kPair = 371293;
  static const int kNoPair = 0;

  // Values for four-card Holdem
  static const int kH4MaxHandVal = 31109;
  static const int kH4Quads = 31096;
  static const int kH4ThreeOfAKind = 30927;
  static const int kH4TwoPair = 30758;
  static const int kH4Pair = 28561;
  static const int kH4NoPair = 0;
 private:
  int EvaluateTwo(Card *cards);
  int EvaluateThree(Card *cards);
  int EvaluateFour(Card *cards);

  int *ranks_;
  int *suits_;
  int *rank_counts_;
  int *suit_counts_;
};

#endif
