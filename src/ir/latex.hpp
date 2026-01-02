// ir/latex.hpp - Latex IR representation
#pragma once
#include "utility/json.hpp"
#include "utility/misc.hpp"
#include <memory>
#include <string>
#include <vector>
#include <utility>
#include "ir/lean.hpp"

/*
TExpr is a base class for Latex IR expressions.
It inherits basic functionality from LExpr (and includes some LExpr types as sub-expressions).
However, it is designed to be closer to Lean's "tactic mode", which builds up
a proof expression in a way that is similar to a written proof.
*/
struct TExpr : public LExpr {};

/*
THave is similar to the `have` tactic in Lean,
and also represents `have` or `let` statements from term mode.
THave does not flatten nested lets/haves.
*/
struct THave : public TExpr {
    std::string name;
    std::unique_ptr<LExpr> type;
    std::unique_ptr<TExpr> value;
    std::unique_ptr<TExpr> body;

    THave(std::string name, std::unique_ptr<LExpr> type, std::unique_ptr<TExpr> value, std::unique_ptr<TExpr> body) : name(std::move(name)) {
        // Set pointers
        if (type) {
            this->type = std::move(type);
            this->type->parent = this;
        }
        if (value) {
            this->value = std::move(value);
            this->value->parent = this;
        }
        if (body) {
            this->body = std::move(body);
            this->body->parent = this;
        }
    };

    json to_json() const override {
        if (!type) {
            throw std::runtime_error("THave missing type");
        }
        if (!value) {
            throw std::runtime_error("THave missing value");
        }
        if (!body) {
            throw std::runtime_error("THave missing body");
        }
        return json{{"kind", "have"}, {"name", name}, {"type", type->to_json()}, {"value", value->to_json()}, {"body", body->to_json()}};
    }

    std::string to_string() const noexcept override {
        std::string out;
        out += "have " + name + " : ";
        if (type) {
            out += type->to_string();
        } else {
            out += "?nullptr";
        }
        out += " := by ";
        if (value) {
            out += "(" + value->to_string() + ")";
        } else {
            out += "(?nullptr)";
        }
        out += "; ";
        if (body) {
            out += "(" + body->to_string() + ")";
        } else {
            out += "(?nullptr)";
        }
        return out;
    }
};

/*
TIntro is similar to the `intro` tactic in Lean,
representing the introduction of one or more binders.
It is similar to lambda expressions in term mode.
*/
struct TIntro : public TExpr {
    std::vector<std::unique_ptr<LBinder>> params;
    std::unique_ptr<TExpr> body;
    TIntro(std::vector<std::unique_ptr<LBinder>> params, std::unique_ptr<TExpr> body) : params(std::move(params)), body(std::move(body)) {
        for (auto& param : this->params) {
            if (param) {
                param->parent = this; // the intro statement has the names from and ownership of the binders
            }
        }
        if (body) {
            this->body = std::move(body);
            this->body->parent = this;
        }
    };
    json to_json() const override {
        json jparams = json::array();
        for (const auto& param : params) {
            if (!param) {
                throw std::runtime_error("TIntro statement has null parameter");
            }
            jparams.push_back(param->to_json());
        }
        if (!body) {
            throw std::runtime_error("TIntro missing body");
        }
        return json{{"kind", "intro"}, {"params", jparams}, {"body", body->to_json()}};
    };
    std::string to_string() const noexcept override {
        std::string out = "intro ";
        for (const auto& param : params) {
            if (param) {
                out += param->to_string() + " ";
            } else {
                out += "(?nullptr) ";
            }
        }
        out += "; ";
        if (body) {
            out += body->to_string();
        } else {
            out += "(?nullptr)";
        }
        return out;
    }
};

/*
TGoal represents solving for a single goal when there are multiple goals.
It explicitly names the goal using a binder. It is most similar to the `case` tactic in Lean.
*/
struct TGoal : public TExpr {
    std::unique_ptr<LBinder> param;
    std::unique_ptr<TExpr> body;
    TGoal(std::unique_ptr<LBinder> param, std::unique_ptr<TExpr> body) : param(std::move(param)), body(std::move(body)) {
        if (this->param) {
            this->param->parent = this;
        }
        if (this->body) {
            this->body->parent = this;
        }
    };
    json to_json() const override {
        if (!param) {
            throw std::runtime_error("TGoal has null parameter");
        }
        if (!body) {
            throw std::runtime_error("TGoal missing body");
        }
        return json{{"kind", "goal"}, {"param", param->to_json()}, {"body", body->to_json()}};
    };
    std::string to_string() const noexcept override {
        std::string out = "case ";
        if (param) {
            out += param->to_string() + " ";
        } else {
            out += "(?nullptr) ";
        }
        out += "=> ";
        if (body) {
            out += body->to_string();
        } else {
            out += "(?nullptr)";
        }
        return out;
    }
};

/*
TApply is similar to the `apply` and `exact` tactics in Lean.
It is represented as function application in term mode.
The `exact` tactic can be represented as an application with only LExpr arguments
(i.e. all arguments have already been solved).
*/
struct TApply : public TExpr {
    std::unique_ptr<LExpr> fn;
    std::vector<std::unique_ptr<LExpr>> args;

    TApply(std::unique_ptr<LExpr> fn, std::vector<std::unique_ptr<LExpr>> args) : fn(std::move(fn)), args(std::move(args)) {
        for (auto& arg : this->args) {
            if (arg) {
                arg->parent = this;
            }
        }
        if (this->fn) {
            this->fn->parent = this;
        }
    };

    json to_json() const override {
        if (!fn) {
            throw std::runtime_error("TApply missing function");
        }
        json jargs = json::array();
        for (const auto& arg : args) {
            if (!arg) {
                throw std::runtime_error("TApply has null argument");
            }
            jargs.push_back(arg->to_json());
        }
        return json{{"kind", "app"}, {"fn", fn->to_json()}, {"args", jargs}};
    }

    bool is_exact() const noexcept {
        for (const auto& arg : args) {
            if (is_a<TExpr>(arg)) {
                return false;
            }
        }
        return true;
    }

    std::string to_string() const noexcept override {
        std::string out = is_exact() ? "exact " : "apply ";
        if (fn) {
            out += fn->to_string();
        } else {
            out += "?nullptr";
        }
        for (const auto& arg : args) {
            out += " ";
            if (arg) {
                out += "(" + arg->to_string() + ")";
            } else {
                out += "(?nullptr)";
            }
        }
        return out;
    }
};

/*
Top-level declarations
*/

// A proof
struct TProof : public TExpr {
    std::unique_ptr<TExpr> expr;

    TProof(std::unique_ptr<TExpr> expr) : expr(std::move(expr)) {
        if (expr) {
            expr->parent = this;
        }
    };

    json to_json() const override {
        if (!expr) {
            throw std::runtime_error("TProof missing expr");
        }
        return json{{"kind", "proof"}, {"expr", expr->to_json()}};
    }

    std::string to_string() const noexcept override {
        if (!expr)
            return "?nullptr";
        return expr->to_string();
    }
};
/* A theorem.
 * e.x. theorem ex_falso type False -> p apply False.elim
 */
struct TTheorem : public TExpr {
    std::string name;
    std::unique_ptr<LExpr> type;
    std::unique_ptr<TProof> proof;
    TTheorem(std::string name, std::unique_ptr<LExpr> type, std::unique_ptr<TProof> proof): name(std::move(name)), type(std::move(type)), proof(std::move(proof)) {
        if (type) {
            type->parent = this;
        }
        if (proof) {
            proof->parent = this;
        }
    }
    json to_json() const override {
        if (!type) {
            throw std::runtime_error("TTheorem " + name + " missing type");
        }
        if (!proof) {
            throw std::runtime_error("TTheorem " + name + " missing proof");
        }
        return json{{"kind", "theorem"}, {"name", name}, {"type", type->to_json()}, {"proof", proof->to_json()}};
    };

    std::string to_string() const noexcept override {
        std::string out = "theorem " + name + " : ";
        if (type) {
            out += type->to_string();
        } else {
            out += "?nullptr";
        }
        out += " := by ";
        if (proof) {
            out += proof->to_string();
        } else {
            out += "?nullptr";
        }
        return out;
    }
};