#ifndef _CFR_CONFIG_H_
#define _CFR_CONFIG_H_

#include <memory>
#include <string>
#include <vector>

class Params;

class CFRConfig {
public:
  CFRConfig(const Params &params);
  ~CFRConfig(void) {}
  const std::string &CFRConfigName(void) const {return cfr_config_name_;}
  const std::string &Algorithm(void) const {return algorithm_;}
  bool NNR(void) const {return nnr_;}
  const std::vector<int> &RegretFloors(void) const {return regret_floors_;}
  const std::vector<int> &RegretCeilings(void) const {return regret_ceilings_;}
  const std::vector<int> &SumprobCeilings(void) const {
    return sumprob_ceilings_;
  }
  const std::vector<double> &RegretScaling(void) const {return regret_scaling_;}
  const std::vector<double> &SumprobScaling(void) const {return sumprob_scaling_;}
  int SoftWarmup(void) const {return soft_warmup_;}
  int HardWarmup(void) const {return hard_warmup_;}
  int SubgameStreet(void) const {return subgame_street_;}
  int SamplingRate(void) const {return sampling_rate_;}
  const std::vector<int> &SumprobStreets(void) const {
    return sumprob_streets_;
  }
  const std::vector<unsigned int> &PruningThresholds(void) const {
    return pruning_thresholds_;
  }
  bool HVBTable(void) const {return hvb_table_;}
  unsigned int CloseThreshold(void) const {return close_threshold_;}
  bool FTL(void) const {return ftl_;}
  bool SampleOppHands(void) const {return sample_opp_hands_;}
  double Explore(void) const {return explore_;}
  bool Probe(void) const {return probe_;}
  bool CharQuantizedStreet(int st) const {
    return char_quantized_streets_[st];
  }
  bool ShortQuantizedStreet(int st) const {return short_quantized_streets_[st];}
  bool ScaledStreet(int st) const {return scaled_streets_[st];}
  int ActiveMod(void) const {return active_mod_;}
  int NumActiveConditions(void) const {return num_active_conditions_;}
  int NumActiveStreets(int c) const {return active_streets_[c].size();}
  int ActiveStreet(int c, int i) const {return active_streets_[c][i];}
  int NumActiveRems(int c) const {return active_rems_[c].size();}
  int ActiveRem(int c, int i) const {return active_rems_[c][i];}
  int BatchSize(void) const {return batch_size_;}
  int SaveInterval(void) const {return save_interval_;}
  bool DoubleRegrets(void) const {return double_regrets_;}
  bool DoubleSumprobs(void) const {return double_sumprobs_;}
  const std::vector<int> &CompressedStreets(void) const {
    return compressed_streets_;
  }
  bool Uniform(void) const {return uniform_;}
  bool DealTwice(void) const {return deal_twice_;}
  const std::vector<double> &BoostThresholds(void) const {return boost_thresholds_;}
  const std::vector<int> &Freeze(void) const {return freeze_;}
 private:
  std::string cfr_config_name_;
  std::string algorithm_;
  bool nnr_;
  std::vector<int> regret_floors_;
  std::vector<int> regret_ceilings_;
  std::vector<int> sumprob_ceilings_;
  std::vector<double> regret_scaling_;
  std::vector<double> sumprob_scaling_;
  int soft_warmup_;
  int hard_warmup_;
  int subgame_street_;
  int sampling_rate_;
  std::vector<int> sumprob_streets_;
  std::vector<unsigned int> pruning_thresholds_;
  bool hvb_table_;
  unsigned int close_threshold_;
  bool ftl_;
  bool sample_opp_hands_;
  double explore_;
  bool probe_;
  std::unique_ptr<bool []> char_quantized_streets_;
  std::unique_ptr<bool []> short_quantized_streets_;
  std::unique_ptr<bool []> scaled_streets_;
  int active_mod_;
  int num_active_conditions_;
  std::unique_ptr<std::vector<int> []> active_streets_;
  std::unique_ptr<std::vector<int> []> active_rems_;
  int batch_size_;
  int save_interval_;
  bool double_regrets_;
  bool double_sumprobs_;
  std::vector<int> compressed_streets_;
  bool uniform_;
  bool deal_twice_;
  std::vector<double> boost_thresholds_;
  std::vector<int> freeze_;
};

#endif
