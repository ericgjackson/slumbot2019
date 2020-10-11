#include <stdio.h>
#include <stdlib.h>

#include "acpc_server.h"
#include "agent.h"
#include "betting_abstraction.h"
#include "betting_abstraction_params.h"
#include "betting_trees.h"
#include "board_tree.h"
#include "buckets.h"
#include "card_abstraction.h"
#include "card_abstraction_params.h"
#include "cards.h"
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

static void Usage(const char *prog_name) {
  fprintf(stderr, "USAGE: %s <game params> <card params> <betting params> "
	  "<CFR params> <it> <seed> <port>\n", prog_name);
  exit(-1);
}

int main(int argc, char *argv[]) {
  if (argc != 8) Usage(argv[0]);
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
  int it, seed, port;
  if (sscanf(argv[5], "%i", &it) != 1)   Usage(argv[0]);
  if (sscanf(argv[6], "%i", &seed) != 1) Usage(argv[0]);
  if (sscanf(argv[7], "%i", &port) != 1) Usage(argv[0]);

  BoardTree::Create();
  BoardTree::CreateLookup();

  int big_blind = 100;

  Agent agent(*card_abstraction, *betting_abstraction, *cfr_config, it, big_blind, seed);
  ACPCServer server(1, port, agent);
  server.MainLoop();
}
