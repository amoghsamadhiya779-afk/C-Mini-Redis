#include "../include/parser.h"
#include "../include/cache_engine.h"
#include <sstream>

// Helper to convert args back to raw RESP format for replication broadcast
std::string to_resp_array(const std::vector<std::string>& args) {
    std::string resp = "*" + std::to_string(args.size()) + "\r\n";
    for (const auto& arg : args) {
        resp += "$" + std::to_string(arg.length()) + "\r\n" + arg + "\r\n";
    }
    return resp;
}

// ---------------------------------------------------------
// COMMAND IMPLEMENTATIONS
// ---------------------------------------------------------
class SetCommand : public ICommand {
    std::string key, val;
    long long exp;
    std::vector<std::string> raw_args;
public:
    SetCommand(const std::vector<std::string>& args, std::string k, std::string v, long long e=0) 
        : key(k), val(v), exp(e), raw_args(args) {}
    
    std::string execute() override {
        CacheEngine::getInstance().set(key, val, exp);
        return "+OK\r\n";
    }
    bool is_write() const override { return true; }
    std::string to_resp() const override { return to_resp_array(raw_args); }
};

class GetCommand : public ICommand {
    std::string key;
public:
    GetCommand(std::string k) : key(k) {}
    std::string execute() override {
        auto val = CacheEngine::getInstance().get(key);
        if (val) return "$" + std::to_string(val->length()) + "\r\n" + *val + "\r\n";
        return "$-1\r\n"; // Nil
    }
};

class DelCommand : public ICommand {
    std::string key;
    std::vector<std::string> raw_args;
public:
    DelCommand(const std::vector<std::string>& args, std::string k) : key(k), raw_args(args) {}
    std::string execute() override {
        return CacheEngine::getInstance().del(key) ? ":1\r\n" : ":0\r\n";
    }
    bool is_write() const override { return true; }
    std::string to_resp() const override { return to_resp_array(raw_args); }
};

class ZAddCommand : public ICommand {
    std::string key; 
    double score;
    std::vector<std::string> raw_args;
public:
    ZAddCommand(const std::vector<std::string>& args, std::string k, double s) : key(k), score(s), raw_args(args) {}
    std::string execute() override {
        CacheEngine::getInstance().zadd(key, score);
        return ":1\r\n";
    }
    bool is_write() const override { return true; }
    std::string to_resp() const override { return to_resp_array(raw_args); }
};

class BgSaveCommand : public ICommand {
public:
    std::string execute() override {
        CacheEngine::getInstance().bgsave();
        return "+Background saving started\r\n";
    }
};

class ReplicaOfCommand : public ICommand {
public:
    std::string execute() override {
        return "+OK\r\n";
    }
    bool is_replicaof() const override { return true; }
};

// ---------------------------------------------------------
// COMMAND FACTORY
// ---------------------------------------------------------
CommandFactory::CommandFactory() {
    factory["SET"] = [](const std::vector<std::string>& args) -> std::unique_ptr<ICommand> {
        if (args.size() < 3) return nullptr;
        long long exp = 0;
        if (args.size() >= 5 && args[3] == "EX") exp = std::stoll(args[4]);
        return std::make_unique<SetCommand>(args, args[1], args[2], exp);
    };
    factory["GET"] = [](const std::vector<std::string>& args) -> std::unique_ptr<ICommand> {
        if (args.size() < 2) return nullptr;
        return std::make_unique<GetCommand>(args[1]);
    };
    factory["DEL"] = [](const std::vector<std::string>& args) -> std::unique_ptr<ICommand> {
        if (args.size() < 2) return nullptr;
        return std::make_unique<DelCommand>(args, args[1]);
    };
    factory["ZADD"] = [](const std::vector<std::string>& args) -> std::unique_ptr<ICommand> {
        if (args.size() < 3) return nullptr;
        return std::make_unique<ZAddCommand>(args, args[1], std::stod(args[2]));
    };
    factory["BGSAVE"] = [](const std::vector<std::string>& args) -> std::unique_ptr<ICommand> {
        return std::make_unique<BgSaveCommand>();
    };
    factory["REPLICAOF"] = [](const std::vector<std::string>& args) -> std::unique_ptr<ICommand> {
        return std::make_unique<ReplicaOfCommand>();
    };
}

CommandFactory& CommandFactory::getInstance() {
    static CommandFactory instance;
    return instance;
}

std::unique_ptr<ICommand> CommandFactory::create(const std::vector<std::string>& args) {
    if (args.empty()) return nullptr;
    auto it = factory.find(args[0]);
    if (it != factory.end()) return it->second(args);
    return nullptr;
}

// ---------------------------------------------------------
// STATEFUL RESP PARSER
// ---------------------------------------------------------
ClientParser::ClientParser() { reset(); }

void ClientParser::reset() {
    expected_args = 0;
    expected_len = 0;
    current_args.clear();
    state = ARRAY_LEN;
}

std::unique_ptr<ICommand> ClientParser::parse(const std::string& raw_data) {
    buffer += raw_data;
    
    if (!buffer.empty() && buffer.front() != '*') {
        size_t pos = buffer.find("\r\n");
        if (pos != std::string::npos) {
            std::istringstream iss(buffer.substr(0, pos));
            std::string token;
            while (iss >> token) current_args.push_back(token);
            buffer.erase(0, pos + 2);
            auto cmd = CommandFactory::getInstance().create(current_args);
            reset();
            return cmd;
        }
        return nullptr;
    }

    while (!buffer.empty()) {
        size_t pos = buffer.find("\r\n");
        if (pos == std::string::npos) return nullptr;

        std::string line = buffer.substr(0, pos);

        if (state == ARRAY_LEN) {
            if (line.empty() || line[0] != '*') { reset(); return nullptr; }
            expected_args = std::stoi(line.substr(1));
            state = BULK_LEN;
        } 
        else if (state == BULK_LEN) {
            if (line.empty() || line[0] != '$') { reset(); return nullptr; }
            expected_len = std::stoi(line.substr(1));
            state = BULK_STR;
        } 
        else if (state == BULK_STR) {
            current_args.push_back(line);
            state = BULK_LEN;

            if (current_args.size() == expected_args) {
                auto cmd = CommandFactory::getInstance().create(current_args);
                buffer.erase(0, pos + 2);
                reset();
                return cmd;
            }
        }
        buffer.erase(0, pos + 2);
    }
    return nullptr;
}