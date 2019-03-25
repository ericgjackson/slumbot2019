#ifndef _CFRP_H_
#define _CFRP_H_

#include <semaphore.h>

#include <string>

#include "cfrp_subgame.h"
#include "vcfr.h"

class BettingTree;
class Buckets;
class CanonicalCards;
class CFRConfig;
class CFRPSubgame;
class HandTree;
class Node;
class Reader;
class Writer;

class CFRP : public VCFR {
public:
  CFRP(const CardAbstraction &ca, const BettingAbstraction &ba, const CFRConfig &cc,
       const Buckets &buckets, const BettingTree *betting_tree, int num_threads, int target_p);
  virtual ~CFRP(void);
  void Initialize(void);
  void Run(int start_it, int end_it);
  void Post(int t);
 protected:
  void WaitForFinalSubgames(void);
  void FloorRegrets(Node *node);
  void SpawnSubgame(Node *node, int bd, const std::string &action_sequence, double *opp_probs);
  void HalfIteration(int p);
  void Checkpoint(int it);
  void ReadFromCheckpoint(int it);

  const BettingTree *betting_tree_;
  const HandTree *hand_tree_;
  int target_p_;
  bool *compressed_streets_;
  bool bucketed_;
  int last_checkpoint_it_;
  double ****final_vals_;
  bool *subgame_running_;
  pthread_t *pthread_ids_;
  CFRPSubgame **active_subgames_;
  sem_t available_;
};

#endif
