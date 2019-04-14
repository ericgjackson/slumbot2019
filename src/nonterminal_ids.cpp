#include <stdio.h>
#include <stdlib.h>

#include "betting_tree.h"
#include "card_abstraction.h"
#include "game.h"
#include "nonterminal_ids.h"

// This doesn't seem like it handles reentrancy.  But it easily could, no?
static void AssignNonterminalIDs2(Node *node, int *num_nonterminals) {
  if (node->Terminal()) return;
  int st = node->Street();
  int p = node->PlayerActing();
  int index = p * (Game::MaxStreet() + 1) + st;
  node->SetNonterminalID(num_nonterminals[index]);
  num_nonterminals[index] += 1;
  int num_succs = node->NumSuccs();
  for (int s = 0; s < num_succs; ++s) {
    AssignNonterminalIDs2(node->IthSucc(s), num_nonterminals);
  }
}

void AssignNonterminalIDs(Node *root, int *num_nonterminals) {
  int num_players = Game::NumPlayers();
  int max_street = Game::MaxStreet();
  int num = num_players * (max_street + 1);
  for (int i = 0; i < num; ++i) num_nonterminals[i] = 0;
  AssignNonterminalIDs2(root, num_nonterminals);
}

// Can pass in NULL for ret_num_nonterminals if you don't want them
void AssignNonterminalIDs(BettingTree *betting_tree, int *num_nonterminals) {
  AssignNonterminalIDs(betting_tree->Root(), num_nonterminals);
}

// Handles reentrancy.  Assumes nonterminal IDs have been assigned densely, and that they account
// for reentrancy.
static void CountNumNonterminals2(Node *node, int *num_nonterminals) {
  if (node->Terminal()) return;
  int pa = node->PlayerActing();
  int st = node->Street();
  int nt_id = node->NonterminalID();
  int index = pa * (Game::MaxStreet() + 1) + st;
  if (nt_id >= num_nonterminals[index]) {
    num_nonterminals[index] = nt_id + 1;
  }
  int num_succs = node->NumSuccs();
  for (int s = 0; s < num_succs; ++s) {
    CountNumNonterminals2(node->IthSucc(s), num_nonterminals);
  }
}

void CountNumNonterminals(Node *root, int *num_nonterminals) {
  int max_street = Game::MaxStreet();
  int num_players = Game::NumPlayers();
  int num = num_players * (max_street + 1);
  for (int i = 0; i < num; ++i) num_nonterminals[i] = 0;
  CountNumNonterminals2(root, num_nonterminals);
}

void CountNumNonterminals(BettingTree *betting_tree, int *num_nonterminals) {
  CountNumNonterminals(betting_tree->Root(), num_nonterminals);
}

