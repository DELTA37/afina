#include <afina/allocator/Pointer.h>

namespace Afina {
namespace Allocator {

Pointer::Pointer() : ptr(NULL), base(NULL) {}
Pointer::Pointer(void* _base) : base(_base), ptr(NULL) {};
Pointer::Pointer(void* _base, FreeSpace* _ptr) : base(_base), ptr(_ptr) {};
Pointer::Pointer(const Pointer &q) {
  this->ptr = q.ptr;
  this->base = q.base;
}
Pointer::Pointer(Pointer &&q) {
  this->ptr = q.ptr;
  this->base = q.base;
}

Pointer &Pointer::operator=(const Pointer &q) {
  this->ptr = q.ptr;
  this->base = q.base;
  return *this; 
}

Pointer &Pointer::operator=(Pointer &&q) {
  this->ptr = q.ptr;
  this->base = q.base;
  return *this; 
}

void* Pointer::get() const { 
  if (this->ptr) {
    return (void*)((char*)this->base + this->ptr->diff); 
  }
  else {
    return NULL;
  }
}

} // namespace Allocator
} // namespace Afina
