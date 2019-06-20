// We are given three betting abstractions.
//   1) The base tree; e.g., mb1b1
//   2) The final tree; e.g., mb2b2tr
//   3) The resolve tree; e.g., mb1b1
// We take a previously solved system for the base betting abstraction and produce a new system
// for the final betting abstraction via resolving.  We resolve at each betting state at which
// there is divergence between the base betting abstraction and the final betting abstraction.
// The tree for resolving allows the richer set of actions from the final tree for the
// current betting state, but the set of actions from the resolve abstraction for future
// actions.
// Only support unsafe method for now.
// We find the minimal street on which the base tree and the final tree ever diverge.  We always
// resolve when we reach that street (even if the two trees don't diverge at that particular
// betting state).

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
#include "constants.h"
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
#include "subgame_utils.h" // WriteSubgame()
#include "unsafe_eg_cfr.h"
#include "vcfr.h"

using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;

class Resolver {
public:
  Resolver(const CardAbstraction &base_card_abstraction,
	   const CardAbstraction &resolve_card_abstraction,
	   const CardAbstraction &final_card_abstraction,
	   const BettingAbstraction &base_betting_abstraction,
	   const BettingAbstraction &resolve_betting_abstraction,
	   const BettingAbstraction &final_betting_abstraction,
	   const CFRConfig &base_cfr_config, const CFRConfig &resolve_cfr_config,
	   const CFRConfig &final_cfr_config, ResolvingMethod method, int base_it,
	   int num_resolve_its, int num_inner_threads, int num_outer_threads);
  ~Resolver(void) {}
  void Walk(Node *base_node, Node *resolve_node, Node *final_node, const string &action_sequence,
	    int gbd, const HandTree *hand_tree, const ReachProbs &reach_probs,
	    shared_ptr<CFRValues> resolve_sumprobs, int last_bet_size, int num_street_bets,
	    int num_bets, int num_players_to_act, int last_st);
  void Walk(void);
private:
  void Split(Node *base_node, Node *resolve_node, Node *final_node, const string &action_sequence,
	     int pgbd, const HandTree *hand_tree, const ReachProbs &reach_probs, int num_bets);
  void StreetInitial(Node *base_node, Node *resolve_node, Node *final_node,
		     const string &action_sequence, int pgbd, const HandTree *hand_tree,
		     const ReachProbs &reach_probs, shared_ptr<CFRValues> resolve_sumprobs,
		     int num_bets);
  shared_ptr<CFRValues> ResolveUnsafe(Node *node, int gbd, const string &action_sequence,
				      const HandTree *hand_tree, const ReachProbs &reach_probs,
				      BettingTrees *resolve_betting_trees);
  shared_ptr<CFRValues> ResolveCombined(Node *node, shared_ptr<CFRValues> prior_sumprobs, int gbd,
					const string &action_sequence,
					const HandTree *prior_hand_tree,
					const HandTree *new_hand_tree,
					const ReachProbs &reach_probs,
					BettingTrees *resolve_betting_trees);
  void CopyValues(Node *base_node, Node *resolve_node, Node *final_node, int gbd,
		  shared_ptr<CFRValues> resolve_sumprobs);
  int ResolveStreet(Node *base_node, Node *final_node);

  const CardAbstraction &base_card_abstraction_;
  const CardAbstraction &resolve_card_abstraction_;
  const CardAbstraction &final_card_abstraction_;
  const BettingAbstraction &base_betting_abstraction_;
  const BettingAbstraction &resolve_betting_abstraction_;
  const BettingAbstraction &final_betting_abstraction_;
  const CFRConfig &base_cfr_config_;
  const CFRConfig &resolve_cfr_config_;
  const CFRConfig &final_cfr_config_;
  Buckets base_buckets_;
  Buckets resolve_buckets_;
  Buckets final_buckets_;
  unique_ptr<BettingTrees> base_betting_trees_;
  unique_ptr<BettingTrees> resolve_betting_trees_;
  unique_ptr<BettingTrees> final_betting_trees_;
  unique_ptr<HandTree> trunk_hand_tree_;
  shared_ptr<CFRValues> base_sumprobs_;
  shared_ptr<CFRValues> final_sumprobs_;
  ResolvingMethod method_;
  int base_it_;
  int num_resolve_its_;
  int num_inner_threads_;
  int num_outer_threads_;
  int resolve_st_;
};

Resolver::Resolver(const CardAbstraction &base_card_abstraction,
		   const CardAbstraction &resolve_card_abstraction,
		   const CardAbstraction &final_card_abstraction,
		   const BettingAbstraction &base_betting_abstraction,
		   const BettingAbstraction &resolve_betting_abstraction,
		   const BettingAbstraction &final_betting_abstraction,
		   const CFRConfig &base_cfr_config, const CFRConfig &resolve_cfr_config,
		   const CFRConfig &final_cfr_config, ResolvingMethod method, int base_it,
		   int num_resolve_its, int num_inner_threads, int num_outer_threads) :
  base_card_abstraction_(base_card_abstraction),
  resolve_card_abstraction_(resolve_card_abstraction),
  final_card_abstraction_(final_card_abstraction),
  base_betting_abstraction_(base_betting_abstraction),
  resolve_betting_abstraction_(resolve_betting_abstraction),
  final_betting_abstraction_(final_betting_abstraction), base_cfr_config_(base_cfr_config),
  resolve_cfr_config_(resolve_cfr_config), final_cfr_config_(final_cfr_config),
  base_buckets_(base_card_abstraction, false), resolve_buckets_(resolve_card_abstraction, false),
  final_buckets_(final_card_abstraction, false), method_(method), base_it_(base_it),
  num_resolve_its_(num_resolve_its), num_inner_threads_(num_inner_threads),
  num_outer_threads_(num_outer_threads) {
  base_betting_trees_.reset(new BettingTrees(base_betting_abstraction_));
  final_betting_trees_.reset(new BettingTrees(final_betting_abstraction_));

  // We need probs for both players
  int max_street = Game::MaxStreet();
  // Loading sumprobs for the whole game currently.  May not need it for unsafe endgame solving.
  base_sumprobs_.reset(new CFRValues(nullptr, nullptr, 0, 0, base_buckets_,
				     base_betting_trees_->GetBettingTree()));
  char dir[500];
  sprintf(dir, "%s/%s.%u.%s.%i.%i.%i.%s.%s", Files::OldCFRBase(), Game::GameName().c_str(),
	  Game::NumPlayers(), base_card_abstraction_.CardAbstractionName().c_str(),
	  Game::NumRanks(), Game::NumSuits(), Game::MaxStreet(),
	  base_betting_abstraction_.BettingAbstractionName().c_str(),
	  base_cfr_config_.CFRConfigName().c_str());
  if (base_betting_abstraction_.Asymmetric()) {
    // Maybe move base_sumprob initialization to inside of loop over
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

  resolve_st_ = ResolveStreet(base_betting_trees_->Root(), final_betting_trees_->Root());
  if (resolve_st_ == kMaxInt) {
    fprintf(stderr, "Base and final betting trees are identical?!?\n");
    exit(-1);
#if 0
    // Temporary
    // resolve_st_ = Game::MaxStreet();
    resolve_st_ = 1;
#endif
  }
  fprintf(stderr, "Resolve st %i\n", resolve_st_);

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
}

// Return the earliest street on which base node diverges from final node.
int Resolver::ResolveStreet(Node *base_node, Node *final_node) {
  int num_succs = base_node->NumSuccs();
  if (final_node->NumSuccs() != num_succs) {
    return base_node->Street();
  }
  int min_st = kMaxInt;
  for (int s = 0; s < num_succs; ++s) {
    int st = ResolveStreet(base_node->IthSucc(s), final_node->IthSucc(s));
    if (st < min_st) min_st = st;
  }
  return min_st;
}

shared_ptr<CFRValues> Resolver::ResolveUnsafe(Node *node, int gbd, const string &action_sequence,
					      const HandTree *hand_tree,
					      const ReachProbs &reach_probs,
					      BettingTrees *resolve_betting_trees) {
  int st = node->Street();
  fprintf(stderr, "Resolve %s st %i nt %i gbd %i\n", action_sequence.c_str(), st,
	  node->NonterminalID(), gbd);

  UnsafeEGCFR eg_cfr(resolve_card_abstraction_, base_card_abstraction_,
		     base_betting_abstraction_, resolve_cfr_config_, base_cfr_config_,
		     resolve_buckets_, num_inner_threads_);
  if (st < Game::MaxStreet()) {
    eg_cfr.SetSplitStreet(st + 1);
  }

  // One solve for unsafe endgame solving, no t_vals
  eg_cfr.SolveSubgame(resolve_betting_trees, gbd, reach_probs, action_sequence, hand_tree,
		       nullptr, -1, true, num_resolve_its_);
  return eg_cfr.Sumprobs();
}

shared_ptr<CFRValues> Resolver::ResolveCombined(Node *node, shared_ptr<CFRValues> prior_sumprobs,
						int gbd, const string &action_sequence,
						const HandTree *prior_hand_tree,
						const HandTree *new_hand_tree,
						const ReachProbs &reach_probs,
						BettingTrees *resolve_betting_trees) {
  int st = node->Street();
  fprintf(stderr, "Resolve %s st %i nt %i gbd %i\n", action_sequence.c_str(), st,
	  node->NonterminalID(), gbd);

  CombinedEGCFR eg_cfr(resolve_card_abstraction_, base_card_abstraction_,
		       base_betting_abstraction_, resolve_cfr_config_, base_cfr_config_,
		       resolve_buckets_, false, true, num_inner_threads_);
  if (st < Game::MaxStreet()) {
    eg_cfr.SetSplitStreet(st + 1);
  }

  DynamicCBR dynamic_cbr(base_card_abstraction_, base_cfr_config_, base_buckets_, 1);
  dynamic_cbr.SetSumprobs(prior_sumprobs);

  shared_ptr<CFRValues> p0_sumprobs, p1_sumprobs;
  for (int solve_p = 0; solve_p < 2; ++solve_p) {
    shared_ptr<double []> t_vals = dynamic_cbr.Compute(node, reach_probs, gbd, prior_hand_tree,
						       solve_p^1, false, true, false, false);
    // fprintf(stderr, "solve_p %i t_vals[0] %f\n", solve_p, t_vals[0]);
    // exit(-1);

    // One solve for unsafe endgame solving, no t_vals
    eg_cfr.SolveSubgame(resolve_betting_trees, gbd, reach_probs, action_sequence, new_hand_tree,
			t_vals.get(), solve_p, false, num_resolve_its_);
    if (solve_p == 0) p0_sumprobs = eg_cfr.Sumprobs();
    else              p1_sumprobs = eg_cfr.Sumprobs();
  }
  // Combine the P0 and P1 sumprobs into a single object
  shared_ptr<CFRValues> resolved_sumprobs(new CFRValues(*p0_sumprobs.get(), *p1_sumprobs.get()));
  return resolved_sumprobs;
}

void Resolver::Split(Node *base_node, Node *resolve_node, Node *final_node,
		     const string &action_sequence, int pgbd, const HandTree *hand_tree,
		     const ReachProbs &reach_probs, int num_bets) {
#if 0
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
#endif
}

void Resolver::StreetInitial(Node *base_node, Node *resolve_node, Node *final_node,
			     const string &action_sequence, int pgbd, const HandTree *hand_tree,
			     const ReachProbs &reach_probs, shared_ptr<CFRValues> resolve_sumprobs,
			     int num_bets) {
  Node *node = base_node ? base_node : resolve_node;
  int nst = node->Street();
  if (nst == 1 && num_outer_threads_ > 1) {
    Split(base_node, resolve_node, final_node, action_sequence, pgbd, hand_tree, reach_probs,
	  num_bets);
  } else {
    int pst = node->Street() - 1;
    int ngbd_begin = BoardTree::SuccBoardBegin(pst, pgbd, nst);
    int ngbd_end = BoardTree::SuccBoardEnd(pst, pgbd, nst);
    for (int ngbd = ngbd_begin; ngbd < ngbd_end; ++ngbd) {
      Walk(base_node, resolve_node, final_node, action_sequence, ngbd, hand_tree, reach_probs,
	   resolve_sumprobs, 0, 0, num_bets, 2, nst);
    }
  }
}

// Could we take the hand tree and use its methods to get lbd?
void Resolver::CopyValues(Node *base_node, Node *resolve_node, Node *final_node, int gbd,
			  shared_ptr<CFRValues> resolve_sumprobs) {
  Node *node = base_node ? base_node : resolve_node;
  int st = node->Street();
  int num_succs = node->NumSuccs();
  int lbd;
  bool doubles;
  if (base_node) {
    doubles = dynamic_cast<CFRStreetValues<double> *>(base_sumprobs_->StreetValues(st));
    lbd = gbd;
  } else {
    if (dynamic_cast<CFRStreetValues<double> *>(resolve_sumprobs->StreetValues(st))) {
      doubles = true;
    } else if (dynamic_cast<CFRStreetValues<int> *>(resolve_sumprobs->StreetValues(st))) {
      doubles = false;
    } else {
      fprintf(stderr, "No resolve sumprobs for street %i?!?\n", st);
      exit(-1);
    }
    if (resolve_sumprobs.get() == nullptr) {
      fprintf(stderr, "Null resolve sumprobs?!?\n");
      exit(-1);
    }
    int root_st = resolve_sumprobs->RootSt();
    int root_bd = resolve_sumprobs->RootBd();
    lbd = BoardTree::LocalIndex(root_st, root_bd, st, gbd);
  }
  // Condense this with templates
  if (doubles) {
    // Copy strategy from base or resolve node into final_sumprobs_.
    double *from_values;
    if (base_node) {
      CFRStreetValues<double> *d_base_vals =
	dynamic_cast<CFRStreetValues<double> *>(base_sumprobs_->StreetValues(st));
      from_values = d_base_vals->AllValues(node->PlayerActing(), node->NonterminalID());
    } else {
      CFRStreetValues<double> *d_resolve_vals =
	dynamic_cast<CFRStreetValues<double> *>(resolve_sumprobs->StreetValues(st));
      from_values = d_resolve_vals->AllValues(node->PlayerActing(), node->NonterminalID());
    }
    CFRStreetValues<double> *d_final_vals =
      dynamic_cast<CFRStreetValues<double> *>(final_sumprobs_->StreetValues(st));
    if (d_final_vals) {
      double *to_values =
	d_final_vals->AllValues(final_node->PlayerActing(), final_node->NonterminalID());
      if (final_buckets_.None(st)) {
	int num_hole_card_pairs = Game::NumHoleCardPairs(st);
	int from_offset = lbd * num_hole_card_pairs * num_succs;
	int to_offset = gbd * num_hole_card_pairs * num_succs;
	for (int i = 0; i < num_hole_card_pairs; ++i) {
	  for (int s = 0; s < num_succs; ++s) {
	    int from_index = from_offset + i * num_succs + s;
	    int to_index = to_offset + i * num_succs + s;
	    to_values[to_index] = from_values[from_index];
	  }
	}
      } else {
	// int num_buckets = final_buckets_.NumBuckets(st);
	fprintf(stderr, "Not supported yet\n");
	exit(-1);
      }
    } else {
      fprintf(stderr, "Do we get here?\n");
      exit(-1);
      // Copying doubles into ints
      CFRStreetValues<int> *i_final_vals =
	dynamic_cast<CFRStreetValues<int> *>(final_sumprobs_->StreetValues(st));
      if (i_final_vals) {
	int *to_values =
	  i_final_vals->AllValues(final_node->PlayerActing(), final_node->NonterminalID());
	if (final_buckets_.None(st)) {
	  int num_hole_card_pairs = Game::NumHoleCardPairs(st);
	  int from_offset = lbd * num_hole_card_pairs * num_succs;
	  int to_offset = gbd * num_hole_card_pairs * num_succs;
	  for (int i = 0; i < num_hole_card_pairs; ++i) {
	    for (int s = 0; s < num_succs; ++s) {
	      int from_index = from_offset + i * num_succs + s;
	      int to_index = to_offset + i * num_succs + s;
	      // Scale up by 1000 so that when we cast to an int, low accumulated probs don't
	      // all get cast to 0.
	      to_values[to_index] = (int)(from_values[from_index] * 1000.0);
	    }
	  }
	} else {
	  // int num_buckets = final_buckets_.NumBuckets(st);
	  fprintf(stderr, "Not supported yet\n");
	  exit(-1);
	}
      } else {
	fprintf(stderr, "Final vals not ints or doubles?!?\n");
	exit(-1);
      }
    }
  } else {
    // Copy strategy from base or resolve node into final_sumprobs_.
    int *from_values;
    if (base_node) {
      CFRStreetValues<int> *i_base_vals =
	dynamic_cast<CFRStreetValues<int> *>(base_sumprobs_->StreetValues(st));
      from_values = i_base_vals->AllValues(node->PlayerActing(), node->NonterminalID());
    } else {
      CFRStreetValues<int> *i_resolve_vals =
	dynamic_cast<CFRStreetValues<int> *>(resolve_sumprobs->StreetValues(st));
      from_values = i_resolve_vals->AllValues(node->PlayerActing(), node->NonterminalID());
    }
    CFRStreetValues<int> *i_final_vals =
      dynamic_cast<CFRStreetValues<int> *>(final_sumprobs_->StreetValues(st));
    if (i_final_vals) {
      int *to_values =
	i_final_vals->AllValues(final_node->PlayerActing(), final_node->NonterminalID());
      if (final_buckets_.None(st)) {
	int num_hole_card_pairs = Game::NumHoleCardPairs(st);
	int from_offset = lbd * num_hole_card_pairs * num_succs;
	int to_offset = gbd * num_hole_card_pairs * num_succs;
	for (int i = 0; i < num_hole_card_pairs; ++i) {
	  for (int s = 0; s < num_succs; ++s) {
	    int from_index = from_offset + i * num_succs + s;
	    int to_index = to_offset + i * num_succs + s;
	    to_values[to_index] = from_values[from_index];
	  }
	}
      } else {
	// int num_buckets = final_buckets_.NumBuckets(st);
	fprintf(stderr, "Not supported yet\n");
	exit(-1);
      }
    } else {
      // Copying ints into doubles
      CFRStreetValues<double> *d_final_vals =
	dynamic_cast<CFRStreetValues<double> *>(final_sumprobs_->StreetValues(st));
      if (d_final_vals) {
	double *to_values =
	  d_final_vals->AllValues(final_node->PlayerActing(), final_node->NonterminalID());
	if (final_buckets_.None(st)) {
	  int num_hole_card_pairs = Game::NumHoleCardPairs(st);
	  int from_offset = lbd * num_hole_card_pairs * num_succs;
	  int to_offset = gbd * num_hole_card_pairs * num_succs;
	  for (int i = 0; i < num_hole_card_pairs; ++i) {
	    for (int s = 0; s < num_succs; ++s) {
	      int from_index = from_offset + i * num_succs + s;
	      int to_index = to_offset + i * num_succs + s;
	      to_values[to_index] = from_values[from_index];
	    }
	  }
	} else {
	  // int num_buckets = final_buckets_.NumBuckets(st);
	  fprintf(stderr, "Not supported yet\n");
	  exit(-1);
	}
      } else {
	fprintf(stderr, "final_sumprobs_ not ints or doubles at street %i?!?\n", st);
	exit(-1);
      }
    }
  }
}

void Resolver::Walk(Node *base_node, Node *resolve_node, Node *final_node,
		    const string &action_sequence, int gbd, const HandTree *hand_tree,
		    const ReachProbs &reach_probs, shared_ptr<CFRValues> resolve_sumprobs,
		    int last_bet_size, int num_street_bets, int num_bets, int num_players_to_act,
		    int last_st) {
  Node *node = base_node ? base_node : resolve_node;
  if (node->Terminal()) return;
  int num_succs = node->NumSuccs();
  int st = node->Street();
  if (st > last_st) {
    if (node->LastBetTo() == final_betting_abstraction_.StackSize()) {
      // No point doing resolving if we are already all-in
      return;
    }
    StreetInitial(base_node, resolve_node, final_node, action_sequence, gbd, hand_tree,
		  reach_probs, resolve_sumprobs, num_bets);
    return;
  }

  unique_ptr<BettingTrees> resolve_betting_trees;
  unique_ptr<HandTree> resolve_hand_tree;
  // Default is that new_hand_tree is the same as the hand tree passed in
  const HandTree *new_hand_tree = hand_tree;
  
  // For resolve when either:
  // a) The final betting tree diverges from the current betting tree (base or resolved); or
  // b) We reach the resolve street for the first time.
  // if (num_succs != final_node->NumSuccs() || (st == resolve_st_ && base_node)) {
  // Only resolve at P0 nodes on resolve street after P1 bet; e.g., cb4
  // if (st == resolve_st_ && base_node && base_node->PlayerActing() == 0 && last_bet_size > 0) {
  // Resolve on resolve street after P0 open check
  // if (st == resolve_st_ && base_node && base_node->PlayerActing() == 1 && last_bet_size == 0) {
  // Resolve on resolve street after P0 open bet
  // if (st == resolve_st_ && base_node && base_node->PlayerActing() == 1 && last_bet_size > 0) {
  // Resolve all P1 nodes on flop; root, c, b.
  // if (st == resolve_st_ && node->PlayerActing() == 0) {
  // Always resolve on resolve_st
  // if (st == resolve_st_) {
  // Only resolve at street-initial nodes on resolve street
  // if (st == resolve_st_ && base_node) {
  // Always resolve on resolve_st and later streets
  if (st >= resolve_st_) {
    // Skip if we are already all in?  Be careful with testing this condition.  After an
    // all-in bet that has not yet been called, we still want to resolve.
    // I think BuildHybridTree() only supports heads-up
    resolve_betting_trees.reset(BuildHybridTree(resolve_betting_abstraction_, -1, final_node,
						last_bet_size, num_street_bets, num_bets));
    resolve_hand_tree.reset(new HandTree(st, gbd, Game::MaxStreet()));
    if (method_ == ResolvingMethod::UNSAFE) {
      resolve_sumprobs = ResolveUnsafe(node, gbd, action_sequence, resolve_hand_tree.get(),
				       reach_probs, resolve_betting_trees.get());
    } else if (method_ == ResolvingMethod::COMBINED) {
      shared_ptr<CFRValues> sumprobs = base_node ? base_sumprobs_ : resolve_sumprobs;
      // The hand tree passed into Walk() is the prior hand tree and matches the sumprobs
      // we passed into Walk().
      resolve_sumprobs = ResolveCombined(node, sumprobs, gbd, action_sequence, hand_tree,
					 resolve_hand_tree.get(), reach_probs,
					 resolve_betting_trees.get());
    } else {
      fprintf(stderr, "Unknown method: %i\n", (int)method_);
      exit(-1);
    }
    // Change the nodes and the hand tree so that we start processing the resolve tree
    base_node = nullptr;
    resolve_node = resolve_betting_trees->Root();
    new_hand_tree = resolve_hand_tree.get();
  }

  CopyValues(base_node, resolve_node, final_node, gbd, resolve_sumprobs);

  const CanonicalCards *hands = new_hand_tree->Hands(st, gbd);
  shared_ptr<ReachProbs []> succ_reach_probs;
  if (base_node) {
    succ_reach_probs = ReachProbs::CreateSuccReachProbs(base_node, gbd, gbd, hands, base_buckets_,
							base_sumprobs_.get(), reach_probs, false);
  } else {
    succ_reach_probs = ReachProbs::CreateSuccReachProbs(resolve_node, gbd, 0, hands,
							resolve_buckets_, resolve_sumprobs.get(),
							reach_probs, false);
  }

  // Note: need to reset node because we may have "switched trees" due to a resolve above
  node = base_node ? base_node : resolve_node;
  num_succs = node->NumSuccs();
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
    Node *new_base_node = base_node ? base_node->IthSucc(s) : nullptr;
    Node *new_resolve_node = resolve_node ? resolve_node->IthSucc(s) : nullptr;
    Walk(new_base_node, new_resolve_node, final_node->IthSucc(s),
	 action_sequence + action, gbd, new_hand_tree, succ_reach_probs[s], resolve_sumprobs,
	 last_bet_size, new_num_street_bets, new_num_bets, 1, st);
  }
}

void Resolver::Walk(void) {
  fprintf(stderr, "Walk\n");
  char dir[500];
  sprintf(dir, "%s/%s.%u.%s.%u.%u.%u.%s.%s", Files::NewCFRBase(), Game::GameName().c_str(),
	  Game::NumPlayers(), final_card_abstraction_.CardAbstractionName().c_str(),
	  Game::NumRanks(), Game::NumSuits(), Game::MaxStreet(),
	  final_betting_abstraction_.BettingAbstractionName().c_str(),
	  final_cfr_config_.CFRConfigName().c_str());
  if (final_betting_abstraction_.Asymmetric()) {
    fprintf(stderr, "Asymmetric not supported yet\n");
    exit(-1);
    // char buf[100];
    // sprintf(buf, ".p%u", target_p_);
    // strcat(dir, buf);
  }
  RecursivelyDeleteDirectory(dir);
  int last_bet_size = Game::BigBlind() - Game::SmallBlind();
  unique_ptr<ReachProbs> reach_probs(ReachProbs::CreateRoot());
  shared_ptr<CFRValues> resolve_sumprobs;
  Walk(base_betting_trees_->Root(), nullptr, final_betting_trees_->Root(),
       "x", 0, trunk_hand_tree_.get(), *reach_probs, resolve_sumprobs, last_bet_size, 0, 0, 2, 0);
  fprintf(stderr, "Done with Walk\n");
  Mkdir(dir);
  final_sumprobs_->Write(dir, num_resolve_its_, final_betting_trees_->Root(), "x", -1, true);
}

static void Usage(const char *prog_name) {
  fprintf(stderr, "USAGE: %s <game params> <base card params> <resolve card params> "
	  "<final card params> <base betting params> <resolve betting params> "
	  "<final betting params> <base CFR params> <resolve CFR params> <final CFR params> "
	  "<method> <base it> <num resolve its> <num inner threads> <num outer threads>\n",
	  prog_name);
  fprintf(stderr, "\n");
  fprintf(stderr, "Method is \"unsafe\" or \"combined\"\n");
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
  unique_ptr<Params> resolve_betting_params = CreateBettingAbstractionParams();
  resolve_betting_params->ReadFromFile(argv[6]);
  unique_ptr<BettingAbstraction> resolve_betting_abstraction(
		   new BettingAbstraction(*resolve_betting_params));
  unique_ptr<Params> final_betting_params = CreateBettingAbstractionParams();
  final_betting_params->ReadFromFile(argv[7]);
  unique_ptr<BettingAbstraction> final_betting_abstraction(
		   new BettingAbstraction(*final_betting_params));
  unique_ptr<Params> base_cfr_params = CreateCFRParams();
  base_cfr_params->ReadFromFile(argv[8]);
  unique_ptr<CFRConfig> base_cfr_config(new CFRConfig(*base_cfr_params));
  unique_ptr<Params> resolve_cfr_params = CreateCFRParams();
  resolve_cfr_params->ReadFromFile(argv[9]);
  unique_ptr<CFRConfig> resolve_cfr_config(new CFRConfig(*resolve_cfr_params));
  unique_ptr<Params> final_cfr_params = CreateCFRParams();
  final_cfr_params->ReadFromFile(argv[10]);
  unique_ptr<CFRConfig> final_cfr_config(new CFRConfig(*final_cfr_params));
  ResolvingMethod method;
  string m = argv[11];
  if (m == "unsafe")        method = ResolvingMethod::UNSAFE;
  else if (m == "combined") method = ResolvingMethod::COMBINED;
  else                      Usage(argv[0]);
  int base_it, num_resolve_its;
  if (sscanf(argv[12], "%i", &base_it) != 1)         Usage(argv[0]);
  if (sscanf(argv[13], "%i", &num_resolve_its) != 1) Usage(argv[0]);
  int num_inner_threads, num_outer_threads;
  if (sscanf(argv[14], "%i", &num_inner_threads) != 1) Usage(argv[0]);
  if (sscanf(argv[15], "%i", &num_outer_threads) != 1) Usage(argv[0]);

  BoardTree::Create();
  HandValueTree::Create();

  Resolver solver(*base_card_abstraction, *resolve_card_abstraction, *final_card_abstraction,
		  *base_betting_abstraction, *resolve_betting_abstraction,
		  *final_betting_abstraction, *base_cfr_config, *resolve_cfr_config,
		  *final_cfr_config, method, base_it, num_resolve_its, num_inner_threads,
		  num_outer_threads);
  for (int asym_p = 0; asym_p <= 1; ++asym_p) {
    DeleteAllSubgames(*base_card_abstraction, *resolve_card_abstraction, *base_betting_abstraction,
		      *resolve_betting_abstraction, *base_cfr_config, *resolve_cfr_config,
		      ResolvingMethod::UNSAFE, asym_p);
  }
  solver.Walk();
}
