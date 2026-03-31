#include "../include/parser.h"
#include "../include/cache_engine.h"
#include <sstream>

// --- Command Implementations ---
class SetCommand : public ICommand {
private:
    std::string key, value;
public:
    SetCommand(std::string k, std::string v) : key(k), value(v) {}
    std::string execute() override {
        CacheEngine::getInstance().set(key, value);
        return "+OK\r\n"; 
    }
};

class GetCommand : public ICommand {
private:
    std::string key;
public:
    GetCommand(std::string k) : key(k) {}
    std::string execute() override {
        auto val = CacheEngine::getInstance().get(key);
        if (val) return "$" + std::to_string(val->length()) + "\r\n" + *val + "\r\n";
        return "$-1\r\n"; 
    }
};

class DelCommand : public ICommand {
private:
    std::string key;
public:
    DelCommand(std::string k) : key(k) {}
    std::string execute() override {
        bool success = CacheEngine::getInstance().del(key);
        return success ? ":1\r\n" : ":0\r\n";
    }
};

// --- Parser Factory Logic ---
Parser::Parser() {
    // Notice the explicitly declared trailing return type: -> std::unique_ptr<ICommand>
    factory["SET"] = [](const std::vector<std::string>& args) -> std::unique_ptr<ICommand> {
        if (args.size() < 3) return nullptr; // Now we can just return a clean nullptr!
        return std::make_unique<SetCommand>(args[1], args[2]);
    };
    
    factory["GET"] = [](const std::vector<std::string>& args) -> std::unique_ptr<ICommand> {
        if (args.size() < 2) return nullptr;
        return std::make_unique<GetCommand>(args[1]);
    };

    factory["DEL"] = [](const std::vector<std::string>& args) -> std::unique_ptr<ICommand> {
        if (args.size() < 2) return nullptr;
        return std::make_unique<DelCommand>(args[1]);
    };
}

std::unique_ptr<ICommand> Parser::parse(const std::string& tcp_payload) {
    std::istringstream stream(tcp_payload);
    std::string token;
    std::vector<std::string> args;

    while (stream >> token) args.push_back(token);

    if (args.empty()) return nullptr;

    auto it = factory.find(args[0]);
    if (it != factory.end()) return it->second(args);
    
    return nullptr; 
}