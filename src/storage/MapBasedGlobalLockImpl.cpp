#include "MapBasedGlobalLockImpl.h"
#include <iostream>

#include <mutex>

namespace Afina {
namespace Backend {

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Put(const std::string &key, const std::string &value) {
  if (this->_now < this->_max_size) {
    this->_now += 1;
    this->_lock.lock();
    this->Insert(key, value);
    this->_lock.unlock();
  } else {
    this->_lock.lock();
    this->Erase(this->_last);
    this->Insert(key, value);
    this->_lock.unlock();
  }
  return true; 
}

void MapBasedGlobalLockImpl::Insert(const std::string &key, const std::string &value) {
  if (this->started) {
    StorageMap::iterator it = this->_backend.find(key);
    if (it == this->_backend.end()) {
      this->_backend.insert(StoragePair(key, StorageValue(value, this->_previous, "")));
    } else {
      std::get<0>(it->second) = value;
    }
    StorageMap::iterator it_pr = this->_backend.find(this->_previous);
    std::get<2>(it_pr->second) = key;
  } else {
    this->_backend.insert(StoragePair(key, StorageValue(value, "", "")));
    this->_last = key;
    this->_previous = key;
    this->started = true;
  }
}

void MapBasedGlobalLockImpl::Erase(const std::string &key) {
  StorageMap::iterator it = this->_backend.find(key);
  if (it != this->_backend.end()) {
    if (key == this->_last) {
      StorageMap::iterator it_last = this->_backend.find(key);
      std::string next = std::get<2>(it_last->second);
      StorageMap::iterator it_next = this->_backend.find(next);
      if (it_next != this->_backend.end()) {
        std::get<1>(it_next->second) = "";
        this->_last = next;
        this->_backend.erase(it);
      } else {
        this->_previous = "";
        this->_last = "";
        this->started = false;
        this->_backend.erase(it);
      }
    } else if (key == this->_previous) {
      StorageMap::iterator it_previous = this->_backend.find(key);
      std::string previous_prev = std::get<1>(it_previous->second);
      StorageMap::iterator it_previous_prev = this->_backend.find(previous_prev);
      if (it_previous_prev != this->_backend.end()) {
        std::get<2>(it_previous_prev->second) = "";
        this->_previous = previous_prev;
        this->_backend.erase(it);
      } else {
        this->_previous = "";
        this->_last = "";
        this->started = false;
        this->_backend.erase(it);
      }
    } else {
      std::string prev = std::get<1>(it->second);
      std::string next = std::get<2>(it->second);
      StorageMap::iterator it_prev = this->_backend.find(prev);
      StorageMap::iterator it_next = this->_backend.find(next);
      std::get<2>(it_prev->second) = next;
      std::get<1>(it_next->second) = prev;
      this->_backend.erase(it);
    }
  }
}

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::PutIfAbsent(const std::string &key, const std::string &value) {
  StorageMap::iterator it = this->_backend.find(key);
  if (it == this->_backend.end()) {
    this->Put(key, value);
  } else {
    return false;
  }
  return true; 
}

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Set(const std::string &key, const std::string &value) { 
  StorageMap::iterator it = this->_backend.find(key);
  if (it != this->_backend.end()) {
    this->Put(key, value);
  } else {
    return false;
  }

  return true; 
}

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Delete(const std::string &key) { 
  this->_lock.lock();
  this->Erase(key);
  this->_lock.unlock();
  return true; 
}

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Get(const std::string &key, std::string &value) const { 
  StorageMap::const_iterator it = this->_backend.find(key);
  if (it == this->_backend.end()) {
    return false; 
  } else {
    value = std::get<0>(it->second);
    return true;
  }
}


} // namespace Backend
} // namespace Afina
