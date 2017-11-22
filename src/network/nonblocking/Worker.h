#ifndef AFINA_NETWORK_NONBLOCKING_WORKER_H
#define AFINA_NETWORK_NONBLOCKING_WORKER_H

#include <assert.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/signalfd.h>
#include <errno.h>

#include <memory>
#include <mutex>
#include <atomic>
#include <exception>
#include <list>
#include <iostream>
#include <cstring>
#include <map>
#include <memory>

#include <protocol/Parser.h>
#include <afina/Executor.h>
#include <afina/execute/Command.h>
#include "Utils.h"

#define MAXEVENTS (100)
#define SENDBUFLEN (1000)

namespace Afina {

// Forward declaration, see afina/Storage.h
class Storage;

namespace Network {
namespace NonBlocking {

struct Connection {
  Connection(int _fd) : fd(_fd), parser(), ready(false), offset(0), body_size(0), out() {
    out.resize(0);
  }
  ~Connection(void) {}
  int fd;

  Protocol::Parser parser;
  bool ready;

  std::vector<char> arg_buf;
  ssize_t offset;
  uint32_t body_size;
  std::unique_ptr<Execute::Command> cmd;

  std::vector<std::string> out;
  std::list<Connection>::iterator it;
};



/**
 * # Thread running epoll
 * On Start spaws background thread that is doing epoll on the given server
 * socket and process incoming connections and its data
 */
class Worker {
public:
    Worker(std::shared_ptr<Afina::Storage> ps);
    ~Worker();
    Worker(const Worker& q) {this->ps = q.ps;}

    /**
     * Spaws new background thread that is doing epoll on the given server
     * socket. Once connection accepted it must be registered and being processed
     * on this thread
     */
    void Start(int server_socket);

    /**
     * Signal background thread to stop. After that signal thread must stop to
     * accept new connections and must stop read new commands from existing. Once
     * all readed commands are executed and results are send back to client, thread
     * must stop
     */
    void Stop();

    /**
     * Blocks calling thread until background one for this worker is actually
     * been destoryed
     */
    void Join();
    
    void processEvent(void);
    
    void processConnection(Connection& con, const char* buf, int len, void (Worker::*write_fn)(Connection& con)); 
    void addConnection(int fd, bool readonly=false, bool et=false);
    void eraseConnection(Connection& con);

    void readSocket(Connection& con);
    void writeSocket(Connection& con);
    
    void freeFIFO(Connection& con);
    void readFIFO(Connection& con);
    void writeFIFO(Connection& con);

    void enableFIFO(const std::string& rfifo, const std::string& wfifo);
    void disableFIFO();
protected:
    /**
     * Method executing by background thread
     */
    void OnRun(int server_socket);
    static void* RunProxy(void* args);
    static void cleanup_worker(void* args);

private:
    pthread_t thread;
    std::atomic<bool> running;
    std::shared_ptr<Afina::Storage> ps;

    int epfd;
    int server_socket;
    epoll_event events[MAXEVENTS];
    std::list<Connection> connections;

    std::string rfifo_name;
    std::string wfifo_name;
    int rfifo_fd;
    int wfifo_fd;
};


} // namespace NonBlocking
} // namespace Network
} // namespace Afina
#endif // AFINA_NETWORK_NONBLOCKING_WORKER_H
