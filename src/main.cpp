#include <chrono>
#include <iostream>
#include <memory>
#include <uv.h>

#include <cxxopts.hpp>

#include <afina/Storage.h>
#include <afina/Version.h>
#include <afina/network/Server.h>

#include "network/blocking/ServerImpl.h"
#include "network/nonblocking/ServerImpl.h"
#include "network/uv/ServerImpl.h"
#include "storage/MapBasedGlobalLockImpl.h"
#include <signal.h>
#include <unistd.h>
#include <string.h>

#include <sys/signalfd.h>
#include <sys/timerfd.h>
#include <sys/epoll.h>

typedef struct {
    std::shared_ptr<Afina::Storage> storage;
    std::shared_ptr<Afina::Network::Server> server;
} Application;

// Handle all signals catched
void signal_handler(uv_signal_t *handle, int signum) {
    Application *pApp = static_cast<Application *>(handle->data);

    std::cout << "Receive stop signal" << std::endl;
    uv_stop(handle->loop);
}

// Called when it is time to collect passive metrics from services
void timer_handler(uv_timer_t *handle) {
    Application *pApp = static_cast<Application *>(handle->data);
    std::cout << "Start passive metrics collection" << std::endl;
}

void timerfd_handler(Application* pApp) {
  std::cout << "Start passive metrics collection" << std::endl;
}


int main(int argc, char **argv) {
    bool pid_mode;
    bool daemon_mode;
    bool fifo_mode;
    std::cout << "Starting Afina " << Afina::Version_Major << "." << Afina::Version_Minor << "."
              << Afina::Version_Patch;

    std::stringstream app_string;
    app_string << "Afina " << Afina::Version_Major << "." << Afina::Version_Minor << "." << Afina::Version_Patch;

    if (Afina::Version_SHA.size() > 0) {
        app_string << "-" << Afina::Version_SHA;
    }

    // Command line arguments parsing
    cxxopts::Options options("afina", "Simple memory caching server");
    try {
        // TODO: use custom cxxopts::value to print options possible values in help message
        // and simplify validation below
        options.add_options()("s,storage", "Type of storage service to use", cxxopts::value<std::string>());
        options.add_options()("n,network", "Type of network service to use", cxxopts::value<std::string>());
        options.add_options()("h,help", "Print usage info");
        options.add_options()("p,pidmode", "Print pid and exit", cxxopts::value<std::string>());
        options.add_options()("d,daemonmode", "Daemon mode");
        options.parse(argc, argv);

        if (options.count("help") > 0) {
            std::cerr << options.help() << std::endl;
            return 0;
        }
    } catch (cxxopts::OptionParseException &ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }
    std::string filename;
    if (options.count("pidmode") > 0) {
      pid_mode = true;
      filename = options["pidmode"].as<std::string>();
    }
    if (options.count("daemonmode") > 0) {
      daemon_mode = true;
      pid_t p = fork();
      if (p != 0) {
        exit(EXIT_SUCCESS);
      } else {
        setsid();
        fclose(stdin);
        fclose(stdout);
        fclose(stderr);
      }
    }

    if (pid_mode) {
      FILE* fp;
      fp = fopen(filename.c_str(), "w");
      fprintf(fp, "%u", getpid());
      fclose(fp);
    }

    // Start boot sequence
    Application app;
    std::cout << "Starting " << app_string.str() << std::endl;

    // Build new storage instance
    std::string storage_type = "map_global";
    if (options.count("storage") > 0) {
        storage_type = options["storage"].as<std::string>();
    }

    if (storage_type == "map_global") {
        app.storage = std::make_shared<Afina::Backend::MapBasedGlobalLockImpl>();
    } else {
        throw std::runtime_error("Unknown storage type");
    }

    // Build  & start network layer
    std::string network_type = "uv";
    if (options.count("network") > 0) {
        network_type = options["network"].as<std::string>();
    }

    if (network_type == "uv") {
        app.server = std::make_shared<Afina::Network::UV::ServerImpl>(app.storage);
        // Init local loop. It will react to signals and performs some metrics collections. Each
        // subsystem is able to push metrics actively, but some metrics could be collected only
        // by polling, so loop here will does that work
        uv_loop_t loop;
        uv_loop_init(&loop);

        uv_signal_t sig;
        uv_signal_init(&loop, &sig);
        uv_signal_start(&sig, signal_handler, SIGTERM | SIGKILL);
        sig.data = &app;

        uv_timer_t timer;
        uv_timer_init(&loop, &timer);
        timer.data = &app;
        uv_timer_start(&timer, timer_handler, 0, 5000);

        // Start services
        try {
            app.storage->Start();
            app.server->Start(8080);

            // Freeze current thread and process events
            std::cout << "Application started" << std::endl;
            uv_run(&loop, UV_RUN_DEFAULT);

            // Stop services
            app.server->Stop();
            app.server->Join();
            app.storage->Stop();

            std::cout << "Application stopped" << std::endl;
        } catch (std::exception &e) {
            std::cerr << "Fatal error" << e.what() << std::endl;
        }

    } else if (network_type == "blocking") {
        app.server = std::make_shared<Afina::Network::Blocking::ServerImpl>(app.storage);
    } else if (network_type == "nonblocking") {
        app.server = std::make_shared<Afina::Network::NonBlocking::ServerImpl>(app.storage);

        int epfd = epoll_create(2);

        int signal_num = 2;
        sigset_t mask;
        sigemptyset(&mask);
        sigaddset(&mask, SIGINT);
        sigaddset(&mask, SIGTERM);
        if (pthread_sigmask(SIG_BLOCK, &mask, NULL) == -1) {
          std::cerr << "Fatal error: pthread_sigmask" << std::endl;
          return 1;
        }
        int sigfd;
        if ((sigfd = signalfd(-1, &mask, SFD_NONBLOCK)) == -1) {
          std::cerr << "Fatal error: signalfd" << std::endl;
          return 1;
        }
        epoll_event sigev;
        sigev.events = EPOLLIN | EPOLLERR;
        sigev.data.fd = sigfd;
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, sigfd, &sigev) == -1) {
          std::cerr << "Fatal error: epoll_ctl" << std::endl;
          return 1;
        }

        itimerspec timer_spec;
        timer_spec.it_interval.tv_sec = 5;
        timer_spec.it_interval.tv_nsec = 0;
        timer_spec.it_value.tv_sec = 5;
        timer_spec.it_value.tv_nsec = 0;
        int timerfd;
        if ((timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK)) == -1) {
          std::cerr << "Fatal error: timerfd_create" << std::endl;
          return 1;
        }
        if (timerfd_settime(timerfd, TFD_TIMER_ABSTIME, &timer_spec, NULL) == -1) {
          std::cerr << "Fatal error: timer_settime" << std::endl;
          return 1;
        }
        epoll_event timerev;
        timerev.events = EPOLLIN | EPOLLERR;
        timerev.data.fd = timerfd;

        if (epoll_ctl(epfd, EPOLL_CTL_ADD, timerfd, &timerev) == -1) {
          std::cerr << "Fatal error: epoll_ctl" << std::endl;
          return 1;
        }

        try {
          app.storage->Start();
          app.server->Start(8080, 10);
          epoll_event evs[2];
          while(1) {
            int n = epoll_wait(epfd, evs, 2, -1);
            for (int i = 0; i < n; ++i) {
              if ((evs[i].events & EPOLLIN) == EPOLLIN) {
                if (evs[i].data.fd == sigfd) {
                  struct signalfd_siginfo sigs[signal_num];
                  int c = read(sigfd, sigs, signal_num * sizeof(struct signalfd_siginfo));
                  int k = c / sizeof(struct signalfd_siginfo);
                  for (int i = 0; i < k; ++i) {
                    if (sigs[i].ssi_signo == SIGINT) {
                      std::cout << "received SIGINT, stopping" << std::endl;
                      app.server->Stop();
                      app.server->Join();
                      app.storage->Stop();
                      return 0;
                    } else if (sigs[i].ssi_signo == SIGTERM) {
                      std::cout << "received SIGTERM, stopping" << std::endl;
                      app.server->Stop();
                      app.server->Join();
                      app.storage->Stop();
                      return 0;
                    }
                  }
                } else if (evs[i].data.fd == timerfd) {
                  uint64_t clk_num = 0;
                  if (read(timerfd, &clk_num, sizeof(uint64_t))) {
                    if (clk_num > 0) {
                      timerfd_handler(&app);
                    } 
                  }
                }
              } else {
                throw std::runtime_error("Fatal error descriptor");
              }
            }
          }
        } catch(std::exception& e) {
            std::cerr << "Fatal error" << e.what() << std::endl;
            return 1;
        }
        close(epfd);
        close(timerfd);
        close(sigfd);
        std::cout << "Application started" << std::endl;

    } else {
        throw std::runtime_error("Unknown network type");
    }


    return 0;
}
