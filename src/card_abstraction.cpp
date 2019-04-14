// Requires the Game object to have been initialized first.

#include <stdio.h>
#include <stdlib.h>

#include <memory>
#include <string>
#include <vector>

#include "card_abstraction.h"
#include "constants.h"
#include "files.h"
#include "game.h"
#include "io.h"
#include "params.h"
#include "split.h"

using std::string;
using std::unique_ptr;
using std::vector;

CardAbstraction::CardAbstraction(const Params &params) {
  card_abstraction_name_ = params.GetStringValue("CardAbstractionName");
  Split(params.GetStringValue("Bucketings").c_str(), ',', false,
	&bucketings_);
  int max_street = Game::MaxStreet();
  if ((int)bucketings_.size() < max_street + 1) {
    fprintf(stderr, "Expected at least %i bucketings\n", max_street + 1);
    exit(-1);
  }
  bucket_thresholds_.reset(new int[max_street + 1]);
  if (params.IsSet("BucketThresholds")) {
    vector<int> v;
    ParseInts(params.GetStringValue("BucketThresholds"), &v);
    if ((int)v.size() != max_street + 1) {
      fprintf(stderr, "Expected %i values in BucketThresholds\n",
	      max_street + 1);
      exit(-1);
    }
    for (int st = 0; st <= max_street; ++st) {
      bucket_thresholds_[st] = v[st];
    }
  } else {
    for (int st = 0; st <= max_street; ++st) {
      bucket_thresholds_[st] = kMaxInt;
    }
  }
}
