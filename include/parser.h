#pragma once 
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>

// Command Interface
class ICommand {
public:
    virtual std::string execute() = 0;
    virtual ~ICommand() = default;
};

// Singleton Command Factory
class CommandFactory {
private:
    using CreatorFunc = std::function<std::unique_ptr<ICommand>(const std::vector<std::string>&)>;
    std::unordered_map<std::string, CreatorFunc> factory;
    CommandFactory();

public:
    static CommandFactory& getInstance();
    std::unique_ptr<ICommand> create(const std::vector<std::string>& args);
};

// Stateful RESP Client Parser
class ClientParser {
private:
    std::string buffer;
    int expected_args;
    int expected_len;
    std::vector<std::string> current_args;

    enum State { ARRAY_LEN, BULK_LEN, BULK_STR };
    State state;

public:
    ClientParser();
    // Feeds data, returns a fully parsed command if ready, or nullptr if more data is needed
    std::unique_ptr<ICommand> parse(const std::string& raw_data);
    void reset();
};