// Tree constructor requires card abstraction because nonterminal IDs are
// indexed by granularity, and the card abstraction gives the number of
// bucketings.

#include <stdio.h>
#include <stdlib.h>

#include <memory>
#include <string>

#include "betting_abstraction.h"
#include "betting_abstraction_params.h"
#include "betting_tree.h"
#include "card_abstraction.h"
#include "card_abstraction_params.h"
#include "files.h"
#include "game.h"
#include "game_params.h"
#include "params.h"

using std::string;
using std::unique_ptr;

static void Usage(const char *prog_name) {
  fprintf(stderr, "USAGE: %s <game params> <betting params> "
	  "([p0|p1])\n", prog_name);
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
  unique_ptr<BettingAbstraction>
    betting_abstraction(new BettingAbstraction(*betting_params));

  BettingTree *betting_tree = NULL;
  if (argc == 4) {
    string p_arg = argv[3];
    unsigned int p;
    if (p_arg == "p0")      p = 0;
    else if (p_arg == "p1") p = 1;
    else                    Usage(argv[0]);
    betting_tree = BettingTree::BuildAsymmetricTree(*betting_abstraction, p);
  } else {
    betting_tree = BettingTree::BuildTree(*betting_abstraction);
  }
  betting_tree->Display();
}
