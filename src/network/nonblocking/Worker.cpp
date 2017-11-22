#include "Worker.h"

namespace Afina {
namespace Network {
namespace NonBlocking {

// See Worker.h
Worker::Worker(std::shared_ptr<Afina::Storage> _ps) : ps(_ps) {}

// See Worker.h
Worker::~Worker() {
  this->disableFIFO();
}

void* Worker::RunProxy(void* _args) {
  auto args = reinterpret_cast<std::pair<Worker*, int>*>(_args);
  Worker* worker_instance = args->first;
  int server_socket = args->second;
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGINT);
  sigaddset(&mask, SIGTERM);
  if (pthread_sigmask(SIG_BLOCK, &mask, NULL) == -1) {
    throw std::runtime_error("pthread_sigmask");
  }
  worker_instance->OnRun(server_socket);
  return NULL;
}

// See Worker.h
void Worker::Start(int server_socket) {
  std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
  this->running.store(true);
  auto args = new std::pair<Worker*, int>(this, server_socket);
  if (pthread_create(&(this->thread), NULL, &(Worker::RunProxy), args) != 0) {
    throw std::runtime_error("cannot create a thread");
  }
}

// See Worker.h
void Worker::Stop() {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    this->running.store(false);
}

// See Worker.h
void Worker::Join() {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    pthread_join(this->thread, NULL);
}

void Worker::processEvent() {
  int n;
  if ((n = epoll_wait(epfd, this->events, MAXEVENTS, 10000)) == -1) {
    throw std::runtime_error("epoll_wait");
  }
  for (int i = 0; i < n; ++i) {
    Connection* con_data = reinterpret_cast<Connection*>(events[i].data.ptr);
    int sock = con_data->fd;
    if (sock == server_socket) {
      if ((events[i].events & EPOLLIN) == EPOLLIN) {
        int sock;
        if ((sock = accept(this->server_socket, NULL, NULL)) == -1) {
          throw std::runtime_error("accept");
        }
        make_socket_non_blocking(sock);
        this->addConnection(sock);
      } else {
        throw std::runtime_error("server socket failed");
      }
    } else if (sock == this->rfifo_fd){
      this->readFIFO(*con_data);
    } else if (sock == this->wfifo_fd) {
      if ((events[i].events & EPOLLIN) == EPOLLIN) {
        this->freeFIFO(*con_data);
      } else {
        this->writeFIFO(*con_data);
      }
    } else {
      if ((events[i].events & EPOLLIN) == EPOLLIN) {
        this->readSocket(*con_data);
      } else if ((events[i].events & EPOLLOUT) == EPOLLOUT) {
        this->writeSocket(*con_data);
      } else {
        this->eraseConnection(*con_data);
      }
    }
  }
}

void Worker::addConnection(int fd, bool readonly, bool et) {
  this->connections.emplace_back(fd);
  this->connections.back().it = std::next(connections.end(), -1);

  epoll_event ev;
  ev.events = EPOLLEXCLUSIVE | EPOLLHUP | EPOLLIN | EPOLLERR;

  if (!readonly) {
    ev.events |= EPOLLOUT;
  }
  if (et) {
    ev.events |= EPOLLET;
  }
  ev.data.ptr = &(this->connections.back());

  if (epoll_ctl(this->epfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
    throw std::runtime_error(std::string(std::strerror(errno)) + " in epoll_ctl");
  }
}

void Worker::eraseConnection(Connection& con) {
  shutdown(con.fd, SHUT_RDWR);
  close(con.fd);
  this->connections.erase(con.it);
}


void Worker::processConnection(Connection& con, const char* buf, int len, void (Worker::*write_fn)(Connection& con)) {
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
        (this->*write_fn)(con);
        return;
      }
      buf += parsed;
      len -= parsed;
      if (con.ready) {
        con.cmd = std::move(con.parser.Build(con.body_size));
        con.parser.Reset();
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
    (this->*write_fn)(con);
  }
}


void Worker::readSocket(Connection& con) {
  int sock = con.fd;
  char buf[SENDBUFLEN];
  int len;
  while((len = recv(sock, buf, SENDBUFLEN, 0)) > 0) {
    this->processConnection(con, buf, len, &Worker::writeSocket);
  }
  if (len < 0) {
    this->eraseConnection(con);
  }
}

void Worker::writeSocket(Connection& con) {
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
      this->eraseConnection(con);
      break;
    }
    con.out[0].erase(0, c);
    if (con.out[0].length() == 0) {
      con.out.erase(con.out.begin());
    }
  }
}

void Worker::readFIFO(Connection& con) {
  char buf[SENDBUFLEN];
  int len;
  while((len = read(this->rfifo_fd, buf, SENDBUFLEN)) > 0) {
    processConnection(con, buf, len, &Worker::writeFIFO);
  }
  if (len < 0) {
    con.parser.Reset();
    con.body_size = 0;
    con.offset = 0;
  }
}

void Worker::writeFIFO(Connection& con) {
  if (this->wfifo_fd == -1) {
    con.out.clear();
    return;
  }
  int c = 1;
  while (c > 0) {
    if (con.out.size() == 0) {
      break;
    }
    if (con.out[0].length() == 0) {
      break;
    }
    c = write(this->wfifo_fd, con.out[0].data(), con.out[0].length());
    if (c < 0) {
      con.out.clear();
      break;
    }
    con.out[0].erase(0, c);
    if (con.out[0].length() == 0) {
      con.out.erase(con.out.begin());
    }
  }
}

void Worker::freeFIFO(Connection& con) {
  char buf[SENDBUFLEN];
  int len;
  while((len = read(this->rfifo_fd, buf, SENDBUFLEN)) > 0) {}
}

void Worker::enableFIFO(const std::string& rfifo, const std::string& wfifo) {
  this->rfifo_name = rfifo;
  this->wfifo_name = wfifo;
  if (rfifo != "") {
    if (mkfifo(rfifo.c_str(), 0666) == -1) {
      throw std::runtime_error("mkfifo rfifo");
    }
    this->rfifo_fd = open(rfifo.c_str(), O_RDONLY | O_NONBLOCK);
    if (this->rfifo_fd == -1) {
      throw std::runtime_error("open rfifo");
    }
    this->addConnection(this->rfifo_fd, true, true);
  }
  std::cout << "here2" << std::endl;
  if (wfifo != "") {
    if (mkfifo(wfifo.c_str(), 0666) == -1) {
      throw std::runtime_error("mkfifo wfifo");
    }
    this->wfifo_fd = open(wfifo.c_str(), O_RDWR | O_NONBLOCK);
    if (this->wfifo_fd == -1) {
      throw std::runtime_error("open wfifo");
    }

    this->addConnection(this->wfifo_fd, false, true);
  }
  
}

void Worker::disableFIFO(void) {
  if (rfifo_name != "") {
    close(this->rfifo_fd);
    unlink(this->rfifo_name.c_str());
  }
  if (wfifo_name != "") {
    close(this->wfifo_fd);
    unlink(this->rfifo_name.c_str());
  }
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
  
  if ((this->epfd = epoll_create(MAXEVENTS)) == -1) {
    throw std::runtime_error("epoll_create");
  }

  this->server_socket = server_socket;
  this->addConnection(server_socket);

  try {
    while(running.load()) {
      try {
        this->processEvent();
      } catch(std::exception& e) {
        std::cout << e.what() << std::endl;
      }
    } // while(running)
    
  } catch (std::exception& e) {
    std::cout << e.what() << std::endl;
  }
  close(epfd);
} // fundtion

} // namespace NonBlocking
} // namespace Network
} // namespace Afina
