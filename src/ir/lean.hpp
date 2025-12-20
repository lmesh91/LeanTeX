// ir/lean.hpp - Lean IR representation
#pragma once
#include "utility/json.hpp"
#include <memory>
#include <string>
#include <vector>
using json = nlohmann::json;

/*
General notes compared to the Lean 4 Expr type:
- Every function-based type (e.g. LApp, LLambda) combines all arguments,
  rather that nesting them into single-argument applications.
- Some types are combined/split up to more align with Lean Syntax.
*/

struct LExpr {

    virtual ~LExpr() = default;

    // Converts the expression to JSON for debugging purposes
    virtual json to_json() const = 0;

    // Converts the type to a string in a safe way,
    // so it can be used in error messages.
    virtual std::string to_string() const noexcept = 0;
};

// Identifiers - used for constants and variables
struct LIdent : public LExpr {
    std::string name;

    LIdent(std::string name) : name(std::move(name)) {};

    json to_json() const override {
        return json{{"kind", "const"}, {"name", name}};
    }

    std::string to_string() const noexcept override {
        return name;
    }
};

// Used for inline variables bound to a type, e.g. p : Prop or x y : Nat
struct LBinder : public LExpr {
    std::string name;
    std::unique_ptr<LExpr> type;

    LBinder(std::string name, std::unique_ptr<LExpr> type) : name(std::move(name)), type(std::move(type)) {};

    json to_json() const override {
        if (!type) {
            throw std::runtime_error("LBinder \"" + name + "\" missing type");
        }
        return json{{"kind", "binder"}, {"name", name}, {"type", type->to_json()}};
    }

    std::string to_string() const noexcept override {
        std::string out = name + " ";
        if (!type) {
            return out + ": ?nullptr";
        }
        return  out + ": " + type->to_string();
    }
};

// Function application. Partial application is not used.
struct LApp : public LExpr {
    std::unique_ptr<LExpr> fn;
    std::vector<std::unique_ptr<LExpr>> args;

    LApp(std::unique_ptr<LExpr> fn, std::vector<std::unique_ptr<LExpr>> args) : fn(std::move(fn)), args(std::move(args)) {};

    json to_json() const override {
        if (!fn) {
            throw std::runtime_error("LApp missing function");
        }
        json jargs = json::array();
        for (const auto& arg : args) {
            if (!arg) {
                throw std::runtime_error("LApp " + to_string() + " has null argument");
            }
            jargs.push_back(arg->to_json());
        }
        return json{{"kind", "app"}, {"fn", fn->to_json()}, {"args", jargs}};
    }

    // This syntax differs slightly from what Lean would do, so that parentheses are always explicit.
    std::string to_string() const noexcept override {
        std::string fn_str = "?nullptr";
        if (fn) {
            fn_str = fn->to_string();
        };
        std::vector<std::string> arg_strs;
        for (const auto& arg : args) {
            if (arg) {
                arg_strs.push_back(arg->to_string());
            } else {
                arg_strs.push_back("?nullptr");
            }
        }
        std::string out = fn_str;
        for (const auto& arg_str : arg_strs) {
            out += " (" + arg_str + ")";
        }
        return out;

    }
};

// Lambda expressions (e.g. fun x => x)
struct LLambda : public LExpr {
    std::vector<std::unique_ptr<LBinder>> params;
    std::unique_ptr<LExpr> body;

    LLambda(std::vector<std::unique_ptr<LBinder>> params, std::unique_ptr<LExpr> body) : params(std::move(params)), body(std::move(body)) {};

    json to_json() const override {
        json jparams = json::array();
        for (const auto& param : params) {
            if (!param) {
                throw std::runtime_error("LLambda " + to_string() + " has null parameter");
            }
            jparams.push_back(param->to_json());
        }
        if (!body) {
            throw std::runtime_error("LLambda " + to_string() + " missing body");
        }
        return json{{"kind", "lambda"}, {"params", jparams}, {"body", body->to_json()}};
    }

    std::string to_string() const noexcept override {
        std::string out = "fun ";
        for (const auto& param : params) {
            if (param) {
                out += "(" + param->to_string() + ") ";
            } else {
                out += "(?nullptr) ";
            }
        }
        out += "=> ";
        if (body) {
            out += "(" + body->to_string() + ")";
        } else {
            out += "(?nullptr)";
        }
        return out;
    }
};

// Arrow expressions
// (e.g. Nat -> Nat)
struct LArrow : public LExpr {
    std::unique_ptr<LExpr> param;
    std::unique_ptr<LExpr> body;

    LArrow(std::unique_ptr<LExpr> param, std::unique_ptr<LExpr> body) : param(std::move(param)), body(std::move(body)) {};

    json to_json() const override {
        if (!param) {
            throw std::runtime_error("LArrow " + to_string() + " has null parameter");
        }
        if (!body) {
            throw std::runtime_error("LArrow " + to_string() + " missing body");
        }
        return json{{"kind", "arrow"}, {"param", param->to_json()}, {"body", body->to_json()}};
    }

    // This syntax differs slightly from what Lean would do,
    // as type names are always explicit and ASCII arrows are used.
    std::string to_string() const noexcept override {
        std::string out;
        if (param) {
            out += "(" + param->to_string() + ") -> ";
        } else {
            out += "(?nullptr) -> ";
        }
        if (body) {
            out += "(" + body->to_string() + ")";
        } else {
            out += "(?nullptr)";
        }
        return out;
    }
};

// Forall expressions. These are *dependent* arrows
// (e.g. ∀ x : Nat, x > 0)
struct LForAll : public LExpr {
    std::vector<std::unique_ptr<LBinder>> params;
    std::unique_ptr<LExpr> body;

    LForAll(std::vector<std::unique_ptr<LBinder>> params, std::unique_ptr<LExpr> body) : params(std::move(params)), body(std::move(body)) {};

    json to_json() const override {
        json jparams = json::array();
        for (const auto& param : params) {
            if (!param) {
                throw std::runtime_error("LForAll " + to_string() + " has null parameter");
            }
            jparams.push_back(param->to_json());
        }
        if (!body) {
            throw std::runtime_error("LForAll " + to_string() + " missing body");
        }
        return json{{"kind", "forall"}, {"params", jparams}, {"body", body->to_json()}};
    }

    // This syntax differs slightly from what Lean would do,
    // as type names are always explicit and ASCII arrows are used.
    std::string to_string() const noexcept override {
        std::string out = "∀";
        for (const auto& param : params) {
            if (param) {
                out += " (" + param->to_string() + ")";
            } else {
                out += " (?nullptr)";
            }
        }
        out += ", ";
        if (body) {
            out += "(" + body->to_string() + ")";
        } else {
            out += "(?nullptr)";
        }
        return out;
    }
};

// Let and have expressions, e.g. have x : Nat := 5
// In Lean 4's Expr, these also store information about the scope where the variable is used.
// However, this is not needed in Lean IR.
struct LHave : public LExpr {
    std::unique_ptr<LBinder> binder;
    std::unique_ptr<LExpr> value;

    LHave(std::unique_ptr<LBinder> binder, std::unique_ptr<LExpr> value) : binder(std::move(binder)), value(std::move(value)) {};

    json to_json() const override {
        if (!binder) {
            throw std::runtime_error("LHave missing binder");
        }
        if (!value) {
            throw std::runtime_error("LHave " + to_string() + " missing value");
        }
        return json{{"kind", "have"}, {"binder", binder->to_json()}, {"value", value->to_json()}};
    }

    std::string to_string() const noexcept override {
        std::string out = "have ";
        if (binder) {
            out += binder->to_string();
        } else {
            out += "?nullptr";
        }
        out += " := ";
        if (value) {
            out += value->to_string();
        } else {
            out += "?nullptr";
        }
        return out;
    }
};

// Integer and string literals
struct LLiteral : public LExpr {
    std::string value; // Store all literals as strings for simplicity

    LLiteral(std::string value) : value(std::move(value)) {};

    json to_json() const override {
        return json{{"kind", "literal"}, {"value", value}};
    }
    std::string to_string() const noexcept override {
        return value;
    }
};

/*
Top-level declarations
For now, only theorems in term mode are supported.
*/

// No extra data, used for typing purposes
struct LProof : public LExpr {};

// A proof in term mode.
struct LTermProof : public LProof {
    std::unique_ptr<LExpr> expr;

    LTermProof(std::unique_ptr<LExpr> expr) : expr(std::move(expr)) {};

    json to_json() const override {
        if (!expr) {
            throw std::runtime_error("LTermProof missing expr");
        }
        return json{{"kind", "term"}, {"expr", expr->to_json()}};
    }

    std::string to_string() const noexcept override {
        if (!expr)
            return "?nullptr";
        return expr->to_string();
    }
};

// A theorem.
// e.g. theorem ex_falso (p : Prop) : False -> p := False.elim
struct LTheorem : public LExpr {
    std::string name;
    std::vector<std::unique_ptr<LBinder>> params;
    std::unique_ptr<LExpr> type;
    std::unique_ptr<LProof> proof;

    LTheorem(std::string name, std::vector<std::unique_ptr<LBinder>> params, std::unique_ptr<LExpr> type, std::unique_ptr<LProof> proof) : name(std::move(name)), params(std::move(params)), type(std::move(type)), proof(std::move(proof)) {};

    json to_json() const override {
        if (!type) {
            throw std::runtime_error("LTheorem " + name + " missing type");
        }
        if (!proof) {
            throw std::runtime_error("LTheorem " + name + " missing proof");
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
                out += "(" + param->to_string() + ") ";
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
        out += " := ";
        if (proof) {
            out += proof->to_string();
        } else {
            out += "?nullptr";
        }
        return out;
    }
};