#pragma once 
#include "data_structures.h"
#include <mutex>
#include <optional>
#include <thread>
#include <queue>
#include <condition_variable>
#include <fstream>
#include <vector>

class CacheEngine {
private:
    LRUHashTable store;
    SkipList zset; // Global SkipList for simplicity in this project

    // AOF CORE
    std::queue<std::string> aof_queue;
    std::mutex aof_mutex;
    std::condition_variable aof_cv;
    std::thread aof_thread;
    bool stop_aof;
    std::ofstream aof_file;

    CacheEngine();
    ~CacheEngine();

    void aof_worker();
    void load_from_aof();

public:
    CacheEngine(const CacheEngine&) = delete;
    CacheEngine& operator=(const CacheEngine&) = delete;

    static CacheEngine& getInstance();

    // Key-Value Operations (Lock-Free for execution thread)
    void set(const std::string& key, const std::string& value, long long expire_at = 0);
    std::optional<std::string> get(const std::string& key);
    bool del(const std::string& key);
    
    // Sorted Set Operations
    void zadd(const std::string& value, double score);
    std::vector<std::string> zrange(double min_score, double max_score);

    // RDB Persistence
    void bgsave();
};
