// loader.cpp - Loads all dependencies/options for LeanTeX to operate
#include <sstream>
#include <filesystem>
#include <cstdlib>
#include <functional>
#include <iostream>
#include "loader.hpp"
#include "utility/misc.hpp"

// Used to handle code path. This function is unique in that
// argp points to the argument itself rather than the flag.
void handle_code_path(int& argp, int argc, char** argv, Loader& l) {
    (void)argc;
    l.set_option("CodePath", argv[argp]);
};

// Returns a function that sets the value of
// a single option
CLIParser handle_single(std::string arg) {
    return [=](int& argp, int argc, char** argv, Loader& l) {
        ensure_args(argp, argc, argv, 1);
        l.set_option(arg, argv[++argp]);
    };
};

void handle_help(int& argp, int argc, char** argv, Loader& l) {
    (void)argp, (void)argc, (void)argv;
    std::cout << "leantex: A tool to convert Lean 4 programs into LaTeX documents.\n" << std::endl
              << "Usage:"                                                             << std::endl
              << "  leantex <file> [options]\n"                                       << std::endl
              << "Options:"                                                           << std::endl
              << "  -j, --jixia        Specify a path to the Jixia binary."           << std::endl
              << "  -w, --working-dir  Specify the working directory."                << std::endl
              << "  -h, --help         Display this help message."                    << std::endl;
    l.set_option("_Exit", "True");
};

const std::unordered_map<std::string, CLIParser> Loader::CLI_OPTIONS = {
    {"-j", handle_single("Jixia")},
    {"--jixia", handle_single("Jixia")},
    {"-w", handle_single("WorkingDir")},
    {"--working-dir", handle_single("WorkingDir")},
    {"-h", handle_help},
    {"--help", handle_help},
};

void Loader::parse_argument(int& argp, int argc, char** argv) {
    try {
        if (argv[argp][0] != '-') { // Default option is code path
            handle_code_path(argp, argc, argv, *this);
        } else {
            CLI_OPTIONS.at(argv[argp])(argp, argc, argv, *this);
        };
    } catch (std::out_of_range& ex) {
        throw std::runtime_error("Error: Unknown command line option " + std::string(argv[argp]));
    }
}

// Loader constructor
Loader::Loader() {
    // Initialize all options with default values
    options["WorkingDir"] = ".leantex";
};

// Does general initialization that should happen *after* arguments are parsed
// Returns false if the program should quit
bool Loader::initialize() {
    if (options.contains("_Exit")) {
        return false;
    }
    // Create the working directories
    std::filesystem::create_directories(get_option("WorkingDir")+"/jixia");
    return true;
}

std::string Loader::get_option(std::string option) {
    try {
        return options.at(option);
    } catch (std::out_of_range& ex) {
        throw std::runtime_error("Error: No value specified for option "+option);
    }
};

void Loader::set_option(std::string option, std::string value) noexcept {
    options[option] = value;
};

// Run Jixia on all files in a project.
void Loader::run_jixia() {
    // todo: run Jixia on entire projects
    // todo: keep track of file modifications to prevent redundant work

    std::string code_name = get_filename(get_option("CodePath"));
    // Running Jixia gets information about term elaboration, the proof state after
    // each line, and the abstract syntax tree.
    std::ostringstream command;
    command << "lake env " << get_option("Jixia")
            << " -e " + get_option("WorkingDir") + "/jixia/" << code_name << ".elab.json"
            << " -l " + get_option("WorkingDir") + "/jixia/" << code_name << ".lines.json"
            << " -a " + get_option("WorkingDir") + "/jixia/" << code_name << ".ast.json"
            << " -i " << get_option("CodePath");
    std::system(command.str().c_str());
};