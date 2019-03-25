#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <memory>

#include "betting_abstraction_params.h"
#include "params.h"

using std::unique_ptr;

unique_ptr<Params> CreateBettingAbstractionParams(void) {
  unique_ptr<Params> params(new Params());
  params->AddParam("BettingAbstractionName", P_STRING);
  params->AddParam("Limit", P_BOOLEAN);
  params->AddParam("StackSize", P_INT);
  params->AddParam("MaxBets", P_STRING);
  params->AddParam("OurMaxBets", P_STRING);
  params->AddParam("OppMaxBets", P_STRING);
  params->AddParam("BetSizes", P_STRING);
  params->AddParam("OurBetSizes", P_STRING);
  params->AddParam("OppBetSizes", P_STRING);
  params->AddParam("MinBet", P_INT);
  params->AddParam("AllBetSizeStreets", P_STRING);
  params->AddParam("AllEvenBetSizeStreets", P_STRING);
  params->AddParam("NoLimitTreeType", P_INT);
  params->AddParam("InitialStreet", P_INT);
  params->AddParam("Asymmetric", P_BOOLEAN);
  params->AddParam("AlwaysAllIn", P_BOOLEAN);
  params->AddParam("OurAlwaysAllIn", P_BOOLEAN);
  params->AddParam("OppAlwaysAllIn", P_BOOLEAN);
  params->AddParam("MinBets", P_STRING);
  params->AddParam("OurMinBets", P_STRING);
  params->AddParam("OppMinBets", P_STRING);
  params->AddParam("MinAllInPot", P_INT);
  params->AddParam("NoOpenLimp", P_BOOLEAN);
  params->AddParam("OurNoOpenLimp", P_BOOLEAN);
  params->AddParam("OppNoOpenLimp", P_BOOLEAN);
  params->AddParam("NoRegularBetThreshold", P_INT);
  params->AddParam("OurNoRegularBetThreshold", P_INT);
  params->AddParam("OppNoRegularBetThreshold", P_INT);
  params->AddParam("OnlyPotThreshold", P_INT);
  params->AddParam("OurOnlyPotThreshold", P_INT);
  params->AddParam("OppOnlyPotThreshold", P_INT);
  params->AddParam("GeometricType", P_INT);
  params->AddParam("OurGeometricType", P_INT);
  params->AddParam("OppGeometricType", P_INT);
  params->AddParam("CloseToAllInFrac", P_DOUBLE);
  params->AddParam("OurBetSizeMultipliers", P_STRING);
  params->AddParam("OppBetSizeMultipliers", P_STRING);
  params->AddParam("ReentrantStreets", P_STRING);
  params->AddParam("BettingKeyStreets", P_STRING);
  params->AddParam("MinReentrantPot", P_INT);
  params->AddParam("MergeRules", P_STRING);
  params->AddParam("AllowableBetTos", P_STRING);
  params->AddParam("LastAggressorKey", P_BOOLEAN);

  return params;
}
