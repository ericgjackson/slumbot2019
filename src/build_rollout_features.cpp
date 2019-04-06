#include <stdio.h>
#include <stdlib.h>

#include "board_tree.h"
#include "constants.h"
#include "files.h"
#include "game.h"
#include "game_params.h"
#include "hand_value_tree.h"
#include "io.h"
#include "params.h"
#include "rollout.h"

using namespace std;

static void Usage(const char *prog_name) {
  fprintf(stderr, "USAGE: %s <game params> <street> <features name> "
	  "<squashing> [wins|wmls] <pct 0> <pct 1>... <pct n>\n", prog_name);
  fprintf(stderr, "\nSquashing of 1.0 means no squashing\n");
  exit(-1);
}

int main(int argc, char *argv[]) {
  if (argc < 7) Usage(argv[0]);
  Files::Init();
  unique_ptr<Params> game_params = CreateGameParams();
  game_params->ReadFromFile(argv[1]);
  Game::Initialize(*game_params);
  int street;
  if (sscanf(argv[2], "%i", &street) != 1) Usage(argv[0]);
  string features_name = argv[3];
  double squashing;
  if (sscanf(argv[4], "%lf", &squashing) != 1) Usage(argv[0]);
  bool wins;
  string warg = argv[5];
  if (warg == "wins")      wins = true;
  else if (warg == "wmls") wins = false;
  else                     Usage(argv[0]);
  
  int num_percentiles = argc - 6;
  double *percentiles = new double[num_percentiles];
  for (int i = 0; i < num_percentiles; ++i) {
    if (sscanf(argv[6 + i], "%lf", &percentiles[i]) != 1) Usage(argv[0]);
  }

  HandValueTree::Create();
  // Need this for ComputeRollout()
  BoardTree::Create();
  short *pct_vals = ComputeRollout(street, percentiles, num_percentiles, squashing, wins);

  unsigned int num_boards = BoardTree::NumBoards(street);
  unsigned int num_hole_card_pairs = Game::NumHoleCardPairs(street);
  unsigned int num_hands = num_boards * num_hole_card_pairs;
  fprintf(stderr, "%u hands\n", num_hands);

  char buf[500];
  sprintf(buf, "%s/features.%s.%u.%s.%u", Files::StaticBase(), Game::GameName().c_str(),
	  Game::NumRanks(), features_name.c_str(), street);
  Writer writer(buf);
  writer.WriteInt(num_percentiles);

  for (unsigned int h = 0; h < num_hands; ++h) {
    for (int p = 0; p < num_percentiles; ++p) {
      writer.WriteShort(pct_vals[h * num_percentiles + p]);
    }
  }
}
