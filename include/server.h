#pragma once
#include "thread_pool.h"
#include "parser.h"
#include <unordered_map>
#include <memory>
#include <queue>
#include <mutex>
#include <vector>

#ifndef _WIN32
#include <poll.h>
#else
// For native Windows compilation, though the project targets Docker
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
    
    // Prevent double I/O dispatch
    std::mutex reading_mutex;
    std::unordered_map<int, bool> client_reading; 
    
    // Execution Queue
    std::queue<CommandTask> command_queue;
    std::mutex cq_mutex;

    void accept_client();
    void handle_client_read(int client_socket);
    void execute_commands();

public:
    Server(int p, int io_threads);
    void start();
};