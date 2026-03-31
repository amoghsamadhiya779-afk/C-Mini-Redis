#pragma once
#include <unordered_map>
#include <shared_mutex>
#include <mutex>
#include <string>
#include <optional>

/*
==============================================================================
FILE: cache_engine.h
PURPOSE: Interface for the core storage engine.
ARCH: STRICTLY DECLARATIONS ONLY. No logic.
==============================================================================
*/

class CacheEngine {
private:
    std::unordered_map<std::string, std::string> store;
    mutable std::shared_mutex rw_lock; 

    // Private constructor for Singleton
    CacheEngine() = default;

public:
    // Prevent cloning
    CacheEngine(const CacheEngine&) = delete;
    CacheEngine& operator=(const CacheEngine&) = delete;

    // Meyers' Singleton Accessor
    static CacheEngine& getInstance();

    // ⚠️ NOTICE: Only semicolons here! The logic lives in cache_engine.cpp
    void set(const std::string& key, const std::string& value);
    std::optional<std::string> get(const std::string& key) const;
    bool del(const std::string& key);
};