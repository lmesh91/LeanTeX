// loader.cpp - Loads all dependencies/options for LeanTeX to operate
#include <sstream>
#include <filesystem>
#include <cstdlib>
#include <iostream>
#include "loader.hpp"
#include "utility.hpp"

// Loader constructor - creates all directories that the program uses
Loader::Loader(std::string path) : code_path(path) {
    std::filesystem::create_directories(".leantex/jixia");
};

// Run Jixia on all files in a project.
void Loader::run_jixia() {
    // todo: run Jixia on entire projects
    // todo: keep track of file modifications to prevent redundant work

    std::string code_name = get_filename(code_path);
    // Running Jixia gets information about term elaboration, the proof state after
    // each line, and the abstract syntax tree.
    std::ostringstream command;
    command << "lake env " << jixia_path
            << " -e .leantex/jixia/" << code_name << ".elab.json"
            << " -l .leantex/jixia/" << code_name << ".lines.json"
            << " -a .leantex/jixia/" << code_name << ".ast.json"
            << " -i " << code_path;
    std::cout << command.str() << std::endl;
    std::system(command.str().c_str());
}

// Note: This function is temporary, before INI file support is added.
void Loader::set_jixia_path(std::string path) {
    jixia_path = path;
}