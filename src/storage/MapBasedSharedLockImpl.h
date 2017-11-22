#ifndef AFINA_STORAGE_MAP_BASED_SHARED_LOCK_IMPL_H
#define AFINA_STORAGE_MAP_BASED_SHARED_LOCK_IMPL_H

#include <map>
#include <unordered_map>
#include <mutex>
#include <string>
#include <queue>
#include <shared_mutex>

#include <afina/Storage.h>

namespace Afina {
namespace Backend {

/**
 * # Map based implementation with shared lock
 *
 *
 */
class MapBasedSharedLockImpl : public Afina::Storage {
public:
    MapBasedSharedLockImpl(size_t max_size = 1024) : _max_size(max_size), _now(0), _last(""), _previous(""), started(false) {}
    ~MapBasedSharedLockImpl() {}

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
    void Insert(const std::string &key, const std::string &value);
    void Erase(const std::string &key);
    typedef std::unordered_map<std::string, std::tuple<std::string, std::string, std::string>> StorageMap;
    typedef std::tuple<std::string, std::string, std::string> StorageValue;
    typedef std::pair<std::string, std::tuple<std::string, std::string, std::string>> StoragePair;

    mutable std::shared_mutex _lock;

    size_t _max_size;
    size_t _now;

    StorageMap _backend;
    bool started;
    std::string _last;
    std::string _previous;
};

} // namespace Backend
} // namespace Afina

#endif // AFINA_STORAGE_MAP_BASED_GLOBAL_LOCK_IMPL_H
