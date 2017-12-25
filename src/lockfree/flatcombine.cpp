#include <afina/flatcombine.h>

FC::FC(void) : head(NULL), storage(_storage) {
  count.store(0);
}

void FC::apply(Record c) {
  thread_local Node node;

  node.r = c;

  if (node.added == false) {
    Node* _head = NULL;
    do {
      Node* _head = head.load();
      node.next = _head; 
    } while(!head.compare_exchange_strong(_head, &node));
  }
  node.active = true;
  uint_fast8_t _count = 0; 
  while (node.active) {
    while((count.load() & 1) == 1 && (node.active)) {
      std::this_thread::yield();
    }
    _count = count.load();
    if (node.active && (_count & 1 == 0) && count.compare_exchange_strong(_count, _count + 1)) {
      scanPubList(); 
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
