// lean_to_ir.cpp - Conversion from Lean AST/Elaboration to Lean IR
#include "lean_to_ir.hpp"
#include "utility/misc.hpp"

std::string kind_to_string(const json& kind) {
    if (kind.is_string()) {
        return kind.get<std::string>();
    } else if (kind.is_array()) {
        std::string out;
        for (size_t i = 0; i < kind.size(); i++) {
            if (i > 0) out += ".";
            out += kind.at(i).get<std::string>();
        }
        return out;
    } else {
        return "<unknown kind>";
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
const json& child(const json& j, int n) {
    try {
        return j.at("children").at(n >= 0 ? n : j.at("children").size() + n);
    } catch (json::out_of_range& ex) {
        throw std::runtime_error("Elaboration node does not have child at index " + std::to_string(n) + " (" + ex.what() + ")");
    }
}

// Overload for zero keys: just return the original object.
inline const json& at(const json& j) {
    return j;
}


std::vector<std::unique_ptr<LExpr>> lean_to_ir(const json& elab) {
    std::vector<std::unique_ptr<LExpr>> out;
    // Elaboration data is ordered by declarations.
    // The last one is skipped since it is always "end of input".
    for (size_t i = 0; i < elab.size() - 1; i++) {
        const json& decl = at(elab, i, "ref", "node");
        if (decl.at("kind") != json::array({"Lean", "Parser", "Command", "declaration"})) {
            log("Skipping non-declaration "+kind_to_string(decl.at("kind")),LogLevel::DEBUG);
            continue;
        }
        // The first argument is declaration modifiers. This may be used
        // to parse custom attributes in the future, but for now it is ignored.
        if (node(decl, 1).at("kind") == json::array({"Lean", "Parser", "Command", "theorem"})) {
            out.push_back(std::make_unique<LTheorem>(parse_theorem(elab.at(i))));
            log("Parsed "+out.back()->to_string(), LogLevel::DEBUG);
        } else {
            log("Declaration of type "+kind_to_string(node(decl, 1).at("kind"))+" is not supported, skipping", LogLevel::WARNING);
            continue;
        }
    };
    return out;
}

LTheorem parse_theorem(const json& elab) {
    // The AST for theorem is of the form ["theorem", declId, declSig, declVal]
    // The elaboration for theorem *appears* to be of the form:
    // [param1Type, param1, ..., proofType, param1, ..., proof, proofName1, proofName2]
    // where global variables only appear in the first parameter list.
    // proofName1 has a type that includes global variables; the other one doesn't.

    std::string name = at(child(elab, -1), "info", "term", "value");
    std::unique_ptr<LExpr> type = parse_expr(at(child(elab, -2), "info", "term", "typeExpr"));

    // In some cases there are more terms between proof and proofName. We keep checking to find it.
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
    std::unique_ptr<LProof> proof = nullptr;
    if (proof_decl.at("info").contains("term")) {
        proof = std::make_unique<LTermProof>(parse_expr(at(proof_decl, "info", "term", "valueExpr")));
    } else {
        log("Only term proofs are supported, skipping theorem " + name, LogLevel::WARNING);
    }
    return LTheorem{name, std::move(type), std::move(proof)};
}

// This function assumes that the AST comes from the elaborator,
// rather than the direct AST of the code. This makes everything
// explicit and simplifies syntax.
// This is a heavy work in progress; most types of syntax are not supported.
std::unique_ptr<LExpr> parse_expr(const json& ast) {
    if (ast.contains("ident")) {
        // All constants and variables are "ident" nodes
        return std::make_unique<LIdent>(at(ast, "ident", "rawVal"));
    } else if (ast.contains("atom")) {
        // Atoms are just syntax components that don't have meaning here
        return nullptr;
    } else if (ast.contains("node")) {
        return parse_expr(ast.at("node"));
    } else {
        // Assume we are INSIDE a node
        if (!ast.contains("kind")) {
            // We must be inside an ident or atom
            if (ast.contains("rawVal")) {
                return std::make_unique<LIdent>(ast.at("rawVal"));
            } else {
                return nullptr;
            }
        }
        const json& expr_kind = ast.at("kind");
        // Constants that have their own Term types
        if (expr_kind == json::array({"Lean", "Parser", "Term", "prop"})) {
            return std::make_unique<LIdent>("Prop");
        } else if (expr_kind == json::array({"Lean", "Parser", "Term", "sorry"})) {
            return std::make_unique<LIdent>("sorry");
        // todo Lean.Parser.Term.type
        } else if (expr_kind == json::array({"Lean", "Parser", "Term", "forall"})) {
            // todo: Forall AST is of the form ["forall", binders, null, ",", body]
            // Treat everything between the "forall" and "," as potential binders
            std::vector<std::unique_ptr<LBinder>> params = {};
            const json& binder_decl = node(ast, 1);
            for (size_t i = 0; i < binder_decl.at("args").size(); i++) {
                params.push_back(parse_binder(node(binder_decl, i)));
            }
            std::unique_ptr<LExpr> body = parse_expr(node(ast, -1));
            return std::make_unique<LArrow>(std::move(params), std::move(body));
        } 
        log("Unsupported expression kind " + kind_to_string(expr_kind), LogLevel::WARNING);
        return nullptr;
    }
};

// Parses a binder object. Note that the type of the binder is irrelevant here.
std::unique_ptr<LBinder> parse_binder(const json& ast) {
    // Binder ASTs are of the form ["(", names, ..., type, ")"]
    std::vector<std::string> names = {};
    int offset = 2;
    if (ast.at("kind") == json::array({"Lean", "Parser", "Term", "explicitBinder"})) {
        // Explicit binders (ones with parentheses) have extra info
        offset = 3;
    }
    for (size_t i = 1; i < ast.at("args").size() - offset; i++) {
        names.push_back(node(node(ast, i), 0).at("rawVal"));
    }
    // We go two layers deep since the first layer is [":", type]
    std::unique_ptr<LExpr> type = parse_expr(node(node(ast, ast.at("args").size() - offset), 1));
    return std::make_unique<LBinder>(std::move(names), std::move(type));
};