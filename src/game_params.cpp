#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <memory>

#include "game_params.h"
#include "params.h"

using std::unique_ptr;

unique_ptr<Params> CreateGameParams(void) {
  unique_ptr<Params> params(new Params());
  params->AddParam("GameName", P_STRING);
  params->AddParam("NumRanks", P_INT);
  params->AddParam("NumSuits", P_INT);
  params->AddParam("StackSize", P_INT);
  params->AddParam("MaxStreet", P_INT);
  params->AddParam("NumHoleCards", P_INT);
  params->AddParam("NumFlopCards", P_INT);
  params->AddParam("Ante", P_INT);
  params->AddParam("SmallBlind", P_INT);
  params->AddParam("FirstToAct", P_STRING);
  params->AddParam("BigBlind", P_INT);
  params->AddParam("NumPlayers", P_INT);
  return params;
}
