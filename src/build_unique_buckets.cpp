// Build buckets from a set of features where every distinct feature vector
// becomes its own bucket.  Currently using percentile hand strength
// features.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <string>
#include <unordered_map>

#include "board_tree.h"
#include "constants.h"
#include "fast_hash.h"
#include "files.h"
#include "game.h"
#include "game_params.h"
#include "io.h"
#include "params.h"
#include "rollout.h"
#include "sparse_and_dense.h"

using namespace std;

static void Usage(const char *prog_name) {
  fprintf(stderr, "USAGE: %s <game params> <street> <bucketing> <features>\n",
	  prog_name);
  exit(-1);
}

int main(int argc, char *argv[]) {
  if (argc != 5) Usage(argv[0]);
  Files::Init();
  unique_ptr<Params> game_params = CreateGameParams();
  game_params->ReadFromFile(argv[1]);
  Game::Initialize(*game_params);
  unsigned int st;
  if (sscanf(argv[2], "%u", &st) != 1) Usage(argv[0]);
  string bucketing = argv[3];
  string features = argv[4];

  BoardTree::Create();
  unsigned int num_boards = BoardTree::NumBoards(st);
  unsigned int num_hole_card_pairs = Game::NumHoleCardPairs(st);
  unsigned int num_hands = num_boards * num_hole_card_pairs;

  unsigned int *buckets = new unsigned int[num_hands];

  char buf[500];
  sprintf(buf, "%s/features.%s.%u.%s.%u", Files::StaticBase(),
	  Game::GameName().c_str(), Game::NumRanks(), features.c_str(), st);
  Reader reader(buf);
  unsigned int num_features = reader.ReadUnsignedIntOrDie();
  fprintf(stderr, "%u features\n", num_features);
  short *feature_vals = new short[num_features];
  SparseAndDenseLong *sad = new SparseAndDenseLong;
  uint64_t hash_seed = 0;
  for (unsigned int h = 0; h < num_hands; ++h) {
    if (h % 10000000 == 0) {
      fprintf(stderr, "h %u\n", h);
    }
    for (unsigned int f = 0; f < num_features; ++f) {
      feature_vals[f] = reader.ReadShortOrDie();
    }
    unsigned long long int hash = fasthash64((void *)feature_vals,
					     num_features * sizeof(short),
					     hash_seed);
    unsigned int old_num = sad->Num();
    unsigned int b = sad->SparseToDense(hash);
    buckets[h] = b;
    unsigned int new_num = sad->Num();
    if (new_num > old_num && new_num % 1000000 == 0) {
      fprintf(stderr, "%u unique feature combos so far\n", new_num);
    }
  }
  delete [] feature_vals;

  unsigned int num_buckets = sad->Num();
  fprintf(stderr, "%u buckets\n", num_buckets);
  delete sad;

  bool short_buckets = num_buckets < 65536;

  sprintf(buf, "%s/buckets.%s.%i.%i.%i.%s.%i",
	  Files::StaticBase(), Game::GameName().c_str(), Game::NumRanks(),
	  Game::NumSuits(), Game::MaxStreet(), bucketing.c_str(), st);
  Writer writer(buf);
  if (short_buckets) {
    for (unsigned int h = 0; h < num_hands; ++h) {
      writer.WriteUnsignedShort((unsigned short)buckets[h]);
    }
  } else {
    for (unsigned int h = 0; h < num_hands; ++h) {
      writer.WriteUnsignedInt(buckets[h]);
    }
  }

  sprintf(buf, "%s/num_buckets.%s.%i.%i.%i.%s.%i",
	  Files::StaticBase(), Game::GameName().c_str(), Game::NumRanks(),
	  Game::NumSuits(), Game::MaxStreet(), bucketing.c_str(), st);
  Writer writer2(buf);
  writer2.WriteUnsignedInt(num_buckets);
}
