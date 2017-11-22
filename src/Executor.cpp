#include <Executor.h>

Executor::Executor(std::string name, int size) { 
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

void Executor::setState(State _state) {
  std::unique_lock<std::mutex> lk(this->mutex);
  this->state = _state;
} 

State Executor::getState(void) {
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

void Executor::Stop(bool await = false) {
  this->setState(State::kStopping);
  if (await) {
    this->Join();
  }
}

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

