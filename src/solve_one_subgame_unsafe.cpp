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
		const string &target_action_sequence, int target_bd, bool quantize, int base_it,
		int num_subgame_its, int num_threads);
  ~SubgameSolver(void) {}
  void Walk(Node *node, const string &action_sequence, const ReachProbs &reach_probs);
  void Walk(void);
private:
  void Resolve(Node *node, const string &action_sequence, const ReachProbs &reach_probs);

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
  const string &target_action_sequence_;
  int target_bd_;
  int solve_st_;
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
			     bool quantize, int base_it, int num_subgame_its, int num_threads) :
  base_card_abstraction_(base_card_abstraction),
  subgame_card_abstraction_(subgame_card_abstraction),
  base_betting_abstraction_(base_betting_abstraction),
  subgame_betting_abstraction_(subgame_betting_abstraction), base_cfr_config_(base_cfr_config),
  subgame_cfr_config_(subgame_cfr_config), base_buckets_(base_buckets),
  subgame_buckets_(subgame_buckets), target_action_sequence_(target_action_sequence),
  target_bd_(target_bd) {
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
    trunk_streets[st] = st < solve_st_;
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
  fprintf(stderr, "Reading trunk sumprobs\n");
  trunk_sumprobs_->Read(dir, base_it_, base_betting_trees_->GetBettingTree(), "x", -1, true,
			quantize);
  fprintf(stderr, "Read trunk sumprobs\n");

  if (solve_st_ > 0) {
    trunk_hand_tree_.reset(new HandTree(0, 0, solve_st_ - 1));
  } else {
    trunk_hand_tree_.reset(new HandTree(0, 0, 0));
  }
}

// Currently assume that this is a street-initial node.
// Might need to do up to four solves.  Imagine we have an asymmetric base
// betting tree, and an asymmetric solving method.
void SubgameSolver::Resolve(Node *node, const string &action_sequence,
			    const ReachProbs &reach_probs) {
  int st = node->Street();
  fprintf(stderr, "Resolve %s st %i nt %i gbd %i\n", action_sequence.c_str(), st,
	  node->NonterminalID(), target_bd_);
  int num_players = Game::NumPlayers();
  HandTree hand_tree(st, target_bd_, Game::MaxStreet());
  UnsafeEGCFR eg_cfr(subgame_card_abstraction_, base_card_abstraction_,
		     base_betting_abstraction_, subgame_cfr_config_, base_cfr_config_,
		     subgame_buckets_, num_threads_);
  if (st < Game::MaxStreet()) {
    eg_cfr.SetSplitStreet(st + 1);
  }
  
  int num_asym_players = base_betting_abstraction_.Asymmetric() ? num_players : 1;
  for (int asym_p = 0; asym_p < num_asym_players; ++asym_p) {
    unique_ptr<BettingTrees> subgame_subtrees(CreateSubtrees(st, node->PlayerActing(),
							     node->LastBetTo(), asym_p,
							     subgame_betting_abstraction_));
    double resolving_secs = 0;
    struct timespec start, finish;
    clock_gettime(CLOCK_MONOTONIC, &start);
    // One solve for unsafe endgame solving, no t_vals
    eg_cfr.SolveSubgame(subgame_subtrees.get(), target_bd_, reach_probs, action_sequence,
			 &hand_tree, nullptr, -1, true, num_subgame_its_);
    clock_gettime(CLOCK_MONOTONIC, &finish);
    resolving_secs += (finish.tv_sec - start.tv_sec);
    resolving_secs += (finish.tv_nsec - start.tv_nsec) / 1000000000.0;
    printf("Resolving secs: %f\n", resolving_secs);
    fflush(stdout);
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
    Resolve(node, action_sequence, reach_probs);
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
	  "<action sequence> <bd> <base it> <num subgame its> [quantize|raw] <num threads>\n",
	  prog_name);
  exit(-1);
}

int main(int argc, char *argv[]) {
  if (argc != 14) Usage(argv[0]);
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
  string q = argv[12];
  bool quantize;
  if (q == "quantize") quantize = true;
  else if (q == "raw") quantize = false;
  else                 Usage(argv[0]);
  int num_threads;
  if (sscanf(argv[13], "%i", &num_threads) != 1) Usage(argv[0]);

  // If card abstractions are the same, should not load both.
  Buckets base_buckets(*base_card_abstraction, false);
  Buckets subgame_buckets(*subgame_card_abstraction, false);

  BoardTree::Create();
  BoardTree::CreateLookup();
  HandValueTree::Create();

  SubgameSolver solver(*base_card_abstraction, *subgame_card_abstraction, *base_betting_abstraction,
		       *subgame_betting_abstraction, *base_cfr_config, *subgame_cfr_config,
		       base_buckets, subgame_buckets, action_sequence, bd, quantize, base_it,
		       num_subgame_its, num_threads);
  solver.Walk();
}
