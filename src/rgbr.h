#ifndef _RGBR_H_
#define _RGBR_H_

#include "cfrp.h"

class BettingAbstraction;
class Buckets;
class CardAbstraction;
class CFRConfig;

class RGBR : public CFRP {
public:
  RGBR(const CardAbstraction &ca, const CFRConfig &cc, const Buckets &buckets, bool current,
       bool quantize, int num_threads, const bool *streets);
  virtual ~RGBR(void);
  double Go(int it, int p, const BettingAbstraction &ba);
 private:
  bool quantize_;
};

#endif
