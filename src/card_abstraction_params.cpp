#include <stdio.h>
#include <stdlib.h>

#include <memory>

#include "card_abstraction_params.h"
#include "params.h"

using std::unique_ptr;

unique_ptr<Params> CreateCardAbstractionParams(void) {
  unique_ptr<Params> params(new Params());
  params->AddParam("CardAbstractionName", P_STRING);
  params->AddParam("Bucketings", P_STRING);
  params->AddParam("BucketThresholds", P_STRING);
  return params;
}
