// Dynamically compute the opponent counterfactual values.
//
// We only solve subgames at street-initial nodes.  There is no nested subgame solving.
//
// Eventually this should become a good imitation of what we would do in the bot.  That means we
// should not assume we can load the trunk sumprobs in memory.  Can I assume a hand tree for the
// trunk in memory?
//
// With base_mem false, solve_all_subgames is pretty slow because we repeatedly read the entire base
// strategy in (inside ReadBaseSubgameStrategy()).
//
// Should allow trunk sumprobs to be quantized.

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
#include "subgame_utils.h" // WriteSubgame()
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
		const Buckets &base_buckets, const Buckets &subgame_buckets, int solve_st,
		ResolvingMethod method, bool cfrs, bool card_level, bool zero_sum, bool current,
		const bool *pure_streets, bool base_mem, int base_it, int num_subgame_its,
		int num_inner_threads, int num_outer_threads);
  ~SubgameSolver(void) {}
  void Walk(Node *node, const string &action_sequence, int gbd, const ReachProbs &reach_probs,
	    int last_bet_size, int num_street_bets, int num_bets, int num_players_to_act,
	    int last_st);
  void Walk(void);
private:
  BettingTrees *CreateSubtrees(Node *node, int target_p, bool base);
  void Split(Node *node, const string &action_sequence, int pgbd, const ReachProbs &reach_probs,
	     int num_bets);
  void StreetInitial(Node *node, const string &action_sequence, int pgbd,
		     const ReachProbs &reach_probs, int num_bets);
  void ResolveUnsafe(Node *node, int gbd, const string &action_sequence,
		     const ReachProbs &reach_probs);
  void ResolveSafe(Node *node, int gbd, const string &action_sequence,
		   const ReachProbs &reach_probs);

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
  int num_inner_threads_;
  int num_outer_threads_;
};

SubgameSolver::SubgameSolver(const CardAbstraction &base_card_abstraction,
			     const CardAbstraction &subgame_card_abstraction,
			     const BettingAbstraction &base_betting_abstraction,
			     const BettingAbstraction &subgame_betting_abstraction,
			     const CFRConfig &base_cfr_config, const CFRConfig &subgame_cfr_config,
			     const Buckets &base_buckets, const Buckets &subgame_buckets,
			     int solve_st, ResolvingMethod method, bool cfrs, bool card_level,
			     bool zero_sum, bool current, const bool *pure_streets, bool base_mem,
			     int base_it, int num_subgame_its, int num_inner_threads,
			     int num_outer_threads) :
  base_card_abstraction_(base_card_abstraction),
  subgame_card_abstraction_(subgame_card_abstraction),
  base_betting_abstraction_(base_betting_abstraction),
  subgame_betting_abstraction_(subgame_betting_abstraction), base_cfr_config_(base_cfr_config),
  subgame_cfr_config_(subgame_cfr_config), base_buckets_(base_buckets),
  subgame_buckets_(subgame_buckets) {
  solve_st_ = solve_st;
  method_ = method;
  cfrs_ = cfrs;
  card_level_ = card_level;
  zero_sum_ = zero_sum;
  current_ = current;
  pure_streets_ = pure_streets;
  base_mem_ = base_mem;
  base_it_ = base_it;
  num_subgame_its_ = num_subgame_its;
  num_inner_threads_ = num_inner_threads;
  num_outer_threads_ = num_outer_threads;

  base_betting_trees_.reset(new BettingTrees(base_betting_abstraction_));

  // We need probs for both players
  int max_street = Game::MaxStreet();
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
  trunk_sumprobs_->Read(dir, base_it_, base_betting_trees_->GetBettingTree(), "x", -1, true, false);

  if (base_mem_) {
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

  if (base_mem_) {
    trunk_hand_tree_.reset(new HandTree(0, 0, max_street));
  } else {
    if (solve_st_ > 0) {
      trunk_hand_tree_.reset(new HandTree(0, 0, solve_st_ - 1));
    } else {
      trunk_hand_tree_.reset(new HandTree(0, 0, 0));
    }
  }
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

class SSThread {
public:
  SSThread(Node *node, const string &action_sequence, int pgbd, const ReachProbs &reach_probs,
	   int num_bets, SubgameSolver *solver, int thread_index, int num_threads);
  ~SSThread(void) {}
  void Run(void);
  void Join(void);
  void Go(void);
private:
  Node *node_;
  string action_sequence_;
  int pgbd_;
  const ReachProbs &reach_probs_;
  int num_bets_;
  SubgameSolver *solver_;
  int thread_index_;
  int num_threads_;
  pthread_t pthread_id_;
};

SSThread::SSThread(Node *node, const string &action_sequence, int pgbd,
		   const ReachProbs &reach_probs, int num_bets, SubgameSolver *solver,
		   int thread_index, int num_threads) :
  node_(node), action_sequence_(action_sequence), pgbd_(pgbd), reach_probs_(reach_probs),
  num_bets_(num_bets), solver_(solver), thread_index_(thread_index), num_threads_(num_threads) {
}

void SSThread::Go(void) {
  int nst = node_->Street();
  int pst = nst - 1;
  int ngbd_begin = BoardTree::SuccBoardBegin(pst, pgbd_, nst);
  int ngbd_end = BoardTree::SuccBoardEnd(pst, pgbd_, nst);
  for (int ngbd = ngbd_begin; ngbd < ngbd_end; ++ngbd) {
    if (ngbd % num_threads_ != thread_index_) continue;
    // Street initial
    solver_->Walk(node_, action_sequence_, ngbd, reach_probs_, 0, 0, num_bets_, 2, nst);
  }
}

static void *ss_thread_run(void *v_t) {
  SSThread *t = (SSThread *)v_t;
  t->Go();
  return NULL;
}

void SSThread::Run(void) {
  pthread_create(&pthread_id_, NULL, ss_thread_run, this);
}

void SSThread::Join(void) {
  pthread_join(pthread_id_, NULL); 
}

void SubgameSolver::Split(Node *node, const string &action_sequence, int pgbd,
			  const ReachProbs &reach_probs, int num_bets) {
  unique_ptr<SSThread * []> threads(new SSThread *[num_outer_threads_]);
  for (int t = 0; t < num_outer_threads_; ++t) {
    threads[t] = new SSThread(node, action_sequence, pgbd, reach_probs, num_bets, this, t,
			      num_outer_threads_);
  }
  for (int t = 1; t < num_outer_threads_; ++t) threads[t]->Run();
  // Do first thread in main thread
  threads[0]->Go();
  for (int t = 1; t < num_outer_threads_; ++t) threads[t]->Join();
  for (int t = 0; t < num_outer_threads_; ++t) delete threads[t];
}

void SubgameSolver::StreetInitial(Node *node, const string &action_sequence, int pgbd,
				  const ReachProbs &reach_probs, int num_bets) {
  int nst = node->Street();
  if (nst == 1 && num_outer_threads_ > 1) {
    Split(node, action_sequence, pgbd, reach_probs, num_bets);
  } else {
    int pst = node->Street() - 1;
    int ngbd_begin = BoardTree::SuccBoardBegin(pst, pgbd, nst);
    int ngbd_end = BoardTree::SuccBoardEnd(pst, pgbd, nst);
    for (int ngbd = ngbd_begin; ngbd < ngbd_end; ++ngbd) {
      Walk(node, action_sequence, ngbd, reach_probs, 0, 0, num_bets, 2, nst);
    }
  }
}

// Currently assume that this is a street-initial node.
// Might need to do up to four solves.  Imagine we have an asymmetric base
// betting tree, and an asymmetric solving method.
void SubgameSolver::ResolveUnsafe(Node *node, int gbd, const string &action_sequence,
				  const ReachProbs &reach_probs) {
  int st = node->Street();
  fprintf(stderr, "ResolveUnsafe %s st %i nt %i gbd %i\n", action_sequence.c_str(), st,
	  node->NonterminalID(), gbd);
  int num_players = Game::NumPlayers();
  HandTree hand_tree(st, gbd, Game::MaxStreet());
  unique_ptr<EGCFR> eg_cfr;
  if (method_ == ResolvingMethod::UNSAFE) {
    eg_cfr.reset(new UnsafeEGCFR(subgame_card_abstraction_, base_card_abstraction_,
				 base_betting_abstraction_, subgame_cfr_config_, base_cfr_config_,
				 subgame_buckets_, num_inner_threads_));
    if (st < Game::MaxStreet()) {
      eg_cfr->SetSplitStreet(st + 1);
    }
  } else {
    fprintf(stderr, "Method not supported yet\n");
    exit(-1);
  }
  
  int num_asym_players = base_betting_abstraction_.Asymmetric() ? num_players : 1;
  for (int asym_p = 0; asym_p < num_asym_players; ++asym_p) {
    BettingTrees *base_subtrees = nullptr;
    BettingTrees *subgame_subtrees = CreateSubtrees(node, asym_p, false);
    if (! base_mem_) {
      fprintf(stderr, "base_mem_ false not supported currently\n");
      exit(-1);
#if 0
      base_subtrees = CreateSubtrees(node, asym_p, true);
      // The action sequence passed in should specify the root of the system
      // we are reading (the base system).
      // We never finished implementing this function.
      unique_ptr<CFRValues> base_subgame_strategy =
	    ReadBaseSubgameStrategy(base_card_abstraction_, base_betting_abstraction_,
				    base_cfr_config_, base_betting_trees_.get(), base_buckets_,
				    subgame_buckets_, base_it_, node,  gbd, "x", reach_probs,
				    base_subtrees, current_, asym_p);
      // We are calculating CBRs from the *base* strategy, not the resolved
      // endgame strategy.  So pass in base_card_abstraction_, etc.
      dynamic_cbr_.reset(new DynamicCBR2(base_card_abstraction_, base_betting_abstraction_,
					 base_cfr_config_, base_buckets_, 1));
      if (current_) dynamic_cbr_->MoveRegrets(base_subgame_strategy);
      else          dynamic_cbr_->MoveSumprobs(base_subgame_strategy);
#endif
    }

    if (method_ == ResolvingMethod::UNSAFE) {
      // One solve for unsafe endgame solving, no t_vals
      eg_cfr->SolveSubgame(subgame_subtrees, gbd, reach_probs, action_sequence, &hand_tree, nullptr,
			   -1, true, num_subgame_its_);
      // Write out the P0 and P1 strategies
      for (int solve_p = 0; solve_p < num_players; ++solve_p) {
	WriteSubgame(subgame_subtrees->Root(), action_sequence, action_sequence, gbd,
		     base_card_abstraction_, subgame_card_abstraction_, base_betting_abstraction_,
		     subgame_betting_abstraction_, base_cfr_config_, subgame_cfr_config_, method_,
		     eg_cfr->Sumprobs().get(), st, gbd, asym_p, solve_p, st);
      }
    } else {
      for (int solve_p = 0; solve_p < num_players; ++solve_p) {
	// What should I be supplying for the betting abstraction, CFR
	// config and betting tree?  Does it matter?
	if (! card_level_) {
	  fprintf(stderr, "DynamicCBR cannot compute bucket-level CVs\n");
	  exit(-1);
	}
	shared_ptr<double []> t_vals;
	if (method_ != ResolvingMethod::UNSAFE) {
	  if (base_mem_) {
	    // When base_mem_ is true, we use a global betting tree, a global
	    // hand tree and have a global base strategy.
	    // We assume that pure_streets_[st] tells us whether to purify
	    // for the entire endgame.
	    t_vals = dynamic_cbr_->Compute(node, reach_probs, gbd, trunk_hand_tree_.get(),
					   solve_p^1, cfrs_, zero_sum_, current_,
					   pure_streets_[st]);
	  } else {
	    t_vals = dynamic_cbr_->Compute(base_subtrees->Root(), reach_probs, gbd, &hand_tree,
					   solve_p^1, cfrs_, zero_sum_, current_,
					   pure_streets_[st]);
	  }
	}

	// Pass in false for both_players.  I am doing separate solves for
	// each player.
	eg_cfr->SolveSubgame(subgame_subtrees, gbd, reach_probs, action_sequence, &hand_tree,
			     t_vals.get(), solve_p, false, num_subgame_its_);

	WriteSubgame(subgame_subtrees->Root(), action_sequence, action_sequence, gbd,
		     base_card_abstraction_, subgame_card_abstraction_, base_betting_abstraction_,
		     subgame_betting_abstraction_, base_cfr_config_, subgame_cfr_config_, method_,
		     eg_cfr->Sumprobs().get(), st, gbd, asym_p, solve_p, st);
      }
    }
  
    delete base_subtrees;
    delete subgame_subtrees;
  }
}

void SubgameSolver::ResolveSafe(Node *node, int gbd, const string &action_sequence,
				const ReachProbs &reach_probs) {
  int st = node->Street();
  fprintf(stderr, "ResolveSafe %s st %i nt %i gbd %i\n", action_sequence.c_str(), st,
	  node->NonterminalID(), gbd);
  int num_players = Game::NumPlayers();
  HandTree hand_tree(st, gbd, Game::MaxStreet());
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
      t_vals = dynamic_cbr_->Compute(node, reach_probs, gbd, trunk_hand_tree_.get(),
				     solve_p^1, cfrs_, zero_sum_, current_, pure_streets_[st]);
      // fprintf(stderr, "solve_p %i t_vals[0] %f\n", solve_p, t_vals[0]);
      // exit(-1);
    } else {
      fprintf(stderr, "base_mem_ false not supported yet\n");
      exit(-1);
#if 0
      t_vals = dynamic_cbr_->Compute(base_subtree->Root(), reach_probs, gbd, &hand_tree,
				     solve_p^1, cfrs_, zero_sum_, current_,
				     pure_streets_[st]);
#endif
    }
    // Pass in false for both_players.  I am doing separate solves for
    // each player.
    eg_cfr->SolveSubgame(subgame_subtrees.get(), gbd, reach_probs, action_sequence, &hand_tree,
			 t_vals.get(), solve_p, false, num_subgame_its_);

    WriteSubgame(subgame_subtrees->Root(), action_sequence, action_sequence, gbd,
		 base_card_abstraction_, subgame_card_abstraction_, base_betting_abstraction_,
		 subgame_betting_abstraction_, base_cfr_config_, subgame_cfr_config_, method_,
		 eg_cfr->Sumprobs().get(), st, gbd, 0, solve_p, st);
  }
}

void SubgameSolver::Walk(Node *node, const string &action_sequence, int gbd,
			 const ReachProbs &reach_probs, int last_bet_size, int num_street_bets,
			 int num_bets, int num_players_to_act, int last_st) {
  if (node->Terminal()) return;
  int st = node->Street();
  if (st > last_st) {
    if (node->LastBetTo() == base_betting_abstraction_.StackSize()) {
      // No point doing resolving if we are already all-in
      return;
    }
    StreetInitial(node, action_sequence, gbd, reach_probs, num_bets);
    return;
  }
  if (st == solve_st_) {
    // Do we assume that this is a street-initial node?
    // We do assume no bet pending
    // Skip if we are already all in?
    if (method_ == ResolvingMethod::UNSAFE) {
      ResolveUnsafe(node, gbd, action_sequence, reach_probs);
    } else {
      ResolveSafe(node, gbd, action_sequence, reach_probs);
    }
    return;
  }

  const CFRValues *sumprobs;
  if (base_mem_ && ! current_) sumprobs = dynamic_cbr_->Sumprobs().get();
  else                         sumprobs = trunk_sumprobs_.get();
  const CanonicalCards *hands = trunk_hand_tree_->Hands(st, gbd);
  shared_ptr<ReachProbs []> succ_reach_probs =
    ReachProbs::CreateSuccReachProbs(node, gbd, gbd, hands, base_buckets_, sumprobs, reach_probs, 
				     false);
  int num_succs = node->NumSuccs();
  for (int s = 0; s < num_succs; ++s) {
    int fsi = node->FoldSuccIndex();
    int csi = node->CallSuccIndex();
    bool bet = s != fsi && s != csi;
    int last_bet_size = 0;
    if (bet) {
      last_bet_size = node->IthSucc(s)->LastBetTo() - node->LastBetTo();
    }
    int new_num_street_bets = num_street_bets + bet ? 1 : 0;
    int new_num_bets = num_bets + bet ? 1 : 0;
    string action = node->ActionName(s);
    Walk(node->IthSucc(s), action_sequence + action, gbd, succ_reach_probs[s], last_bet_size,
	 new_num_street_bets, new_num_bets, 1, st);
  }
}

void SubgameSolver::Walk(void) {
  int last_bet_size = Game::BigBlind() - Game::SmallBlind();
  unique_ptr<ReachProbs> reach_probs(ReachProbs::CreateRoot());
  Walk(base_betting_trees_->Root(), "x", 0, *reach_probs, last_bet_size, 0, 0, 2, 0);
}

static void Usage(const char *prog_name) {
  fprintf(stderr, "USAGE: %s <game params> <base card params> <subgame card params> "
	  "<base betting params> <subgame betting params> <base CFR params> <subgame CFR params> "
	  "<solve street> <base it> <num subgame its> [unsafe|cfrd|maxmargin|combined] [cbrs|cfrs] "
	  "[card|bucket] [zerosum|raw] [current|avg] <pure streets> [mem|disk] "
	  "<num inner threads> <num outer threads>\n", prog_name);
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
  fprintf(stderr, "\n");
  fprintf(stderr, "We support two different methods of multithreading.  The first type is the "
	  "multithreading inside of VCFR.  The second type is the multithreading inside of "
	  "solve_all_subgames.  <num inner threads> controls the first type; <num outer threads> "
	  "controls the second type.\n");
  exit(-1);
}

int main(int argc, char *argv[]) {
  if (argc != 20) Usage(argv[0]);
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
  int solve_st, base_it, num_subgame_its;
  if (sscanf(argv[8], "%i", &solve_st) != 1)         Usage(argv[0]);
  if (sscanf(argv[9], "%i", &base_it) != 1)         Usage(argv[0]);
  if (sscanf(argv[10], "%i", &num_subgame_its) != 1) Usage(argv[0]);
  string m = argv[11];
  ResolvingMethod method;
  if (m == "unsafe")         method = ResolvingMethod::UNSAFE;
  else if (m == "cfrd")      method = ResolvingMethod::CFRD;
  else if (m == "maxmargin") method = ResolvingMethod::MAXMARGIN;
  else if (m == "combined")  method = ResolvingMethod::COMBINED;
  else                       Usage(argv[0]);
  string v = argv[12];
  bool cfrs;
  if (v == "cbrs")      cfrs = false;
  else if (v == "cfrs") cfrs = true;
  else                  Usage(argv[0]);
  string l = argv[13];
  bool card_level;
  if (l == "card")        card_level = true;
  else if (l == "bucket") card_level = false;
  else                    Usage(argv[0]);
  string z = argv[14];
  bool zero_sum;
  if (z == "zerosum")  zero_sum = true;
  else if (z == "raw") zero_sum = false;
  else                 Usage(argv[0]);
  string c = argv[15];
  bool current;
  if (c == "current")  current = true;
  else if (c == "avg") current = false;
  else                 Usage(argv[0]);
  int max_street = Game::MaxStreet();
  unique_ptr<bool []> pure_streets(new bool[max_street + 1]);
  string p = argv[16];
  for (int st = 0; st <= max_street; ++st) pure_streets[st] = false;
  if (p != "none") {
    vector<int> v;
    ParseInts(p, &v);
    int num = v.size();
    for (int i = 0; i < num; ++i) {
      pure_streets[v[i]] = true;
    }
  }
  string mem = argv[17];
  bool base_mem;
  if (mem == "mem")       base_mem = true;
  else if (mem == "disk") base_mem = false;
  else                    Usage(argv[0]);
  int num_inner_threads, num_outer_threads;
  if (sscanf(argv[18], "%i", &num_inner_threads) != 1) Usage(argv[0]);
  if (sscanf(argv[19], "%i", &num_outer_threads) != 1) Usage(argv[0]);

  if (num_inner_threads > 1 && solve_st == max_street) {
    fprintf(stderr, "Can't have num_inner_threads > 1 if solve_st == max_street\n");
    exit(-1);
  }
  
  // If card abstractions are the same, should not load both.
  Buckets base_buckets(*base_card_abstraction, false);
  Buckets subgame_buckets(*subgame_card_abstraction, false);

  BoardTree::Create();
  HandValueTree::Create();

  SubgameSolver solver(*base_card_abstraction, *subgame_card_abstraction, *base_betting_abstraction,
		       *subgame_betting_abstraction, *base_cfr_config, *subgame_cfr_config,
		       base_buckets, subgame_buckets, solve_st, method, cfrs, card_level, zero_sum,
		       current, pure_streets.get(), base_mem, base_it, num_subgame_its,
		       num_inner_threads, num_outer_threads);
  for (int asym_p = 0; asym_p <= 1; ++asym_p) {
    DeleteAllSubgames(*base_card_abstraction, *subgame_card_abstraction, *base_betting_abstraction,
		      *subgame_betting_abstraction, *base_cfr_config, *subgame_cfr_config, method,
		      asym_p);
  }
  solver.Walk();
}
