// Max smallest mdf_diff: 0.190476
// Max allin mdf_diff: 0.311688
// Max other mdf_diff: 0.166667

// allin st 1 pa 1 nt 1960 s 2/3 mdf 0.545455 lmdf 0.857143 lbt 75 bs 125
//              "CBBBC CB" (id 1960 lbt 50 ns 6 st 1 p0c)
//                "CBBBC CBB" (id 1960 lbt 75 ns 3 st 1 p1c)
//                   "CBBBC CBBB" (id 1961 lbt 200 ns 2 st 1 p0c)
// Ordinarily we would allow a 1/2 pot bet of 75, but this is considered to close too all-in
// and gets pruned.  If I allowed min-raises then I would allow a raise with a bet-to of 100.
// Could also allow a 1/3 pot third bet.

// smallest st 0 pa 1 nt 294 s 2/6 mdf 0.666667 lmdf 0.857143 lbt 6 bs 6
//     "BB" (id 294 lbt 6 ns 6 st 0 p1c)
//       "BBB" (id 294 lbt 12 ns 6 st 0 p0c)

// other st 0 pa 1 nt 0 s 7/9 mdf 0.166667 lmdf 0.333333 lbt 2 bs 20
//   Gap between 2x and 5x
// other st 1 pa 0 nt 15 s 3/6 mdf 0.500000 lmdf 0.666667 lbt 18 bs 36
//            "CC CBBB" (id 15 lbt 18 ns 6 st 1 p0c)
//              "CC CBBBB" (id 15 lbt 36 ns 5 st 1 p1c)
//              "CC CBBBB" (id 19 lbt 54 ns 4 st 1 p1c)
//              "CC CBBBB" (id 21 lbt 90 ns 3 st 1 p1c)
//              "CC CBBBB" (id 22 lbt 200 ns 2 st 1 p1c)
//   Gap between 0.5x and 1x


#include <stdio.h>
#include <stdlib.h>

#include <memory>
#include <vector>

#include "backup_tree.h"
#include "betting_abstraction.h"
#include "betting_abstraction_params.h"
#include "betting_tree.h"
#include "files.h"
#include "game.h"
#include "game_params.h"
#include "params.h"

using std::unique_ptr;
using std::vector;

static void Walk(Node *node, int stack_size, int last_bet_size, double *max_smallest_mdf_diff,
		 double *max_allin_mdf_diff, double *max_other_mdf_diff) {
  // Only want to do for flop and preflop
  if (node->Street() >= 2) return;
  int num_succs = node->NumSuccs();
  int pot_size = 2 * node->LastBetTo();
  int min_bet = std::max(2, last_bet_size);
  // double min_frac = min_bet / (double)pot_size;
  // double last_pot_frac = min_frac;
  double min_mdf = pot_size / (double)(pot_size + min_bet);
  double last_mdf = min_mdf;
  for (int s = 0; s < num_succs; ++s) {
    if (s == node->CallSuccIndex()) continue;
    if (s == node->FoldSuccIndex()) continue;
    int next_bet_to = node->IthSucc(s)->LastBetTo();
    // Skip all-ins?  Not obviously correct
    // if (next_bet_to == stack_size) continue;
    int bet_size = next_bet_to - node->LastBetTo();
    // double pot_frac = bet_size / (double)pot_size;
    double mdf = pot_size / (double)(pot_size + bet_size);
    // double ratio = pot_frac / last_pot_frac;
    double mdf_diff = std::abs(mdf - last_mdf);
    if (next_bet_to == stack_size) {
      if (mdf_diff > *max_allin_mdf_diff) *max_allin_mdf_diff = mdf_diff;
      if (mdf_diff >= 0.3116) {
	fprintf(stderr, "allin st %i pa %i nt %i s %i/%i mdf %f lmdf %f lbt %i bs %i\n",
		node->Street(), node->PlayerActing(), node->NonterminalID(), s, num_succs,
		mdf, last_mdf, node->LastBetTo(), bet_size);
      }
    } else if (last_mdf == min_mdf) {
      if (mdf_diff > *max_smallest_mdf_diff) *max_smallest_mdf_diff = mdf_diff;
      if (mdf_diff >= 0.19) {
	fprintf(stderr, "smallest st %i pa %i nt %i s %i/%i mdf %f lmdf %f lbt %i bs %i\n",
		node->Street(), node->PlayerActing(), node->NonterminalID(), s, num_succs,
		mdf, last_mdf, node->LastBetTo(), bet_size);
      }
    } else {
      if (mdf_diff > *max_other_mdf_diff) *max_other_mdf_diff = mdf_diff;
      if (mdf_diff >= 0.1666) {
	fprintf(stderr, "other st %i pa %i nt %i s %i/%i mdf %f lmdf %f lbt %i bs %i\n",
		node->Street(), node->PlayerActing(), node->NonterminalID(), s, num_succs,
		mdf, last_mdf, node->LastBetTo(), bet_size);
      }
    }
    last_mdf = mdf;
  }
  for (int s = 0; s < num_succs; ++s) {
    Node *succ = node->IthSucc(s);
    int succ_last_bet_size;
    if (s == node->CallSuccIndex() || s == node->FoldSuccIndex()) {
      succ_last_bet_size = 0;
    } else {
      succ_last_bet_size = succ->LastBetTo() - node->LastBetTo();
    }
    Walk(succ, stack_size, succ_last_bet_size, max_smallest_mdf_diff, max_allin_mdf_diff,
	 max_other_mdf_diff);
  }
}

static void Usage(const char *prog_name) {
  fprintf(stderr, "USAGE: %s <game params> <betting abstraction>\n", prog_name);
  exit(-1);
}

int main(int argc, char *argv[]) {
  if (argc != 3) Usage(argv[0]);
  Files::Init();
  unique_ptr<Params> game_params = CreateGameParams();
  game_params->ReadFromFile(argv[1]);
  Game::Initialize(*game_params);
  unique_ptr<Params> betting_params = CreateBettingAbstractionParams();
  betting_params->ReadFromFile(argv[2]);
  unique_ptr<BettingAbstraction> ba(new BettingAbstraction(*betting_params));

  BettingTree betting_tree(*ba);

  double max_smallest_mdf_diff = 0, max_allin_mdf_diff = 0, max_other_mdf_diff = 0;
  Walk(betting_tree.Root(), ba->StackSize(), 0, &max_smallest_mdf_diff, &max_allin_mdf_diff,
       &max_other_mdf_diff);
  printf("Max smallest mdf_diff: %f\n", max_smallest_mdf_diff);
  printf("Max allin mdf_diff: %f\n", max_allin_mdf_diff);
  printf("Max other mdf_diff: %f\n", max_other_mdf_diff);
}
