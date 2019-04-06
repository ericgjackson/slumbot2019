// Currently we assume all features values are shorts, but we should generalize.

#include <stdio.h>
#include <stdlib.h>

#include "board_tree.h"
#include "constants.h"
#include "fast_hash.h"
#include "files.h"
#include "game.h"
#include "game_params.h"
#include "io.h"
#include "kmeans.h"
#include "params.h"
#include "rand.h"
#include "sparse_and_dense.h"

using namespace std;

static void Write(int st, const string &bucketing, KMeans *kmeans, int *indices, int num_buckets) {
  int max_street = Game::MaxStreet();
  char buf[500];
  bool short_buckets = num_buckets <= 65536;
  sprintf(buf, "%s/buckets.%s.%i.%i.%i.%s.%i", Files::StaticBase(), Game::GameName().c_str(),
	  Game::NumRanks(), Game::NumSuits(), max_street, bucketing.c_str(), st);
  Writer writer(buf);
  unsigned int num_hole_card_pairs = Game::NumHoleCardPairs(st);
  unsigned int num_hands = ((unsigned int)BoardTree::NumBoards(st)) * num_hole_card_pairs;
  for (unsigned int h = 0; h < num_hands; ++h) {
    int index = indices[h];
    int b = kmeans->Assignment(index);
    if (short_buckets) {
      if (b > kMaxUnsignedShort) {
	fprintf(stderr, "Bucket %i out of range for short\n", b);
	exit(-1);
      }
      writer.WriteUnsignedShort(b);
    } else {
      writer.WriteInt(b);
    }
  }

  sprintf(buf, "%s/num_buckets.%s.%i.%i.%i.%s.%i", Files::StaticBase(), Game::GameName().c_str(),
	  Game::NumRanks(), Game::NumSuits(), max_street, bucketing.c_str(), st);
  Writer writer2(buf);
  writer2.WriteUnsignedInt(num_buckets);
}

static void Usage(const char *prog_name) {
  fprintf(stderr, "USAGE: %s <game params> <street> <num clusters> <bucketing> <features> "
	  "<neighbor thresh> <num iterations> <num threads>\n", prog_name);
  exit(-1);
}

int main(int argc, char *argv[]) {
  if (argc != 9) Usage(argv[0]);
  Files::Init();
  unique_ptr<Params> game_params = CreateGameParams();
  game_params->ReadFromFile(argv[1]);
  Game::Initialize(*game_params);
  int st;
  if (sscanf(argv[2], "%i", &st) != 1) Usage(argv[0]);
  int num_clusters;
  if (sscanf(argv[3], "%i", &num_clusters) != 1) Usage(argv[0]);
  string bucketing = argv[4];
  string features = argv[5];
  double neighbor_thresh;
  if (sscanf(argv[6], "%lf", &neighbor_thresh) != 1) Usage(argv[0]);
  int num_iterations, num_threads;
  if (sscanf(argv[7], "%i", &num_iterations) != 1)  Usage(argv[0]);
  if (sscanf(argv[8], "%i", &num_threads) != 1)     Usage(argv[0]);

  // Make clustering deterministic
  SeedRand(0);

  // Just need this to get number of hands
  BoardTree::Create();
  unsigned int num_hole_card_pairs = Game::NumHoleCardPairs(st);
  unsigned int num_hands = ((unsigned int)BoardTree::NumBoards(st)) * num_hole_card_pairs;
  fprintf(stderr, "%u hands\n", num_hands);
  int *indices = new int[num_hands];

  char buf[500];
  sprintf(buf, "%s/features.%s.%i.%s.%i", Files::StaticBase(), Game::GameName().c_str(),
	  Game::NumRanks(), features.c_str(), st);
  Reader reader(buf);
  int num_features = reader.ReadIntOrDie();
  fprintf(stderr, "%u features\n", num_features);
  short *feature_vals = new short[num_features];
  SparseAndDenseLong *sad = new SparseAndDenseLong;
  uint64_t hash_seed = 0;
  vector<short *> *unique_objects = new vector<short *>;
  for (unsigned int h = 0; h < num_hands; ++h) {
    if (h % 10000000 == 0) {
      fprintf(stderr, "h %u\n", h);
    }
    for (int f = 0; f < num_features; ++f) {
      feature_vals[f] = reader.ReadShortOrDie();
    }
    unsigned long long int hash = fasthash64((void *)feature_vals, num_features * sizeof(short),
					     hash_seed);
    int old_num = sad->Num();
    int index = sad->SparseToDense(hash);
    indices[h] = index;
    int new_num = sad->Num();
    if (new_num > old_num && new_num % 1000000 == 0) {
      fprintf(stderr, "%i unique feature combos so far\n", new_num);
    }
    if (new_num > old_num) {
      // Previously unseen feature value vector
      short *copy = new short[num_features];
      for (int f = 0; f < num_features; ++f) {
	copy[f] = feature_vals[f];
      }
      unique_objects->push_back(copy);
    }
    if (sad->Num() != (int)unique_objects->size()) {
      fprintf(stderr, "Size mismatch: %i vs. %i\n", sad->Num(), (int)unique_objects->size());
      exit(-1);
    }
  }
  delete [] feature_vals;

  int num_unique = sad->Num();
  if (num_unique != (int)unique_objects->size()) {
    fprintf(stderr, "Final size mismatch: %i vs. %i\n", num_unique, (int)unique_objects->size());
    exit(-1);
  }
  fprintf(stderr, "%i unique objects\n", num_unique);
  delete sad;

  float **objects = new float *[num_unique];
  for (int i = 0; i < num_unique; ++i) {
    objects[i] = new float[num_features];
    for (int f = 0; f < num_features; ++f) {
      objects[i][f] = (*unique_objects)[i][f];
    }
    delete [] (*unique_objects)[i];
  }
  delete unique_objects;

  KMeans kmeans(num_clusters, num_features, num_unique, objects, neighbor_thresh, num_threads);
  kmeans.Cluster(num_iterations);
  int num_actual = kmeans.NumClusters();
  fprintf(stderr, "Num actual buckets: %i\n", num_actual);

  for (int i = 0; i < num_unique; ++i) {
    delete [] objects[i];
  }
  delete [] objects;

  Write(st, bucketing, &kmeans, indices, num_actual);

  delete [] indices;
}
