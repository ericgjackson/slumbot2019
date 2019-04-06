// Takes a bucketing for the previous street and an IR bucketing for the current street and creates
// a new perfect recall bucketing that remembers the bucket from the previous street.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <memory>
#include <string>

#include "board_tree.h"
#include "constants.h"
#include "fast_hash.h"
#include "files.h"
#include "game.h"
#include "game_params.h"
#include "hand_tree.h"
#include "io.h"
#include "params.h"
#include "sparse_and_dense.h"

using std::string;
using std::unique_ptr;

static void Usage(const char *prog_name) {
  fprintf(stderr, "USAGE: %s <game params> <prev bucketing> <IR bucketing> <new bucketing> "
	  "<street>\n", prog_name);
  exit(-1);
}

int main(int argc, char *argv[]) {
  if (argc != 6) Usage(argv[0]);
  Files::Init();
  unique_ptr<Params> game_params = CreateGameParams();
  game_params->ReadFromFile(argv[1]);
  Game::Initialize(*game_params);
  string prev_bucketing = argv[2];
  string ir_bucketing = argv[3];
  string new_bucketing = argv[4];
  int st;
  if (sscanf(argv[5], "%i", &st) != 1) Usage(argv[0]);
  int max_street = Game::MaxStreet();
  if (st < 1 || st > max_street) {
    fprintf(stderr, "Street OOB\n");
    exit(-1);
  }
  int pst = st - 1;

  char buf[500];
  sprintf(buf, "%s/num_buckets.%s.%i.%i.%i.%s.%i", Files::StaticBase(), Game::GameName().c_str(),
	  Game::NumRanks(), Game::NumSuits(), max_street, prev_bucketing.c_str(), pst);
  Reader nb_reader(buf);
  long long int prev_num_buckets = nb_reader.ReadIntOrDie();

  BoardTree::Create();
  long long int prev_num_boards = BoardTree::NumBoards(pst);
  long long int prev_num_hole_card_pairs = Game::NumHoleCardPairs(pst);
  long long int prev_num_hands = prev_num_boards * prev_num_hole_card_pairs;
  unique_ptr<int []> prev_buckets(new int[prev_num_hands]);
  sprintf(buf, "%s/buckets.%s.%i.%i.%i.%s.%i", Files::StaticBase(), Game::GameName().c_str(),
	  Game::NumRanks(), Game::NumSuits(), max_street, prev_bucketing.c_str(), pst);
  Reader prev_reader(buf);
  long long int prev_file_size = prev_reader.FileSize();
  if (prev_file_size == 2 * prev_num_hands) {
    for (long long int h = 0; h < prev_num_hands; ++h) {
      prev_buckets[h] = prev_reader.ReadUnsignedShortOrDie();
    }
  } else if (prev_file_size == 4 * prev_num_hands) {
    for (long long int h = 0; h < prev_num_hands; ++h) {
      prev_buckets[h] = prev_reader.ReadIntOrDie();
    }
  } else {
    fprintf(stderr, "Unexpected file size: %lli\n", prev_file_size);
    exit(-1);
  }

  long long int num_boards = BoardTree::NumBoards(st);
  long long int num_hole_card_pairs = Game::NumHoleCardPairs(st);
  long long int num_hands = num_boards * num_hole_card_pairs;

  sprintf(buf, "%s/buckets.%s.%i.%i.%i.%s.%i", Files::StaticBase(), Game::GameName().c_str(),
	  Game::NumRanks(), Game::NumSuits(), max_street, ir_bucketing.c_str(), st);
  Reader ir_reader(buf);
  long long int ir_file_size = ir_reader.FileSize();
  bool ir_shorts;
  if (ir_file_size == 2 * num_hands) {
    ir_shorts = true;
  } else if (ir_file_size == 4 * num_hands) {
    ir_shorts = false;
  } else {
    fprintf(stderr, "Unexpected file size B: %lli\n", ir_file_size);
    fprintf(stderr, "Num hands %lli\n", num_hands);
    fprintf(stderr, "File: %s\n", buf);
    exit(-1);
  }

  BoardTree::CreateLookup();
  unique_ptr<int []> buckets(new int[num_hands]);
  int num_board_cards = Game::NumBoardCards(st);
  int max_card = Game::MaxCard();
  Card cards[7];
  SparseAndDenseLong sad;
  for (int bd = 0; bd < num_boards; ++bd) {
    if (bd % 1000 == 0) fprintf(stderr, "bd %i/%lli\n", bd, num_boards);
    const Card *board = BoardTree::Board(st, bd);
    int prev_bd = BoardTree::LookupBoard(board, pst);
    for (int i = 0; i < num_board_cards; ++i) {
      cards[i + 2] = board[i];
    }
    long long int h = ((long long int)bd) * ((long long int)num_hole_card_pairs);
    for (int hi = 1; hi <= max_card; ++hi) {
      if (InCards(hi, cards + 2, num_board_cards)) continue;
      cards[0] = hi;
      for (int lo = 0; lo < hi; ++lo) {
	if (InCards(lo, cards + 2, num_board_cards)) continue;
	cards[1] = lo;
	long long int ir_b;
	if (ir_shorts) {
	  ir_b = ir_reader.ReadUnsignedShortOrDie();
	} else {
	  ir_b = ir_reader.ReadIntOrDie();
	}
	int prev_hcp = HCPIndex(pst, cards);
	long long int prev_h =
	  ((long long int)prev_bd) * ((long long int)prev_num_hole_card_pairs) + prev_hcp;
	long long int prev_b = prev_buckets[prev_h];
	long long int sparse = ir_b * prev_num_buckets + prev_b;
	int b = sad.SparseToDense(sparse);
	buckets[h] = b;
	++h;
      }
    }
  }

  int num_buckets = sad.Num();
  bool short_buckets = num_buckets < 65536;

  sprintf(buf, "%s/buckets.%s.%i.%i.%i.%s.%i", Files::StaticBase(), Game::GameName().c_str(),
	  Game::NumRanks(), Game::NumSuits(), Game::MaxStreet(), new_bucketing.c_str(), st);
  Writer writer(buf);
  if (short_buckets) {
    for (long long int h = 0; h < num_hands; ++h) {
      writer.WriteUnsignedShort((unsigned short)buckets[h]);
    }
  } else {
    for (long long int h = 0; h < num_hands; ++h) {
      writer.WriteInt(buckets[h]);
    }
  }

  sprintf(buf, "%s/num_buckets.%s.%i.%i.%i.%s.%i", Files::StaticBase(), Game::GameName().c_str(),
	  Game::NumRanks(), Game::NumSuits(), Game::MaxStreet(), new_bucketing.c_str(), st);
  Writer writer2(buf);
  writer2.WriteInt(num_buckets);
  printf("%i buckets\n", num_buckets);
}
