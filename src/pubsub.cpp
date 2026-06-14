#include "../include/pubsub.h"

PubSubEngine& PubSubEngine::getInstance() {
    static PubSubEngine instance;
    return instance;
}

void PubSubEngine::subscribe(int client_fd, const std::string& channel) {
    auto& subs = channels[channel];
    if (std::find(subs.begin(), subs.end(), client_fd) == subs.end()) {
        subs.push_back(client_fd);
        client_channels[client_fd].push_back(channel);
    }
}

void PubSubEngine::unsubscribe(int client_fd) {
    if (client_channels.find(client_fd) == client_channels.end()) return;

    for (const std::string& channel : client_channels[client_fd]) {
        auto& subs = channels[channel];
        subs.erase(std::remove(subs.begin(), subs.end(), client_fd), subs.end());
    }
    client_channels.erase(client_fd);
}

std::vector<int> PubSubEngine::get_subscribers(const std::string& channel) {
    if (channels.find(channel) != channels.end()) {
        return channels[channel];
    }
    return {};
}
