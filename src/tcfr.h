#ifndef _TCFR_H_
#define _TCFR_H_

#include <memory>

// #include "cfr.h"

using namespace std;

class BettingAbstraction;
class BettingTree;
class Buckets;
class CardAbstraction;
class CFRConfig;
class Node;
class Reader;
class Writer;

#define T_REGRET unsigned int
#define T_VALUE int
#define T_SUM_PROB unsigned int

#define SUCCPTR(ptr) (ptr + 8)

static const int kNumPregenRNGs = 10000000;

class TCFRThread {
public:
  TCFRThread(const BettingAbstraction &ba, const CFRConfig &cc, const Buckets &buckets,
	     int batch_index, int thread_index, int num_threads, unsigned char *data,
	     int target_player, float *rngs, unsigned int *uncompress,
	     unsigned int *short_uncompress, unsigned int *pruning_thresholds,
	     bool **sumprob_streets, const double *boost_thresholds, const bool *freeze,
	     unsigned char *hvb_table, unsigned char ***cards_to_indices, int num_raw_boards,
	     const int *board_table_, int batch_size, unsigned long long int *total_its);
  virtual ~TCFRThread(void);
  void RunThread(void);
  void Join(void);
  void Run(void);
  int ThreadIndex(void) const {return thread_index_;}
  unsigned long long int ProcessCount(void) const {return process_count_;}
  unsigned long long int FullProcessCount(void) const {
    return full_process_count_;
  }
 protected:
  static const int kStackDepth = 500;
  static const int kMaxSuccs = 50;

  virtual T_VALUE Process(unsigned char *ptr, int last_player_acting, int last_st);
  void HVBDealHand(void);
  void NoHVBDealHand(void);
  int Round(double d);

  const BettingAbstraction &betting_abstraction_;
  const CFRConfig &cfr_config_;
  const Buckets &buckets_;
  int batch_index_;
  int thread_index_;
  int num_threads_;
  unsigned char *data_;
  bool asymmetric_;
  int num_players_;
  int target_player_;
  int p_;
  int *winners_;
  // Keep this as a signed int so we can use it in winnings calculation
  // without casting.
  int *contributions_;
  bool *folded_;
  int *canon_bds_;
  int *hi_cards_;
  int *lo_cards_;
  int *hole_cards_;
  int *hvs_;
  int *hand_buckets_;
  pthread_t pthread_id_;
  T_VALUE **succ_value_stack_;
  int **succ_iregret_stack_;
  int stack_index_;
  double explore_;
  unsigned int *sumprob_ceilings_;
  unsigned long long int it_;
  float *rngs_;
  int rng_index_;
  unique_ptr<bool []> char_quantized_streets_;
  unique_ptr<bool []> short_quantized_streets_;
  bool *scaled_streets_;
  bool full_only_avg_update_;
  unsigned int *uncompress_;
  unsigned int *short_uncompress_;
  unsigned int *pruning_thresholds_;
  bool **sumprob_streets_;
  const double *boost_thresholds_;
  const bool *freeze_;
  unsigned char *hvb_table_;
  unsigned long long int bytes_per_hand_;
  unsigned char ***cards_to_indices_;
  int num_raw_boards_;
  const int *board_table_;
  int max_street_;
  bool all_full_;
  bool *full_;
  unique_ptr<unsigned int []> close_thresholds_;
  unsigned long long int process_count_;
  unsigned long long int full_process_count_;
  int active_mod_;
  int num_active_conditions_;
  int *num_active_streets_;
  int *num_active_rems_;
  int **active_streets_;
  int **active_rems_;
  int batch_size_;
  unsigned long long int *total_its_;
  struct drand48_data rand_buf_;
  // Keep this as a signed int so we can use it in winnings calculation
  // without casting.
  int board_count_;
  bool deal_twice_;
  int **force_regrets_;
};

class TCFR {
public:
  TCFR(const CardAbstraction &ca, const BettingAbstraction &ba, const CFRConfig &cc,
       const Buckets &buckets, int num_threads, int target_player);
  ~TCFR(void);
  void Run(int start_batch_index, int end_batch_index, int batch_size, int save_interval);
private:
  void ReadRegrets(unsigned char *ptr, Node *node, Reader ***readers, bool ***seen);
  void WriteRegrets(unsigned char *ptr, Node *node, Writer ***writers, bool ***seen);
  void ReadSumprobs(unsigned char *ptr, Node *node, Reader ***readers, bool ***seen);
  void WriteSumprobs(unsigned char *ptr, Node *node, Writer ***writers, bool ***seen);
  void Read(int batch_index);
  void Write(int batch_index);
  void Run(void);
  void RunBatch(int batch_size);
  unsigned char *Prepare(unsigned char *ptr, Node *node, unsigned short last_bet_to,
			 unsigned long long int ***offsets);
  void MeasureTree(Node *node, bool ***seen, unsigned long long int *allocation_size);
  void Prepare(void);

  const CardAbstraction &card_abstraction_;
  const BettingAbstraction &betting_abstraction_;
  const CFRConfig &cfr_config_;
  const Buckets &buckets_;
  unique_ptr<BettingTree> betting_tree_;
  bool asymmetric_;
  unique_ptr<double []> boost_thresholds_;
  unique_ptr<bool []> freeze_;
  int num_players_;
  int target_player_;
  unsigned char *data_;
  int batch_index_;
  int num_cfr_threads_;
  TCFRThread **cfr_threads_;
  float *rngs_;
  unsigned int *uncompress_;
  unsigned int *short_uncompress_;
  int max_street_;
  unsigned int *pruning_thresholds_;
  bool **sumprob_streets_;
  unique_ptr<bool []> char_quantized_streets_;
  unique_ptr<bool []> short_quantized_streets_;
  unsigned char *hvb_table_;
  unsigned char ***cards_to_indices_;
  int num_raw_boards_;
  unique_ptr<int []> board_table_;
  unsigned long long int total_process_count_;
  unsigned long long int total_full_process_count_;
  unsigned long long int total_its_;
};

#endif
