#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <memory>
#include <string>

#include "board_tree.h"
#include "canonical_cards.h"
#include "constants.h"
#include "files.h"
#include "game.h"
#include "game_params.h"
#include "io.h"
#include "params.h"

using std::string;
using std::unique_ptr;

static void Usage(const char *prog_name) {
  fprintf(stderr, "USAGE: %s <game params> <street>\n", prog_name);
  exit(-1);
}

int main(int argc, char *argv[]) {
  if (argc != 3) Usage(argv[0]);
  Files::Init();
  unique_ptr<Params> game_params = CreateGameParams();
  game_params->ReadFromFile(argv[1]);
  Game::Initialize(*game_params);
  int street;
  if (sscanf(argv[2], "%u", &street) != 1) Usage(argv[0]);

  BoardTree::Create();
  int num_boards = BoardTree::NumBoards(street);
  int num_hole_card_pairs = Game::NumHoleCardPairs(street);
  int num_hands = num_boards * num_hole_card_pairs;
  int num_hole_cards = Game::NumCardsForStreet(0);
  int num_board_cards = Game::NumBoardCards(street);
  unique_ptr<int []> buckets(new int[num_hands]);
  int max_card = Game::MaxCard();
  int num_enc = (max_card + 1) * (max_card + 1);
  unique_ptr<int []> enc_buckets(new int[num_enc]);

  int b = 0;
  for (int bd = 0; bd < num_boards; ++bd) {
    const Card *board = BoardTree::Board(street, bd);
    CanonicalCards hands(num_hole_cards, board, num_board_cards,
			 BoardTree::SuitGroups(street, bd), true);
    int num_raw = hands.NumRaw();
    for (int hcp = 0; hcp < num_raw; ++hcp) {
      if (hands.NumVariants(hcp) == 0) continue;
      const Card *cards = hands.Cards(hcp);
      int enc = cards[0] * (max_card + 1) + cards[1];
      enc_buckets[enc] = b;
      int h = bd * num_hole_card_pairs + hcp;
      buckets[h] = b++;
      
    }
    for (int hcp = 0; hcp < num_raw; ++hcp) {
      if (hands.NumVariants(hcp) != 0) continue;
      int h = bd * num_hole_card_pairs + hcp;
      int enc = hands.Canon(hcp);
      buckets[h] = enc_buckets[enc];
    }
  }
  int num_buckets = b;
  printf("Num buckets: %u\n", num_buckets);

  bool short_buckets = (num_buckets <= 65536);

  char buf[500];
  sprintf(buf, "%s/buckets.%s.%u.%u.%u.null.%u",
	  Files::StaticBase(), Game::GameName().c_str(), Game::NumRanks(),
	  Game::NumSuits(), Game::MaxStreet(), street);
  Writer writer(buf);
  for (int h = 0; h < num_hands; ++h) {
    if (short_buckets) writer.WriteUnsignedShort(buckets[h]);
    else               writer.WriteInt(buckets[h]);
  }

  sprintf(buf, "%s/num_buckets.%s.%u.%u.%u.null.%u",
	  Files::StaticBase(), Game::GameName().c_str(), Game::NumRanks(),
	  Game::NumSuits(), Game::MaxStreet(), street);
  Writer writer2(buf);
  writer2.WriteInt(num_buckets);
}
