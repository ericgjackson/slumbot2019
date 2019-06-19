#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <memory>
#include <string>
#include <unordered_set>

#include "betting_abstraction.h"
#include "betting_abstraction_params.h"
#include "betting_tree.h"
#include "board_tree.h"
#include "buckets.h"
#include "canonical_cards.h"
#include "card_abstraction.h"
#include "card_abstraction_params.h"
#include "cards.h"
#include "cfr_config.h"
#include "cfr_params.h"
#include "cfr_values.h"
#include "constants.h"
#include "files.h"
#include "game.h"
#include "game_params.h"
#include "hand_tree.h"
#include "hand_value_tree.h"
#include "io.h"
#include "params.h"
#include "reach_probs.h"

using std::string;
using std::shared_ptr;
using std::unique_ptr;
using std::unordered_set;

void Show(Node *node, const string &action_sequence, const Buckets &buckets,
	  const CFRValues &sumprobs, const HandTree &hand_tree, const ReachProbs &reach_probs) {
  int num_succs = node->NumSuccs();
  int max_card1 = Game::MaxCard() + 1;
  const CanonicalCards *hands = hand_tree.Hands(0, 0);
  int num_hole_card_pairs = Game::NumHoleCardPairs(0);
  for (int p = 0; p < 2; ++p) {
    for (int i = 0; i < num_hole_card_pairs; ++i) {
      const Card *cards = hands->Cards(i);
      Card hi = cards[0];
      Card lo = cards[1];
      int enc = hi * max_card1 + lo;
      printf("%s p %i ", action_sequence.c_str(), p);
      OutputTwoCards(cards);
      printf(" %f\n", reach_probs.Get(p, enc));
    }
  }	
  if (node->Street() == 1) return;
  if (node->Terminal()) return;
  shared_ptr<ReachProbs []> succ_reach_probs =
    ReachProbs::CreateSuccReachProbs(node, 0, 0, hands, buckets, &sumprobs, reach_probs, false);
  for (int s = 0; s < num_succs; ++s) {
    string action = node->ActionName(s);
    Show(node->IthSucc(s), action_sequence + action, buckets, sumprobs, hand_tree,
	 succ_reach_probs[s]);
  }
}

static void Usage(const char *prog_name) {
  fprintf(stderr, "USAGE: %s <game params> <card params> <betting params> "
	  "<CFR params> <it>\n", prog_name);
  exit(-1);
}

int main(int argc, char *argv[]) {
  if (argc != 6) Usage(argv[0]);
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
  unique_ptr<CFRConfig>
    cfr_config(new CFRConfig(*cfr_params));
  int it;
  if (sscanf(argv[5], "%i", &it) != 1) Usage(argv[0]);

  // Excessive to load all buckets.  Only need buckets for the preflop.
  Buckets buckets(*card_abstraction, false);
  unique_ptr<BettingTree> betting_tree;
  betting_tree.reset(new BettingTree(*betting_abstraction));
  
  int max_street = Game::MaxStreet();
  unique_ptr<bool []> streets(new bool[max_street + 1]);
  for (int st = 0; st <= max_street; ++st) {
    streets[st] = (st == 0);
  }
  CFRValues sumprobs(nullptr, streets.get(), 0, 0, buckets, betting_tree.get());
  char dir[500];
  sprintf(dir, "%s/%s.%u.%s.%u.%u.%u.%s.%s", Files::OldCFRBase(), Game::GameName().c_str(),
	  Game::NumPlayers(), card_abstraction->CardAbstractionName().c_str(),
	  Game::NumRanks(), Game::NumSuits(), Game::MaxStreet(),
	  betting_abstraction->BettingAbstractionName().c_str(),
	  cfr_config->CFRConfigName().c_str());
  sumprobs.Read(dir, it, betting_tree.get(), "x", -1, true, false);
  unique_ptr<ReachProbs> reach_probs(ReachProbs::CreateRoot());
  HandTree hand_tree(0, 0, 0);
  Show(betting_tree->Root(), "x", buckets, sumprobs, hand_tree, *reach_probs);
}
