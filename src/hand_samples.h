#ifndef _HAND_SAMPLES_H_
#define _HAND_SAMPLES_H_

class HandSamples {
public:
  HandSamples(int sample_st);
  ~HandSamples(void);
  void AddBoard(int bd, int num_samples);
  int Count(int st, int bd, int hcp) const;
private:
  int sample_st_;
  int ***counts_;
};

#endif
