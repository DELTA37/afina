#include "Worker.h"

#include <iostream>

#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <protocol/Parser.h>
#include "Utils.h"
#include <map>
#include <memory>
namespace Afina {
namespace Network {
namespace NonBlocking {

#define MAXEVENTS (100)

// See Worker.h
Worker::Worker(std::shared_ptr<Afina::Storage> _ps) : ps(_ps) {}

// See Worker.h
Worker::~Worker() {}

void* Worker::RunProxy(void* _args) {
  auto args = reinterpret_cast<std::pair<Worker*, int>*>(_args);
  Worker* worker_instance = args->first;
  int server_socket = args->second;
  worker_instance->OnRun(server_socket);
  return NULL;
}

// See Worker.h
void Worker::Start(int server_socket) {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    auto args = std::make_pair(this, server_socket);
    if (pthread_create(&thread, NULL, Worker::RunProxy, &args) != 0) {
      throw std::runtime_error("cannot create a thread");
    }
}

// See Worker.h
void Worker::Stop() {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    running.store(false);
}

// See Worker.h
void Worker::Join() {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    pthread_join(this->thread, NULL);
}

void Worker::cleanup_worker(void* _args) {
  auto args = *(reinterpret_cast<std::tuple<Worker*, int>*>(_args));
  Worker* worker_instance = std::get<0>(args);
  int epfd = std::get<1>(args);
  close(epfd);
}

// See Worker.h
void Worker::OnRun(int server_socket) {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;

    // TODO: implementation here
    // 1. Create epoll_context here
    // 2. Add server_socket to context
    // 3. Accept new connections, don't forget to call make_socket_nonblocking on
    //    the client socket descriptor
    // 4. Add connections to the local context
    // 5. Process connection events
    //
    // Do not forget to use EPOLLEXCLUSIVE flag when register socket
    // for events to avoid thundering herd type behavior.
    std::map<int, std::string> coms;
    std::map<int, std::shared_ptr<Execute::Command>> com;
    std::map<int, Protocol::Parser> parsers;
    std::map<int, int> body_sizes;
    int sendbuf_len = 1000;
    int epfd = epoll_create(2);
    if (epfd == -1) {
      throw std::runtime_error("cannot epoll_create");
    }
    epoll_event ev;
    epoll_event events[MAXEVENTS];

    ev.events = EPOLLEXCLUSIVE | EPOLLHUP | EPOLLIN | EPOLLERR;
    ev.data.fd = server_socket; 

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, server_socket, &ev) == -1) {
      throw std::runtime_error("cannot epoll_ctl");
    }

    auto args = std::make_tuple(this, server_socket);
    pthread_cleanup_push(Worker::cleanup_worker, &args);
     
    while(running.load()) {
      int n = epoll_wait(epfd, events, MAXEVENTS, -1);
      if (n == -1) {
        throw std::runtime_error("cannot epoll_wait");
      }
      for (int i = 0; i < n; i++) {
        if (events[i].data.fd == server_socket) {
          if (events[i].events & EPOLLIN) {
            int sock = accept(server_socket, NULL, NULL);
            make_socket_non_blocking(sock);
            ev.events = EPOLLIN | EPOLLERR;
            ev.data.fd = sock;
            if (epoll_ctl(epfd, EPOLL_CTL_ADD, sock, &ev) == -1) {
              throw std::runtime_error("cannot epoll_ctl");
            }
          } else if ((events[i].events & EPOLLERR) && (events[i].events & EPOLLHUP))  {
            pthread_exit(NULL);
          }
        } else {
          int sock = events[i].data.fd;
          if (events[i].events & EPOLLERR == EPOLLERR) {
            epoll_ctl(epfd, EPOLL_CTL_DEL, sock, NULL);
            close(sock);
            parsers.erase(sock);
            coms.erase(sock);
            body_sizes.erase(sock);
          } else if (events[i].events & EPOLLIN == EPOLLIN) {
            char buf[sendbuf_len + 1];
            ssize_t s;
            size_t parsed; 
            uint32_t body_size;
            std::string out("");

            if ((s = recv(sock, buf, sendbuf_len, 0)) < 0) {
              epoll_ctl(epfd, EPOLL_CTL_DEL, sock, NULL);
              close(sock);
              parsers.erase(sock);
              coms.erase(sock);
              body_sizes.erase(sock);
            } else {
              buf[s] = '\0';
              auto diff = std::string(buf);
              coms[sock] += diff;
              if (body_sizes.count(sock) && (diff.length() >= body_sizes[sock])) {
                std::string args;
                std::copy(coms[sock].begin(), coms[sock].begin() + body_size, std::back_inserter(args));
                coms[sock].erase(0, body_sizes[sock]);
                try {
                  //com[sock]->Execute(*(this->ps), args, out);
                } catch (...) {
                  out = "Server Error";
                }
                if (send(sock, out.data(), out.size(), 0) <= 0) {
                  epoll_ctl(epfd, EPOLL_CTL_DEL, sock, NULL);
                  close(sock);
                  parsers.erase(sock);
                  coms.erase(sock);
                  body_sizes.erase(sock);
                }
                body_sizes.erase(sock);
                com.erase(sock);
              } else if (!parsers[sock].Parse(coms[sock].data(), s, parsed)) {
                coms[sock].erase(0, parsed);
              } else {
                auto _com = parsers[sock].Build(body_size);
                //com[sock] = std::make_shared<Afina::Execute::Command>(std::move(_com));
                body_sizes[sock] = body_size;
                if (body_size > 0) {
                  coms[sock].erase(0, parsed);
                  body_sizes[sock] = body_size;
                } else {
                  std::string args;
                  std::copy(coms[sock].begin(), coms[sock].begin() + body_size, std::back_inserter(args));
                  coms[sock].erase(0, body_size);
                  try {
                    //com[sock]->Execute(*(this->ps), args, out);
                  } catch (...) {
                    out = "Server Error";
                  }
                  if (send(sock, out.data(), out.size(), 0) <= 0) {
                    epoll_ctl(epfd, EPOLL_CTL_DEL, sock, NULL);
                    close(sock);
                    parsers.erase(sock);
                    coms.erase(sock);
                    body_sizes.erase(sock);
                    com.erase(sock);
                  }
                  body_sizes.erase(sock);
                  com.erase(sock);
                }
              }
            }
          }
        }
      }
    }
    pthread_cleanup_pop(1);
    pthread_exit(NULL);
}

} // namespace NonBlocking
} // namespace Network
} // namespace Afina
