#pragma once 
#include <string>
#include <vector>
#include <memory>
#include  <unordered_map>
#include <functional>
//1.Command Interface
class ICommand{
    public:
    virtual std::string execute()=0;
    virtual ~ICommand() = default;

};

//2. Parser Factory Class
class Parser {
    private:
    using CreatorFunc=std::function<std::unique_ptr<ICommand>(const std::vector<std::string>&)>;
    std::unordered_map<std::string, CreatorFunc> factory;

public:
    Parser();
    std::unique_ptr<ICommand> parse(const std::string& tcp_payload);
};