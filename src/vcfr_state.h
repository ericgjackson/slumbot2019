#ifndef _VCFR_STATE_H_
#define _VCFR_STATE_H_

#include <memory>
#include <string>

#include "hand_tree.h"

class CanonicalCards;

class VCFRState {
 public:
  VCFRState(int p, const HandTree *hand_tree);
  VCFRState(int p, const std::shared_ptr<double []> &opp_probs, const HandTree *hand_tree, 
	    const std::string &action_sequence);
  VCFRState(const VCFRState &pred, Node *node, int s);
  VCFRState(const VCFRState &pred, Node *node, int s, const std::shared_ptr<double []> &opp_probs);
  virtual ~VCFRState(void) {}
  int P(void) const {return p_;}
  std::shared_ptr<double []> OppProbs(void) const {return opp_probs_;}
  double SumOppProbs(void) const {return sum_opp_probs_;}
  void SetSumOppProbs(double s) {sum_opp_probs_ = s;}
  void AllocateTotalCardProbs(void);
  std::shared_ptr<double []> TotalCardProbs(void) const {return total_card_probs_;}
  int *StreetBuckets(int st) const;
  const std::shared_ptr<int []> AllStreetBuckets(void) const {return street_buckets_;}
  const std::string &ActionSequence(void) const {return action_sequence_;}
  const HandTree *GetHandTree(void) const {return hand_tree_;}
  int RootSt(void) const {return hand_tree_->RootSt();}
  int RootBd(void) const {return hand_tree_->RootBd();}
  int LocalBoardIndex(int st, int gbd) const {return hand_tree_->LocalBoardIndex(st, gbd);}
  const CanonicalCards *Hands(int st, int gbd) const {
    return hand_tree_->Hands(st, gbd);
  }
  void SetOppProbs(const std::shared_ptr<double []> &opp_probs) {opp_probs_ = opp_probs;}
 protected:
  int p_;
  std::shared_ptr<double []> opp_probs_;
  double sum_opp_probs_;
  std::shared_ptr<double []> total_card_probs_;
  std::shared_ptr<int []> street_buckets_;
  std::string action_sequence_;
  const HandTree *hand_tree_;
};

#endif
