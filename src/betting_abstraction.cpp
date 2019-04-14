#include <stdio.h>
#include <stdlib.h>

#include <string>
#include <vector>

#include "betting_abstraction.h"
#include "constants.h"
#include "game.h"
#include "params.h"
#include "split.h"

using std::string;
using std::vector;

static void ParseBetSizes(const string &param_value,
			  vector<vector<vector<double> > > *bet_sizes) {
  vector<string> v1;
  Split(param_value.c_str(), '|', true, &v1);
  int v1sz = v1.size();
  bet_sizes->resize(v1sz);
  for (int st = 0; st < v1sz; ++st) {
    const string &s2 = v1[st];
    if (s2 != "") {
      vector<string> v2;
      Split(s2.c_str(), ';', false, &v2);
      int v2sz = v2.size();
      (*bet_sizes)[st].resize(v2sz);
      for (int i = 0; i < v2sz; ++i) {
	const string &s3 = v2[i];
	vector<string> v3;
	Split(s3.c_str(), ',', false, &v3);
	int v3sz = v3.size();
	(*bet_sizes)[st][i].resize(v3sz);
	for (int j = 0; j < v3sz; ++j) {
	  double frac;
	  if (sscanf(v3[j].c_str(), "%lf", &frac) != 1) {
	    fprintf(stderr, "Couldn't parse bet sizes: %s\n", param_value.c_str());
	    exit(-1);
	  }
	  (*bet_sizes)[st][i][j] = frac;
	}
      }
    }
  }
}

static void ParseMultipliers(const string &param_value, vector<vector<double> > *multipliers) {
  vector<string> v1;
  Split(param_value.c_str(), '|', true, &v1);
  int v1sz = v1.size();
  multipliers->resize(v1sz);
  for (int st = 0; st < v1sz; ++st) {
    const string &s2 = v1[st];
    if (s2 != "") {
      vector<string> v2;
      Split(s2.c_str(), ';', false, &v2);
      int v2sz = v2.size();
      (*multipliers)[st].resize(v2sz);
      for (int i = 0; i < v2sz; ++i) {
	double m;
	if (sscanf(v2[i].c_str(), "%lf", &m) != 1) {
	  fprintf(stderr, "Couldn't parse multipliers: %s\n", param_value.c_str());
	  exit(-1);
	}
	(*multipliers)[st][i] = m;
      }
    }
  }
}

static void ParseMaxBets(const Params &params, const string &param, vector<int> *max_bets) {
  if (! params.IsSet(param.c_str())) {
    fprintf(stderr, "%s must be set\n", param.c_str());
    exit(-1);
  }
  const string &pv = params.GetStringValue(param.c_str());
  int max_street = Game::MaxStreet();
  max_bets->resize(max_street + 1);
  vector<int> v;
  ParseInts(pv, &v);
  int num = v.size();
  if (num < max_street + 1) {
    fprintf(stderr, "Expect at least %u max bets values\n", max_street + 1);
    exit(-1);
  }
  for (int st = 0; st <= max_street; ++st) {
    (*max_bets)[st] = v[st];
  }
}

void BettingAbstraction::ParseMinBets(const string &value, vector< vector<bool> > *min_bets) {
  int max_street = Game::MaxStreet();
  vector<string> v1;
  Split(value.c_str(), ';', true, &v1);
  if ((int)v1.size() != max_street + 1) {
    fprintf(stderr, "ParseMinBets: expected %i street values\n", max_street + 1);
    exit(-1);
  }
  min_bets->resize(max_street + 1);
  for (int st = 0; st <= max_street; ++st) {
    int max_bets = max_bets_[st];
    (*min_bets)[st].resize(max_bets);
    // Default
    for (int b = 0; b < max_bets; ++b) (*min_bets)[st][b] = false;
    const string &sv = v1[st];
    vector<string> v2;
    Split(sv.c_str(), ',', false, &v2);
    int num = v2.size();
    for (int i = 0; i < num; ++i) {
      const string &sv2 = v2[i];
      int nb;
      if (sscanf(sv2.c_str(), "%i", &nb) != 1) {
	fprintf(stderr, "ParseMinBets: couldn't parse %s\n", value.c_str());
	exit(-1);
      }
      if (nb >= max_bets) {
	fprintf(stderr, "ParseMinBets: OOB value %u\n", nb);
	exit(-1);
      }
      (*min_bets)[st][nb] = true;
    }
  }
}

static void ParseMergeRules(const string &value, vector< vector<int> > *merge_rules) {
  int max_street = Game::MaxStreet();
  int num_players = Game::NumPlayers();
  vector<string> v1;
  Split(value.c_str(), ';', true, &v1);
  if ((int)v1.size() != max_street + 1) {
    fprintf(stderr, "ParseMergeRules: expected %u street values\n",
	    max_street + 1);
    exit(-1);
  }
  merge_rules->resize(max_street + 1);
  for (int st = 0; st <= max_street; ++st) {
    const string &sv = v1[st];
    vector<string> v2;
    Split(sv.c_str(), ',', false, &v2);
    if ((int)v2.size() != num_players + 1) {
      fprintf(stderr, "ParseMergeRules: expected v2 size to be num players + 1\n");
      exit(-1);
    }
    (*merge_rules)[st].resize(num_players + 1);
    for (int p = 0; p <= num_players; ++p) {
      const string &str = v2[p];
      int nb;
      if (sscanf(str.c_str(), "%u", &nb) != 1) {
	fprintf(stderr, "ParseMergeRules: couldn't parse %s\n", value.c_str());
	exit(-1);
      }
      (*merge_rules)[st][p] = nb;
    }
  }
}

BettingAbstraction::BettingAbstraction(const Params &params) {
  betting_abstraction_name_ = params.GetStringValue("BettingAbstractionName");
  limit_ = params.GetBooleanValue("Limit");
  stack_size_ = params.GetIntValue("StackSize");
  min_bet_ = params.GetIntValue("MinBet");
  int max_street = Game::MaxStreet();
  all_bet_sizes_.reset(new bool[max_street + 1]);
  for (int st = 0; st <= max_street; ++st) {
    // Default
    all_bet_sizes_[st] = false;
  }
  if (params.IsSet("AllBetSizeStreets")) {
    vector<int> v;
    ParseInts(params.GetStringValue("AllBetSizeStreets"), &v);
    int num = v.size();
    for (int i = 0; i < num; ++i) {
      all_bet_sizes_[v[i]] = true;
    }
  }
  all_even_bet_sizes_.reset(new bool[max_street + 1]);
  for (int st = 0; st <= max_street; ++st) {
    // Default
    all_even_bet_sizes_[st] = false;
  }
  if (params.IsSet("AllEvenBetSizeStreets")) {
    vector<int> v;
    ParseInts(params.GetStringValue("AllEvenBetSizeStreets"), &v);
    int num = v.size();
    for (int i = 0; i < num; ++i) {
      all_even_bet_sizes_[v[i]] = true;
    }
  }
  initial_street_ = params.GetIntValue("InitialStreet");
  asymmetric_ = params.GetBooleanValue("Asymmetric");

  no_limit_tree_type_ = params.GetIntValue("NoLimitTreeType");

  if (asymmetric_) {
    ParseMaxBets(params, "OurMaxBets", &our_max_bets_);
    ParseMaxBets(params, "OppMaxBets", &opp_max_bets_);
  } else {
    ParseMaxBets(params, "MaxBets", &max_bets_);
  }

  bool need_bet_sizes = false;
  for (int st = 0; st <= max_street; ++st) {
    if (! all_bet_sizes_[st] && ! all_even_bet_sizes_[st]) {
      need_bet_sizes = true;
      break;
    }
  }
  
  if (no_limit_tree_type_ == 3) {
  } else {
    if (need_bet_sizes) {
      if (asymmetric_) {
	if (params.IsSet("BetSizes")) {
	  fprintf(stderr, "Use OurBetSizes and OppBetSizes for asymmetric "
		  "systems, not BetSizes\n");
	  exit(-1);
	}
	if (! params.IsSet("OurBetSizes") || ! params.IsSet("OppBetSizes")) {
	  fprintf(stderr, "Expect OurBetSizes and OppBetSizes to be set\n");
	  exit(-1);
	}
	ParseBetSizes(params.GetStringValue("OurBetSizes"), &our_bet_sizes_);
	for (int st = 0; st <= max_street; ++st) {
	  if ((int)our_bet_sizes_[st].size() != our_max_bets_[st]) {
	    fprintf(stderr, "Max bets mismatch\n");
	    exit(-1);
	  }
	}
	ParseBetSizes(params.GetStringValue("OppBetSizes"), &opp_bet_sizes_);
	for (int st = 0; st <= max_street; ++st) {
	  if ((int)opp_bet_sizes_[st].size() != opp_max_bets_[st]) {
	    fprintf(stderr, "Max bets mismatch\n");
	    exit(-1);
	  }
	}
      } else {
	if (! params.IsSet("BetSizes")) {
	  fprintf(stderr, "Expect BetSizes to be set\n");
	  exit(-1);
	}
	ParseBetSizes(params.GetStringValue("BetSizes"), &bet_sizes_);
	for (int st = 0; st <= max_street; ++st) {
	  if ((int)bet_sizes_[st].size() != max_bets_[st]) {
	    fprintf(stderr, "Max bets mismatch\n");
	    exit(-1);
	  }
	}
      }
    }
  }

  always_all_in_ = params.GetBooleanValue("AlwaysAllIn");
  our_always_all_in_ = params.GetBooleanValue("OurAlwaysAllIn");
  opp_always_all_in_ = params.GetBooleanValue("OppAlwaysAllIn");

  if (params.IsSet("MinBets")) {
    ParseMinBets(params.GetStringValue("MinBets"), &always_min_bet_);
  }
  if (params.IsSet("OurMinBets")) {
    ParseMinBets(params.GetStringValue("OurMinBets"), &our_always_min_bet_);
  }
  if (params.IsSet("OppMinBets")) {
    ParseMinBets(params.GetStringValue("OppMinBets"), &opp_always_min_bet_);
  }
  
  min_all_in_pot_ = params.GetIntValue("MinAllInPot");
  no_open_limp_ = params.GetBooleanValue("NoOpenLimp");
  our_no_open_limp_ = params.GetBooleanValue("OurNoOpenLimp");
  opp_no_open_limp_ = params.GetBooleanValue("OppNoOpenLimp");
  if (params.IsSet("NoRegularBetThreshold")) {
    no_regular_bet_threshold_ = params.GetIntValue("NoRegularBetThreshold");
  } else {
    no_regular_bet_threshold_ = kMaxInt;
  }
  if (params.IsSet("OurNoRegularBetThreshold")) {
    our_no_regular_bet_threshold_ =
      params.GetIntValue("OurNoRegularBetThreshold");
  } else {
    our_no_regular_bet_threshold_ = kMaxInt;
  }
  if (params.IsSet("OppNoRegularBetThreshold")) {
    opp_no_regular_bet_threshold_ =
      params.GetIntValue("OppNoRegularBetThreshold");
  } else {
    opp_no_regular_bet_threshold_ = kMaxInt;
  }
  if (params.IsSet("OnlyPotThreshold")) {
    only_pot_threshold_ = params.GetIntValue("OnlyPotThreshold");
  } else {
    only_pot_threshold_ = kMaxInt;
  }
  if (params.IsSet("OurOnlyPotThreshold")) {
    our_only_pot_threshold_ = params.GetIntValue("OurOnlyPotThreshold");
  } else {
    our_only_pot_threshold_ = kMaxInt;
  }
  if (params.IsSet("OppOnlyPotThreshold")) {
    opp_only_pot_threshold_ = params.GetIntValue("OppOnlyPotThreshold");
  } else {
    opp_only_pot_threshold_ = kMaxInt;
  }
  geometric_type_ = params.GetIntValue("GeometricType");
  our_geometric_type_ = params.GetIntValue("OurGeometricType");
  opp_geometric_type_ = params.GetIntValue("OppGeometricType");
  close_to_all_in_frac_ = params.GetDoubleValue("CloseToAllInFrac");
  if (params.IsSet("OurBetSizeMultipliers")) {
    ParseMultipliers(params.GetStringValue("OurBetSizeMultipliers"), &our_bet_size_multipliers_);
  }
  if (params.IsSet("OppBetSizeMultipliers")) {
    ParseMultipliers(params.GetStringValue("OppBetSizeMultipliers"), &opp_bet_size_multipliers_);
  }
  reentrant_streets_.reset(new bool[max_street + 1]);
  for (int st = 0; st <= max_street; ++st) {
    // Default
    reentrant_streets_[st] = false;
  }
  if (params.IsSet("ReentrantStreets")) {
    vector<int> v;
    ParseInts(params.GetStringValue("ReentrantStreets"), &v);
    int num = v.size();
    for (int i = 0; i < num; ++i) {
      reentrant_streets_[v[i]] = true;
    }
  }
  betting_key_.reset(new bool[max_street + 1]);
  for (int st = 0; st <= max_street; ++st) {
    // Default
    betting_key_[st] = false;
  }
  if (params.IsSet("BettingKeyStreets")) {
    vector<int> v;
    ParseInts(params.GetStringValue("BettingKeyStreets"), &v);
    int num = v.size();
    for (int i = 0; i < num; ++i) {
      betting_key_[v[i]] = true;
    }
  }
  min_reentrant_pot_ = params.GetIntValue("MinReentrantPot");
  if (params.IsSet("MergeRules")) {
    ParseMergeRules(params.GetStringValue("MergeRules"), &merge_rules_);
  }
  if (params.IsSet("AllowableBetTos")) {
    vector<int> v;
    ParseInts(params.GetStringValue("AllowableBetTos"), &v);
    int num = v.size();
    allowable_bet_tos_.reset(new bool[stack_size_ + 1]);
    for (int bt = 0; bt <= stack_size_; ++bt) {
      allowable_bet_tos_[bt] = false;
    }
    for (int i = 0; i < num; ++i) {
      int bt = v[i];
      if (bt > stack_size_) {
	fprintf(stderr, "OOB bet to %u\n", bt);
	exit(-1);
      }
      allowable_bet_tos_[bt] = true;
    }
  } else {
    allowable_bet_tos_.reset(nullptr);
  }
  last_aggressor_key_ = params.GetBooleanValue("LastAggressorKey");
}
