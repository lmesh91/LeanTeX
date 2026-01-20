// stage3.hpp - Conversion from LaTeX IR to a LaTeX string
#pragma once
#include "ir/lean.hpp"
#include "ir/latex.hpp"
#include <string>



/*
VarContext is a structure for storing the context necessary for translation:
the name, type, and on occasion value of statements.
*/
struct VarContext {
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
    ConvMode mode = ConvMode::Text;
    bool value = true; // Use the value instead of the type when possible. Used in ambiguous situations.
    int priority = 0; // Priority - used for inserting parentheses automatically
};

// Core translation function
std::string translate(std::string name, std::unordered_map<std::string, std::unique_ptr<LExpr>>& args, Context& context);

std::string to_latex(std::unique_ptr<LExpr> expr, Context& context);
std::string latex_conv(std::vector<std::unique_ptr<TExpr>>&& latex_ir);
void latex_post_process(std::string& tex);
void output_latex(std::string tex);