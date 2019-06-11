#ifndef _BETTING_TREE_BUILDER_H_
#define _BETTING_TREE_BUILDER_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class BettingAbstraction;
class Node;
class Writer;

class BettingTreeBuilder {
public:
  BettingTreeBuilder(const BettingAbstraction &ba);
  BettingTreeBuilder(const BettingAbstraction &ba, int target_player);
  void Build(void);
  void Write(void);
  // std::shared_ptr<Node> CreateLimitTree(int *terminal_id);
  std::shared_ptr<Node>
    CreateNoLimitTree1(int target_player, int *terminal_id);
  // Public so we can call in subgame solving; e.g., from solve_all_subgames
  std::shared_ptr<Node> CreateNoLimitSubtree(int st, int last_bet_size, int bet_to,
					     int num_street_bets, int player_acting,
					     int target_player, int *terminal_id);
  
  std::shared_ptr<Node> CreateMPFoldSucc(int street, int last_bet_size,
				    int bet_to, int num_street_bets,
				    int num_bets, int player_acting,
				    int num_players_to_act, bool *folded,
				    int target_player, std::string *key,
				    int *terminal_id);
  std::shared_ptr<Node> CreateMPCallSucc(int street, int last_bet_size,
				    int bet_to, int num_street_bets,
				    int num_bets, int player_acting,
				    int num_players_to_act, bool *folded,
				    int target_player, std::string *key,
				    int *terminal_id);
  void MPHandleBet(int street, int last_bet_size, int last_bet_to,
		   int new_bet_to, int num_street_bets, int num_bets,
		   int player_acting, int num_players_to_act, bool *folded,
		   int target_player, std::string *key, int *terminal_id,
		   std::vector< std::shared_ptr<Node> > *bet_succs);
  void CreateMPSuccs(int street, int last_bet_size, int bet_to,
		     int num_street_bets, int num_bets,
		     int player_acting, int num_players_to_act, bool *folded,
		     int target_player, std::string *key, int *terminal_id,
		     std::shared_ptr<Node> *call_succ, std::shared_ptr<Node> *fold_succ,
		     std::vector< std::shared_ptr<Node> > *bet_succs);
  std::shared_ptr<Node> CreateMPSubtree(int st, int last_bet_size, int bet_to, int num_street_bets,
					int num_bets, int player_acting, int num_players_to_act,
					bool *folded, int target_player, std::string *key,
					int *terminal_id);
  std::shared_ptr<Node> CreateMPStreet(int street, int bet_to, int num_bets,
				  bool *folded, int target_player, std::string *key,
				  int *terminal_id);
  std::shared_ptr<Node> CreateMPTree(int target_player, int *terminal_id);
  
private:
  bool FindReentrantNode(const std::string &key, std::shared_ptr<Node> *node);
  void AddReentrantNode(const std::string &key, std::shared_ptr<Node> node);
  int NearestAllowableBetTo(int old_pot_size, int new_bet_to, int last_bet_size);
  void GetNewBetTos(int old_bet_to, int last_bet_size, const std::vector<double> &pot_fracs,
		    int player_acting, int target_player, bool *bet_to_seen);
  void HandleBet(int street, int last_bet_size, int last_bet_to, int new_bet_to,
		 int num_street_bets, int player_acting, int target_player, int *terminal_id,
		 std::vector< std::shared_ptr<Node> > *bet_succs);
  std::shared_ptr<Node>
    CreateCallSucc(int street, int last_bet_size, int bet_to, int num_street_bets,
		   int player_acting, int target_player, int *terminal_id);
  std::shared_ptr<Node>
    CreateFoldSucc(int street, int last_bet_size, int bet_to, int player_acting, int *terminal_id);
  void CreateNoLimitSuccs(int street, int last_bet_size, int bet_to, int num_street_bets,
			  int player_acting, int target_player, int *terminal_id,
			  std::shared_ptr<Node> *call_succ, std::shared_ptr<Node> *fold_succ,
			  std::vector< std::shared_ptr<Node> > *bet_succs);
  void CreateLimitSuccs(int street, int pot_size, int last_bet_size, int num_bets, int last_bettor,
			int player_acting, int *terminal_id, std::shared_ptr<Node> *call_succ,
			std::shared_ptr<Node> *fold_succ,
			std::vector< std::shared_ptr<Node> > *bet_succs);
  std::shared_ptr<Node>
    CreateLimitSubtree(int street, int pot_size, int last_bet_size, int num_bets, int last_bettor,
		       int player_acting, int *terminal_id);

  void GetNewPotSizes(int old_pot_size, const std::vector<int> &bet_amounts, int player_acting,
		      int target_player, std::vector<int> *new_pot_sizes);
  void Initialize(void);
  void Write(Node *node, std::vector< std::vector<int> > *num_nonterminals, Writer *writer);

  const BettingAbstraction &betting_abstraction_;
  bool asymmetric_;
  int target_player_;
  int initial_street_;
  int stack_size_;
  int all_in_pot_size_;
  int min_bet_;
  // Pool *pool_;
  std::shared_ptr<Node> root_;
  int num_terminals_;
  // For reentrant trees
  std::unique_ptr< std::unordered_map< unsigned long long int, std::shared_ptr<Node> > >
    node_map_;
};

#endif
