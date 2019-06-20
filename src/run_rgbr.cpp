#include <stdio.h>
#include <stdlib.h>

#include <memory>
#include <string>
#include <vector>

#include "betting_abstraction.h"
#include "betting_abstraction_params.h"
#include "betting_trees.h"
#include "buckets.h"
#include "card_abstraction.h"
#include "card_abstraction_params.h"
#include "cfr_config.h"
#include "cfr_params.h"
#include "files.h"
#include "game.h"
#include "game_params.h"
#include "io.h"
#include "params.h"
#include "rgbr.h"
#include "split.h"

using std::string;
using std::unique_ptr;
using std::vector;

static void Usage(const char *prog_name) {
  fprintf(stderr, "USAGE: %s <game params> <card params> <betting params> "
	  "<CFR params> <num threads> <it> [current|avg] [quantize|raw] (<streets>)\n",
	  prog_name);
  exit(-1);
}

int main(int argc, char *argv[]) {
  if (argc != 9 && argc != 10) Usage(argv[0]);
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
  int num_threads, it;
  if (sscanf(argv[5], "%i", &num_threads) != 1) Usage(argv[0]);
  if (sscanf(argv[6], "%i", &it) != 1)          Usage(argv[0]);
  string carg = argv[7];
  bool current;
  if (carg == "current")  current = true;
  else if (carg == "avg") current = false;
  else                    Usage(argv[0]);
  string qarg = argv[8];
  bool quantize;
  if (qarg == "quantize") quantize = true;
  else if (qarg == "raw") quantize = false;
  else                    Usage(argv[0]);
  int max_street = Game::MaxStreet();
  unique_ptr<bool []> streets(new bool[max_street + 1]);
  if (argc == 10) {
    for (int st = 0; st <= max_street; ++st) streets[st] = false;
    vector<string> comps;
    Split(argv[9], ',', false, &comps);
    int num = comps.size();
    for (int i = 0; i < num; ++i) {
      int st;
      if (sscanf(comps[i].c_str(), "%i", &st) != 1) Usage(argv[0]);
      if (st > max_street) Usage(argv[0]);
      streets[st] = true;
    }
  } else {
    for (int st = 0; st <= max_street; ++st) {
      streets[st] = true;
    }
  }
  Buckets buckets(*card_abstraction, false);

  int num_players = Game::NumPlayers();
  unique_ptr<double []> evs(new double[num_players]);
  for (int p = 0; p < num_players; ++p) evs[p] = 0;

  RGBR rgbr(*card_abstraction, *cfr_config, buckets, current, quantize, num_threads,
	    streets.get());
  if (betting_abstraction->Asymmetric()) {
    if (num_players > 2) {
      fprintf(stderr, "How to handle more than two players?!?\n");
      exit(-1);
    }
    for (int target_p = 0; target_p < num_players; ++target_p) {
      int responder_p = target_p^1;
      evs[responder_p] = rgbr.Go(it, responder_p, *betting_abstraction);
    }
  } else {
    for (int p = 0; p < num_players; ++p) {
      evs[p] = rgbr.Go(it, p, *betting_abstraction);
    }
  }

  double gap = 0;
  for (int p = 0; p < num_players; ++p) {
    // Divide by two to convert chips into big blinds (assumption is that the
    // small blind is one chip).  Multiply by 1000 to convert big blinds into
    // milli-big-blinds.
    printf("P%u best response: %f (%.2f mbb/g)\n", p, evs[p],
	   (evs[p] / 2.0) * 1000.0);
    gap += evs[p];
  }
  printf("Gap: %f\n", gap);
  printf("Exploitability: %.2f mbb/g\n", ((gap / 2.0) / num_players) * 1000.0);
  fflush(stdout);
}
