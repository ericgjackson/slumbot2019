#ifndef _BOT_H_
#define _BOT_H_

#include <memory>
#include <string>

#include "agent.h"
#include "socket_io.h"

class Bot {
public:
  Bot(Agent &agent, const std::string &hostname, int port);
  ~Bot(void) {}
  void MainLoop(void);
private:
  Agent &agent_;
  std::unique_ptr<SocketIO> socket_io_;
};

#endif
