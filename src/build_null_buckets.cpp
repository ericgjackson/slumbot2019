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

#if 0
static void NullPreflop(Writer *writer, bool short_buckets,
			int *num_buckets) {
  int num_hole_cards = Game::NumCardsForStreet(0);
  fprintf(stderr, "num_hole_cards %u\n", num_hole_cards);
  int max_card = Game::MaxCard();
  int num;
  if (num_hole_cards == 1) num = max_card + 1;
  else                     num = (max_card + 1) * (max_card + 1);
  CanonicalCards hands(num_hole_cards, NULL, 0, 0);
  int num_raw_hands = hands.Num();
  int *buckets = new int[num];
  for (int i = 0; i < num; ++i) buckets[i] = -1;
  int bucket_index = 0;
  for (int i = 0; i < num_raw_hands; ++i) {
    if (hands.NumVariants(i) > 0) {
      Card *cards = hands.Cards(i);
      int enc;
      if (num_hole_cards == 1) {
	enc = cards[0];
      } else {
	enc = cards[0] * (max_card + 1) + cards[1];
      }
      buckets[enc] = bucket_index++;
    }
  }
  for (int i = 0; i < num_raw_hands; ++i) {
    int enc = hands.Canon(i);
    int b = buckets[enc];
    if (short_buckets) {
      if (b > kMaxUnsignedShort) {
	fprintf(stderr, "Bucket %i out of range for short\n", b);
	exit(-1);
      }
      writer->WriteUnsignedShort(b);
    } else {
      writer->WriteInt(b);
    }
  }
  printf("%u preflop buckets\n", bucket_index);
  *num_buckets = bucket_index;
  delete [] buckets;
}

static void NullFlop(Writer *writer, bool short_buckets,
		     int *num_buckets) {
  int num_hole_cards = Game::NumCardsForStreet(0);
  int num_board_cards = Game::NumBoardCards(1);
  CanonicalCards flops(num_board_cards, NULL, 0, 0);
  int num_raw = flops.Num();
  int max_card = Game::MaxCard();
  int num;
  if (num_hole_cards == 1) num = max_card + 1;
  else                     num = (max_card + 1) * (max_card + 1);
  Card *board = new Card[num_board_cards];
  int bucket_index = 0;
  for (int raw_fbd = 0; raw_fbd < num_raw; ++raw_fbd) {
    if (flops.NumVariants(raw_fbd) == 0) continue;
    for (int i = 0; i < num_board_cards; ++i) {
      board[i] = flops.Cards(raw_fbd)[i];
    }
    CanonicalCards hands(num_hole_cards, board, num_board_cards,
			 flops.SuitGroups(raw_fbd));
    int num_raw_hands = hands.Num();
    int *buckets = new int[num];
    for (int i = 0; i < num; ++i) buckets[i] = -1;
    for (int i = 0; i < num_raw_hands; ++i) {
      if (hands.NumVariants(i) > 0) {
	Card *cards = hands.Cards(i);
	int enc;
	if (num_hole_cards == 1) {
	  enc = cards[0];
	} else {
	  enc = cards[0] * (max_card + 1) + cards[1];
	}
	buckets[enc] = bucket_index++;
      }
    }
    for (int i = 0; i < num_raw_hands; ++i) {
      int enc = hands.Canon(i);
      int b = buckets[enc];
      if (short_buckets) {
	if (b > kMaxUnsignedShort) {
	  fprintf(stderr, "Bucket %i out of range for short\n", b);
	  exit(-1);
	}
	writer->WriteUnsignedShort(b);
      } else {
	writer->WriteInt(b);
      }
    }
    delete [] buckets;
  }
  printf("%u flop buckets\n", bucket_index);
  *num_buckets = bucket_index;
  delete [] board;
}

static void NullTurn(Writer *writer, bool short_buckets,
		     int *num_buckets) {
  int num_hole_cards = Game::NumCardsForStreet(0);
  int num_flop_board_cards = Game::NumBoardCards(1);
  int num_turn_board_cards = Game::NumBoardCards(2);
  int num_incr_turn_board_cards = Game::NumCardsForStreet(2);
  CanonicalCards flops(num_flop_board_cards, NULL, 0, 0);
  int num_raw = flops.Num();
  int max_card = Game::MaxCard();
  int num;
  if (num_hole_cards == 1) num = max_card + 1;
  else                     num = (max_card + 1) * (max_card + 1);
  Card *board = new Card[num_turn_board_cards];
  int bucket_index = 0;
  for (int raw_fbd = 0; raw_fbd < num_raw; ++raw_fbd) {
    if (flops.NumVariants(raw_fbd) == 0) continue;
    for (int i = 0; i < num_flop_board_cards; ++i) {
      board[i] = flops.Cards(raw_fbd)[i];
    }
    CanonicalCards turns(num_incr_turn_board_cards, board, num_flop_board_cards,
			 flops.SuitGroups(raw_fbd));
    int num_raw_turns = turns.Num();
    for (int raw_tbd = 0; raw_tbd < num_raw_turns; ++raw_tbd) {
      if (turns.NumVariants(raw_tbd) == 0) continue;
      for (int i = 0; i < num_incr_turn_board_cards; ++i) {
	board[num_flop_board_cards + i] = turns.Cards(raw_tbd)[i];
      }
      CanonicalCards hands(num_hole_cards, board, num_turn_board_cards,
			   turns.SuitGroups(raw_tbd));
      int num_raw_hands = hands.Num();
      int *buckets = new int[num];
      for (int i = 0; i < num; ++i) buckets[i] = -1;
      for (int i = 0; i < num_raw_hands; ++i) {
	if (hands.NumVariants(i) > 0) {
	  Card *cards = hands.Cards(i);
	  int enc;
	  if (num_hole_cards == 1) {
	    enc = cards[0];
	  } else {
	    enc = cards[0] * (max_card + 1) + cards[1];
	  }
	  buckets[enc] = bucket_index++;
	}
      }
      for (int i = 0; i < num_raw_hands; ++i) {
	int enc = hands.Canon(i);
	int b = buckets[enc];
	if (short_buckets) {
	  if (b > kMaxUnsignedShort) {
	    fprintf(stderr, "Bucket %i out of range for short\n", b);
	    exit(-1);
	  }
	  writer->WriteUnsignedShort(b);
	} else {
	  writer->WriteInt(b);
	}
      }
      delete [] buckets;
    }
  }
  printf("%u turn buckets\n", bucket_index);
  *num_buckets = bucket_index;
  delete [] board;
}

static void NullRiver(Writer *writer, bool short_buckets,
		      int *num_buckets) {
  int num_hole_cards = Game::NumCardsForStreet(0);
  int num_flop_board_cards = Game::NumBoardCards(1);
  int num_turn_board_cards = Game::NumBoardCards(2);
  int num_river_board_cards = Game::NumBoardCards(3);
  int num_incr_turn_board_cards = Game::NumCardsForStreet(2);
  int num_incr_river_board_cards = Game::NumCardsForStreet(3);
  CanonicalCards flops(num_flop_board_cards, NULL, 0, 0);
  int num_raw = flops.Num();
  int max_card = Game::MaxCard();
  int num;
  if (num_hole_cards == 1) num = max_card + 1;
  else                     num = (max_card + 1) * (max_card + 1);
  Card *board = new Card[num_river_board_cards];
  int bucket_index = 0;
  for (int raw_fbd = 0; raw_fbd < num_raw; ++raw_fbd) {
    if (flops.NumVariants(raw_fbd) == 0) continue;
    for (int i = 0; i < num_flop_board_cards; ++i) {
      board[i] = flops.Cards(raw_fbd)[i];
    }
    CanonicalCards turns(num_incr_turn_board_cards, board, num_flop_board_cards,
			 flops.SuitGroups(raw_fbd));
    int num_raw_turns = turns.Num();
    for (int raw_tbd = 0; raw_tbd < num_raw_turns; ++raw_tbd) {
      if (turns.NumVariants(raw_tbd) == 0) continue;
      for (int i = 0; i < num_incr_turn_board_cards; ++i) {
	board[num_flop_board_cards + i] = turns.Cards(raw_tbd)[i];
      }
      CanonicalCards rivers(num_incr_river_board_cards, board,
			    num_turn_board_cards, turns.SuitGroups(raw_tbd));
      int num_raw_rivers = rivers.Num();
      for (int raw_rbd = 0; raw_rbd < num_raw_rivers; ++raw_rbd) {
	if (rivers.NumVariants(raw_rbd) == 0) continue;
	for (int i = 0; i < num_incr_river_board_cards; ++i) {
	  board[num_turn_board_cards + i] = rivers.Cards(raw_rbd)[i];
	}
	CanonicalCards hands(num_hole_cards, board, num_river_board_cards,
			     rivers.SuitGroups(raw_rbd));
	int num_raw_hands = hands.Num();
	int *buckets = new int[num];
	for (int i = 0; i < num; ++i) buckets[i] = -1;
	for (int i = 0; i < num_raw_hands; ++i) {
	  if (hands.NumVariants(i) > 0) {
	    Card *cards = hands.Cards(i);
	    int enc;
	    if (num_hole_cards == 1) {
	      enc = cards[0];
	    } else {
	      enc = cards[0] * (max_card + 1) + cards[1];
	    }
	    buckets[enc] = bucket_index++;
	  }
	}
	for (int i = 0; i < num_raw_hands; ++i) {
	  int enc = hands.Canon(i);
	  int b = buckets[enc];
	  if (short_buckets) {
	    if (b > kMaxUnsignedShort) {
	      fprintf(stderr, "Bucket %i out of range for short\n", b);
	      exit(-1);
	    }
	    writer->WriteUnsignedShort(b);
	  } else {
	    writer->WriteInt(b);
	  }
	}
	delete [] buckets;
      }
    }
  }
  printf("%u river buckets\n", bucket_index);
  *num_buckets = bucket_index;
  delete [] board;
}
#endif

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
