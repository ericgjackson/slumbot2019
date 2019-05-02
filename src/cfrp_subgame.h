#ifndef _CFRP_SUBGAME_H_
#define _CFRP_SUBGAME_H_

#include <memory>
#include <string>

#include "betting_trees.h"
#include "cfrp.h"
#include "vcfr.h"

class BettingAbstraction;
class BettingTrees;
class Buckets;
class CardAbstraction;
class CFRP;
class HandTree;
class Node;

class CFRPSubgame : public VCFR {
public:
  CFRPSubgame(const CardAbstraction &ca, const BettingAbstraction &ba, const CFRConfig &cc,
	      const Buckets &buckets, Node *root, int root_bd, const std::string &action_sequence,
	      int p, CFRP *cfr);
  virtual ~CFRPSubgame(void) {}
  void Go(void);
  Node *Root(void) const {return root_;}
  int RootBd(void) const {return root_bd_;}
  std::shared_ptr<double []> FinalVals(void) const {return final_vals_;}

  void SetOppProbs(const std::shared_ptr<double []> &opp_probs);
  void SetThreadIndex(int t) {thread_index_ = t;}
  void SetIt(int it) {it_ = it;}
  void SetLastCheckpointIt(int it) {last_checkpoint_it_ = it;}
  void SetTargetP(int p) {target_p_ = p;}
  void SetBestResponseStreets(const bool *sts);
  void SetBRCurrent(bool b) {br_current_ = b;}
  void SetValueCalculation(bool b) {value_calculation_ = b;}
 private:
  void DeleteOldFiles(int it);

  const std::string &action_sequence_;
  Node *root_;
  int root_bd_;
  int root_bd_st_;
  std::unique_ptr<BettingTrees> subtrees_;
  int p_;
  CFRP *cfr_;
  std::unique_ptr<bool []> subtree_streets_;
  std::shared_ptr<double []> opp_probs_;
  std::unique_ptr<const HandTree> hand_tree_;
  int thread_index_;
  std::shared_ptr<double []> final_vals_;
  int last_checkpoint_it_;
  int target_p_;
};

#endif
