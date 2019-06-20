// Not finished yet.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <memory>
#include <string>
#include <vector>

#include "betting_abstraction.h"
#include "betting_abstraction_params.h"
#include "betting_tree_builder.h"
#include "betting_tree.h"
#include "betting_trees.h"
#include "board_tree.h"
#include "buckets.h"
#include "canonical_cards.h"
#include "card_abstraction.h"
#include "card_abstraction_params.h"
#include "cfr_config.h"
#include "cfr_params.h"
#include "cfr_utils.h"
#include "cfrd_eg_cfr.h"
#include "combined_eg_cfr.h"
#include "dynamic_cbr.h"
#include "eg_cfr.h"
#include "files.h"
#include "game.h"
#include "game_params.h"
#include "hand_tree.h"
#include "hand_value_tree.h"
#include "io.h"
#include "params.h"
#include "reach_probs.h"
#include "split.h"
#include "subgame_utils.h" // CreateSubtrees()
#include "unsafe_eg_cfr.h"
#include "vcfr.h"

using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;

class SubgameSolver {
public:
  SubgameSolver(const CardAbstraction &base_card_abstraction,
		const CardAbstraction &subgame_card_abstraction,
		const BettingAbstraction &base_betting_abstraction,
		const BettingAbstraction &subgame_betting_abstraction,
		const CFRConfig &base_cfr_config, const CFRConfig &subgame_cfr_config,
		const Buckets &base_buckets, const Buckets &subgame_buckets,
		const string &target_action_sequence, int target_bd, ResolvingMethod method,
		bool cfrs, bool card_level, bool zero_sum, bool current, bool quantize,
		const bool *pure_streets, bool base_mem, int base_it, int num_subgame_its,
		int num_threads);
  ~SubgameSolver(void) {}
  void Walk(Node *node, const string &action_sequence, const ReachProbs &reach_probs);
  void Walk(void);
private:
  BettingTrees *CreateSubtrees(Node *node, int target_p, bool base);
  void ResolveUnsafe(Node *node, const string &action_sequence, const ReachProbs &reach_probs);
  void ResolveSafe(Node *node, const string &action_sequence, const ReachProbs &reach_probs);

  const CardAbstraction &base_card_abstraction_;
  const CardAbstraction &subgame_card_abstraction_;
  const BettingAbstraction &base_betting_abstraction_;
  const BettingAbstraction &subgame_betting_abstraction_;
  const CFRConfig &base_cfr_config_;
  const CFRConfig &subgame_cfr_config_;
  const Buckets &base_buckets_;
  const Buckets &subgame_buckets_;
  unique_ptr<BettingTrees> base_betting_trees_;
  unique_ptr<HandTree> trunk_hand_tree_;
  shared_ptr<CFRValues> trunk_sumprobs_;
  unique_ptr<DynamicCBR> dynamic_cbr_;
  const string &target_action_sequence_;
  int target_bd_;
  int solve_st_;
  ResolvingMethod method_;
  bool cfrs_;
  bool card_level_;
  bool zero_sum_;
  bool current_;
  const bool *pure_streets_;
  bool base_mem_;
  int base_it_;
  int num_subgame_its_;
  int num_threads_;
  unique_ptr<int []> pred_boards_;
};

// Don't expect initial 'x'
static int StreetFromAction(const char *action, int st, bool initial) {
  if (action[0] == 0) {
    if (! initial) {
      fprintf(stderr, "Bad action: %s\n", action);
      exit(-1);
    }
    return st;
  }
  if (action[0] == 'c') {
    if (initial) {
      // Street-initial check/call
      return StreetFromAction(action + 1, st, false);
    } else {
      // Street-closing check/call
      return StreetFromAction(action + 1, st + 1, true);
    }
  } else if (action[0] == 'b') {
    int i = 1;
    while (action[i] != 0 && action[i] >= '0' && action[i] <= '9') ++i;
    if (action[i] == 0) {
      fprintf(stderr, "Bad action: %s\n", action);
      exit(-1);
    }
    return StreetFromAction(action + i, st, false);
  } else {
    fprintf(stderr, "Bad action: %s\n", action);
    exit(-1);
  }
}

SubgameSolver::SubgameSolver(const CardAbstraction &base_card_abstraction,
			     const CardAbstraction &subgame_card_abstraction,
			     const BettingAbstraction &base_betting_abstraction,
			     const BettingAbstraction &subgame_betting_abstraction,
			     const CFRConfig &base_cfr_config, const CFRConfig &subgame_cfr_config,
			     const Buckets &base_buckets, const Buckets &subgame_buckets,
			     const string &target_action_sequence, int target_bd,
			     ResolvingMethod method, bool cfrs, bool card_level, bool zero_sum,
			     bool current, bool quantize, const bool *pure_streets, bool base_mem,
			     int base_it, int num_subgame_its, int num_threads) :
  base_card_abstraction_(base_card_abstraction),
  subgame_card_abstraction_(subgame_card_abstraction),
  base_betting_abstraction_(base_betting_abstraction),
  subgame_betting_abstraction_(subgame_betting_abstraction), base_cfr_config_(base_cfr_config),
  subgame_cfr_config_(subgame_cfr_config), base_buckets_(base_buckets),
  subgame_buckets_(subgame_buckets), target_action_sequence_(target_action_sequence),
  target_bd_(target_bd) {
  method_ = method;
  cfrs_ = cfrs;
  card_level_ = card_level;
  zero_sum_ = zero_sum;
  current_ = current;
  pure_streets_ = pure_streets;
  base_mem_ = base_mem;
  base_it_ = base_it;
  num_subgame_its_ = num_subgame_its;
  num_threads_ = num_threads;
  solve_st_ = StreetFromAction(target_action_sequence.c_str(), 0, true);
  fprintf(stderr, "solve_st_ %i\n", solve_st_);

  base_betting_trees_.reset(new BettingTrees(base_betting_abstraction_));

  int max_street = Game::MaxStreet();
  pred_boards_.reset(new int[solve_st_]);
  const Card *board = BoardTree::Board(solve_st_, target_bd_);
  for (int st = 0; st < solve_st_; ++st) {
    pred_boards_[st] = BoardTree::LookupBoard(board, st);
  }
  
  // We need probs for both players
  unique_ptr<bool []> trunk_streets(new bool[max_street + 1]);
  for (int st = 0; st <= max_street; ++st) {
    if (method == ResolvingMethod::UNSAFE) {
      // For unsafe method, don't need base probs outside trunk.
      trunk_streets[st] = st < solve_st_;
    } else {
      trunk_streets[st] = (base_mem_ && ! current_) || st < solve_st_;
    }
  }
  unique_ptr<bool []> compressed_streets(new bool[max_street + 1]);
  for (int st = 0; st <= max_street; ++st) compressed_streets[st] = false;
  const vector<int> &csv = base_cfr_config_.CompressedStreets();
  int num_csv = csv.size();
  for (int i = 0; i < num_csv; ++i) {
    int st = csv[i];
    compressed_streets[st] = true;
  }
  trunk_sumprobs_.reset(new CFRValues(nullptr, trunk_streets.get(), 0, 0, base_buckets_,
				      base_betting_trees_->GetBettingTree()));
  char dir[500];
  sprintf(dir, "%s/%s.%u.%s.%i.%i.%i.%s.%s", Files::OldCFRBase(), Game::GameName().c_str(),
	  Game::NumPlayers(), base_card_abstraction_.CardAbstractionName().c_str(),
	  Game::NumRanks(), Game::NumSuits(), Game::MaxStreet(),
	  base_betting_abstraction_.BettingAbstractionName().c_str(),
	  base_cfr_config_.CFRConfigName().c_str());
  if (base_betting_abstraction_.Asymmetric()) {
    // Maybe move trunk_sumprob initialization to inside of loop over
    // target players.
    fprintf(stderr, "Asymmetric not supported yet\n");
    exit(-1);
  }
#if 0
  // How do I handle this?
  if (base_betting_abstraction_.Asymmetric()) {
    char buf[100];
    sprintf(buf, ".p%i", target_p_);
    strcat(dir, buf);
  }
#endif
  fprintf(stderr, "Reading trunk sumprobs\n");
  trunk_sumprobs_->Read(dir, base_it_, base_betting_trees_->GetBettingTree(), "x", -1, true,
			quantize);
  fprintf(stderr, "Read trunk sumprobs\n");

  if (base_mem_ && method != ResolvingMethod::UNSAFE) {
    // We are calculating CBRs from the *base* strategy, not the resolved
    // endgame strategy.  So pass in base_card_abstraction_, etc.
    dynamic_cbr_.reset(new DynamicCBR(base_card_abstraction_, base_cfr_config_, base_buckets_, 1));
    if (current_) {
      unique_ptr<bool []> subgame_streets(new bool[max_street + 1]);
      for (int st = 0; st <= max_street; ++st) {
	subgame_streets[st] = st >= solve_st_;
      }
      shared_ptr<CFRValues> regrets;
      regrets.reset(new CFRValues(nullptr, subgame_streets.get(), 0, 0, base_buckets_,
				  base_betting_trees_->GetBettingTree()));
      regrets->Read(dir, base_it_, base_betting_trees_->GetBettingTree(), "x", -1, false, false);
      dynamic_cbr_->SetRegrets(regrets);
    } else {
      dynamic_cbr_->SetSumprobs(trunk_sumprobs_);
    }
  }

  if (base_mem_ && method != ResolvingMethod::UNSAFE) {
    trunk_hand_tree_.reset(new HandTree(0, 0, max_street));
  } else {
    if (solve_st_ > 0) {
      fprintf(stderr, "Creating hand tree\n");
      trunk_hand_tree_.reset(new HandTree(0, 0, solve_st_ - 1));
      fprintf(stderr, "Created hand tree\n");
    } else {
      trunk_hand_tree_.reset(new HandTree(0, 0, 0));
    }
  }
}

static Node *FindCorrespondingNode(Node *old_node, Node *old_target, Node *new_node) {
  if (old_node->Terminal()) return nullptr;
  if (old_node == old_target) return new_node;
  int num_succs = old_node->NumSuccs();
  for (int s = 0; s < num_succs; ++s) {
    Node *new_target = FindCorrespondingNode(old_node->IthSucc(s), old_target,
					     new_node->IthSucc(s));
    if (new_target) return new_target;
  }
  return nullptr;
}

// Get rid of this; use CreateSubtrees() from subgame_utils.cpp instead
// Assume no bet pending
// Doesn't support multiplayer yet
BettingTrees *SubgameSolver::CreateSubtrees(Node *node, int target_p, bool base) {
					   
  int player_acting = node->PlayerActing();
  int bet_to = node->LastBetTo();
  int st = node->Street();
  int last_bet_size = 0;
  int num_street_bets = 0;
  int num_terminals = 0;
  // Only need initial street, stack size and min bet from
  // base_betting_abstraction_.
  const BettingAbstraction &betting_abstraction = base ?
    base_betting_abstraction_ : subgame_betting_abstraction_;
  BettingTreeBuilder betting_tree_builder(betting_abstraction, target_p);
  shared_ptr<Node> subtree_root =
    betting_tree_builder.CreateNoLimitSubtree(st, last_bet_size, bet_to, num_street_bets,
					      player_acting, target_p, &num_terminals);
  // Delete the nodes under subtree_root?  Or does garbage collection
  // automatically take care of it because they are shared pointers.
  return new BettingTrees(subtree_root.get());
}

// Currently assume that this is a street-initial node.
// Might need to do up to four solves.  Imagine we have an asymmetric base
// betting tree, and an asymmetric solving method.
void SubgameSolver::ResolveUnsafe(Node *node, const string &action_sequence,
				  const ReachProbs &reach_probs) {
  int st = node->Street();
  fprintf(stderr, "ResolveUnsafe %s st %i nt %i gbd %i\n", action_sequence.c_str(), st,
	  node->NonterminalID(), target_bd_);
  int num_players = Game::NumPlayers();
  HandTree hand_tree(st, target_bd_, Game::MaxStreet());
  unique_ptr<EGCFR> eg_cfr;
  if (method_ == ResolvingMethod::UNSAFE) {
    eg_cfr.reset(new UnsafeEGCFR(subgame_card_abstraction_, base_card_abstraction_,
				 base_betting_abstraction_, subgame_cfr_config_, base_cfr_config_,
				 subgame_buckets_, 1));
  } else {
    fprintf(stderr, "Method not supported yet\n");
    exit(-1);
  }
  
  int num_asym_players = base_betting_abstraction_.Asymmetric() ? num_players : 1;
  for (int asym_p = 0; asym_p < num_asym_players; ++asym_p) {
    BettingTrees *subgame_subtrees = CreateSubtrees(node, asym_p, false);
    if (method_ != ResolvingMethod::UNSAFE) {
      fprintf(stderr, "Expecting unsafe method\n");
      exit(-1);
    }

    double resolving_secs = 0;
    struct timespec start, finish;
    clock_gettime(CLOCK_MONOTONIC, &start);
    // One solve for unsafe endgame solving, no t_vals
    eg_cfr->SolveSubgame(subgame_subtrees, target_bd_, reach_probs, action_sequence, &hand_tree,
			 nullptr, -1, true, num_subgame_its_);
    clock_gettime(CLOCK_MONOTONIC, &finish);
    resolving_secs += (finish.tv_sec - start.tv_sec);
    resolving_secs += (finish.tv_nsec - start.tv_nsec) / 1000000000.0;
    printf("Resolving secs: %f\n", resolving_secs);
    fflush(stdout);
    // Don't write out the resolved strategies
    delete subgame_subtrees;
  }
}

void SubgameSolver::ResolveSafe(Node *node, const string &action_sequence,
				const ReachProbs &reach_probs) {
  int st = node->Street();
  fprintf(stderr, "ResolveSafe %s st %i nt %i gbd %i\n", action_sequence.c_str(), st,
	  node->NonterminalID(), target_bd_);
  int num_players = Game::NumPlayers();
  HandTree hand_tree(st, target_bd_, Game::MaxStreet());
  unique_ptr<EGCFR> eg_cfr;
  if (method_ == ResolvingMethod::CFRD) {
    eg_cfr.reset(new CFRDEGCFR(subgame_card_abstraction_, base_card_abstraction_,
			       base_betting_abstraction_, subgame_cfr_config_, base_cfr_config_,
			       subgame_buckets_, cfrs_, zero_sum_, 1));
  } else if (method_ == ResolvingMethod::COMBINED) {
    eg_cfr.reset(new CombinedEGCFR(subgame_card_abstraction_, base_card_abstraction_,
				   base_betting_abstraction_, subgame_cfr_config_, base_cfr_config_,
				   subgame_buckets_, cfrs_, zero_sum_, 1));
  } else {
    fprintf(stderr, "ResolveSafe unsupported method\n");
    exit(-1);
  }
  // Don't support asymmetric yet
  unique_ptr<BettingTrees> subgame_subtrees(CreateSubtrees(node, 0, false));
  for (int solve_p = 0; solve_p < num_players; ++solve_p) {
    if (! card_level_) {
      fprintf(stderr, "DynamicCBR cannot compute bucket-level CVs\n");
      exit(-1);
    }
    shared_ptr<double []> t_vals;
    if (base_mem_) {
      // When base_mem_ is true, we use a global betting tree, a global
      // hand tree and have a global base strategy.
      // We assume that pure_streets_[st] tells us whether to purify
      // for the entire endgame.
      t_vals = dynamic_cbr_->Compute(node, reach_probs, target_bd_, trunk_hand_tree_.get(),
				     solve_p^1, cfrs_, zero_sum_, current_, pure_streets_[st]);
    } else {
      fprintf(stderr, "base_mem_ false not supported yet\n");
      exit(-1);
#if 0
      t_vals = dynamic_cbr_->Compute(base_subtree->Root(), reach_probs, target_bd_, &hand_tree,
				     st, target_bd_, solve_p^1, cfrs_, zero_sum_, current_,
				     pure_streets_[st]);
#endif
    }
    // Pass in false for both_players.  I am doing separate solves for
    // each player.
    eg_cfr->SolveSubgame(subgame_subtrees.get(), target_bd_, reach_probs, action_sequence,
			 &hand_tree, t_vals.get(), solve_p, false, num_subgame_its_);
    // Don't write out the resolved strategies
  }
}

void SubgameSolver::Walk(Node *node, const string &action_sequence, const ReachProbs &reach_probs) {
  if (node->Terminal()) return;
  if (node->LastBetTo() == base_betting_abstraction_.StackSize()) {
    // No point doing resolving if we are already all-in
    return;
  }
  if (action_sequence == target_action_sequence_) {
    // Do we assume that this is a street-initial node?
    // We do assume no bet pending
    // Skip if we are already all in?
    if (method_ == ResolvingMethod::UNSAFE) {
      ResolveUnsafe(node, action_sequence, reach_probs);
    } else {
      ResolveSafe(node, action_sequence, reach_probs);
    }
    return;
  }
  // Return if current action sequence is not a prefix of the target action sequence.
  if (action_sequence.size() >= target_action_sequence_.size()) return;
  if (strncmp(target_action_sequence_.c_str(), action_sequence.c_str(), action_sequence.size())) {
    return;
  }

  const CFRValues *sumprobs;
  // if (base_mem_ && ! current_) sumprobs = dynamic_cbr_->Sumprobs();
  // else                         sumprobs = trunk_sumprobs_.get();
  sumprobs = trunk_sumprobs_.get();
  int st = node->Street();
  int bd = pred_boards_[st];
  const CanonicalCards *hands = trunk_hand_tree_->Hands(st, bd);
  shared_ptr<ReachProbs []> succ_reach_probs =
    ReachProbs::CreateSuccReachProbs(node, bd, bd, hands, base_buckets_, sumprobs, reach_probs, 
				     false);
  int num_succs = node->NumSuccs();
  for (int s = 0; s < num_succs; ++s) {
    string action = node->ActionName(s);
    Walk(node->IthSucc(s), action_sequence + action, succ_reach_probs[s]);
  }
}

void SubgameSolver::Walk(void) {
  unique_ptr<ReachProbs> reach_probs(ReachProbs::CreateRoot());
  // Don't start action sequence with "x"
  Walk(base_betting_trees_->Root(), "", *reach_probs);
}

static void Usage(const char *prog_name) {
  fprintf(stderr, "USAGE: %s <game params> <base card params> <subgame card params> "
	  "<base betting params> <subgame betting params> <base CFR params> <subgame CFR params> "
	  "<action sequence> <bd> <base it> <num subgame its> [unsafe|cfrd|maxmargin|combined] "
	  "[cbrs|cfrs] [card|bucket] [zerosum|raw] [current|avg] [quantize|raw] <pure streets> "
	  "[mem|disk] <num threads>\n", prog_name);
  fprintf(stderr, "\n");
  fprintf(stderr, "\"current\" or \"avg\" signifies whether we use the opponent's current strategy "
	  "(from regrets) in the subgame CBR calculation, or, as per usual, the avg strategy (from "
	  "sumprobs)\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "pure streets are those streets on which to purify probabilities.  In the "
	  "endgame, this means purifying the opponent's strategy when computing the endgame CBRs.  "
	  "In the trunk, this means purifying the reach probs for *both* players.  Use \"none\" "
	  "to signify no pure streets.\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "\"mem\" or \"disk\" signifies whether the base strategy for the subgame "
	  "streets is loaded into memory at startup, or whether we read the base subgame strategy "
	  "as needed.  Note that the trunk streets are loaded into memory at startup "
	  "regardless.\n");
  exit(-1);
}

int main(int argc, char *argv[]) {
  if (argc != 21) Usage(argv[0]);
  Files::Init();
  unique_ptr<Params> game_params = CreateGameParams();
  game_params->ReadFromFile(argv[1]);
  Game::Initialize(*game_params);
  unique_ptr<Params> base_card_params = CreateCardAbstractionParams();
  base_card_params->ReadFromFile(argv[2]);
  unique_ptr<CardAbstraction>
    base_card_abstraction(new CardAbstraction(*base_card_params));
  unique_ptr<Params> subgame_card_params = CreateCardAbstractionParams();
  subgame_card_params->ReadFromFile(argv[3]);
  unique_ptr<CardAbstraction>
    subgame_card_abstraction(new CardAbstraction(*subgame_card_params));
  unique_ptr<Params> base_betting_params = CreateBettingAbstractionParams();
  base_betting_params->ReadFromFile(argv[4]);
  unique_ptr<BettingAbstraction>
    base_betting_abstraction(new BettingAbstraction(*base_betting_params));
  unique_ptr<Params> subgame_betting_params = CreateBettingAbstractionParams();
  subgame_betting_params->ReadFromFile(argv[5]);
  unique_ptr<BettingAbstraction> subgame_betting_abstraction(
		   new BettingAbstraction(*subgame_betting_params));
  unique_ptr<Params> base_cfr_params = CreateCFRParams();
  base_cfr_params->ReadFromFile(argv[6]);
  unique_ptr<CFRConfig> base_cfr_config(new CFRConfig(*base_cfr_params));
  unique_ptr<Params> subgame_cfr_params = CreateCFRParams();
  subgame_cfr_params->ReadFromFile(argv[7]);
  unique_ptr<CFRConfig> subgame_cfr_config(new CFRConfig(*subgame_cfr_params));
  string action_sequence = argv[8];
  int bd, base_it, num_subgame_its;
  if (sscanf(argv[9], "%i", &bd) != 1)               Usage(argv[0]);
  if (sscanf(argv[10], "%i", &base_it) != 1)         Usage(argv[0]);
  if (sscanf(argv[11], "%i", &num_subgame_its) != 1) Usage(argv[0]);
  string m = argv[12];
  ResolvingMethod method;
  if (m == "unsafe")         method = ResolvingMethod::UNSAFE;
  else if (m == "cfrd")      method = ResolvingMethod::CFRD;
  else if (m == "maxmargin") method = ResolvingMethod::MAXMARGIN;
  else if (m == "combined")  method = ResolvingMethod::COMBINED;
  else                       Usage(argv[0]);
  string v = argv[13];
  bool cfrs;
  if (v == "cbrs")      cfrs = false;
  else if (v == "cfrs") cfrs = true;
  else                  Usage(argv[0]);
  string l = argv[14];
  bool card_level;
  if (l == "card")        card_level = true;
  else if (l == "bucket") card_level = false;
  else                    Usage(argv[0]);
  string z = argv[15];
  bool zero_sum;
  if (z == "zerosum")  zero_sum = true;
  else if (z == "raw") zero_sum = false;
  else                 Usage(argv[0]);
  string c = argv[16];
  bool current;
  if (c == "current")  current = true;
  else if (c == "avg") current = false;
  else                 Usage(argv[0]);
  string q = argv[17];
  bool quantize;
  if (q == "quantize") quantize = true;
  else if (q == "raw") quantize = false;
  else                 Usage(argv[0]);
  int max_street = Game::MaxStreet();
  unique_ptr<bool []> pure_streets(new bool[max_street + 1]);
  string p = argv[18];
  for (int st = 0; st <= max_street; ++st) pure_streets[st] = false;
  if (p != "none") {
    vector<int> v;
    ParseInts(p, &v);
    int num = v.size();
    for (int i = 0; i < num; ++i) {
      pure_streets[v[i]] = true;
    }
  }
  string mem = argv[19];
  bool base_mem;
  if (mem == "mem")       base_mem = true;
  else if (mem == "disk") base_mem = false;
  else                    Usage(argv[0]);
  int num_threads;
  if (sscanf(argv[20], "%i", &num_threads) != 1) Usage(argv[0]);

  // If card abstractions are the same, should not load both.
  Buckets base_buckets(*base_card_abstraction, false);
  Buckets subgame_buckets(*subgame_card_abstraction, false);

  BoardTree::Create();
  BoardTree::CreateLookup();
  HandValueTree::Create();

  SubgameSolver solver(*base_card_abstraction, *subgame_card_abstraction, *base_betting_abstraction,
		       *subgame_betting_abstraction, *base_cfr_config, *subgame_cfr_config,
		       base_buckets, subgame_buckets, action_sequence, bd, method, cfrs, card_level,
		       zero_sum, current, quantize, pure_streets.get(), base_mem, base_it,
		       num_subgame_its, num_threads);
  solver.Walk();
}
