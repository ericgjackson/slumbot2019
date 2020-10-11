// We assume caller is sensitive to alignment issues.

#include <netdb.h> // gethostbyname(), hostent
#include <netinet/tcp.h> // TCP_NODELAY
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <memory>
#include <string>

#include "logging.h"
#include "socket_io.h"

using std::string;
using std::unique_ptr;

// Note: in the case where no one is listening at hostname:port, this constructor will just hang
// (either forever, or for a long time) because connect() hangs.
SocketIO::SocketIO(const char *hostname, int port) {
  valid_ = false;

  // Create an AF_INET stream socket
  fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (fd_ < 0) {
    Warning("socket() returned %i\n", fd_);
    return;
  }

  int status;
  struct addrinfo hints;
  struct addrinfo *servinfo;  // will point to the results

  memset(&hints, 0, sizeof hints); // make sure the struct is empty
  hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
  hints.ai_flags = AI_PASSIVE;     // fill in my IP for me
  char port_buf[10];
  sprintf(port_buf, "%i", port);

  if ((status = getaddrinfo(hostname, port_buf, &hints, &servinfo)) != 0) {
    Warning("getaddrinfo error: %s\n", gai_strerror(status));
    return;
  }

  // servinfo now points to a linked list of 1 or more struct addrinfos
  // Just use the first one?
  if (servinfo == NULL) {
    Warning("servinfo NULL\n");
    return;
  }

  // Connect
  int rc = connect(fd_, servinfo->ai_addr, servinfo->ai_addrlen);
  if (rc < 0) {
    Warning("Error returned from connect (%s %i): %i; errno %i\n", hostname, port, rc, errno);
    return;
  }

  int v = 1;
  setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, (char *)&v, sizeof(int));

  valid_ = true;
}

SocketIO::SocketIO(int listen_sock) {
  valid_ = false;
  struct sockaddr_in addr;
  socklen_t addr_len = sizeof(addr);
  fd_ = accept(listen_sock, (struct sockaddr *)&addr, &addr_len);
  if (fd_ < 0) {
    Warning("Could not accept connection; accept() returned %i\n", fd_);
    return;
  }

  // This may be very important to reduce latency.  Without this Nagle's
  // algorithm may be used and the kernel may wait for multiple writes
  // before sending.  I see this helping a lot.
  int v = 1;
  setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, (char *)&v, sizeof(int));

  valid_ = true;
}

SocketIO::~SocketIO(void) {
  if (fd_ >= 0) close(fd_);
}

bool SocketIO::WriteInt(int i) const {
  if (write(fd_, (void *)&i, sizeof(int)) != sizeof(int)) {
    return false;
  }
  return true;
}

bool SocketIO::WriteDouble(double d) const {
  if (write(fd_, (void *)&d, sizeof(double)) != sizeof(double)) {
    return false;
  }
  return true;
}

// Write out the length (in text form) prior to the string itself.
// We write out the length as a fixed 10 byte value.
bool SocketIO::WriteMessage(const string &str) const {
  // 10^9 seems like more than enough.
  if (str.size() > kMaxMessageLen) {
    Warning("String size too long: %lli?!?\n", (long long int)str.size());
    return false;
  }
  int len = str.size();
  char buf[100];
  sprintf(buf, "%.10i", len);
  if (write(fd_, (void *)buf, 10) != 10) {
    return false;
  }
  // Each time through this loop we try to write all the remaining bytes to the socket.  But
  // write() may fail to do so.  It may write some positive number of bytes.  It may return -1
  // and set errno to EAGAIN or EWOULDBLOCK.  This is not an error; it just means we need to
  // wait a bit for the socket to clear.  This is only possible for non-blocking sockets, but
  // currently NBSocketIO inherits this method.
  int cum = 0;
  int rem = len;
  int us_delay = 1;
  while (rem > 0) {
    int written = write(fd_, (void *)(str.c_str() + cum), rem);
    if (written <= 0) {
      if (written < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
	Warning("SocketIO:WriteMessage(): write() returned %i errno %i (len %i cum %i rem %i)\n",
		written, errno, len, cum, rem);
	return false;
      }
    }
    if (written > 0) {
      // We succeeded in writing *something* (maybe everything).  Update cum and rem and reset
      // the sleep delay to 1 microsecond.
      cum += written;
      rem -= written;
      us_delay = 1;
    } else {
      // We failed to write anything, but it was not an error; we just need to wait and try
      // again.  We sleep with an exponentially increasing delay to avoid a busy loop.
      // But at some point we have to give up.
      if (us_delay == 8388608) {
	Warning("SocketIO:WriteMessage(): Couldn't write (len %i cum %i rem %i); errno %i\n",
		written, len, cum, rem, errno);
	return false;
      }
      usleep(us_delay);
      // Double the delay each time through this loop.
      us_delay *= 2;
    }
  }
  return true;
}

bool SocketIO::ReadNBytes(int n, char *bytes) const {
  if (n < 0 || n > kMaxMessageLen) {
    Warning("SocketIO::ReadNBytes() bad n %i\n", n);
    return false;
  }
  // Allow n = 0?
  if (n == 0) {
    return true;
  }
  int cum = 0;
  while (true) {
    int left = n - cum;
    ssize_t ret = read(fd_, bytes + cum, left);
    if (ret <= 0) {
      Warning("SocketIO::ReadNBytes: failed to read; ret %i; n %i cum %i left %i\n", (int)ret, n,
	      cum, left);
      return false;
    }
    cum += ret;
    if (cum >= n) break;
  }
  return true;
}

// Expects a 10 byte length value prior to the string.
bool SocketIO::ReadMessage(string *str, int timeout_secs) const {
  *str = "";
  if (! WaitForReadable(timeout_secs)) {
    return false;
  }
  char len_str[11];
  ssize_t ret = read(fd_, len_str, 10);
  if  (ret != 10) {
    // We don't want to spit out warning messages every time someone connects and then
    // doesn't send data in the format we expect.  Monitoring services will do that,
    // people will telnet, etc.
    // Warning("Couldn't read string length str from socket\n");
    return false;
  }
  len_str[10] = 0;
  int len;
  if (sscanf(len_str, "%10d", &len) != 1) {
    Warning("Couldn't parse len_str: \"%s\"\n", len_str);
    return false;
  }
  if (len < 0 || len > kMaxMessageLen) {
    Warning("SocketIO::ReadMessage() bad length %s\n", len_str);
    return false;
  }
  unique_ptr<char []> buf( new char[len + 1]);
  // Do we allow the writing of empty strings?
  if (len == 0) {
    buf[0] = 0;
    return true;
  }
  int cum = 0;
  while (true) {
    int left = len - cum;
    ssize_t ret = read(fd_, buf.get(), left);
    if (ret <= 0) {
      Warning("SocketIO::ReadString: failed to read; ret %i; len %i cum %i left %i\n", (int)ret, len, cum, left);
      if (cum > 0) {
	buf[cum] = 0;
	Warning("  Read so far: %s\n", str->c_str());
      }
      return false;
    }
    buf[ret] = 0;
    *str += buf.get();
    cum += ret;
    if (cum >= len) break;
  }
  return true;
}

bool SocketIO::WriteRaw(const std::string &str) const {
  int len = str.size();
  if (write(fd_, (void *)str.c_str(), len) != len) {
    return false;
  }
  return true;
}

// Don't produce any error or warning messages.  A common scenario is that the other side closes
// the socket and we don't want lots of spurious messages for that.
bool SocketIO::ReadChar(char *c) const {
  ssize_t ret = read(fd_, c, 1);
  if (ret != 1) {
    return false;
  }
  return true;
}

bool SocketIO::ReadInt(int *i) const {
  ssize_t ret = read(fd_, i, sizeof(int));
  if (ret != sizeof(int)) {
    Warning("Failed to read int\n");
    return false;
  }
  return true;
}

bool SocketIO::ReadDouble(double *d) const {
  ssize_t ret = read(fd_, d, sizeof(double));
  if (ret != sizeof(double)) {
    Warning("Failed to read double\n");
    return false;
  }
  return true;
}

// Read a line terminated by \n.  We strip the \n in the string returned.
bool SocketIO::ReadLine(string *str) const {
  *str = "";
  char c;
  while (true) {
    if (! ReadChar(&c)) {
      if (*str != "") {
	// It is common for ReadChar() to fail if the other side closes the socket.  Normally
	// this happens before anything has been read.  It's unusual (although presumably possible)
	// for this to happen when part of a line has been read, so we print a warning in this
	// case.
	Warning("ReadLine: failed with partial line \"%s\"; fd %i\n", str->c_str(), fd_);
      }
      return false;
    }
    *str += c;
    int len = str->size();
    if (len >= 1 && (*str)[len-1] == '\n') {
      str->resize(len - 1);
      return true;
    }
  }
}

// Read an HTTP line.  These lines are terminated by \r\n.  We strip the \r and the \n in the
// string returned.
bool SocketIO::ReadHTTPLine(string *str) const {
  *str = "";
  char c;
  while (true) {
    if (! ReadChar(&c)) {
      if (*str != "") {
	// It is common for ReadChar() to fail if the other side closes the socket.  Normally
	// this happens before anything has been read.  It's unusual (although presumably possible)
	// for this to happen when part of a line has been read, so we print a warning in this
	// case.
	Warning("ReadHTTPLine: failed with partial line \"%s\"; fd %i\n", str->c_str(), fd_);
      }
      return false;
    }
    *str += c;
    // Accept either \r\n or just \n to terminate line
    int len = str->size();
    if (len >= 2 && (*str)[len-2] == '\r' && (*str)[len-1] == '\n') {
      str->resize(len - 2);
      return true;
    } else if (len >= 1 && (*str)[len-1] == '\n') {
      str->resize(len - 1);
      return true;
    }
  }
}

// Uses select() to see if there is anything readable on the socket.
bool SocketIO::WaitForReadable(int secs) const {
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(fd_, &fds);

  struct timeval tv;
  tv.tv_sec = secs;
  tv.tv_usec = 0;
  return (select(fd_ + 1, &fds, NULL, NULL, &tv) == 1);
}

int GetListenSocket(int port) {
  int sock, t;
  struct sockaddr_in addr;

  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    Warning("socket() returned %i.  Is there another program listening on this port?\n", sock);
    return -1;
  }

  /* allow fast socket reuse - ignore failure */
  t = 1;
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &t, sizeof(int));

  /* bind the socket to the port */
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  int ret;
  if ((ret = bind(sock, (struct sockaddr *)&addr, sizeof(addr))) < 0) {
    Warning("bind() returned %i.  Is there another program listening on this port?\n", ret);
    return -1;
  }

  /* listen on the socket */
  if ((ret = listen(sock, 8)) < 0) {
    Warning("listen() returned %i\n", ret);
    return -1;
  }

  return sock;
}
