// utility/misc.hpp - Utility functions
#pragma once
#include <string>
#include <iostream>
#include <unordered_map>
#include "utility/json.hpp"
using json = nlohmann::json;

// Returns just the name of a file
std::string get_filename(const std::string& file);

json get_json(const std::string& path);

// Throws an error if there are not enough arguments to handle a command line flag.
void ensure_args(int argp, int argc, char** argv, int count);

enum class LogLevel {
    DEBUG,      // Use for detailed debug information (--verbose)
    INFO,       // Use for general information (default)
    WARNING,    // Use for warnings (--quiet)
    ERROR       // Use for errors
};

struct Color {
    std::string code;
    operator std::string() const { return code; }
};

namespace ANSIColor {
    const Color RESET      = {"\033[0m"};
    const Color BOLD       = {"\033[1m"};
    const Color ITALIC     = {"\033[3m"};
    const Color BLACK      = {"\033[30m"};
    const Color RED        = {"\033[31m"};
    const Color GREEN      = {"\033[32m"};
    const Color YELLOW     = {"\033[33m"};
    const Color BLUE       = {"\033[34m"};
    const Color MAGENTA    = {"\033[35m"};
    const Color CYAN       = {"\033[36m"};
    const Color WHITE      = {"\033[37m"};
    const Color BR_BLACK   = {"\033[90m"};
    const Color BR_RED     = {"\033[91m"};
    const Color BR_GREEN   = {"\033[92m"};
    const Color BR_YELLOW  = {"\033[93m"};
    const Color BR_BLUE    = {"\033[94m"};
    const Color BR_MAGENTA = {"\033[95m"};
    const Color BR_CYAN    = {"\033[96m"};
    const Color BR_WHITE   = {"\033[97m"};
}

const std::unordered_map<LogLevel, Color> log_colors = {
    {LogLevel::DEBUG,   ANSIColor::GREEN},
    {LogLevel::INFO,    ANSIColor::BLUE},
    {LogLevel::WARNING, ANSIColor::YELLOW},
    {LogLevel::ERROR,   ANSIColor::RED}
};

const std::unordered_map<LogLevel, std::string> log_names = {
    {LogLevel::DEBUG,   "DEBUG"},
    {LogLevel::INFO,    "INFO"},
    {LogLevel::WARNING, "WARNING"},
    {LogLevel::ERROR,   "ERROR"}
};

// Logger function for LeanTeX.
void log(const std::string& message, LogLevel level = LogLevel::INFO,
         const std::string& component = "[LeanTeX]", const Color& comp_color = ANSIColor::CYAN,
         std::ostream& out = std::cout, LogLevel* new_log_level = nullptr);

void set_log_level(LogLevel level);

// Strips leading and trailing whitespace from a string
std::string strip(const std::string& str);

// Downcast to a unique pointer, destroying the base pointer if successful
template <typename Derived, typename Base>
std::unique_ptr<Derived> downcast_unique(std::unique_ptr<Base>& base_ptr) {
    if (!base_ptr) {
        return nullptr;
    }
    Derived* derived_ptr = dynamic_cast<Derived*>(base_ptr.get());
    if (!derived_ptr) {
        return nullptr;
    }
    base_ptr.release(); // Release ownership from base_ptr
    return std::unique_ptr<Derived>(derived_ptr); // Transfer ownership to Derived unique_ptr
}

// Downcast from an rvalue unique_ptr: transfer ownership if the dynamic_cast succeeds
template <typename Derived, typename Base>
std::unique_ptr<Derived> downcast_unique(std::unique_ptr<Base>&& base_ptr) {
    if (!base_ptr) {
        return nullptr;
    }
    Derived* derived_ptr = dynamic_cast<Derived*>(base_ptr.get());
    if (!derived_ptr) {
        return nullptr;
    }
    base_ptr.release(); // Release ownership from base_ptr
    return std::unique_ptr<Derived>(derived_ptr); // Transfer ownership to Derived unique_ptr
}

// Checks if a downcast is possible, without transferring ownership
template <typename Derived, typename Base>
bool is_a(std::unique_ptr<Base>& base_ptr) {
    if (!base_ptr) {
        return false;
    }
    Derived* derived_ptr = dynamic_cast<Derived*>(base_ptr.get());
    return derived_ptr != nullptr;
}

template <typename Derived, typename Base>
bool is_a(const std::unique_ptr<Base>& base_ptr) {
    if (!base_ptr) {
        return false;
    }
    Derived* derived_ptr = dynamic_cast<Derived*>(base_ptr.get());
    return derived_ptr != nullptr;
}

// converts text string to allowable LaTeX form; for now, turns _ into "\_" and \ into "\\"
std::string latexify(const std::string& str);

// Returns a human-readable name, even for internal variables
std::string get_var_name(std::string name);