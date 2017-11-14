#include "Worker.h"

namespace Afina {
namespace Network {
namespace NonBlocking {

// See Worker.h
Worker::Worker(std::shared_ptr<Afina::Storage> _ps) : ps(_ps) {}

// See Worker.h
Worker::~Worker() {}

void* Worker::RunProxy(void* _args) {
  auto args = reinterpret_cast<std::pair<Worker*, int>*>(_args);
  Worker* worker_instance = args->first;
  int server_socket = args->second;
  worker_instance->thread = pthread_self();
  worker_instance->OnRun(server_socket);
  return NULL;
}

// See Worker.h
void Worker::Start(int server_socket) {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    this->running.store(true);
    auto args = new std::pair<Worker*, int>(this, server_socket);
    pthread_t _thread;
    if (pthread_create(&_thread, NULL, &(Worker::RunProxy), args) != 0) {
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
    try {
      EpollManager manager(this->ps, this, server_socket);
      while(running.load()) {
        try {
          manager.processEvent();
        } catch(std::exception& e) {
          std::cout << e.what() << std::endl;
          //pthread_exit(NULL);
        }
      } // while(running)
      
    } catch (std::exception& e) {
      std::cout << e.what() << std::endl;
    }
} // fundtion

} // namespace NonBlocking
} // namespace Network
} // namespace Afina
