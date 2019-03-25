#include <stdio.h>
#include <stdlib.h>

#include <memory>

#include "cfr_params.h"
#include "params.h"

using std::unique_ptr;

unique_ptr<Params> CreateCFRParams(void) {
  unique_ptr<Params> params(new Params());
  params->AddParam("CFRConfigName", P_STRING);
  params->AddParam("Algorithm", P_STRING);
  params->AddParam("NNR", P_BOOLEAN);
  params->AddParam("RegretFloors", P_STRING);
  params->AddParam("RegretCeilings", P_STRING);
  params->AddParam("SumprobCeilings", P_STRING);
  params->AddParam("RegretScaling", P_STRING);
  params->AddParam("SumprobScaling", P_STRING);
  params->AddParam("BootstrapIt", P_INT);
  params->AddParam("BootstrapStreets", P_STRING);
  params->AddParam("SoftWarmup", P_INT);
  params->AddParam("HardWarmup", P_INT);
  params->AddParam("SubgameStreet", P_INT);
  params->AddParam("OverweightingFactor", P_INT);
  params->AddParam("SamplingRate", P_INT);
  params->AddParam("SumprobStreets", P_STRING);
  params->AddParam("PruningThresholds", P_STRING);
  params->AddParam("HVBTable", P_BOOLEAN);
  params->AddParam("FTL", P_BOOLEAN);
  params->AddParam("SampleOppHands", P_BOOLEAN);
  params->AddParam("Explore", P_DOUBLE);
  params->AddParam("Probe", P_BOOLEAN);
  params->AddParam("DoubleRegrets", P_BOOLEAN);
  params->AddParam("DoubleSumprobs", P_BOOLEAN);
  params->AddParam("CompressedStreets", P_STRING);
  params->AddParam("CloseThreshold", P_INT);
  params->AddParam("ActiveMod", P_INT);
  params->AddParam("ActiveConditions", P_STRING);
  params->AddParam("Uniform", P_BOOLEAN);
  params->AddParam("CharQuantizedStreets", P_STRING);
  params->AddParam("ShortQuantizedStreets", P_STRING);
  params->AddParam("ScaledStreets", P_STRING);
  params->AddParam("DealTwice", P_BOOLEAN);
  params->AddParam("BoostThresholds", P_STRING);
  params->AddParam("Freeze", P_STRING);

  return params;
}
