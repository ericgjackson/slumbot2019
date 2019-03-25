#ifndef _MP_VCFR_H_
#define _MP_VCFR_H_

#include <memory>
#include <string>

class BettingAbstraction;
class BettingTree;
class Buckets;
class CardAbstraction;
class CFRConfig;
class CFRValues;
class HandTree;

class MPVCFR {
 public:
  MPVCFR(const CardAbstraction &ca, const BettingAbstraction &ba, const CFRConfig &cc,
	 const Buckets &buckets, const BettingTree *betting_tree, int num_threads);
  HandTree *GetHandTree(void) const {return hand_tree_;}
  void SetStreetBuckets(int st, int gbd, int **street_buckets);
  double *Process(Node *node, int lbd, int last_bet_to, int *contributions, double **opp_probs,
		  int **street_buckets, const std::string &action_sequence, int last_st);
 protected:
  double *OurChoice(Node *node, int lbd, int last_bet_to, int *contributions, double **opp_probs,
		    int **street_buckets, const std::string &action_sequence);
  double *OppChoice(Node *node, int lbd, int last_bet_to, int *contributions, double **opp_probs,
		    int **street_buckets, const std::string &action_sequence);
  double *StreetInitial(Node *node, int plbd, int last_bet_to, int *contributions,
			double **opp_probs, int **street_buckets,
			const std::string &action_sequence);
  int **InitializeStreetBuckets(void);
  void DeleteStreetBuckets(int **street_buckets);
  void SetCurrentStrategy(Node *node);
  
  const CardAbstraction &card_abstraction_;
  const BettingAbstraction &betting_abstraction_;
  const CFRConfig &cfr_config_;
  const Buckets &buckets_;
  const BettingTree *betting_tree_;
  std::unique_ptr<CFRValues> regrets_;
  std::unique_ptr<CFRValues> sumprobs_;
  std::unique_ptr<CFRValues> current_strategy_;
  // best_response_ is true in run_rgbr, build_cbrs, build_prbrs
  // Whenever best_response_ is true, value_calculation_ is true
  std::unique_ptr<bool []> best_response_streets_;
  bool br_current_;
  // value_calculation_ is true in run_rgbr, build_cbrs, build_prbrs,
  // build_cfrs.
  bool value_calculation_;
  bool nn_regrets_;
  bool uniform_;
  int soft_warmup_;
  int hard_warmup_;
  double explore_;
  bool *compressed_streets_;
  bool **sumprob_streets_;
  int *regret_floors_;
  double *sumprob_scaling_;
  int it_;
  int p_;
  HandTree *hand_tree_;
  int num_threads_;
};

#endif
