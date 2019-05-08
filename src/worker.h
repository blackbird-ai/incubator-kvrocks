#pragma once

#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <thread>
#include <string>
#include <vector>

#include "storage.h"
#include "replication.h"

class Server;
// forward declare
namespace Redis {
class Request;
class Connection;
}

class Worker {
  friend Redis::Request;

 public:
  Worker(Server *svr, Config *config, bool repl = false);
  ~Worker();
  Worker(const Worker &) = delete;
  Worker(Worker &&) = delete;
  void Stop();
  void Run(std::thread::id tid);
  void RemoveConnection(int fd);
  void RemoveConnectionByID(int fd, uint64_t id);
  Status AddConnection(Redis::Connection *c);
  Status EnableWriteEvent(int fd);
  Status Reply(int fd, const std::string &reply);
  bool IsRepl() { return repl_; }
  void BecomeMonitorConn(Redis::Connection *conn);
  void FeedMonitorConns(Redis::Connection *conn, const std::vector<std::string> &tokens);

  std::string GetClientsStr();
  void KillClient(Redis::Connection *self, uint64_t id, std::string addr, bool skipme, int64_t *killed);
  void KickoutIdleClients(int timeout);

  Server *svr_;

 private:
  Status listen(const std::string &host, int port, int backlog);
  static void newConnection(evconnlistener *listener, evutil_socket_t fd,
                            sockaddr *address, int socklen, void *ctx);
  static void TimerCB(int, int16_t events, void *ctx);

  event_base *base_;
  event *timer_;
  std::thread::id tid_;
  std::vector<evconnlistener*> listen_events_;
  std::mutex conns_mu_;
  std::map<int, Redis::Connection*> conns_;
  std::map<int, Redis::Connection*> monitor_conns_;
  int last_iter_conn_fd = 0;   // fd of last processed connection in previous cron

  bool repl_;
};

class WorkerThread {
 public:
  explicit WorkerThread(Worker *worker) : worker_(worker) {}
  ~WorkerThread() { delete worker_; }
  WorkerThread(const WorkerThread&) = delete;
  WorkerThread(WorkerThread&&) = delete;
  Worker *GetWorker() { return worker_; }
  void Start();
  void Stop();
  void Join();

  std::string GetClientsStr();
  void KillClient(int64_t *killed, std::string addr, uint64_t id, bool skipme, Redis::Connection *conn);

 private:
  std::thread t_;
  Worker *worker_;
};
