// Compute a pseudo-best-response using Monte-Carlo Tree Search.

#include <stdio.h>
#include <stdlib.h>

#include <memory>
#include <string>
#include <vector>

#include "betting_abstraction.h"
#include "betting_abstraction_params.h"
#include "betting_tree.h"
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
	  "<CFR params> <num threads> <it> [current|avg] (<streets>)\n",
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
  unsigned int num_threads, it;
  if (sscanf(argv[5], "%u", &num_threads) != 1) Usage(argv[0]);
  if (sscanf(argv[6], "%u", &it) != 1)          Usage(argv[0]);
  string carg = argv[7];
  bool current;
  if (carg == "current")  current = true;
  else if (carg == "avg") current = false;
  else                    Usage(argv[0]);
  unsigned int max_street = Game::MaxStreet();
  unique_ptr<bool []> streets(new bool[max_street + 1]);
  if (argc == 9) {
    for (unsigned int st = 0; st <= max_street; ++st) {
      streets[st] = false;
    }
    vector<string> comps;
    Split(argv[8], ',', false, &comps);
    unsigned int num = comps.size();
    for (unsigned int i = 0; i < num; ++i) {
      unsigned int st;
      if (sscanf(comps[i].c_str(), "%u", &st) != 1) Usage(argv[0]);
      if (st > max_street) Usage(argv[0]);
      streets[st] = true;
    }
  } else {
    for (unsigned int st = 0; st <= max_street; ++st) {
      streets[st] = true;
    }
  }
  Buckets buckets(*card_abstraction, false);
}
