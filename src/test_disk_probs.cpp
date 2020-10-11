#include <stdio.h>
#include <stdlib.h>

#include <memory>

#include "betting_abstraction.h"
#include "betting_abstraction_params.h"
#include "betting_trees.h"
#include "buckets.h"
#include "card_abstraction.h"
#include "card_abstraction_params.h"
#include "cfr_config.h"
#include "cfr_params.h"
#include "cfr_values.h"
#include "disk_probs.h"
#include "files.h"
#include "game.h"
#include "game_params.h"
#include "io.h"
#include "params.h"
#include "split.h"

using std::unique_ptr;

static void Verify(Node *node, DiskProbs *disk_probs, CFRValues *sumprobs, const Buckets &buckets) {
  if (node->Terminal()) return;
  int num_succs = node->NumSuccs();
  if (num_succs > 1) {
    int st = node->Street();
    int nt = node->NonterminalID();
    int num_buckets = buckets.NumBuckets(st);
    int pa = node->PlayerActing();
    unique_ptr<double []> probs1(new double[num_succs]);
    unique_ptr<double []> probs2(new double[num_succs]);
    int dsi = 0;
    for (int b = 0; b < num_buckets; ++b) {
      int offset = b * num_succs;
      sumprobs->RMProbs(st, pa, nt, offset, num_succs, dsi, probs1.get());
      disk_probs->Probs(pa, st, nt, b, num_succs, probs2.get());
      for (int s = 0; s < num_succs; ++s) {
	double hi, lo;
	if (probs1[s] > probs2[s]) {hi = probs1[s]; lo = probs2[s];}
	else                       {hi = probs2[s]; lo = probs1[s];}
	// double ratio = lo ? hi / lo : 1.0;
	double diff = hi - lo;
	if (diff >= 0.004) {
	  fprintf(stderr, "st %i nt %i pa %i b %i s %i: %f %f\n", st, nt, pa, b, s, probs1[s],
		  probs2[s]);
	  exit(-1);
	}
#if 0
	if (probs1[s] != probs2[s]) {
	  fprintf(stderr, "st %i nt %i pa %i b %i s %i: %f %f\n", st, nt, pa, b, s, probs1[s],
		  probs2[s]);
	  exit(-1);
	}
#endif
      }
    }
  }
  for (int s = 0; s < num_succs; ++s) {
    Verify(node->IthSucc(s), disk_probs, sumprobs, buckets);
  }
}

static void Usage(const char *prog_name) {
  fprintf(stderr, "USAGE: %s <game params> <card params> <betting params> "
	  "<CFR params> <it>\n", prog_name);
  exit(-1);
}

int main(int argc, char *argv[]) {
  if (argc != 6) Usage(argv[0]);
  Files::Init();
  unique_ptr<Params> game_params = CreateGameParams();
  game_params->ReadFromFile(argv[1]);
  Game::Initialize(*game_params);
  unique_ptr<Params> card_params = CreateCardAbstractionParams();
  card_params->ReadFromFile(argv[2]);
  unique_ptr<CardAbstraction>
    card_abstraction(new CardAbstraction(*card_params));
  unique_ptr<Params> betting_params = CreateBettingAbstractionParams();
  betting_params->ReadFromFile(argv[3]);
  unique_ptr<BettingAbstraction>
    betting_abstraction(new BettingAbstraction(*betting_params));
  unique_ptr<Params> cfr_params = CreateCFRParams();
  cfr_params->ReadFromFile(argv[4]);
  unique_ptr<CFRConfig> cfr_config(new CFRConfig(*cfr_params));
  int it;
  if (sscanf(argv[5], "%i", &it) != 1) Usage(argv[0]);

  Buckets buckets(*card_abstraction, true);
  BettingTrees betting_trees(*betting_abstraction);
  const BettingTree *betting_tree = betting_trees.GetBettingTree();
  unique_ptr<DiskProbs> disk_probs(new DiskProbs(*card_abstraction, *betting_abstraction,
						 *cfr_config, buckets, betting_tree, it));

  char dir[500];
  sprintf(dir, "%s/%s.%u.%s.%i.%i.%i.%s.%s", Files::OldCFRBase(),
	  Game::GameName().c_str(), Game::NumPlayers(),
	  card_abstraction->CardAbstractionName().c_str(), Game::NumRanks(),
	  Game::NumSuits(), Game::MaxStreet(),
	  betting_abstraction->BettingAbstractionName().c_str(),
	  cfr_config->CFRConfigName().c_str());
  unique_ptr<CFRValues> sumprobs;
  sumprobs.reset(new CFRValues(nullptr, nullptr, 0, 0, buckets, betting_tree));
  bool quantize = false;
  sumprobs->Read(dir, it, betting_tree, "x", -1, true, quantize);
  Verify(betting_tree->Root(), disk_probs.get(), sumprobs.get(), buckets);
}
