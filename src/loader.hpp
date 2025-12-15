// loader.hpp - Loads all dependencies/options for LeanTeX to operate
#include <string>

class Loader {
private:
    // todo - get Jixia path from an INI file
    // todo - automatically download and compile Jixia from GitHub
    std::string jixia_path = "~/Programs/jixia/.lake/build/bin/jixia";
    std::string code_path;
public:
    Loader(std::string path);
    void run_jixia();
    void set_jixia_path(std::string path);
};