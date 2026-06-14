#include "../include/parser.h"
#include "../include/cache_engine.h"
#include <sstream>

// ---------------------------------------------------------
// COMMAND IMPLEMENTATIONS
// ---------------------------------------------------------
class SetCommand : public ICommand {
    std::string key, val;
    long long exp;
public:
    SetCommand(std::string k, std::string v, long long e=0) : key(k), val(v), exp(e) {}
    std::string execute() override {
        CacheEngine::getInstance().set(key, val, exp);
        return "+OK\r\n";
    }
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
public:
    DelCommand(std::string k) : key(k) {}
    std::string execute() override {
        return CacheEngine::getInstance().del(key) ? ":1\r\n" : ":0\r\n";
    }
};

class ZAddCommand : public ICommand {
    std::string key; 
    double score;
public:
    ZAddCommand(std::string k, double s) : key(k), score(s) {}
    std::string execute() override {
        CacheEngine::getInstance().zadd(key, score);
        return ":1\r\n";
    }
};

class BgSaveCommand : public ICommand {
public:
    std::string execute() override {
        CacheEngine::getInstance().bgsave();
        return "+Background saving started\r\n";
    }
};

// ---------------------------------------------------------
// COMMAND FACTORY
// ---------------------------------------------------------
CommandFactory::CommandFactory() {
    factory["SET"] = [](const std::vector<std::string>& args) -> std::unique_ptr<ICommand> {
        if (args.size() < 3) return nullptr;
        long long exp = 0;
        if (args.size() >= 5 && args[3] == "EX") exp = std::stoll(args[4]);
        return std::make_unique<SetCommand>(args[1], args[2], exp);
    };
    factory["GET"] = [](const std::vector<std::string>& args) -> std::unique_ptr<ICommand> {
        if (args.size() < 2) return nullptr;
        return std::make_unique<GetCommand>(args[1]);
    };
    factory["DEL"] = [](const std::vector<std::string>& args) -> std::unique_ptr<ICommand> {
        if (args.size() < 2) return nullptr;
        return std::make_unique<DelCommand>(args[1]);
    };
    factory["ZADD"] = [](const std::vector<std::string>& args) -> std::unique_ptr<ICommand> {
        if (args.size() < 3) return nullptr;
        return std::make_unique<ZAddCommand>(args[1], std::stod(args[2]));
    };
    factory["BGSAVE"] = [](const std::vector<std::string>& args) -> std::unique_ptr<ICommand> {
        return std::make_unique<BgSaveCommand>();
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
    
    // Support basic inline commands (like `SET key val\r\n` from netcat)
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
        return nullptr; // Wait for \r\n
    }

    // Stateful RESP Parsing
    while (!buffer.empty()) {
        size_t pos = buffer.find("\r\n");
        if (pos == std::string::npos) return nullptr; // Wait for more data

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