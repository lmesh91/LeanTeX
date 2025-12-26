#include "ir_to_latex.hpp"
#include "utility/misc.hpp"
#include <fstream>
#include <sstream>
void ir_to_latex() {
    std::ofstream latexOut("LeanTex.tex");
    if (latexOut) {
        latexOut << "\\documentclass{article}\n\\usepackage{amsthm}\n\\usepackage{amsmath}\n" <<
                    "\\newtheorem{theorem}{Theorem}\n[section]\n\n\\begin{document}" << std::endl;
        latexOut << load_ir() << std::endl;
        latexOut << "\\end{proof}\n\\end{document}";
        log("Wrote to LeanText.tex", LogLevel::INFO);
    }
    else {
        log("Unable to open LeanTex.tex", LogLevel::ERROR);
    }
}
std::string load_ir() { // todo - handle multi-theorem parsing
    std::ifstream latexIn("ir.txt");
    std::string output;
    if (latexIn) {
        while (latexIn) {
            std::string command;
            latexIn >> command;
            if (command == "theorem") {
                std::string t_name;
                latexIn >> t_name;
                output += "\\begin{theorem}[" + latexify(t_name) + "]\n";
            }
            else if (command == "type") {
                std::string statement;
                std::getline(latexIn >> std::ws, statement);
                output += statement + "\n\\end{theorem}\n\\begin{proof}\n";
            }
            else {
                output += "not parsing";
            }
        }
    }
    else {
        log("Unable to open ir.txt", LogLevel::ERROR);
        return "unable to open theorem";
    }
    return output;
}
std::string match_command(const std::string& command) {
    std::string name;
    std::istringstream commandStream(command);
    commandStream >> name;
    if (name == "True.intro") {
        return "is valid by the definition of True.";
    }
    if (name == "False.elim") {
        std::string false_var_name;
        std::getline(commandStream >> std::ws, false_var_name);
        if (false_var_name == "False") {
            return "is valid from falsehood.";
        }
        return "is valid since the statement " + false_var_name + " is False.";
    }
    return "Command " + command + " does not have a current implementation";
}