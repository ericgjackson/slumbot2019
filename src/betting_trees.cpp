// The BettingTrees class allows us to handle symmetric and asymmetric betting abstractions
// uniformly.  The BettingTrees class holds one or more betting trees; typically, one for a
// symmetric betting abstraction and N (one per player) for an asymmetric betting abstraction.

#include <stdio.h>
#include <stdlib.h>

#include <memory>

#include "betting_abstraction.h"
#include "betting_tree.h"
#include "betting_trees.h"
#include "game.h"

using std::shared_ptr;
using std::unique_ptr;

BettingTrees::BettingTrees(const BettingAbstraction &ba) {
  asymmetric_ = ba.Asymmetric();
  target_player_ = -1;
  int num_players = Game::NumPlayers();
  betting_trees_.reset(new shared_ptr<BettingTree>[num_players]);
  if (asymmetric_) {
    for (int p = 0; p < num_players; ++p) {
      betting_trees_[p].reset(new BettingTree(ba, p));
    }
  } else {
    betting_trees_[0].reset(new BettingTree(ba));
    for (int p = 1; p < num_players; ++p) {
      betting_trees_[p] = betting_trees_[0];
    }
  }
}

// Only construct the target player's betting tree
BettingTrees::BettingTrees(const BettingAbstraction &ba, int target_player) {
  if (! ba.Asymmetric()) {
    fprintf(stderr, "Can only call this constructor for asymmetric betting abstraction\n");
    exit(-1);
  }
  asymmetric_ = true;
  target_player_ = target_player;
  int num_players = Game::NumPlayers();
  betting_trees_.reset(new shared_ptr<BettingTree>[num_players]);
  betting_trees_[target_player].reset(new BettingTree(ba, target_player));
}

// For cloning a (sub)tree; symmetric only?
BettingTrees::BettingTrees(Node *subtree_root) {
  asymmetric_ = false;
  target_player_ = -1;
  int num_players = Game::NumPlayers();
  betting_trees_.reset(new shared_ptr<BettingTree>[num_players]);
  betting_trees_[0].reset(new BettingTree(subtree_root));
  for (int p = 1; p < num_players; ++p) {
    betting_trees_[p] = betting_trees_[0];
  }
}

const BettingTree *BettingTrees::GetBettingTree(void) const {
  if (asymmetric_) {
    if (target_player_ == -1) {
      fprintf(stderr, "Cannot call GetBettingTree() without player arg on asymmetric tree\n");
      exit(-1);
    }
    return betting_trees_[target_player_].get();
  }
  return betting_trees_[0].get();
}

Node *BettingTrees::Root(void) const {
  if (asymmetric_) {
    if (target_player_ == -1) {
      fprintf(stderr, "Cannot call Root() without player arg on asymmetric tree\n");
      exit(-1);
    }
    return betting_trees_[target_player_]->Root();
  }
  return betting_trees_[0]->Root();
}

int BettingTrees::NumNonterminals(int p, int st) const {
  if (asymmetric_) {
    if (target_player_ == -1) {
      fprintf(stderr, "Cannot call NumNonterminals() without asym_p arg on asymmetric tree\n");
      exit(-1);
    }
    return betting_trees_[target_player_]->NumNonterminals(p, st);
  }
  return betting_trees_[0]->NumNonterminals(p, st);
}
