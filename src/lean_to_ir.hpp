// lean_to_ir.hpp - Conversion from Lean Elaboration to Lean IR
#pragma once
#include "ir/lean.hpp"
#include <utility>

// Utility functions
std::string kind_to_string(const json& kind);
const json& node(const json& j, int n);

// The main function to convert Lean AST and elaboration JSON into Lean IR.
std::vector<std::unique_ptr<LExpr>> lean_to_ir(const json& elab);

// Parsers for individual syntax
LTheorem parse_theorem(const json& elab);
std::string parse_decl_id(const json& elab);
//std::pair<std::vector<LBinder>,LExpr> parse_decl_sig(const json& elab);