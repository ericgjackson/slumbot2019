#ifndef _VCFR_H_
#define _VCFR_H_

#include <memory>
#include <string>
#include <queue>

#include "cfr_values.h"
#include "prob_method.h"

class BettingAbstraction;
class BettingTrees;
class Buckets;
class CardAbstraction;
class CFRConfig;
class HandTree;
class VCFRState;
class VCFRWorker;

enum class RequestType {
  PROCESS,
  QUIT
};

class Request {
public:
  Request(RequestType t, Node *p0_node, Node *p1_node, int gbd, const VCFRState *pred_state,
	  int *prev_canons);
  ~Request(void) {}
  RequestType GetRequestType(void) const {return request_type_;}
  Node *P0Node(void) const {return p0_node_;}
  Node *P1Node(void) const {return p1_node_;}
  int GBD(void) const {return gbd_;}
  const VCFRState &PredState(void) const {return *pred_state_;}
  int *PrevCanons(void) const {return prev_canons_;}
private:
  RequestType request_type_;
  Node *p0_node_;
  Node *p1_node_;
  int gbd_;
  const VCFRState *pred_state_;
  int *prev_canons_;
};

class VCFR {
 public:
  VCFR(const CardAbstraction &ca, const CFRConfig &cc, const Buckets &buckets, int num_threads);
  virtual ~VCFR(void);
  virtual std::shared_ptr<double []> ProcessRoot(const BettingTrees *betting_trees, int p,
						 HandTree *hand_tree);
  virtual std::shared_ptr<double []> ProcessSubgame(Node *p0_node, Node *p1_node, int gbd,
						    const VCFRState &pred_state);
  virtual std::shared_ptr<double []> ProcessSubgame(Node *p0_node, Node *p1_node, int gbd,
						    int p, std::shared_ptr<double []> opp_probs,
						    const HandTree *hand_tree,
						    const std::string &action_sequence);
  std::shared_ptr<CFRValues> Sumprobs(void) const {return sumprobs_;}
  void SetSumprobs(std::shared_ptr<CFRValues> &src) {sumprobs_ = src;}
  void SetRegrets(std::shared_ptr<CFRValues> &src) {regrets_ = src;}
  void ClearSumprobs(void) {sumprobs_.reset();}
  virtual void SetStreetBuckets(int st, int gbd, VCFRState *state);
  virtual void SetValueCalculation(bool b) {value_calculation_ = b;}
  virtual void SetBestResponseStreet(int st, bool b) {best_response_streets_[st] = b;}
  virtual void SetSplitStreet(int st) {split_street_ = st;}
  int It(void) const {return it_;}
  void SpawnWorkers(void);
  void IncrementNumDone(void);
  std::queue<Request> *GetRequestQueue(void) {return &request_queue_;}
  pthread_mutex_t *GetQueueMutex(void) {return &queue_mutex_;}
  pthread_mutex_t *GetNumDoneMutex(void) {return &num_done_mutex_;}
  pthread_cond_t *GetQueueNotEmpty(void) {return &queue_not_empty_;}
  pthread_cond_t *GetQueueNotFull(void) {return &queue_not_full_;}
 protected:
  static const int kRequestQueueMaxSize = 100;
  
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
  virtual std::shared_ptr<double []> OurChoice(Node *p0_node, Node *p1_node, int gbd,
					       VCFRState *state);
  virtual std::shared_ptr<double []> OppChoice(Node *p0_node, Node *p1_node, int gbd,
					       VCFRState *state);
  virtual void Split(Node *p0_node, Node *p1_node, int pgbd, VCFRState *state,
		     int *prev_canons, double *vals);
  virtual std::shared_ptr<double []> StreetInitial(Node *p0_node, Node *p1_node, int pgbd,
						   VCFRState *state);
  virtual void InitializeOppData(VCFRState *state, int st, int gbd);
  virtual std::shared_ptr<double []> Process(Node *p0_node, Node *p1_node, int gbd,
					     VCFRState *state, int last_st);
  virtual void SetCurrentStrategy(Node *node);
  
  const CardAbstraction &card_abstraction_;
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
  int split_street_;
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
  std::unique_ptr<std::unique_ptr<VCFRWorker> []> workers_;
  std::queue<Request> request_queue_;
  pthread_mutex_t queue_mutex_;
  pthread_mutex_t num_done_mutex_;
  pthread_cond_t queue_not_empty_;
  pthread_cond_t queue_not_full_;
  int num_done_;
};

class VCFRWorker {
public:
  VCFRWorker(VCFR *vcfr);
  ~VCFRWorker(void) {}
  void Reset(int pst);
  void HandleRequest(const Request &request);
  void MainLoop(void);
  void Run(void);
  void Join(void);
  double *Vals(void) const {return vals_.get();}
private:
  VCFR *vcfr_;
  std::unique_ptr<double []> vals_;
  pthread_t pthread_id_;
};

#endif
