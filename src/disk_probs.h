#ifndef _DISK_PROBS_H_
#define _DISK_PROBS_H_

#include <memory>

class BettingAbstraction;
class BettingTree;
class Buckets;
class CardAbstraction;
class CFRConfig;
class Node;
class Reader;

class DiskProbs {
public:
  DiskProbs(const CardAbstraction &ca, const BettingAbstraction &ba, const CFRConfig &cc,
	    const Buckets &buckets, const BettingTree *betting_tree, int it);
  ~DiskProbs(void);
  void Probs(int p, int st, int nt, int b, int num_succs, double *probs);
private:
  void ComputeOffsets(Node *node, long long int **current_offsets);

  const Buckets &buckets_;
  std::unique_ptr<int []> prob_sizes_;
  long long int ***offsets_;
  Reader ***readers_;
};

#endif
