#include <stdio.h>
#include <stdlib.h>

#include <memory>
#include <vector>

#include "backup_tree.h"
#include "betting_abstraction.h"
#include "betting_abstraction_params.h"
#include "betting_tree.h"
#include "betting_trees.h"
#include "files.h"
#include "game.h"
#include "game_params.h"
#include "params.h"

using std::unique_ptr;
using std::vector;

#if 0
static void Walk(Node *node, vector<Node *> *path, BackupBuilder *builder, int st) {
  int num_succs = node->NumSuccs();
  if (node->Street() >= st) {
    path->push_back(node);
    BettingTree subtree(builder->Build(*path, st).get());
    fprintf(stderr, "xxx1\n");
    subtree.Display();
    fprintf(stderr, "xxx2\n");
    if (node->PlayerActing() == 1) exit(-1);
  }
  for (int s = 0; s < num_succs; ++s) {
    Walk(node->IthSucc(s), path, builder, st);
  }
  if (node->Street() >= st) {
    path->pop_back();
  }
}
#endif

static void Usage(const char *prog_name) {
  fprintf(stderr, "USAGE: %s <game params> <betting abstraction> <st>\n", prog_name);
  exit(-1);
}

int main(int argc, char *argv[]) {
  if (argc != 4) Usage(argv[0]);
  Files::Init();
  unique_ptr<Params> game_params = CreateGameParams();
  game_params->ReadFromFile(argv[1]);
  Game::Initialize(*game_params);
  unique_ptr<Params> betting_params = CreateBettingAbstractionParams();
  betting_params->ReadFromFile(argv[2]);
  unique_ptr<BettingAbstraction> ba(new BettingAbstraction(*betting_params));
  int st;
  if (sscanf(argv[3], "%i", &st) != 1) Usage(argv[0]);

  BettingTree betting_tree(*ba);
  
  BackupBuilder builder(ba->StackSize());
#if 0
  vector<Node *> path;
  Walk(betting_tree.Root(), &path, &builder, st);
#endif
  ObservedBets observed_bets;
  // P1 bet on turn after P0 check
  observed_bets.AddObservedBet(2, 1, 0, 0, 18);
  // P0 opening bet on river
  observed_bets.AddObservedBet(3, 0, 0, 1, 36);
  unique_ptr<BettingTrees> subtrees(builder.BuildTrees(observed_bets, 2, 18));
  subtrees->GetBettingTree()->Display();
}
