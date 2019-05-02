#ifndef _BETTING_TREES_H_
#define _BETTING_TREES_H_

#include <memory>

#include "betting_tree.h"

class BettingAbstraction;

class BettingTrees {
 public:
  // Normal constructor
  BettingTrees(const BettingAbstraction &ba);
  // For creating only a single player's betting tree
  BettingTrees(const BettingAbstraction &ba, int target_player);
  // For cloning a (sub)tree; symmetric only?
  BettingTrees(Node *subtree_root);
  const BettingTree *GetBettingTree(void) const;
  const BettingTree *GetBettingTree(int p) const {return betting_trees_[p].get();}
  Node *Root(void) const;
  Node *Root(int p) const {return betting_trees_[p]->Root();}
  int NumNonterminals(int p, int st) const;
  int NumNonterminals(int asym_p, int p, int st) const {
    return betting_trees_[asym_p]->NumNonterminals(p, st);
  }
  virtual ~BettingTrees(void) {}
 private:
  bool asymmetric_;
  int target_player_;
  std::unique_ptr<std::shared_ptr<BettingTree> []> betting_trees_;
};

#endif
