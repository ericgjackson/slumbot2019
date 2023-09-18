// Only working for bucketed systems currently.

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
#include "io.h"
#include "params.h"

using std::string;
using std::unique_ptr;
using std::unordered_set;

void Show(Node *node, const Buckets &buckets, const CFRValues &values) {
  int pa = node->PlayerActing();
  int nt = node->NonterminalID();
  int st = node->Street();
  int num_succs = node->NumSuccs();
  int dsi = node->DefaultSuccIndex();
  AbstractCFRStreetValues *street_values = values.StreetValues(st);
  CFRStreetValues<double> *d_street_values;
  CFRStreetValues<int> *i_street_values;
  int *i_values = nullptr;
  double *d_values = nullptr;
  if ((d_street_values =
       dynamic_cast<CFRStreetValues<double> *>(street_values))) {
    d_values = d_street_values->AllValues(pa, nt);
  } else {
    i_street_values = dynamic_cast<CFRStreetValues<int> *>(street_values);
    i_values = i_street_values->AllValues(pa, nt);
  }

  unsigned int num_hole_card_pairs = Game::NumHoleCardPairs(st);
  unsigned int num_board_cards = Game::NumBoardCards(st);
  unsigned int num_boards = BoardTree::NumBoards(st);
  int max_card = Game::MaxCard();

  for (unsigned int bd = 0; bd < num_boards; ++bd) {
    const Card *board = BoardTree::Board(st, bd);
    unsigned int hcp = 0;
    for (int hi = 1; hi <= max_card; ++hi) {
      if (InCards(hi, board, num_board_cards)) continue;
      for (int lo = 0; lo < hi; ++lo) {
	if (InCards(lo, board, num_board_cards)) continue;
	OutputNCards(board, num_board_cards);
	printf(" / ");
	OutputTwoCards(hi, lo);
	int offset, b;
	if (buckets.None(st)) {
	  offset = hcp * num_succs;
	} else {
	  unsigned int h = bd * num_hole_card_pairs + hcp;
	  b = buckets.Bucket(st, h);
	  offset = b * num_succs;
	}
	double sum = 0;
	if (i_values) {
	  for (int s = 0; s < num_succs; ++s) {
	    int iv = i_values[offset + s];
	    if (iv > 0) sum += iv;
	  }
	} else {
	  for (int s = 0; s < num_succs; ++s) {
	    double dv = d_values[offset + s];
	    if (dv > 0) sum += dv;
	  }
	}
	if (sum == 0) {
	  for (int s = 0; s < num_succs; ++s) {
	    printf(" %f", s == dsi ? 1.0 : 0);
	  }
	} else {
	  for (int s = 0; s < num_succs; ++s) {
	    if (i_values) {
	      int iv = i_values[offset + s];
	      printf(" %f (%i)", iv > 0 ? iv / sum : 0,
		     i_values[offset + s]);
	    } else {
	      double dv = d_values[offset + s];
	      printf(" %f (%f)", dv > 0 ? dv / sum : 0,
		     d_values[offset + s]);
	    }
	  }
	}
	if (! buckets.None(st)) {
	  printf(" (b %i)", b);
	}
	printf(" (pa %i nt %i)\n", node->PlayerActing(), node->NonterminalID());
	fflush(stdout);
	++hcp;
      }
    }
  }
}

void Show(Node *node, const string &action_sequence, const string &target_action_sequence,
	  const Buckets &buckets, const CFRValues &values, bool ***seen) {
  if (node->Terminal()) return;
  int pa = node->PlayerActing();
  int nt = node->NonterminalID();
  int st = node->Street();
  if (seen[st][pa][nt]) return;
  seen[st][pa][nt] = true;
  if (action_sequence == target_action_sequence) {
    Show(node, buckets, values);
  }
  int num_succs = node->NumSuccs();
  for (int s = 0; s < num_succs; ++s) {
    string action = node->ActionName(s);
    Show(node->IthSucc(s), action_sequence + action, target_action_sequence, buckets,
	 values, seen);
  }
}

static void Usage(const char *prog_name) {
  fprintf(stderr, "USAGE: %s <game params> <card params> <betting params> "
	  "<CFR params> <it> [current|cum] <betting sequence> ([p0|p1])\n", prog_name);
  exit(-1);
}

int main(int argc, char *argv[]) {
  if (argc != 8 && argc != 9) Usage(argv[0]);
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
  bool current;
  string c = argv[6];
  if (c == "current")  current = true;
  else if (c == "cum") current = false;
  else                 Usage(argv[0]);
  string target_action_sequence = argv[7];
  int target_p = -1;
  if (betting_abstraction->Asymmetric()) {
    if (argc == 8) {
      fprintf(stderr, "Expect p0/p1 argument for asymmetric systems\n");
      exit(-1);
    }
    string p_arg = argv[8];
    if (p_arg == "p0")      target_p = 0;
    else if (p_arg == "p1") target_p = 1;
    else                    Usage(argv[0]);
  } else {
    if (argc == 9) {
      fprintf(stderr, "Don't expect p0/p1 argument for symmetric systems\n");
      exit(-1);
    }
  }

  // Excessive to load all buckets.  Only need buckets for the preflop.
  Buckets buckets(*card_abstraction, false);
  unique_ptr<BettingTree> betting_tree;
  if (betting_abstraction->Asymmetric()) {
    betting_tree.reset(new BettingTree(*betting_abstraction, target_p));
  } else {
    betting_tree.reset(new BettingTree(*betting_abstraction));
  }
  int num_players = Game::NumPlayers();
  unique_ptr<bool []> players(new bool[num_players]);
  if (betting_abstraction->Asymmetric()) {
    for (int p = 0; p < num_players; ++p) {
      // Have both player's strategies and want to show them.
      // players[p] = (p == target_p);
      players[p] = true;
    }
  } else {
    for (int p = 0; p < num_players; ++p) {
      players[p] = true;
    }
  }
  int max_street = Game::MaxStreet();
  unique_ptr<bool []> streets(new bool[max_street + 1]);
  for (int st = 0; st <= max_street; ++st) {
    streets[st] = true;
  }
  CFRValues values(players.get(), streets.get(), 0, 0, buckets, betting_tree.get());
  char dir[500];
  sprintf(dir, "%s/%s.%u.%s.%u.%u.%u.%s.%s", Files::OldCFRBase(), Game::GameName().c_str(),
	  Game::NumPlayers(), card_abstraction->CardAbstractionName().c_str(),
	  Game::NumRanks(), Game::NumSuits(), Game::MaxStreet(),
	  betting_abstraction->BettingAbstractionName().c_str(),
	  cfr_config->CFRConfigName().c_str());
  if (betting_abstraction->Asymmetric()) {
    char buf[20];
    sprintf(buf, ".p%u", target_p);
    strcat(dir, buf);
  }
  values.Read(dir, it, betting_tree.get(), "x", -1, ! current, false);
  bool ***seen = new bool **[max_street + 1];
  for (int st = 0; st <= max_street; ++st) {
    seen[st] = new bool *[num_players];
    for (int p = 0; p < num_players; ++p) {
      int num_nt = betting_tree->NumNonterminals(p, st);
      seen[st][p] = new bool[num_nt];
      for (int i = 0; i < num_nt; ++i) {
	seen[st][p][i] = false;
      }
    }
  }
  Show(betting_tree->Root(), "", target_action_sequence, buckets, values, seen);
  for (int st = 0; st <= max_street; ++st) {
    for (int p = 0; p < num_players; ++p) {
      delete [] seen[st][p];
    }
    delete [] seen[st];
  }
  delete [] seen;
}
