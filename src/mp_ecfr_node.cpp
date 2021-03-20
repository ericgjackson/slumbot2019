// Players can be inactive because they are all-in (i.e., not able to bet) but still be entitled
// to part of the pot.

#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <memory>
#include <set>
#include <vector>

#include "betting_tree.h"
#include "buckets.h"
#include "game.h"
#include "mp_ecfr_node.h"
#include "sorting.h"

using std::pair;
using std::set;
using std::unique_ptr;
using std::vector;

class Pot {
public:
  Pot(void);
  Pot(int num_players, int amount);
  ~Pot(void) {}
  void AddPlayer(int p) {
    players_[num_limited_++] = p;
  }
private:
  // The players specified here are those who are *limited* to this pot.
  int num_limited_;
  // Hard limit of 10 on number of players in a game
  int players_[10];
  int amount_;
};

Pot::Pot(void) {
  num_limited_ = 0;
  amount_ = 0;
}

Pot::Pot(int num_players, int amount) {
  num_limited_ = 0;
  amount_ = amount;
}

class State {
public:
  State(int num_players);
private:
  int num_pots_;
  unique_ptr<Pot []> pots_;
};

State::State(int num_players) {
  num_pots_ = 0;
  pots_.reset(new Pot[num_players]);
}

// Probably need to honor some chopping rules that dictate who gets the extra chip.
void MPECFRNode::SetShowdownPots(bool *folded, bool *active, int *contributions,
				 int *stack_sizes) {
  int num_players = Game::NumPlayers();
  // These are contributions of players who did not fold
  set<int> unique_contributions;
  for (int p = 0; p < num_players; ++p) {
    if (! folded[p]) {
      unique_contributions.insert(contributions[p]);
    }
  }
  vector<int> sorted_unique_contributions;
  for (auto it = unique_contributions.begin(); it != unique_contributions.end(); ++it) {
    sorted_unique_contributions.push_back(*it);
  }
  sort(sorted_unique_contributions.begin(), sorted_unique_contributions.end());
  int num_unique_contributions = sorted_unique_contributions.size();
  for (int i = 0; i < num_unique_contributions; ++i) {
    int max_contribution = sorted_unique_contributions[i];
    int pot = 0;
    for (int p = 0; p < num_players; ++p) {
      pot += std::min(max_contribution, contributions[p]);
    }
  }
}

// folded: which players have folded
// active: which players are active (not folded and not all-in)
// contributions: how many chips each player has contributed
// stack_sizes: starting stack sizes of each player
MPECFRNode::MPECFRNode(Node *node, const Buckets &buckets, bool *folded, bool *active,
		       int *contributions, int *stack_sizes) {
  terminal_ = node->Terminal();
  if (terminal_) {
    showdown_ = node->Showdown();
    last_bet_to_ = node->LastBetTo();
    // No longer just one player remaining
    player_remaining_ = node->PlayerActing();
    if (showdown_) {
      SetShowdownPots(folded, active, contributions, stack_sizes);
    }
    return;
  }
  st_ = node->Street();
  player_acting_ = node->PlayerActing();
  if (folded[player_acting_]) {
    fprintf(stderr, "Folded player acting?!?\n");
    exit(-1);
  }
  if (! active[player_acting_]) {
    fprintf(stderr, "Inactive player acting?!?\n");
    exit(-1);
  }
  int num_buckets = buckets.NumBuckets(st_);
  num_succs_ = node->NumSuccs();
  if (num_succs_ > 1) {
    // Only store regrets, etc. if there is more than one succ
    int num_values = num_buckets * num_succs_;
    regrets_.reset(new double[num_values]);
    sumprobs_.reset(new int[num_values]);
    for (int i = 0; i < num_values; ++i) {
      regrets_[i] = 0;
      sumprobs_[i] = 0;
    }
  }
  int csi = node->CallSuccIndex();
  int fsi = node->FoldSuccIndex();
  succs_.reset(new unique_ptr<MPECFRNode>[num_succs_]);
  for (int s = 0; s < num_succs_; ++s) {
    int old_contribution = contributions[player_acting_];
    int new_contribution = old_contribution;
    bool new_folded = false;
    bool new_active = true;
    Node *succ = node->IthSucc(s);
    if (s == csi) {
      int stack_size = stack_sizes[player_acting_];
      int last_bet_to = node->LastBetTo();
      if (last_bet_to >= stack_size) {
	new_contribution = stack_size;
	new_active = false;
      } else {
	new_contribution = last_bet_to;
      }
    } else if (s == fsi) {
      new_folded = true;
      new_active = false;
    } else {
      new_contribution = succ->LastBetTo();
      if (new_contribution > stack_sizes[player_acting_]) {
	fprintf(stderr, "Bet more than stack?!?\n");
	exit(-1);
      }
    }
    folded[player_acting_] = new_folded;
    active[player_acting_] = new_active;
    contributions[player_acting_] = new_contribution;
    succs_[s].reset(new MPECFRNode(succ, buckets, folded, active, contributions, stack_sizes));
    // Restore old values
    folded[player_acting_] = false;
    active[player_acting_] = true;
    contributions[player_acting_] = old_contribution;
  }
}
