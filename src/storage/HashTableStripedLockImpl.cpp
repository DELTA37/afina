#include "HashTableStripedLockImpl.h"
#include <iostream>

#include <mutex>

namespace Afina {
namespace Backend {

// See HashTableStripedLockImpl.h
bool HashTableStripedLockImpl::Put(const std::string &key, const std::string &value) {
  return this->Insert(key, value, InsertType::Put);
}

bool HashTableStripedLockImpl::Insert(const std::string &key, const std::string &value, InsertType type) {
  size_t i = std::hash<std::string>()(key) % this->_hash_size;
  std::unique_lock<std::mutex> lk(this->_lock[i]);
  bool need_pop = false;
  if (this->_backend[i].size() > this->_max_size / this->_hash_size) {
    need_pop = true;
  }
  for (auto& el : this->_backend[i]) {
    if (el.first == key) {
      if ((type == InsertType::Put) || (type == InsertType::Set)) {
        el.second = value;
      } else if (type == InsertType::PutIfAbsent) {
        return false; 
      }
    }
  }
  if (type == InsertType::Set) {
    return false;
  }
  if (need_pop) {
    this->_backend[i].pop_front();
  }
  this->_backend[i].push_back(std::make_pair(key, value));
  return true;
}

bool HashTableStripedLockImpl::Erase(const std::string &key) {
  size_t i = std::hash<std::string>()(key) % this->_hash_size;
  std::unique_lock<std::mutex> lk(this->_lock[i]);
  for (auto it = this->_backend[i].begin(); it != this->_backend[i].end(); ++it) {
    if (it->first == key) {
      this->_backend[i].erase(it);
      return true;
    }
  }
  return false;
}

// See HashTableStripedLockImpl.h
bool HashTableStripedLockImpl::Get(const std::string &key, std::string& value) const {
  size_t i = std::hash<std::string>()(key) % this->_hash_size;
  std::unique_lock<std::mutex> lk(this->_lock[i]);
  for (auto it = this->_backend[i].begin(); it != this->_backend[i].end(); ++it) {
    if (it->first == key) {
      /*
      this->_backend[i].push_back(*it);
      this->_backend[i].erase(it);
      */
      value = it->second;
      return true;
    }
  }
  return false;
}

// See HashTableStripedLockImpl.h
bool HashTableStripedLockImpl::PutIfAbsent(const std::string &key, const std::string &value) {
  return this->Insert(key, value, InsertType::PutIfAbsent);
}

// See HashTableStripedLockImpl.h
bool HashTableStripedLockImpl::Set(const std::string &key, const std::string &value) { 
  return this->Insert(key, value, InsertType::Set);
}

// See HashTableStripedLockImpl.h
bool HashTableStripedLockImpl::Delete(const std::string &key) { 
  return this->Erase(key);
}


} // namespace Backend
} // namespace Afina
