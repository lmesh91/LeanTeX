// ir/latex.hpp - Latex IR representation
/*
 * This representation as an AST is very similar to the Lean IR
 * And is in fact even more similar to lean's "tactic" mode
 */
#pragma once
#include "utility/json.hpp"
#include "utility/misc.hpp"
#include <memory>
#include <string>
#include <vector>
#include <utility>
#include "ir/lean.hpp"

/*
THave represents `have` expressions in Latex IR.
THave does not flatten nested lets/haves.
*/
struct THave : public LExpr {
    std::string name;
    std::unique_ptr<LExpr> type;
    std::unique_ptr<LExpr> value;
    std::unique_ptr<LExpr> body;

    THave(std::string name, std::unique_ptr<LExpr> type, std::unique_ptr<LExpr> value, std::unique_ptr<LExpr> body) : name(std::move(name)) {
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
            throw std::runtime_error("LLet missing type");
        }
        if (!value) {
            throw std::runtime_error("LLet missing value");
        }
        if (!body) {
            throw std::runtime_error("LLet missing body");
        }
        return json{{"kind", "let"}, {"name", name}, {"type", type->to_json()}, {"value", value->to_json()}, {"body", body->to_json()}};
    }

    std::string to_string() const noexcept override {
        std::string out;
        out += "have " + name + " : ";
        if (type) {
            out += type->to_string();
        } else {
            out += "?nullptr";
        }
        out += " := ";
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
 * TIntro - A representation of an "intro" statement - simply intro attached to one or more TBinders
 */
struct TIntro : public LExpr {
    std::vector<std::unique_ptr<LBinder>> params;
    TIntro(std::vector<std::unique_ptr<LBinder>> params) : params(std::move(params)) {
        for (auto& param : this->params) {
            if (param) {
                param->parent = this; // the intro statement has the names from and ownership of the binders
            }
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
        return json{{"kind", "intro"}, {"params", jparams}};
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
        return out;
    }
};

/*
 * TGoal - A representation of a "goal" statement - a goal attached to one or more binders
 * and a series of statements that proves the goal
 */

struct TGoal : public LExpr {
    std::vector<std::unique_ptr<LBinder>> params;
    std::vector<std::unique_ptr<LExpr>> statements;
    TGoal(std::vector<std::unique_ptr<LBinder>> params, std::vector<std::unique_ptr<LExpr>> statements) : params(std::move(params)), statements(std::move(statements)) {
        for (auto& param : this->params) {
            if (param) {
                param->parent = this; // the intro statement has the names from and ownership of the binders
            }
        }
        for (auto& statement : this->statements) {
            if (statement) {
                statement->parent = this; // the intro statement has the names from and ownership of the binders
            }
        }
    };
    json to_json() const override {
        json jparams = json::array();
        for (const auto& param : params) {
            if (!param) {
                throw std::runtime_error("TGoal statement has null parameter");
            }
            jparams.push_back(param->to_json());
        }
        json jstatements = json::array();
        for (const auto& statement : statements) {
            if (!statement) {
                throw std::runtime_error("TGoal statement has null parameter");
            }
            jstatements.push_back(statement->to_json());
        }
        return json{{"kind", "goal"}, {"params", jparams}, {"statements", jstatements}};
    };
    std::string to_string() const noexcept override {
        std::string out = "goal ";
        for (const auto& param : params) {
            if (param) {
                out += param->to_string() + " ";
            } else {
                out += "(?nullptr) ";
            }
        }
        out += "\n";
        for (const auto& statement : statements) {
            if (statement) {
                out += statement->to_string() + "\n";
            } else {
                out += "(?nullptr) ";
            }
        }
        return out;
    }
};

/*
 * TApply statements are actually a wrapper for a function application!
 */
struct TApply : public LExpr {
    std::unique_ptr<LApp> fn;
    TApply(std::unique_ptr<LApp> fn) : fn(std::move(fn)) {
        if (fn) {
            fn->parent = this; // the intro statement has the names from and ownership of the binders
        }
    };
    json to_json() const override {
        json jparams = json::array();
        if (!fn) {
            throw std::runtime_error("TApply statement has null parameter");
        }
        return json{{"kind", "apply"}, {"app", fn->to_json()}};
    };
    std::string to_string() const noexcept override {
        std::string out = "apply ";
        if (fn) {
            out += fn->to_string() + " ";
        } else {
            out += "(?nullptr) ";
        }
        return out;
    }
};

/*
 * TExact statements are identical to TApply statements, but are semantically different
 * as they are intended to represent statements that end right on variables/constants
 */
struct TExact : public LExpr {
    std::unique_ptr<LApp> fn;
    TExact(std::unique_ptr<LApp> fn) : fn(std::move(fn)) {
        if (fn) {
            fn->parent = this; // the intro statement has the names from and ownership of the binders
        }
    };
    json to_json() const override {
        json jparams = json::array();
        if (!fn) {
            throw std::runtime_error("TExact statement has null parameter");
        }
        return json{{"kind", "exact"}, {"app", fn->to_json()}};
    };
    std::string to_string() const noexcept override {
        std::string out = "exact ";
        if (fn) {
            out += fn->to_string() + " ";
        } else {
            out += "(?nullptr) ";
        }
        return out;
    }
};

/*
Top-level declarations
*/

// A proof
struct TProof : public LExpr {
    std::unique_ptr<LExpr> expr;

    TProof(std::unique_ptr<LExpr> expr) : expr(std::move(expr)) {
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
struct TTheorem : public LExpr {
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
        std::string out = "theorem " + name + "\n";
        out += "type ";
        if (type) {
            out += type->to_string();
        } else {
            out += "?nullptr";
        }
        out += "\n\n";
        if (proof) {
            out += proof->to_string();
        } else {
            out += "?nullptr";
        }
        return out;
    }
};