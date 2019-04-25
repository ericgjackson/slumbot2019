#include <stdio.h>
#include <stdlib.h>

#include <memory>

#include "board_tree.h"
#include "files.h"
#include "game.h"
#include "game_params.h"
#include "io.h"
#include "params.h"

using std::unique_ptr;

static void Usage(const char *prog_name) {
  fprintf(stderr, "USAGE: %s <game params>\n", prog_name);
  exit(-1);
}

int main(int argc, char *argv[]) {
  if (argc != 2) Usage(argv[0]);
  Files::Init();
  unique_ptr<Params> game_params = CreateGameParams();
  game_params->ReadFromFile(argv[1]);
  Game::Initialize(*game_params);

  BoardTree::Create();
  int max_street = Game::MaxStreet();
  for (int st = 0; st <= max_street; ++st) {
    printf("St %i: %i boards\n", st, BoardTree::NumBoards(st));
  }
}
