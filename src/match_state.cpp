#include <stdio.h>
#include <stdlib.h>

#include <string>

#include "match_state.h"

using std::string;

// Should I advance the street after the call, or wait for the "/"?
// Let's try waiting for the "/".
// If the players get all-in on a street prior to the river, we may see streets with no action.
// For example: cr9900r19997c/cr20000c//
void MatchState::ProcessAction(void) {
  int len = action_.size();
  terminal_ = false;
  int st = 0;
  int pa = 1;
  bool call_ends_street = false;
  int last_bet_to = big_blind_;
  int i = 0;
  while (i < len) {
    char c = action_[i];
    if (c == 'c') {
      ++i;
      if (call_ends_street) {
	pa = 0;
	call_ends_street = false;
	if (st == 3) {
	  terminal_ = true;
	  break;
	}
	if (last_bet_to == stack_size_) {
	  // If we call an all-in bet, also set terminal_ to true
	  terminal_ = true;
	  // Don't break; there may be trailing slashes to consume
	}
      } else {
	call_ends_street = true;
	pa = pa^1;
      }
    } else if (c == 'f') {
      ++i;
      terminal_ = true;
      break;
    } else if (c == 'r') {
      ++i;
      int j = i;
      while (i < len && action_[i] >= '0' && action_[i] <= '9') ++i;
      string str(action_, j, i-j);
      if (sscanf(str.c_str(), "%i", &last_bet_to) != 1) {
	fprintf(stderr, "Couldn't parse: %s\n", action_.c_str());
	exit(-1);
      }
      call_ends_street = true;
      pa = pa^1;
    } else if (c == '/') {
      ++st;
      ++i;
    } else {
      fprintf(stderr, "Couldn't parse: %s\n", action_.c_str());
      exit(-1);
    }      
  }
  if (i != len) {
    fprintf(stderr, "i %i len %i action %s\n", i, len, action_.c_str());
    exit(-1);
  }
  street_ = st;
}

MatchState::MatchState(bool p1, int hand_no, const string &action, int our_hi, int our_lo,
		       int opp_hi, int opp_lo, int *board, int big_blind, int stack_size) {
  p1_ = p1;
  hand_no_ = hand_no;
  action_ = action;
  our_hi_ = our_hi;
  our_lo_ = our_lo;
  opp_hi_ = opp_hi;
  opp_lo_ = opp_lo;
  for (int i = 0; i < 5; ++i) board_[i] = board[i];
  big_blind_ = big_blind;
  stack_size_ = stack_size;
  ProcessAction();
}
