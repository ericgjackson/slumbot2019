#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <memory>
#include <string>

#include "acpc_protocol.h"
#include "acpc_server.h"
#include "agent.h"
#include "bot.h"
#include "logging.h"
#include "match_state.h"
#include "nb_socket_io.h"
#include "server.h"

using std::string;
using std::unique_ptr;

Bot::Bot(Agent &agent, const string &hostname, int port) : agent_(agent) {
  socket_io_.reset(new SocketIO(hostname.c_str(), port));
}

void Bot::MainLoop(void) {
  string version = "VERSION:2.0.0\n";
  if (! socket_io_->WriteRaw(version)) {
    fprintf(stderr, "Could not send version string\n");
    exit(-1);
  }
  string line;
  while (true) {
    if (! socket_io_->ReadHTTPLine(&line)) {
      break;
    }
    fprintf(stderr, "-----------------------------------\n");
    fprintf(stderr, "In: %s\n", line.c_str());
    unique_ptr<MatchState> match_state(ParseACPCRequest(line, agent_.BigBlind(),
							agent_.StackSize()));
    if (! match_state.get()) {
      fprintf(stderr, "Couldn't parse match state\n");
      exit(-1);
    }
    if (match_state->Terminal()) continue;
    bool call, fold;
    int bet_size;
    if (agent_.ProcessMatchState(*match_state, nullptr, &call, &fold, &bet_size)) {
      fprintf(stderr, "ProcessMatchState returned true\n");
      char buf[100];
      if (call) {
	strcpy(buf, "c");
      } else if (fold) {
	strcpy(buf, "f");
      } else {
	sprintf(buf, "r%i", bet_size);
      }
      string new_line = line + ':' + buf;
      fprintf(stderr, "%s\n", new_line.c_str());
      new_line += "\r\n";
      if (! socket_io_->WriteRaw(new_line)) {
	fprintf(stderr, "WriteRaw() returned false\n");
	exit(-1);
      }
    } else {
      fprintf(stderr, "ProcessMatchState returned false\n");
    }
  }
}
