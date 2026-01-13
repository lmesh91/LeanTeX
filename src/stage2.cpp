// stage2.cpp - Conversion from Lean IR to LaTeX IR
#include "stage2.hpp"
#include "ir/utils.hpp"
#include <deque>
#include <forward_list>

// Converts a vector of Lean IR expressions to LaTeX IR expressions
// Note that the original object is invalidated, as some LExpr may be moved over
std::vector<std::unique_ptr<TExpr>> ir_conv(std::vector<std::unique_ptr<LExpr>>&& lean_ir) {
    std::vector<std::unique_ptr<TExpr>> out;
    for (auto& expr : lean_ir) {
        std::unique_ptr<TExpr> converted = downcast_unique<TExpr>(_ir_conv(std::move(expr)));
        if (converted) {
            log("Converted Lean IR expression to LaTeX IR expression:\n" + converted->to_tactic(), LogLevel::DEBUG);
            out.push_back(std::move(converted));
        }
        else {
            log("Unable to convert Lean IR expression!", LogLevel::WARNING);
        }
    }
    return out;
}

// Helper function to convert a single Lean IR expression to LaTeX IR expression
// Some LExpr may remain LExprs, so they are unconverted
std::unique_ptr<LExpr> _ir_conv(std::unique_ptr<LExpr> expr, int depth) {
    // We go through each possible LExpr type and convert it to the corresponding TExpr type
    if (auto var = downcast_unique<LVar>(expr)) {
        std::vector<std::unique_ptr<LExpr>> LArgs;
        return std::make_unique<TApply>(std::move(var), std::move(LArgs));
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
        bool reached_case = false;
        int count = 0;
        int tcount = 0;
        std::unique_ptr<LForAll> fn_type = downcast_unique<LForAll>(infer_type(app->fn));
        int argcount = LArgs.size();
        for (auto& arg : LArgs) {
            // Case 2 - arg is LLambda
            if (is_a<LLambda>(arg)) {
                reached_case = true;
                std::unique_ptr<LExpr> TArg = _ir_conv(arg->clone(), depth);
                if (is_a<TIntro>(TArg)) {
                    auto type = infer_type(arg);
                    // todo: determine goal names based on type of LApp; currently hardcoded for Iff
                    if (argcount == 1) {
                        TArgs.push_back(std::move(TArg));
                    }
                    else {
                        std::string goal_name = fn_type->binders[tcount]->name;
                        TArgs.push_back(std::make_unique<TGoal>(std::make_unique<LBinder>(goal_name, std::move(type), LBinder::Info::Explicit), downcast_unique<TExpr>(TArg)));
                    }
                } else {
                    log("Conversion returned null for lambda arg", LogLevel::WARNING);
                    TArgs.push_back(nullptr);
                }
            }
            // Case 3 - arg is LApp
            else if (is_a<LApp>(arg)) {
                std::unique_ptr<LExpr> type = infer_type(arg);
                if (is_a<LSort>(type)) {
                    TArgs.push_back(std::move(arg));
                }
                else {
                    has_have = true;
                    Apps.push_front(std::move(arg));
                    std::unique_ptr<LVar> var_ = std::make_unique<LVar>(LVar::Type::Free, "temp");
                    std::unique_ptr<LBinder> var_info = std::make_unique<LBinder>("h!" + std::to_string(depth) + "_" + std::to_string(count), std::move(type), LBinder::Info::Explicit);
                    var_->solve(std::move(var_info));
                    TArgs.push_back(downcast_unique<LExpr>(var_));
                    count++;
                }
            }
            // Case 1 - arg is LVar or other
            else {
                if (!reached_case) {
                    argcount--;
                    TArgs.push_back(std::move(arg));
                }
                else { // case solved exactly
                    auto type = infer_type(arg);
                    std::string goal_name = fn_type->binders[tcount]->name;
                    std::vector<std::unique_ptr<LExpr>> LArgs;
                    std::unique_ptr<TApply> fn = std::make_unique<TApply>(std::move(arg), std::move(LArgs));
                    TArgs.push_back(std::make_unique<TGoal>(std::make_unique<LBinder>(goal_name, std::move(type), LBinder::Info::Explicit), std::move(fn)));
                }
            }
            tcount++;
        }
        std::unique_ptr<TExpr> parent = std::make_unique<TApply>(std::move(app->fn), std::move(TArgs));
        if (!has_have) {
            return parent;
        }
        for (auto& appl : Apps) {
            count--;
            std::unique_ptr<LExpr> type = infer_type(appl);
            std::unique_ptr<TExpr> converted = downcast_unique<TExpr>(_ir_conv(std::move(appl), depth + 1));
            parent = std::make_unique<THave>("h!" + std::to_string(depth) + "_" + std::to_string(count), std::move(type), std::move(converted), std::move(parent));
        }
        return parent;
    } else if (auto binder = downcast_unique<LBinder>(expr)) {
        log("Conversion of LBinder expressions not supported", LogLevel::WARNING);
        return binder;
    } else if (auto lambda = downcast_unique<LLambda>(expr)) {
        std::unique_ptr<LExpr> body = _ir_conv(std::move(lambda->body), depth);
        return std::make_unique<TIntro>(std::move(lambda->binders), downcast_unique<TExpr>(body));
    } else if (auto forall = downcast_unique<LForAll>(expr)) {
        log("Conversion of LForAll expressions not supported", LogLevel::WARNING);
        return forall;
    } else if (auto let = downcast_unique<LLet>(expr)) {
        if (is_a<LLambda>(let->value)) {
            std::unique_ptr<LLambda> lam = downcast_unique<LLambda>(std::move(let->value));
            std::unique_ptr<TExpr> body = downcast_unique<TExpr>(_ir_conv(std::move(let->body), depth));
            std::unique_ptr<TExpr> intro = std::make_unique<TIntro>(std::move(lam->binders), downcast_unique<TExpr>(_ir_conv(std::move(lam->body), depth)));
            return std::make_unique<THave>(let->name, std::move(let->type), std::move(intro), std::move(body));
        }
        if (is_a<LApp>(let->value)) {
            std::unique_ptr<TExpr> body = downcast_unique<TExpr>(_ir_conv(std::move(let->body), depth));
            std::unique_ptr<TExpr> val = downcast_unique<TExpr>(_ir_conv(std::move(let->value), depth));
            return std::make_unique<THave>(let->name, std::move(let->type), std::move(val), std::move(body));
        }
        log("Conversion of this LLet expression not supported", LogLevel::WARNING);
        return let;
    } else if (auto literal = downcast_unique<LLiteral>(expr)) {
        return literal;
    } else if (auto proj = downcast_unique<LProj>(expr)) {
        log("Conversion of LProj expressions not supported", LogLevel::WARNING);
        return proj;
    } else if (auto term_proof = downcast_unique<LTermProof>(expr)) {
        return std::make_unique<TProof>(downcast_unique<TExpr>(_ir_conv(std::move(term_proof->expr), depth)));
    } else if (auto theorem = downcast_unique<LTheorem>(expr)) {
        std::unique_ptr<TProof> proof = downcast_unique<TProof>(_ir_conv(std::move(theorem->proof), depth));
        return std::make_unique<TTheorem>(theorem->name, std::move(theorem->params), std::move(theorem->type), std::move(proof));
    } else {
        log("Unable to convert unknown expression "+expr->to_string()+" to LaTeX IR", LogLevel::WARNING);
        return expr;
    }
}