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
            log("No translation data found for constant " + name, LogLevel::WARNING);
            return json::object();
        }
        return translation_json.at(name);
    } else {
        size_t dot_pos = name.find('.');
        if (!translation_json.contains(name.substr(0, dot_pos))) {
            log("No translation data found for constant " + name, LogLevel::WARNING);
            return json::object();
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
    auto expr_type = infer_type(expr);
    if (!expr_type || is_a<LSort>(expr_type)) { // For base types, we want the value in place of the type
        return {.type = expr->clone(), .value = expr->clone()};
    } else {
        return {.type = std::move(expr_type), .value = expr->clone()};
    }
}

std::string translate(std::string name, ExprMap& args, Context& context) {
    std::string translation_str;
    // Fallback order: specific -> text. Some modes fall back to math first.
    if (!translation_data(name).contains(translation_mode(context.mode))) {
        if (context.mode == ConvMode::Intro && translation_data(name).contains("math")) {
            translation_str = translation_data(name).at("math");
            context.mode = ConvMode::Math;
        } else if (translation_data(name).contains("text")) {
            translation_str = translation_data(name).at("text");
            context.mode = ConvMode::Text;
        } else {
            log("No suitable translation found for " + name, LogLevel::WARNING);
            // Generic fallback
            std::string out;
            if (context.mode == ConvMode::Math) {
                out = "\\mathrm{" + latexify(name) + "}(";
            } else {
                out = latexify(name) + "(";
            }
            for (auto& [arg_name, vc] : args) {
                out += latexify(arg_name) + "=" + to_latex(std::move(vc.type ? vc.type : vc.value), context) + ", ";
            }
            if (args.size() > 0) {
                out = out.substr(0, out.length() - 2); // Remove trailing comma and space
            }
            out += ")";
            return out;
        }
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
        int prev_priority = context.priority;
        context.mode = ConvMode::Math;
        if (translation_data(name).contains("priority")) {
            context.priority = translation_data(name).at("priority");
        } else {
            context.priority = 0;
        }
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
        context.value = value;
        std::string arg_name = translation_str.substr(start_var_name, end_var_name - start_var_name + 1);
        std::string arg_latex;
        if (args.find(arg_name) == args.end()) {
            log("Argument " + arg_name + " not found for translation of " + name, LogLevel::WARNING);
            for (const auto& [key, vc] : args) {
                log("Available arg: " + key + " type: " + (vc.type ? vc.type->to_string() : "null") + " value: " + vc.value->to_string(), LogLevel::DEBUG);
            }
            arg_latex = "\\langle " + arg_name + "?\\rangle";
        } else {
            arg_latex = to_latex(std::move(value ? args[arg_name].value : args[arg_name].type), context);
        }
        if ((prev_priority != 0 || context.priority != 0) && context.priority >= prev_priority) {
            arg_latex = "(" + arg_latex + ")";
        }
        if ((context.mode == ConvMode::Math) ^ (prev_mode == ConvMode::Math)) {
            arg_latex = "$" + arg_latex + "$";
        }
        context.mode = prev_mode; // Restore previous state
        context.priority = prev_priority;
        translation_str.replace(start, end - start + 1, arg_latex);
        pos = start + arg_latex.length();
    }
    return translation_str;
}

std::string to_latex(std::unique_ptr<LExpr> expr, Context& context) {
    // LExpr types
    if (!expr) return "?nullptr";
    if (auto lit = downcast_raw<LLiteral>(expr)) {
        if (lit->type == LLiteral::Type::String) {
            context.mode = ConvMode::Text;
            if (lit->str_value.front() == '$') {
                return lit->str_value; // Raw LaTeX string from mid-conversion
            }
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
        //log("Parsing Var: " + var->to_string(), LogLevel::DEBUG);
        context.priority = 0;
        // Always use math mode for named variables
        if (context.value && var->solved) {
            if (!is_a<LSort>(var->type)) {
                return to_latex(var->type->clone(), context);
            }
        }
        if (context.mode != ConvMode::Math) {
            return "$" + latexify(var->name) + "$";
        }
        return latexify(var->name);
    }
    else if (auto fa = downcast_raw<LForAll>(expr)) {
        ExprMap args;
        log("Parsing For All: " + fa->to_string(), LogLevel::DEBUG);
        for (size_t i = 0; i < fa->binders.size(); ++i) {
            args["a" + std::to_string(i+1)] = get_vc(fa->binders[i]->type);
        }
        auto body = fa->body->clone();
        args[".out"] = get_vc(body);
        return translate("_LeanTeX.ForAll"+std::to_string(fa->binders.size()), args, context);
    }
    else if (auto app = downcast_raw<LApp>(expr)) {
        log("Parsing LApp: " + app->to_string(), LogLevel::DEBUG);
        if (app->args.size() == 0) {
            return to_latex(app->fn->clone(), context);
        }
        // The infer_type code for LApp may be helpful in debugging this later
        ExprMap args;
        auto f_type = downcast_unique<LForAll>(infer_type(app->fn));
        if (f_type) {
            for (size_t i = 0; (i < app->args.size() && i < f_type->binders.size()); ++i) {
                args[f_type->binders[i]->name] = get_vc(app->args[i]);
            }
        }
        auto app_type = infer_type(expr);
        if (app_type) {
            args[".out"] = get_vc(app_type);
        }
        auto fn = app->fn->clone();
        while (!is_a<LConst>(fn)) {
            if (is_a<LSort>(fn)) {
                return translate(fn->to_string(), args, context);
            }
            if (is_a<LForAll>(fn)) {
                auto fa = downcast_raw<LForAll>(fn);
                auto fa_ref = fa->clone();
                args["fa"] = get_vc(fa_ref);
                for (size_t i = 0; i < fa->binders.size(); ++i) {
                    args["a" + std::to_string(i+1)] = get_vc(fa->binders[i]->type);
                }
                auto body = fa->body->clone();
                args[".out"] = get_vc(body);
                return translate("_LeanTeX.AppForAll"+std::to_string(fa->binders.size()), args, context);
            }
            log("Unwrapping LApp function to find LConst: " + fn->to_string(), LogLevel::DEBUG);
            fn = infer_type(fn);
            log("Now is: " + fn->to_string(), LogLevel::DEBUG);
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
            return to_latex(std::make_unique<LApp>(std::move(lapp)), context);
        } else {
            std::string raw_tex = to_latex(std::make_unique<LApp>(std::move(lapp)), context);
            if (context.mode == ConvMode::Math) {
                // This is in math mode, make it into a sentence
                context.mode = ConvMode::Text; // This prevents $...$ from being added again
                ExprMap args;
                auto tex_expr = std::unique_ptr<LExpr>(new LLiteral("$" + raw_tex + "$"));
                args["body"] = get_vc(tex_expr);
                return translate("_LeanTeX.Exact", args, context);
            }
            return raw_tex;
        }
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
        auto type = have->type->clone();
        args["type"] = get_vc(type);
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
    else if (auto theorem = downcast_raw<TTheorem>(expr)) {
        ExprMap args;
        auto name = std::unique_ptr<LExpr>(new LLiteral(theorem->name));
        args["name"] = get_vc(name);
        args["type"] = get_vc(theorem->type);
        auto expr = theorem->proof->expr->clone();
        args["expr"] = get_vc(expr);
        return translate("_LeanTeX.Theorem", args, context);
    } else {
        log("Conversion to LaTeX not supported for expression " + expr->to_string(), LogLevel::WARNING);
        return latexify(expr->to_string());
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
    latex_post_process(out);
    return out;
};

// Does final post-processing steps for the LaTeX string
void latex_post_process(std::string& tex) {
    // The \_period marker indicates the end of a sentence.
    size_t pos = 0;
    while ((pos = tex.find("\\_period", pos)) != std::string::npos) {
        tex.replace(pos, 8, ".");
        pos++;
        // Start the next sentence with a capital letter
        while (pos < tex.length() && std::isspace(tex[pos])) {
            pos++; // find the first non-whitespace character
        }
        if (pos < tex.length()) {
            tex[pos] = std::toupper(tex[pos]);
        }
    }
    // The \_start marker indicates the start of a paragraph.
    pos = 0;
    while ((pos = tex.find("\\_start", pos)) != std::string::npos) {
        tex.replace(pos, 7, "");
        // Start the first sentence in a paragraph with a capital letter
        if (pos < tex.length()) {
            tex[pos] = std::toupper(tex[pos]);
        }
    }
}

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
    #if (defined(WIN32) || defined(_WIN32) || defined(__WIN32)) && !defined(__CYGWIN__)
    // Windows Compilation
    std::ostringstream command;
    command << "cd " << output_dir << " && " << LOADER.get_option("LaTeX") << " " << code_name << ".tex";
    log("Compiling LaTeX file with command: " + command.str(), LogLevel::INFO);
    std::system(command.str().c_str());
    #else
    // Linux Compilation
    std::ostringstream command;
    command << "cd " << output_dir << "; " << LOADER.get_option("LaTeX") << " " << code_name << ".tex > /dev/null";
    log("Compiling LaTeX file with command: " + command.str(), LogLevel::INFO);
    std::system(command.str().c_str());
    #endif
}