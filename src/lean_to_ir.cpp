// lean_to_ir.cpp - Conversion from Lean AST/Elaboration to Lean IR
#include "loader.hpp"
#include "lean_to_ir.hpp"
#include "utility/misc.hpp"

// Converts Names in Lean into strings, e.g. ["_uniq", 6] -> "_uniq.6"
std::string kind_to_string(const json& kind) {
    if (kind.is_string()) {
        return kind.get<std::string>();
    } else if (kind.is_array()) {
        std::string out;
        for (size_t i = 0; i < kind.size(); i++) {
            if (i > 0) out += ".";
            const json& k = kind.at(i);
            if (k.is_string()) {
                out += k.get<std::string>();
            } else if (k.is_number_integer()) {
                out += std::to_string(k.get<long long>());
            } else if (k.is_number_unsigned()) {
                out += std::to_string(k.get<unsigned long long>());
            } else if (k.is_number_float()) {
                out += std::to_string(k.get<double>());
            } else {
                // Fallback: dump the JSON (e.g. objects), should be rare.
                out += k.dump();
            }
        }
        return out;
    } else {
        log("kind_to_string received invalid kind JSON type", LogLevel::ERROR);
        log("JSON: " + kind.dump(), LogLevel::DEBUG);
        return "<unknown kind>";
    }
}

// Gets the kind from an elaboration term
std::string kind(const json& j) {
    try {
        return kind_to_string(j.at("ref").at("kind"));
    } catch (json::out_of_range& ex) {
        return "<no kind>";
    }
}

// Gets the child node directly from an AST
const json& node(const json& j, int n) {
    // The final key could be "node", "ident", or "atom" depending on what it is
    try {
        const json& node_container = j.at("args").at(n >= 0 ? n : j.at("args").size() + n);
        if (node_container.contains("node")) {
            return node_container.at("node");
        } else if (node_container.contains("ident")) {
            return node_container.at("ident");
        } else if (node_container.contains("atom")) {
            return node_container.at("atom");
        } else {
            throw std::runtime_error("AST node does not contain a valid child at index " + std::to_string(n));
        }
    } catch (json::out_of_range& ex) {
        throw std::runtime_error("AST node does not have child at index " + std::to_string(n) + " (" + ex.what() + ")");
    }
}

// Gets the child directly from an elaboration tree
json child(const json& j, int n) {
    try {
        return j.at("children").at(n >= 0 ? n : j.at("children").size() + n);
    } catch (json::out_of_range& ex) {
        throw std::runtime_error("Elaboration node "+kind(j)+" does not have child at index " + std::to_string(n) + " (" + ex.what() + ")");
    }
}

// This version filters for only info of a certain type, then gets the nth element of that type
json child(std::string type, const json& j, int n) {
    try {
        int i = 0;
        for (const auto& c : j.at("children")) {
            if (c.at("info").contains(type) || (c.at("info").contains("simple") && at(c, "info", "simple") == type)) {
                if (i == n) return c;
                i++;
            }
        }
    } catch (json::out_of_range& ex) {
        throw std::runtime_error("Elaboration node "+kind(j)+" does not have child of type " + type + " at index " + std::to_string(n) + " (" + ex.what() + ")");
    }
    throw std::runtime_error("Elaboration node "+kind(j)+" does not have child of type " + type + " at index " + std::to_string(n));
}

// Overload for zero keys: just return the original object.
inline const json& at(const json& j) {
    return j;
}

LBinder::Info binder_info_of(const std::string& info_str) {
    if (info_str == "default") {
        return LBinder::Info::Explicit;
    } else if (info_str == "implicit") {
        return LBinder::Info::Implicit;
    } else if (info_str == "strict_implicit") {
        return LBinder::Info::StrictImplicit;
    } else if (info_str == "inst_implicit") {
        return LBinder::Info::InstImplicit;
    } else {
        log("Unknown binder info '"+info_str+"', defaulting to explicit", LogLevel::WARNING);
        return LBinder::Info::Explicit;
    }
}

// Solves all free and bound variables in the expression using the provided context
// This function is a wrapper that initializes the parameters used in the recursive helper
void solve_variables(LExpr* expr, const json& ctx) {
    std::map<std::string, std::string> free_names;
    for (size_t i = 0; i < ctx.size(); i++) {
        free_names[kind_to_string(at(ctx, i, "id"))] = kind_to_string(at(ctx, i, "name"));
    }
    _solve_variables(expr, free_names, {});
}

// Main recursive function to solve variables:
// - free_names: mapping from free variable IDs to their names
// - bound_names: stack of bound variables in scope
void _solve_variables(LExpr* expr, const std::map<std::string, std::string>& free_names, std::vector<std::string> bound_names) {
    if (!expr) return;
    if (LVar* var = dynamic_cast<LVar*>(expr)) {
        if (var->type == LVar::Type::Free) {
            if (free_names.contains(var->name)) {
                var->solve(free_names.at(var->name));
            } else {
                log("Free variable '"+var->name+"' not found in context, cannot solve", LogLevel::WARNING);
            }
        } else if (var->type == LVar::Type::Bound) {
            // De Brujin indices count from the innermost binder outwards
            if (var->index < bound_names.size()) {
                var->solve(bound_names[bound_names.size() - 1 - var->index]);
            } else {
                log("Bound variable with index "+std::to_string(var->index)+" out of range (only "+std::to_string(bound_names.size())+" binders in scope), cannot solve", LogLevel::WARNING);
            }
        }
    } else if (LApp* app = dynamic_cast<LApp*>(expr)) {
        _solve_variables(app->fn.get(), free_names, bound_names);
        for (const auto& arg : app->args) {
            _solve_variables(arg.get(), free_names, bound_names);
        }
    } else if (LLambda* lam = dynamic_cast<LLambda*>(expr)) {
        for (const auto& binder : lam->binders) {
            _solve_variables(binder->type.get(), free_names, bound_names);
            bound_names.push_back(binder->name);
        }
        _solve_variables(lam->body.get(), free_names, bound_names);
        for (size_t i = 0; i < lam->binders.size(); i++) {
            bound_names.pop_back();
        }
    } else if (LForAll* fa = dynamic_cast<LForAll*>(expr)) {
        for (const auto& binder : fa->binders) {
            _solve_variables(binder->type.get(), free_names, bound_names);
            bound_names.push_back(binder->name);
        }
        _solve_variables(fa->body.get(), free_names, bound_names);
        for (size_t i = 0; i < fa->binders.size(); i++) {
            bound_names.pop_back();
        }
    } else if (LLet* let = dynamic_cast<LLet*>(expr)) {
        _solve_variables(let->type.get(), free_names, bound_names);
        _solve_variables(let->value.get(), free_names, bound_names);
        bound_names.push_back(let->name);
        _solve_variables(let->body.get(), free_names, bound_names);
        bound_names.pop_back();
    } else if (LProj* proj = dynamic_cast<LProj*>(expr)) {
        _solve_variables(proj->structE.get(), free_names, bound_names);
    }
    // The remaining expression types do not contain variables or bindings
}

std::vector<std::unique_ptr<LExpr>> lean_to_ir(const json& elab) {
    std::vector<std::unique_ptr<LExpr>> out;
    // Elaboration data is ordered by declarations.
    // The last one is skipped since it is always "end of input".
    for (size_t i = 0; i < elab.size() - 1; i++) {
        if (at(elab, i, "ref", "kind") != json::array({"Lean", "Parser", "Command", "declaration"})) {
            log("Skipping non-declaration "+kind_to_string(at(elab, i, "ref", "kind")),LogLevel::DEBUG);
            continue;
        }
        std::string decl = at(elab, i, "ref", "str");
        decl = decl.substr(0, decl.find_first_of(" "));
        // The first argument is declaration modifiers. This may be used
        // to parse custom attributes in the future, but for now it is ignored.
        if (decl == "theorem") {
            out.push_back(parse_theorem(elab.at(i)));
            log("Parsed "+out.back()->to_string(), LogLevel::DEBUG);
        } else {
            log("Declaration of type "+decl+" is not supported, skipping", LogLevel::WARNING);
            continue;
        }
    };
    return out;
}

std::unique_ptr<LTheorem> parse_theorem(const json& elab) {
    // The elaboration for theorem *appears* to be of the form:
    // [param1Type, param1, ..., proofType, param1, ..., proof, proofName1, proofName2]
    // where global variables only appear in the first parameter list.
    // proofName1 has a type that includes global variables; the other one doesn't.

    std::string name = at(child(elab, -1), "info", "term", "value");
    std::vector<std::unique_ptr<LBinder>> params = {};
    std::unique_ptr<LExpr> type = nullptr;
    std::unique_ptr<LProof> proof = nullptr;

    // First, parse the parameters and type
    
    // Start by figuring out how many parameters there are
    int n_params = 0;
    while (true) {
        // When we finish the parameter list, the elaborations will be [proofType, param1 OR proof]
        // If it is param1, then the reference will be before the last parameter's reference
        try {
            if (n_params > 0) {
                int pOld_ref = at(child(elab, 2*n_params-1), "ref", "range", 0);
                int pNew_ref = at(child(elab, 2*n_params+1), "ref", "range", 0);
                if (pOld_ref >= pNew_ref) {
                    break;
                }
            }
            // Otherwise, we will have reached the proof, which will not be a term
            if (!child(elab, 2*n_params+1).at("info").contains("term")) {
                break;
            }
            n_params++;
        } catch (std::exception& ex) {
            // This could happen for valid reasons (e.g. a missing term in elaboration),
            // and it also means that we are done parsing parameters.
            break;
        };
    };

    // Parse each parameter and the proof type
    for (int i = 0; i < n_params; i++) {
        std::unique_ptr<LExpr> param_type = parse_expr(child(elab, 2*i));
        solve_variables(param_type.get(), at(child(elab, 2*i), "info", "term", "context"));
        params.push_back(std::make_unique<LBinder>(
            at(child(elab, 2*i+1), "ref", "str"),
            std::move(param_type),
            binder_info_of(at(child(elab, 2*i+1), "info", "term", "context", 0, "binderInfo"))
        ));
    };
    type = parse_expr(child(elab, 2*n_params));
    solve_variables(type.get(), at(child(elab, 2*n_params), "info", "term", "context"));

    // The next step is to check if we are in term mode
    // Figure out which part of the elaboration contains the proof
    int i = -3;
    while (i + (int)elab.at("children").size() >= 0) {
        try {
            child(child(elab, i), 0);
            break;
        } catch (std::exception& ex) {
            i--;
        }
    }
    const json& proof_decl = child(child(elab, i), 0);
    if (proof_decl.at("info").contains("term")) {
        std::unique_ptr<LExpr> proof_term = parse_expr(proof_decl);
        solve_variables(proof_term.get(), at(child(child(elab, i),0), "info", "term", "context"));
        proof = std::make_unique<LTermProof>(std::move(proof_term));
    } else {
        log("Only term proofs are supported, skipping theorem " + name, LogLevel::WARNING);
    }

    return std::make_unique<LTheorem>(name, std::move(params), std::move(type), std::move(proof));
}

// Parses a Lean Expr object.
// todo: solve variables
std::unique_ptr<LExpr> parse_expr(const json& expr) {
    // Check if we are in the elaboration tree
    if (expr.contains("info") && expr.at("info").contains("term")) {
        // By default, we parse the value of the expression rather than the type
        return parse_expr(at(expr, "info", "term", "valueExpr"));
    }
    if (!expr.contains("expr")) {
        log("Expression does not contain 'expr' field, cannot parse", LogLevel::WARNING);
        log("Expression JSON:\n"+expr.dump(), LogLevel::DEBUG);
        return nullptr;
    }
    std::string type = expr.at("expr");
    // Note: metadata nodes are skipped in Jixia, so we do not have to handle it.
    if (type == "bvar") {
        return std::make_unique<LVar>(expr.at("deBrujinIndex"));
    } else if (type == "fvar") {
        return std::make_unique<LVar>(LVar::Type::Free, kind_to_string(expr.at("id")));
    } else if (type == "mvar") { // Note: There shouldn't be any metavariables after elaboration
        return std::make_unique<LVar>(LVar::Type::Meta, kind_to_string(expr.at("id")));
    } else if (type == "sort") {
        return std::make_unique<LSort>(parse_level(expr.at("level")));
    } else if (type == "const") {
        std::vector<std::unique_ptr<LLevel>> levels = {};
        for (const auto& lvl : expr.at("levels")) {
            levels.push_back(parse_level(lvl));
        }
        return std::make_unique<LConst>(kind_to_string(expr.at("name")), std::move(levels));
    } else if (type == "app") {
        std::unique_ptr<LExpr> fn = parse_expr(expr.at("fn"));
        std::unique_ptr<LExpr> arg = parse_expr(expr.at("arg"));
        return std::make_unique<LApp>(std::move(fn), std::move(arg));
    } else if (type == "lam") {
        std::unique_ptr<LBinder> binder = std::make_unique<LBinder>(
            kind_to_string(expr.at("name")),
            parse_expr(expr.at("binderType")),
            binder_info_of(expr.at("binderInfo"))
        );
        std::unique_ptr<LExpr> body = parse_expr(expr.at("body"));
        return std::make_unique<LLambda>(std::move(binder), std::move(body));
    } else if (type == "forallE") {
        std::unique_ptr<LBinder> binder = std::make_unique<LBinder>(
            kind_to_string(expr.at("name")),
            parse_expr(expr.at("binderType")),
            binder_info_of(expr.at("binderInfo"))
        );
        std::unique_ptr<LExpr> body = parse_expr(expr.at("body"));
        return std::make_unique<LForAll>(std::move(binder), std::move(body));
    } else if (type == "letE") {
        return std::unique_ptr<LLet>(new LLet(
            kind_to_string(expr.at("name")),
            parse_expr(expr.at("type")),
            parse_expr(expr.at("value")),
            parse_expr(expr.at("body")),
            expr.at("nondep")
        ));
    } else if (type == "lit") {
        if (expr.at("value").contains("natVal")) {
            return std::make_unique<LLiteral>((unsigned int)at(expr, "value", "natVal", "val"));
        } else if (expr.at("value").contains("strVal")) {
            return std::make_unique<LLiteral>((std::string)at(expr, "value", "strVal", "val"));
        } else {
            log("Unknown literal type in 'lit' expression, cannot parse", LogLevel::WARNING);
            log("Expression JSON:\n"+expr.dump(), LogLevel::DEBUG);
            return nullptr;
        }
    } else if (type == "proj") {
        return std::make_unique<LProj>(
            kind_to_string(expr.at("name")),
            expr.at("idx"),
            parse_expr(expr.at("struct"))
        );
    } else {
        log("Unknown expression type '"+type+"', cannot parse", LogLevel::WARNING);
        return nullptr;
    }
};

std::unique_ptr<LLevel> parse_level(const json& level) {
    if (level == "zero") {
        return std::make_unique<LLevel>();
    } else if (level.contains("succ")) {
        return std::make_unique<LLevel>(LLevel::Type::Succ, parse_level(level.at("succ")));
    } else if (level.contains("max")) {
        return std::make_unique<LLevel>(LLevel::Type::Max,
            parse_level(level.at("max").at(0)),
            parse_level(level.at("max").at(1))
        );
    } else if (level.contains("imax")) {
        return std::make_unique<LLevel>(LLevel::Type::IMax,
            parse_level(level.at("imax").at(0)),
            parse_level(level.at("imax").at(1))
        );
    } else if (level.contains("param")) {
        return std::make_unique<LLevel>(LLevel::Type::Param, kind_to_string(level.at("param")));
    } else if (level.contains("mvar")) {
        return std::make_unique<LLevel>(LLevel::Type::Meta, kind_to_string(at(level, "mvar", "name")));
    } else {
        log("Unknown level type in JSON, cannot parse", LogLevel::WARNING);
        log("Level JSON:\n"+level.dump(), LogLevel::DEBUG);
        return nullptr;
    }
}