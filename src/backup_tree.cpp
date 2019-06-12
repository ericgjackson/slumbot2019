// Imagine we are on the turn or river facing an opponent not constrained to our abstraction,
// and we want to use resolving.  For best results, we want to "back up" and solve a subgame
// rooted at a turn street-initial node, even if the actual hand has advanced further.  To make
// our solution match the actual hand we have to use the actual bet sizes observed.  These could
// be bets either by us or by the opponent.  We also allow one additional bet per street beyond
// what was observed.

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
using std::unique_ptr;
using std::vector;

BackupBuilder::BackupBuilder(int stack_size) : stack_size_(stack_size) {
}

shared_ptr<Node> BackupBuilder::Build(const vector< vector<double> > &bet_fracs, int *num_bets,
				      int st, int pa, int num_street_bets, int last_bet_size,
				      int last_bet_to, bool street_initial) {
  fprintf(stderr, "bbb1 st %i si %i\n", st, (int)street_initial);
  shared_ptr<Node> call_succ, fold_succ;
  vector< shared_ptr<Node> > bet_succs;
  if (street_initial) {
    call_succ = Build(bet_fracs, num_bets, st, pa^1, num_street_bets, 0, last_bet_to, false);
  } else {
    if (st == Game::MaxStreet()) {
      fprintf(stderr, "sss1\n");
      // Showdown
      call_succ.reset(new Node(terminal_id_++, st, 255, nullptr, nullptr, nullptr, 2, last_bet_to));
      fprintf(stderr, "sss2\n");
    } else {
      // Advance street
      call_succ = Build(bet_fracs, num_bets, st+1, 0, 0, 0, last_bet_to, true);
    }
  }
  if (last_bet_size > 0) {
    int contribution = last_bet_to - last_bet_size;
    // The player remaining is the opponent of the player acting (i.e., folding)
    fold_succ.reset(new Node(terminal_id_++, st, pa^1, nullptr, nullptr, nullptr, 1, contribution));
  }
  fprintf(stderr, "bbb2\n");
  bool bet_allowed = num_street_bets <= num_bets[st] && last_bet_to < stack_size_;
  fprintf(stderr, "bbb3\n");
  if (bet_allowed) {
    fprintf(stderr, "bbb4\n");
    int last_pot_size = 2 * last_bet_to;
    double bet_frac;
    if (num_street_bets < num_bets[st]) {
      bet_frac = bet_fracs[st][num_street_bets];
    } else {
      bet_frac = 1.0;
    }
    int new_bet_size = bet_frac * last_pot_size + 0.5;
    if (last_bet_to + new_bet_size > stack_size_) {
      new_bet_size = stack_size_ - last_bet_to;
    }
    shared_ptr<Node> bet_succ = Build(bet_fracs, num_bets, st, pa^1, num_street_bets + 1,
				      new_bet_size, last_bet_to + new_bet_size, false);
    bet_succs.push_back(bet_succ);
  }
  fprintf(stderr, "bbb5\n");
  shared_ptr<Node> node = std::make_shared<Node>(-1, st, pa, call_succ, fold_succ, &bet_succs, 2,
						 last_bet_to);
  fprintf(stderr, "bbb6\n");
  return node;
}

shared_ptr<Node> BackupBuilder::Build(const vector< vector<double> > &bet_fracs, int st,
				      int last_bet_to) {
  int max_street = Game::MaxStreet();
  unique_ptr<int []> num_bets(new int[max_street + 1]);
  for (int st1 = st; st1 <= max_street; ++st1) {
    num_bets[st1] = bet_fracs[st1].size();
  }
  terminal_id_ = 0;
  return Build(bet_fracs, num_bets.get(), st, 0, 0, 0, last_bet_to, true);
}

