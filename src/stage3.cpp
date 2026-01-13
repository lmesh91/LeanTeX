// stage3.hpp - Conversion from LaTeX IR to a LaTeX string
#include "stage3.hpp"
#include "loader.hpp"
#include "ir/lean.hpp"
#include "ir/latex.hpp"
#include "ir/utils.hpp"
#include <fstream>

/*
VarContext is a structure for storing the context necessary for translation:
the name, type, and on occasion value of statements.
*/
struct VarContext {
    std::string name;
    std::unique_ptr<LExpr> type;
    std::unique_ptr<LExpr> value;
};

/*
ConvMode is an enumeration for the current conversion mode from LaTeX IR to LaTeX string.
*/
enum class ConvMode {
    Text, // default mode, outputs normal text
    Math, // math mode, outputs LaTeX math expressions
    Apply, // alternate translation when inside TApply
    Intro, // alternate translation when inside TIntro
};

/*
Context is a structure for storing the overall context during LaTeX conversion.
*/
struct Context {
    std::vector<VarContext> vars;
    ConvMode mode = ConvMode::Text;
};

std::string to_latex(std::unique_ptr<LExpr> expr, Context& context) {
    // LExpr types
    if (auto const_expr = downcast_raw<LConst>(expr)) {
        // todo read from JSON
        if (const_expr->name == "True.intro") {
            return "True follows from the definition of true.";
        } else {
            return latexify(const_expr->name);
        }
    }
    // TExpr types
    else if (auto app = downcast_raw<TApply>(expr)) {
        // todo expand on this
        return to_latex(app->fn->clone(), context);
    }
    else if (auto proof = downcast_raw<TProof>(expr)) {
        auto expr = downcast_clone<LExpr>(proof->expr);
        return "\\begin{proof}\n" + to_latex(std::move(expr), context) + "\n\\end{proof}";
    }
    else if (auto theorem = downcast_raw<TTheorem>(expr)) {
        std::string name_ = latexify(theorem->name);
        std::string type_ = to_latex(theorem->type->clone(), context);
        std::string proof_ = to_latex(theorem->proof->clone(), context);
        std::string out = "\\begin{theorem}[" + name_ + "]\n" + type_ + "\n\\end{theorem}\n" + proof_;
        return out;
    } else {
        throw std::runtime_error("Conversion to LaTeX not supported for expression " + expr->to_string());
    }
};

/*
from TGoal
    std::string to_latex(Context& context) const override {
        VarContext new_context = VarContex
        context.vars.push_back()
        std::string type_ = param->type->to_latex();
        std::string body_ = body->to_latex();
        std::string out = "To show that " + type_ + ", " + body_;
        return out;
    };
*/

// Converts a vector of Lean IR expressions to LaTeX IR expressions
// Note that the original object is invalidated, as some LExpr may be moved over
std::string latex_conv(std::vector<std::unique_ptr<TExpr>>&& latex_ir) {
    std::string out = "\\documentclass{article}\n\\usepackage{amsthm}\n\\usepackage{amsmath}\n\\newtheorem{theorem}{Theorem}[section]\n\n\\begin{document}\n";
    Context ctx;
    for (auto& expr : latex_ir) {
        std::string theorem = to_latex(expr->clone(), ctx);
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
    std::string output_dir = LOADER.get_option("WorkingDir") + "/out";
    std::string tex_output = output_dir + "/" + code_name + ".tex";
    std::ofstream tex_file(tex_output);
    if (!tex_file.is_open()) {
        throw std::runtime_error("Failed to open LaTeX output file for writing: " + tex_output);
    }
    tex_file << tex;
    tex_file.close();
    log("Wrote LaTeX output to " + tex_output, LogLevel::INFO);
    // Compile the LaTeX file to PDF
    std::ostringstream command;
    command << "cd " << output_dir << "; " << LOADER.get_option("LaTeX") << " " << code_name << ".tex > /dev/null";
    log("Compiling LaTeX file with command: " + command.str(), LogLevel::INFO);
    std::system(command.str().c_str());
}