#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "betting_abstraction.h"
#include "betting_tree.h"
#include "betting_tree_builder.h"
#include "constants.h"
#include "game.h"
// #include "pool.h"
#include "split.h"

#if 0
// Produces the new possible pot sizes.  Assumes any size bet is allowed by
// the abstraction (well, only multiples of the small blind).
// This function does not rule out certain illegal bet sizes; we do not
// check that the bet size is at least one big blind and we do not check
// that a raise is at least as big as the previous bet.  These restrictions
// are enforced in HandleBet().
// If there is a pending bet, old_pot_size reflects the size of the pot
// including that bet *and* a call of that bet.
void BettingTreeBuilder::GetAllNewPotSizes(int old_pot_size,
					   vector<int> *new_pot_sizes) {
  int all_in_pot_size = 2 * betting_abstraction_.StackSize();
  // Already all-in; no bets possible
  if (old_pot_size == all_in_pot_size) return;
  int one_player_committed = old_pot_size / 2;
  int remaining = betting_abstraction_.StackSize() - one_player_committed;
  for (int bet_size = 1; bet_size <= remaining; ++bet_size) {
    new_pot_sizes->push_back(old_pot_size + 2 * bet_size);
  }
}

// Produces the new possible pot sizes.  Assumes any even size bet is allowed
// by the abstraction (i.e., even multiples of the small blind).
// This function does not rule out certain illegal bet sizes; we do not check
// that a raise is at least as big as the previous bet.  These restrictions
// are enforced in HandleBet().
// If there is a pending bet, old_pot_size reflects the size of the pot
// including that bet *and* a call of that bet.
void BettingTreeBuilder::GetAllNewEvenPotSizes(int old_pot_size,
					       vector<int> *new_pot_sizes) {
  int all_in_pot_size = 2 * betting_abstraction_.StackSize();
  // Already all-in; no bets possible
  if (old_pot_size == all_in_pot_size) return;
  int one_player_committed = old_pot_size / 2;
  int remaining = betting_abstraction_.StackSize() - one_player_committed;
  for (int bet_size = 2; bet_size <= remaining; bet_size += 2) {
    new_pot_sizes->push_back(old_pot_size + 2 * bet_size);
  }
}

// Add one bet size if pot size is >= 1/9 of all-in
bool BettingTreeBuilder::AddGeometric1Bet(int old_pot_size,
					  bool *pot_size_seen) {
  int all_in_pot_size = 2 * betting_abstraction_.StackSize();
  if (old_pot_size > 0.33333 * all_in_pot_size) {
    // One bet left - all in
    pot_size_seen[all_in_pot_size] = true;
    return true;
    // } else if (old_pot_size >= 0.11112 * all_in_pot_size) {
  } else if (old_pot_size >= 0.2 * all_in_pot_size) {
    double ratio = all_in_pot_size / old_pot_size;
    double pot_frac = sqrt(ratio);
    double bet_frac = (pot_frac - 1.0) / 2.0;
    double double_bet_size = old_pot_size * bet_frac;
    int bet_size = (int)(double_bet_size + 0.5);
    if (bet_size == 0) return false;
    int new_pot_size = old_pot_size + 2 * bet_size;
    if (new_pot_size > all_in_pot_size) {
      pot_size_seen[all_in_pot_size] = true;
    } else {
      pot_size_seen[new_pot_size] = true;
    }
    return true;
  } else {
    return false;
  }
}

// Should be a superset of geometric-1 bets so that we can do
// asymmetric evaluations.
bool BettingTreeBuilder::AddGeometric2Bet(int old_pot_size,
					  bool *pot_size_seen) {
  
  int all_in_pot_size = 2 * betting_abstraction_.StackSize();
  if (old_pot_size >= 0.33333 * all_in_pot_size) {
    // One bet left - all in
    pot_size_seen[all_in_pot_size] = true;
    return true;
    // } else if (old_pot_size >= 0.11112 * all_in_pot_size) {
  } else if (old_pot_size >= 0.2 * all_in_pot_size) {
    // Allow 1/2 pot, full pot and geometric bet
    for (unsigned int num = 1; num <= 2; ++num) {
      double frac = ((double)num) / 2.0;
      double double_bet_size = old_pot_size * frac;
      int bet_size = (int)(double_bet_size + 0.5);
      if (bet_size > 0) {
	int new_pot_size = old_pot_size + 2 * bet_size;
	if (new_pot_size > all_in_pot_size) {
	  pot_size_seen[all_in_pot_size] = true;
	} else {
	  pot_size_seen[new_pot_size] = true;
	}
      }
    }
    double ratio = all_in_pot_size / old_pot_size;
    double pot_frac = sqrt(ratio);
    double bet_frac = (pot_frac - 1.0) / 2.0;
    double double_bet_size = old_pot_size * bet_frac;
    int bet_size = (int)(double_bet_size + 0.5);
    if (bet_size > 0) {
      int new_pot_size = old_pot_size + 2 * bet_size;
      if (new_pot_size > all_in_pot_size) {
	pot_size_seen[all_in_pot_size] = true;
      } else {
	pot_size_seen[new_pot_size] = true;
      }
    }
    return true;
  } else {
    return false;
  }
}

// Could rule out pot size of 380 when all_in_pot_size is 400.
// There need to be some additional tests performed on the pot sizes.  For
// example, if these new pot sizes correspond to raises we need to check
// that the raise size is valid (at least one big blind, at least as big
// as the previous bet).
void BettingTreeBuilder::GetNewPotSizes(int old_pot_size,
					const vector<double> &pot_fracs,
					unsigned int player_acting,
					unsigned int target_player,
					bool *pot_size_seen) {
  int all_in_pot_size = 2 * betting_abstraction_.StackSize();
  // Already all-in; no bets possible
  if (old_pot_size == all_in_pot_size) return;
  if ((! betting_abstraction_.Asymmetric() &&
	      (unsigned int)old_pot_size >=
	      betting_abstraction_.OnlyPotThreshold()) ||
	     (betting_abstraction_.Asymmetric() &&
	      player_acting == target_player &&
	      (unsigned int)old_pot_size >=
	      betting_abstraction_.OurOnlyPotThreshold()) ||
	     (betting_abstraction_.Asymmetric() &&
	      player_acting != target_player &&
	      (unsigned int)old_pot_size >=
	      betting_abstraction_.OppOnlyPotThreshold())) {
    // Only pot-size bets allowed.  (Well, maybe all-ins also.)  So skip code
    // below that looks at pot_fracs.
    if (old_pot_size > 0) {
      int new_pot_size = 3 * old_pot_size;
      if (new_pot_size > all_in_pot_size) {
	pot_size_seen[all_in_pot_size] = true;
      } else {
	pot_size_seen[new_pot_size] = true;
      }
    }
  } else {
    // Geometric bets are used only as an alternative to the normal
    // method (from pot_fracs) when the pot is big enough.
    bool used_geometric = false;
    if ((! betting_abstraction_.Asymmetric() &&
	 betting_abstraction_.GeometricType() == 1) ||
	(betting_abstraction_.Asymmetric() && player_acting == target_player &&
	 betting_abstraction_.OurGeometricType() == 1) ||
	(betting_abstraction_.Asymmetric() && player_acting != target_player &&
	 betting_abstraction_.OppGeometricType() == 1)) {
      used_geometric = AddGeometric1Bet(old_pot_size, pot_size_seen);
    }
    if ((! betting_abstraction_.Asymmetric() &&
	 betting_abstraction_.GeometricType() == 2) ||
	(betting_abstraction_.Asymmetric() && player_acting == target_player &&
	 betting_abstraction_.OurGeometricType() == 2) ||
	(betting_abstraction_.Asymmetric() && player_acting != target_player &&
	 betting_abstraction_.OppGeometricType() == 2)) {
      used_geometric = AddGeometric2Bet(old_pot_size, pot_size_seen);
    }

    if (! used_geometric) {
      unsigned int num_pot_fracs = pot_fracs.size();
      for (unsigned int i = 0; i < num_pot_fracs; ++i) {
	double frac = pot_fracs[i];
	double double_bet_size = old_pot_size * frac;
	int bet_size = (int)(double_bet_size + 0.5);
	if (bet_size == 0) continue;
	int new_pot_size = old_pot_size + 2 * bet_size;
	if (betting_abstraction_.CloseToAllInFrac() > 0 &&
	    new_pot_size >=
	    all_in_pot_size * betting_abstraction_.CloseToAllInFrac()) {
	  // return (old_pot_size > 0.33333 * all_in_pot_size);
	  // Don't add the bet, but add an all-in bet instead
	  pot_size_seen[all_in_pot_size] = true;
	} else if (new_pot_size > all_in_pot_size) {
	  pot_size_seen[all_in_pot_size] = true;
	} else {
	  pot_size_seen[new_pot_size] = true;
	}
      }
    }
  }
}
#endif

unsigned int
BettingTreeBuilder::NearestAllowableBetTo(unsigned int old_pot_size,
					  unsigned int new_bet_to,
					  unsigned int last_bet_size) {
  unsigned int min_bet = last_bet_size;
  if (min_bet < betting_abstraction_.MinBet()) {
    min_bet = betting_abstraction_.MinBet();
  }
  unsigned int lower_bound = old_pot_size / 2 + min_bet;
  unsigned int below;
  for (below = new_bet_to - 1; below >= lower_bound; --below) {
    if (betting_abstraction_.AllowableBetTo(below)) break;
  }
  unsigned int stack_size = betting_abstraction_.StackSize();
  unsigned int above;
  for (above = new_bet_to + 1; above < stack_size; ++above) {
    if (betting_abstraction_.AllowableBetTo(above)) break;
  }
  if (below >= lower_bound) {
    double below_pot_frac = (below - old_pot_size) / (double)old_pot_size;
    // There should always be a valid above bet because all-in is always
    // allowed so if the original new-bet-to was all-in then we wouldn't get
    // here.
    double above_pot_frac = (above - old_pot_size) / (double)old_pot_size;
    double actual_pot_frac = (new_bet_to - old_pot_size) / (double)old_pot_size;
    // Use Sam's translation method
    double below_prob = ((above_pot_frac - actual_pot_frac) *
			 (1.0 + below_pot_frac)) /
      ((above_pot_frac - below_pot_frac) *
       (1.0 + actual_pot_frac));
    if (below_prob >= 0.5) {
      return below;
    } else {
      return above;
    }
  } else {
    return above;
  }

}

// Some additional tests get performed later.  For example, if a bet is a
// raise, we check that the raise size is valid (at least one big blind, at
// least as big as the previous bet).
void BettingTreeBuilder::GetNewBetTos(unsigned int old_bet_to,
				      unsigned int last_bet_size,
				      const vector<double> &pot_fracs,
				      unsigned int player_acting,
				      unsigned int target_player,
				      bool *bet_to_seen) {
  unsigned int all_in_bet_to = betting_abstraction_.StackSize();
  // Already all-in; no bets possible
  if (old_bet_to == all_in_bet_to) return;
  unsigned int old_pot_size = 2 * old_bet_to;
  if ((! betting_abstraction_.Asymmetric() &&
       old_pot_size >= betting_abstraction_.OnlyPotThreshold()) ||
      (betting_abstraction_.Asymmetric() &&
       player_acting == target_player &&
       old_pot_size >=
       betting_abstraction_.OurOnlyPotThreshold()) ||
      (betting_abstraction_.Asymmetric() &&
       player_acting != target_player &&
       old_pot_size >=
       betting_abstraction_.OppOnlyPotThreshold())) {
    // Only pot-size bets allowed.  (Well, maybe all-ins also.)  So skip code
    // below that looks at pot_fracs.
    if (old_pot_size > 0) {
      unsigned int new_bet_to = 3 * old_pot_size / 2;
      if (new_bet_to > all_in_bet_to) {
	bet_to_seen[all_in_bet_to] = true;
      } else {
	bet_to_seen[new_bet_to] = true;
      }
    }
  } else {
    unsigned int num_pot_fracs = pot_fracs.size();
    for (unsigned int i = 0; i < num_pot_fracs; ++i) {
      double frac = pot_fracs[i];
      double double_bet_size = old_pot_size * frac;
      unsigned int bet_size = (unsigned int)(double_bet_size + 0.5);
      if (bet_size == 0) continue;
      unsigned int new_bet_to = old_bet_to + bet_size;
      unsigned int new_pot_size = 2 * new_bet_to;
      if (betting_abstraction_.CloseToAllInFrac() > 0 &&
	  new_pot_size >=
	  2 * all_in_bet_to * betting_abstraction_.CloseToAllInFrac()) {
	// Don't add the bet, but add an all-in bet instead
	bet_to_seen[all_in_bet_to] = true;
      } else if (new_bet_to > all_in_bet_to) {
	bet_to_seen[all_in_bet_to] = true;
      } else {
	if (betting_abstraction_.AllowableBetTo(new_bet_to)) {
	  bet_to_seen[new_bet_to] = true;
	} else {
	  unsigned int nearest_allowable_bet_to =
	    NearestAllowableBetTo(old_pot_size, new_bet_to, last_bet_size);
	  bet_to_seen[nearest_allowable_bet_to] = true;
	}
      }
    }
  }
}

// We are contemplating adding a bet.  We might or might not be facing a
// previous bet.
void BettingTreeBuilder::HandleBet(unsigned int street,
				   unsigned int last_bet_size,
				   unsigned int last_bet_to,
				   unsigned int new_bet_to,
				   unsigned int num_street_bets,
				   unsigned int player_acting,
				   unsigned int target_player,
				   unsigned int *terminal_id,
				   vector< shared_ptr<Node> > *bet_succs) {
  // New bet must be of size greater than zero
  if (new_bet_to <= last_bet_to) return;

  unsigned int new_bet_size = new_bet_to - last_bet_to;

  bool all_in_bet = (new_bet_to == betting_abstraction_.StackSize());

  // Cannot make a bet that is smaller than the min bet (usually the big
  // blind)
  if (new_bet_size < betting_abstraction_.MinBet() && ! all_in_bet) {
    return;
  }

  // In general, cannot make a bet that is smaller than the previous
  // bet size.  Exception is that you can always raise all-in
  if (new_bet_size < last_bet_size && ! all_in_bet) {
    return;
  }

  // For bets we pass in the pot size without the pending bet included
  shared_ptr<Node> bet =
    CreateNoLimitSubtree(street, new_bet_size, new_bet_to, num_street_bets + 1,
			 player_acting^1, target_player, terminal_id);
  bet_succs->push_back(bet);
}

// May return NULL
shared_ptr<Node>
BettingTreeBuilder::CreateCallSucc(unsigned int street,
				   unsigned int last_bet_size,
				   unsigned int bet_to,
				   unsigned int num_street_bets,
				   unsigned int player_acting,
				   unsigned int target_player,
				   unsigned int *terminal_id) {
  // We advance the street if we are calling a bet
  // Note that a call on the final street is considered to advance the street
  bool advance_street = num_street_bets > 0;
  // This assumes heads-up
  advance_street |= (Game::FirstToAct(street) != player_acting);
  shared_ptr<Node> call_succ;
  unsigned int max_street = Game::MaxStreet();
  if (street < max_street && advance_street) {
    call_succ = CreateNoLimitSubtree(street + 1, 0, bet_to, 0,
				     Game::FirstToAct(street + 1),
				     target_player, terminal_id);
  } else if (! advance_street) {
    // This is a check that does not advance the street
    call_succ = CreateNoLimitSubtree(street, 0, bet_to, num_street_bets,
				     player_acting^1, target_player,
				     terminal_id);
  } else {
    // This is a call on the final street
    call_succ.reset(new Node((*terminal_id)++, street, 255, nullptr, nullptr,
			     nullptr, 2, bet_to));

  }
  return call_succ;
}

shared_ptr<Node>
BettingTreeBuilder::CreateFoldSucc(unsigned int street,
				   unsigned int last_bet_size,
				   unsigned int bet_to,
				   unsigned int player_acting,
				   unsigned int *terminal_id) {
  unsigned int player_remaining = player_acting^1;
  shared_ptr<Node> fold_succ;
  // Player acting field encodes player remaining at fold nodes
  // bet_to - last_bet_size is how many chips the opponent put in
  fold_succ.reset(new Node((*terminal_id)++, street, player_remaining, nullptr,
			   nullptr, nullptr, 1, bet_to - last_bet_size));
  return fold_succ;
}

// There are several pot sizes here which is confusing.  When we call
// CreateNoLimitSuccs() it may be as the result of adding a bet action.
// So we have the pot size before that bet, the pot size after that bet,
// and potentially the pot size after a raise of the bet.  last_pot_size is
// the first pot size - i.e., the pot size before the pending bet.
// current_pot_size is the pot size after the pending bet.
void BettingTreeBuilder::CreateNoLimitSuccs(unsigned int street,
					    unsigned int last_bet_size,
					    unsigned int bet_to,
					    unsigned int num_street_bets,
					    unsigned int player_acting,
					    unsigned int target_player,
					    unsigned int *terminal_id,
					    shared_ptr<Node> *call_succ,
					    shared_ptr<Node> *fold_succ,
					    vector< shared_ptr<Node> > *
					    bet_succs) {
  // *fold_succ = NULL;
  // *call_succ = NULL;
  bet_succs->clear();
  bool no_open_limp = 
    ((! betting_abstraction_.Asymmetric() &&
      betting_abstraction_.NoOpenLimp()) ||
     (betting_abstraction_.Asymmetric() && player_acting == target_player &&
      betting_abstraction_.OurNoOpenLimp()) ||
     (betting_abstraction_.Asymmetric() && player_acting != target_player &&
      betting_abstraction_.OppNoOpenLimp()));
  if (! (street == 0 && num_street_bets == 0 &&
	 player_acting == Game::FirstToAct(0) && no_open_limp)) {
    *call_succ = CreateCallSucc(street, last_bet_size, bet_to, num_street_bets,
				player_acting, target_player, terminal_id);
  }
  bool can_fold = (num_street_bets > 0);
  if (! can_fold && street == 0) {
    // Special case for the preflop.  When num_street_bets is zero, the small
    // blind can still fold.
    can_fold = (player_acting == Game::FirstToAct(0));
  }
  if (can_fold) {
    *fold_succ = CreateFoldSucc(street, last_bet_size, bet_to, player_acting,
				terminal_id);
  }

  bool our_bet = (target_player == player_acting);
#if 0
  unsigned int current_pot_size = last_pot_size + 2 * last_bet_size;
  vector<int> new_pot_sizes;
  if (betting_abstraction_.AllBetSizeStreet(street)) {
    if (num_street_bets < betting_abstraction_.MaxBets(street, our_bet)) {
      GetAllNewPotSizes(current_pot_size, &new_pot_sizes);
    }
  } else if (betting_abstraction_.AllEvenBetSizeStreet(street)) {
    if (num_street_bets < betting_abstraction_.MaxBets(street, our_bet)) {
      GetAllNewEvenPotSizes(current_pot_size, &new_pot_sizes);
    }
  }
#endif
  unsigned int all_in_bet_to = betting_abstraction_.StackSize();
  bool *bet_to_seen = new bool[all_in_bet_to + 1];
  for (unsigned int bt = 0; bt <= all_in_bet_to; ++bt) {
    bet_to_seen[bt] = false;
  }
    
  if (num_street_bets < betting_abstraction_.MaxBets(street, our_bet)) {
    if ((! betting_abstraction_.Asymmetric() &&
	 betting_abstraction_.AlwaysAllIn()) ||
	(betting_abstraction_.Asymmetric() && our_bet &&
	 betting_abstraction_.OurAlwaysAllIn()) ||
	(betting_abstraction_.Asymmetric() && ! our_bet &&
	 betting_abstraction_.OppAlwaysAllIn())) {
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
      unsigned int min_bet;
      if (num_street_bets == 0) {
	min_bet = betting_abstraction_.MinBet();
      } else {
	min_bet = last_bet_size;
      }
      unsigned int new_bet_to = bet_to + min_bet;
      if (new_bet_to > all_in_bet_to) {
	bet_to_seen[all_in_bet_to] = true;
      } else {
	if (betting_abstraction_.AllowableBetTo(new_bet_to)) {
	  bet_to_seen[new_bet_to] = true;
	} else {
	  unsigned int old_pot_size = 2 * bet_to;
	  unsigned int nearest_allowable_bet_to =
	    NearestAllowableBetTo(old_pot_size, new_bet_to, last_bet_size);
	  bet_to_seen[nearest_allowable_bet_to] = true;
	}
      }
    }
  }

#if 0
  double multiplier = 0;
  if (num_street_bets < betting_abstraction_.MaxBets(street, our_bet)) {
    multiplier = betting_abstraction_.BetSizeMultiplier(street,
							num_street_bets,
							our_bet);
  }
  if (multiplier > 0) {
    int min_bet;
    if (num_street_bets == 0) {
      min_bet = betting_abstraction_.MinBet();
    } else {
      min_bet = last_bet_size;
    }
    int current_bet_to = current_pot_size / 2;
    int bet = min_bet;
    // We can add illegally small raise sizes here, but they get filtered
    // out in HandleBet().
    while (true) {
      int new_bet_to = current_bet_to + bet;
      int new_pot_size = 2 * new_bet_to;
      if (new_pot_size > all_in_pot_size) {
	break;
      }
      if (betting_abstraction_.CloseToAllInFrac() > 0 &&
	  new_pot_size >=
	  all_in_pot_size * betting_abstraction_.CloseToAllInFrac()) {
	break;
      }
      pot_size_seen[new_pot_size] = true;
      double d_bet = bet * multiplier;
      bet = (int)(d_bet + 0.5);
    }
    // Add all-in
    pot_size_seen[all_in_pot_size] = true;
  }
  
  // Not using multipliers
  bool no_regular_bets = 
    ((! betting_abstraction_.Asymmetric() &&
      current_pot_size >= betting_abstraction_.NoRegularBetThreshold()) ||
     (betting_abstraction_.Asymmetric() &&
      player_acting == target_player &&
      current_pot_size >=
      betting_abstraction_.OurNoRegularBetThreshold()) ||
     (betting_abstraction_.Asymmetric() &&
      player_acting != target_player &&
      current_pot_size >=
      betting_abstraction_.OppNoRegularBetThreshold()));
#endif

  if (num_street_bets < betting_abstraction_.MaxBets(street, our_bet)) {
    const vector<double> *pot_fracs =
      betting_abstraction_.BetSizes(street, num_street_bets, our_bet);
    GetNewBetTos(bet_to, last_bet_size, *pot_fracs, player_acting,
		 target_player, bet_to_seen);
  }
  vector<unsigned int> new_bet_tos;
  for (unsigned int bt = 0; bt <= all_in_bet_to; ++bt) {
    if (bet_to_seen[bt]) {
      new_bet_tos.push_back(bt);
    }
  }  
  delete [] bet_to_seen;

  unsigned int num_bet_tos = new_bet_tos.size();
  for (unsigned int i = 0; i < num_bet_tos; ++i) {
    HandleBet(street, last_bet_size, bet_to, new_bet_tos[i], num_street_bets,
	      player_acting, target_player, terminal_id, bet_succs);
  }
}

shared_ptr<Node>
BettingTreeBuilder::CreateNoLimitSubtree(unsigned int st,
					 unsigned int last_bet_size,
					 unsigned int bet_to,
					 unsigned int num_street_bets,
					 unsigned int player_acting,
					 unsigned int target_player,
					 unsigned int *terminal_id) {
  shared_ptr<Node> call_succ(nullptr);
  shared_ptr<Node> fold_succ(nullptr);
  vector< shared_ptr<Node> > bet_succs;
  CreateNoLimitSuccs(st, last_bet_size, bet_to, num_street_bets,
		     player_acting, target_player, terminal_id, &call_succ,
		     &fold_succ, &bet_succs);
  if (call_succ == NULL && fold_succ == NULL && bet_succs.size() == 0) {
    fprintf(stderr, "Creating nonterminal with zero succs\n");
    fprintf(stderr, "This will cause problems\n");
    exit(-1);
  }
  // Assign nonterminal ID of kMaxUInt for now.
  shared_ptr<Node> node;
  node.reset(new Node(kMaxUInt, st, player_acting, call_succ, fold_succ,
		      &bet_succs, 2, bet_to));
  return node;
}

shared_ptr<Node>
BettingTreeBuilder::CreateNoLimitTree1(unsigned int target_player,
				       unsigned int *terminal_id) {
  unsigned int initial_street = betting_abstraction_.InitialStreet();
  unsigned int player_acting = Game::FirstToAct(initial_street_);
  unsigned int initial_bet_to = Game::BigBlind();
  unsigned int last_bet_size = Game::BigBlind() - Game::SmallBlind();
  
  return CreateNoLimitSubtree(initial_street, last_bet_size, initial_bet_to,
			      0, player_acting, target_player,
			      terminal_id);
}

