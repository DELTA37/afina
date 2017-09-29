#include "MapBasedGlobalLockImpl.h"

namespace Afina {
namespace Backend {

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Put(const std::string &key, const std::string &value) {

  std::map<std::string, std::string>::iterator it = this->_backend.find(key);
  if (it == this->_backend.end()) {
    if (this->_now < this->_max_size) {
      this->_now += 1;
      this->_lock.lock();
      this->_backend.insert(std::make_pair(key, value));
      this->_lock.unlock();
    } else {
      return false;
    }
  }
  else {
    this->_lock.lock();
    it->second = value;
    this->_lock.unlock();
  }

  return true; 
}

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::PutIfAbsent(const std::string &key, const std::string &value) {

  std::map<std::string, std::string>::iterator it = this->_backend.find(key);
  if (it == this->_backend.end()) {
    if (this->_now < this->_max_size) {
      this->_now += 1;
      this->_lock.lock();
      this->_backend.insert(std::make_pair(key, value));
      this->_lock.unlock();
    } else {
      return false;
    }
  }

  return true; 
}

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Set(const std::string &key, const std::string &value) { 
  std::map<std::string, std::string>::iterator it = this->_backend.find(key);
  if (it != this->_backend.end()) {
    if (this->_now < this->_max_size) {
      this->_now += 1;
      this->_lock.lock();
      it->second = value;
      this->_lock.unlock();
    } else {
      return false;
    }
  }

  return true; 
}

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Delete(const std::string &key) { 
  std::map<std::string, std::string>::iterator it = this->_backend.find(key);
  if (it == this->_backend.end()) {
    return false;
  }
  this->_lock.lock();
  this->_backend.erase(it);
  this->_now -= 1;
  this->_lock.unlock();
  return true; 
}

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Get(const std::string &key, std::string &value) const { 
  std::map<std::string, std::string>::const_iterator it = this->_backend.find(key);
  if (it == this->_backend.end()) {
    return false; 
  } else {
    value = it->second;
    return true;
  }
}

} // namespace Backend
} // namespace Afina
