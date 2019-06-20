#ifndef _UNSAFE_EG_CFR_H_
#define _UNSAFE_EG_CFR_H_

#include <string>

#include "eg_cfr.h"

class BettingTrees;
class ReachProbs;

class UnsafeEGCFR : public EGCFR {
 public:
  UnsafeEGCFR(const CardAbstraction &ca, const CardAbstraction &base_ca,
	      const BettingAbstraction &base_ba, const CFRConfig &cc, const CFRConfig &base_cc,
	      const Buckets &buckets, int num_threads);
  void SolveSubgame(BettingTrees *subtrees, int solve_bd, const ReachProbs &reach_probs,
		    const std::string &action_sequence, const HandTree *hand_tree, double *opp_cvs,
		    int target_p, bool both_players, int num_its);
 protected:
};

#endif
