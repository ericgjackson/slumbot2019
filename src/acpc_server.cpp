#include <stdio.h>
#include <stdlib.h>

#include <memory>
#include <string>

#include "acpc_protocol.h"
#include "acpc_server.h"
#include "agent.h"
#include "logging.h"
#include "match_state.h"
#include "nb_socket_io.h"
#include "server.h"

using std::string;
using std::unique_ptr;

bool ACPCServer::HandleRequest(NBSocketIO *socket_io) {
  string request;
  // I think it suffices to wait for only 1 second; since select() has fired data should be
  // soon available on the socket.
  if (! socket_io->ReadMessage(&request, 1)) {
    // We don't want to spit out warning messages every time someone connects and then
    // doesn't send data in the format we expect.  Monitoring services will do that,
    // people will telnet, etc.
    // Warning("Couldn't read string from socket\n");
    // Use "null" as a generic failure response
    socket_io->WriteMessage("null");
    return false;
  }
  unique_ptr<MatchState> match_state(ParseACPCRequest(request, agent_.BigBlind(),
						      agent_.StackSize()));
  if (! match_state.get()) return false;
  return true;
}

ACPCServer::ACPCServer(int num_workers, int port, const Agent &agent):
  Server(num_workers, port), agent_(agent) {
}
