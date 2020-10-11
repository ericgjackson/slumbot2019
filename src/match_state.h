#ifndef _MATCH_STATE_H_
#define _MATCH_STATE_H_

#include <string>

class MatchState {
 public:
  MatchState(bool p1, int hand_no, const std::string &action, int our_hi, int our_lo, int opp_hi,
	     int opp_lo, int *board, int big_blind, int stack_size);
  ~MatchState(void) {}
  bool P1(void) const {return p1_;}
  int HandNo(void) const {return hand_no_;}
  const std::string &Action(void) const {return action_;}
  int OurHi(void) const {return our_hi_;}
  int OurLo(void) const {return our_lo_;}
  int OppHi(void) const {return opp_hi_;}
  int OppLo(void) const {return opp_lo_;}
  const int *Board(void) const {return board_;}
  int Street(void) const {return street_;}
  bool Terminal(void) const {return terminal_;}
  void ProcessAction(void);
 private:
  bool p1_;
  int hand_no_;
  bool terminal_;
  std::string action_;
  int street_;
  int our_hi_;
  int our_lo_;
  int opp_hi_;
  int opp_lo_;
  int board_[5];
  int big_blind_;
  int stack_size_;
};

#endif
