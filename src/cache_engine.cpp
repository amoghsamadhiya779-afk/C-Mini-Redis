#include "../include/cache_engine.h"

CacheEngine& CacheEngine::getInstance() {
    // Static local variable is guaranteed thread-safe by the C++11 standard
    static CacheEngine instance;
    return instance;
}

void CacheEngine::set(const std::string& key, const std::string& value) {
    std::unique_lock<std::shared_mutex> lock(rw_lock);
    store[key] = value;
}

std::optional<std::string> CacheEngine::get(const std::string& key) const {
    std::shared_lock<std::shared_mutex> lock(rw_lock);
    auto it = store.find(key);
    if (it != store.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool CacheEngine::del(const std::string& key) {
    std::unique_lock<std::shared_mutex> lock(rw_lock);
    return store.erase(key) > 0;
}