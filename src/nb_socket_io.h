#ifndef _NB_SOCKET_IO_H_
#define _NB_SOCKET_IO_H_

#include <string>

#include "socket_io.h"

class NBSocketIO : public SocketIO {
public:
  NBSocketIO(const char *hostname, int port);
  NBSocketIO(int listen_sock);
  ~NBSocketIO(void) {}
  bool ReadChar(char *c) const;
  bool ReadInt(int *i) const;
  bool ReadDouble(double *d) const;
  bool ReadMessage(std::string *str, int timeout_secs) const;
  bool ReadNBytes(int n, char *bytes) const;
 private:
  static const int kConnectTimeoutSecs = 1;
  static const int kConnectTimeoutUSecs = 0;
  static const int kReadTotalTimeoutSecs = 10;
  static const int kReadTimeoutSecs = 1;
  static const int kReadTimeoutUSecs = 0;
};

int GetNBListenSocket(int port);

#endif
