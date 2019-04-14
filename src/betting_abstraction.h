#ifndef _BETTING_ABSTRACTION_H_
#define _BETTING_ABSTRACTION_H_

#include <memory>
#include <string>
#include <vector>

class Params;

class BettingAbstraction {
 public:
  BettingAbstraction(const Params &params);
  virtual ~BettingAbstraction(void) {}
  const std::string &BettingAbstractionName(void) const {
    return betting_abstraction_name_;
  }
  bool Limit(void) const {return limit_;}
  int StackSize(void) const {return stack_size_;}
  int MinBet(void) const {return min_bet_;}
  bool AllBetSizeStreet(int st) const {return all_bet_sizes_[st];}
  bool AllEvenBetSizeStreet(int st) const {
    return all_even_bet_sizes_[st];
  }
  int InitialStreet(void) const {return initial_street_;}
  int MaxBets(int st, bool our_bet) const {
    if (max_bets_.size() > 0) return max_bets_[st];
    else if (our_bet)         return our_max_bets_[st];
    else                      return opp_max_bets_[st];
  }
  int NumBetSizes(int st, int npb, bool our_bet) const {
    if (bet_sizes_.size() > 0)   return bet_sizes_[st][npb].size();
    else if (our_bet)            return our_bet_sizes_[st][npb].size();
    else                         return opp_bet_sizes_[st][npb].size();
  }
  const std::vector<double> &BetSizes(int st, int npb, bool our_bet, int pa) const {
    if (bet_sizes_.size() > 0) return bet_sizes_[st][npb];
    else if (our_bet)          return our_bet_sizes_[st][npb];
    else                       return opp_bet_sizes_[st][npb];
  }
  bool Asymmetric(void) const {return asymmetric_;}
  bool AlwaysAllIn(void) const {return always_all_in_;}
  bool OurAlwaysAllIn(void) const {return our_always_all_in_;}
  bool OppAlwaysAllIn(void) const {return opp_always_all_in_;}
  bool AlwaysMinBet(int st, int nsb) const {
    if (always_min_bet_.size() == 0) return false;
    return always_min_bet_[st][nsb];
  }
  bool OurAlwaysMinBet(int st, int nsb) const {
    if (our_always_min_bet_.size() == 0) return false;
    return our_always_min_bet_[st][nsb];
  }
  bool OppAlwaysMinBet(int st, int nsb) const {
    if (opp_always_min_bet_.size() == 0) return false;
    return opp_always_min_bet_[st][nsb];
  }
  int MinAllInPot(void) const {return min_all_in_pot_;}
  int NoLimitTreeType(void) const {return no_limit_tree_type_;}
  bool NoOpenLimp(void) const {return no_open_limp_;}
  bool OurNoOpenLimp(void) const {return our_no_open_limp_;}
  bool OppNoOpenLimp(void) const {return opp_no_open_limp_;}
  int NoRegularBetThreshold(void) const {
    return no_regular_bet_threshold_;
  }
  int OurNoRegularBetThreshold(void) const {
    return our_no_regular_bet_threshold_;
  }
  int OppNoRegularBetThreshold(void) const {
    return opp_no_regular_bet_threshold_;
  }
  int OnlyPotThreshold(void) const {return only_pot_threshold_;}
  int OurOnlyPotThreshold(void) const {
    return our_only_pot_threshold_;
  }
  int OppOnlyPotThreshold(void) const {
    return opp_only_pot_threshold_;
  }
  int GeometricType(void) const {return geometric_type_;}
  int OurGeometricType(void) const {return our_geometric_type_;}
  int OppGeometricType(void) const {return opp_geometric_type_;}
  double CloseToAllInFrac(void) const {return close_to_all_in_frac_;}
  double BetSizeMultiplier(int st, int npb, bool our_bet) const {
    if (our_bet && our_bet_size_multipliers_.size() > 0) {
      return our_bet_size_multipliers_[st][npb];
    } else if (! our_bet && opp_bet_size_multipliers_.size() > 0) {
      return opp_bet_size_multipliers_[st][npb];
    } else {
      return 0;
    }
  }
  bool ReentrantStreet(int st) const {return reentrant_streets_[st];}
  bool BettingKey(int st) const {return betting_key_[st];}
  int MinReentrantPot(void) const {return min_reentrant_pot_;}
  int MinReentrantBets(int st, int num_rem) const {
    if (merge_rules_.size() == 0) return 0;
    else                          return merge_rules_[st][num_rem];
  }
  bool AllowableBetTo(int bt) const {
    if (allowable_bet_tos_.get() == nullptr) return true;
    else                                     return allowable_bet_tos_[bt];
  }
  bool LastAggressorKey(void) const {return last_aggressor_key_;}
 private:
  void ParseMinBets(const std::string &value, std::vector< std::vector<bool> > *min_bets);
  
  std::string betting_abstraction_name_;
  bool limit_;
  int stack_size_;
  int min_bet_;
  std::unique_ptr<bool []> all_bet_sizes_;
  std::unique_ptr<bool []> all_even_bet_sizes_;
  int initial_street_;
  std::vector<int> max_bets_;
  std::vector<int> our_max_bets_;
  std::vector<int> opp_max_bets_;
  std::vector<std::vector<std::vector<double> > > bet_sizes_;
  std::vector<std::vector<std::vector<double> > > our_bet_sizes_;
  std::vector<std::vector<std::vector<double> > > opp_bet_sizes_;
  bool asymmetric_;
  bool always_all_in_;
  bool our_always_all_in_;
  bool opp_always_all_in_;
  std::vector< std::vector<bool> > always_min_bet_;
  std::vector< std::vector<bool> > our_always_min_bet_;
  std::vector< std::vector<bool> > opp_always_min_bet_;
  int min_all_in_pot_;
  int no_limit_tree_type_;
  bool no_open_limp_;
  bool our_no_open_limp_;
  bool opp_no_open_limp_;
  int no_regular_bet_threshold_;
  int our_no_regular_bet_threshold_;
  int opp_no_regular_bet_threshold_;
  int only_pot_threshold_;
  int our_only_pot_threshold_;
  int opp_only_pot_threshold_;
  int geometric_type_;
  int our_geometric_type_;
  int opp_geometric_type_;
  double close_to_all_in_frac_;
  std::vector< std::vector<double> > our_bet_size_multipliers_;
  std::vector< std::vector<double> > opp_bet_size_multipliers_;
  std::unique_ptr<bool []> reentrant_streets_;
  std::unique_ptr<bool []> betting_key_;
  int min_reentrant_pot_;
  std::vector< std::vector<int> > merge_rules_;
  std::unique_ptr<bool []> allowable_bet_tos_;
  bool last_aggressor_key_;
};

#endif
