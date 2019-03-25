#ifndef _RGBR_H_
#define _RGBR_H_

#include "cfrp.h"

class BettingAbstraction;
class BettingTree;
class Buckets;
class CardAbstraction;
class CFRConfig;

class RGBR : public CFRP {
public:
  RGBR(const CardAbstraction &ca, const BettingAbstraction &ba, const CFRConfig &cc,
       const Buckets &buckets, const BettingTree *betting_tree, bool current, int num_threads,
       const bool *streets);
  virtual ~RGBR(void);
  double Go(int it, int p);
};

#endif
