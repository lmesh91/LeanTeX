// stage2.hpp - Conversion from Lean IR to LaTeX IR
#pragma once
#include "ir/lean.hpp"
#include "ir/latex.hpp"
#include <forward_list>

std::vector<std::unique_ptr<TExpr>> ir_conv(std::vector<std::unique_ptr<LExpr>>&& lean_ir);
std::unique_ptr<LExpr> _ir_conv(std::unique_ptr<LExpr> lean_expr, int depth = 0);