#ifndef _EG_CFR_H_
#define _EG_CFR_H_

#include <memory>
#include <string>

#include "resolving_method.h"
#include "vcfr.h"

class BettingAbstraction;
class BettingTrees;
class CardAbstraction;
class CFRConfig;
class HandTree;
class ReachProbs;

class EGCFR : public VCFR {
 public:
  EGCFR(const CardAbstraction &ca, const CardAbstraction &base_ca,
	const BettingAbstraction &base_ba, const CFRConfig &cc, const CFRConfig &base_cc,
	const Buckets &buckets, ResolvingMethod method, bool cfrs, bool zero_sum, int num_threads);
  virtual void SolveSubgame(BettingTrees *subtrees, int solve_bd, const ReachProbs &reach_probs,
			    const std::string &action_sequence, const HandTree *hand_tree,
			    double *opp_cvs, int target_p, bool both_players, int num_its) = 0;
 protected:
  virtual std::shared_ptr<double []> HalfIteration(BettingTrees *subtrees, int p,
						   std::shared_ptr<double []> opp_probs,
						   const HandTree *hand_tree,
						   const std::string &action_sequence);

  const CardAbstraction &base_card_abstraction_;
  const BettingAbstraction &base_betting_abstraction_;
  const CFRConfig &base_cfr_config_;
};

#endif
