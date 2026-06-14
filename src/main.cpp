#include "../include/server.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    int port = 6379;
    std::string master_host = "";
    int master_port = 0;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--replicaof" && i + 2 < argc) {
            master_host = argv[++i];
            master_port = std::stoi(argv[++i]);
        }
    }

    Server redis_server(port, 10);
    
    if (!master_host.empty()) {
        redis_server.set_replica_of(master_host, master_port);
    }

    redis_server.start();
    return 0;
}