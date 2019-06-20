#ifndef _DYNAMIC_CBR_H_
#define _DYNAMIC_CBR_H_

#include <memory>

#include "vcfr.h"

class Buckets;
class CanonicalCards;
class CardAbstraction;
class HandTree;
class Node;
class ReachProbs;

class DynamicCBR : public VCFR {
public:
  DynamicCBR(const CardAbstraction &ca, const CFRConfig &cc, const Buckets &buckets,
	     int num_threads);
  DynamicCBR(void);
  ~DynamicCBR(void);
  std::shared_ptr<double []> Compute(Node *node, const ReachProbs &reach_probs, int gbd,
				     const HandTree *hand_tree, int target_p, bool cfrs,
				     bool zero_sum, bool current, bool purify_opp);
private:
  std::shared_ptr<double []> Compute(Node *node, int p, const std::shared_ptr<double []> &opp_probs,
				     int gbd, const HandTree *hand_tree);

  bool cfrs_;
};

#endif
