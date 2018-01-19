#include <afina/lockfree/flatcombine.h>
#include <afina/Storage.h>

namespace Afina {
namespace LockFree {

enum class Method {
  Put, PutIfAbsent, Get, Delete, Set 
};

struct ApplierSlot {
  bool status;
  Method opcode;
  std::string key;
  std::string val;
  std::string res;
};

class Applier : public FC<ApplierSlot> {
  Storage* storage;
  Applier(Storage* _storage) : storage(_storage) {}
  virtual void flat_combine(void);
};

} // LockFree
} // Afina
