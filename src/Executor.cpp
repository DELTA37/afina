#include <afina/Executor.h>
#include <iostream>

namespace Afina {

Executor::Executor(std::string name, int size) { 
  this->name = name;
  this->state.store(State::kRun);
  n_running_threads.store(size);
  for (int i = 0; i < size; ++i) {
    std::pair<Executor*, int>* arg = new std::pair<Executor*, int>(this, i);
    this->threads.emplace_back(perform, arg);
  }
}

void Executor::Join(void) {
  for (int i = 0; i < this->threads.size(); ++i) {
    if (this->threads[i].joinable()) {
      this->threads[i].join();
    }
  }
}

void Executor::Stop(bool await) {
  this->state.store(State::kStopping);
}

void* perform(void* args) {
  auto arg = reinterpret_cast<std::pair<Executor*, int>*>(args);
  Executor* executor = arg->first;
  int i = arg->second;
  delete arg;

  pthread_setname_np(pthread_self(), (executor->name + std::to_string(i)).c_str());

  while(executor->state.load() == Executor::State::kRun || executor->state.load() == Executor::State::kStopping) {
    std::function<void()> task;
    {
      std::unique_lock<std::mutex> lk(executor->mutex);
      executor->empty_condition.wait(lk, [&executor]() { 
        return (executor->tasks.size() > 0) || (executor->state.load() == Executor::State::kStopping); 
      });
      if (executor->tasks.size() > 0) {
        task = executor->tasks.front();
        executor->tasks.pop_front();
      } else {
        break;
      }
    }
    try {
      task();
    } catch(...) {
      
    }
  }
  executor->n_running_threads.fetch_sub(1);

  if (executor->n_running_threads.load() == 0) {
    executor->state.store(Executor::State::kStopped);
  }
  return NULL;
}

} // Afina
