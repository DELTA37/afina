#include <mutex>
#include <atomic>
#include <set>
#include <string>
#include <thread>

namespace Afina {

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
    Record* local_node = node.get();
    if (local_node == NULL) {
      local_node = new Record();
      local_node->slot = c;
      node.set(local_node);
    }
    
  }

  void acquire_lock(void) {
    uint8_t _count = 0;
    while(count.compare_exchange_weak(_count, _count | 1, std::memory_order_release, std::memory_order_relaxed)) { // пытаемся установить мьютекс
      _count &= ~uint8_t(1); // ожидаем сброшенный мьютекст
      std::this_thread::yield();
    }
    _count += 2; // подсчет эры, несбрасывая мьютекса
  }

  void release_lock(void) {
    count.store(count & 0);
  }

  void apply(Slot c) {
    Record* local_node = node.get();
    if (local_node == NULL) {
      local_node = new Record();
      local_node->slot = c;
      node.set(local_node);
    }
    while(!(local_node->next = head.compare_exchange_weak(local_node->next, local_node, std::memory_order_release, std::memory_order_relaxed))) {}
    // added node
  }
  FC(void);
  ~FC(void);

  virtual void flat_combine(void);
};

} // Afina
