// Utility functions for both types of IR, not directly tied to the conversion process
#pragma once
#include "utility/misc.hpp"
#include "ir/lean.hpp"
#include "ir/latex.hpp"
#include "loader.hpp"
#include <functional>

// Downcasts and clones an LExpr object
template <typename Derived>
std::unique_ptr<Derived> downcast_clone(std::unique_ptr<LExpr>& base_ptr) {
    return downcast_unique<Derived>(base_ptr->clone());
}
template <typename Derived>
std::unique_ptr<Derived> downcast_clone(std::unique_ptr<TExpr>& base_ptr) {
    return downcast_unique<Derived>(base_ptr->clone());
}

// Downcasts an LExpr object into a raw pointer
template <typename Derived>
Derived* downcast_raw(std::unique_ptr<LExpr>& base_ptr) {
    return dynamic_cast<Derived*>(base_ptr.get());
}
template <typename Derived>
Derived* downcast_raw(std::unique_ptr<TExpr>& base_ptr) {
    return dynamic_cast<Derived*>(base_ptr.get());
}

void recursive_apply_l(std::unique_ptr<LExpr>& expr, std::function<void(std::unique_ptr<LExpr>&)> fun);

// Specific helper function for LBinders
inline void recursive_apply_l(std::unique_ptr<LBinder>& expr, std::function<void(std::unique_ptr<LExpr>&)> fun) {
    if (!expr) return;
    auto lexpr = expr->clone();
    fun(lexpr);
    recursive_apply_l(expr->type, fun);
}

// Applies a function recursively on an LExpr and its children.
// Note that the pointers are passed by reference so they can modify their containers if they are overridden.
inline void recursive_apply_l(std::unique_ptr<LExpr>& expr, std::function<void(std::unique_ptr<LExpr>&)> fun) {
    if (!expr) return;
    fun(expr);
    if (auto binder = downcast_raw<LBinder>(expr)) {
        recursive_apply_l(binder->type, fun);
    } else if (auto var = downcast_raw<LVar>(expr)) {
        recursive_apply_l(var->type, fun);
    } else if (auto const_expr = downcast_raw<LConst>(expr)) {
        recursive_apply_l(const_expr->meta.expr, fun);
    } else if (auto app = downcast_raw<LApp>(expr)) {
        recursive_apply_l(app->fn, fun);
        for (auto& arg : app->args) {
            recursive_apply_l(arg, fun);
        }
    } else if (auto lam = downcast_raw<LLambda>(expr)) {
        for (auto& bin : lam->binders) {
            recursive_apply_l(bin, fun);
        }
        recursive_apply_l(lam->body, fun);
    } else if (auto fa = downcast_raw<LForAll>(expr)) {
        for (auto& bin : fa->binders) {
            recursive_apply_l(bin, fun);
        }
        recursive_apply_l(fa->body, fun);
    } else if (auto let = downcast_raw<LLet>(expr)) {
        recursive_apply_l(let->type, fun);
        recursive_apply_l(let->value, fun);
        recursive_apply_l(let->body, fun);
    } else if (auto proj = downcast_raw<LProj>(expr)) {
        recursive_apply_l(proj->structE, fun);
    }
}

// Substitutes types for given typenames
// This is a helper that recurses through an LLevel
inline void _substitute_types(std::unordered_map<std::string, LLevel*>& types, std::unique_ptr<LLevel>& lv) {
    if (lv->arg1) {
        if (lv->arg1->type == LLevel::Type::Param) {
            if (types.contains(lv->arg1->name)) {
                lv->arg1 = types[lv->arg1->name]->clone();
            }
        } else {
            _substitute_types(types, lv->arg1);
        }
    }
    if (lv->arg2) {
        if (lv->arg2->type == LLevel::Type::Param) {
            if (types.contains(lv->arg2->name)) {
                lv->arg2 = types[lv->arg2->name]->clone();
            }
        } else {
            _substitute_types(types, lv->arg2);
        }
    }
}

inline void substitute_types(std::unordered_map<std::string, LLevel*>& types, std::unique_ptr<LExpr>& expr) {
    recursive_apply_l(expr, [&types](std::unique_ptr<LExpr>& e) {
        if (auto sort = downcast_raw<LSort>(e)) {
            _substitute_types(types, sort->level);
        }
    });
}

inline void substitute_binder(std::string name, LExpr* value, std::unique_ptr<LExpr>& expr) {
    // Todo: Use De Bruijn indices to avoid variable capture issues

    static int i = 1; // For renaming bound variables, we use a unique number to avoid collisions
    auto check = [&name, &value, &expr](std::unique_ptr<LExpr>& e) {
        if (auto var = downcast_raw<LVar>(e)) {
            if (var->name == name) {
                e = value->clone();
            }
        }
        return false;
    };
    // Check for naming collisions in what is being substituted
    bool contains = false;
    auto contains_check = [&name, &contains](std::unique_ptr<LExpr>& e) {
        if (auto var = downcast_raw<LVar>(e)) {
            if (var->name == name) {
                contains = true;
            }
        }
    };
    auto value_uniq = std::unique_ptr<LExpr>(value);
    recursive_apply_l(value_uniq, contains_check);
    if (contains) {
        // We rename the bound variable to avoid potential infinite loops
        auto contains_fix = [&name](std::unique_ptr<LExpr>& e) {
            if (auto var = downcast_raw<LVar>(e)) {
                if (var->name == name) {
                    var->name += "." + std::to_string(i);
                }
            }
        };
        recursive_apply_l(value_uniq, contains_fix);
        i++;
    }

    recursive_apply_l(expr, check);
    value_uniq.release(); // Don't delete the original pointer
}

// Type inference
// First pass: does main type inference logic
inline std::unique_ptr<LExpr> infer_type(std::unique_ptr<LExpr>& expr) {
    if (!expr) {
        return nullptr;
    }
    if (auto var = downcast_clone<LVar>(expr)) {
        if (!var->type) {
            log("Cannot infer type of unsolved variable"+var->to_string(), LogLevel::WARNING);
            return nullptr;
        }
        return var->type->clone();
    } else if (auto sort = downcast_clone<LSort>(expr)) {
        // A sort has the type that is one sort higher
        return std::make_unique<LSort>(std::make_unique<LLevel>(LLevel::Type::Succ, sort->level->clone()));
    } else if (auto const_expr = downcast_clone<LConst>(expr)) {
        if (!const_expr->solved) {
            // Last ditch effort: try and solve from global constant mapping
            if (LOADER.const_sym.contains(const_expr->name)) {
                const_expr->solve(LOADER.const_sym[const_expr->name]);
            } else {
                log("Cannot infer type of unsolved constant "+const_expr->to_string(), LogLevel::WARNING);
                return nullptr;
            }
        }
        std::unique_ptr<LExpr> type = const_expr->meta.expr->clone();
        // Apply universe variables to the type
        std::unordered_map<std::string, LLevel*> level_types;
        for (size_t i = 0; i < const_expr->levels.size(); i++) {
            level_types[const_expr->meta.levels[i]] = const_expr->levels[i].get();
        }
        substitute_types(level_types, type);
        return type;
    } else if (auto lit = downcast_clone<LLiteral>(expr)) {
        if (lit->type == LLiteral::Type::Nat) {
            return std::make_unique<LConst>("Nat", std::vector<std::unique_ptr<LLevel>>{});
        } else {
            return std::make_unique<LConst>("String", std::vector<std::unique_ptr<LLevel>>{});
        }
    } else if (auto app = downcast_clone<LApp>(expr)) {
        // For each argument that is applied, we go one step further inside
        // the LForAll associated with this, and substitute any values of bound variables
        // todo: resolve universes of substituted types
        if (app->args.size() == 0) {
            return infer_type(app->fn); // LApp is a useless wrapper here
        }
        auto type_generic = infer_type(app->fn);
        while (auto app2 = downcast_raw<LApp>(type_generic)) {
            // Some functions return functions, so we need to unwrap those first
            type_generic = infer_type(app2->fn);
        }
        if (auto type = downcast_unique<LForAll>(type_generic)) {
            for (auto& arg : app->args) {
                if (type->binders.size() == 0) {
                    log("LApp has too many arguments to determine type", LogLevel::WARNING);
                    return nullptr;
                }
                std::string bound_name = type->binders[0]->name;
                type->binders.erase(type->binders.begin()); // Remove first element
                // I use a shallow copy to prevent memory issues, while still updating appropriate args
                std::unique_ptr<LExpr> upcasted_type = type->clone();
                substitute_binder(bound_name, arg.get(), upcasted_type);
                type = downcast_unique<LForAll>(upcasted_type);
                if (type->binders.size() == 0) {
                    auto body = type->body->clone();
                    if (!(type = downcast_unique<LForAll>(body))) {
                        // Logically, this must have happened on the last step, or there is an error
                        return body;
                    }
                }
            }
            return type;
        } else {
            if (app->args.size() == 0) {
                return type_generic;
            } else {
                log("LApp has too many arguments to determine type", LogLevel::WARNING);
                return nullptr;
            }
        }
    } else if (auto lam = downcast_clone<LLambda>(expr)) {
        // An LLambda expression has an LForAll type
        // We construct the LForAll one binder at a time to ensure
        // all of the applications are unfolded properly
        std::unique_ptr<LExpr> type = infer_type(lam->body);
        for (int i = lam->binders.size()-1; i >= 0; i--) {
            type = std::make_unique<LForAll>(downcast_unique<LBinder>(lam->binders[i]->clone()), std::move(type));
        }
        return type;
    } else if (auto fa = downcast_clone<LForAll>(expr)) {
        // For simplicity, we just return "Type" here, however this ignores
        // the possibility of higher universes in the expression
        return std::make_unique<LSort>(std::make_unique<LLevel>(LLevel::Type::Succ, std::make_unique<LLevel>()));
    } else if (auto let = downcast_clone<LLet>(expr)) {
        // This is just the type of the body
        return infer_type(let->body);
    } else if (auto proj = downcast_clone<LProj>(expr)) {
        log("Cannot infer type of LProj", LogLevel::WARNING);
        return nullptr;
    // TExpr's
    } else if (auto have = downcast_clone<THave>(expr)) {
        auto have_ptr = have->body->clone();
        return infer_type(have_ptr);
    } else if (auto intro = downcast_clone<TIntro>(expr)) {
        // Convert to an LForAll
        auto intro_ptr = intro->body->clone();
        auto lfa = std::make_unique<LForAll>(nullptr, infer_type(intro_ptr));
        lfa->binders.clear(); // Will start with one arg from constructor
        for (auto& param : intro->params) {
            lfa->binders.push_back(downcast_unique<LBinder>(param->clone()));
        }
        auto lfa_ptr = downcast_unique<LExpr>(std::move(lfa));
        return lfa_ptr;
    } else if (auto goal = downcast_clone<TGoal>(expr)) {
        auto goal_ptr = goal->body->clone();
        return infer_type(goal_ptr);
    } else if (auto app = downcast_clone<TApply>(expr)) {
        // Convert to an LApp
        auto lapp = std::make_unique<LApp>(app->fn->clone(), nullptr);
        lapp->args.clear(); // Will start with one arg from constructor
        for (auto& arg : app->args) {
            lapp->args.push_back(arg->clone());
        }
        auto lapp_ptr = downcast_unique<LExpr>(std::move(lapp));
        return infer_type(lapp_ptr);
    }
    log("Cannot infer type of expression: "+expr->to_string(), LogLevel::DEBUG);
    return nullptr;
}