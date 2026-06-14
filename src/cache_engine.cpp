#include "../include/cache_engine.h"
#include <sstream>
#include <iostream>
#include <cstdio>
#ifndef _WIN32
#include <unistd.h>
#endif

CacheEngine& CacheEngine::getInstance() {
    static CacheEngine instance;
    return instance;
}

CacheEngine::CacheEngine() : stop_aof(false) {
    aof_file.open("appendonly.aof", std::ios::app);
    load_from_aof(); 
    aof_thread = std::thread(&CacheEngine::aof_worker, this);
}

CacheEngine::~CacheEngine() {
    {
        std::unique_lock<std::mutex> lock(aof_mutex);
        stop_aof = true;
    }
    aof_cv.notify_one();
    if (aof_thread.joinable()) aof_thread.join();
    if (aof_file.is_open()) aof_file.close();
}

void CacheEngine::aof_worker() {
    while (true) {
        std::string command;
        {
            std::unique_lock<std::mutex> lock(aof_mutex);
            aof_cv.wait(lock, [this] { return stop_aof || !aof_queue.empty(); });
            if (stop_aof && aof_queue.empty()) return;

            command = aof_queue.front();
            aof_queue.pop();
        }
        
        if (aof_file.is_open()) {
            aof_file << command << "\n";
            aof_file.flush(); 
        }
    }
}

// ---------------------------------------------------------
// SINGLE-THREADED EXECUTOR (No rw_locks required!)
// ---------------------------------------------------------

void CacheEngine::set(const std::string& key, const std::string& value, long long expire_at) {
    store.set(key, value, expire_at);
    
    std::unique_lock<std::mutex> lock(aof_mutex);
    std::string cmd = "SET " + key + " " + value;
    aof_queue.push(cmd);
    aof_cv.notify_one();
}

std::optional<std::string> CacheEngine::get(const std::string& key) {
    CacheNode* node = store.get(key);
    if (node) return node->value;
    return std::nullopt;
}

bool CacheEngine::del(const std::string& key) {
    bool erased = store.remove(key);
    if (erased) {
        std::unique_lock<std::mutex> lock(aof_mutex);
        aof_queue.push("DEL " + key);
        aof_cv.notify_one();
    }
    return erased;
}

void CacheEngine::zadd(const std::string& value, double score) {
    zset.insert(value, score);
    
    std::unique_lock<std::mutex> lock(aof_mutex);
    aof_queue.push("ZADD " + value + " " + std::to_string(score));
    aof_cv.notify_one();
}

std::vector<std::string> CacheEngine::zrange(double min_score, double max_score) {
    return zset.range(min_score, max_score);
}

// ---------------------------------------------------------
// RDB SNAPSHOT VIA OS FORK (COPY-ON-WRITE)
// ---------------------------------------------------------
void CacheEngine::bgsave() {
#ifndef _WIN32
    pid_t pid = fork();
    if (pid == 0) {
        // Child Process - gets instantaneous clone of memory via COW
        std::ofstream rdb("dump.rdb", std::ios::binary);
        auto snapshot = store.snapshot();
        for (const auto& pair : snapshot) {
            rdb << pair.first << ":" << pair.second << "\n";
        }
        rdb.close();
        std::cout << "\n💾 RDB BGSAVE complete. Background process exiting.\n";
        _exit(0); // Exit without calling C++ destructors for parent resources
    } else if (pid > 0) {
        // Parent Process
        std::cout << "🚀 Forked background process (PID: " << pid << ") for RDB Save.\n";
    } else {
        std::cerr << "❌ Fork failed!\n";
    }
#else
    std::cout << "⚠️ fork() is a POSIX system call. RDB skipped. Please run in Linux/Docker.\n";
#endif
}

void CacheEngine::load_from_aof() {
    std::ifstream file("appendonly.aof");
    if (!file.is_open()) return;

    std::string line, cmd, key, value;
    int restored = 0;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        iss >> cmd;
        if (cmd == "SET") {
            iss >> key >> value;
            store.set(key, value);
            restored++;
        } else if (cmd == "DEL") {
            iss >> key;
            store.remove(key);
        } else if (cmd == "ZADD") {
            double score;
            iss >> value >> score;
            zset.insert(value, score);
        }
    }
    std::cout << "💾 AOF Engine Online: Restored " << restored << " commands from disk.\n";
}