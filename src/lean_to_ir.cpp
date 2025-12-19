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
        const json& node_container = j.at("args").at(n);
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

std::vector<std::unique_ptr<LExpr>> lean_to_ir(const json& elab) {
    std::vector<std::unique_ptr<LExpr>> out;
    // Elaboration data is ordered by declarations.
    // The last one is skipped since it is always "end of input".
    for (size_t i = 0; i < elab.size() - 1; i++) {
        const json& decl = elab.at(i).at("ref").at("node");
        // The first argument is declaration modifiers. This may be used
        // to parse custom attributes in the future, but for now it is ignored.
        if (node(decl, 1).at("kind") == json::array({"Lean", "Parser", "Command", "theorem"})) {
            out.push_back(std::make_unique<LTheorem>(parse_theorem(elab.at(i))));
            log("Parsed theorem "+out.back()->to_string(), LogLevel::DEBUG);
        } else {
            log("Declaration of type "+kind_to_string(node(decl, 1).at("kind"))+" is not supported, skipping", LogLevel::WARNING);
            continue;
        }
    };
    return out;
}

LTheorem parse_theorem(const json& elab) {
    // The AST for theorem is of the form ["theorem", declId, declSig, declVal]
    // The elaboration for theorem is of the form []
    const json& ast = node(elab.at("ref").at("node"), 1); // We go through the declaration node
    std::string name = parse_decl_id(node(ast, 1));

    // This part is all placeholder
    std::vector<std::unique_ptr<LBinder>> params = {};
    std::unique_ptr<LExpr> type = nullptr;
    std::unique_ptr<LProof> proof = nullptr;
    LTheorem th;
    th.name = std::move(name);
    th.params = std::move(params);
    th.type = std::move(type);
    th.proof = std::move(proof);
    return th;
};

std::string parse_decl_id(const json& decl) {
    return node(decl, 0).at("rawVal");
};