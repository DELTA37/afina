#include <mutex>
#include <afina/Storage.h>
#include <atomic>

enum class Method { 
  Put=0, Get, Set, Delete, PutIfAbsent 
}

struct Record {
  Method opcode = -1;
  std::string key;
  std::string val;
  std::string* res = NULL;
  bool status = true;
}

struct Node {
  bool active = false;
  Record r;
  Node* next(NULL);
  bool added = false;
}

class FC {
  std::mutex mutex;
  std::atomic<uint_fast8_t> count;
  std::atomic<Node*> head(NULL);
  Afina::Storage* storage;
public:
  void apply(Command c);
  void scanPubList(void);
  FC(Afina::Storage* _storage);
  ~FC(void);
}
