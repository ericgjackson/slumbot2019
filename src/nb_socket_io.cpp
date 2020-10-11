// Non-blocking socket I/O class
// I won't override the writing routines.  I'll just assume that the write buffer is big enough
// to handle all writes.

#include <fcntl.h> // fcntl()
#include <netdb.h> // gethostbyname(), hostent
#include <netinet/tcp.h> // TCP_NODELAY
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include <memory>
#include <string>

#include "logging.h"
#include "nb_socket_io.h"
#include "socket_io.h"

using std::string;
using std::unique_ptr;

// Client calls this to connect to a server
// Note: in the case where no one is listening at hostname:port, this constructor should fail
// immediately because we have made the socket non-blocking.
NBSocketIO::NBSocketIO(const char *hostname, int port) {
  valid_ = false;

  // Create an AF_INET stream socket
  fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (fd_ < 0) {
    Warning("socket() returned %i\n", fd_);
    return;
  }

  int flags = fcntl(fd_, F_GETFL, 0);
  if (flags == -1) {
    Warning("Couldn't get flags with fcntl(): %i\n", flags);
    return;
  }
  flags |= O_NONBLOCK;
  int ret;
  if ((ret = fcntl(fd_, F_SETFL, flags)) != 0) {
    Warning("Couldn't set flags with fcntl(): %i\n", ret);
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
    if (errno == EINPROGRESS) {
      // This is normal with non-blocking sockets and just indicates that the connection is
      // taking some time.
      fd_set fds;
      FD_ZERO(&fds);
      FD_SET(fd_, &fds);
      struct timeval timeout;
      timeout.tv_sec  = kConnectTimeoutSecs;
      timeout.tv_usec = kConnectTimeoutUSecs;
      // Note that we pass fds in as writefds, not readfds
      int ret = select(fd_ + 1, NULL, &fds, NULL, &timeout);
      if (ret != 1) {
	// Treat timeout or other error the same
	Warning("Non-blocking connect() failed (%s %i): ret %i\n", hostname, port, ret);
	return;
      }
      int optval = -1;
      socklen_t optlen = sizeof(optval);

      if (getsockopt(fd_, SOL_SOCKET, SO_ERROR, &optval, &optlen) == -1) {
	Warning("getsockopt() failed\n");
	return;
      }
      // getsockopt() puts the errno value for connect into optval so 0 means no-error.
      if (optval != 0) {
	Warning("Non-blocking connect() failed (%s %i): optval %s\n", hostname, port,
		strerror(optval));
	return;
      }
    } else {
      Warning("Error returned from connect (%s %i): %i; errno %i\n", hostname, port, rc, errno);
      return;
    }
  }

  freeaddrinfo(servinfo); // free the linked-list

  int v = 1;
  setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, (char *)&v, sizeof(int));

  valid_ = true;
}

// Server calls this when select() detects an incoming connection
// I expect accept() to succeed even though listen_sock is non-blocking
NBSocketIO::NBSocketIO(int listen_sock) {
  valid_ = false;
  struct sockaddr_in addr;
  socklen_t addr_len = sizeof(addr);
  fd_ = accept(listen_sock, (struct sockaddr *)&addr, &addr_len);
  if (fd_ < 0) {
    Warning("Could not accept connection; accept() returned %i\n", fd_);
    return;
  }

  // Make fd_ non-blocking
  int flags = fcntl(fd_, F_GETFL, 0);
  if (flags == -1) {
    Warning("Couldn't get flags with fcntl(): %i\n", flags);
    return;
  }
  flags |= O_NONBLOCK;
  int ret;
  if ((ret = fcntl(fd_, F_SETFL, flags)) != 0) {
    Warning("Couldn't set flags with fcntl(): %i\n", ret);
    return;
  }

  // This may be very important to reduce latency.  Without this Nagle's
  // algorithm may be used and the kernel may wait for multiple writes
  // before sending.  I see this helping a lot.
  int v = 1;
  setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, (char *)&v, sizeof(int));

  valid_ = true;
}

// Two timeouts.  One per read.  Another for the total time allowed to read all bytes.
bool NBSocketIO::ReadNBytes(int n, char *bytes) const {
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(fd_, &fds);
  struct timeval timeout;

  time_t start_t = time(NULL);
  int num_remaining = n;
  char *bytes_ptr = bytes;
  
  while (num_remaining > 0) {
    timeout.tv_sec  = kReadTimeoutSecs;
    timeout.tv_usec = kReadTimeoutUSecs;
    int ret = select(fd_ + 1, &fds, NULL, NULL, &timeout);
    if (ret != 0 && ret != 1) {
      // Typically -1 indicating an error
      return false;
    } else if (ret == 0) {
      // Timeout.  Should I check for EAGAIN or EWOULDBLOCK or some such?
      time_t now = time(NULL);
      double elapsed_secs = difftime(now, start_t);
      if (elapsed_secs > kReadTotalTimeoutSecs) return false;
    } else {
      if (! FD_ISSET(fd_, &fds)) {
	// Not sure how this could happen
	return false;
      }

      // Now we can make a blocking read
      int read_sz = read(fd_, bytes_ptr, num_remaining);
      if (read_sz <= 0) {
	return false;
      }
      if (read_sz > num_remaining) {
	// Really shouldn't happen!
	return false;
      }
      bytes_ptr += read_sz;
      num_remaining -= read_sz;
    }
  }
  
  return true;
}

// Don't produce any error or warning messages.  A common scenario is that the other side closes
// the socket and we don't want lots of spurious messages for that.
bool NBSocketIO::ReadChar(char *c) const {
  return ReadNBytes(1, c);
}

bool NBSocketIO::ReadInt(int *i) const {
  return ReadNBytes(sizeof(int), (char *)i);
}

bool NBSocketIO::ReadDouble(double *d) const {
  return ReadNBytes(sizeof(double), (char *)d);
}

// Expects a 10 byte length value prior to the string.
bool NBSocketIO::ReadMessage(string *str, int timeout_secs) const {
  *str = "";
  if (! WaitForReadable(timeout_secs)) {
    return false;
  }
  char len_str[11];
  if (! ReadNBytes(10, len_str)) {
    return false;
  }
  len_str[10] = 0;
  int len;
  if (sscanf(len_str, "%10d", &len) != 1) {
    // Too many warning messages
    // Warning("Couldn't parse len_str: \"%s\"\n", len_str);
    return false;
  }
  if (len < 0 || len > kMaxMessageLen) {
    Warning("NBSocketIO::ReadMessage() bad length %s\n", len_str);
    return false;
  }
  unique_ptr<char []> buf( new char[len + 1]);
  if (! ReadNBytes(len, buf.get())) {
    return false;
  }
  buf[len] = 0;
  // Needless copy?
  *str = buf.get();
  return true;
}

int GetNBListenSocket(int port) {
  int sock, t;
  struct sockaddr_in addr;

  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    Warning("socket() returned %i.  Is there another program listening on this port?\n", sock);
    return -1;
  }

  int flags = fcntl(sock, F_GETFL, 0);
  if (flags == -1) {
    Warning("Couldn't get flags with fcntl(): %i\n", flags);
    return -1;
  }
  flags |= O_NONBLOCK;
  int ret;
  if ((ret = fcntl(sock, F_SETFL, flags)) != 0) {
    Warning("Couldn't set flags with fcntl(): %i\n", ret);
    return -1;
  }

  /* allow fast socket reuse - ignore failure */
  t = 1;
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &t, sizeof(int));

  /* bind the socket to the port */
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
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
