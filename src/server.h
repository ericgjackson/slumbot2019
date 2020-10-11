#ifndef _SERVER_H_
#define _SERVER_H_

#include <memory>
#include <queue>
#include <string>

class NBSocketIO;
class Server;

class Worker {
public:
  Worker(Server *server);
  virtual ~Worker(void) {}
  virtual void MainLoop(void);
  virtual void Run(void);
  virtual void Join(void);
  bool Busy(void) const {return busy_;}
protected:
  Server *server_;
  bool busy_;
  pthread_t pthread_id_;
};

class Server {
public:
  Server(int num_workers, int port);
  virtual ~Server(void);
  virtual void MainLoop(void);
  virtual bool HandleRequest(NBSocketIO *socket_io) = 0;
  std::queue<NBSocketIO *> *GetRequestQueue(void) {return &request_queue_;}
  pthread_mutex_t *GetQueueMutex(void) {return &queue_mutex_;}
  pthread_cond_t *GetQueueNotEmpty(void) {return &queue_not_empty_;}
  pthread_cond_t *GetQueueNotFull(void) {return &queue_not_full_;}
  virtual const char *ServerName(void) {return "";}
  void IncrementNumRequests(void) {++num_requests_;}
  int GetNumRequests(void) const {return num_requests_;}
protected:
  static const int kRequestQueueMaxSize = 100;
  
  virtual void SpawnWorkers(void);
  virtual void PreMainLoop(void) {}
  virtual void ExecutePeriodicActions(void) {}
  
  int num_workers_;
  int port_;
  int listen_sock_;
  std::unique_ptr<Worker * []> workers_;
  time_t launch_time_;
  std::string launch_timestamp_;
  int num_requests_;
  std::queue<NBSocketIO *> request_queue_;
  pthread_mutex_t queue_mutex_;
  pthread_cond_t queue_not_empty_;
  pthread_cond_t queue_not_full_;
};

std::string GetHostname(void);

#endif
