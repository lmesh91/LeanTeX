// stage3.hpp - Conversion from LaTeX IR to a LaTeX string
#include "stage3.hpp"
#include "loader.hpp"
#include <fstream>

// Converts a vector of Lean IR expressions to LaTeX IR expressions
// Note that the original object is invalidated, as some LExpr may be moved over
std::string latex_conv(std::vector<std::unique_ptr<TExpr>>&& latex_ir) {
    std::string out = "\\documentclass{article}\n\\usepackage{amsthm}\n\\usepackage{amsmath}\n\\newtheorem{theorem}{Theorem}[section]\n\n\\begin{document}\n";
    for (auto& expr : latex_ir) {
        std::string theorem = expr->to_latex({});
        if (!(theorem.empty())) {
            log("Converted Latex IR expression to LaTeX:\n" + theorem, LogLevel::DEBUG);
            out += "\n\n" + theorem;
        }
        else {
            log("Unable to convert Latex IR expression!", LogLevel::WARNING);
        }
    }
    out += "\n\\end{document}";
    return out;
};

// Outputs the LaTeX string to a .tex file and compiles it
// We assume that pdftex is used on Linux, todo for other OSes
void output_latex(std::string tex) {
    std::string code_name = get_filename(LOADER.get_option("CodePath"));
    // todo custom output path for PDF
    std::string tex_output = LOADER.get_option("WorkingDir") + "/out/" + code_name + ".tex";
    std::ofstream tex_file(tex_output);
    if (!tex_file.is_open()) {
        throw std::runtime_error("Failed to open LaTeX output file for writing: " + tex_output);
    }
    tex_file << tex;
    tex_file.close();
    log("Wrote LaTeX output to " + tex_output, LogLevel::INFO);
    // Compile the LaTeX file to PDF
    std::ostringstream command;
    command << LOADER.get_option("LaTeX") << " " << tex_output;
    log("Compiling LaTeX file with command: " + command.str(), LogLevel::INFO);
    std::system(command.str().c_str());
}