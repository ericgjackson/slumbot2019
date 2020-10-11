#ifndef _SOCKET_IO_H_
#define _SOCKET_IO_H_

#include <string>

class SocketIO {
public:
  SocketIO(const char *hostname, int port);
  SocketIO(int listen_sock);
  virtual ~SocketIO(void);
  bool Valid(void) const {return valid_;}
  // For communicating with backends we use a simple TCP protocol in which messages are prefixed
  // with their length.
  bool WriteMessage(const std::string &str) const;
  bool WriteInt(int i) const;
  bool WriteDouble(double d) const;
  // Doesn't use our protocol
  bool WriteRaw(const std::string &str) const;
  virtual bool ReadChar(char *c) const;
  virtual bool ReadInt(int *i) const;
  virtual bool ReadDouble(double *d) const;
  virtual bool ReadMessage(std::string *str, int timeout_secs) const;
  virtual bool ReadNBytes(int n, char *bytes) const;
  bool ReadLine(std::string *str) const;
  bool ReadHTTPLine(std::string *str) const;
  bool WaitForReadable(int secs) const;
  int FD(void) const {return fd_;}
 protected:
  static const int kMaxMessageLen = 1000000000;

  // Only for use by NBSocketIO constructor
  SocketIO(void) {}

  bool valid_;
  int fd_;
};

int GetListenSocket(int port);

#endif
