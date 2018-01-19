#include <mutex>
#include <atomic>
#include <set>
#include <string>
#include <thread>
#include <afina/lockfree/thread_local.h>

namespace Afina {

namespace LockFree {

template<typename Slot>
class FC {
protected:
  struct Record {
    bool active = false;
    size_t been_activated = 0; 
    Slot slot;
    Record* next = NULL;
  };

  ThreadLocal<Record*> node;

  volatile std::atomic<uint8_t> count;
  volatile std::atomic<Record*> head;
public:
  void enqueque_slot(const Slot& c) {
    publicate(c);
    if (acquire_lock()) {
      flat_combine();
      release_lock();
    }
  }

  bool acquire_lock(void) {
    uint8_t _count = 0;
    uint8_t _old = 0;
    while(count.compare_exchange_weak(_count, _count | 1, std::memory_order_release, std::memory_order_relaxed)) { // пытаемся установить мьютекс
      _count &= ~uint8_t(1); // ожидаем сброшенный мьютекст
      Record* local_record = node.get();
      if (!local_record || !local_record->active) {
        return false;
      }
      std::this_thread::yield();
    }
    _count += 2; // подсчет эры, несбрасывая мьютекса
    return true;
  }

  void release_lock(void) {
    count.store(count & 0, std::memory_order_relaxed);
  }

  void publicate(const Slot& c) {
    Record* local_node = node.get();
    if (local_node == NULL) {
      local_node = new Record();
      local_node->slot = c;
      node.set(local_node);
    }
    while(!(head.compare_exchange_weak(local_node->next, local_node, std::memory_order_release, std::memory_order_relaxed))) {}
  }

  FC(void) : count(0), head(NULL) {};
  ~FC(void) {};

  virtual void flat_combine(void) {}
};

} // LockFree
} // Afina
