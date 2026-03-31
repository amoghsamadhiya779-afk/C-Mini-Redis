#include "../include/server.h"
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUFFER_SIZE 1024

Server::Server(int p, int threads) : port(p), pool(threads) {}

void Server::handle_client(int client_socket) {
    char raw_buffer[BUFFER_SIZE] = {0};
    std::string tcp_stream_buffer = "";

    while (true) {
        ssize_t bytes_read = read(client_socket, raw_buffer, BUFFER_SIZE - 1);
        if (bytes_read <= 0) break; // Client disconnected

        tcp_stream_buffer.append(raw_buffer, bytes_read);

        // TCP Fragmentation safely handled using \r\n delimiter
        size_t pos;
        while ((pos = tcp_stream_buffer.find("\r\n")) != std::string::npos) {
            std::string full_command = tcp_stream_buffer.substr(0, pos);
            tcp_stream_buffer.erase(0, pos + 2); // Remove processed command from buffer

            auto cmd = command_parser.parse(full_command);
            if (cmd) {
                std::string response = cmd->execute();
                send(client_socket, response.c_str(), response.length(), 0);
            } else {
                std::string error = "-ERR unknown command\r\n";
                send(client_socket, error.c_str(), error.length(), 0);
            }
        }
    }
    close(client_socket); 
}

void Server::start() {
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    
    // FAANG Requirement: Allows immediate port reuse if the server crashes
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        std::cerr << "Bind failed!\n";
        return;
    }

    listen(server_fd, 100);
    std::cout << "⚡ Mini-Redis Engine listening on Port " << port << "...\n";

    while (true) {
        int addrlen = sizeof(address);
        int client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        
        if (client_socket >= 0) {
            pool.enqueue([this, client_socket] { this->handle_client(client_socket); });
        }
    }
}