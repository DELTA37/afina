#include <afina/allocator/Pointer.h>

namespace Afina {
namespace Allocator {

Pointer::Pointer() : ptr(NULL) {}
Pointer::Pointer(FreeSpace* _ptr) : ptr(_ptr) {};
Pointer::Pointer(const Pointer &q) {
  this->ptr = q.ptr;
}
Pointer::Pointer(Pointer &&q) {
  this->ptr = q.ptr;
}

Pointer &Pointer::operator=(const Pointer &q) {
  this->ptr = q.ptr;
  return *this; 
}

Pointer &Pointer::operator=(Pointer &&q) {
  this->ptr = q.ptr;
  return *this; 
}

void* Pointer::get() const { 
  if (this->ptr) {
    return (void*)((char*)Pointer::base + this->ptr->diff); 
  }
  else {
    return NULL;
  }
}

} // namespace Allocator
} // namespace Afina
