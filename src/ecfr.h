#ifndef _ECFR_H_
#define _ECFR_H_

#include <memory>

class ECFRNode;
class ECFRThread;
class BettingAbstraction;
class Buckets;
class CardAbstraction;
class CFRConfig;
class Node;
class Reader;
class Writer;

class ECFRNode {
public:
  ECFRNode(void) {}
  ECFRNode(Node *node, const Buckets &buckets);
  ~ECFRNode(void) {}
  bool Terminal(void) const {return terminal_;}
  bool Showdown(void) const {return showdown_;}
  int Street(void) const {return st_;}
  int PlayerActing(void) const {return player_acting_;}
  int NumSuccs(void) const {return num_succs_;}
  int LastBetTo(void) const {return last_bet_to_;}
  ECFRNode *IthSucc(int i) const {return succs_[i].get();}
  double *Regrets(void) {return regrets_.get();}
  int *Sumprobs(void) {return sumprobs_.get();}
private:
  bool terminal_;
  bool showdown_;
  int st_;
  int player_acting_;
  int num_succs_;
  int last_bet_to_;
  std::unique_ptr<std::unique_ptr<ECFRNode> []> succs_;
  std::unique_ptr<double []> regrets_;
  std::unique_ptr<int []> sumprobs_;
};

class ECFRThread {
public:
  ECFRThread(const CFRConfig &cfr_config, const Buckets &buckets, ECFRNode *root, int seed,
	      int batch_size, const int *board_table, int num_raw_boards,
	      unsigned long long int *total_its, int thread_index, int num_threads);
  ~ECFRThread(void) {}
  void Run(void);
  void RunThread(void);
  void Join(void);
private:
  double Process(ECFRNode *node);
  void Deal(void);
  
  const Buckets &buckets_;
  ECFRNode *root_;
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

class ECFR {
public:
  ECFR(const CardAbstraction &ca, const BettingAbstraction &ba, const CFRConfig &cc,
	const Buckets &buckets, int num_threads);
  ~ECFR(void) {}
  void Run(int start_batch_index, int end_batch_index, int batch_size, int save_interval);
private:
  void ReadRegrets(ECFRNode *node, std::unique_ptr<Reader> *readers);
  void ReadSumprobs(ECFRNode *node, std::unique_ptr<Reader> *readers);
  void Read(int batch_index);
  void WriteRegrets(ECFRNode *node, std::unique_ptr<Writer> *writers);
  void WriteSumprobs(ECFRNode *node, std::unique_ptr<Writer> *writers);
  void Write(int batch_index);
  void Run(void);
  void RunBatch(int batch_index, int batch_size);

  const CardAbstraction &card_abstraction_;
  const BettingAbstraction &betting_abstraction_;
  const CFRConfig &cfr_config_;
  const Buckets &buckets_;
  std::unique_ptr<ECFRNode> root_;
  std::unique_ptr<int []> board_table_;
  int num_raw_boards_;
  struct drand48_data rand_buf_;
  int num_cfr_threads_;
  std::unique_ptr<std::unique_ptr<ECFRThread> []> cfr_threads_;
  unsigned long long int total_its_;
};

#endif
