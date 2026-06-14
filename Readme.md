# Elite Mini-Redis: High-Performance Caching Engine

An advanced, multi-threaded, in-memory caching engine built from scratch in modern C++. Designed to mirror the architecture of real-world Redis, this engine utilizes a Hybrid Threading model, custom Data Structures, and OS-level optimizations to serve concurrent workloads with extreme throughput and sub-millisecond latency.

## Elite System Architecture

Unlike simple lock-based servers, this engine utilizes a **Hybrid Networking Architecture** (similar to Redis v6+):
1. **Event Loop (`poll`)**: The Main Thread exclusively handles the Event Loop, monitoring thousands of connected clients simultaneously without context-switching.
2. **I/O Thread Pool**: Network parsing is CPU-intensive. When data arrives, the Main Thread delegates socket reads and RESP parsing to background I/O threads.
3. **Lock-Free Execution**: Parsed commands are fed back to the Main Thread via a queue. Because *only* the Main Thread modifies the database, the core cache operates entirely **without locks**, preventing thread contention and massive latency spikes.

## Custom Data Structures & Memory Management
We abandoned `std::unordered_map` and standard containers to build a high-performance foundation from scratch:
- **Hash Table with Chaining**: A custom dynamic array resolving collisions via linked lists.
- **LRU Cache (Least Recently Used)**: Built-in $O(1)$ cache eviction using a Doubly Linked List intertwined with the Hash Table.
- **Skip Lists (`ZSET`)**: A probabilistic multi-layered linked list enabling $O(\log N)$ inserts, deletes, and range queries for Sorted Sets.
- **TTL (Time-To-Live)**: Lazy evaluation for key expiration.

## Persistence Models
- **RDB Snapshotting (`BGSAVE`)**: Utilizes the POSIX `fork()` system call to create a Copy-On-Write (COW) clone of the database in memory, serializing it to disk in the background with zero blocking to the main server.
- **AOF (Append-Only File)**: A dedicated background thread flushes mutation commands to disk asynchronously.

## Quick Start (Docker)

The server is fully containerized to guarantee execution across any host OS, bypassing local dependency hell (and enabling Linux-specific features like `fork()`).

### 1. Build the Engine
```bash
docker build -t mini-redis:prod .
```

### 2. Boot the Server
```bash
docker run -d --rm -p 6379:6379 --name redis_backend mini-redis:prod
```

### 3. Connect via TCP Client
Use `netcat` or `telnet` to connect to the raw socket:
```bash
nc localhost 6379
```

## Supported Commands

| Command | Syntax | Complexity | Description |
| :--- | :--- | :--- | :--- |
| **SET** | `SET <key> <value> [EX seconds]` | $O(1)$ | Stores a string value. Supports optional TTL. |
| **GET** | `GET <key>` | $O(1)$ | Retrieves a value. Returns nil if missing or expired. |
| **DEL** | `DEL <key>` | $O(1)$ | Deletes a key from memory. |
| **ZADD**| `ZADD <key> <score>` | $O(\log N)$ | Adds a value to the global Skip List with a given score. |
| **BGSAVE**| `BGSAVE` | $O(N)$ | Forks a background process to save an RDB snapshot. |

Developed as an elite deep-dive into Operating Systems, Networking, and Low-Level Algorithm Design.