#ifndef _REACH_PROBS_H_
#define _REACH_PROBS_H_

#include <memory>

class Buckets;
class CanonicalCards;
class CFRValues;
class Node;

class ReachProbs {
public:
  ~ReachProbs(void) {}
  static ReachProbs *CreateRoot(void);
  static std::shared_ptr<ReachProbs []> CreateSuccReachProbs(Node *node, int gbd, int lbd,
							     const CanonicalCards *hands,
							     const Buckets &buckets,
							     const CFRValues *sumprobs,
							     const ReachProbs &pred_reach_probs,
							     bool purify);
  std::shared_ptr<double []> Get(int p) const {return probs_[p];}
  void Set(int p, std::shared_ptr<double []> probs) {probs_[p] = probs;}
  double Get(int p, int enc) const {return probs_[p][enc];}
  void Set(int p, int enc, double prob) {probs_[p][enc] = prob;}
private:
  ReachProbs(void);
  void Allocate(int p);
  
  std::unique_ptr<std::shared_ptr<double []> []> probs_;
};

#endif
