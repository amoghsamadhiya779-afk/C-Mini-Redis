#pragma once
#include "thread_pool.h"
#include "parser.h"

class Server {
private:
    int port;
    int server_fd;
    ThreadPool pool;
    Parser command_parser;

    void handle_client(int client_socket);

public:
    Server(int p, int threads);
    void start();
};