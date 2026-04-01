⚡ Mini-Redis: High-Performance Concurrent Key-Value Store

A multi-threaded, in-memory caching engine built from scratch in modern C++17. Designed to bypass standard disk I/O bottlenecks, this engine directly manipulates POSIX TCP sockets to serve concurrent read/write workloads with sub-millisecond latency.

🏗️ System Architecture

Unlike standard single-threaded Redis, this engine utilizes a Pre-booted Worker Thread Pool and fine-grained locking mechanisms to handle massive concurrent traffic without OS-level context-switching overhead.

Network Layer: Raw POSIX TCP sockets with SO_REUSEADDR for immediate port rebinding.

Concurrency Model: Lock-based concurrency using std::shared_mutex (Readers-Writer lock). Allows infinite simultaneous GET requests while safely isolating exclusive SET and DEL operations.

Command Parsing: Custom RESP (Redis Serialization Protocol) parser implementing the Factory and Command Design Patterns. Ensures strict adherence to the Open/Closed Principle (OCP)—new database commands can be added without modifying core parsing logic.

Memory Safety: Strict adherence to RAII, utilizing std::unique_ptr and explicit copy-constructor deletion to prevent memory leaks and dangling pointers.

🚀 Quick Start (Docker)

The server is fully containerized to guarantee execution across any host OS, bypassing local dependency hell.

1. Build the Engine

docker build -t mini-redis:prod .


2. Boot the Server

docker run -d --rm -p 6379:6379 --name redis_backend mini-redis:prod


3. Connect via TCP Client

Use netcat or telnet to connect to the raw socket:

nc localhost 6379


💻 Supported Commands

Command

Syntax

Time Complexity

Description

SET

SET <key> <value>

$O(1)$

Stores a string value via Exclusive Write Lock.

GET

GET <key>

$O(1)$

Retrieves a value via Shared Read Lock. Returns -1 if missing.

DEL

DEL <key>

$O(1)$

Deletes a key from memory. Returns 1 if successful, 0 if not.

🛠️ Low-Level Design (LLD) Details

The Spurious Wakeup Trap: The Thread Pool leverages std::condition_variable wrapped in a strict while loop (rather than an if statement) to protect against OS-level spurious wakeups, guaranteeing thread safety when popping tasks from the queue.

Return Type Deduction: Lambda-based Factory registration utilizes explicit trailing return types (-> std::unique_ptr<ICommand>) to resolve template deduction conflicts during compilation.

🔮 Roadmap

[ ] AOF Persistence: Implement an Observer Pattern to asynchronously log mutations to an Append Only File via a dedicated I/O background thread.

[ ] TTL Expiration: Lazy evaluation combined with a Min-Heap active sweeper thread for $O(1)$ key eviction.

[ ] Eviction Policies: LRU (Least Recently Used) cache eviction using a Doubly Linked List + Hash Map when memory capacity is reached.

Developed as a deep-dive into Operating Systems, Systems Programming, and C++ Memory Management.