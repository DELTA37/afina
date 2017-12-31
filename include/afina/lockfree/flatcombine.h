#include <mutex>
#include <afina/Storage.h>
#include <atomic>
#include <set>
#include <string>
#include <thread>

namespace Afina {

enum class Method { 
  Put=0, Get, Set, Delete, PutIfAbsent 
};

struct Record {
  Method opcode;
  std::string key;
  std::string val;
  std::string* res = NULL;
  bool status = true;
};

struct Node {
  bool active = false;
  Record r;
  Node* next = NULL;
  bool added = false;
};

template<typename Container>
class FC {
  std::atomic<uint_fast8_t> count;
  std::atomic<Node*> head;
public:
  void apply(Record c);
  void scanPubList(void);
  FC(Afina::Storage* _storage);
  ~FC(void);
};

} // Afina
