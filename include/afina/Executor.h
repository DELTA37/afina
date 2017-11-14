#ifndef AFINA_THREADPOOL_H
#define AFINA_THREADPOOL_H

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

namespace Afina {

static void* perform(void*);

/**
 * # Thread pool
 */
class Executor {
    /**
     * Main function that all pool threads are running. It polls internal task queue and execute tasks
     */
    friend void* perform(void* args);

    enum class State {
        // Threadpool is fully operational, tasks could be added and get executed
        kRun,

        // Threadpool is on the way to be shutdown, no ned task could be added, but existing will be
        // completed as requested
        kStopping,

        // Threadppol is stopped
        kStopped
    };
    
    Executor(std::string name, int size) { 
      if (name == "kRun") {
        this->state = State::kRun;
      } else if (name == "kStopping") {
        this->state = State::kStopping;
      } else if (name == "kStopped") {
        this->state = State::kStopped;
      }
      for (int i = 0; i < size; ++i) {
        this->threads.emplace_back(perform, this);
      }
    }

    ~Executor() {};

    /**
     * Signal thread pool to stop, it will stop accepting new jobs and close threads just after each become
     * free. All enqueued jobs will be complete.
     *
     * In case if await flag is true, call won't return until all background jobs are done and all threads are stopped
     */

    void setState(State _state) {
      std::unique_lock<std::mutex> lk(this->mutex);
      this->state = _state;
    } 
    State getState(void) {
      std::unique_lock<std::mutex> lk(this->mutex);
      return this->state;
    } 
    
    void Join(void) {
      for (int i = 0; i < this->threads.size(); ++i) {
        if (this->threads[i].joinable()) {
          this->threads[i].join();
        }
      }
    }

    void Stop(bool await = false) {
      this->setState(State::kStopping);
      if (await) {
        this->Join();
      }
    }

    /**
     * Add function to be executed on the threadpool. Method returns true in case if task has been placed
     * onto execution queue, i.e scheduled for execution and false otherwise.
     *
     * That function doesn't wait for function result. Function could always be written in a way to notify caller about
     * execution finished by itself
     */
    template <typename F, typename... Types> bool Execute(F &&func, Types... args) {
        // Prepare "task"
        auto exec = std::bind(std::forward<F>(func), std::forward<Types>(args)...);

        std::unique_lock<std::mutex> lock(this->mutex);
        if (state != State::kRun) {
            return false;
        }

        // Enqueue new task
        tasks.push_back(exec);
        empty_condition.notify_one();
        return true;
    }

private:
    // No copy/move/assign allowed
    Executor(const Executor &)             = delete;
    Executor(Executor &&)                  = delete;
    Executor &operator=(const Executor &)  = delete;
    Executor &operator=(Executor &&)       = delete;


    /**
     * Mutex to protect state below from concurrent modification
     */
    std::mutex mutex;

    /**
     * Conditional variable to await new data in case of empty queue
     */
    std::condition_variable empty_condition;

    /**
     * Vector of actual threads that perorm execution
     */
    std::vector<std::thread> threads;

    /**
     * Task queue
     */
    std::deque<std::function<void()>> tasks;

    /**
     * Flag to stop bg threads
     */
    State state;
};

void* perform(void* args) {
  Executor* executor = reinterpret_cast<Executor*>(args);
  while(executor->getState() == Executor::State::kRun) {
    std::function<void()> task;
    {
      std::unique_lock<std::mutex> lk(executor->mutex);
      executor->empty_condition.wait(lk, [&executor]() {return (executor->tasks.size() > 0) || (executor->state == Executor::State::kStopping);});
      if (executor->state == Executor::State::kStopping) {
        break;
      }
      task = executor->tasks.front();
      executor->tasks.pop_front();
    }
    task();
  }
  return NULL;
}



} // namespace Afina

#endif // AFINA_THREADPOOL_H
