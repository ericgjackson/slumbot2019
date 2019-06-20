#ifndef _COMBINED_EG_CFR_H_
#define _COMBINED_EG_CFR_H_

#include <memory>
#include <string>

#include "eg_cfr.h"

class BettingTrees;
class ReachProbs;

class CombinedEGCFR : public EGCFR {
 public:
  CombinedEGCFR(const CardAbstraction &ca, const CardAbstraction &base_ca,
		const BettingAbstraction &base_ba, const CFRConfig &cc, const CFRConfig &base_cc,
		const Buckets &buckets, bool cfrs, bool zero_sum, int num_threads);
  void SolveSubgame(BettingTrees *subtrees, int solve_bd, const ReachProbs &reach_probs,
		    const std::string &action_sequence, const HandTree *hand_tree, double *opp_cvs,
		    int target_p, bool both_players, int num_its);
 protected:
  void HalfIteration(BettingTrees *subtrees, int target_p, int p, const ReachProbs &reach_probs,
		     const HandTree *hand_tree, const std::string &action_sequence,
		     double *opp_cvs);
  
  std::unique_ptr<double []> combined_regrets_;
};

#endif
