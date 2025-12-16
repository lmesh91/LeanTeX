// utility/misc.cpp - Utility functions
#include "utility/misc.hpp"
#include <sstream>
#include <iostream>

// Returns just the name of a file
// e.g. ~/Code/Example.lean => Example
std::string get_filename(const std::string& file) {
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
