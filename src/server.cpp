#include <netdb.h> // gethostbyname(), hostent
#include <signal.h>
#include <unistd.h> // gethostname()

#include <memory>
#include <queue>
#include <string>

#include "logging.h"
#include "nb_socket_io.h"
#include "server.h"

using std::queue;
using std::string;
using std::unique_ptr;

Worker::Worker(Server *server) : server_(server) {
  busy_ = false;
}

void Worker::MainLoop(void) {
  queue<NBSocketIO *> *request_queue = server_->GetRequestQueue();
  pthread_mutex_t *queue_mutex = server_->GetQueueMutex();
  pthread_cond_t *queue_not_empty = server_->GetQueueNotEmpty();
  pthread_cond_t *queue_not_full = server_->GetQueueNotFull();

  while (true) {
    pthread_mutex_lock(queue_mutex);

    // Wait while the queue is empty.
    while (request_queue->empty()) {
      pthread_cond_wait(queue_not_empty, queue_mutex);
    }

    NBSocketIO *socket_io = request_queue->front();
    request_queue->pop();

    pthread_cond_signal(queue_not_full);
    pthread_mutex_unlock(queue_mutex);

    // I used to WaitForReadable(1) here.  Don't think I need to now.  With blocking sockets,
    // wanted to "fail fast", rather than waiting for read() to time out.
    busy_ = true;
    server_->HandleRequest(socket_io);
    busy_ = false;
    delete socket_io;
  }
}

static void *worker_thread_run(void *v_w) {
  Worker *w = (Worker *)v_w;
  w->MainLoop();
  return NULL;
}

void Worker::Run(void) {
  pthread_create(&pthread_id_, NULL, worker_thread_run, this);
}

void Worker::Join(void) {
  pthread_join(pthread_id_, NULL);
}

Server::Server(int num_workers, int port) :
  num_workers_(num_workers), port_(port) {
  pthread_mutex_init(&queue_mutex_, NULL);
  pthread_cond_init(&queue_not_empty_, NULL);
  pthread_cond_init(&queue_not_full_, NULL);
  // If we don't do this, then when the server tries to write to a socket that has been closed,
  // the server exits.
  signal(SIGPIPE, SIG_IGN);
}

Server::~Server(void) {
  pthread_mutex_destroy(&queue_mutex_);
  pthread_cond_destroy(&queue_not_empty_);
  pthread_cond_destroy(&queue_not_full_);
  for (int i = 0; i < num_workers_; ++i) {
    delete workers_[i];
  }
}

void Server::SpawnWorkers(void) {
  workers_.reset(new Worker *[num_workers_]);
  for (int i = 0; i < num_workers_; ++i) {
    workers_[i] = new Worker(this);
    if (workers_[i]) workers_[i]->Run();
  }
}

void Server::MainLoop(void) {
  fd_set listen_fds;
  struct timeval tv;

  SpawnWorkers();

  listen_sock_ = GetListenSocket(port_);
  if (listen_sock_ < 0) {
    FatalError("Could not initialize listen_sock_\n");
  }
  launch_time_ = time(NULL);
  launch_timestamp_ = UTCTimestamp();
  num_requests_ = 0;

  // A callback in which to take any actions that should occur before the execution of the main
  // loop.
  PreMainLoop();
  
  Output("Listening for connection on port %i\n", port_);

  while (true) {
    FD_ZERO(&listen_fds);
    FD_SET(listen_sock_, &listen_fds);
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    // Wait for up to 1 second for a connection.
    if (select(listen_sock_ + 1, &listen_fds, NULL, NULL, &tv) == 1) {
      NBSocketIO *socket_io = new NBSocketIO(listen_sock_);
      // Push onto the queue under mutex protection
      pthread_mutex_lock(&queue_mutex_);

      // Wait until there’s room in the queue.
      while (request_queue_.size() == kRequestQueueMaxSize) {
	pthread_cond_wait(&queue_not_full_, &queue_mutex_);
      }
      request_queue_.push(socket_io);
      // Inform waiting threads that queue has a request
      pthread_cond_signal(&queue_not_empty_);

      pthread_mutex_unlock(&queue_mutex_);
    }

    ExecutePeriodicActions();
  }
}
