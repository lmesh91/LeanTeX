// ir_to_latex.hpp - Conversion from Lean IR to LaTeX IR
#include "ir_to_latex.hpp"
#include "ir/utils.hpp"
#include <deque>
#include <forward_list>

#include "ir/utils.hpp"

// Converts a vector of Lean IR expressions to LaTeX IR expressions
// Note that the original object is invalidated, as some LExpr may be moved over
std::vector<std::unique_ptr<TExpr>> ir_conv(std::vector<std::unique_ptr<LExpr>>&& lean_ir) {
    std::vector<std::unique_ptr<TExpr>> out;
    for (auto& expr : lean_ir) {
        std::unique_ptr<LExpr> converted = _ir_conv(std::move(expr));
        if (converted) {
            log("Converted Lean IR expression to LaTeX IR expression: " + converted->to_string(), LogLevel::DEBUG);
            out.push_back(std::move(downcast_unique<TExpr>(converted)));
        }
    }
    return out;
}

// Helper function to convert a single Lean IR expression to LaTeX IR expression
// Some LExpr may remain LExprs, so they are unconverted
std::unique_ptr<LExpr> _ir_conv(std::unique_ptr<LExpr> expr) {
    // We go through each possible LExpr type and convert it to the corresponding TExpr type
    if (auto var = downcast_unique<LVar>(expr)) {
        log("Conversion of LVar expressions not supported", LogLevel::WARNING);
        return var;
    } else if (auto sort = downcast_unique<LSort>(expr)) {
        log("Conversion of LSort expressions not supported", LogLevel::WARNING);
        return sort;
    } else if (auto const_expr = downcast_unique<LConst>(expr)) {
        // if an LConst is not attached to a larger structure, it can be treated as a function application with no arguments
        return std::make_unique<TApply>(std::move(const_expr), std::vector<std::unique_ptr<LExpr>>{});
    } else if (auto app = downcast_unique<LApp>(expr)) {
        std::vector<std::unique_ptr<LExpr>> LArgs = std::move(app->args);
        std::vector<std::unique_ptr<LExpr>> TArgs;
        std::deque<std::unique_ptr<LExpr>> Apps;
        bool has_have = false;
        int count = 0;
        for (auto& arg : LArgs) {
            // Case 1 - arg is LVar
            if (is_a<LVar>(arg)) {
                TArgs.push_back(std::move(arg));
            }
            // Case 2 - arg is LLambda
            if (is_a<LLambda>(arg)) {
                TArgs.push_back(_ir_conv(std::move(arg)));
            }
            // Case 3 - arg is LApp
            if (is_a<LApp>(arg)) {
                has_have = true;
                Apps.push_front(std::move(arg));
                TArgs.push_back(std::make_unique<LVar>(LVar::Type::Free, "th" + std::to_string(count)));
                count++;
            }
        }
        std::unique_ptr<TExpr> parent = std::make_unique<TApply>(std::move(app->fn), std::move(TArgs));
        if (!has_have) {
            return parent;
        }
        for (auto& appl : Apps) {
            count--;
            std::unique_ptr<LExpr> type = infer_type(appl);
            std::unique_ptr<TExpr> converted = downcast_unique<TExpr>(_ir_conv(std::move(appl)));
            parent = std::make_unique<THave>("th" + std::to_string(count), std::move(type), std::move(converted), std::move(parent));
        }
        return parent;
    } else if (auto binder = downcast_unique<LBinder>(expr)) {
        log("Conversion of LBinder expressions not supported", LogLevel::WARNING);
        return binder;
    } else if (auto lambda = downcast_unique<LLambda>(expr)) {
        // Clone the lambda as an LExpr so we can pass a lvalue reference
        std::unique_ptr<LExpr> lambda_as_lexpr = lambda->clone();
        std::unique_ptr<LExpr> type = infer_type(lambda_as_lexpr);
        std::unique_ptr<LExpr> body = _ir_conv(std::move(lambda->body));
        std::unique_ptr<TExpr> intro = std::make_unique<TIntro>(std::move(lambda->binders), std::move(body));
        return std::make_unique<TGoal>(std::make_unique<LBinder>("LBinder", std::move(type), LBinder::Info::Explicit), std::move(intro));
    } else if (auto forall = downcast_unique<LForAll>(expr)) {
        log("Conversion of LForAll expressions not supported", LogLevel::WARNING);
        return forall;
    } else if (auto let = downcast_unique<LLet>(expr)) {
        if (is_a<LLambda>(let->value)) {
            std::unique_ptr<LLambda> lam = downcast_unique<LLambda>(std::move(let->value));
            std::unique_ptr<TExpr> body = downcast_unique<TExpr>(_ir_conv(std::move(let->body)));
            std::unique_ptr<TExpr> intro = std::make_unique<TIntro>(std::move(lam->binders), downcast_unique<TExpr>(_ir_conv(std::move(lam->body))));
            return std::make_unique<THave>("m", std::move(let->type), std::move(intro), std::move(body));
        }
        if (is_a<LApp>(let->value)) {
            return _ir_conv(std::move(let->value));
        }
        log("Conversion of this LLet expression not supported", LogLevel::WARNING);
        return let;
    } else if (auto literal = downcast_unique<LLiteral>(expr)) {
        return literal;
    } else if (auto proj = downcast_unique<LProj>(expr)) {
        log("Conversion of LProj expressions not supported", LogLevel::WARNING);
        return proj;
    } else if (auto term_proof = downcast_unique<LTermProof>(expr)) {
        return std::make_unique<TProof>(downcast_unique<TExpr>(_ir_conv(std::move(term_proof->expr))));
    } else if (auto theorem = downcast_unique<LTheorem>(expr)) {
        std::unique_ptr<TProof> proof = downcast_unique<TProof>(_ir_conv(std::move(theorem->proof)));
        return std::make_unique<TTheorem>(theorem->name, std::move(theorem->type), std::move(proof));
    } else {
        log("Unable to convert unknown expression "+expr->to_string()+" to LaTeX IR", LogLevel::WARNING);
        return expr;
    }
}