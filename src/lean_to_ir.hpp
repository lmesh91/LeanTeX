// lean_to_ir.hpp - Conversion from Lean Elaboration to Lean IR
#pragma once
#include "ir/lean.hpp"
#include <utility>

// Utility functions
std::string kind_to_string(const json& kind);
std::string kind(const json& j);
const json& node(const json& j, int n);
json child(const json& j, int n);
json child(std::string type, const json& j, int n);
LBinder::Info binder_info_of(const std::string& info_str);

// Takes in an arbitrary number of keys and traverses through the JSON object with them
// This function itself is pretty messy but it makes the rest of the code much cleaner
// when multi-layer traversal is required.
template<typename... Keys>
const json& at(const json& j, Keys&&... keys) {
    // Collect keys as strings so we can both iterate and build an error message.
    auto to_str = [](auto&& k) -> std::string {
        using K = std::decay_t<decltype(k)>;
        if constexpr (std::is_integral_v<K>) {
            return std::to_string(k);
        } else {
            return std::string(std::forward<decltype(k)>(k));
        }
    };
    const std::vector<std::string> parts{ to_str(std::forward<Keys>(keys))... };
    const json* cur = &j; // We must use pointer traversal
    try {
        for (const auto& k : parts) {
            // If k is a decimal integer, treat it as an array index; otherwise as an object key.
            bool is_number = !k.empty() && std::all_of(k.begin(), k.end(),
                [](unsigned char c){ return std::isdigit(c); });
            if (is_number) {
                std::size_t idx = static_cast<std::size_t>(std::stoull(k));
                cur = &cur->at(idx);
            } else {
                cur = &cur->at(k);
            }
        }
    } catch (json::out_of_range& ex) {
        std::string path;
        for (size_t i = 0; i < parts.size(); ++i) {
            if (i) path += ".";
            path += parts[i];
        }
        throw std::runtime_error("JSON does not contain path '" + path + "': " + ex.what());
    }
    return *cur;
}

// The main function to convert Lean AST and elaboration JSON into Lean IR.
std::vector<std::unique_ptr<LExpr>> lean_to_ir(const json& elab);

// Parsers for individual syntax
std::unique_ptr<LTheorem> parse_theorem(const json& elab);
std::unique_ptr<LExpr> parse_expr(const json& expr);
std::unique_ptr<LLevel> parse_level(const json& level);