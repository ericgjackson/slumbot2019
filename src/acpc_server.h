#ifndef _ACPC_SERVER_H_
#define _ACPC_SERVER_H_

#include <memory>

#include "agent.h"
#include "server.h"

class NBSocketIO;

class ACPCServer : public Server {
 public:
  ACPCServer(int num_workers, int port, const Agent &agent);
  ~ACPCServer(void) {}
  bool HandleRequest(NBSocketIO *socket_io);
 private:
  const Agent &agent_;
};

#endif
