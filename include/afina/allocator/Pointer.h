#ifndef AFINA_ALLOCATOR_POINTER_H
#define AFINA_ALLOCATOR_POINTER_H
#include "stddef.h"

namespace Afina {
namespace Allocator {
// Forward declaration. Do not include real class definition
// to avoid expensive macros calculations and increase compile speed
class Simple;

struct FreeSpace {
  size_t        size;
  ptrdiff_t     diff;
};

class Pointer {
public:
    FreeSpace* ptr; 
    void* base;

    Pointer();
    Pointer(void* base);
    Pointer(void* base, FreeSpace* _ptr);

    Pointer(const Pointer &);
    Pointer(Pointer &&);

    Pointer &operator=(const Pointer &);
    Pointer &operator=(Pointer &&);

    void* get() const; 
};

} // namespace Allocator
} // namespace Afina

#endif // AFINA_ALLOCATOR_POINTER_H
