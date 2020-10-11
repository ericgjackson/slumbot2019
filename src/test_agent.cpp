#include <stdio.h>
#include <stdlib.h>

#include <memory>
#include <string>
#include <vector>

#include "agent.h"
#include "betting_abstraction.h"
#include "betting_abstraction_params.h"
#include "betting_trees.h"
#include "board_tree.h"
#include "buckets.h"
#include "card_abstraction.h"
#include "card_abstraction_params.h"
#include "cards.h"
#include "cfr_config.h"
#include "cfr_params.h"
#include "cfr_values.h"
#include "disk_probs.h"
#include "files.h"
#include "game.h"
#include "game_params.h"
#include "io.h"
#include "params.h"
#include "split.h"

using std::string;
using std::unique_ptr;
using std::vector;

static void Usage(const char *prog_name) {
  fprintf(stderr, "USAGE: %s <game params> <card params> <betting params> "
	  "<CFR params> <it> <seed>\n", prog_name);
  exit(-1);
}

int main(int argc, char *argv[]) {
  if (argc != 7) Usage(argv[0]);
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
  int it, seed;
  if (sscanf(argv[5], "%i", &it) != 1)   Usage(argv[0]);
  if (sscanf(argv[6], "%i", &seed) != 1) Usage(argv[0]);

  BoardTree::Create();
  BoardTree::CreateLookup();

  int big_blind = 100;
  Agent agent(*card_abstraction, *betting_abstraction, *cfr_config, it, big_blind, seed);
  string action = "";
  unique_ptr<Card []> board(new Card[5]);
  unique_ptr<Card []> hole_cards(new Card[4]);
  board[0] = MakeCard(5, 3);
  board[1] = MakeCard(1, 2);
  board[2] = MakeCard(0, 1);
  board[3] = MakeCard(4, 2);
  board[4] = MakeCard(2, 2);
  hole_cards[0] = MakeCard(4, 1);
  hole_cards[1] = MakeCard(4, 0);
  hole_cards[2] = MakeCard(6, 1);
  hole_cards[3] = MakeCard(5, 0);
  bool call, fold;
  int bet_size;
  int st = 0;
  int pa = 1;
  bool call_ends_street = false;
  int hand_no = 0;
  while (true) {
    Card *our_hole_cards = hole_cards.get() + pa * 2;
    Card *opp_hole_cards = hole_cards.get() + (pa^1) * 2;
    MatchState match_state(pa == 1, hand_no, action, our_hole_cards[0], our_hole_cards[1],
			   opp_hole_cards[0], opp_hole_cards[1], board.get(), st);
    if (! agent.ProcessMatchState(match_state, &call, &fold, &bet_size)) {
      fprintf(stderr, "Not player %i's turn to play?!?\n", pa);
      fprintf(stderr, "Action: %s\n", action.c_str());
      exit(-1);
    }
    if (call) {
      printf("\"%s\" call\n", action.c_str());
      action += "c";
      if (call_ends_street) {
	st += 1;
	pa = 0;
	call_ends_street = false;
	if (st == 4) break;
      } else {
	call_ends_street = true;
	pa = pa^1;
      }
    } else if (fold) {
      printf("\"%s\" fold\n", action.c_str());
      action += "f";
      break;
    } else {
      printf("\"%s\" bet %i\n", action.c_str(), bet_size);
      char buf[10];
      sprintf(buf, "r%i", bet_size);
      action += buf;
      call_ends_street = true;
      pa = pa^1;
    }
  }
  printf("Final action: %s\n", action.c_str());
}
