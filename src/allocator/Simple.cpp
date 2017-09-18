#include <afina/allocator/Simple.h>
#include <afina/allocator/Error.h>
#include <afina/allocator/Pointer.h>
#include <iostream>
#include "assert.h"
#include <sstream>

namespace Afina {
namespace Allocator {

extern void* Pointer::base;
extern size_t Pointer::size;

extern void* FreeSpace::last_chain;
extern ptrdiff_t FreeSpace::pointer_info_start;

struct Couple {
  void* first;
  void* second;
};

static void cleanup(void* base, size_t N) {
  char* mem = (char*)base;
  for (size_t i = 0; i < N; i++) {
    mem[i] = 0;
  }
}

static inline void* move(void* base, ptrdiff_t d) {
  return reinterpret_cast<void*>(reinterpret_cast<char*>(base) + d);
}

static inline FreeSpace* info(void* block) {
  return reinterpret_cast<FreeSpace*>(block);
}

static void* startChain() {
  size_t m = sizeof(FreeSpace);
  FreeSpace* inf = info(move(Pointer::base, Pointer::size - m));
  if (inf->size == 0) {
    return NULL;
  }
  else {
    return move(Pointer::base, inf->diff);
  }
}

static inline ptrdiff_t dst(void* p1, void* p2) {
  char* q1 = reinterpret_cast<char*>(p1);
  char* q2 = reinterpret_cast<char*>(p2);
  return q2 - q1;
}

static void unite(void* cur) {
  FreeSpace* cur_info = info(cur);
  void* next = NULL;
  FreeSpace* next_info = NULL;
  while(cur_info->diff > 0) {
    next = move(Pointer::base, cur_info->diff);
    next_info = info(next);
    if(move(cur, cur_info->size) == next) {
      cur_info->diff = next_info->diff;
      cur_info->size += next_info->size;
    }
    else {
      break;
    }
  }
}

static void setChain(void* base, size_t size, ptrdiff_t diff) {
  FreeSpace* cur = info(base);
  cur->size = size;
  cur->diff = diff;
}

static void setStartChain(ptrdiff_t _diff) {
  size_t m = sizeof(FreeSpace);
  FreeSpace* inf = info(move(Pointer::base, Pointer::size - m));
  inf->diff = _diff;
  inf->size = 1;
}

static Couple findEnoughFree(void* base, size_t N) {
  void* ptr = base;
  void* prev = NULL;
  FreeSpace* cur = info(ptr);
  while (cur->size < N) {
    if (cur->diff == 0) {
      return Couple({NULL, NULL});
    }
    prev = ptr;
    ptr = move(Pointer::base, cur->diff);
    cur = info(ptr);
  }
  return Couple({prev, ptr});
}

static void insertChain(void* prev, void* base, size_t size) {
  size_t m = sizeof(FreeSpace);
  size_t new_size = ((size + m - 1) / m) * m;

  FreeSpace* cur   = info(base);
  FreeSpace* last  = info(prev);
  FreeSpace* new_cur = NULL;
  FreeSpace* start_info = info(move(Pointer::base, Pointer::size - m));
  void* new_base = NULL;

  if (cur->size < new_size) {
    throw std::runtime_error("");
  }
  if (cur->size >= new_size + m) {
    /*
     * Part of the cell is left
     */
    new_base = move(base, new_size);
    if (base == FreeSpace::last_chain) {
      FreeSpace::last_chain = new_base;
    }
    new_cur = info(new_base);
    new_cur->size = cur->size - new_size;
    new_cur->diff = cur->diff;

    if (prev != NULL) {
      last->diff = dst(Pointer::base, new_base);
    }
    else {
      start_info->diff = dst(Pointer::base, new_base);
    }
  } 
  else if (cur->size == new_size) {
    /*
     * Delete whole cell
     */
    if (prev != NULL) {
      last->diff = cur->diff;
      if (base == FreeSpace::last_chain) {
        FreeSpace::last_chain = prev;
      }
    }
    else {
      start_info->diff = cur->diff;
      if (cur->diff == 0) {
        start_info->size = 0;
        assert(FreeSpace::last_chain == base);
        FreeSpace::last_chain = NULL;
      }
      else {
        assert(FreeSpace::last_chain != base);
      }
    }
  }
  else {
    throw std::runtime_error("");
  }
}

static void* findPrevious(void* base, void* mem) {
  if (base == NULL) {
    return NULL;
  }
  void* res = NULL;
  void* ptr = base;
  FreeSpace* cur = info(ptr);
  while(ptr < mem) {
    if (cur->diff == 0) {
      return ptr;
    }
    res = ptr;
    ptr = move(Pointer::base, cur->diff);
    cur = info(ptr);
  }
  return res;
}

static bool isPlaceForPointer() {
  size_t m = sizeof(FreeSpace);
  FreeSpace* inf = NULL;
  FreeSpace* last_info = info(FreeSpace::last_chain);
  for (size_t i = FreeSpace::pointer_info_start; i < Pointer::size - m; i+=m) {
    inf = info(move(Pointer::base, i));
    if (inf->diff == 0 && inf->size == 0) {
      return true;
    }
  }
  if (last_info->size <= 2 * m) {
    return false;
  }
  return true;
}

static bool isPlaceForPointer(size_t size) {
  size_t m = sizeof(FreeSpace);
  FreeSpace* inf = NULL;
  FreeSpace* last_info = info(FreeSpace::last_chain);
  if (last_info->size <= size + m) {
    return false;
  }
  for (size_t i = FreeSpace::pointer_info_start; i < Pointer::size - m; i+=m) {
    inf = info(move(Pointer::base, i));
    if (inf->diff == 0 && inf->size == 0) {
      return true;
    }
  }
  if (last_info->size <= 2 * m + size) {
    return false;
  }
  return true;
}


static FreeSpace* registerPointer(ptrdiff_t diff, size_t size) {
  size_t m = sizeof(FreeSpace);
  for (size_t i = FreeSpace::pointer_info_start; i < Pointer::size - m; i+=m) {
    FreeSpace* ptr_info = info(move(Pointer::base, i));
    if (ptr_info->size == 0) {
      setChain(move(Pointer::base, i), size, diff);
      return info(move(Pointer::base, i));
    } 
  } 
  FreeSpace* last_info = info(FreeSpace::last_chain);
  if (last_info->size <= 2 * m) {
    return NULL;
  }
  FreeSpace::pointer_info_start -= m;
  last_info->size -= m;
  setChain(move(Pointer::base, FreeSpace::pointer_info_start), size, diff);
  return info(move(Pointer::base, FreeSpace::pointer_info_start));
}

static void unregisterPointer(FreeSpace* memreg) {
  size_t m = sizeof(FreeSpace);
  if (memreg == info(move(Pointer::base, FreeSpace::pointer_info_start))) {
    FreeSpace::pointer_info_start += m;
    FreeSpace* last_info = info(FreeSpace::last_chain);
    last_info->size += m;
    return;
  }
  memreg->size = 0;
  memreg->diff = 0;
}

static int unregisterPointer(ptrdiff_t diff, size_t size) {
  size_t m = sizeof(FreeSpace);
  for (size_t i = FreeSpace::pointer_info_start; i < Pointer::size - m; i+=m) {
    FreeSpace* ptr_info = info(move(Pointer::base, i));
    if (ptr_info->diff == diff) {
      if (ptr_info->size == size) {
        throw std::runtime_error("");
        return 0;
      }
      unregisterPointer(ptr_info);
      return 1;
    } 
  } 
  return 0;
}


Simple::Simple(void *base, size_t size) {
  size_t m = sizeof(FreeSpace);
  size_t new_size = (size / m) * m;
  Pointer::base = base;
  Pointer::size = new_size;
  cleanup(Pointer::base, Pointer::size);
  setChain(base, Pointer::size - m, 0);
  setChain(move(base, Pointer::size - m), 1, 0);
  FreeSpace::pointer_info_start = Pointer::size - m;
  FreeSpace::last_chain = base;
}

/**
 * TODO: semantics
 * @param N size_t
 */
Pointer Simple::alloc(size_t N) {
  size_t m = sizeof(FreeSpace);
  if (N == 0) {
    return Pointer();
  }

  FreeSpace* start_info = info(move(Pointer::base, Pointer::size - m));
  if (start_info->size == 0) {
    throw AllocError(AllocErrorType::NoMemory, "i have eaten all your memory. it's a joke. No place...");
    return Pointer();
  }

  if (not isPlaceForPointer()) {
    throw AllocError(AllocErrorType::NoMemory, "i have eaten all your memory. it's a joke.");
    return Pointer();
  }

  FreeSpace* last_info = info(FreeSpace::last_chain);
  void* base = startChain(); 
  Couple cp = findEnoughFree(base, N);
  void* prev = cp.first;
  void* cur = cp.second;

  if (cur == NULL) {
    throw AllocError(AllocErrorType::NoMemory, "i have eaten all your memory. it's a joke.");
    return Pointer();
  }
  else {
    FreeSpace* cur_info = info(cur);
    if ((cur == FreeSpace::last_chain) && not isPlaceForPointer((N + m - 1) / m * m)) {
        throw AllocError(AllocErrorType::NoMemory, "i have eaten all your memory. it's a joke.");
        return Pointer();
    }
    insertChain(prev, cur, N);
    FreeSpace* res = registerPointer(dst(Pointer::base, cur), ((N + m - 1) / m) * m); // for return 
    return Pointer(res);
  }
}

/**
 * TODO: semantics
 * @param p Pointer
 * @param N size_t
 */
void Simple::realloc(Pointer &pt, size_t N) {
  if (pt.ptr == NULL) {
    pt = alloc(N);
  }
  FreeSpace p = *(pt.ptr);
  size_t m = sizeof(FreeSpace);
  FreeSpace* start_info = info(move(Pointer::base, Pointer::size - m));
  if (p.size == N) {
    return;
  }
  if (p.size > N) {
    /*
     * add to chain free space
     */
    void* ptr = move(move(Pointer::base, p.diff), N);
    if (start_info->size == 0) {
      /*
       * If there are no free space
       */
      start_info->size = 1;
      start_info->diff = dst(Pointer::base, ptr);
      setChain(ptr, p.size - N, 0);
      p.size = N;
      FreeSpace::last_chain = ptr;
      return;
    }
    void* base = startChain(); // as there is a free space
    void* prev = NULL;
    if (ptr > base) {
      prev = findPrevious(base, ptr);
    }
    else {
      prev = move(Pointer::base, Pointer::size - m);
    }
    FreeSpace* cur_info = info(ptr);
    FreeSpace* prev_info = info(prev);
    cur_info->diff = prev_info->diff;
    cur_info->size = p.size - N;
    prev_info->diff = dst(Pointer::base, ptr);
    p.size = N;
    if (cur_info->diff == 0) {
      FreeSpace::last_chain = ptr;
    }
    return;
  }
  // we need some free space
  if (start_info->size == 0) {
    throw AllocError(AllocErrorType::NoMemory, "i have eaten all your memory!");
  }

  void* ptr = move(Pointer::base, p.diff);
  void* base = startChain();
  void* prev = NULL;
  if (base < ptr) {
    prev = findPrevious(base, ptr);
  }
  else {
    prev = move(Pointer::base, Pointer::size - m);
  } // as we do not change a size of previous 
  void* next = NULL;
  FreeSpace* prev_info = info(prev);
  FreeSpace* next_info = NULL;

  if (prev_info->diff > 0) {
    next = move(Pointer::base, prev_info->diff);
    unite(next);
    next_info = info(next);
    if (
        move(ptr, p.size) == next && N <= p.size + next_info->size && 
        (next != FreeSpace::last_chain || N + 2 * m <= p.size + next_info->size)
      ) {
      /*
       * delete next from chain
       */
      if (N < p.size + next_info->size) {
        /*
         * Delete part of next
         */
        void* new_ptr = move(ptr, N);
        FreeSpace* new_ptr_info = info(new_ptr);
        new_ptr_info->diff = next_info->diff;
        new_ptr_info->size = p.size + next_info->size - N;
        prev_info->diff = dst(Pointer::base, new_ptr);
        p.size = N;
        if (new_ptr_info->diff == 0) {
          FreeSpace::last_chain = new_ptr;
        }
        return;
      }
      else {
        /*
         * Delete all next
         */
        prev_info->diff = next_info->diff;
        p.size = N;
        if (prev_info->diff == 0 && prev != move(Pointer::base, Pointer::size - m)) {
          FreeSpace::last_chain = prev;
        }
        return;
      }
    }
  }
  /*
   * There is no way to extend p not using changing p.diff
   */
  Pointer q = alloc(N);
  char* dst = reinterpret_cast<char*>(move(Pointer::base, q.ptr->diff));
  char* src = reinterpret_cast<char*>(move(Pointer::base, p.diff));
  for (size_t i = 0; i < N; i++) {
    dst[i] = src[i];
  }
  free(pt);
  pt = q;
}

/**
 * TODO: semantics
 * @param p Pointer
 */
void Simple::free(Pointer &pt) {
  if (pt.ptr == NULL) {
    throw AllocError(AllocErrorType::InvalidFree, "Null pointer free");
  }
  FreeSpace p = *(pt.ptr);
  size_t m = sizeof(FreeSpace);
  FreeSpace* start_info = info(move(Pointer::base, Pointer::size - m));
  if (start_info->size == 0) {
    start_info->diff = p.diff;
    start_info->size = 1;
    setChain(move(Pointer::base, p.diff), p.size, 0);
    FreeSpace::last_chain = move(Pointer::base, p.diff);
  }
  else {
    void* base = startChain();
    void* ptr = move(Pointer::base, p.diff);
    FreeSpace* cur_info = info(ptr);
    FreeSpace* base_info = info(base);
    void* prev = findPrevious(base, ptr);

    if (prev != NULL) {
      FreeSpace* prev_info = info(prev);
      cur_info->diff = prev_info->diff; // diff for next free space
      cur_info->size = p.size;
      prev_info->diff = p.diff; // diff for ptr == dst(Pointer::base, ptr)
    }
    else {
      cur_info->diff = dst(Pointer::base, base); // as base > ptr
      cur_info->size = p.size;
      setStartChain(p.diff);
    }
    if (cur_info->diff == 0) {
      FreeSpace::last_chain = ptr;
    }
  }
  unregisterPointer(pt.ptr);
  pt.ptr = NULL;
}

/**
 * TODO: semantics
 */

static void copy(void* src, void* dst, size_t size) {
  if (src == dst) {
    return;
  }
  char* c_src = reinterpret_cast<char*>(src);
  char* c_dst = reinterpret_cast<char*>(dst);
  for (size_t i = 0; i < size; i++) {
    c_dst[i] = c_src[i];
  }
}

static void placeTo(FreeSpace* p_inf, ptrdiff_t where) {
  copy(move(Pointer::base, p_inf->diff), move(Pointer::base, where), p_inf->size);
  p_inf->diff = where;
}

static FreeSpace* getNext(ptrdiff_t l) {
  size_t m = sizeof(FreeSpace);
  FreeSpace* res = NULL;
  ptrdiff_t mn = Pointer::size;
  for (size_t i = FreeSpace::pointer_info_start; i < Pointer::size - m; i+=m) {
    FreeSpace* inf = info(move(Pointer::base, i));
    //std::cout << inf->diff << " " << inf->size << std::endl;
    if (inf->diff >= l && inf->diff < mn && inf->size > 0) {
      res = inf;
      mn = inf->diff;
    }
  }
  return res;
}

void Simple::defrag() {
  size_t m = sizeof(FreeSpace);
  FreeSpace* start_info = info(move(Pointer::base, Pointer::size - m));
  void* base = NULL;
  if (start_info->size == 0) {
    return;
  }
  base = move(Pointer::base, start_info->diff);
  void* cur = base;
  FreeSpace* cur_info = info(cur);
  while (cur_info->diff > 0) {
    unite(cur);
    cur_info = info(cur);
    if (cur_info->diff == 0) {
      break;
    }
    cur = move(Pointer::base, cur_info->diff);
  }
  /*
   * moving
   */
  size_t len = 0;
  FreeSpace* inf = NULL;
  for (size_t j = FreeSpace::pointer_info_start; j < Pointer::size - m; j+=m) {
    inf = getNext(len);
    if (inf == NULL) {
      break;
    }
    placeTo(inf, len);
    len += inf->size;
  }
  FreeSpace::last_chain = move(Pointer::base, len);
  setStartChain(dst(Pointer::base, FreeSpace::last_chain));
  setChain(FreeSpace::last_chain, FreeSpace::pointer_info_start - len, 0);
}

/**
 * TODO: semantics
 */
std::string Simple::dump() const { 
  size_t m = sizeof(FreeSpace);
  std::stringstream buf;
  FreeSpace* inf = NULL;
  for (size_t i = FreeSpace::pointer_info_start; i < Pointer::size - m; i+=m) {
    inf = info(move(Pointer::base, i));
    buf << inf->diff << " " << inf->size << std::endl;
  }
  return buf.str();
}

} // namespace Allocator
} // namespace Afina
