#ifndef _CFRD_EG_CFR_H_
#define _CFRD_EG_CFR_H_

#include <memory>
#include <string>

#include "eg_cfr.h"

class CFRDEGCFR : public EGCFR {
 public:
  CFRDEGCFR(const CardAbstraction &ca, const CardAbstraction &base_ca,
	    const BettingAbstraction &ba, const BettingAbstraction &base_ba,
	    const CFRConfig &cc, const CFRConfig &base_cc, const Buckets &buckets, bool cfrs,
	    bool zero_sum, int num_threads);
  void SolveSubgame(BettingTree *subtree, int solve_bd, std::shared_ptr<double []> *reach_probs,
		    const std::string &action_sequence, const HandTree *hand_tree, double *opp_cvs,
		    int target_p, bool both_players, int num_its);
 protected:
  void HalfIteration(BettingTree *subtree, int target_p, VCFRState *state, double *opp_cvs);
  
  std::unique_ptr<double []> cfrd_regrets_;
};

#endif
