// loader.hpp - Loads all dependencies/options for LeanTeX to operate
#pragma once
#include <string>
#include <unordered_map>
#include <functional>

class Loader;

// Arguments: Position of flag, argc, argv, loader class
using CLIParser = std::function<void(int&, int, char**, Loader&)>;

class Loader {
private:
    static const std::unordered_map<std::string, CLIParser> CLI_OPTIONS;
    // todo - automatically download and compile Jixia from GitHub
    std::unordered_map<std::string, std::string> options;
    // Keeps track of if an option was set by the user.
    // This allows the INI file to override defaults but not user settings.
    std::unordered_map<std::string, bool> tampered;
public:
    Loader();
    bool initialize();
    void run_jixia();
    void convert();
    std::string get_option(const std::string& option);
    void load_ini();
    void set_option(const std::string& option, const std::string& value) noexcept;
    bool is_tampered(const std::string& option);
    void parse_argument(int& argp, int argc, char** argv);
};