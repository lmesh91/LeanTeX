// loader.hpp - Loads all dependencies/options for LeanTeX to operate
#include <string>
#include <unordered_map>
#include <functional>

class Loader;

// Arguments: Position of flag, argc, argv, loader class
typedef std::function<void(int&, int, char**, Loader&)> CLIParser;

class Loader {
private:
    static const std::unordered_map<std::string, CLIParser> CLI_OPTIONS;
    // todo - INI file parsing
    // todo - automatically download and compile Jixia from GitHub
    std::unordered_map<std::string, std::string> options; 
public:
    Loader();
    bool initialize();
    void run_jixia();
    std::string get_option(std::string option);
    void set_option(std::string option, std::string value) noexcept;
    void parse_argument(int& argp, int argc, char** argv);
};