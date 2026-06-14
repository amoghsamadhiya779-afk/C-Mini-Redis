#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>

class PubSubEngine {
private:
    std::unordered_map<std::string, std::vector<int>> channels;
    std::unordered_map<int, std::vector<std::string>> client_channels;
    PubSubEngine() = default;

public:
    PubSubEngine(const PubSubEngine&) = delete;
    PubSubEngine& operator=(const PubSubEngine&) = delete;

    static PubSubEngine& getInstance();

    void subscribe(int client_fd, const std::string& channel);
    void unsubscribe(int client_fd);
    std::vector<int> get_subscribers(const std::string& channel);
};
