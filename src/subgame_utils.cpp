// There are up to four subgame strategies written per solve, which can be
// confusing.  If we are solving an asymmetric game, then there will be
// a choice of which player's game we are solving, which affects the betting
// tree.  In addition, regardless of whether we are solving a symmetric or
// asymmetric game, we have to write out both player's strategies in the
// subgame.  We refer to the first player parameter as the asym_p and the
// second player parameter as the target_pa.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <memory>
#include <string>

#include "betting_abstraction.h"
#include "betting_tree.h"
#include "betting_trees.h"
#include "betting_tree_builder.h"
#include "board_tree.h"
#include "buckets.h"
#include "canonical_cards.h"
#include "card_abstraction.h"
#include "cfr_config.h"
#include "cfr_values.h"
#include "subgame_utils.h"
#include "files.h"
#include "game.h"
#include "hand_tree.h"
#include "io.h"
#include "resolving_method.h"

using std::shared_ptr;
using std::string;
using std::unique_ptr;

// Create a subtree for resolving rooted at the node (from a base tree) passed in.
// Assumes the subtree is rooted at a street-initial node.
// Doesn't support asymmetric yet
// Doesn't support multiplayer yet (?)
BettingTrees *CreateSubtrees(int st, int player_acting, int last_bet_to, int target_p,
			     const BettingAbstraction &betting_abstraction) {
  // Assume subtree rooted at street-initial node
  int last_bet_size = 0;
  int num_street_bets = 0;
  BettingTreeBuilder betting_tree_builder(betting_abstraction, target_p);
  int num_terminals = 0;
  // Should call routine from mp_betting_tree.cpp instead
  shared_ptr<Node> subtree_root =
    betting_tree_builder.CreateNoLimitSubtree(st, last_bet_size, last_bet_to, num_street_bets,
					      player_acting, target_p, &num_terminals);
  // Delete the nodes under subtree_root?  Or does garbage collection
  // automatically take care of it because they are shared pointers.
  return new BettingTrees(subtree_root.get());
}

// I always load probabilities for both players because I need the reach
// probabilities for both players.  In addition, as long as we are
// zero-summing the T-values, we need the strategy for both players for the
// CBR computation.
// Actually: for normal subgame solving, I think I only need both players'
// strategies if we are zero-summing.
unique_ptr<CFRValues> ReadBaseSubgameStrategy(const CardAbstraction &base_card_abstraction,
					      const BettingAbstraction &base_betting_abstraction,
					      const CFRConfig &base_cfr_config,
					      const BettingTrees *base_betting_trees,
					      const Buckets &base_buckets,
					      const Buckets &subgame_buckets, int base_it,
					      Node *base_node, int gbd,
					      const string &action_sequence, double **reach_probs,
					      BettingTree *subtree, bool current, int asym_p) {

  // We need probs for subgame streets only
  int max_street = Game::MaxStreet();
  unique_ptr<bool []> subgame_streets(new bool[max_street + 1]);
  for (int st = 0; st <= max_street; ++st) {
    subgame_streets[st] = st >= base_node->Street();
  }
  unique_ptr<CFRValues> strategy(new CFRValues(nullptr, subgame_streets.get(),
					       gbd, base_node->Street(), base_buckets,
					       base_betting_trees->GetBettingTree()));

  char dir[500];
  sprintf(dir, "%s/%s.%u.%s.%i.%i.%i.%s.%s", Files::OldCFRBase(),
	  Game::GameName().c_str(), Game::NumPlayers(),
	  base_card_abstraction.CardAbstractionName().c_str(),
	  Game::NumRanks(),
	  Game::NumSuits(), Game::MaxStreet(),
	  base_betting_abstraction.BettingAbstractionName().c_str(),
	  base_cfr_config.CFRConfigName().c_str());
  if (base_betting_abstraction.Asymmetric()) {
    char buf[100];
    sprintf(buf, ".p%u", asym_p);
    strcat(dir, buf);
  }

  unique_ptr<int []> num_full_holdings(new int[max_street + 1]);
  for (int st = 0; st <= max_street; ++st) {
    if (subgame_buckets.None(st)) {
      num_full_holdings[st] =
	BoardTree::NumBoards(st) * Game::NumHoleCardPairs(st);
    } else {
      num_full_holdings[st] = subgame_buckets.NumBuckets(st);
    }
  }

  fprintf(stderr, "Need to implement ReadSubtreeFromFull()\n");
  exit(-1);
#if 0
  strategy->ReadSubtreeFromFull(dir, base_it, base_betting_trees->Root(),
				base_node, subtree->Root(), action_sequence,
				num_full_holdings.get(), -1);
#endif
  
  return strategy;
}

// Only write out strategy for nodes at or below below_action_sequence.
void WriteSubgame(Node *node, const string &action_sequence, const string &below_action_sequence,
		  int gbd, const CardAbstraction &base_card_abstraction,
		  const CardAbstraction &subgame_card_abstraction,
		  const BettingAbstraction &base_betting_abstraction,
		  const BettingAbstraction &subgame_betting_abstraction,
		  const CFRConfig &base_cfr_config, const CFRConfig &subgame_cfr_config,
		  ResolvingMethod method, const CFRValues *sumprobs, int root_bd_st, int root_bd,
		  int asym_p, int target_pa, int last_st) {
  if (node->Terminal()) return;
  int st = node->Street();
  if (st > last_st) {
    int ngbd_begin = BoardTree::SuccBoardBegin(last_st, gbd, st);
    int ngbd_end = BoardTree::SuccBoardEnd(last_st, gbd, st);
    for (int ngbd = ngbd_begin; ngbd < ngbd_end; ++ngbd) {
      WriteSubgame(node, action_sequence, below_action_sequence, ngbd, base_card_abstraction,
		   subgame_card_abstraction, base_betting_abstraction, subgame_betting_abstraction,
		   base_cfr_config, subgame_cfr_config, method, sumprobs, root_bd_st, root_bd,
		   asym_p, target_pa, st);
    }
    return;
  }
  int num_succs = node->NumSuccs();
  if (node->PlayerActing() == target_pa && num_succs > 1) {
    if (below_action_sequence.size() <= action_sequence.size() &&
	std::equal(below_action_sequence.begin(), below_action_sequence.end(),
		   action_sequence.begin())) {
      char dir[500], dir2[500], dir3[500], filename[500];
      sprintf(dir, "%s/%s.%u.%s.%u.%u.%u.%s.%s", Files::NewCFRBase(),
	      Game::GameName().c_str(), Game::NumPlayers(),
	      base_card_abstraction.CardAbstractionName().c_str(),
	      Game::NumRanks(), Game::NumSuits(), Game::MaxStreet(),
	      base_betting_abstraction.BettingAbstractionName().c_str(),
	      base_cfr_config.CFRConfigName().c_str());
      if (base_betting_abstraction.Asymmetric()) {
	sprintf(dir2, "%s.p%u/subgames.%s.%s.%s.%s.p%u.p%u", dir, asym_p,
		subgame_card_abstraction.CardAbstractionName().c_str(),
		subgame_betting_abstraction.BettingAbstractionName().c_str(),
		subgame_cfr_config.CFRConfigName().c_str(),
		ResolvingMethodName(method), asym_p, target_pa);
      } else {
	sprintf(dir2, "%s/subgames.%s.%s.%s.%s.p%u", dir, 
		subgame_card_abstraction.CardAbstractionName().c_str(),
		subgame_betting_abstraction.BettingAbstractionName().c_str(),
		subgame_cfr_config.CFRConfigName().c_str(),
		ResolvingMethodName(method), target_pa);
      }
      Mkdir(dir2);
    
      if (action_sequence == "") {
	fprintf(stderr, "Empty action sequence not allowed\n");
	exit(-1);
      }
      sprintf(dir3, "%s/%s", dir2, action_sequence.c_str());
      Mkdir(dir3);

      sprintf(filename, "%s/%u", dir3, gbd);

      // If we resolve more than one street, things get a little tricky.  We are writing one
      // file per final-street board, but this sumprobs object will contain more than one
      // board's data.
      Writer writer(filename);
      int num_hole_card_pairs = Game::NumHoleCardPairs(node->Street());
      int lbd = BoardTree::LocalIndex(root_bd_st, root_bd, st, gbd);
      sumprobs->WriteBoardValuesForNode(node, &writer, nullptr, lbd, num_hole_card_pairs);
    }
  }

  for (int s = 0; s < num_succs; ++s) {
    string action = node->ActionName(s);
    WriteSubgame(node->IthSucc(s), action_sequence + action,
		 below_action_sequence, gbd,
		 base_card_abstraction, subgame_card_abstraction,
		 base_betting_abstraction, subgame_betting_abstraction,
		 base_cfr_config, subgame_cfr_config, method, sumprobs,
		 root_bd_st, root_bd, asym_p, target_pa, st);
  }
}

static void ReadSubgame(Node *node, const string &action_sequence, int gbd,
			const CardAbstraction &base_card_abstraction,
			const CardAbstraction &subgame_card_abstraction,
			const BettingAbstraction &base_betting_abstraction,
			const BettingAbstraction &subgame_betting_abstraction,
			const CFRConfig &base_cfr_config, const CFRConfig &subgame_cfr_config,
			ResolvingMethod method,	CFRValues *sumprobs, int root_bd_st, int root_bd,
			int asym_p, int target_pa, int last_st) {
  if (node->Terminal()) {
    return;
  }
  int st = node->Street();
  if (st > last_st) {
    int ngbd_begin = BoardTree::SuccBoardBegin(last_st, gbd, st);
    int ngbd_end = BoardTree::SuccBoardEnd(last_st, gbd, st);
    for (int ngbd = ngbd_begin; ngbd < ngbd_end; ++ngbd) {
      ReadSubgame(node, action_sequence, ngbd, base_card_abstraction,
		  subgame_card_abstraction, base_betting_abstraction,
		  subgame_betting_abstraction, base_cfr_config,
		  subgame_cfr_config, method, sumprobs, root_bd_st, root_bd,
		  asym_p, target_pa, st);
    }
    return;
  }
  int num_succs = node->NumSuccs();
  if (node->PlayerActing() == target_pa && num_succs > 1) {
    char dir[500], dir2[500], dir3[500], filename[500];
    sprintf(dir, "%s/%s.%u.%s.%u.%u.%u.%s.%s", Files::NewCFRBase(),
	    Game::GameName().c_str(), Game::NumPlayers(),
	    base_card_abstraction.CardAbstractionName().c_str(),
	    Game::NumRanks(), Game::NumSuits(), Game::MaxStreet(),
	    base_betting_abstraction.BettingAbstractionName().c_str(),
	    base_cfr_config.CFRConfigName().c_str());
    if (base_betting_abstraction.Asymmetric()) {
      sprintf(dir2, "%s.p%u/subgames.%s.%s.%s.%s.p%u.p%u", dir, asym_p,
	      subgame_card_abstraction.CardAbstractionName().c_str(),
	      subgame_betting_abstraction.BettingAbstractionName().c_str(),
	      subgame_cfr_config.CFRConfigName().c_str(),
	      ResolvingMethodName(method), asym_p, target_pa);
    } else {
      sprintf(dir2, "%s/subgames.%s.%s.%s.%s.p%u", dir, 
	      subgame_card_abstraction.CardAbstractionName().c_str(),
	      subgame_betting_abstraction.BettingAbstractionName().c_str(),
	      subgame_cfr_config.CFRConfigName().c_str(),
	      ResolvingMethodName(method), target_pa);
    }
    
    if (action_sequence == "") {
      fprintf(stderr, "Empty action sequence not allowed\n");
      exit(-1);
    }
    sprintf(dir3, "%s/%s", dir2, action_sequence.c_str());

    sprintf(filename, "%s/%u", dir3, gbd);

    Reader reader(filename);
    // Assume doubles in file
    // Also assume subgame solving is unabstracted
    // We write only one board's data per file, even on streets later than
    // solve street.
    int lbd = BoardTree::LocalIndex(root_bd_st, root_bd, st, gbd);
    int num_hole_card_pairs = Game::NumHoleCardPairs(node->Street());
    sumprobs->ReadBoardValuesForNode(node, &reader, nullptr, lbd, num_hole_card_pairs);
    if (! reader.AtEnd()) {
      fprintf(stderr, "Reader didn't get to end; pos %lli size %lli\nFile: %s\n",
	      reader.BytePos(), reader.FileSize(), filename);
      exit(-1);	      
    }
  }

  for (int s = 0; s < num_succs; ++s) {
    string action = node->ActionName(s);
    ReadSubgame(node->IthSucc(s), action_sequence + action, gbd, base_card_abstraction,
		subgame_card_abstraction, base_betting_abstraction, subgame_betting_abstraction,
		base_cfr_config, subgame_cfr_config, method, sumprobs, root_bd_st, root_bd, asym_p,
		target_pa, st);
  }
}

// I always load probabilities for both players because I need the reach
// probabilities for both players in case we are performing nested subgame
// solving.  In addition, as long as we are zero-summing the T-values, we
// need the strategy for both players for the CBR computation.
unique_ptr<CFRValues> ReadSubgame(const string &action_sequence, BettingTrees *subtrees, int gbd,
				  const CardAbstraction &base_card_abstraction,
				  const CardAbstraction &subgame_card_abstraction,
				  const BettingAbstraction &base_betting_abstraction,
				  const BettingAbstraction &subgame_betting_abstraction,
				  const CFRConfig &base_cfr_config,
				  const CFRConfig &subgame_cfr_config,
				  const Buckets &subgame_buckets, ResolvingMethod method,
				  int root_bd_st, int root_bd, int asym_p) {
  int max_street = Game::MaxStreet();
  unique_ptr<bool []> subgame_streets(new bool[max_street + 1]);
  for (int st = 0; st <= max_street; ++st) {
    // Assume symmetric?
    subgame_streets[st] = st >= subtrees->Root()->Street();
  }

  // Buckets needed for num buckets and for knowing what streets are
  // bucketed.
  unique_ptr<CFRValues> sumprobs(new CFRValues(nullptr, subgame_streets.get(), gbd,
					       subtrees->Root()->Street(), subgame_buckets,
					       subtrees->GetBettingTree()));
  // Assume doubles
  for (int st = 0; st <= max_street; ++st) {
    if (subgame_streets[st]) sumprobs->CreateStreetValues(st, CFR_DOUBLE);
  }

  for (int target_pa = 0; target_pa <= 1; ++target_pa) {
    ReadSubgame(subtrees->Root(), action_sequence, gbd, base_card_abstraction,
		subgame_card_abstraction, base_betting_abstraction,
		subgame_betting_abstraction, base_cfr_config,
		subgame_cfr_config, method, sumprobs.get(), root_bd_st, root_bd,
		asym_p, target_pa, subtrees->Root()->Street());
  }
  
  return sumprobs;
}

// Hard-coded for heads-up
shared_ptr<double []> **GetSuccReachProbs(Node *node, int gbd, HandTree *hand_tree,
					  const Buckets &buckets, const CFRValues *sumprobs,
					  shared_ptr<double []> *reach_probs, int root_bd_st,
					  int root_bd, bool purify) {
  int num_succs = node->NumSuccs();
  shared_ptr<double []> **succ_reach_probs = new shared_ptr<double []> *[num_succs];
  int max_card1 = Game::MaxCard() + 1;
  int num_enc = max_card1 * max_card1;
  int st = node->Street();
  int num_hole_card_pairs = Game::NumHoleCardPairs(st);
  int lbd = BoardTree::LocalIndex(root_bd_st, root_bd, st, gbd);
  const CanonicalCards *hands = hand_tree->Hands(st, lbd);
  for (int s = 0; s < num_succs; ++s) {
    succ_reach_probs[s] = new shared_ptr<double []>[2];
    for (int p = 0; p < 2; ++p) {
      succ_reach_probs[s][p].reset(new double[num_enc]);
    }
  }
  // Can happen when we are all-in.  Only succ is check.
  if (num_succs == 1) {
    for (int i = 0; i < num_hole_card_pairs; ++i) {
      const Card *cards = hands->Cards(i);
      Card hi = cards[0];
      Card lo = cards[1];
      int enc = hi * max_card1 + lo;
      for (int p = 0; p <= 1; ++p) {
	succ_reach_probs[0][p][enc] = reach_probs[p][enc];
      }
    }
    return succ_reach_probs;
  }
  int pa = node->PlayerActing();
  int nt = node->NonterminalID();
  int dsi = node->DefaultSuccIndex();
  unique_ptr<double []> probs(new double[num_succs]);
  for (int i = 0; i < num_hole_card_pairs; ++i) {
    const Card *cards = hands->Cards(i);
    Card hi = cards[0];
    Card lo = cards[1];
    int enc = hi * max_card1 + lo;
    int offset;
    if (buckets.None(st)) {
      offset = lbd * num_hole_card_pairs * num_succs + i * num_succs;
    } else {
      unsigned int h = ((unsigned int)lbd) * ((unsigned int)num_hole_card_pairs) + i;
      int b = buckets.Bucket(st, h);
      offset = b * num_succs;
    }
    if (purify) {
      sumprobs->PureProbs(st, pa, nt, offset, num_succs, probs.get());
    } else {
      sumprobs->RMProbs(st, pa, nt, offset, num_succs, dsi, probs.get());
    }
    for (int s = 0; s < num_succs; ++s) {
      for (int p = 0; p <= 1; ++p) {
	if (p == pa) {
	  succ_reach_probs[s][p][enc] = reach_probs[p][enc] * probs[s];
	} else {
	  succ_reach_probs[s][p][enc] = reach_probs[p][enc];
	}
      }
    }
  }
  
  return succ_reach_probs;
}

void DeleteAllSubgames(const CardAbstraction &base_card_abstraction,
		       const CardAbstraction &subgame_card_abstraction,
		       const BettingAbstraction &base_betting_abstraction,
		       const BettingAbstraction &subgame_betting_abstraction,
		       const CFRConfig &base_cfr_config, const CFRConfig &subgame_cfr_config,
		       ResolvingMethod method, int asym_p) {
  char dir[500], dir2[500];
  sprintf(dir, "%s/%s.%s.%u.%u.%u.%s.%s", Files::OldCFRBase(),
	  Game::GameName().c_str(),
	  base_card_abstraction.CardAbstractionName().c_str(),
	  Game::NumRanks(), Game::NumSuits(), Game::MaxStreet(),
	  base_betting_abstraction.BettingAbstractionName().c_str(),
	  base_cfr_config.CFRConfigName().c_str());
  if (base_betting_abstraction.Asymmetric()) {
    for (int target_pa = 0; target_pa <= 1; ++target_pa) {
      sprintf(dir2, "%s.p%u/subgames.%s.%s.%s.%s.p%u.p%u", dir, asym_p,
	      subgame_card_abstraction.CardAbstractionName().c_str(),
	      subgame_betting_abstraction.BettingAbstractionName().c_str(),
	      subgame_cfr_config.CFRConfigName().c_str(),
	      ResolvingMethodName(method), asym_p, target_pa);
      if (FileExists(dir2)) {
	fprintf(stderr, "Recursively deleting %s\n", dir2);
	RecursivelyDeleteDirectory(dir2);
      }
    }
  } else {
    for (int target_pa = 0; target_pa <= 1; ++target_pa) {
      sprintf(dir2, "%s/subgames.%s.%s.%s.%s.p%u", dir,
	      subgame_card_abstraction.CardAbstractionName().c_str(),
	      subgame_betting_abstraction.BettingAbstractionName().c_str(),
	      subgame_cfr_config.CFRConfigName().c_str(),
	      ResolvingMethodName(method), target_pa);
      if (FileExists(dir2)) {
	fprintf(stderr, "Recursively deleting %s\n", dir2);
	RecursivelyDeleteDirectory(dir2);
      }
    }
  }
}

void FloorCVs(Node *subtree_root, double *opp_reach_probs, const CanonicalCards *hands,
	      double *cvs) {
  int st = subtree_root->Street();
  int num_hole_card_pairs = Game::NumHoleCardPairs(st);
  int maxcard1 = Game::MaxCard() + 1;
  for (int i = 0; i < num_hole_card_pairs; ++i) {
    const Card *our_cards = hands->Cards(i);
    Card our_hi = our_cards[0];
    Card our_lo = our_cards[1];
    double sum_opp_reach_probs = 0;
    for (int j = 0; j < num_hole_card_pairs; ++j) {
      const Card *opp_cards = hands->Cards(j);
      Card opp_hi = opp_cards[0];
      Card opp_lo = opp_cards[1];
      if (opp_hi == our_hi || opp_hi == our_lo || opp_lo == our_hi ||
	  opp_lo == our_lo) {
	continue;
      }
      int opp_enc = opp_hi * maxcard1 + opp_lo;
      sum_opp_reach_probs += opp_reach_probs[opp_enc];
    }
    double our_norm_cv = cvs[i] / sum_opp_reach_probs;
    if (our_norm_cv < -(double)subtree_root->LastBetTo()) {
      cvs[i] = (-(double)subtree_root->LastBetTo()) * sum_opp_reach_probs;
    }
  }
  
}

static void CalculateMeanCVs(double *p0_cvs, double *p1_cvs, int num_hole_card_pairs,
			     shared_ptr<double []> *reach_probs, const CanonicalCards *hands,
			     double *p0_mean_cv, double *p1_mean_cv) {
  int maxcard1 = Game::MaxCard() + 1;
  double sum_p0_cvs = 0, sum_p1_cvs = 0, sum_joint_probs = 0;
  for (int i = 0; i < num_hole_card_pairs; ++i) {
    const Card *our_cards = hands->Cards(i);
    Card our_hi = our_cards[0];
    Card our_lo = our_cards[1];
    int our_enc = our_hi * maxcard1 + our_lo;
    double sum_p0_opp_probs = 0;
    for (int j = 0; j < num_hole_card_pairs; ++j) {
      const Card *opp_cards = hands->Cards(j);
      Card opp_hi = opp_cards[0];
      Card opp_lo = opp_cards[1];
      if (opp_hi == our_hi || opp_hi == our_lo || opp_lo == our_hi ||
	  opp_lo == our_lo) {
	continue;
      }
      int opp_enc = opp_hi * maxcard1 + opp_lo;
      sum_p0_opp_probs += reach_probs[0][opp_enc];
    }
    double p0_prob = reach_probs[0][our_enc];
    double p1_prob = reach_probs[1][our_enc];
    sum_p0_cvs += p0_cvs[i] * p0_prob;
    sum_p1_cvs += p1_cvs[i] * p1_prob;
    sum_joint_probs += p1_prob * sum_p0_opp_probs;
  }
  *p0_mean_cv = sum_p0_cvs / sum_joint_probs;
  *p1_mean_cv = sum_p1_cvs / sum_joint_probs;
}

void ZeroSumCVs(double *p0_cvs, double *p1_cvs, int num_hole_card_pairs,
		shared_ptr<double []> *reach_probs, const CanonicalCards *hands) {
  double p0_mean_cv, p1_mean_cv;
  CalculateMeanCVs(p0_cvs, p1_cvs, num_hole_card_pairs, reach_probs, hands, &p0_mean_cv,
		   &p1_mean_cv);
  // fprintf(stderr, "Mean CVs: %f, %f\n", p0_mean_cv, p1_mean_cv);

  double avg = (p0_mean_cv + p1_mean_cv) / 2.0;
  double adj = -avg;
  int maxcard1 = Game::MaxCard() + 1;
  for (int i = 0; i < num_hole_card_pairs; ++i) {
    const Card *our_cards = hands->Cards(i);
    Card our_hi = our_cards[0];
    Card our_lo = our_cards[1];
    double sum_p0_opp_probs = 0, sum_p1_opp_probs = 0;    
    for (int j = 0; j < num_hole_card_pairs; ++j) {
      const Card *opp_cards = hands->Cards(j);
      Card opp_hi = opp_cards[0];
      Card opp_lo = opp_cards[1];
      if (opp_hi == our_hi || opp_hi == our_lo || opp_lo == our_hi ||
	  opp_lo == our_lo) {
	continue;
      }
      int opp_enc = opp_hi * maxcard1 + opp_lo;
      sum_p0_opp_probs += reach_probs[0][opp_enc];
      sum_p1_opp_probs += reach_probs[1][opp_enc];
    }
    p0_cvs[i] += adj * sum_p1_opp_probs;
    p1_cvs[i] += adj * sum_p0_opp_probs;
  }

  // I can take this out
  double adj_p0_mean_cv, adj_p1_mean_cv;
  CalculateMeanCVs(p0_cvs, p1_cvs, num_hole_card_pairs, reach_probs, hands, &adj_p0_mean_cv,
		   &adj_p1_mean_cv);
  // fprintf(stderr, "Adj mean CVs: P0 %f, P1 %f\n", adj_p0_mean_cv, adj_p1_mean_cv);
  
}
