#ifndef _VCFR_H_
#define _VCFR_H_

#include <memory>
#include <string>

#include "cfr_values.h"
#include "prob_method.h"

class BettingAbstraction;
class Buckets;
class CardAbstraction;
class CFRConfig;
class HandTree;
class VCFRState;

class VCFR {
 public:
  VCFR(const CardAbstraction &ca, const BettingAbstraction &ba, const CFRConfig &cc,
       const Buckets &buckets, int num_threads);
  virtual ~VCFR(void) {}
  virtual std::shared_ptr<double []> Process(Node *p0_node, Node *p1_node, int lbd,
					     const VCFRState &state, int last_st);
  CFRValues *Sumprobs(void) const {return sumprobs_.get();}
  void SetSumprobs(std::shared_ptr<CFRValues> &src) {sumprobs_ = src;}
  void SetRegrets(std::shared_ptr<CFRValues> &src) {regrets_ = src;}
  void ClearSumprobs(void) {sumprobs_.reset();}
  virtual void SetStreetBuckets(int st, int gbd, const VCFRState &state);
  virtual void SetValueCalculation(bool b) {value_calculation_ = b;}
  virtual void SetBestResponseStreet(int st, bool b) {best_response_streets_[st] = b;}
 protected:
  template <typename T>
    void UpdateRegrets(Node *node, double *vals, std::shared_ptr<double []> *succ_vals, T *regrets);
  virtual void UpdateRegrets(Node *node, int lbd, double *vals,
			     std::shared_ptr<double []> *succ_vals);
  virtual void UpdateRegretsBucketed(Node *node, int *street_buckets, double *vals,
				     std::shared_ptr<double []> *succ_vals, int *regrets);
  virtual void UpdateRegretsBucketed(Node *node, int *street_buckets, double *vals,
				     std::shared_ptr<double []> *succ_vals, double *regrets);
  virtual void UpdateRegretsBucketed(Node *node, int *street_buckets, double *vals,
				     std::shared_ptr<double []> *succ_vals);
  virtual std::shared_ptr<double []> OurChoice(Node *p0_node, Node *p1_node, int lbd,
					       const VCFRState &state);
  virtual std::shared_ptr<double []> OppChoice(Node *p0_node, Node *p1_node, int lbd,
					       const VCFRState &state);
  virtual void Split(Node *p0_node, Node *p1_node, int p,
		     const std::shared_ptr<double []> &opp_probs, const HandTree *hand_tree,
		     const std::string &action_sequence, int *prev_canons, double *vals);
  virtual std::shared_ptr<double []> StreetInitial(Node *p0_node, Node *p1_node,
						   int lbd, const VCFRState &state);
  virtual void SetCurrentStrategy(Node *node);
  
  const CardAbstraction &card_abstraction_;
  const BettingAbstraction &betting_abstraction_;
  const CFRConfig &cfr_config_;
  const Buckets &buckets_;
  int num_threads_;
  std::shared_ptr<CFRValues> regrets_;
  std::shared_ptr<CFRValues> sumprobs_;
  std::unique_ptr<CFRValues> current_strategy_;
  // best_response_streets_ are set to true in run_rgbr, for example.
  // Whenever some streets are best-response streets, value_calculation_ is true.
  std::unique_ptr<bool []> best_response_streets_;
  bool br_current_;
  ProbMethod prob_method_;
  // value_calculation_ is true in, e.g., run_rgbr
  bool value_calculation_;
  bool prune_;
  int subgame_street_;
  bool nn_regrets_;
  int soft_warmup_;
  int hard_warmup_;
  std::unique_ptr<bool []> sumprob_streets_;
  std::unique_ptr<int []> regret_floors_;
  std::unique_ptr<int []> regret_ceilings_;
  std::unique_ptr<double []> regret_scaling_;
  std::unique_ptr<double []> sumprob_scaling_;
  int it_;
  bool pre_phase_;
};

#endif
