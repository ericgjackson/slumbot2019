#include <stdio.h>
#include <stdlib.h>

#include "buckets.h"
#include "card_abstraction.h"
#include "card_abstraction_params.h"
#include "files.h"
#include "game.h"
#include "game_params.h"
#include "io.h"
#include "params.h"

using std::unique_ptr;

static void Usage(const char *prog_name) {
  fprintf(stderr, "USAGE: %s <game params> <card params>\n", prog_name);
  exit(-1);
}

int main(int argc, char *argv[]) {
  if (argc != 3) Usage(argv[0]);
  Files::Init();
  unique_ptr<Params> game_params = CreateGameParams();
  game_params->ReadFromFile(argv[1]);
  Game::Initialize(*game_params);
  unique_ptr<Params> card_params = CreateCardAbstractionParams();
  card_params->ReadFromFile(argv[2]);
  unique_ptr<CardAbstraction>
    card_abstraction(new CardAbstraction(*card_params));
  Buckets buckets(*card_abstraction, true);
  int max_street = Game::MaxStreet();
  for (int st = 0; st <= max_street; ++st) {
    fprintf(stderr, "%i: %i\n", st, buckets.NumBuckets(st));
  }
}
