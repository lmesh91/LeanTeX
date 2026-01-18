// stage3.hpp - Conversion from LaTeX IR to a LaTeX string
#include "stage3.hpp"
#include "loader.hpp"
#include "ir/lean.hpp"
#include "ir/latex.hpp"
#include "ir/utils.hpp"
#include <fstream>

using ExprMap = std::unordered_map<std::string, VarContext>;

// Utility functions for translation
json translation_data(std::string name, json& translation_json = LOADER.translation_data) {
    if (name.find('.') == std::string::npos) {
        if (!translation_json.contains(name)) {
            throw std::runtime_error("No translation data found for constant " + name);
        }
        return translation_json.at(name);
    } else {
        size_t dot_pos = name.find('.');
        if (!translation_json.contains(name.substr(0, dot_pos))) {
            throw std::runtime_error("No translation data found for constant " + name);
        } else {
            return translation_data(name.substr(dot_pos + 1), translation_json.at(name.substr(0, dot_pos)).at("children"));
        }
    }
}

std::string translation_mode(ConvMode mode) {
    switch (mode) {
        case ConvMode::Text:
            return "text";
        case ConvMode::Math:
            return "math";
        case ConvMode::Apply:
            return "apply";
        case ConvMode::Intro:
            return "intro";
        default:
            return "text";
    }
}

VarContext get_vc(std::unique_ptr<LExpr>& expr) {
    if (is_a<TExpr>(expr) || is_a<LSort>(infer_type(expr))) { // For base types, we want the value in place of the type
        return {.type = expr->clone(), .value = expr->clone()};
    } else {
        return {.type = infer_type(expr), .value = expr->clone()};
    }
}

std::string translate(std::string name, ExprMap& args, Context& context) {
    std::string translation_str;
    if (!translation_data(name).contains(translation_mode(context.mode))) {
        translation_str = translation_data(name).at("text");
        context.mode = ConvMode::Text;
    } else {
        translation_str = translation_data(name).at(translation_mode(context.mode));
    }
    // Find angle bracket pairs in the string
    size_t pos = 0;
    while (true) {
        size_t start = translation_str.find('<', pos);
        size_t start_var_name = start + 1;
        if (start == std::string::npos) {
            break;
        };
        size_t end = translation_str.find('>', start);
        size_t end_var_name = end - 1;
        if (end == std::string::npos) {
            throw std::runtime_error("Mismatched angle brackets in translation string for " + name);
        }
        bool value = false;
        // Check for '@' indicating value mode
        if (translation_str[start_var_name] == '@') {
            value = true;
            start_var_name++;
        }
        ConvMode prev_mode = context.mode;
        context.mode = ConvMode::Math;
        // Check for translation mode indicators
        if (translation_str[end_var_name-1] == ':') {
            char mode_char = translation_str[end_var_name];
            switch (mode_char) {
                case 't':
                    context.mode = ConvMode::Text;
                    break;
                case 'm':
                    context.mode = ConvMode::Math;
                    break;
                case 'a':
                    context.mode = ConvMode::Apply;
                    break;
                case 'i':
                    context.mode = ConvMode::Intro;
                    break;
                default:
                    throw std::runtime_error("Unknown translation mode '" + std::string(1, mode_char) + "' in translation string for " + name);
            }
            end_var_name -= 2; // Adjust to exclude mode indicator
        }
        std::string arg_name = translation_str.substr(start_var_name, end_var_name - start_var_name + 1);
        if (args.find(arg_name) == args.end()) {
            throw std::runtime_error("Argument " + arg_name + " not found for translation of " + name);
        }
        std::string arg_latex = to_latex(std::move(value ? args[arg_name].value : args[arg_name].type), context);
        if ((context.mode == ConvMode::Math) ^ (prev_mode == ConvMode::Math)) {
            arg_latex = "$" + arg_latex + "$";
        }
        context.mode = prev_mode; // Restore previous state
        translation_str.replace(start, end - start + 1, arg_latex);
        pos = start + arg_latex.length();
    }
    return translation_str;
}

std::string to_latex(std::unique_ptr<LExpr> expr, Context& context) {
    // LExpr types
    if (!expr) return "?nullptr";
    log("Converting expression to LaTeX: " + expr->to_string(), LogLevel::DEBUG);
    if (auto lit = downcast_raw<LLiteral>(expr)) {
        if (lit->type == LLiteral::Type::String) {
            context.mode = ConvMode::Text;
            return latexify(lit->str_value);
        } else if (lit->type == LLiteral::Type::Nat) {
            context.mode = ConvMode::Math;
            return std::to_string(lit->nat_value);
        } else {
            throw std::runtime_error("Unknown literal type for LaTeX conversion");
        }
    }
    else if (auto const_expr = downcast_raw<LConst>(expr)) {
        ExprMap args;
        return translate(const_expr->name, args, context);
    }
    else if (auto var = downcast_raw<LVar>(expr)) {
        // Always use math mode for variables
        if (context.mode != ConvMode::Math) {
            return "$" + latexify(var->name) + "$";
        }
        return latexify(var->name);
    }
    else if (auto fa = downcast_raw<LForAll>(expr)) {
        ExprMap args;
        for (size_t i = 0; i < fa->binders.size(); ++i) {
            args["a" + std::to_string(i+1)] = get_vc(fa->binders[i]->type);
        }
        auto body = fa->body->clone();
        args[".out"] = get_vc(body);
        return translate("_LeanTeX.ForAll"+std::to_string(fa->binders.size()), args, context);
    }
    else if (auto app = downcast_raw<LApp>(expr)) {
        if (app->args.size() == 0) {
            return to_latex(app->fn->clone(), context);
        }
        // The infer_type code for LApp may be helpful in debugging this later
        ExprMap args;
        auto f_type = downcast_unique<LForAll>(infer_type(app->fn));
        for (size_t i = 0; i < app->args.size(); ++i) {
            args[f_type->binders[i]->name] = get_vc(app->args[i]);
        }
        auto app_type = infer_type(expr);
        args[".out"] = get_vc(app_type);
        auto fn = app->fn->clone();
        while (!is_a<LConst>(fn)) {
            fn = infer_type(fn);
            log("Unwrapping LApp function to find LConst: " + fn->to_string(), LogLevel::DEBUG);
        }
        std::string fn_name = downcast_clone<LConst>(fn)->name;
        return translate(fn_name, args, context);
    }
    else if (auto binder = downcast_raw<LBinder>(expr)) {
        return to_latex(binder->type->clone(), context);
    }
    // TExpr types
    else if (auto app = downcast_raw<TApply>(expr)) {
        // Convert to an LApp
        LApp lapp(app->fn->clone(), nullptr);
        lapp.args.clear(); // Will start with one arg from constructor
        for (auto& arg : app->args) {
            lapp.args.push_back(arg->clone());
        }
        if (!app->is_exact()) {
            context.mode = ConvMode::Apply;
        }
        return to_latex(std::make_unique<LApp>(std::move(lapp)), context);
    }
    else if (auto goal = downcast_raw<TGoal>(expr)) {
        ExprMap args;
        auto type = goal->param->type->clone();
        args["type"] = get_vc(type);
        auto body = goal->body->clone();
        args["body"] = get_vc(body);
        return translate("_LeanTeX.Goal", args, context);
    }
    else if (auto have = downcast_raw<THave>(expr)) {
        ExprMap args;
        auto value = have->value->clone();
        args["value"] = get_vc(value);
        auto body = have->body->clone();
        args["body"] = get_vc(body);
        return translate("_LeanTeX.Have", args, context);
    }
    else if (auto intro = downcast_raw<TIntro>(expr)) {
        ExprMap args;
        for (size_t i = 0; i < intro->params.size(); ++i) {
            args["i" + std::to_string(i+1)] = get_vc(intro->params[i]->type);
        }
        auto body = intro->body->clone();
        args[".out"] = get_vc(body);
        return translate("_LeanTeX.Intro"+std::to_string(intro->params.size()), args, context);
    }
    else if (auto proof = downcast_raw<TProof>(expr)) {
        ExprMap args;
        auto expr = proof->expr->clone();
        args["expr"] = get_vc(expr);
        return translate("_LeanTeX.Proof", args, context);
    }
    else if (auto theorem = downcast_raw<TTheorem>(expr)) {
        ExprMap args;
        auto name = std::unique_ptr<LExpr>(new LLiteral(theorem->name));
        args["name"] = get_vc(name);
        args["type"] = get_vc(theorem->type);
        auto proof = theorem->proof->clone();
        args["proof"] = get_vc(proof);
        return translate("_LeanTeX.Theorem", args, context);
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