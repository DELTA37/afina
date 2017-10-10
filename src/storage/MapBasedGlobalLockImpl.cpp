#include "MapBasedGlobalLockImpl.h"
#include <iostream>

#include <mutex>

namespace Afina {
namespace Backend {

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Put(const std::string &key, const std::string &value) {

  std::map<std::string, std::tuple<std::string, std::string, std::string>>::iterator it = this->_backend.find(key);
  if (it == this->_backend.end()) {
    if (this->_now < this->_max_size) {
      this->_now += 1;
      this->_lock.lock();

      this->_backend.insert(std::make_pair(key, std::make_tuple(value, this->_previous, std::string())));
      std::map<std::string, std::tuple<std::string, std::string, std::string>>::iterator it_pr = this->_backend.find(this->_previous);
      if (it_pr != this->_backend.end()) {
        std::get<2>(it_pr->second) = key;
      } else {
        this->_last = key;
      }
      this->_previous = key;

      this->_lock.unlock();
    } else {
      this->_lock.lock();

      std::map<std::string, std::tuple<std::string, std::string, std::string>>::iterator it_last = this->_backend.find(this->_last);
      this->_last = std::get<2>(it_last->second);
      std::get<1>(it_last->second) = "";
      this->_backend.erase(it);

      this->_backend.insert(std::make_pair(key, std::make_tuple(value, this->_previous, std::string())));
      std::map<std::string, std::tuple<std::string, std::string, std::string>>::iterator it_pr = this->_backend.find(this->_previous);
      std::get<2>(it_pr->second) = key;
      this->_previous = key;

      this->_lock.unlock();
    }
  }
  else {
    this->_lock.lock();
    std::get<0>(it->second) = value;
    std::string prev = std::get<1>(it->second);
    std::string next = std::get<2>(it->second);

    std::map<std::string, std::tuple<std::string, std::string, std::string>>::iterator it_pr = this->_backend.find(prev);
    std::map<std::string, std::tuple<std::string, std::string, std::string>>::iterator it_ne = this->_backend.find(next);

    if (it_pr != this->_backend.end()) {
      std::get<2>(it_pr->second) = next;
    }
    if (it_ne != this->_backend.end()) {
      std::get<1>(it_ne->second) = prev;
    }
  }

  this->_lock.unlock();
  return true; 
}

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::PutIfAbsent(const std::string &key, const std::string &value) {
  std::map<std::string, std::tuple<std::string, std::string, std::string>>::iterator it = this->_backend.find(key);
  if (it == this->_backend.end()) {
    if (this->_now < this->_max_size) {
      this->_now += 1;
      this->_lock.lock();

      this->_backend.insert(std::make_pair(key, std::make_tuple(value, this->_previous, std::string())));
      std::map<std::string, std::tuple<std::string, std::string, std::string>>::iterator it_pr = this->_backend.find(this->_previous);
      std::get<2>(it_pr->second) = key;
      this->_previous = key;

      this->_lock.unlock();
    } else {
      this->_lock.lock();

      std::map<std::string, std::tuple<std::string, std::string, std::string>>::iterator it = this->_backend.find(this->_last);
      this->_last = std::get<2>(it->second);
      std::map<std::string, std::tuple<std::string, std::string, std::string>>::iterator it_last = this->_backend.find(this->_last);
      std::get<1>(it_last->second) = "";
      this->_backend.erase(it);

      this->_backend.insert(std::make_pair(key, std::make_tuple(value, this->_previous, std::string())));
      std::map<std::string, std::tuple<std::string, std::string, std::string>>::iterator it_pr = this->_backend.find(this->_previous);
      std::get<2>(it_pr->second) = key;
      this->_previous = key;

      std::string prev = std::get<1>(it->second);
      std::string next = std::get<2>(it->second);

      it_pr = this->_backend.find(prev);
      std::map<std::string, std::tuple<std::string, std::string, std::string>>::iterator it_ne = this->_backend.find(next);

      if (it_pr != this->_backend.end()) {
        std::get<2>(it_pr->second) = next;
      }
      if (it_ne != this->_backend.end()) {
        std::get<1>(it_ne->second) = prev;
      }

    
      this->_lock.unlock();
    }

  }

  return true; 
}

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Set(const std::string &key, const std::string &value) { 
  std::map<std::string, std::tuple<std::string, std::string, std::string>>::iterator it = this->_backend.find(key);
  if (it != this->_backend.end()) {
    if (this->_now < this->_max_size) {
      this->_now += 1;
      this->_lock.lock();
      std::get<0>(it->second) = value;
      this->_lock.unlock();

      std::string prev = std::get<1>(it->second);
      std::string next = std::get<2>(it->second);

      std::map<std::string, std::tuple<std::string, std::string, std::string>>::iterator it_pr = this->_backend.find(prev);
      std::map<std::string, std::tuple<std::string, std::string, std::string>>::iterator it_ne = this->_backend.find(next);

      if (it_pr != this->_backend.end()) {
        std::get<2>(it_pr->second) = next;
      }
      if (it_ne != this->_backend.end()) {
        std::get<1>(it_ne->second) = prev;
      }


    } else {
      return false;
    }
  }

  return true; 
}

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Delete(const std::string &key) { 
  std::map<std::string, std::tuple<std::string, std::string, std::string>>::iterator it = this->_backend.find(key);
  if (it == this->_backend.end()) {
    return false;
  }
  this->_lock.lock();
  
  std::string next = std::get<2>(it->second);
  std::string prev = std::get<1>(it->second);

  std::map<std::string, std::tuple<std::string, std::string, std::string>>::iterator it_pr = this->_backend.find(prev);
  std::get<2>(it_pr->second) = next;

  std::map<std::string, std::tuple<std::string, std::string, std::string>>::iterator it_ne = this->_backend.find(next);
  std::get<1>(it_ne->second) = prev;

  this->_backend.erase(it);
  this->_now -= 1;

  this->_lock.unlock();
  return true; 
}

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Get(const std::string &key, std::string &value) const { 
  std::map<std::string, std::tuple<std::string, std::string, std::string>>::const_iterator it = this->_backend.find(key);
  if (it == this->_backend.end()) {
    return false; 
  } else {
    value = std::get<0>(it->second);
    return true;
  }
}

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Set(const std::string &key, const std::string &value) {
    std::unique_lock<std::mutex> guard(_lock);
    return false;
}

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Delete(const std::string &key) {
    std::unique_lock<std::mutex> guard(_lock);
    return false;
}

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Get(const std::string &key, std::string &value) const {
    std::unique_lock<std::mutex> guard(*const_cast<std::mutex *>(&_lock));
    return false;
}

} // namespace Backend
} // namespace Afina
