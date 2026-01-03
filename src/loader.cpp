// loader.cpp - Loads all dependencies/options for LeanTeX to operate
#include <sstream>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <functional>
#include "loader.hpp"
#include "utility/misc.hpp"
#include "lean_to_ir.hpp"
#include "ir_to_latex.hpp"
#include "ir/latex.hpp"

Loader LOADER;

// Used to handle code path. This function is unique in that
// argp points to the argument itself rather than the flag.
void handle_code_path(int& argp, int argc, char** argv, Loader& l) {
    (void)argc;
    if (l.is_tampered("CodePath")) {
        log("Option CodePath already set, using first value: " + l.get_option("CodePath"), LogLevel::WARNING);
        return;
    }
    log("Setting option CodePath to " + std::string(argv[argp]), LogLevel::DEBUG);
    l.set_option("CodePath", argv[argp]);
}

// Returns a function that sets the value of a single option
CLIParser handle_single(const std::string& arg) {
    return [arg](int& argp, int argc, char** argv, Loader& l) {
        if (l.is_tampered(arg)) {
            log("Option " + arg + " already set, using first value: " + l.get_option(arg), LogLevel::WARNING);
            return;
        }
        ensure_args(argp, argc, argv, 1);
        log("Setting option " + arg + " to " + std::string(argv[argp+1]), LogLevel::DEBUG);
        l.set_option(arg, argv[++argp]);
    };
}

// Returns a function that sets the log level
CLIParser handle_log(LogLevel level) {
    return [level](int& argp, int argc, char** argv, Loader& l) {
        (void)argp; (void)argc; (void)argv; (void)l;
        log("Setting log level to " + log_names.at(level), LogLevel::DEBUG);
        set_log_level(level);
    };
}

void handle_help(int& argp, int argc, char** argv, Loader& l) {
    (void)argp; (void)argc; (void)argv;
    log("Printing help message", LogLevel::DEBUG);
    std::cout << "leantex: A tool to convert Lean 4 programs into LaTeX documents.\n" << std::endl
              << "Usage:"                                                             << std::endl
              << "  leantex <file> [options]\n"                                       << std::endl
              << "Options:"                                                           << std::endl
              << "  -j, --jixia        Specify a path to the Jixia binary."           << std::endl
              << "  -w, --working-dir  Specify the working directory."                << std::endl
              << "  -h, --help         Display this help message."                    << std::endl
              << "  -i, --ini          Specify an INI configuration file."            << std::endl
              << "  -v, --verbose      Set verbose logging."                          << std::endl
              << "  -q, --quiet        Set quiet logging."                            << std::endl;
    l.set_option("_Exit", "True");
}

const std::unordered_map<std::string, CLIParser> Loader::CLI_OPTIONS = {
    {"-j", handle_single("Jixia")},
    {"--jixia", handle_single("Jixia")},
    {"-w", handle_single("WorkingDir")},
    {"--working-dir", handle_single("WorkingDir")},
    {"-i", handle_single("_IniFile")},
    {"--ini", handle_single("_IniFile")},
    {"-h", handle_help},
    {"--help", handle_help},
    {"-v", handle_log(LogLevel::DEBUG)},
    {"--verbose", handle_log(LogLevel::DEBUG)},
    {"-q", handle_log(LogLevel::WARNING)},
    {"--quiet", handle_log(LogLevel::WARNING)},
};

void Loader::parse_argument(int& argp, int argc, char** argv) {
    if (argp >= argc || std::string(argv[argp]).empty()) {
        throw std::runtime_error("Empty command line argument at position " + std::to_string(argp));
    }
    log("Parsing argument " + std::string(argv[argp]), LogLevel::DEBUG);
    try {
        if (argv[argp][0] != '-') { // Default option is code path
            handle_code_path(argp, argc, argv, *this);
        } else {
            CLI_OPTIONS.at(argv[argp])(argp, argc, argv, *this);
        };
    } catch (std::out_of_range& ex) {
        throw std::runtime_error("Unknown command line option " + std::string(argv[argp]) + " (" + ex.what() + ")");
    } catch (std::invalid_argument& ex) {
        throw std::runtime_error("Failed to parse argument " + std::string(argv[argp]) + " (" + ex.what() + ")");
    }
}

// Loader constructor
Loader::Loader() {
    // Initialize all options with default values
    options["WorkingDir"] = ".leantex";
    options["_IniFile"] = "leantex.ini";
}

void Loader::load_ini() {
    log("Loading INI file " + options["_IniFile"], LogLevel::DEBUG);
    std::ifstream ini_file(options["_IniFile"]);
    if (!ini_file.is_open()) {
        log("No INI file found at " + options["_IniFile"], tampered["_IniFile"] ? LogLevel::WARNING : LogLevel::DEBUG);
        return;
    }
    std::string line;
    std::getline(ini_file, line);
    if (strip(line) != "[LeanTeX]") {
        log("Ignoring INI file with invalid header: " + line, LogLevel::WARNING);
        return;
    }
    while (std::getline(ini_file, line)) {
        // Ignore comments and empty lines
        std::string stripped = strip(line);
        if (stripped.empty() || stripped[0] == '#' || stripped[0] == ';') continue;
        size_t eq_pos = line.find('=');
        if (eq_pos == std::string::npos) {
            log("Ignoring malformed INI line: " + line, LogLevel::WARNING);
            continue;
        }
        std::string key = strip(line.substr(0, eq_pos));
        std::string value = strip(line.substr(eq_pos + 1));
        // Only set the option if it wasn't tampered with via command line
        if (key[0] == '_') {
            log("Cannot set special option " + key + " in an INI file", LogLevel::WARNING);
            continue;
        } else if (!tampered.contains(key)) { // Note that this checks if the key is not found, not if it is false
            log("Setting option " + key + " to " + value + " from INI file", LogLevel::DEBUG);
            set_option(key, value);
        } else {
            log("Skipping INI option " + key + " because it was set via command line", LogLevel::DEBUG);
            continue;
        }
    }
};

// Does general initialization that should happen *after* arguments are parsed
// Returns false if the program should quit
bool Loader::initialize() {
    log("Initializing Loader", LogLevel::DEBUG);
    load_ini();
    if (options.contains("_Exit")) {
        return false;
    }
    // Check required options
    if (!options.contains("CodePath")) {
        throw std::runtime_error("No code path specified.");
    }
    if (!options.contains("Jixia")) {
        throw std::runtime_error("No Jixia path specified. Add one using -j or in an INI file.");
    }
    // Create the working directories
    std::filesystem::create_directories(get_option("WorkingDir")+"/jixia");
    std::filesystem::create_directories(get_option("WorkingDir")+"/temp");
    return true;
}

std::string Loader::get_option(const std::string& option) {
    try {
        return options.at(option);
    } catch (std::out_of_range& ex) {
        throw std::runtime_error("No value specified for option "+option+" ("+ex.what()+")");
    }
}

void Loader::set_option(const std::string& option, const std::string& value) noexcept {
    options[option] = value;
    tampered[option] = true;
}

bool Loader::is_tampered(const std::string& option) {
    return tampered.contains(option);
}

// Run Jixia on all files in a project.
void Loader::run_jixia() {
    log("Running Jixia on " + get_option("CodePath"));
    // todo: run Jixia on entire projects
    // todo: keep track of file modifications to prevent redundant work
    std::string code_name = get_filename(get_option("CodePath"));
    // Running Jixia gets information about term elaboration, the proof state after
    // each line, and the abstract syntax tree.
    // todo: verify that the paths are valid (prevent users from executing arbitrary commands)
    std::ostringstream command;
    command << "lake env " << get_option("Jixia")
            << " -e " << get_option("WorkingDir") << "/jixia/" << code_name << ".elab.json"
            << " -s " << get_option("WorkingDir") << "/jixia/" << code_name << ".sym.json"
            << " -i " << get_option("CodePath");
    log("Executing command: " + command.str(), LogLevel::DEBUG);
    std::system(command.str().c_str());
}

// Use Jixia to extract the AST of a Lean string.
json Loader::get_ast(const std::string& data) {
    // Write the data to a temporary file
    std::string temp_path = get_option("WorkingDir") + "/temp/Temp.lean";
    std::ofstream temp_file(temp_path);
    if (!temp_file.is_open()) {
        throw std::runtime_error("Failed to open temporary file for writing Lean code: " + temp_path);
    }
    temp_file << data;
    temp_file.close();
    // Run Jixia on the temporary file
    std::ostringstream command;
    command << "lake env " << get_option("Jixia")
            << " -a " << get_option("WorkingDir") << "/temp/Temp.ast.json"
            << " -i " << temp_path;
    // Suppress output, as there are often warnings we want to ignore
    #if (defined(WIN32) || defined(_WIN32) || defined(__WIN32)) && !defined(__CYGWIN__)
    command << " > NUL";
    #else
    command << " 1>/dev/null 2>/dev/null";
    #endif
    std::system(command.str().c_str());
    // Load and return the elaboration JSON
    return get_json(get_option("WorkingDir") + "/temp/Temp.ast.json").at(0).at("node");
}

// Runs all conversions steps to get from Lean to LaTeX
void Loader::convert() {
    log("Converting Lean code to Lean IR");
    std::string code_name = get_filename(get_option("CodePath"));
    // Load elaboration JSON files
    json elab_json = get_json(get_option("WorkingDir") + "/jixia/" + code_name + ".elab.json");
    json sym_json = get_json(get_option("WorkingDir") + "/jixia/" + code_name + ".sym.json");
    log("Loaded elaboration JSON files", LogLevel::DEBUG);
    // Convert to Lean IR
    std::vector<std::unique_ptr<LExpr>> lean_ir = lean_to_ir(elab_json, sym_json);
    log("Converted Lean code to Lean IR with " + std::to_string(lean_ir.size()) + " top-level expressions", LogLevel::DEBUG);
    // Convert to LaTeX IR
    log("Converting Lean IR to LaTeX IR");
    std::vector<std::unique_ptr<TExpr>> latex_ir = ir_conv(std::move(lean_ir));
}