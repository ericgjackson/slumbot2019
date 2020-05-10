// Imagine we are on the turn or river facing an opponent not constrained to our abstraction,
// and we want to use resolving.  For best results, we want to "back up" and solve a subgame
// rooted at a turn street-initial node, even if the actual hand has advanced further.  To make
// our solution match the actual hand we have to use the actual bet sizes observed.
//
// We take a list of bets we have observed in the hand so far.  These might be bets by us or by
// the opponent.  An observed bet is something like the first bet by P1 on the turn was 18 chips.
// We always allow the observed bet sizes.  In addition, we allow a pot-size bet if no bet
// was observed in a given game state.  In addition, we allow a pot-size raise after the last
// observed bet.

#include <stdio.h>
#include <stdlib.h>

#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "backup_tree.h"
#include "betting_tree.h"
#include "betting_trees.h"
#include "game.h"

using std::set;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;

ObservedBets::ObservedBets(const ObservedBets &src) {
  int num = src.observed_bets_.size();
  observed_bets_.resize(num);
  for (int i = 0; i < num; ++i) {
    observed_bets_[i] = src.observed_bets_[i];
  }
}

// No checking for dups
void ObservedBets::AddObservedBet(int st, int p, int npbs, int npb, int sz) {
  ObservedBet ob;
  ob.st = st;
  ob.p = p;
  ob.npbs = npbs;
  ob.npb = npb;
  ob.sz = sz;
  observed_bets_.push_back(ob);
}

// Exclude calls (size zero bets)
void ObservedBets::GetObservedBetSizes(int st, int p, int npbs, int npb, vector<int> *sizes) const {
  sizes->clear();
  int num = observed_bets_.size();
  for (int i = 0; i < num; ++i) {
    ObservedBet ob = observed_bets_[i];
    if (ob.st == st && ob.p == p && ob.npbs == npbs && ob.npb == npb && ob.sz > 0) {
      sizes->push_back(ob.sz);
    }
  }
}

bool ObservedBets::ObservedACall(int st, int p, int npbs, int npb) const {
  int num = observed_bets_.size();
  for (int i = 0; i < num; ++i) {
    ObservedBet ob = observed_bets_[i];
    if (ob.st == st && ob.p == p && ob.npbs == npbs && ob.npb == npb && ob.sz == 0) {
      return true;
    }
  }
  return false;
}

void ObservedBets::Remove(int st, int p, int npbs, int npb) {
  int num = observed_bets_.size();
  int i = 0;
  while (i < num) {
    ObservedBet ob = observed_bets_[i];
    if (ob.st == st && ob.p == p && ob.npbs == npbs && ob.npb == npb) {
      observed_bets_.erase(observed_bets_.begin() + i);
      --num;
    } else {
      ++i;
    }
  }
}

BackupBuilder::BackupBuilder(int stack_size) : stack_size_(stack_size) {
}

shared_ptr<Node> BackupBuilder::Build(const ObservedBets &observed_bets, const int *min_bets,
				      const int *max_bets, int st, int pa, int npbs, int npb,
				      int last_bet_size, int last_bet_to, bool street_initial,
				      bool on_path) {
  shared_ptr<Node> call_succ, fold_succ;
  vector< shared_ptr<Node> > bet_succs;
  if (street_initial) {
    bool new_on_path = on_path && observed_bets.ObservedACall(st, pa, npbs, npb);
    call_succ = Build(observed_bets, min_bets, max_bets, st, pa^1, npbs, npb, 0, last_bet_to, false,
		      new_on_path);
  } else {
    if (st == Game::MaxStreet()) {
      // Showdown
      call_succ.reset(new Node(terminal_id_++, st, 255, nullptr, nullptr, nullptr, 2, last_bet_to));
    } else {
      // Advance street
      bool new_on_path = on_path && observed_bets.ObservedACall(st, pa, npbs, npb);
      call_succ = Build(observed_bets, min_bets, max_bets, st+1, 0, 0, npb, 0, last_bet_to, true,
			new_on_path);
    }
  }
  if (last_bet_size > 0) {
    int contribution = last_bet_to - last_bet_size;
    // The player remaining is the opponent of the player acting (i.e., folding)
    fold_succ.reset(new Node(terminal_id_++, st, pa^1, nullptr, nullptr, nullptr, 1, contribution));
  }
  if (last_bet_to < stack_size_ && (max_bets == nullptr || npbs < max_bets[st])) {
    set<int> new_bet_sizes;
    vector<int> observed_sizes;
    if (on_path) observed_bets.GetObservedBetSizes(st, pa, npbs, npb, &observed_sizes);
    bool new_on_path = on_path && observed_sizes.size() > 0;
    for (int i = 0; i < (int)observed_sizes.size(); ++i) {
      int observed_sz = observed_sizes[i];
      if (last_bet_to + observed_sz > stack_size_) {
	fprintf(stderr, "last_bet_to + observed_sz > stack_size_?!?\n");
	exit(-1);
      }
      new_bet_sizes.insert(observed_sz);
    }
    if (observed_sizes.size() == 0) {
      // If there are no observed sizes, then we allow a single bet size (a pot size bet) under
      // two conditions:
      // 1) First bet on street
      // 2) Raise of observed bet
      bool allow_pot_size_bet = false;
      if (npbs == 0) {
	allow_pot_size_bet = true;
      } else if (on_path) {
	allow_pot_size_bet = true;
      } else if (min_bets && npbs < min_bets[st]) {
	allow_pot_size_bet = true;
      }
      if (allow_pot_size_bet) {
	int new_bet_size = 2 * last_bet_to;
	if (last_bet_to + new_bet_size > stack_size_) {
	  new_bet_size = stack_size_ - last_bet_to;
	}
	new_bet_sizes.insert(new_bet_size);
      }
    }
    // Should confirm STL set orders ints from low to high
    for (auto it = new_bet_sizes.begin(); it != new_bet_sizes.end(); ++it) {
      int new_bet_size = *it;
      shared_ptr<Node> bet_succ = Build(observed_bets, min_bets, max_bets, st, pa^1, npbs + 1,
					npb + 1, new_bet_size, last_bet_to + new_bet_size, false,
					new_on_path);
      bet_succs.push_back(bet_succ);
    }
  }
  shared_ptr<Node> node = std::make_shared<Node>(-1, st, pa, call_succ, fold_succ, &bet_succs, 2,
						 last_bet_to);
  return node;
}

shared_ptr<Node> BackupBuilder::Build(const ObservedBets &observed_bets, const int *min_bets,
				      const int *max_bets, int st, int last_bet_to) {
  terminal_id_ = 0;
  return Build(observed_bets, min_bets, max_bets, st, 0, 0, 0, 0, last_bet_to, true, true);
}

BettingTrees *BackupBuilder::BuildTrees(const ObservedBets &observed_bets, const int *min_bets,
					const int *max_bets, int st, int last_bet_to) {
  return new BettingTrees(Build(observed_bets, min_bets, max_bets, st, last_bet_to).get());
}
