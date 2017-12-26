#include <afina/lockfree/flatcombine.h>

namespace Afina {

FC::FC(Afina::Storage *_storage) : head(NULL), storage(_storage) {
  count.store(0);
  head.store(NULL);
}

void FC::apply(Record c) {
  thread_local Node node;

  node.r = c;

  if (node.added == false) {
    Node* _head = NULL;
    while(head.compare_exchange_weak(_head, &node, std::memory_order_release, std::memory_order_relaxed)) {
      node.next = _head; 
    }
  }

  node.active = true;
  uint_fast8_t _count = 0; 

  while(node.active) {
    while(count.compare_exchange_weak(_count, 1, std::memory_order_release, std::memory_order_relaxed) && node.active) {
      _count = 0;
      std::this_thread::yield();
    }
    if (node.active) {
      scanPubList();
      count.store(0, std::memory_order_release);
    }
  }
  
}

void FC::scanPubList(void) {
  Node* _head = head.load();
  std::set<std::string> deleted;
  while(_head != NULL) {
    if (_head->active) {
      switch(_head->r.opcode) {
        case Method::Put: {
          if (deleted.count(_head->r.key)) {
            _head->r.status = false;
          } else {
            _head->r.status = storage->Put(_head->r.key, _head->r.val);
          }
          break;
        }
        case Method::PutIfAbsent: {
          if (deleted.count(_head->r.key)) {
            _head->r.status = false;
          } else {
            _head->r.status = storage->PutIfAbsent(_head->r.key, _head->r.val);
          }

          break;
        }
        case Method::Delete: {
          deleted.insert(_head->r.key);
          _head->r.status = storage->Delete(_head->r.key);
          break;
        }
        case Method::Set: {
          if (deleted.count(_head->r.key)) {
            _head->r.status = false;
          } else {
            _head->r.status = storage->Set(_head->r.key, _head->r.val);
          }
          break;
        }
        case Method::Get: {
          if (deleted.count(_head->r.key)) {
            _head->r.status = false;
          } else {
            _head->r.status = storage->Get(_head->r.key, *(_head->r.res));
          }
          break;
        }
      }
      _head->active = false;
    }
    _head = _head->next;
  }
}

} // Afina
