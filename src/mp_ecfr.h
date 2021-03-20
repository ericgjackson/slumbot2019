#ifndef _MP_ECFR_H_
#define _MP_ECFR_H_

#include <memory>

#include "mp_ecfr_node.h"

class MPECFRThread;
class BettingAbstraction;
class Buckets;
class CardAbstraction;
class CFRConfig;
class Node;
class Reader;
class Writer;

class MPECFRThread {
public:
  MPECFRThread(const CFRConfig &cfr_config, const Buckets &buckets, MPECFRNode *root, int seed,
	      int batch_size, const int *board_table, int num_raw_boards,
	      unsigned long long int *total_its, int thread_index, int num_threads);
  ~MPECFRThread(void) {}
  void Run(void);
  void RunThread(void);
  void Join(void);
private:
  double Process(MPECFRNode *node);
  void Deal(void);
  
  const Buckets &buckets_;
  MPECFRNode *root_;
  int batch_size_;
  const int *board_table_;
  int num_raw_boards_;
  unsigned long long int *total_its_;
  int thread_index_;
  int num_threads_;
  std::unique_ptr<int []> canon_bds_;
  std::unique_ptr<int []> hole_cards_;
  std::unique_ptr<int []> hi_cards_;
  std::unique_ptr<int []> lo_cards_;
  std::unique_ptr<int []> hvs_;
  std::unique_ptr<int []> hand_buckets_;
  struct drand48_data rand_buf_;
  int it_;
  int p_;
  int p1_outcome_;
  pthread_t pthread_id_;
};

class MPECFR {
public:
  MPECFR(const CardAbstraction &ca, const BettingAbstraction &ba, const CFRConfig &cc,
	const Buckets &buckets, int num_threads);
  ~MPECFR(void) {}
  void Run(int start_batch_index, int end_batch_index, int batch_size, int save_interval);
private:
  void ReadRegrets(MPECFRNode *node, std::unique_ptr<Reader> *readers);
  void ReadSumprobs(MPECFRNode *node, std::unique_ptr<Reader> *readers);
  void Read(int batch_index);
  void WriteRegrets(MPECFRNode *node, std::unique_ptr<Writer> *writers);
  void WriteSumprobs(MPECFRNode *node, std::unique_ptr<Writer> *writers);
  void Write(int batch_index);
  void Run(void);
  void RunBatch(int batch_index, int batch_size);

  const CardAbstraction &card_abstraction_;
  const BettingAbstraction &betting_abstraction_;
  const CFRConfig &cfr_config_;
  const Buckets &buckets_;
  std::unique_ptr<MPECFRNode> root_;
  std::unique_ptr<int []> board_table_;
  int num_raw_boards_;
  struct drand48_data rand_buf_;
  int num_cfr_threads_;
  std::unique_ptr<std::unique_ptr<MPECFRThread> []> cfr_threads_;
  unsigned long long int total_its_;
};

#endif
