// Create a betting tree that mimics a path from an input tree, but follows an mb1b1 abstraction
// off of that path.

#include <stdio.h>
#include <stdlib.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "backup_tree.h"
#include "betting_tree.h"
#include "game.h"

using std::shared_ptr;
using std::string;
using std::vector;

BackupBuilder::BackupBuilder(int stack_size) : stack_size_(stack_size) {
}

shared_ptr<Node> BackupBuilder::OffPathSubtree(int st, int player_acting, bool street_initial,
					       int last_bet_size, int last_bet_to,
					       int num_street_bets) {
  fprintf(stderr, "OffPathSubtree nsb %i lbs %i lbt %i\n", num_street_bets, last_bet_size,
	  last_bet_to);
  vector< shared_ptr<Node> > bet_succs;
  if (num_street_bets == 0) {
    // Only allow one bet per street
    // This is the pot size prior to the bet I am not considering making.
    int last_pot_size = 2 * last_bet_to;
    int new_bet_to = last_bet_to + last_pot_size;
    if (new_bet_to > 0.75 * stack_size_) {
      new_bet_to = stack_size_;
    }
    int new_bet_size = new_bet_to - last_bet_to;
    shared_ptr<Node> bet_succ(OffPathSubtree(st, player_acting^1, false, new_bet_size, new_bet_to,
					     num_street_bets + 1));
    bet_succs.push_back(bet_succ);
  }
  shared_ptr<Node> call_succ;
  if (street_initial) {
    call_succ = OffPathSubtree(st, player_acting^1, false, 0, last_bet_to, num_street_bets);
  } else {
    if (st == Game::MaxStreet()) {
      // This is a call on the final street
      call_succ.reset(new Node(terminal_id_++, st, 255, nullptr, nullptr, nullptr, 2, last_bet_to));
    } else {
      // Call completes action on current street.
      call_succ = OffPathSubtree(st + 1, 0, true, 0, last_bet_to, 0);
    }
  }
  shared_ptr<Node> fold_succ;
  if (last_bet_size > 0) {
    // The player remaining is the opponent of the player acting (i.e., folding)
    // The last argument is the number of chips that the folding player has put into the pot
    fold_succ.reset(new Node(terminal_id_++, st, player_acting^1, nullptr, nullptr, nullptr, 1,
			     last_bet_to - last_bet_size));
  }
  // Assign nonterminal ID of -1 for now.
  shared_ptr<Node> node;
  node.reset(new Node(-1, st, player_acting, call_succ, fold_succ, &bet_succs, 2, last_bet_to));
  return node;
}

shared_ptr<Node> BackupBuilder::Build(const vector<Node *> &path, int index, int num_street_bets,
				      int last_bet_size) {
  int sz = path.size();
  Node *node = path[index];
  int st = node->Street();
  int pa = node->PlayerActing();
  int last_bet_to = node->LastBetTo();
  if (index == sz - 1) {
    if (node->Terminal()) {
      fprintf(stderr, "Shouldn't find terminal node on path\n");
      exit(-1);
    }
    fprintf(stderr, "Calling OffPathSubtree\n");
    return OffPathSubtree(st, pa, node->StreetInitial(), last_bet_size, last_bet_to,
			  num_street_bets);
  }
  Node *next_node = path[index+1];
  int num_succs = node->NumSuccs();
  int s;
  for (s = 0; s < num_succs; ++s) {
    if (node->IthSucc(s) == next_node) break;
  }
  if (s == num_succs) {
    fprintf(stderr, "Couldn't connect node to next node\n");
    exit(-1);
  }
  int fsi = node->FoldSuccIndex();
  if (s == fsi) {
    fprintf(stderr, "Shouldn't have fold on source path\n");
    exit(-1);
  }
  shared_ptr<Node> call_succ, fold_succ;
  vector< shared_ptr<Node> > bet_succs;
  bool call = (s == node->CallSuccIndex());  
  if (call) {
    call_succ = Build(path, index + 1, num_street_bets, 0);
    if (num_street_bets == 0) {
      int last_pot_size = 2 * last_bet_to;
      int new_bet_to = last_bet_to + last_pot_size;
      if (new_bet_to > 0.75 * stack_size_) {
	new_bet_to = stack_size_;
      }
      int new_bet_size = new_bet_to - last_bet_to;
      shared_ptr<Node> bet_succ(OffPathSubtree(st, pa^1, false, new_bet_size, new_bet_to,
					       num_street_bets + 1));
      bet_succs.push_back(bet_succ);
    }
  } else {
    int new_bet_to = next_node->LastBetTo();
    int new_bet_size = new_bet_to - last_bet_to;
    shared_ptr<Node> bet_succ = Build(path, index + 1, num_street_bets + 1, new_bet_size);
    bet_succs.push_back(bet_succ);
    if (node->StreetInitial()) {
      call_succ = OffPathSubtree(st, pa^1, false, 0, last_bet_to, num_street_bets);
    } else {
      // Could this be a terminal call?
      if (st == Game::MaxStreet()) {
	// Showdown
	call_succ.reset(new Node(terminal_id_++, st, 255, nullptr, nullptr, nullptr, 2,
				 last_bet_to));
      } else {
	call_succ = OffPathSubtree(st+1, pa^1, true, 0, last_bet_to, num_street_bets);
      }
    }
  }
  if (last_bet_size > 0) {
    Node *old_fold = node->IthSucc(fsi);
    // The player remaining is the opponent of the player acting (i.e., folding)
    fold_succ.reset(new Node(terminal_id_++, st, pa^1, nullptr, nullptr, nullptr, 1,
			     old_fold->LastBetTo()));
  }
  shared_ptr<Node> new_node(new Node(-1, st, pa, call_succ, fold_succ, &bet_succs, 2, last_bet_to));
  return new_node;
}

shared_ptr<Node> BackupBuilder::Build(const vector<Node *> &path, int st) {
  int i = 0;
  int sz = path.size();
  fprintf(stderr, "path sz %i\n", sz);
  while (i < sz) {
    if (path[i]->Street() == st) break;
  }
  if (i == sz) {
    fprintf(stderr, "Path never reaches target street?!?\n");
    exit(-1);
  }
  terminal_id_ = 0;
  return Build(path, i, 0, 0);
}
