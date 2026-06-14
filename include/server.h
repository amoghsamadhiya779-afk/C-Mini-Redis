#pragma once
#include "thread_pool.h"
#include "parser.h"
#include <unordered_map>
#include <memory>
#include <queue>
#include <mutex>
#include <vector>
#include <string>

#ifndef _WIN32
#include <poll.h>
#else
#include <winsock2.h>
#endif

struct CommandTask {
    std::unique_ptr<ICommand> cmd;
    int client_socket;
};

class Server {
private:
    int port;
    int server_fd;
    ThreadPool io_pool;
    
    // Networking
    std::vector<struct pollfd> fds;
    std::unordered_map<int, std::unique_ptr<ClientParser>> client_parsers;
    
    std::mutex reading_mutex;
    std::unordered_map<int, bool> client_reading; 
    
    // Execution Queue
    std::queue<CommandTask> command_queue;
    std::mutex cq_mutex;

    // Replication
    std::string master_host;
    int master_port;
    int master_socket;
    bool is_replica;
    std::vector<int> replicas;

    void accept_client();
    void handle_client_read(int client_socket);
    void execute_commands();
    void connect_to_master();
    void broadcast_to_replicas(const std::string& resp_cmd);

public:
    Server(int p, int io_threads);
    void set_replica_of(std::string host, int p);
    void start();
};