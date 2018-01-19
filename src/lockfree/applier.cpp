#include <afina/lockfree/applier.h>
#include <set>
#include <string>

namespace Afina {
namespace LockFree {

void Applier::flat_combine(void) {
  Record* _head = head.load();
  std::set<std::string> deleted;
  while(_head != NULL) {
    if (_head->active) {
      switch(_head->slot.opcode) {
        case Method::Put: {
          if (deleted.count(_head->slot.key)) {
            _head->slot.status = false;
          } else {
            _head->slot.status = storage->Put(_head->slot.key, _head->slot.val);
          }
          break;
        }
        case Method::PutIfAbsent: {
          if (deleted.count(_head->slot.key)) {
            _head->slot.status = false;
          } else {
            _head->slot.status = storage->PutIfAbsent(_head->slot.key, _head->slot.val);
          }

          break;
        }
        case Method::Delete: {
          deleted.insert(_head->slot.key);
          _head->slot.status = storage->Delete(_head->slot.key);
          break;
        }
        case Method::Set: {
          if (deleted.count(_head->slot.key)) {
            _head->slot.status = false;
          } else {
            _head->slot.status = storage->Set(_head->slot.key, _head->slot.val);
          }
          break;
        }
        case Method::Get: {
          if (deleted.count(_head->slot.key)) {
            _head->slot.status = false;
          } else {
            _head->slot.status = storage->Get(_head->slot.key, _head->slot.res);
          }
          break;
        }
      }
      _head->active = false;
    }
    _head = _head->next;
  }
}

} // LockFree

} // Afina
