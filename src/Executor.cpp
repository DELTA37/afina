#include <afina/Executor.h>

namespace Afina {

Executor::Executor(std::string name, int size) { 
  this->name = name;
  pthread_setname_np(pthread_self(), name.c_str());
  for (int i = 0; i < size; ++i) {
    this->threads.emplace_back(perform, this);
  }
}

void Executor::setState(State _state) {
  std::unique_lock<std::mutex> lk(this->mutex);
  this->state = _state;
} 

Executor::State Executor::getState(void) {
  std::unique_lock<std::mutex> lk(this->mutex);
  return this->state;
} 

void Executor::Join(void) {
  for (int i = 0; i < this->threads.size(); ++i) {
    if (this->threads[i].joinable()) {
      this->threads[i].join();
    }
  }
}

void Executor::Stop(bool await) {
  this->setState(State::kStopping);
  if (await) {
    this->Join();
  }
}

void* perform(void* args) {
  Executor* executor = reinterpret_cast<Executor*>(args);
  pthread_setname_np(pthread_self(), executor->name.c_str());
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
    try {
      task();
    } catch(...) {
      
    }
  }
  return NULL;
}

} // Afina
