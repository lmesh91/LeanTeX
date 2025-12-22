// lean_to_ir.cpp - Conversion from Lean AST/Elaboration to Lean IR
#include "loader.hpp"
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
            out.push_back(std::make_unique<LTheorem>(parse_theorem(elab.at(i))));
            log("Parsed "+out.back()->to_string(), LogLevel::DEBUG);
        } else {
            log("Declaration of type "+decl+" is not supported, skipping", LogLevel::WARNING);
            continue;
        }
    };
    return out;
}

LTheorem parse_theorem(const json& elab) {
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
    };

    // Parse each parameter and the proof type
    for (int i = 0; i < n_params; i++) {
        params.push_back(std::make_unique<LBinder>(
            at(child(elab, 2*i+1), "ref", "str"),
            parse_expr(child(elab, 2*i))
        ));
    };
    type = parse_expr(child(elab, 2*n_params));

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
        proof = std::make_unique<LTermProof>(parse_expr(proof_decl));
    } else {
        log("Only term proofs are supported, skipping theorem " + name, LogLevel::WARNING);
    }

    return LTheorem{name, std::move(params), std::move(type), std::move(proof)};
}

std::unique_ptr<LExpr> parse_expr(const json& elab) {
    return nullptr;
};