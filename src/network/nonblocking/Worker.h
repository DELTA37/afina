#ifndef AFINA_NETWORK_NONBLOCKING_WORKER_H
#define AFINA_NETWORK_NONBLOCKING_WORKER_H

#include <sys/signalfd.h>
#include <memory>
#include <pthread.h>
#include <mutex>
#include <atomic>
#include <exception>
#include "Utils.h"
#include "unistd.h"
#include <list>
#include <iostream>
#include <cstring>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <protocol/Parser.h>
#include "Utils.h"
#include <signal.h>
#include <assert.h>
#include <map>
#include <memory>
#include <afina/Executor.h>
#include <afina/execute/Command.h>

#define MAXEVENTS (100)
#define SENDBUFLEN (1000)

namespace Afina {

// Forward declaration, see afina/Storage.h
class Storage;

namespace Network {
namespace NonBlocking {


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
};

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

class EpollManager {
private:
  std::list<Connection> connections;

  int fifo_infd;
  int fifo_outfd;

  int epfd;
  int server_socket;
  sigset_t mask;
  epoll_event events[MAXEVENTS];
  std::shared_ptr<Afina::Storage> ps;
  Worker* self_worker;
public:
  EpollManager(std::shared_ptr<Afina::Storage>& _ps, Worker* _self_worker, int _server_socket) : ps(_ps), self_worker(_self_worker) {
    sigemptyset(&mask);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGINT);
    this->server_socket = _server_socket;
    
    // epoll specification

    if ((this->epfd = epoll_create(MAXEVENTS)) == -1) {
      throw std::runtime_error("epoll_create");
    }

    connections.emplace_back(server_socket);
    connections.back().it = std::next(connections.end(), -1);

    epoll_event ev;
    ev.events = EPOLLEXCLUSIVE | EPOLLHUP | EPOLLIN | EPOLLERR;
    ev.data.ptr = &(connections.back());

    if (epoll_ctl(this->epfd, EPOLL_CTL_ADD, server_socket, &ev) == -1) {
      throw std::runtime_error("epoll_ctl");
    }
    fifo_infd = -1;
    fifo_outfd = -1;
  }

  ~EpollManager(void) {
    close(this->epfd);
  }
  
  void addFIFO(int infd, int outfd=-1) {
    this->fifo_infd = infd;
    this->fifo_outfd = outfd;
  }

  void addConnection(void) {
    int sock;
    if ((sock = accept(this->server_socket, NULL, NULL)) == -1) {
      throw std::runtime_error("accept");
    }
    
    make_socket_non_blocking(sock);

    connections.emplace_back(sock);
    connections.back().it = std::next(connections.end(), -1);

    epoll_event ev;
    ev.events = EPOLLIN | EPOLLHUP | EPOLLRDHUP | EPOLLERR;
    ev.data.ptr = &(connections.back());

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, sock, &ev) == -1) {
      throw std::runtime_error("epoll_ctl");
    }
  }
  void eraseConnection(Connection& con) {
    shutdown(con.fd, SHUT_RDWR);
    close(con.fd);
    this->connections.erase(con.it);
  }
  
  void processConnection(Connection& con, const char* buf, int len, std::string type="Socket") {
    while(len != 0) {
      if (!con.ready) {
        size_t parsed = 0;
        try {
          con.ready = con.parser.Parse(buf, len, parsed);
        } catch(...) {
          con.out.push_back("Server Error");
          con.ready = false;
          con.offset = 0;
          con.body_size = 0;
          if (type == "Socket") {
            this->writeSocket(con);
          } else if (type == "FIFO") {
            this->writeFIFO(con);
          }
          return;
        }
        if (con.ready) {
          con.cmd = std::move(con.parser.Build(con.body_size));
          con.parser.Reset();
          buf += parsed;
          len -= parsed;
          if (con.body_size == 0) {
            std::string _out;
            try {
              std::string args;
              args.assign(con.arg_buf.data(), con.arg_buf.size());
              con.cmd->Execute(*(this->ps), args, _out); 
            } catch(...) {
              _out = "Server Error";
            }
            con.out.push_back(_out);
            con.ready = false;
            con.offset = 0;
            con.body_size = 0;
          } else {
            con.arg_buf.resize(con.body_size + 2);
            con.offset = 0;
          }
        }
      }
      if (con.ready) {
        int parse_len = std::min(len, int(con.body_size + 2 - con.offset));
        std::memcpy(con.arg_buf.data() + con.offset, buf, parse_len);
        con.offset += parse_len;
        buf += parse_len;
        len -= parse_len;
        if (con.offset == con.body_size + 2) {
          std::string _out;
          try {
            std::string args;
            args.assign(con.arg_buf.data(), con.arg_buf.size());
            con.cmd->Execute(*(this->ps), args, _out); 
          } catch(...) {
            _out = "Server Error";
          }
          con.out.push_back(_out);
          con.ready = false;
          con.offset = 0;
          con.body_size = 0;
        }
      }
      if (type == "Socket") {
        this->writeSocket(con);
      } else if (type == "FIFO") {
        this->writeFIFO(con);
      }
    }
  }

  void readSocket(Connection& con) {
    int sock = con.fd;
    char buf[SENDBUFLEN];
    int len;
    while((len = recv(sock, buf, SENDBUFLEN, 0)) > 0) {
      this->processConnection(con, buf, len);
    }
    if (len < 0) {
      eraseConnection(con);
    }
  }

  void writeSocket(Connection& con) {
    int sock = con.fd;
    int c = 1;
    while (c > 0) {
      if (con.out.size() == 0) {
        break;
      }
      if (con.out[0].length() == 0) {
        break;
      }
      c = send(sock, con.out[0].data(), con.out[0].length(), 0);
      if (c < 0) {
        throw std::runtime_error("send");
      }
      con.out[0].erase(0, c);
      if (con.out[0].length() == 0) {
        con.out.erase(con.out.begin());
      }
    }
  }
  
  void readFIFO(Connection& con) {
    // specify for FIFO
  }
  void writeFIFO(Connection& con) {
    // specify for FIFO
  }

  void processEvent() {
    int n;
    if ((n = epoll_wait(epfd, this->events, MAXEVENTS, 10000)) == -1) {
      throw std::runtime_error("epoll_wait");
    }
    for (int i = 0; i < n; ++i) {
      Connection* con_data = reinterpret_cast<Connection*>(events[i].data.ptr);
      int sock = con_data->fd;
      if (sock == server_socket) {
        if ((events[i].events & EPOLLIN) == EPOLLIN) {
          this->addConnection();
        } else {
          throw std::runtime_error("server socket failed");
        }
      } else if (sock == this->fifo_infd) {
        this->readFIFO(*con_data);
        this->writeFIFO(*con_data);
      } else if (sock == this->fifo_outfd) {
        this->writeFIFO(*con_data);
      } else {
        if ((events[i].events & EPOLLIN) == EPOLLIN) {
          this->readSocket(*con_data);
        } else if ((events[i].events & EPOLLOUT) == EPOLLOUT) {
          this->writeSocket(*con_data);
        } else {
          std::cout << "here" << std::endl;
          this->eraseConnection(*con_data);
        }
      }
    }
  }
};


} // namespace NonBlocking
} // namespace Network
} // namespace Afina
#endif // AFINA_NETWORK_NONBLOCKING_WORKER_H
