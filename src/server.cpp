#include "../include/server.h"
#include "../include/pubsub.h"
#include <iostream>
#include <algorithm>
#include <cstring>
#include <fcntl.h>

#ifndef _WIN32
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
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

Server::Server(int p, int threads) 
    : port(p), io_pool(threads), master_port(0), master_socket(-1), is_replica(false) {}

void Server::set_replica_of(std::string host, int p) {
    master_host = host;
    master_port = p;
    is_replica = true;
}

void Server::connect_to_master() {
    if (!is_replica) return;

    master_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(master_port);

    if (inet_pton(AF_INET, master_host.c_str(), &serv_addr.sin_addr) <= 0) {
        inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);
    }

    if (connect(master_socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "❌ Failed to connect to Leader at " << master_host << ":" << master_port << "\n";
        master_socket = -1;
        is_replica = false;
        return;
    }

    set_nonblocking(master_socket);

    struct pollfd pfd;
    pfd.fd = master_socket;
    pfd.events = POLLIN;
    pfd.revents = 0;
    fds.push_back(pfd);

    client_parsers[master_socket] = std::make_unique<ClientParser>();
    client_reading[master_socket] = false;

    std::string handshake = "*1\r\n$9\r\nREPLICAOF\r\n";
#ifndef _WIN32
    send(master_socket, handshake.c_str(), handshake.length(), 0);
#else
    send(master_socket, handshake.c_str(), handshake.length(), 0);
#endif

    std::cout << "🔗 Connected to Leader node. Running as Follower.\n";
}

void Server::broadcast_to_replicas(const std::string& resp_cmd) {
    for (int rep_sock : replicas) {
#ifndef _WIN32
        send(rep_sock, resp_cmd.c_str(), resp_cmd.length(), 0);
#else
        send(rep_sock, resp_cmd.c_str(), resp_cmd.length(), 0);
#endif
    }
}

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
            auto& parser = client_parsers[client_socket];
            while (true) {
                auto cmd = parser->parse(data);
                data.clear();
                
                if (cmd) {
                    cmd->set_client(client_socket); // Give command context of who sent it
                    std::lock_guard<std::mutex> lock(cq_mutex);
                    command_queue.push({std::move(cmd), client_socket});
                } else {
                    break;
                }
            }
        } else if (bytes_read < 0) {
#ifndef _WIN32
            if (err == EAGAIN || err == EWOULDBLOCK) break;
#else
            if (err == WSAEWOULDBLOCK) break;
#endif
            disconnect = true;
            break;
        } else if (bytes_read == 0) {
            disconnect = true;
            break;
        }
    }

    if (disconnect) {
        std::lock_guard<std::mutex> lock(cq_mutex);
        command_queue.push({nullptr, client_socket});
    }

    {
        std::lock_guard<std::mutex> lk(reading_mutex);
        client_reading[client_socket] = false;
    }
}

void Server::execute_commands() {
    std::lock_guard<std::mutex> lock(cq_mutex);
    while (!command_queue.empty()) {
        auto task = std::move(command_queue.front());
        command_queue.pop();

        if (task.cmd == nullptr) {
            if (task.client_socket == master_socket) {
                std::cout << "❌ Connection to Leader lost!\n";
                master_socket = -1;
            } else {
                replicas.erase(std::remove(replicas.begin(), replicas.end(), task.client_socket), replicas.end());
                PubSubEngine::getInstance().unsubscribe(task.client_socket);
                std::cout << "Client disconnected: " << task.client_socket << "\n";
            }
            
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
            continue;
        }

        if (task.cmd->is_replicaof()) {
            replicas.push_back(task.client_socket);
            std::cout << "🔗 New Follower connected (FD: " << task.client_socket << ")\n";
        }

        std::string response = task.cmd->execute();

        // PubSub Broadcasting
        for (const auto& bcast : task.cmd->get_broadcasts()) {
#ifndef _WIN32
            send(bcast.fd, bcast.msg.c_str(), bcast.msg.length(), 0);
#else
            send(bcast.fd, bcast.msg.c_str(), bcast.msg.length(), 0);
#endif
        }

        if (task.cmd->is_write()) {
            broadcast_to_replicas(task.cmd->to_resp());
        }
        
        if (task.client_socket != master_socket) {
#ifndef _WIN32
            send(task.client_socket, response.c_str(), response.length(), 0);
#else
            send(task.client_socket, response.c_str(), response.length(), 0);
#endif
        }
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

    connect_to_master();

    while (true) {
#ifndef _WIN32
        int poll_count = poll(fds.data(), fds.size(), 10);
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
                        
                        if (dispatch) {
                            io_pool.enqueue([this, client_socket] { 
                                this->handle_client_read(client_socket); 
                            });
                        }
                    }
                }
            }
        }
        
        execute_commands();
    }
}