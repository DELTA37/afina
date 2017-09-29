#ifndef AFINA_ALLOCATOR_SIMPLE_H
#define AFINA_ALLOCATOR_SIMPLE_H

#include <string>
#include <cstddef>

namespace Afina {
namespace Allocator {

// Forward declaration. Do not include real class definition
// to avoid expensive macros calculations and increase compile speed
class Pointer;
struct FreeSpace;
/**
 * Wraps given memory area and provides defagmentation allocator interface on
 * the top of it.
 *
 * Allocator instance doesn't take ownership of wrapped memmory and do not delete it
 * on destruction. So caller must take care of resource cleaup after allocator stop
 * being needs
 */
// TODO: Implements interface to allow usage as C++ allocators
class Simple {
public:
    Simple(void *base, const size_t size);

    /**
     * TODO: semantics
     * @param N size_t
     */
    Pointer alloc(size_t N);

    /**
     * TODO: semantics
     * @param p Pointer
     * @param N size_t
     */
    void realloc(Pointer &p, size_t N);

    /**
     * TODO: semantics
     * @param p Pointer
     */
    void free(Pointer &p);

    /**
     * TODO: semantics
     */
    void defrag();

    /**
     * TODO: semantics
     */
    std::string dump() const;
private:
  void* base;
  size_t size;
  void* last_chain;
  ptrdiff_t pointer_info_start;

  void setFirstBlock(ptrdiff_t _diff);
  void* findFirstBlock(void);
  void unite(void* cur);
  int unregisterPointer(ptrdiff_t diff, size_t size);
  void unregisterPointer(FreeSpace* memreg);
  std::pair<void*, void*> findEnoughFree(void* base, size_t N);
  void setBlock(void* base, size_t size, ptrdiff_t diff);
  void insertBlock(void* prev, void* base, size_t size);
  void* findPrevious(void* base, void* mem);
  bool isPlaceForPointer(void);
  bool isPlaceForPointer(size_t size);
  FreeSpace* registerPointer(ptrdiff_t diff, size_t size);
  void placeTo(FreeSpace* p_inf, ptrdiff_t where);
  FreeSpace* getNext(ptrdiff_t l);
};

} // namespace Allocator
} // namespace Afina
#endif // AFINA_ALLOCATOR_SIMPLE_H
