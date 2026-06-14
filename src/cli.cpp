#include <iostream>
#include <string>
#include <vector>
#include <sstream>

#ifndef _WIN32
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

std::string format_resp(const std::string& input) {
    std::istringstream iss(input);
    std::vector<std::string> args;
    std::string token;
    while (iss >> token) args.push_back(token);

    if (args.empty()) return "";

    std::string resp = "*" + std::to_string(args.size()) + "\r\n";
    for (const auto& arg : args) {
        resp += "$" + std::to_string(arg.length()) + "\r\n" + arg + "\r\n";
    }
    return resp;
}

int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    int port = 6379;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-h" && i + 1 < argc) host = argv[++i];
        if (arg == "-p" && i + 1 < argc) port = std::stoi(argv[++i]);
    }

#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Could not connect to Redis at " << host << ":" << port << "\n";
        return 1;
    }

    std::cout << "Connected to Mini-Redis at " << host << ":" << port << "\n";
    
    std::string line;
    char buffer[4096];
    while (true) {
        std::cout << host << ":" << port << "> ";
        if (!std::getline(std::cin, line)) break;
        if (line == "exit" || line == "quit") break;

        std::string resp = format_resp(line);
        if (resp.empty()) continue;

#ifndef _WIN32
        send(sock, resp.c_str(), resp.length(), 0);
        int bytes = read(sock, buffer, sizeof(buffer) - 1);
#else
        send(sock, resp.c_str(), resp.length(), 0);
        int bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
#endif
        if (bytes > 0) {
            buffer[bytes] = '\0';
            std::cout << buffer;
        } else {
            std::cout << "Connection closed by server.\n";
            break;
        }
    }

#ifndef _WIN32
    close(sock);
#else
    closesocket(sock);
#endif
    return 0;
}
