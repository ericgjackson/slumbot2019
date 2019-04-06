#ifndef _CFRP_SUBGAME_H_
#define _CFRP_SUBGAME_H_

#include <string>

#include "cfrp.h"
#include "vcfr.h"

class BettingAbstraction;
class BettingTree;
class Buckets;
class CardAbstraction;
class CFRP;
class HandTree;
class Node;

class CFRPSubgame : public VCFR {
public:
  CFRPSubgame(const CardAbstraction &ca, const BettingAbstraction &ba,
	      const CFRConfig &cc, const Buckets &buckets, Node *root,
	      int root_bd, const std::string &action_sequence, CFRP *cfr);
  virtual ~CFRPSubgame(void);
  void Go(void);
  Node *Root(void) const {return root_;}
  int RootBd(void) const {return root_bd_;}
  double *FinalVals(void) const {return final_vals_;}

  void SetOppProbs(const std::shared_ptr<double []> &opp_probs);
  void SetThreadIndex(int t) {thread_index_ = t;}
  void SetIt(int it) {it_ = it;}
  void SetLastCheckpointIt(int it) {last_checkpoint_it_ = it;}
  void SetP(int p) {p_ = p;}
  void SetTargetP(int p) {target_p_ = p;}
  void SetBestResponseStreets(bool *sts);
  void SetBRCurrent(bool b) {br_current_ = b;}
  void SetValueCalculation(bool b) {value_calculation_ = b;}
 private:
  void DeleteOldFiles(int it);

  const std::string &action_sequence_;
  Node *root_;
  int root_bd_;
  int root_bd_st_;
  BettingTree *subtree_;
  CFRP *cfr_;
  bool *subtree_streets_;
  std::shared_ptr<double []> opp_probs_;
  const HandTree *hand_tree_;
  int thread_index_;
  double *final_vals_;
  int last_checkpoint_it_;
  int target_p_;
};

#endif
