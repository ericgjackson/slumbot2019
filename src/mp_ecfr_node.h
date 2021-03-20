#ifndef _MP_ECFR_NODE_H_
#define _MP_ECFR_NODE_H_

#include <memory>
#include <vector>

class Buckets;
class Node;

class MPECFRNode {
public:
  MPECFRNode(void) {}
  MPECFRNode(Node *node, const Buckets &buckets, bool *folded, bool *active, int *contributions,
	     int *stack_sizes);
  ~MPECFRNode(void) {}
  bool Terminal(void) const {return terminal_;}
  bool Showdown(void) const {return showdown_;}
  int Street(void) const {return st_;}
  int PlayerActing(void) const {return player_acting_;}
  int PlayerRemaining(void) const {return player_remaining_;}
  int NumSuccs(void) const {return num_succs_;}
  int LastBetTo(void) const {return last_bet_to_;}
  MPECFRNode *IthSucc(int i) const {return succs_[i].get();}
  double *Regrets(void) {return regrets_.get();}
  int *Sumprobs(void) {return sumprobs_.get();}
private:
  void SetShowdownPots(bool *folded, bool *active, int *contributions, int *stack_sizes);
  
  bool terminal_;
  bool showdown_;
  int st_;
  int player_acting_;
  int player_remaining_;
  int num_succs_;
  int last_bet_to_;
  std::unique_ptr<std::unique_ptr<MPECFRNode> []> succs_;
  std::unique_ptr<double []> regrets_;
  std::unique_ptr<int []> sumprobs_;
};

#endif
