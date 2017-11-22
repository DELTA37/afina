#ifndef AFINA_STORAGE_MAP_BASED_GLOBAL_LOCK_IMPL_H
#define AFINA_STORAGE_MAP_BASED_GLOBAL_LOCK_IMPL_H

#include <map>
#include <list>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <string>
#include <queue>
#include <afina/Storage.h>

namespace Afina {
namespace Backend {

/**
 * # Map based implementation with global lock
 *
 *
 */


enum class InsertType { Put, PutIfAbsent, Set };

class PriorityMapBasedGlobalLockImpl : public Afina::Storage {
public:
    PriorityMapBasedGlobalLockImpl(size_t max_size = 1024) : _max_size(max_size), _hash_size(max_size / 32){}
    ~PriorityMapBasedGlobalLockImpl() {}

    // Implements Afina::Storage interface
    bool Put(const std::string &key, const std::string &value) override;

    // Implements Afina::Storage interface
    bool PutIfAbsent(const std::string &key, const std::string &value) override;

    // Implements Afina::Storage interface
    bool Set(const std::string &key, const std::string &value) override;

    // Implements Afina::Storage interface
    bool Delete(const std::string &key) override;

    // Implements Afina::Storage interface
    bool Get(const std::string &key, std::string &value) const override;

private:
    bool Insert(const std::string &key, const std::string &value, InsertType type);
    bool Erase(const std::string &key);

    typedef std::vector<std::list<std::pair<std::string, std::string> > > StorageMap;

    mutable std::mutex _lock;

    size_t _max_size;
    size_t _hash_size;

    StorageMap _backend;
};

} // namespace Backend
} // namespace Afina

#endif // AFINA_STORAGE_MAP_BASED_GLOBAL_LOCK_IMPL_H
