#pragma once
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>

/*
==============================================================================
FILE: thread_pool.h
PURPOSE: Interface for the worker thread pool.
ARCH: STRICTLY DECLARATIONS ONLY.
==============================================================================
*/

class ThreadPool {
private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;

public:
    // ⚠️ NOTICE: Only semicolons here! The logic lives in thread_pool.cpp
    ThreadPool(size_t num_threads);
    ~ThreadPool();
    void enqueue(std::function<void()> task);
};