// utility/misc.hpp - Utility functions
#pragma once
#include <string>
#include <sstream>

// Returns just the name of a file
// e.g. ~/Code/Example.lean => Example
inline std::string get_filename(const std::string& file) {
    size_t slash_pos = file.rfind('/');
    size_t dot_pos = file.rfind('.');
    size_t name_start = (slash_pos == std::string::npos) ? 0 : slash_pos + 1;
    if (dot_pos == std::string::npos || dot_pos < slash_pos) {
        return file.substr(name_start);
    } else {
        return file.substr(name_start, dot_pos-name_start);
    }
}

// Throws an error if there are not enough arguments to handle a command line flag.
inline void ensure_args(int argp, int argc, char** argv, int count) {
    if (argp + count >= argc) {
        std::ostringstream err_msg;
        err_msg << "Option " << argv[argp] << " expected " << count
                << " arguments, received " << argc-argp-1;
        throw std::invalid_argument(err_msg.str());
    }
}