// utility/misc.cpp - Utility functions
#include "utility/misc.hpp"
#include <sstream>
#include <fstream>
#include <iostream>

// Returns just the name of a file
// e.g. ~/Code/Example.lean => Example
std::string get_filename(const std::string& file) {
    // Windows paths also treat \ as a slash, while Linux paths only use /
    #if (defined(WIN32) || defined(_WIN32) || defined(__WIN32)) && !defined(__CYGWIN__)
    size_t slash_pos = file.find_last_of('/\\');
    #else
    size_t slash_pos = file.rfind('/');
    #endif
    size_t dot_pos = file.rfind('.');
    size_t name_start = (slash_pos == std::string::npos) ? 0 : slash_pos + 1;
    if (dot_pos == std::string::npos || dot_pos < name_start) {
        return file.substr(name_start);
    } else {
        return file.substr(name_start, dot_pos-name_start);
    }
}

// Gets JSON of a file
json get_json(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open JSON file: " + path);
    }
    json j;
    file >> j;
    return j;
};

// Throws an error if there are not enough arguments to handle a command line flag.
void ensure_args(int argp, int argc, char** argv, int count) {
    if (argp + count >= argc) {
        std::ostringstream err_msg;
        err_msg << "Option " << argv[argp] << " expected " << count
                << " arguments, received " << argc-argp-1;
        throw std::invalid_argument(err_msg.str());
    }
}

// Logger function for LeanTeX.
// Outputs a log message of the form:
// [LeanTeX] DEBUG: This is a log message.
void log(const std::string& message, LogLevel level, const std::string& component, 
         const Color& comp_color, std::ostream& out, LogLevel* new_log_level) {
    // We store the state of the current log level as a static variable,
    // so it persists without needing to be global.
    
    static LogLevel current_level = LogLevel::INFO;
    if (new_log_level != nullptr) {
        current_level = *new_log_level;
        return;
    }
    // Determine if the message should be logged
    if (static_cast<int>(level) < static_cast<int>(current_level)) {
        return;
    }
    out << (std::string)ANSIColor::BOLD << (std::string)comp_color << component
        << (std::string)ANSIColor::RESET << " " << (std::string)log_colors.at(level) 
        << log_names.at(level) << ": " << (std::string)ANSIColor::RESET << message << std::endl;
}

void set_log_level(LogLevel level) {
    // Note that the other arguments are ignored in the log function
    // when passing a new log level. They are only there to match the signature.
    log("", LogLevel::DEBUG, "", ANSIColor::RESET, std::cout, &level);
}

// Strips leading and trailing whitespace from a string
std::string strip(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    size_t end = str.find_last_not_of(" \t\n\r");
    if (start == std::string::npos || end == std::string::npos)
        return "";
    return str.substr(start, end - start + 1);
}
