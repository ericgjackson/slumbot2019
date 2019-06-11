// This is a work in progress.
//
// Do I want to do compression?  Maybe add it later?
//
// Should I create these every time they are needed?  That might be
// easiest.  What about the hand_tree_?  Expensive to create it on every
// iteration.  But it requires memory.  Maybe best to create it once, in which
// case we should only create these subgame objects once.
//
// Actually, as things stand now, we will have lots of copies of the same
// hand tree.  Maybe better to recreate subgame objects on each iteration.
// This will simplify some other things as well.
//
// Should I save subgame files from checkpoint iterations?  I used to.
//
// Can I have a subgame for every *turn* board, not every *flop* board?  I
// don't see why not.

#include <math.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <memory>
#include <string>

#include "betting_abstraction.h"
#include "betting_tree.h"
#include "betting_trees.h"
#include "board_tree.h"
#include "buckets.h"
#include "canonical_cards.h"
#include "card_abstraction.h"
#include "cards.h"
#include "cfr_config.h"
#include "cfr_utils.h"
#include "cfr_value_type.h"
#include "cfr_values.h"
#include "cfrp.h"
#include "cfrp_subgame.h"
#include "files.h"
#include "game.h"
#include "hand_tree.h"
#include "io.h"
#include "vcfr_state.h"
#include "vcfr.h"

using std::shared_ptr;
using std::string;
using std::unique_ptr;

CFRPSubgame::CFRPSubgame(const CardAbstraction &ca, const BettingAbstraction &ba,
			 const CFRConfig &cc, const Buckets &buckets, Node *root, int root_bd,
			 const string &action_sequence, int p, CFRP *cfr) :
  VCFR(ca, ba, cc, buckets, 1), action_sequence_(action_sequence), p_(p) {
  // subgame_ = true;
  root_ = root;
  root_bd_ = root_bd;
  root_bd_st_ = root->Street() - 1;
  cfr_ = cfr;
  
  int max_street = Game::MaxStreet();

  subtrees_.reset(new BettingTrees(root));
  
  int subtree_st = root->Street();
  subtree_streets_.reset(new bool[max_street + 1]);
  for (int st = 0; st <= max_street; ++st) {
    subtree_streets_[st] = st >= subtree_st;
  }

  hand_tree_.reset(new HandTree(root_bd_st_, root_bd_, max_street));
}

void CFRPSubgame::SetOppProbs(const shared_ptr<double []> &opp_probs) {
  int max_card1 = Game::MaxCard() + 1;
  int num_enc = max_card1 * max_card1;
  opp_probs_.reset(new double[num_enc]);
  for (int i = 0; i < num_enc; ++i) {
    opp_probs_[i] = opp_probs[i];
  }
}

void CFRPSubgame::SetBestResponseStreets(const bool *sts) {
  int max_street = Game::MaxStreet();
  for (int st = 0; st <= max_street; ++st) {
    best_response_streets_[st] = sts[st];
  }
}

// Don't delete files from last_checkpoint_it
void CFRPSubgame::DeleteOldFiles(int it) {
  if (it_ < 3) {
    // Don't delete anything on first two iterations
    return;
  }
  int delete_it = it_ - 2;
  if (delete_it == last_checkpoint_it_) return;
  
  char dir[500], buf[500];
  sprintf(dir, "%s/%s.%s.%i.%i.%i.%s.%s", Files::OldCFRBase(), Game::GameName().c_str(),
	  card_abstraction_.CardAbstractionName().c_str(), Game::NumRanks(),
	  Game::NumSuits(), Game::MaxStreet(),
	  betting_abstraction_.BettingAbstractionName().c_str(),
	  cfr_config_.CFRConfigName().c_str());
  if (betting_abstraction_.Asymmetric()) {
    sprintf(buf, ".p%u", target_p_);
    strcat(dir, buf);
  }
  int max_street = Game::MaxStreet();
  for (int st = 0; st <= max_street; ++st) {
    if (! subtree_streets_[st]) continue;
    // Remove the regret file created for the current player two iterations ago
    sprintf(buf, "%s/regrets.%s.%i.%i.%i.%i.p%u.i", dir, action_sequence_.c_str(), root_bd_st_,
	    root_bd_, st, delete_it, p_);
#if 0
    if (! FileExists(buf)) {
      // It should exist.  Test for debugging purposes.
      fprintf(stderr, "DeleteOldFiles: %s does not exist\n", buf);
      exit(-1);
    }
#endif
    RemoveFile(buf);

#if 0
    // I don't think I need this any more.
    
    // In the P1 phase of iteration 3, we want to remove P2 sumprobs from
    // iteration 1.
    // In the P2 phase of iteration 2, we want to remove P1 sumprobs from
    // iteration 1.
    // The P1 phase precedes the P2 phase in running CFR.
    int last_it = 0;
    if (p_ && it >= 3) {
      last_it = it - 2;
    } else if (! p_ && it >= 2) {
      last_it = it - 1;
    }
#endif
    sprintf(buf, "%s/sumprobs.%s.%i.%i.%i.%i.p%i.i", dir, action_sequence_.c_str(), root_bd_st_,
	    root_bd_, st, delete_it, p_^1);
#if 0
    if (! FileExists(buf)) {
      // It should exist.  Test for debugging purposes.
      fprintf(stderr, "DeleteOldFiles: %s does not exist\n", buf);
      exit(-1);
    }
#endif
    RemoveFile(buf);
  }
}

void CFRPSubgame::Go(void) {
  if (! value_calculation_) {
    DeleteOldFiles(it_);
  }
  int subtree_st = subtrees_->Root()->Street();

  char dir[500];
  sprintf(dir, "%s/%s.%s.%u.%u.%u.%s.%s", Files::OldCFRBase(),
	  Game::GameName().c_str(),
	  card_abstraction_.CardAbstractionName().c_str(), Game::NumRanks(),
	  Game::NumSuits(), Game::MaxStreet(),
	  betting_abstraction_.BettingAbstractionName().c_str(),
	  cfr_config_.CFRConfigName().c_str());
  if (betting_abstraction_.Asymmetric()) {
    char buf[100];
    sprintf(buf, ".p%u", target_p_);
    strcat(dir, buf);
  }

  int num_players = Game::NumPlayers();
  unique_ptr<bool []> sp_players(new bool[num_players]);
  for (int p = 0; p < num_players; ++p) {
    sp_players[p] = p != p_;
  }
  if (value_calculation_) {
    // Only need the opponent's sumprobs
    sumprobs_.reset(new CFRValues(sp_players.get(), subtree_streets_.get(), root_bd_, root_bd_st_,
				  buckets_, subtrees_->GetBettingTree()));
    sumprobs_->Read(dir, it_, subtrees_->GetBettingTree(), action_sequence_, -1, true, false);
  } else {
    // Need both players regrets
    regrets_.reset(new CFRValues(nullptr, subtree_streets_.get(), root_bd_, root_bd_st_, buckets_,
				 subtrees_->GetBettingTree()));
    // Only need the opponent's sumprobs
    sumprobs_.reset(new CFRValues(sp_players.get(), subtree_streets_.get(), root_bd_, root_bd_st_,
				  buckets_, subtrees_->GetBettingTree()));

    if (it_ == 1) {
      if (p_ == 1) {
	// It 1 P1 phase: initialize P0 and P1 regrets
	// Shouldn't I support doubles as well as ints based on setting inside CFRConfig?
	regrets_->AllocateAndClear(subtrees_->GetBettingTree(), CFRValueType::CFR_INT, false, -1);
      } else {
	// It 1 P0 phase: read P1 regrets from disk; initialize P0 regrets
	regrets_->Read(dir, it_, subtrees_->GetBettingTree(), action_sequence_, 1, false, false);
	regrets_->AllocateAndClear(subtrees_->GetBettingTree(), CFRValueType::CFR_INT, false, 0);
      }
    } else {
      if (p_ == 1) {
	// Read regrets for both players from previous iteration
	regrets_->Read(dir, it_ - 1, subtrees_->GetBettingTree(), action_sequence_, -1, false,
		       false);
      } else {
	// Read P1 regrets from current iteration
	// Read P0 regrets from previous iteration
	regrets_->Read(dir, it_, subtrees_->GetBettingTree(), action_sequence_, 1, false, false);
	regrets_->Read(dir, it_ - 1, subtrees_->GetBettingTree(), action_sequence_, 0, false,
		       false);
      }
    }
    if (it_ == 1) {
      sumprobs_->AllocateAndClear(subtrees_->GetBettingTree(), CFRValueType::CFR_INT, false, -1);
    } else {
      sumprobs_->Read(dir, it_ - 1, subtrees_->GetBettingTree(), action_sequence_, -1, true, false);
    }
  }

  // Should set buckets for initial street of subgame
  if (! buckets_.None(subtree_st)) {
    fprintf(stderr, "Need to set buckets for initial street of subgame\n");
    exit(-1);
  }
  fprintf(stderr, "See comment in cfrp_subgame.cpp\n");
  exit(-1);
  // Old code called Process() passing in subtree_st - 1.  This would ensure that StreetInitial()
  // would get called.  ProcessSubgame() doesn't have that ability currently.  Could add a
  // last_st parameter but doesn't that get ugly?
  // final_vals_ = Process(subtrees_->Root(), subtrees_->Root(), 0, state, subtree_st - 1);
  final_vals_ = ProcessSubgame(subtrees_->Root(), subtrees_->Root(), root_bd_, p_, opp_probs_,
			       hand_tree_.get(), action_sequence_);

  if (! value_calculation_) {
    Mkdir(dir);
    regrets_->Write(dir, it_, subtrees_->Root(), action_sequence_, p_, false);
    sumprobs_->Write(dir, it_, subtrees_->Root(), action_sequence_, -1, true);
  }

  // This should delete the regrets and sumprobs, no?
  regrets_.reset();
  sumprobs_.reset();

  cfr_->Post(thread_index_);
}
