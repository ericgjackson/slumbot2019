// Solve all subgames on a given street and following streets using the backup method.  Produce
// a new resolved system that combines the base with the resolved strategies.
//
// Was trying to do simple test with base, resolve and final tree all mb1b1.  But the resolve
// tree allows an additional pot size raise.  I think I got to a mismatch in the number of succs
// between the final tree and the resolve tree.  Should we be able to tolerate that?  Maybe I need
// to use a different final tree.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <memory>
#include <string>
#include <vector>

#include "backup_tree.h"
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
#include "cfr_street_values.h"
#include "cfr_utils.h"
#include "cfr_values.h"
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
		const CardAbstraction &resolve_card_abstraction,
		const CardAbstraction &final_card_abstraction,
		const BettingAbstraction &base_betting_abstraction,
		const BettingAbstraction &final_betting_abstraction,
		const CFRConfig &base_cfr_config, const CFRConfig &resolve_cfr_config,
		const CFRConfig &final_cfr_config, const Buckets &base_buckets,
		const Buckets &resolve_buckets, int resolve_st, ResolvingMethod method,
		int base_it, int num_resolve_its, int num_inner_threads, int num_outer_threads);
  ~SubgameSolver(void) {}
  void Go(void);
  void Walk(int p, Node *node, Node *final_node, const string &action_sequence,
	    const string &resolve_action_sequence, int npbs, int npb, int gbd, int resolve_bet_to,
	    int resolve_gbd, const HandTree *resolve_hand_tree, const ReachProbs &reach_probs,
	    shared_ptr<CFRValues> resolve_sumprobs, ObservedBets *observed_bets, int last_st);
private:
  void Split(int p, Node *node, Node *final_node, const string &action_sequence,
	     const string &resolve_action_sequence, int npbs, int npb, int pgbd, int resolve_bet_to,
	     int resolve_gbd, const HandTree *resolve_hand_tree, const ReachProbs &reach_probs,
	     shared_ptr<CFRValues> resolve_sumprobs, ObservedBets *observed_bets);
  void StreetInitial(int p, Node *node, Node *final_node, const string &action_sequence,
		     const string &resolve_action_sequence, int npbs, int npb, int pgbd,
		     int resolve_bet_to, int resolve_gbd, const HandTree *resolve_hand_tree,
		     const ReachProbs &reach_probs, shared_ptr<CFRValues> resolve_sumprobs,
		     ObservedBets *observed_bets);
  shared_ptr<CFRValues> ResolveUnsafe(int gbd, const string &action_sequence,
				      const HandTree *hand_tree, const ReachProbs &reach_probs,
				      BettingTrees *betting_trees);
  shared_ptr<CFRValues> ResolveSafe(int gbd, const string &action_sequence,
				    const HandTree *hand_tree, const ReachProbs &reach_probs,
				    BettingTrees *betting_trees);
  shared_ptr<CFRValues> Resolve(int gbd, const string &action_sequence, const HandTree *hand_tree,
				const ReachProbs &reach_probs, BettingTrees *betting_trees);
  void CopyTrunk(Node *node);

  const CardAbstraction &base_card_abstraction_;
  const CardAbstraction &resolve_card_abstraction_;
  const CardAbstraction &final_card_abstraction_;
  const BettingAbstraction &base_betting_abstraction_;
  const BettingAbstraction &final_betting_abstraction_;
  const CFRConfig &base_cfr_config_;
  const CFRConfig &resolve_cfr_config_;
  const CFRConfig &final_cfr_config_;
  Buckets base_buckets_;
  Buckets resolve_buckets_;
  Buckets final_buckets_;
  unique_ptr<BettingTrees> base_betting_trees_;
  unique_ptr<BettingTrees> final_betting_trees_;
  unique_ptr<HandTree> trunk_hand_tree_;
  shared_ptr<CFRValues> base_sumprobs_;
  shared_ptr<CFRValues> final_sumprobs_;
  unique_ptr<DynamicCBR> dynamic_cbr_;
  int resolve_st_;
  ResolvingMethod method_;
  int base_it_;
  int num_resolve_its_;
  int num_inner_threads_;
  int num_outer_threads_;
  unique_ptr<int []> min_bets_;
  unique_ptr<int []> max_bets_;
};

SubgameSolver::SubgameSolver(const CardAbstraction &base_card_abstraction,
			     const CardAbstraction &resolve_card_abstraction,
			     const CardAbstraction &final_card_abstraction,
			     const BettingAbstraction &base_betting_abstraction,
			     const BettingAbstraction &final_betting_abstraction,
			     const CFRConfig &base_cfr_config, const CFRConfig &resolve_cfr_config,
			     const CFRConfig &final_cfr_config, const Buckets &base_buckets,
			     const Buckets &resolve_buckets, int resolve_st, ResolvingMethod method,
			     int base_it, int num_resolve_its, int num_inner_threads,
			     int num_outer_threads) :
  base_card_abstraction_(base_card_abstraction),
  resolve_card_abstraction_(resolve_card_abstraction),
  final_card_abstraction_(final_card_abstraction),
  base_betting_abstraction_(base_betting_abstraction),
  final_betting_abstraction_(final_betting_abstraction), base_cfr_config_(base_cfr_config),
  resolve_cfr_config_(resolve_cfr_config), final_cfr_config_(final_cfr_config),
  base_buckets_(base_card_abstraction, false), resolve_buckets_(resolve_card_abstraction, false),
  final_buckets_(final_card_abstraction, false) {
  resolve_st_ = resolve_st;
  method_ = method;
  base_it_ = base_it;
  num_resolve_its_ = num_resolve_its;
  num_inner_threads_ = num_inner_threads;
  num_outer_threads_ = num_outer_threads;

  base_betting_trees_.reset(new BettingTrees(base_betting_abstraction_));
  final_betting_trees_.reset(new BettingTrees(final_betting_abstraction_));

  // We need probs for both players
  int max_street = Game::MaxStreet();
  unique_ptr<bool []> base_streets(new bool[max_street + 1]);
  for (int st = 0; st <= max_street; ++st) {
    if (method == ResolvingMethod::UNSAFE) {
      // For unsafe method, don't need base probs outside trunk.
      base_streets[st] = st < resolve_st_;
    } else {
      base_streets[st] = true;
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
  base_sumprobs_.reset(new CFRValues(nullptr, base_streets.get(), 0, 0, base_buckets_,
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
  base_sumprobs_->Read(dir, base_it_, base_betting_trees_->GetBettingTree(), "x", -1, true, false);

  // We are calculating CBRs from the *base* strategy, not the resolved
  // endgame strategy.  So pass in base_card_abstraction_, etc.
  dynamic_cbr_.reset(new DynamicCBR(base_card_abstraction_, base_cfr_config_, base_buckets_, 1));
  dynamic_cbr_->SetSumprobs(base_sumprobs_);

  if (method_ == ResolvingMethod::UNSAFE) {
    trunk_hand_tree_.reset(new HandTree(0, 0, resolve_st_ - 1));
  } else {
    // For combined or CFR-D resolving, we need to compute CBR values.  This requires base
    // sumprobs for the whole game and correspondingly the trunk hand tree for the whole game.
    trunk_hand_tree_.reset(new HandTree(0, 0, max_street));
  }

  final_sumprobs_.reset(new CFRValues(nullptr, nullptr, 0, 0, final_buckets_,
				      final_betting_trees_->GetBettingTree()));
  unique_ptr<CFRValueType []> value_types(new CFRValueType[max_street + 1]);
  // Use doubles for all streets on which we are doing resolves.
  // For previous streets, use whatever type the base system uses.
  for (int st = 0; st <= max_street; ++st) {
    value_types[st] = (st >= resolve_st_ ? CFRValueType::CFR_DOUBLE :
		       (base_cfr_config_.DoubleSumprobs() ? CFRValueType::CFR_DOUBLE :
			CFRValueType::CFR_INT));
  }
  final_sumprobs_->AllocateAndClear(final_betting_trees_->GetBettingTree(), value_types.get(),
				    false, -1);

  for (int st = 0; st < resolve_st_; ++st) {
    for (int p = 0; p < 2; ++p) {
      int base_num_nt = base_betting_trees_->NumNonterminals(p, st);
      int final_num_nt = final_betting_trees_->NumNonterminals(p, st);
      if (base_num_nt != final_num_nt) {
	fprintf(stderr, "Base/final trunk num nt mismatch\n");
	exit(-1);
      }
    }
  }
  CopyTrunk(base_betting_trees_->Root());
  
  min_bets_.reset(new int[max_street + 1]);
  max_bets_.reset(new int[max_street + 1]);
  for (int st = 0; st < resolve_st_; ++st) {
    min_bets_[st] = 0;
    max_bets_[st] = 0;
  }
  for (int st = resolve_st_; st <= max_street; ++st) {
    // Min and max bets are the same
    min_bets_[st] = final_betting_abstraction_.MaxBets(st, true);
    max_bets_[st] = final_betting_abstraction_.MaxBets(st, true);
  }
}

// Copy the base values for the trunk to final_sumprobs_.
// There's an assumption that nodes in the base tree and the final tree have the same indices
// (in the trunk).
void SubgameSolver::CopyTrunk(Node *node) {
  int st = node->Street();
  if (st >= resolve_st_) return;
  if (node->Terminal()) return;
  int pa = node->PlayerActing();
  int nt = node->NonterminalID();
  int num_succs = node->NumSuccs();
  if (base_buckets_.None(st)) {
    CFRStreetValues<int> *base_csv =
      dynamic_cast<CFRStreetValues<int> *>(base_sumprobs_->StreetValues(st));
    CFRStreetValues<int> *final_csv =
      dynamic_cast<CFRStreetValues<int> *>(final_sumprobs_->StreetValues(st));
    int num_boards = BoardTree::NumBoards(st);
    for (int bd = 0; bd < num_boards; ++bd) {
      CopyUnabstractedValues(base_csv->AllValues(pa, nt), final_csv->AllValues(pa, nt), st,
			     num_succs, bd, bd);
    }
  } else {
    int num_buckets = base_buckets_.NumBuckets(st);
    CFRStreetValues<int> *from_csv =
      dynamic_cast<CFRStreetValues<int> *>(base_sumprobs_->StreetValues(st));
    CFRStreetValues<int> *to_csv =
      dynamic_cast<CFRStreetValues<int> *>(final_sumprobs_->StreetValues(st));
    int *from_values = from_csv->AllValues(pa, nt);
    int *to_values = to_csv->AllValues(pa, nt);
    int num = num_buckets * num_succs;
    for (int i = 0; i < num; ++i) {
      to_values[i] = from_values[i];
    }
  }
  for (int s = 0; s < num_succs; ++s) {
    CopyTrunk(node->IthSucc(s));
  }
}

shared_ptr<CFRValues> SubgameSolver::ResolveUnsafe(int gbd, const string &action_sequence,
						   const HandTree *hand_tree,
						   const ReachProbs &reach_probs,
						   BettingTrees *betting_trees) {
  UnsafeEGCFR eg_cfr(resolve_card_abstraction_, base_card_abstraction_,
		     base_betting_abstraction_, resolve_cfr_config_, base_cfr_config_,
		     resolve_buckets_, num_inner_threads_);
  int st = betting_trees->Root()->Street();
  if (st < Game::MaxStreet()) {
    eg_cfr.SetSplitStreet(st + 1);
  }

  // One solve for unsafe endgame solving, no t_vals
  if (hand_tree == nullptr) {
    fprintf(stderr, "NULL hand_tree\n");
    exit(-1);
  }
  eg_cfr.SolveSubgame(betting_trees, gbd, reach_probs, action_sequence, hand_tree,
		      nullptr, -1, true, num_resolve_its_);
  return eg_cfr.Sumprobs();
}

shared_ptr<CFRValues> SubgameSolver::ResolveSafe(int gbd, const string &action_sequence,
						 const HandTree *hand_tree,
						 const ReachProbs &reach_probs,
						 BettingTrees *betting_trees) {
  shared_ptr<CFRValues> sumprobs;
  return sumprobs;
}

static Node *FindNode(Node *node, const string &action_sequence) {
  if (action_sequence == "") return node;
  int num_succs = node->NumSuccs();
  for (int s = 0; s < num_succs; ++s) {
    string action = node->ActionName(s);
    if (! strncmp(action_sequence.c_str(), action.c_str(), (int)action.size())) {
      return FindNode(node->IthSucc(s), action_sequence.c_str() + action.size());
    }
  }
  return nullptr;
}

// Skip if we are already all in?
shared_ptr<CFRValues> SubgameSolver::Resolve(int gbd, const string &action_sequence,
					     const HandTree *resolve_hand_tree,
					     const ReachProbs &reach_probs,
					     BettingTrees *betting_trees) {
  shared_ptr<CFRValues> resolve_sumprobs;
  if (method_ == ResolvingMethod::UNSAFE) {
    resolve_sumprobs = ResolveUnsafe(gbd, action_sequence, resolve_hand_tree, reach_probs,
				     betting_trees);
  } else {
    resolve_sumprobs = ResolveSafe(gbd, action_sequence, resolve_hand_tree, reach_probs,
				   betting_trees);
  }
  return resolve_sumprobs;
}

class SSThread {
public:
  SSThread(int p, Node *node, Node *final_node, const string &action_sequence,
	   const string &resolve_action_sequence, int resolve_st, int nbps, int npb, int pgbd,
	   int resolve_bet_to, int resolve_gbd, const HandTree *resolve_hand_tree,
	   const ReachProbs &reach_probs, shared_ptr<CFRValues> resolve_sumprobs,
	   const ObservedBets &src_observed_bets, SubgameSolver *solver, int thread_index,
	   int num_threads);
  ~SSThread(void) {}
  void Run(void);
  void Join(void);
  void Go(void);
private:
  int p_;
  Node *node_;
  Node *final_node_;
  const string &action_sequence_;
  const string &resolve_action_sequence_;
  int resolve_st_;
  int npbs_;
  int npb_;
  int pgbd_;
  int resolve_bet_to_;
  int resolve_gbd_;
  const HandTree *resolve_hand_tree_;
  const ReachProbs &reach_probs_;
  shared_ptr<CFRValues> resolve_sumprobs_;
  unique_ptr<ObservedBets> observed_bets_;
  int num_bets_;
  SubgameSolver *solver_;
  int thread_index_;
  int num_threads_;
  pthread_t pthread_id_;
};

SSThread::SSThread(int p, Node *node, Node *final_node, const string &action_sequence,
		   const string &resolve_action_sequence, int resolve_st, int npbs, int npb,
		   int pgbd, int resolve_bet_to, int resolve_gbd, const HandTree *resolve_hand_tree,
		   const ReachProbs &reach_probs, shared_ptr<CFRValues> resolve_sumprobs,
		   const ObservedBets &src_observed_bets, SubgameSolver *solver, int thread_index,
		   int num_threads) :
  p_(p), node_(node), final_node_(final_node), action_sequence_(action_sequence),
  resolve_action_sequence_(resolve_action_sequence), resolve_st_(resolve_st), npbs_(npbs),
  npb_(npb), pgbd_(pgbd), resolve_gbd_(resolve_gbd), resolve_hand_tree_(resolve_hand_tree),
  reach_probs_(reach_probs), resolve_sumprobs_(resolve_sumprobs), solver_(solver),
  thread_index_(thread_index), num_threads_(num_threads) {
  observed_bets_.reset(new ObservedBets(src_observed_bets));
}

void SSThread::Go(void) {
  int nst = node_->Street();
  int pst = nst - 1;
  int ngbd_begin = BoardTree::SuccBoardBegin(pst, pgbd_, nst);
  int ngbd_end = BoardTree::SuccBoardEnd(pst, pgbd_, nst);
  for (int ngbd = ngbd_begin; ngbd < ngbd_end; ++ngbd) {
    if (ngbd % num_threads_ != thread_index_) continue;
    // Street initial
    if (nst == resolve_st_) {
      HandTree new_resolve_hand_tree(nst, ngbd, Game::MaxStreet());
      solver_->Walk(p_, node_, final_node_, action_sequence_, "", 0, 0, ngbd, node_->LastBetTo(),
		    ngbd, &new_resolve_hand_tree, reach_probs_, resolve_sumprobs_,
		    observed_bets_.get(), nst);
    } else {
      solver_->Walk(p_, node_, final_node_, action_sequence_, resolve_action_sequence_, npbs_, npb_,
		    ngbd, resolve_bet_to_, resolve_gbd_, resolve_hand_tree_, reach_probs_,
		    resolve_sumprobs_, observed_bets_.get(), nst);
    }
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

void SubgameSolver::Split(int p, Node *node, Node *final_node, const string &action_sequence,
			  const string &resolve_action_sequence, int npbs, int npb, int pgbd,
			  int resolve_bet_to, int resolve_gbd, const HandTree *resolve_hand_tree,
			  const ReachProbs &reach_probs, shared_ptr<CFRValues> resolve_sumprobs,
			  ObservedBets *observed_bets) {
  unique_ptr<SSThread * []> threads(new SSThread *[num_outer_threads_]);
  for (int t = 0; t < num_outer_threads_; ++t) {
    threads[t] = new SSThread(p, node, final_node, action_sequence, resolve_action_sequence,
			      resolve_st_, npbs, npb, pgbd, resolve_bet_to, resolve_gbd,
			      resolve_hand_tree, reach_probs, resolve_sumprobs, *observed_bets,
			      this, t, num_outer_threads_);
  }
  for (int t = 1; t < num_outer_threads_; ++t) threads[t]->Run();
  // Do first thread in main thread
  threads[0]->Go();
  for (int t = 1; t < num_outer_threads_; ++t) threads[t]->Join();
  for (int t = 0; t < num_outer_threads_; ++t) delete threads[t];
}

void SubgameSolver::StreetInitial(int p, Node *node, Node *final_node,
				  const string &action_sequence,
				  const string &resolve_action_sequence, int npbs, int npb,
				  int pgbd, int resolve_bet_to, int resolve_gbd,
				  const HandTree *resolve_hand_tree, const ReachProbs &reach_probs,
				  shared_ptr<CFRValues> resolve_sumprobs,
				  ObservedBets *observed_bets) {
  int nst = node->Street();
  if (nst == 1 && num_outer_threads_ > 1) {
    Split(p, node, final_node, action_sequence, resolve_action_sequence, npbs, npb, pgbd,
	  resolve_bet_to, resolve_gbd, resolve_hand_tree, reach_probs, resolve_sumprobs,
	  observed_bets);
  } else {
    int pst = node->Street() - 1;
    int ngbd_begin = BoardTree::SuccBoardBegin(pst, pgbd, nst);
    int ngbd_end = BoardTree::SuccBoardEnd(pst, pgbd, nst);
    for (int ngbd = ngbd_begin; ngbd < ngbd_end; ++ngbd) {
      if (nst == resolve_st_) {
	HandTree new_resolve_hand_tree(nst, ngbd, Game::MaxStreet());
	Walk(p, node, final_node, action_sequence, "", 0, 0, ngbd, node->LastBetTo(),
	     ngbd, &new_resolve_hand_tree, reach_probs, resolve_sumprobs, observed_bets, nst);
      } else {
	Walk(p, node, final_node, action_sequence, resolve_action_sequence, npbs, npb, ngbd,
	     resolve_bet_to, resolve_gbd, resolve_hand_tree, reach_probs, resolve_sumprobs,
	     observed_bets, nst);
      }
    }
  }
}

static Node *FindInternalNode(Node *node, const string &action_sequence,
			      const string &target_action_sequence) {
  if (action_sequence == target_action_sequence) return node;
  int num_succs = node->NumSuccs();
  for (int s = 0; s < num_succs; ++s) {
    string action = node->ActionName(s);
    Node *ret = FindInternalNode(node->IthSucc(s), action_sequence + action,
				 target_action_sequence);
    if (ret) return ret;
  }
  return nullptr;
}

// The node passed in will be from the base tree on streets prior to resolve_st_ and from
// a resolve tree on resolve_st_ and later.
void SubgameSolver::Walk(int p, Node *node, Node *final_node, const string &action_sequence,
			 const string &resolve_action_sequence, int npbs, int npb, int gbd,
			 int resolve_bet_to, int resolve_gbd, const HandTree *resolve_hand_tree,
			 const ReachProbs &reach_probs, shared_ptr<CFRValues> resolve_sumprobs,
			 ObservedBets *observed_bets, int last_st) {
  if (node->Terminal()) return;
  int st = node->Street();
  int pa = node->PlayerActing();
  int num_succs = node->NumSuccs();
  if (final_node->NumSuccs() != num_succs) {
    fprintf(stderr, "Mismatch num succs: final %i base/resolve %i\n", final_node->NumSuccs(),
	    num_succs);
    exit(-1);
  }

  if (st > last_st) {
    if (node->LastBetTo() == base_betting_abstraction_.StackSize()) {
      // No point doing resolving if we are already all-in
      return;
    }
    StreetInitial(p, node, final_node, action_sequence, resolve_action_sequence, npbs, npb, gbd,
		  resolve_bet_to, resolve_gbd, resolve_hand_tree, reach_probs, resolve_sumprobs,
		  observed_bets);
    return;
  }

  unique_ptr<BettingTrees> resolve_betting_trees;

  int csi = node->CallSuccIndex();
  int fsi = node->FoldSuccIndex();
  if (st >= resolve_st_ && num_succs > 1) {
    // Record a call as a zero-size bet.
    if (csi != -1) observed_bets->AddObservedBet(st, pa, npbs, npb, 0);
    for (int s = 0; s < num_succs; ++s) {
      if (s != fsi && s != csi) {
	int bet_size = node->IthSucc(s)->LastBetTo() - node->LastBetTo();
	observed_bets->AddObservedBet(st, pa, npbs, npb, bet_size);
      }
    }
    // Resolve if a) on resolve_st_ or later; b) not already all-in and c) either, opp choice
    // node or resolve-street-initial node.
    if (p != pa || resolve_sumprobs.get() == nullptr) {
      BackupBuilder builder(final_betting_abstraction_.StackSize());
      resolve_betting_trees.reset(builder.BuildTrees(*observed_bets, min_bets_.get(),
						     max_bets_.get(), resolve_st_, resolve_bet_to));
      int prefix_len = action_sequence.size() - resolve_action_sequence.size();
      string backup_action_sequence = string(action_sequence, 0, prefix_len);
      // resolve_betting_trees->GetBettingTree()->Display();
      fprintf(stderr, "Resolve %s st %i gbd %i (%s %i)\n", action_sequence.c_str(), st, gbd,
	      backup_action_sequence.c_str(), resolve_gbd);
      resolve_sumprobs = Resolve(resolve_gbd, backup_action_sequence, resolve_hand_tree,
				 reach_probs, resolve_betting_trees.get());
      node = FindInternalNode(resolve_betting_trees->Root(), "", resolve_action_sequence);
      if (node == nullptr) {
	fprintf(stderr, "Couldn't find internal resolve node: as %s ras %s rbt %i\n",
		action_sequence.c_str(), resolve_action_sequence.c_str(), resolve_bet_to);
	const BettingTree *betting_tree = resolve_betting_trees->GetBettingTree();
	betting_tree->Display();
	exit(-1);
      }
      // Can num_succs change?
      num_succs = node->NumSuccs();
      if (final_node->NumSuccs() != num_succs) {
	fprintf(stderr, "Mismatch after resolve with num succs: final %i resolve %i\n",
		final_node->NumSuccs(), num_succs);
	fprintf(stderr, "Action sequence: %s\n", action_sequence.c_str());
	fprintf(stderr, "Resolve action sequence: %s\n", resolve_action_sequence.c_str());
	fprintf(stderr, "Resolve bet to: %i\n", resolve_bet_to);
	const BettingTree *betting_tree = resolve_betting_trees->GetBettingTree();
	betting_tree->Display();
	exit(-1);
      }
    }
  }

  const CFRValues *sumprobs;
  const CanonicalCards *hands;
  Buckets *buckets;
  int lbd;
  if (resolve_sumprobs.get()) {
    sumprobs = resolve_sumprobs.get();
    hands = resolve_hand_tree->Hands(st, gbd);
    lbd = resolve_hand_tree->LocalBoardIndex(st, gbd);
    buckets = &resolve_buckets_;
  } else {
    sumprobs = base_sumprobs_.get();
    hands = trunk_hand_tree_->Hands(st, gbd);
    lbd = gbd;
    buckets = &base_buckets_;
  }
  shared_ptr<ReachProbs []> succ_reach_probs;
  if (st < resolve_st_) {
    succ_reach_probs =
      ReachProbs::CreateSuccReachProbs(node, gbd, lbd, hands, *buckets, sumprobs, reach_probs, 
				       false);
  }

  if (st >= resolve_st_ && num_succs > 1 && p == pa) {
    int st = node->Street();
    int pa = node->PlayerActing();
    int nt = node->NonterminalID();
    int final_nt = final_node->NonterminalID();
    int lbd = resolve_hand_tree->LocalBoardIndex(st, gbd);
    CFRStreetValues<double> *resolve_csv =
      dynamic_cast<CFRStreetValues<double> *>(resolve_sumprobs->StreetValues(st));
    CFRStreetValues<double> *final_csv =
      dynamic_cast<CFRStreetValues<double> *>(final_sumprobs_->StreetValues(st));
    CopyUnabstractedValues(resolve_csv->AllValues(pa, nt),
			   final_csv->AllValues(pa, final_nt), st, num_succs, lbd, gbd);
  }
  
  for (int s = 0; s < num_succs; ++s) {
    string action = node->ActionName(s);
    string new_resolve_action_sequence = st < resolve_st_ ? "" : resolve_action_sequence + action;
    // Only interested in bets on the resolve streets
    bool bet = st >= resolve_st_ && s != csi && s != fsi;
    int new_npbs = npbs + bet ? 1 : 0;
    int new_npb = npb + bet ? 1 : 0;
    const ReachProbs &next_reach_probs = st < resolve_st_ ? succ_reach_probs[s] : reach_probs;
    Walk(p, node->IthSucc(s), final_node->IthSucc(s), action_sequence + action,
	 new_resolve_action_sequence, new_npbs, new_npb, gbd, resolve_bet_to, resolve_gbd,
	 resolve_hand_tree, next_reach_probs, resolve_sumprobs, observed_bets, st);
  }

  // Remove the bet sizes we observed here
  if (st >= resolve_st_ && num_succs > 1) {
    observed_bets->Remove(st, pa, npbs, npb);
  }
}

void SubgameSolver::Go(void) {
  char dir[500];
  sprintf(dir, "%s/%s.%u.%s.%u.%u.%u.%s.%s", Files::NewCFRBase(), Game::GameName().c_str(),
	  Game::NumPlayers(), final_card_abstraction_.CardAbstractionName().c_str(),
	  Game::NumRanks(), Game::NumSuits(), Game::MaxStreet(),
	  final_betting_abstraction_.BettingAbstractionName().c_str(),
	  final_cfr_config_.CFRConfigName().c_str());
  RecursivelyDeleteDirectory(dir);
  for (int p = 0; p < 2; ++p) {
    ObservedBets observed_bets;
    unique_ptr<ReachProbs> reach_probs(ReachProbs::CreateRoot());
    shared_ptr<CFRValues> resolve_sumprobs;
    Walk(p, base_betting_trees_->Root(), final_betting_trees_->Root(), "x", "", -1, -1, 0, -1,
	 -1, nullptr, *reach_probs, resolve_sumprobs, &observed_bets, 0);
  }
  Mkdir(dir);
  final_sumprobs_->Write(dir, num_resolve_its_, final_betting_trees_->Root(), "x", -1, true);
}


static void Usage(const char *prog_name) {
  fprintf(stderr, "USAGE: %s <game params> <base card params> <resolve card params> "
	  "<final card params> <base betting params> <resolve betting params> <base CFR params> "
	  "<resolve CFR params> <final CFR params> <resolve street> <base it> <num resolve its> "
	  "[unsafe|cfrd|maxmargin|combined] <num inner threads> <num outer threads>\n", prog_name);
  fprintf(stderr, "\n");
  fprintf(stderr, "\"current\" or \"avg\" signifies whether we use the opponent's current strategy "
	  "(from regrets) in the subgame CBR calculation, or, as per usual, the avg strategy (from "
	  "sumprobs)\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "We support two different methods of multithreading.  The first type is the "
	  "multithreading inside of VCFR.  The second type is the multithreading inside of "
	  "solve_all_subgames.  <num inner threads> controls the first type; <num outer threads> "
	  "controls the second type.\n");
  exit(-1);
}

int main(int argc, char *argv[]) {
  if (argc != 16) Usage(argv[0]);
  Files::Init();
  unique_ptr<Params> game_params = CreateGameParams();
  game_params->ReadFromFile(argv[1]);
  Game::Initialize(*game_params);
  unique_ptr<Params> base_card_params = CreateCardAbstractionParams();
  base_card_params->ReadFromFile(argv[2]);
  unique_ptr<CardAbstraction>
    base_card_abstraction(new CardAbstraction(*base_card_params));
  unique_ptr<Params> resolve_card_params = CreateCardAbstractionParams();
  resolve_card_params->ReadFromFile(argv[3]);
  unique_ptr<CardAbstraction>
    resolve_card_abstraction(new CardAbstraction(*resolve_card_params));
  unique_ptr<Params> final_card_params = CreateCardAbstractionParams();
  final_card_params->ReadFromFile(argv[4]);
  unique_ptr<CardAbstraction>
    final_card_abstraction(new CardAbstraction(*final_card_params));
  unique_ptr<Params> base_betting_params = CreateBettingAbstractionParams();
  base_betting_params->ReadFromFile(argv[5]);
  unique_ptr<BettingAbstraction>
    base_betting_abstraction(new BettingAbstraction(*base_betting_params));
  unique_ptr<Params> final_betting_params = CreateBettingAbstractionParams();
  final_betting_params->ReadFromFile(argv[6]);
  unique_ptr<BettingAbstraction> final_betting_abstraction(
		   new BettingAbstraction(*final_betting_params));
  unique_ptr<Params> base_cfr_params = CreateCFRParams();
  base_cfr_params->ReadFromFile(argv[7]);
  unique_ptr<CFRConfig> base_cfr_config(new CFRConfig(*base_cfr_params));
  unique_ptr<Params> resolve_cfr_params = CreateCFRParams();
  resolve_cfr_params->ReadFromFile(argv[8]);
  unique_ptr<CFRConfig> resolve_cfr_config(new CFRConfig(*resolve_cfr_params));
  unique_ptr<Params> final_cfr_params = CreateCFRParams();
  final_cfr_params->ReadFromFile(argv[9]);
  unique_ptr<CFRConfig> final_cfr_config(new CFRConfig(*final_cfr_params));
  int resolve_st, base_it, num_resolve_its;
  if (sscanf(argv[10], "%i", &resolve_st) != 1)         Usage(argv[0]);
  if (sscanf(argv[11], "%i", &base_it) != 1)         Usage(argv[0]);
  if (sscanf(argv[12], "%i", &num_resolve_its) != 1) Usage(argv[0]);
  string m = argv[13];
  ResolvingMethod method;
  if (m == "unsafe")         method = ResolvingMethod::UNSAFE;
  else if (m == "cfrd")      method = ResolvingMethod::CFRD;
  else if (m == "maxmargin") method = ResolvingMethod::MAXMARGIN;
  else if (m == "combined")  method = ResolvingMethod::COMBINED;
  else                       Usage(argv[0]);
  int num_inner_threads, num_outer_threads;
  if (sscanf(argv[14], "%i", &num_inner_threads) != 1) Usage(argv[0]);
  if (sscanf(argv[15], "%i", &num_outer_threads) != 1) Usage(argv[0]);

  int max_street = Game::MaxStreet();
  if (num_inner_threads > 1 && resolve_st == max_street) {
    fprintf(stderr, "Can't have num_inner_threads > 1 if resolve_st == max_street\n");
    exit(-1);
  }
  
  // If card abstractions are the same, should not load both.
  Buckets base_buckets(*base_card_abstraction, false);
  Buckets resolve_buckets(*resolve_card_abstraction, false);

  BoardTree::Create();
  HandValueTree::Create();

  SubgameSolver solver(*base_card_abstraction, *resolve_card_abstraction, *final_card_abstraction,
		       *base_betting_abstraction, *final_betting_abstraction, *base_cfr_config,
		       *resolve_cfr_config, *final_cfr_config, base_buckets, resolve_buckets,
		       resolve_st, method, base_it, num_resolve_its, num_inner_threads,
		       num_outer_threads);
  solver.Go();
}
