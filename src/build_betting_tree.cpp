#include <stdio.h>
#include <stdlib.h>

#include <memory>
#include <string>

#include "betting_abstraction.h"
#include "betting_abstraction_params.h"
#include "betting_tree_builder.h"
#include "files.h"
#include "game.h"
#include "game_params.h"
#include "io.h"
#include "params.h"

using std::string;
using std::unique_ptr;

static void Usage(const char *prog_name) {
  fprintf(stderr, "USAGE: %s <game params> <betting params> ([p0|p1])\n",
	  prog_name);
  exit(-1);
}

int main(int argc, char *argv[]) {
  if (argc != 3 && argc != 4) Usage(argv[0]);
  Files::Init();
  unique_ptr<Params> game_params = CreateGameParams();
  game_params->ReadFromFile(argv[1]);
  Game::Initialize(*game_params);
  unique_ptr<Params> betting_params = CreateBettingAbstractionParams();
  betting_params->ReadFromFile(argv[2]);
  unique_ptr<BettingAbstraction> ba(new BettingAbstraction(*betting_params));

  BettingTreeBuilder *builder = NULL;
  if (argc == 4) {
    if (! ba->Asymmetric()) {
      fprintf(stderr, "With symmetric betting abstractions, no player argument allowed\n");
      exit(-1);
    }
    int p;
    string p_arg = argv[3];
    if (p_arg == "p0")      p = 0;
    else if (p_arg == "p1") p = 1;
    else                    Usage(argv[0]);
    builder = new BettingTreeBuilder(*ba, p);
  } else {
    if (ba->Asymmetric()) {
      fprintf(stderr, "With asymmetric betting abstractions, player argument required\n");
      exit(-1);
    }
    builder = new BettingTreeBuilder(*ba);
  }
  builder->Build();
  builder->Write();
  delete builder;
}
