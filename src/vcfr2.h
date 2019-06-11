#ifndef _VCFR2_H_
#define _VCFR2_H_

#include <memory>
#include <queue>

#include "vcfr.h"

class BettingAbstraction;
class Buckets;
class CardAbstraction;
class CFRConfig;
class Node;
class VCFRState;

class VCFR2Worker;

class VCFR2 : public VCFR {
public:
  VCFR2(const CardAbstraction &ca, const BettingAbstraction &ba, const CFRConfig &cc,
	const Buckets &buckets, int num_threads);
  ~VCFR2(void);
  void Split(Node *p0_node, Node *p1_node, int pgbd, const VCFRState &state, int *prev_canons,
	     double *vals);
  VCFR2Worker *SpawnWorker(void);
  void SpawnWorkers(void);
  void IncrementNumDone(void);
  std::queue<Request> *GetRequestQueue(void) {return &request_queue_;}
  pthread_mutex_t *GetQueueMutex(void) {return &queue_mutex_;}
  pthread_mutex_t *GetNumDoneMutex(void) {return &num_done_mutex_;}
  pthread_cond_t *GetQueueNotEmpty(void) {return &queue_not_empty_;}
  pthread_cond_t *GetQueueNotFull(void) {return &queue_not_full_;}
private:
  static const int kRequestQueueMaxSize = 100;
  
  std::unique_ptr<std::unique_ptr<VCFR2Worker> []> workers_;
  std::queue<Request> request_queue_;
  pthread_mutex_t queue_mutex_;
  pthread_mutex_t num_done_mutex_;
  pthread_cond_t queue_not_empty_;
  pthread_cond_t queue_not_full_;
  int num_done_;
};

class VCFR2Worker {
public:
  VCFR2Worker(VCFR2 *vcfr2);
  ~VCFR2Worker(void);
  void Reset(int pst);
  void HandleRequest(const Request &request);
  void MainLoop(void);
  void Run(void);
  void Join(void);
  double *Vals(void) const {return vals_.get();}
private:
  VCFR2 *vcfr2_;
  std::unique_ptr<double []> vals_;
  pthread_t pthread_id_;
};

#endif
