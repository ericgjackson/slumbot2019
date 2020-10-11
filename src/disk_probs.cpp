// Shouldn't do anything for resolve streets.

#include <stdio.h>
#include <stdlib.h>

#include <memory>

#include "betting_abstraction.h"
#include "betting_tree.h"
#include "buckets.h"
#include "card_abstraction.h"
#include "cfr_config.h"
#include "cfr_value_type.h"
#include "disk_probs.h"
#include "files.h"
#include "game.h"
#include "io.h"

using std::unique_ptr;

void DiskProbs::ComputeOffsets(Node *node, long long int **current_offsets) {
  if (node->Terminal()) return;
  int st = node->Street();
  int nt = node->NonterminalID();
  int p = node->PlayerActing();
  long long int offset = current_offsets[p][st];
  offsets_[p][st][nt] = offset;
  int num_buckets = buckets_.NumBuckets(st);
  int num_succs = node->NumSuccs();
  if (num_succs > 1) {
    current_offsets[p][st] += num_buckets * prob_sizes_[st] * num_succs;
  }
  for (int s = 0; s < num_succs; ++s) {
    ComputeOffsets(node->IthSucc(s), current_offsets);
  }
}

// Will return the smallest type available
static CFRValueType ValueType(int st, const char *dir, int it) {
  char buf[500];

  for (int t = 0; t < 4; ++t) {
    CFRValueType value_type;
    char suffix;
    if (t == 0) {
      suffix = 'c';
      value_type = CFRValueType::CFR_CHAR;
    } else if (t == 1) {
      suffix = 's';
      value_type = CFRValueType::CFR_SHORT;
    } else if (t == 2) {
      suffix = 'i';
      value_type = CFRValueType::CFR_INT;
    } else if (t == 3) {
      suffix = 'd';
      value_type = CFRValueType::CFR_DOUBLE;
    }
    sprintf(buf, "%s/sumprobs.x.0.0.%i.%i.p0.%c", dir, st, it, suffix);
    if (FileExists(buf)) {
      return value_type;
    }
  }
  fprintf(stderr, "No sumprobs files for street %i\n", st);
  exit(-1);
}

DiskProbs::DiskProbs(const CardAbstraction &ca, const BettingAbstraction &ba, const CFRConfig &cc,
		     const Buckets &buckets, const BettingTree *betting_tree, int it) :
  buckets_(buckets) {
  char dir[500];
  sprintf(dir, "%s/%s.%u.%s.%i.%i.%i.%s.%s", Files::OldCFRBase(),
	  Game::GameName().c_str(), Game::NumPlayers(),
	  ca.CardAbstractionName().c_str(), Game::NumRanks(),
	  Game::NumSuits(), Game::MaxStreet(),
	  ba.BettingAbstractionName().c_str(),
	  cc.CFRConfigName().c_str());
  int max_street = Game::MaxStreet();
  prob_sizes_.reset(new int[max_street + 1]);
  for (int st = 0; st <= max_street; ++st) {
    CFRValueType value_type = ValueType(st, dir, it);
    if (value_type == CFRValueType::CFR_CHAR) {
      prob_sizes_[st] = 1;
    } else if (value_type == CFRValueType::CFR_SHORT) {
      prob_sizes_[st] = 2;
    } else if (value_type == CFRValueType::CFR_INT) {
      prob_sizes_[st] = 4;
    } else if (value_type == CFRValueType::CFR_DOUBLE) {
      prob_sizes_[st] = 8;
    }
  }
  int num_players = Game::NumPlayers();
  offsets_ = new long long int **[num_players];
  for (int p = 0; p < num_players; ++p) {
    offsets_[p] = new long long int *[max_street + 1];
    for (int st = 0; st <= max_street; ++st) {
      int num_nt = betting_tree->NumNonterminals(p, st);
      offsets_[p][st] = new long long int[num_nt];
    }
  }
  long long int **current_offsets = new long long int *[num_players];
  for (int p = 0; p < num_players; ++p) {
    current_offsets[p] = new long long int[max_street + 1];
    for (int st = 0; st <= max_street; ++st) {
      current_offsets[p][st] = 0LL;
    }
  }
  ComputeOffsets(betting_tree->Root(), current_offsets);
  for (int p = 0; p < num_players; ++p) {
    delete [] current_offsets[p];
  }
  delete [] current_offsets;

  char buf[500];
  readers_ = new Reader **[num_players];
  for (int p = 0; p < num_players; ++p) {
    readers_[p] = new Reader *[max_street + 1];
    for (int st = 0; st <= max_street; ++st) {
      char suffix;
      if (prob_sizes_[st] == 1)      suffix = 'c';
      else if (prob_sizes_[st] == 2) suffix = 's';
      else if (prob_sizes_[st] == 4) suffix = 'i';
      else if (prob_sizes_[st] == 8) suffix = 'd';
      sprintf(buf, "%s/sumprobs.x.0.0.%i.%i.p%i.%c", dir, st, it, p, suffix);
      readers_[p][st] = new Reader(buf);
    }
  }
}

DiskProbs::~DiskProbs(void) {
  int num_players = Game::NumPlayers();
  int max_street = Game::MaxStreet();
  
  for (int p = 0; p < num_players; ++p) {
    for (int st = 0; st <= max_street; ++st) {
      delete readers_[p][st];
    }
    delete [] readers_[p];
  }
  delete [] readers_;
  
  for (int p = 0; p < num_players; ++p) {
    for (int st = 0; st <= max_street; ++st) {
      delete [] offsets_[p][st];
    }
    delete [] offsets_[p];
  }
  delete [] offsets_;
}

void DiskProbs::Probs(int p, int st, int nt, int b, int num_succs, double *probs) {
  if (num_succs == 0) {
    return;
  } else if (num_succs == 1) {
    probs[0] = 1.0;
    return;
  }
  
  int prob_size = prob_sizes_[st];
  long long int offset = offsets_[p][st][nt] + b * num_succs * prob_sizes_[st];
  Reader *reader = readers_[p][st];
  reader->SeekTo(offset);
  unique_ptr<double []> raw(new double[num_succs]);
  double sum = 0;
  for (int s = 0; s < num_succs; ++s) {
    double p;
    if (prob_size == 1) {
      unsigned char c = reader->ReadUnsignedCharOrDie();
      p = c;
    } else if (prob_size == 2) {
      unsigned short s = reader->ReadUnsignedShortOrDie();
      p = s;
    } else if (prob_size == 4) {
      int i = reader->ReadIntOrDie();
      p = i;
    } else if (prob_size == 8) {
      p = reader->ReadDoubleOrDie();
    }
    raw[s] = p;
    sum += p;
  }
  if (sum == 0) {
    probs[0] = 1.0;
    for (int s = 1; s < num_succs; ++s) probs[s] = 0.0;
    return;
  } else {
    for (int s = 0; s < num_succs; ++s) {
      probs[s] = raw[s] / sum;
    }
  }
}
