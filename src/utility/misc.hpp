// utility/misc.hpp - Utility functions
#include <string>
#include <sstream>

// Returns just the name of a file
// e.g. ~/Code/Example.lean => Example
std::string get_filename(std::string file) {
    size_t slash_pos = file.rfind('/');
    size_t dot_pos = file.rfind('.');
    if (dot_pos < slash_pos) {
        return file.substr(slash_pos+1);
    } else {
        return file.substr(slash_pos+1, dot_pos-slash_pos-1);
    }
};

// Throws an error if there are not enough arguments to handle a command line flag.
void ensure_args(int argp, int argc, char** argv, int count) {
    if (argp + count >= argc) {
        std::ostringstream err_msg;
        err_msg << "Option " << argv[argp] << " expected " << count
                << " arguments, received " << argc-argp-1;
        throw std::invalid_argument(err_msg.str());
    }
};