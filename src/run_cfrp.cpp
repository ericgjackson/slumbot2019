#include <stdio.h>
#include <stdlib.h>

#include <memory>
#include <string>

#include "betting_abstraction.h"
#include "betting_abstraction_params.h"
#include "betting_trees.h"
#include "buckets.h"
#include "card_abstraction.h"
#include "card_abstraction_params.h"
#include "cfr_config.h"
#include "cfr_params.h"
#include "cfrp.h"
#include "constants.h"
#include "files.h"
#include "game.h"
#include "game_params.h"
#include "io.h"
#include "params.h"

using std::string;
using std::unique_ptr;

static void Usage(const char *prog_name) {
  fprintf(stderr, "USAGE: %s <game params> <card params> <betting params> "
	  "<CFR params> <num threads> <start it> <end it> ([p0|p1])\n",
	  prog_name);
  exit(-1);
}

int main(int argc, char *argv[]) {
  if (argc != 8 && argc != 9) Usage(argv[0]);
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
  unsigned int num_threads, start_it, end_it;
  if (sscanf(argv[5], "%u", &num_threads) != 1) Usage(argv[0]);
  if (sscanf(argv[6], "%u", &start_it) != 1)    Usage(argv[0]);
  if (sscanf(argv[7], "%u", &end_it) != 1)      Usage(argv[0]);
  unsigned int target_p = -1;
  if (argc == 9) {
    if (! betting_abstraction->Asymmetric()) {
      fprintf(stderr, "Don't specify p0/p1 when using symmetric betting abstraction\n");
      exit(-1);
    }
    string p_arg = argv[8];
    if (p_arg == "p0")      target_p = 0;
    else if (p_arg == "p1") target_p = 1;
    else                    Usage(argv[0]);
  } else {
    if (betting_abstraction->Asymmetric()) {
      fprintf(stderr, "Must specify p0/p1 when using asymmetric betting abstraction\n");
      exit(-1);
    }
  }
  Buckets buckets(*card_abstraction, false);

  if (cfr_config->Algorithm() == "cfrp") {
    CFRP cfr(*card_abstraction, *cfr_config, buckets, num_threads);
    cfr.Initialize(*betting_abstraction, target_p);
    cfr.Run(start_it, end_it);
  } else {
    fprintf(stderr, "Unknown algorithm: %s\n",
	    cfr_config->Algorithm().c_str());
    exit(-1);
  }
}
