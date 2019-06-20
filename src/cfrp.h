#ifndef _CFRP_H_
#define _CFRP_H_

#include <semaphore.h>

#include <memory>
#include <string>

// #include "cfrp_subgame.h"
#include "hand_tree.h"
#include "vcfr.h"

class BettingAbstraction;
class BettingTrees;
class Buckets;
class CanonicalCards;
class CFRConfig;
// class CFRPSubgame;
class Node;
class Reader;
class Writer;

class CFRP : public VCFR {
public:
  CFRP(const CardAbstraction &ca, const CFRConfig &cc, const Buckets &buckets, int num_threads);
  virtual ~CFRP(void) {}
  void Initialize(const BettingAbstraction &ba, int target_p);
  void Run(int start_it, int end_it);
  // void Post(int t);
 protected:
#if 0
  void WaitForFinalSubgames(void);
  void SpawnSubgame(Node *node, int bd, const std::string &action_sequence, int p,
		    const std::shared_ptr<double []> &opp_probs);
#endif
  void FloorRegrets(Node *node, int p);
  void HalfIteration(int p);
  void Checkpoint(int it);
  void ReadFromCheckpoint(int it);

  bool asymmetric_;
  std::string betting_abstraction_name_;
  std::unique_ptr<BettingTrees> betting_trees_;
  std::unique_ptr<HandTree> hand_tree_;
  int target_p_;
  bool *compressed_streets_;
  bool bucketed_;
  int last_checkpoint_it_;
  // std::shared_ptr<double []> ***final_vals_;
  // bool *subgame_running_;
  // pthread_t *pthread_ids_;
  // CFRPSubgame **active_subgames_;
  // sem_t available_;
};

#endif
