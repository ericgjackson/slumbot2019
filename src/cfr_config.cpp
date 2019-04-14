#include <stdio.h>
#include <stdlib.h>

#include <string>
#include <vector>

#include "cfr_config.h"
#include "constants.h"
#include "game.h"
#include "params.h"
#include "split.h"

using std::string;
using std::vector;

CFRConfig::CFRConfig(const Params &params) {
  cfr_config_name_ = params.GetStringValue("CFRConfigName");
  algorithm_ = params.GetStringValue("Algorithm");
  nnr_ = params.GetBooleanValue("NNR");
  ParseInts(params.GetStringValue("RegretFloors"), &regret_floors_);
  ParseInts(params.GetStringValue("RegretCeilings"), &regret_ceilings_);
  ParseInts(params.GetStringValue("SumprobCeilings"), &sumprob_ceilings_);
  ParseDoubles(params.GetStringValue("RegretScaling"), &regret_scaling_);
  ParseDoubles(params.GetStringValue("SumprobScaling"), &sumprob_scaling_);
  if (params.IsSet("SoftWarmup") && params.IsSet("HardWarmup")) {
    fprintf(stderr, "Cannot have both soft and hard warmup\n");
    exit(-1);
  }
  soft_warmup_ = params.GetIntValue("SoftWarmup");
  hard_warmup_ = params.GetIntValue("HardWarmup");
  if (params.IsSet("SubgameStreet")) {
    subgame_street_ = params.GetIntValue("SubgameStreet");
  } else {
    subgame_street_ = -1;
  }
  if (params.IsSet("SamplingRate")) {
    sampling_rate_ = params.GetIntValue("SamplingRate");
  }
  const string &str = params.GetStringValue("SumprobStreets");
  if (str == "") {
    // Default is to maintain sumprobs for all streets
    int max_street = Game::MaxStreet();
    for (int st = 0; st <= max_street; ++st) {
      sumprob_streets_.push_back(st);
    }
  } else {
    ParseInts(str, &sumprob_streets_);
  }
  int max_street = Game::MaxStreet();
  const string &pstr = params.GetStringValue("PruningThresholds");
  if (pstr == "") {
    // Default is no pruning on any street.  kMaxInt signifies no pruning.
    for (int st = 0; st <= max_street; ++st) {
      pruning_thresholds_.push_back(kMaxUnsignedInt);
    }
  } else {
    ParseUnsignedInts(pstr, &pruning_thresholds_);
    if ((int)pruning_thresholds_.size() != max_street + 1) {
      fprintf(stderr, "Didn't see expected number of pruning thresholds\n");
      exit(-1);
    }
  }
  hvb_table_ = params.GetBooleanValue("HVBTable");
  ftl_ = params.GetBooleanValue("FTL");
  sample_opp_hands_ = params.GetBooleanValue("SampleOppHands");
  explore_ = params.GetDoubleValue("Explore");
  probe_ = params.GetBooleanValue("Probe");
  char_quantized_streets_.reset(new bool[max_street + 1]);
  short_quantized_streets_.reset(new bool[max_street + 1]);
  scaled_streets_.reset(new bool[max_street + 1]);
  for (int st = 0; st <= max_street; ++st) {
    char_quantized_streets_[st] = false;
    short_quantized_streets_[st] = false;
    scaled_streets_[st] = false;
  }
  vector<int> cqsv, sqsv, ssv;
  ParseInts(params.GetStringValue("CharQuantizedStreets"), &cqsv);
  int num_cqsv = cqsv.size();
  for (int i = 0; i < num_cqsv; ++i) {
    char_quantized_streets_[cqsv[i]] = true;
  }
  ParseInts(params.GetStringValue("ShortQuantizedStreets"), &sqsv);
  int num_sqsv = sqsv.size();
  for (int i = 0; i < num_sqsv; ++i) {
    short_quantized_streets_[sqsv[i]] = true;
  }
  ParseInts(params.GetStringValue("ScaledStreets"), &ssv);
  int num_ssv = ssv.size();
  for (int i = 0; i < num_ssv; ++i) {
    scaled_streets_[ssv[i]] = true;
  }
  double_regrets_ = params.GetBooleanValue("DoubleRegrets");
  double_sumprobs_ = params.GetBooleanValue("DoubleSumprobs");
  ParseInts(params.GetStringValue("CompressedStreets"), &compressed_streets_);

  close_threshold_ = params.GetIntValue("CloseThreshold");
  active_mod_ = params.GetIntValue("ActiveMod");
  vector<string> conditions;
  Split(params.GetStringValue("ActiveConditions").c_str(), ';', false,
	&conditions);
  num_active_conditions_ = conditions.size();
  if (num_active_conditions_ > 0) {
    active_streets_.reset(new vector<int>[num_active_conditions_]);
    active_rems_.reset(new vector<int>[num_active_conditions_]);
    for (int c = 0; c < num_active_conditions_; ++c) {
      vector<string> comps, streets, rems;
      Split(conditions[c].c_str(), ':', false, &comps);
      if (comps.size() != 2) {
	fprintf(stderr, "Expected two components\n");
	exit(-1);
      }
      Split(comps[0].c_str(), ',', false, &streets);
      Split(comps[1].c_str(), ',', false, &rems);
      if (streets.size() == 0) {
	fprintf(stderr, "Expected at least one street\n");
	exit(-1);
      }
      if (rems.size() == 0) {
	fprintf(stderr, "Expected at least one rem\n");
	exit(-1);
      }
      for (int i = 0; i < (int)streets.size(); ++i) {
	int st;
	if (sscanf(streets[i].c_str(), "%u", &st) != 1) {
	  fprintf(stderr, "Couldn't parse street\n");
	  exit(-1);
	}
	active_streets_[c].push_back(st);
      }
      for (int i = 0; i < (int)rems.size(); ++i) {
	int rem;
	if (sscanf(rems[i].c_str(), "%u", &rem) != 1) {
	  fprintf(stderr, "Couldn't parse rem\n");
	  exit(-1);
	}
	active_rems_[c].push_back(rem);
      }
    }
  }

  uniform_ = params.GetBooleanValue("Uniform");
  deal_twice_ = params.GetBooleanValue("DealTwice");
  ParseDoubles(params.GetStringValue("BoostThresholds"), &boost_thresholds_);
  ParseInts(params.GetStringValue("Freeze"), &freeze_);
}
