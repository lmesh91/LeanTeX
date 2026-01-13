// stage3.hpp - Conversion from LaTeX IR to a LaTeX string
#pragma once
#include "ir/lean.hpp"
#include "ir/latex.hpp"
#include <string>

std::string latex_conv(std::vector<std::unique_ptr<TExpr>>&& latex_ir);
void output_latex(std::string tex);