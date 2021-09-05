#include <stdio.h>
#include <stdlib.h>

#include <string>

#include "betting_abstraction.h"
#include "betting_abstraction_params.h"
#include "betting_trees.h"
#include "buckets.h"
#include "card_abstraction.h"
#include "card_abstraction_params.h"
#include "cfr_config.h"
#include "cfr_params.h"
#include "constants.h"
#include "files.h"
#include "game.h"
#include "game_params.h"
#include "io.h"
#include "params.h"
#include "tcfr.h"

using std::string;

static void Walk(Node *node, const Buckets &buckets, const CFRConfig &cfr_config,
		 long long int *num_bytes, long long int *num_sumprob_bytes,
		 long long int *num_sumprob_flop_bytes,
		 long long int *num_sumprob_turn_bytes,
		 long long int *num_sumprob_river_bytes,
		 long long int *num_preflop_bytes,
		 long long int *num_flop_bytes,
		 bool ***seen) {
  if (node->Terminal()) return;
  int st = node->Street();
  int pa = node->PlayerActing();
  int nt = node->NonterminalID();
  if (seen[st][pa][nt]) return;
  seen[st][pa][nt] = true;
  int num_succs = node->NumSuccs();
  if (num_succs > 1) {
    int nb = buckets.NumBuckets(st);
    // regret bytes
    int rb;
    if (cfr_config.CharQuantizedStreet(st)) {
      rb = 1;
    } else if (cfr_config.ShortQuantizedStreet(st)) {
      rb = 2;
    } else {
      rb = 4;
    }
    // Assumes we are maintaining sumprobs on all streets
    // sumprobs are always 4 bytes
    long long int bytes = nb * (rb + 4) * num_succs;
    long long int sumprob_bytes = nb * 4 * num_succs;
    *num_sumprob_bytes += sumprob_bytes;
    *num_bytes += bytes;
    if (st == 1)      *num_sumprob_flop_bytes += sumprob_bytes;
    else if (st == 2) *num_sumprob_turn_bytes += sumprob_bytes;
    else if (st == 3) *num_sumprob_river_bytes += sumprob_bytes;
    if (st == 0)      *num_preflop_bytes += bytes;
    else if (st == 1) *num_flop_bytes += bytes;
  }
  for (int s = 0; s < num_succs; ++s) {
    Walk(node->IthSucc(s), buckets, cfr_config, num_bytes, num_sumprob_bytes, num_sumprob_flop_bytes,
	 num_sumprob_turn_bytes, num_sumprob_river_bytes, num_preflop_bytes, num_flop_bytes,
	 seen);
  }
}

static void Usage(const char *prog_name) {
  fprintf(stderr, "USAGE: %s <game params> <card params> <betting params> <cfr params>\n",
	  prog_name);
  exit(-1);
}

int main(int argc, char *argv[]) {
  if (argc != 5) Usage(argv[0]);
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
  Buckets buckets(*card_abstraction, true);
  BettingTrees betting_trees(*betting_abstraction);

  long long int num_bytes = 0LL, num_sumprob_bytes = 0LL;
  long long int num_sumprob_flop_bytes = 0LL;
  long long int num_sumprob_turn_bytes = 0LL;
  long long int num_sumprob_river_bytes = 0LL;
  long long int num_preflop_bytes = 0LL, num_flop_bytes = 0LL;

  int max_street = Game::MaxStreet();
  int num_players = Game::NumPlayers();
  bool ***seen = new bool **[max_street + 1];
  for (int st = 0; st <= max_street; ++st) {
    seen[st] = new bool *[num_players];
    for (int p = 0; p < num_players; ++p) {
      const BettingTree *betting_tree = betting_trees.GetBettingTree(p);
      int num_nt = betting_tree->NumNonterminals(p, st);
      seen[st][p] = new bool[num_nt];
      for (int i = 0; i < num_nt; ++i) {
	seen[st][p][i] = false;
      }
    }
  }

  Walk(betting_trees.Root(), buckets, *cfr_config, &num_bytes, &num_sumprob_bytes,
       &num_sumprob_flop_bytes, &num_sumprob_turn_bytes, &num_sumprob_river_bytes,
       &num_preflop_bytes, &num_flop_bytes, seen);
  printf("%lli bytes\n", num_bytes);
  printf("%lli sumprob bytes\n", num_sumprob_bytes);
  printf("%lli sumprob flop bytes\n", num_sumprob_flop_bytes);
  printf("%lli sumprob turn bytes\n", num_sumprob_turn_bytes);
  printf("%lli sumprob river bytes\n", num_sumprob_river_bytes);
  printf("%lli preflop bytes\n", num_preflop_bytes);
  printf("%lli flop bytes\n", num_flop_bytes);
}
