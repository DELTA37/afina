#include "Worker.h"

#include <iostream>

#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <protocol/Parser.h>
#include "Utils.h"
#include <map>
#include <memory>
#include <afina/Executor.h>
#include <afina/execute/Command.h>
namespace Afina {
namespace Network {
namespace NonBlocking {

#define MAXEVENTS (100)
#define SENDBUFLEN (1000)
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
  delete args;
}

// See Worker.h
void Worker::Start(int server_socket) {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    auto args = new std::pair<Worker*, int>(this, server_socket);
    if (pthread_create(&thread, NULL, Worker::RunProxy, args) != 0) {
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
  auto args = reinterpret_cast<std::tuple<Worker*, int>*>(_args);
  Worker* worker_instance = std::get<0>(*args);
  int epfd = std::get<1>(*args);
  close(epfd);
  delete args;
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
    std::map<int, std::string> text;
    std::map<int, std::unique_ptr<Execute::Command>> com;
    std::map<int, Protocol::Parser> parser;
    std::map<int, uint32_t> body_size;

    int sendbuf_len = SENDBUFLEN;
    int epfd = epoll_create(MAXEVENTS);
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

    auto args = new std::tuple<Worker*, int>(this, server_socket);
    pthread_cleanup_push(Worker::cleanup_worker, args);
     
    while(running.load()) {
      std::cout << "start wait" << std::endl;
      int n = epoll_wait(epfd, events, MAXEVENTS, -1);
      std::cout << "end wait" << std::endl;
      if (n == -1) {
        throw std::runtime_error("cannot epoll_wait");
      }
      for (int i = 0; i < n; i++) {
        if (events[i].data.fd == server_socket) {
          if ((events[i].events & EPOLLIN) == EPOLLIN) {
            int sock = accept(server_socket, NULL, NULL);
            make_socket_non_blocking(sock);
            //ev.events = EPOLLIN | EPOLLERR | EPOLLHUP;
            ev.events = EPOLLIN | EPOLLHUP;
            ev.data.fd = sock;
            if (epoll_ctl(epfd, EPOLL_CTL_ADD, sock, &ev) == -1) {
              throw std::runtime_error("cannot epoll_ctl");
            }
            parser.emplace(sock, Protocol::Parser());
          } else if (((events[i].events & EPOLLERR) == EPOLLERR) && ((events[i].events & EPOLLHUP) == EPOLLHUP))  {
            pthread_exit(NULL);
          }
        } else {
          int sock = events[i].data.fd;
          if ((events[i].events & EPOLLIN) == EPOLLIN) {
            size_t sendbuf_len = 100;
            ssize_t s;
            char buf[sendbuf_len + 1];
            
            if ((s = recv(sock, buf, sendbuf_len, 0)) < 0) {
              epoll_ctl(epfd, EPOLL_CTL_DEL, sock, NULL);
              close(sock);
              parser.erase(sock);
              com.erase(sock);
              body_size.erase(sock);
              text.erase(sock);
            } else {
              buf[s] = '\0';
              std::string diff = std::string(buf);
              text[sock] += diff;
              
              bool waiting = false;

              while((text[sock].length() > 2) && (not waiting)) {
                if (body_size.find(sock) == body_size.end()) {
                  size_t parsed_len;
                  try {
                    if (!parser[sock].Parse(text[sock].data(), text[sock].length(), parsed_len)) {
                      text[sock].erase(0, parsed_len);
                    } else {
                      text[sock].erase(0, parsed_len);
                      uint32_t _body_size;
                      auto _com = parser[sock].Build(_body_size);
                      com.emplace(sock, std::move(_com));
                      body_size.emplace(sock, _body_size);
                    }
                  } catch(...) {
                    std::string out = "Server Error";
                    if (send(sock, out.data(), out.size(), 0) <= 0) {
                      epoll_ctl(epfd, EPOLL_CTL_DEL, sock, NULL);
                      close(sock);
                      parser.erase(sock);
                      com.erase(sock);
                      body_size.erase(sock);
                      text.erase(sock);
                      break;
                    }
                    text[sock] = "";
                  }
                }

                if (body_size.find(sock) != body_size.end()) {
                  if (text[sock].length() >= body_size[sock]) {
                    std::string out; 
                    std::string args;
                    std::copy(text[sock].begin(), text[sock].begin() + body_size[sock], std::back_inserter(args));
                    text[sock].erase(0, body_size[sock]);

                    try {
                      com[sock]->Execute(*(this->ps), args, out);
                    } catch (...) {
                      out = "Server Error";
                    }

                    if (send(sock, out.data(), out.size(), 0) <= 0) {
                      epoll_ctl(epfd, EPOLL_CTL_DEL, sock, NULL);
                      close(sock);
                      parser.erase(sock);
                      com.erase(sock);
                      body_size.erase(sock);
                      text.erase(sock);
                      break;
                    }
                    body_size.erase(sock);
                    com.erase(sock);
                    waiting = false;
                  } else {
                    waiting = true;
                  }
                }
              } // while(processing client)
            } // recv
          } else {
            std::cout << "here" << std::endl;
            epoll_ctl(epfd, EPOLL_CTL_DEL, sock, NULL);
            close(sock);
            parser.erase(sock);
            text.erase(sock);
            com.erase(sock);
            body_size.erase(sock);
          }
        } // client socket
      } // for (events)
    } // while(running)
    pthread_cleanup_pop(1);
    pthread_exit(NULL);
} // fundtion

} // namespace NonBlocking
} // namespace Network
} // namespace Afina
