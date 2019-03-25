#include <stdio.h>
#include <stdlib.h>

#include "betting_tree.h"
#include "card_abstraction.h"
#include "game.h"
#include "nonterminal_ids.h"

static void AssignNonterminalIDs(Node *node, int **num_nonterminals) {
  if (node->Terminal()) return;
  int st = node->Street();
  int p = node->PlayerActing();
  node->SetNonterminalID(num_nonterminals[p][st]);
  num_nonterminals[p][st] += 1;
  int num_succs = node->NumSuccs();
  for (int s = 0; s < num_succs; ++s) {
    AssignNonterminalIDs(node->IthSucc(s), num_nonterminals);
  }
}

// Can pass in NULL for ret_num_nonterminals if you don't want them
void AssignNonterminalIDs(Node *root, int ***ret_num_nonterminals) {
  int **num_nonterminals = new int *[2];
  int max_street = Game::MaxStreet();
  for (int p = 0; p <= 1; ++p) {
    num_nonterminals[p] = new int[max_street + 1];
    for (int st = 0; st <= max_street; ++st) {
      num_nonterminals[p][st] = 0;
    }
  }
  AssignNonterminalIDs(root, num_nonterminals);
  if (ret_num_nonterminals) {
    *ret_num_nonterminals = num_nonterminals;
  } else {
    for (int p = 0; p <= 1; ++p) {
      delete [] num_nonterminals[p];
    }
    delete [] num_nonterminals;
  }
}

// Can pass in NULL for ret_num_nonterminals if you don't want them
void AssignNonterminalIDs(BettingTree *betting_tree,
			  int ***ret_num_nonterminals) {
  AssignNonterminalIDs(betting_tree->Root(), ret_num_nonterminals);
}

// Handles reentrancy.  Assumes nonterminal IDs have been assigned densely,
// and that they account for reentrancy.
static void CountNumNonterminals(Node *node, int **num_nonterminals) {
  if (node->Terminal()) return;
  int pa = node->PlayerActing();
  int st = node->Street();
  int nt_id = node->NonterminalID();
  if (nt_id >= num_nonterminals[pa][st]) {
    num_nonterminals[pa][st] = nt_id + 1;
  }
  int num_succs = node->NumSuccs();
  for (int s = 0; s < num_succs; ++s) {
    CountNumNonterminals(node->IthSucc(s), num_nonterminals);
  }
}

int **CountNumNonterminals(Node *root) {
  int max_street = Game::MaxStreet();
  int num_players = Game::NumPlayers();
  int **num_nonterminals = new int *[num_players];
  for (int pa = 0; pa < num_players; ++pa) {
    num_nonterminals[pa] = new int[max_street + 1];
    for (int st = 0; st <= max_street; ++st) {
      num_nonterminals[pa][st] = 0;
    }
  }
  CountNumNonterminals(root, num_nonterminals);
  return num_nonterminals;
}

int **CountNumNonterminals(BettingTree *betting_tree) {
  return CountNumNonterminals(betting_tree->Root());
}

