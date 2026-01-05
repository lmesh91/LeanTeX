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

The to_tactic function is designed to emit a tactic mode proof in Lean.
*/
struct TExpr : public LExpr {
    static const int INDENT_SIZE = 2;
    virtual std::string to_tactic(int depth = 0) const noexcept = 0;
};

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

    std::unique_ptr<LExpr> clone() const override {
        return std::make_unique<THave>(name, type ? type->clone() : nullptr, value ? downcast_unique<TExpr>(value->clone()) : nullptr, body ? downcast_unique<TExpr>(body->clone()) : nullptr);
    }

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
            out += value->to_string();
        } else {
            out += "?nullptr";
        }
        out += "; ";
        if (body) {
            out += body->to_string();
        } else {
            out += "?nullptr";
        }
        return out;
    }

    std::string to_tactic(int depth) const noexcept override {
        std::string out;
        out += std::string(depth, ' ') + "have " + name + " : ";
        if (type) {
            out += type->to_string();
        } else {
            out += "?nullptr";
        }
        out += " := by\n";
        if (value) {
            out += value->to_tactic(depth+INDENT_SIZE);
        } else {
            out += std::string(depth+INDENT_SIZE, ' ') + "?nullptr";
        }
        out += "\n";
        if (body) {
            out += body->to_tactic(depth);
        } else {
            out += std::string(depth, ' ') + "?nullptr";
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
    std::unique_ptr<LExpr> clone() const override {
        std::vector<std::unique_ptr<LBinder>> param_clones;
        for (const auto& param : params) {
            param_clones.push_back(param ? downcast_unique<LBinder>(param->clone()) : nullptr);
        }
        return std::make_unique<TIntro>(std::move(param_clones), body ? downcast_unique<TExpr>(body->clone()) : nullptr);
    }
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
        for (size_t i = 0; i < params.size(); i++) {
            if (params.at(i)) {
                out += get_var_name(params.at(i)->name) + " /- ";
                if (params.at(i)->type) {
                    out += params.at(i)->type->to_string();
                } else {
                    out += "?nullptr";
                }
                out += " -/";
            } else {
                out += "?nullptr";
            }
            if (i < params.size() - 1) {
                out += " ";
            }
        }
        out += "; ";
        if (body) {
            out += body->to_string();
        } else {
            out += "?nullptr";
        }
        return out;
    }

    std::string to_tactic(int depth) const noexcept override {
        std::string out = std::string(depth, ' ') + "intro ";
        for (size_t i = 0; i < params.size(); i++) {
            if (params.at(i)) {
                out += get_var_name(params.at(i)->name) + " /- ";
                if (params.at(i)->type) {
                    out += params.at(i)->type->to_string();
                } else {
                    out += "?nullptr";
                }
                out += " -/";
            } else {
                out += "?nullptr";
            }
            if (i < params.size() - 1) {
                out += " ";
            }
        }
        out += "\n";
        if (body) {
            out += body->to_tactic(depth);
        } else {
            out += "?nullptr";
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
    std::unique_ptr<LExpr> clone() const override {
        return std::make_unique<TGoal>(param ? downcast_unique<LBinder>(param->clone()) : nullptr, body ? downcast_unique<TExpr>(body->clone()) : nullptr);
    }
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
            out += get_var_name(param->name) + " /- ";
            if (param->type) {
                out += param->type->to_string();
            } else {
                out += "?nullptr";
            }
            out += " -/ ";
        } else {
            out += "?nullptr";
        }
        out += "=> ";
        if (body) {
            out += body->to_string();
        } else {
            out += "?nullptr";
        }
        return out;
    }
    std::string to_tactic(int depth) const noexcept override {
        std::string out = std::string(depth, ' ') + "case ";
        if (param) {
            out += get_var_name(param->name) + " /- ";
            if (param->type) {
                out += param->type->to_string();
            } else {
                out += "?nullptr";
            }
            out += " -/ ";
        } else {
            out += "?nullptr";
        }
        out += "=>\n";
        if (body) {
            out += body->to_tactic(depth+INDENT_SIZE);
        } else {
            out += std::string(depth+INDENT_SIZE, ' ') + "?nullptr";
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

    std::unique_ptr<LExpr> clone() const override {
        std::unique_ptr<LExpr> fn_clone = fn ? fn->clone() : nullptr;
        std::vector<std::unique_ptr<LExpr>> arg_clones;
        for (const auto& arg : args) {
            arg_clones.push_back(arg ? arg->clone() : nullptr);
        }
        return std::make_unique<TApply>(std::move(fn_clone), std::move(arg_clones));
    }

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
            if (is_a<TExpr>(arg)) {
                out += "; ";
                if (arg) {
                    out += arg->to_string();
                } else {
                    out += "?nullptr";
                }
            } else {
                out += " ";
                if (arg) {
                    out += "(" + arg->to_string() + ")";
                } else {
                    out += "(?nullptr)";
                }
            }
        }
        return out;
    }

    std::string to_tactic(int depth) const noexcept override {
        std::string out = std::string(depth, ' ') + (is_exact() ? "exact " : "apply ");
        if (fn) {
            out += fn->to_string();
        } else {
            out += "?nullptr";
        }
        for (const auto& arg : args) {
            if (auto t_arg = dynamic_cast<TExpr*>(arg.get())) {
                out += "\n" + t_arg->to_tactic(depth);
            } else {
                out += " ";
                if (arg) {
                    out += "(" + arg->to_string() + ")";
                } else {
                    out += "(?nullptr)";
                }
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

    std::unique_ptr<LExpr> clone() const override {
        return std::make_unique<TProof>(expr ? downcast_unique<TExpr>(expr->clone()) : nullptr);
    }

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

    std::string to_tactic(int depth) const noexcept override {
        if (!expr)
            return std::string(depth, ' ') + "?nullptr";
        return expr->to_tactic(depth);
    }
};
/* A theorem.
 * e.x. theorem ex_falso type False -> p apply False.elim
 */
struct TTheorem : public TExpr {
    std::string name;
    std::vector<std::unique_ptr<LBinder>> params;
    std::unique_ptr<LExpr> type;
    std::unique_ptr<TProof> proof;
    TTheorem(std::string name, std::vector<std::unique_ptr<LBinder>> params, std::unique_ptr<LExpr> type, std::unique_ptr<TProof> proof): name(std::move(name)), params(std::move(params)), type(std::move(type)), proof(std::move(proof)) {
        if (type) {
            type->parent = this;
        }
        if (proof) {
            proof->parent = this;
        }
        for (auto& param : this->params) {
            if (param) {
                param->parent = this;
            }
        }
    }
    std::unique_ptr<LExpr> clone() const override {
        std::vector<std::unique_ptr<LBinder>> param_clones;
        for (const auto& param : params) {
            param_clones.push_back(param ? downcast_unique<LBinder>(param->clone()) : nullptr);
        }
        return std::make_unique<TTheorem>(name, std::move(param_clones), type ? type->clone() : nullptr, proof ? downcast_unique<TProof>(proof->clone()) : nullptr);
    }
    json to_json() const override {
        if (!type) {
            throw std::runtime_error("TTheorem " + name + " missing type");
        }
        if (!proof) {
            throw std::runtime_error("TTheorem " + name + " missing proof");
        }
        json jparams = json::array();
        for (const auto& param : params) {
            if (!param) {
                throw std::runtime_error("LTheorem " + name + " has null parameter");
            }
            jparams.push_back(param->to_json());
        }
        return json{{"kind", "theorem"}, {"name", name}, {"params", jparams}, {"type", type->to_json()}, {"proof", proof->to_json()}};
    };

    std::string to_string() const noexcept override {
        std::string out = "theorem " + name + " ";
        for (const auto& param : params) {
            if (param) {
                out += param->to_string() + " ";
            } else {
                out += "(?nullptr) ";
            }
        }
        out += ": ";
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

    std::string to_tactic(int depth) const noexcept override {
        std::string out = std::string(depth, ' ') + "theorem " + name + " ";
        for (const auto& param : params) {
            if (param) {
                out += param->to_string() + " ";
            } else {
                out += "(?nullptr) ";
            }
        }
        out += ": ";
        if (type) {
            out += type->to_string();
        } else {
            out += "?nullptr";
        }
        out += " := by\n";
        if (proof) {
            out += proof->to_tactic(depth+INDENT_SIZE);
        } else {
            out += std::string(depth+INDENT_SIZE, ' ') + "?nullptr";
        }
        return out;
    }
};