#ifndef _ACPC_PROTOCOL_H_
#define _ACPC_PROTOCOL_H_

#include <string>

class MatchState;

MatchState *ParseACPCRequest(const std::string &request, int big_blind, int stack_size);

#endif
