#include "../include/server.h"
#include <iostream>
#include <algorithm>
#include <cstring>
#include <fcntl.h>

#ifndef _WIN32
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#else
#pragma comment(lib, "ws2_32.lib")
#endif

#define BUFFER_SIZE 4096

void set_nonblocking(int sock) {
#ifndef _WIN32
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#else
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
#endif
}

Server::Server(int p, int threads) : port(p), io_pool(threads) {}

void Server::accept_client() {
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    int client_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
    
    if (client_socket >= 0) {
        set_nonblocking(client_socket);
        struct pollfd pfd;
        pfd.fd = client_socket;
        pfd.events = POLLIN;
        pfd.revents = 0;
        fds.push_back(pfd);
        
        client_parsers[client_socket] = std::make_unique<ClientParser>();
        
        std::lock_guard<std::mutex> lk(reading_mutex);
        client_reading[client_socket] = false;
        
        std::cout << "Client connected: " << client_socket << "\n";
    }
}

void Server::handle_client_read(int client_socket) {
    char raw_buffer[BUFFER_SIZE] = {0};
    bool disconnect = false;

    while (true) {
#ifndef _WIN32
        ssize_t bytes_read = read(client_socket, raw_buffer, BUFFER_SIZE - 1);
        int err = errno;
#else
        int bytes_read = recv(client_socket, raw_buffer, BUFFER_SIZE - 1, 0);
        int err = WSAGetLastError();
#endif

        if (bytes_read > 0) {
            std::string data(raw_buffer, bytes_read);
            
            // Feed to parser
            auto& parser = client_parsers[client_socket];
            while (true) {
                auto cmd = parser->parse(data);
                data.clear(); // Only feed once, loop until parse returns nullptr
                
                if (cmd) {
                    std::lock_guard<std::mutex> lock(cq_mutex);
                    command_queue.push({std::move(cmd), client_socket});
                } else {
                    break;
                }
            }
        } else if (bytes_read < 0) {
#ifndef _WIN32
            if (err == EAGAIN || err == EWOULDBLOCK) {
#else
            if (err == WSAEWOULDBLOCK) {
#endif
                break; // No more data to read right now
            } else {
                disconnect = true;
                break; // Real error
            }
        } else if (bytes_read == 0) {
            disconnect = true;
            break; // Client closed connection
        }
    }

    if (disconnect) {
        std::lock_guard<std::mutex> lock(cq_mutex);
        command_queue.push({nullptr, client_socket}); // Signal main thread to disconnect
    }

    {
        std::lock_guard<std::mutex> lk(reading_mutex);
        client_reading[client_socket] = false; // Release the I/O lock
    }
}

void Server::execute_commands() {
    std::lock_guard<std::mutex> lock(cq_mutex);
    while (!command_queue.empty()) {
        auto task = std::move(command_queue.front());
        command_queue.pop();

        if (task.cmd == nullptr) {
            // Disconnect signal
#ifndef _WIN32
            close(task.client_socket);
#else
            closesocket(task.client_socket);
#endif
            client_parsers.erase(task.client_socket);
            
            std::lock_guard<std::mutex> lk(reading_mutex);
            client_reading.erase(task.client_socket);
            
            fds.erase(std::remove_if(fds.begin(), fds.end(), 
                [&](const pollfd& p) { return p.fd == task.client_socket; }), fds.end());
            
            std::cout << "Client disconnected: " << task.client_socket << "\n";
            continue;
        }

        // Execute Command on MAIN THREAD exclusively (Lock-Free Database!)
        std::string response = task.cmd->execute();
        
#ifndef _WIN32
        send(task.client_socket, response.c_str(), response.length(), 0);
#else
        send(task.client_socket, response.c_str(), response.length(), 0);
#endif
    }
}

void Server::start() {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    
#ifndef _WIN32
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
#else
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
#endif

    set_nonblocking(server_fd);

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        std::cerr << "Bind failed!\n";
        return;
    }

    listen(server_fd, 10000);
    std::cout << "⚡ Elite Engine listening on Port " << port << "...\n";

    struct pollfd pfd;
    pfd.fd = server_fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    fds.push_back(pfd);

    while (true) {
#ifndef _WIN32
        int poll_count = poll(fds.data(), fds.size(), 10); // 10ms timeout
#else
        int poll_count = WSAPoll(fds.data(), fds.size(), 10);
#endif

        if (poll_count > 0) {
            for (size_t i = 0; i < fds.size(); i++) {
                if (fds[i].revents & POLLIN) {
                    if (fds[i].fd == server_fd) {
                        accept_client();
                    } else {
                        int client_socket = fds[i].fd;
                        bool dispatch = false;
                        
                        {
                            std::lock_guard<std::mutex> lk(reading_mutex);
                            if (!client_reading[client_socket]) {
                                client_reading[client_socket] = true;
                                dispatch = true;
                            }
                        }
                        
                        // Farm network parsing out to the I/O Thread Pool
                        if (dispatch) {
                            io_pool.enqueue([this, client_socket] { 
                                this->handle_client_read(client_socket); 
                            });
                        }
                    }
                }
            }
        }
        
        // Single-Threaded execution of all parsed commands
        execute_commands();
    }
}