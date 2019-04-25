#include <stdio.h>
#include <stdlib.h>

#include <string>

#include "board_tree.h"
#include "cards.h"
#include "files.h"
#include "game.h"
#include "game_params.h"
#include "io.h"
#include "params.h"

using namespace std;

static void Show(unsigned int street) {
  unsigned int num_board_cards = Game::NumBoardCards(street);
  unsigned int num_boards = BoardTree::NumBoards(street);

  for (unsigned int bd = 0; bd < num_boards; ++bd) {
    const Card *board = BoardTree::Board(street, bd);
    printf("%u ", bd);
    OutputNCards(board, num_board_cards);
    printf("\n");
  }
}

static void Usage(const char *prog_name) {
  fprintf(stderr, "USAGE: %s <game params> <street>\n",
	  prog_name);
  exit(-1);
}

int main(int argc, char *argv[]) {
  if (argc != 3) Usage(argv[0]);
  Files::Init();
  unique_ptr<Params> game_params = CreateGameParams();
  game_params->ReadFromFile(argv[1]);
  Game::Initialize(*game_params);
  unsigned int street;
  if (sscanf(argv[2], "%u", &street) != 1) Usage(argv[0]);
  BoardTree::Create();
  Show(street);
}
