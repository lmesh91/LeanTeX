// utility.hpp - Utility functions
#include <string>

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