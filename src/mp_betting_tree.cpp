#include <stdio.h>
#include <stdlib.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "betting_abstraction.h"
#include "betting_tree_builder.h"
#include "betting_tree.h"
#include "fast_hash.h"
#include "game.h"

using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::unordered_map;
using std::vector;

static void AddStringToKey(const string &s, string *key) {
  *key += s;
}

static void AddIntToKey(int i, string *key) {
  char buf[10];
  sprintf(buf, "%i", i);
  *key += buf;
}

static unsigned long long int HashKey(const string &key) {
  return fasthash64((void *)key.c_str(), key.size(), 0);
}

bool BettingTreeBuilder::FindReentrantNode(const string &key, shared_ptr<Node> *node) {
  unordered_map< unsigned long long int, shared_ptr<Node> >::iterator it;
  unsigned long long int h = HashKey(key);
  it = node_map_->find(h);
  if (it != node_map_->end()) {
    *node = it->second;
    return true;
  } else {
    return false;
  }
}

void BettingTreeBuilder::AddReentrantNode(const string &key, shared_ptr<Node> node) {
  unsigned long long int h = HashKey(key);
  (*node_map_)[h] = node;
}

// Determine the next player to act, taking into account who has folded.
// Pass in first candidate for next player to act.
static int NextPlayerToAct(int p, bool *folded) {
  int num_players = Game::NumPlayers();
  while (folded[p]) {
    p = (p + 1) % num_players;
  }
  return p;
}

shared_ptr<Node>
BettingTreeBuilder::CreateMPFoldSucc(int street, int last_bet_size, int bet_to, int num_street_bets,
				     int num_bets, int player_acting, int num_players_to_act,
				     bool *folded, int target_player, string *key,
				     int *terminal_id) {
  shared_ptr<Node> fold_succ;
  int max_street = Game::MaxStreet();
  int num_players = Game::NumPlayers();
  int num_players_remaining = 0;
  for (int p = 0; p < num_players; ++p) {
    if (! folded[p]) ++num_players_remaining;
  }
  if (num_players_remaining <= 1) {
    fprintf(stderr, "CreateMPFoldSucc npr %u?!?\n", num_players_remaining);
    exit(-1);
  }
  string new_key = *key;
  if (betting_abstraction_.BettingKey(street)) {
    AddStringToKey("f", &new_key);
  }
  if (num_players_remaining == 2) {
    // This fold completes the hand
    int p;
    for (p = 0; p < num_players; ++p) {
      if (! folded[p] && p != player_acting) break;
    }
    if (p == num_players) {
      fprintf(stderr, "Everyone folded?!?\n");
      fprintf(stderr, "street %u\n", street);
      fprintf(stderr, "num_players_to_act %u\n", num_players_to_act);
      exit(-1);
    }
    // Subtract last_bet_size from bet_to so that we get the last amount
    // of chips that the last opponent put in.  Not sure this is useful
    // though, for multiplayer.
    fold_succ.reset(new Node((*terminal_id)++, street, p, nullptr, nullptr, nullptr, 1,
			     bet_to - last_bet_size));
  } else if (num_players_to_act == 1 && street == max_street) {
    // Showdown
    fold_succ.reset(new Node((*terminal_id)++, street, 255, nullptr, nullptr, nullptr,
			     num_players_remaining - 1, bet_to));
  } else {
    // Hand is not over
    unique_ptr<bool []> new_folded(new bool[num_players]);
    for (int p = 0; p < num_players; ++p) {
      new_folded[p] = folded[p];
    }
    new_folded[player_acting] = true;
    if (num_players_to_act == 1) {
      // This fold completes the street
      fold_succ = CreateMPStreet(street + 1, bet_to, num_bets, new_folded.get(), target_player,
				 &new_key, terminal_id);
    } else {
      // This is a fold that does not end the street
      int next_player_to_act =
	NextPlayerToAct((player_acting + 1) % num_players, new_folded.get());
      fold_succ = CreateMPSubtree(street, last_bet_size, bet_to, num_street_bets, num_bets,
				  next_player_to_act, num_players_to_act - 1, new_folded.get(),
				  target_player, &new_key, terminal_id);
    }
  }
  return fold_succ;
}

// num_players_to_act is (re)initialized when a player bets.  It gets
// decremented every time a player calls or folds.  When it reaches zero
// the action on the current street is complete.
shared_ptr<Node> BettingTreeBuilder::CreateMPCallSucc(int street, int last_bet_size, int bet_to,
						      int num_street_bets, int num_bets,
						      int player_acting, int num_players_to_act,
						      bool *folded, int target_player,
						      string *key, int *terminal_id) {
  bool advance_street = (num_players_to_act == 1);
  shared_ptr<Node> call_succ;
  int max_street = Game::MaxStreet();
  int num_players = Game::NumPlayers();
  int num_players_remaining = 0;
  for (int p = 0; p < num_players; ++p) {
    if (! folded[p]) ++num_players_remaining;
  }
  if (folded[player_acting]) {
    fprintf(stderr, "CreateMPCallSucc: Player already folded\n");
    exit(-1);
  }
  if (num_players_to_act == 0) exit(-1);
  if (num_players_to_act > 1000000) exit(-1);
  if (num_players_to_act > num_players_remaining) exit(-1);
  string new_key = *key;
  if (betting_abstraction_.BettingKey(street)) {
    AddStringToKey("c", &new_key);
  }
  if (street < max_street && advance_street) {
    // Call completes action on current street.
    call_succ = CreateMPStreet(street + 1, bet_to, num_bets, folded,
			       target_player, &new_key, terminal_id);
  } else if (! advance_street) {
    // This is a check or call that does not advance the street
    int next_player_to_act =
      NextPlayerToAct((player_acting + 1) % num_players, folded);
    // Shouldn't happen
    if (next_player_to_act == player_acting) exit(-1);
    call_succ = CreateMPSubtree(street, 0, bet_to, num_street_bets, num_bets, next_player_to_act,
				num_players_to_act - 1,	folded, target_player, &new_key,
				terminal_id);
  } else {
    // This is a call on the final street
    call_succ.reset(new Node((*terminal_id)++, street, 255, nullptr, nullptr, nullptr,
			     num_players_remaining, bet_to));

  }
  return call_succ;
}

// We are contemplating adding a bet.  We might or might not be facing a previous bet.
void BettingTreeBuilder::MPHandleBet(int street, int last_bet_size, int last_bet_to, int new_bet_to,
				     int num_street_bets, int num_bets, int player_acting,
				     int num_players_to_act, bool *folded, int target_player,
				     string *key, int *terminal_id,
				     vector< shared_ptr<Node> > *bet_succs) {
  // New bet must be of size greater than zero
  if (new_bet_to <= last_bet_to) return;

  int new_bet_size = new_bet_to - last_bet_to;

  int stack_size = betting_abstraction_.StackSize();
  bool all_in_bet = (new_bet_to == stack_size);

  // Cannot make a bet that is smaller than the min bet (usually the big blind)
  if (new_bet_size < betting_abstraction_.MinBet() && ! all_in_bet) {
    return;
  }

  // In general, cannot make a bet that is smaller than the previous bet size.  Exception is that
  // you can always raise all-in
  if (new_bet_size < last_bet_size && ! all_in_bet) {
    return;
  }

  // If CloseToAllInFrac is set, eliminate some bet sizes that are too close to an all-in bet.
  int new_pot_size = new_bet_to * 2;
  if (! all_in_bet && betting_abstraction_.CloseToAllInFrac() > 0 &&
      new_pot_size >= 2 * stack_size * betting_abstraction_.CloseToAllInFrac()) {
    return;
  }
  
  string new_key = *key;
  if (betting_abstraction_.BettingKey(street)) {
    AddStringToKey("b", &new_key);
    AddIntToKey(new_bet_size, &new_key);
  }
  
  int num_players = Game::NumPlayers();
  int num_players_remaining = 0;
  for (int p = 0; p < num_players; ++p) {
    if (! folded[p]) ++num_players_remaining;
  }
  int next_player_to_act =
    NextPlayerToAct((player_acting + 1) % num_players, folded);

  // Shouldn't happen
  if (num_players_remaining == 1) {
    fprintf(stderr, "Only one player remaining after bet?!?\n");
    exit(-1);
  }
  // For bets we pass in the pot size without the pending bet included
  shared_ptr<Node> bet =
    CreateMPSubtree(street, new_bet_size, new_bet_to, num_street_bets + 1, num_bets + 1,
		    next_player_to_act, num_players_remaining - 1, folded, target_player,
		    &new_key, terminal_id);
  bet_succs->push_back(bet);
}

// last_bet_size is the *size* of the last bet.  Needed to ensure raises
// are legal.  bet_to is what the last bet was *to*.
void BettingTreeBuilder::CreateMPSuccs(int street, int last_bet_size, int bet_to,
				       int num_street_bets, int num_bets, int player_acting,
				       int num_players_to_act, bool *folded, int target_player,
				       string *key, int *terminal_id, shared_ptr<Node> *call_succ,
				       shared_ptr<Node> *fold_succ,
				       vector< shared_ptr<Node> > *bet_succs) {
  if (folded[player_acting]) {
    fprintf(stderr, "CreateMPSuccs: Player already folded\n");
    exit(-1);
  }
  bet_succs->clear();
  *call_succ = CreateMPCallSucc(street, last_bet_size, bet_to, num_street_bets, num_bets,
				player_acting, num_players_to_act, folded, target_player, key,
				terminal_id);
  // Allow fold if num_street_bets > 0 OR this is the very first action of
  // the hand (i.e., the small blind can open fold).
  // Preflop you can fold when num_street_bets is zero if
  bool can_fold = (num_street_bets > 0);
  if (! can_fold && street == 0) {
    // Special case for the preflop.  When num_street_bets is zero, everyone
    // except the big blind can still fold.  The big blind is the player
    // prior to the player who is first to act.
    int fta = Game::FirstToAct(0);
    int bb;
    if (fta == 0) bb = Game::NumPlayers() - 1;
    else          bb = fta - 1;
    can_fold = (player_acting != bb);
  }
  if (can_fold) {
    *fold_succ = CreateMPFoldSucc(street, last_bet_size, bet_to, num_street_bets, num_bets,
				  player_acting, num_players_to_act, folded, target_player, key,
				  terminal_id);
  }

  bool our_bet = (target_player == player_acting);
  int all_in_bet_to = betting_abstraction_.StackSize();
  unique_ptr<bool []> bet_to_seen(new bool[all_in_bet_to + 1]);
  for (int bt = 0; bt <= all_in_bet_to; ++bt) bet_to_seen[bt] = false;
  
  if (num_street_bets < betting_abstraction_.MaxBets(street, our_bet)) {
    if ((! betting_abstraction_.Asymmetric() && betting_abstraction_.AlwaysAllIn()) ||
	(betting_abstraction_.Asymmetric() && our_bet && betting_abstraction_.OurAlwaysAllIn()) ||
	(betting_abstraction_.Asymmetric() && ! our_bet && betting_abstraction_.OppAlwaysAllIn())) {
      // Allow an all-in bet
      bet_to_seen[all_in_bet_to] = true;
    }
  }
  
  if (num_street_bets < betting_abstraction_.MaxBets(street, our_bet)) {
    if ((! betting_abstraction_.Asymmetric() &&
	 betting_abstraction_.AlwaysMinBet(street, num_street_bets)) ||
	(betting_abstraction_.Asymmetric() && our_bet &&
	 betting_abstraction_.OurAlwaysMinBet(street, num_street_bets)) ||
	(betting_abstraction_.Asymmetric() && ! our_bet &&
	 betting_abstraction_.OppAlwaysMinBet(street, num_street_bets))) {
      // Allow a min bet
      int min_bet;
      if (num_street_bets == 0) {
	min_bet = betting_abstraction_.MinBet();
      } else {
	min_bet = last_bet_size;
      }
      int new_bet_to = bet_to + min_bet;
      if (new_bet_to > all_in_bet_to) {
	bet_to_seen[all_in_bet_to] = true;
      } else {
	if (betting_abstraction_.AllowableBetTo(new_bet_to)) {
	  bet_to_seen[new_bet_to] = true;
	} else {
	  int old_pot_size = 2 * bet_to;
	  int nearest_allowable_bet_to =
	    NearestAllowableBetTo(old_pot_size, new_bet_to, last_bet_size);
	  bet_to_seen[nearest_allowable_bet_to] = true;
#if 0
	  if (nearest_allowable_bet_to != new_bet_to) {
	    fprintf(stderr, "Changed %u to %u\n", new_bet_to - bet_to,
		    nearest_allowable_bet_to - bet_to);
	  }
#endif
	}
      }
    }
  }
  
  if (num_street_bets < betting_abstraction_.MaxBets(street, our_bet)) {
    const vector<double> &pot_fracs =
      betting_abstraction_.BetSizes(street, num_street_bets, our_bet, player_acting);
    GetNewBetTos(bet_to, last_bet_size, pot_fracs, player_acting, target_player, bet_to_seen.get());
  }

  for (int new_bet_to = 0; new_bet_to <= all_in_bet_to; ++new_bet_to) {
    if (bet_to_seen[new_bet_to]) {
      MPHandleBet(street, last_bet_size, bet_to, new_bet_to, num_street_bets, num_bets,
		  player_acting, num_players_to_act, folded, target_player, key, terminal_id,
		  bet_succs);
    }
  }
}

shared_ptr<Node>
BettingTreeBuilder::CreateMPSubtree(int st, int last_bet_size, int bet_to, int num_street_bets,
				    int num_bets, int player_acting, int num_players_to_act,
				    bool *folded, int target_player, string *key,
				    int *terminal_id) {
  if (folded[player_acting]) {
    fprintf(stderr, "CreateMPSubtree: Player already folded\n");
    exit(-1);
  }
  string final_key;
  bool merge = false;
  // As it stands, we don't encode which players have folded.  But we do
  // encode num_players_to_act.
  int num_players = Game::NumPlayers();
  int num_rem = 0;
  for (int p = 0; p < num_players; ++p) {
    if (! folded[p]) ++num_rem;
  }
  if (betting_abstraction_.ReentrantStreet(st) &&
      2 * bet_to >= betting_abstraction_.MinReentrantPot() &&
      num_bets >= betting_abstraction_.MinReentrantBets(st, num_rem)) {
    merge = true;
    final_key = *key;
    AddStringToKey(":", &final_key);
    AddIntToKey(st, &final_key);
    AddStringToKey(":", &final_key);
    AddIntToKey(player_acting, &final_key);
    AddStringToKey(":", &final_key);
    AddIntToKey(num_street_bets, &final_key);
    AddStringToKey(":", &final_key);
    AddIntToKey(bet_to, &final_key);
    AddStringToKey(":", &final_key);
    AddIntToKey(last_bet_size, &final_key);
    AddStringToKey(":", &final_key);
    AddIntToKey(num_rem, &final_key);
    AddStringToKey(":", &final_key);
    AddIntToKey(num_players_to_act, &final_key);
    // fprintf(stderr, "Key: %s\n", final_key.c_str());
    shared_ptr<Node> node;
    if (FindReentrantNode(final_key, &node)) {
      return node;
    }
  }
  shared_ptr<Node> call_succ;
  shared_ptr<Node> fold_succ;
  vector< shared_ptr<Node> > bet_succs;
  CreateMPSuccs(st, last_bet_size, bet_to, num_street_bets, num_bets, player_acting,
		num_players_to_act, folded, target_player, key, terminal_id, &call_succ, &fold_succ,
		&bet_succs);
  if (call_succ == nullptr && fold_succ == nullptr && bet_succs.size() == 0) {
    fprintf(stderr, "Creating nonterminal with zero succs\n");
    fprintf(stderr, "This will cause problems\n");
    exit(-1);
  }
  // Assign nonterminal ID of -1 for now.
  shared_ptr<Node> node;
  node.reset(new Node(-1, st, player_acting, call_succ, fold_succ, &bet_succs, num_rem, bet_to));
  if (merge) {
    AddReentrantNode(final_key, node);
  }
  return node;
}

shared_ptr<Node>
BettingTreeBuilder::CreateMPStreet(int street, int bet_to, int num_bets, bool *folded,
				   int target_player, string *key, int *terminal_id) {
  int num_players = Game::NumPlayers();
  int num_players_remaining = 0;
  for (int p = 0; p < num_players; ++p) {
    if (! folded[p]) ++num_players_remaining;
  }
  int next_player_to_act =
    NextPlayerToAct(Game::FirstToAct(street + 1), folded);
  shared_ptr<Node> node =
    CreateMPSubtree(street, 0, bet_to, 0, num_bets, next_player_to_act, num_players_remaining,
		    folded, target_player, key, terminal_id);
  return node;
}


shared_ptr<Node>
BettingTreeBuilder::CreateMPTree(int target_player, int *terminal_id) {
  int initial_street = betting_abstraction_.InitialStreet();
  int player_acting = Game::FirstToAct(initial_street_);
  int initial_bet_to = Game::BigBlind();
  int last_bet_size = Game::BigBlind() - Game::SmallBlind();
  int num_players = Game::NumPlayers();
  unique_ptr<bool []> folded(new bool[num_players]);
  for (int p = 0; p < num_players; ++p) {
    folded[p] = false;
  }
  string key;
  return CreateMPSubtree(initial_street, last_bet_size, initial_bet_to, 0, 0, player_acting,
			 Game::NumPlayers(), folded.get(), target_player, &key, terminal_id);
}
