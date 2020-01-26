#include <stdio.h>
#include <stdlib.h>

#include <memory>
#include <string>

#include "betting_abstraction.h"
#include "betting_abstraction_params.h"
#include "betting_tree.h"
#include "buckets.h"
#include "card_abstraction.h"
#include "card_abstraction_params.h"
#include "cfr_config.h"
#include "cfr_params.h"
#include "constants.h"
#include "files.h"
#include "game.h"
#include "game_params.h"
#include "io.h"
#include "params.h"
#include "tcfr.h"

using std::string;
using std::unique_ptr;

static void Usage(const char *prog_name) {
  fprintf(stderr, "USAGE: %s <game params> <card params> <betting params> <CFR params> "
	  "<num threads> <start batch> <end batch> <batch size> <save interval> ([p0|p1])\n",
	  prog_name);
  exit(-1);
}

int main(int argc, char *argv[]) {
  if (argc != 10 && argc != 11) Usage(argv[0]);
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
  int num_threads, start_batch, end_batch, batch_size, save_interval;
  if (sscanf(argv[5], "%i", &num_threads) != 1)   Usage(argv[0]);
  if (sscanf(argv[6], "%i", &start_batch) != 1)   Usage(argv[0]);
  if (sscanf(argv[7], "%i", &end_batch) != 1)     Usage(argv[0]);
  if (sscanf(argv[8], "%i", &batch_size) != 1)    Usage(argv[0]);
  if (sscanf(argv[9], "%i", &save_interval) != 1) Usage(argv[0]);
  int target_p = -1;
  if (argc == 11) {
    if (! betting_abstraction->Asymmetric()) {
      fprintf(stderr, "Don't specify p1/p2 when using symmetric betting "
	      "abstraction\n");
      exit(-1);
    }
    string p_arg = argv[10];
    if (p_arg == "p0")      target_p = 0;
    else if (p_arg == "p1") target_p = 1;
    else                    Usage(argv[0]);
  }
  if (save_interval == 0) {
    fprintf(stderr, "save_interval must not be zero\n");
    exit(-1);
  }

  if (cfr_config->Algorithm() == "tcfr") {
    Buckets buckets(*card_abstraction, false);
    TCFR cfr(*card_abstraction, *betting_abstraction, *cfr_config, buckets, num_threads, target_p);
    cfr.Run(start_batch, end_batch, batch_size, save_interval);
  } else {
    fprintf(stderr, "Unknown algorithm: %s\n", cfr_config->Algorithm().c_str());
    exit(-1);
  }
}
