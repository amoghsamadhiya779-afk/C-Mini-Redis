# Elite Mini-Redis: Distributed Caching Engine

An advanced, multi-threaded, in-memory caching engine built from scratch in modern C++. Designed to mirror the architecture of real-world Redis, this engine utilizes a Hybrid Threading model, custom Data Structures, OS-level optimizations, and Distributed System Replication to serve concurrent workloads with extreme throughput and high availability.

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

## Distributed Systems: Replication, PubSub, & Persistence
- **Leader-Follower Replication**: Massive horizontal scaling through asynchronous replication. Any node can be booted as a Follower and automatically stream live data from the Leader.
- **Publish/Subscribe Engine**: A custom message broker allowing clients to subscribe to channels and instantly receive broadcast messages without database polling.
- **RDB Snapshotting (`BGSAVE`)**: Utilizes the POSIX `fork()` system call to create a Copy-On-Write (COW) clone of the database in memory, serializing it to disk in the background with zero blocking to the main server.
- **AOF (Append-Only File)**: A dedicated background thread flushes mutation commands to disk asynchronously.

## Quick Start (Docker Cluster)

The server is fully containerized. You can boot a distributed cluster using Docker.

### 1. Build the Engine & CLI
```bash
docker build -t mini-redis:prod .
```

### 2. Boot the Leader Node
```bash
docker run -d --rm --net host --name redis_leader mini-redis:prod ./mini_redis_server --port 6379
```

### 3. Connect via Custom CLI
Connect to the Leader to write data or test the **Pub/Sub Broker**:

**Terminal 1 (Subscriber):**
```bash
docker run -it --rm --net host mini-redis:prod ./mini_redis_cli -p 6379
127.0.0.1:6379> SUBSCRIBE sports
```

**Terminal 2 (Publisher):**
```bash
docker run -it --rm --net host mini-redis:prod ./mini_redis_cli -p 6379
127.0.0.1:6379> PUBLISH sports "Messi scores!"
```

Terminal 1 will instantly receive the live broadcast!

## Supported Commands

| Command | Syntax | Complexity | Description |
| :--- | :--- | :--- | :--- |
| **SET** | `SET <key> <value> [EX seconds]` | $O(1)$ | Stores a string value. Supports optional TTL. |
| **GET** | `GET <key>` | $O(1)$ | Retrieves a value. Returns nil if missing or expired. |
| **DEL** | `DEL <key>` | $O(1)$ | Deletes a key from memory. |
| **ZADD**| `ZADD <key> <score>` | $O(\log N)$ | Adds a value to the global Skip List with a given score. |
| **BGSAVE**| `BGSAVE` | $O(N)$ | Forks a background process to save an RDB snapshot. |
| **SUBSCRIBE**| `SUBSCRIBE <channel>` | $O(1)$ | Subscribes the client to a message broadcast channel. |
| **PUBLISH**| `PUBLISH <channel> <msg>` | $O(K)$ | Broadcasts a message to all K subscribed clients. |

Developed as an elite deep-dive into Distributed Systems, Operating Systems, Networking, and Low-Level Algorithm Design.