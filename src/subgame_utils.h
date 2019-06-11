#ifndef _SUBGAME_UTILS_H_
#define _SUBGAME_UTILS_H_

#include <memory>
#include <string>

#include "resolving_method.h"

class BettingAbstraction;
class BettingTree;
class BettingTrees;
class Buckets;
class CardAbstraction;
class CFRConfig;
class CFRValues;
class HandTree;
class Node;
class ReachProbs;

BettingTrees *CreateSubtrees(int st, int player_acting, int last_bet_to, int target_p,
			     const BettingAbstraction &betting_abstraction);
BettingTrees *CreateSubtrees(int st, int last_bet_size, int last_bet_to, int num_street_bets,
			     int num_bets, int player_acting, int num_players_to_act,
			     int target_player, const BettingAbstraction &betting_abstraction);
BettingTrees *BuildHybridTree(const BettingAbstraction &betting_abstraction, int target_p,
			      Node *source, int last_bet_size, int num_street_bets, int num_bets);
std::unique_ptr<CFRValues>
ReadBaseSubgameStrategy(const CardAbstraction &base_card_abstraction,
			const BettingAbstraction &base_betting_abstraction,
			const CFRConfig &base_cfr_config,
			const BettingTrees *base_betting_trees,
			const Buckets &base_buckets, const Buckets &subgame_buckets,
			int base_it, Node *base_node, int gbd,
			const std::string &action_sequence, double **reach_probs,
			BettingTree *subtree, bool current, int target_p);
void WriteSubgame(Node *node, const std::string &action_sequence,
		  const std::string &below_action_sequence, int gbd,
		  const CardAbstraction &base_card_abstraction,
		  const CardAbstraction &subgame_card_abstraction,
		  const BettingAbstraction &base_betting_abstraction,
		  const BettingAbstraction &subgame_betting_abstraction,
		  const CFRConfig &base_cfr_config, const CFRConfig &subgame_cfr_config,
		  ResolvingMethod method, const CFRValues *sumprobs, int root_bd_st, int root_bd,
		  int target_p, int cfr_target_p, int last_st);
std::unique_ptr<CFRValues>
ReadSubgame(const std::string &action_sequence, BettingTrees *subtrees, int gbd,
	    const CardAbstraction &base_card_abstraction,
	    const CardAbstraction &subgame_card_abstraction,
	    const BettingAbstraction &base_betting_abstraction,
	    const BettingAbstraction &subgame_betting_abstraction,
	    const CFRConfig &base_cfr_config, const CFRConfig &subgame_cfr_config,
	    const Buckets &subgame_buckets, ResolvingMethod method, int root_bd_st,
	    int root_bd, int target_p);
void DeleteAllSubgames(const CardAbstraction &base_card_abstraction,
		       const CardAbstraction &subgame_card_abstraction,
		       const BettingAbstraction &base_betting_abstraction,
		       const BettingAbstraction &subgame_betting_abstraction,
		       const CFRConfig &base_cfr_config, const CFRConfig &subgame_cfr_config,
		       ResolvingMethod method, int target_p);
void FloorCVs(Node *subtree_root, double *opp_reach_probs, const CanonicalCards *hands,
	      double *cvs);
void ZeroSumCVs(double *p0_cvs, double *p1_cvs, int num_hole_card_pairs,
		const ReachProbs &reach_probs, const CanonicalCards *hands);

#endif
