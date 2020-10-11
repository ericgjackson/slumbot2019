#ifndef _AGENT_H_
#define _AGENT_H_

#include "betting_trees.h"
#include "buckets.h"
#include "cards.h"
#include "disk_probs.h"
#include "dynamic_cbr.h"
#include "eg_cfr.h"
#include "match_state.h"

class BettingAbstraction;
class Buckets;
class CardAbstraction;
class CFRConfig;
class CFRValues;
class EGCFR;
class Node;

class Agent {
public:
  Agent(const CardAbstraction &ca, const BettingAbstraction &ba, const CFRConfig &cc, int it,
	int big_blind, int seed);
  Agent(const CardAbstraction &ca, const BettingAbstraction &ba, const CFRConfig &cc,
	const CardAbstraction *subgame_ca, const BettingAbstraction *subgame_ba,
	const CFRConfig *subgame_cc, int it, int big_blind, int resolve_st, int seed);
  ~Agent(void) {}
  bool ProcessMatchState(const MatchState &match_state, CFRValues **resolved_strategy, bool *call,
			 bool *fold, int *bet_size);
  int BigBlind(void) const {return big_blind_;}
  int StackSize(void) const {return stack_size_;}
private:
  void Initialize(const CardAbstraction &ca, const BettingAbstraction &ba, const CFRConfig &cc,
		  int it, int big_blind, int seed);
  void SetBuckets(int st, const Card *raw_board, const Card *raw_hole_cards, int *buckets);
  Node *ChooseOurActionWithRaiseNode(Node *node, Node *raise_node, const int *buckets,
				     bool mapped_bet_to_closing_call);
  Node *ChooseOurAction(Node *node, const int *buckets, bool mapped_bet_to_closing_call);
  Node *ChooseOurAction(Node *node, Node *raise_node, const int *buckets,
			bool mapped_bet_to_closing_call);
  void GetTwoClosestSuccs(Node *node, int actual_bet_to, int *below_succ, int *below_bet_to,
			  int *above_succ, int *above_bet_to);
  double BelowProb(int actual_bet_to, int below_bet_to, int above_bet_to, int actual_pot_size);
  int ChooseBetweenBetAndCall(Node *node, int below_succ, int above_succ, int actual_bet_to,
			      int actual_pot_size, Node **raise_node);
  int ChooseBetweenTwoBets(Node *node, int below_succ, int above_succ, int actual_bet_to,
			   int actual_pot_size);
  int ChooseOppAction(Node *node, int below_succ, int above_succ, int actual_bet_to,
		      int actual_pot_size, Node **raise_node);
  Node *ProcessAction(const std::string &action, int we_p, const int *buckets, Node **raise_node,
		      bool *mapped_bet_to_closing_call);
  void ReadSumprobsFromDisk(Node *node, int p, const Card *board, CFRValues *values);
  CFRValues *ReadSumprobs(Node *node, int p, const Card *board);
  CFRValues *Resolve(const Card *board, int we_p, Node *node);

  const BettingAbstraction *subgame_ba_;
  struct drand48_data rand_buf_;
  int small_blind_;
  int big_blind_;
  int stack_size_;
  int resolve_st_;
  int translation_method_;
  std::unique_ptr<int []> boards_;
  std::unique_ptr<Buckets> buckets_;
  std::unique_ptr<BettingTrees> betting_trees_;
  std::unique_ptr<DiskProbs> disk_probs_;
  std::unique_ptr<DynamicCBR> dynamic_cbr_;
  std::unique_ptr<Buckets> subgame_buckets;
  std::unique_ptr<EGCFR> eg_cfr;
};

#endif
