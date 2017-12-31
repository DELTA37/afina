#include <pthread.h>

template<typename T>
class ThreadLocal {
private:
  typedef std::remove_reference_t<T> ValueType;
  pthread_key_t key;
  void (*destr_fn)(void*);
  static void custom_destr_fn(void* mem) {
    if (mem != NULL) {
      T* val = reinterpret_cast<T*>(mem);
      delete val;
    }
  }
public:
  ThreadLocal(void (*_destr_fn = &(ThreadLocal::custom_destr_fn))(void*)) {
    this->destr_fn = _destr_fn;
    pthread_key_create(&this->key, this->destr_fn);
    pthread_setspecific(this->key, NULL);
  }
  ~ThreadLocal() {
    pthread_key_delete(this->key);
  }
  void set(T* val) {
    pthread_setspecific(this->key, val);
  }
  T* get(void) {
    return pthread_getspecific(this->key);
  }
};
