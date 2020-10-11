#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "acpc_protocol.h"
#include "logging.h"
#include "match_state.h"
#include "split.h"

using std::string;
using std::vector;

static int MakeCard(int rank, int suit) {
  return rank * 4 + suit;
}

// Card indices range from 0 to 51
// Returns -1 in case of error
static int ParseCard(const char *str) {
  char c = str[0];
  int rank;
  if (c >= '0' && c <= '9') {
    rank = (c - '0') - 2;
  } else if (c == 'A') {
    rank = 12;
  } else if (c == 'K') {
    rank = 11;
  } else if (c == 'Q') {
    rank = 10;
  } else if (c == 'J') {
    rank = 9;
  } else if (c == 'T') {
    rank = 8;
  } else {
    Warning("Couldn't parse card rank\n");
    Warning("Str %s\n", str);
    return -1;
  }
  c = str[1];
  if (c == 'c') {
    return MakeCard(rank, 0);
  } else if (c == 'd') {
    return MakeCard(rank, 1);
  } else if (c == 'h') {
    return MakeCard(rank, 2);
  } else if (c == 's') {
    return MakeCard(rank, 3);
  } else {
    Warning("Couldn't parse card suit\n");
    Warning("Str %s\n", str);
    return -1;
  }
}

// The card string contains one or two hole card pairs and also between zero
// and five coard cards.  Examples:
//   AdAc|KsKh/4s3h2d/8c/7d
//   AdAc|KsKh/4s3h2d
//   AdAc|
//   |KsKh
//   AdAc|/4s3h2d
// Hole cards and flop cards returned are sorted from high to low.
static bool ParseCardString(const string &cards, int *p2_hi, int *p2_lo, int *p1_hi, int *p1_lo,
			    int *board, int *street) {
  *p2_hi = 0;
  *p2_lo = 0;
  *p1_hi = 0;
  *p1_lo = 0;
  *street = 0;
  for (int i = 0; i < 5; ++i) board[i] = 0;
  vector<string> card_groups;
  Split(cards.c_str(), '/', false, &card_groups);
  if (card_groups.size() < 1) {
    Warning("Couldn't parse card groups\n");
    Warning("%s\n", cards.c_str());
    return false;
  }
  if (card_groups.size() > 4) {
    Warning("Too many card groups?!?\n");
    Warning("%s\n", cards.c_str());
    return false;
  }
  vector<string> hole_card_strings;
  const string &hole_card_pairs = card_groups[0];
  if (hole_card_pairs.size() == 9) {
    Split(hole_card_pairs.c_str(), '|', false, &hole_card_strings);
  } else if (hole_card_pairs.size() == 5) {
    if (hole_card_pairs[0] == '|') {
      hole_card_strings.push_back("");
      hole_card_strings.push_back(string(hole_card_pairs, 1, 4));
    } else if (hole_card_pairs[4] == '|') {
      hole_card_strings.push_back(string(hole_card_pairs, 0, 4));
      hole_card_strings.push_back("");
    }
  } else {
    int len = hole_card_pairs.size();
    fprintf(stderr, "len %i\n", len);
    fprintf(stderr, "\"%s\"\n", hole_card_pairs.c_str());
    exit(-1);
  }
  if (hole_card_strings.size() != 2) {
    Warning("Couldn't parse hole card pairs\n");
    Warning("%s\n", cards.c_str());
    Warning("%s\n", hole_card_pairs.c_str());
    return false;
  }
  for (int i = 0; i < 2; ++i) {
    if (hole_card_strings[i] == "") continue;
    int c1 = ParseCard(hole_card_strings[i].c_str());
    if (c1 == -1) return false;
    int c2 = ParseCard(hole_card_strings[i].c_str() + 2);
    if (c2 == -1) return false;
    int hi, lo;
    if (c1 > c2) { hi = c1; lo = c2; }
    else         { hi = c2; lo = c1; }
    if (i == 0) { *p2_hi = hi; *p2_lo = lo; }
    else        { *p1_hi = hi; *p1_lo = lo; }
  }

  if (card_groups.size() >= 2) {
    *street = 1;
    // Parse flop.  Order flop from high to low.
    int c1 = ParseCard(card_groups[1].c_str());
    if (c1 == -1) return false;
    int c2 = ParseCard(card_groups[1].c_str() + 2);
    if (c2 == -1) return false;
    int c3 = ParseCard(card_groups[1].c_str() + 4);
    if (c3 == -1) return false;
    if (c1 > c2 && c1 > c3 && c2 > c3) {
      board[0] = c1; board[1] = c2; board[2] = c3;
    } else if (c1 > c2 && c1 > c3 && c3 > c2) {
      board[0] = c1; board[1] = c3; board[2] = c2;
    } else if (c2 > c1 && c2 > c3 && c1 > c3) {
      board[0] = c2; board[1] = c1; board[2] = c3;
    } else if (c2 > c1 && c2 > c3 && c3 > c1) {
      board[0] = c2; board[1] = c3; board[2] = c1;
    } else if (c3 > c1 && c3 > c2 && c1 > c2) {
      board[0] = c3; board[1] = c1; board[2] = c2;
    } else if (c3 > c1 && c3 > c2 && c2 > c1) {
      board[0] = c3; board[1] = c2; board[2] = c1;
    }
  }

  if (card_groups.size() >= 3) {
    *street = 2;
    board[3] = ParseCard(card_groups[2].c_str());
    if (board[3] == -1) return false;
  }

  if (card_groups.size() >= 4) {
    *street = 3;
    board[4] = ParseCard(card_groups[3].c_str());
    if (board[4] == -1) return false;
  }

  return true;
}

// Messages sent over the wire have five components (when you split the line
// using ':' as a separator):
// 1) MATCHSTATE
// 2) Position (0 or 1)
// 3) Hand number
// 4) The betting action
// 5) The cards
static bool ParseMatchState(const string &match_state, bool *p1, int *hand_no,
			    string *action, int *our_hi, int *our_lo, int *opp_hi,
			    int *opp_lo, int *board, int *street) {
  *p1 = true;
  *hand_no = 0;
  *action = "";
  *our_hi = 0;
  *our_lo = 0;
  *opp_hi = 0;
  *opp_lo = 0;
  for (int i = 0; i < 5; ++i) board[i] = 0;
  vector<string> comps;
  Split(match_state.c_str(), ':', true, &comps);
  if (comps[0] != "MATCHSTATE") {
    Warning("Didn't find MATCHSTATE at beginning of line\n");
    return false;
  }
  if (comps.size() != 5) {
    Warning("Expected 5 components of MATCHSTATE line, found %i\n", (int)comps.size());
    Warning("%s\n", match_state.c_str());
    return false;
  }
  if (comps[1] == "0") {
    *p1 = false;
  } else if (comps[1] == "1") {
    *p1 = true;
  } else {
    Warning("Expected 0 or 1 as second component of match state\n");
    Warning("%s\n", match_state.c_str());
    return false;
  }
  if (sscanf(comps[2].c_str(), "%i", hand_no) != 1) {
    Warning("Couldn't parse hand number from match state\n");
    Warning("%s\n", match_state.c_str());
    return false;
  }
  *action = comps[3];
  int p2_hi, p2_lo, p1_hi, p1_lo;
  fprintf(stderr, "\"%s\"\n", comps[4].c_str());
  if (! ParseCardString(comps[4], &p2_hi, &p2_lo, &p1_hi, &p1_lo, board,
			street)) {
    return false;
  }
  if (*p1) {
    *our_hi = p1_hi;
    *our_lo = p1_lo;
    *opp_hi = p2_hi;
    *opp_lo = p2_lo;
  } else {
    *our_hi = p2_hi;
    *our_lo = p2_lo;
    *opp_hi = p1_hi;
    *opp_lo = p1_lo;
  }
  if (*our_hi == 0) {
    fprintf(stderr, "We expect to always have hole cards for ourself\n");
    fprintf(stderr, "%s\n", comps[4].c_str());
    return false;
  }
  return true;
}

MatchState *ParseACPCRequest(const string &request, int big_blind, int stack_size) {
  if (! strncmp(request.c_str(), "MATCHSTATE:", 11)) {
    bool p1;
    int hand_no, our_hi, our_lo, opp_hi, opp_lo, street;
    int board[5];
    string action;
    if (! ParseMatchState(request, &p1, &hand_no, &action, &our_hi, &our_lo, &opp_hi, &opp_lo,
			  board, &street)) {
      return nullptr;
    }
    MatchState *ms = new MatchState(p1, hand_no, action, our_hi, our_lo, opp_hi, opp_lo, board,
				    big_blind, stack_size);
    if (street != ms->Street()) {
      fprintf(stderr, "Card/action street mismatch: %i vs. %i\n", street, ms->Street());
      exit(-1);
    }
    return ms;
  } else if (request == "ENDGAME" || request == "#GAMEOVER"){
    return nullptr;
  } else {
    Warning("Unexpected request: \"%s\"\n", request.c_str());
    return nullptr;
  }
}
