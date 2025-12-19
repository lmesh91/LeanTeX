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
- Some types (e.g. variables, let/have, literals) are simplified for clarity.
- Position metadata is also included when possible.
*/

struct LExpr {
    // Metadata shared by all expressions
    int pos; // Position is used instead of line/column to match Jixia output

    virtual ~LExpr() = default;

    // Converts the expression to JSON for debugging purposes
    virtual json to_json() const = 0;

    // Converts the metadata to JSON
    json meta_json() const {
        json j;
        if (pos >= 0) {
            j["pos"] = pos;
        }
        return j;
    }

    // Converts the type to a string in a safe way,
    // so it can be used in error messages.
    virtual std::string to_string() const noexcept = 0;
};

// Used for constants that have been defined elsewhere
// (e.g. other theorems or functions)
struct LConst : public LExpr {
    std::string name;

    json to_json() const override {
        return json{{"kind", "const"}, {"name", name}, {"meta", meta_json()}};
    }

    std::string to_string() const noexcept override {
        return "C_" + name;
    }
};

// Used for variables defined within an object
// (e.g. parameters used in a function or proof)
struct LVar : public LExpr {
    std::string name;

    json to_json() const override {
        return json{{"kind", "var"}, {"name", name}, {"meta", meta_json()}};
    }

    std::string to_string() const noexcept override {
        return "V_" + name;
    }
};

// Used for inline variables bound to a type, e.g. p : Prop
struct LBinder : public LExpr {
    std::string name;
    std::unique_ptr<LExpr> type;

    json to_json() const override {
        if (!type) {
            throw std::runtime_error("LBinder \"" + name + "\" missing type");
        }
        return json{{"kind", "binder"}, {"name", name}, {"type", type->to_json()}, {"meta", meta_json()}};
    }

    std::string to_string() const noexcept override {
        return name;
    }
};

// Function application. Partial application is not used.
struct LApp : public LExpr {
    std::unique_ptr<LExpr> fn;
    std::vector<std::unique_ptr<LExpr>> args;

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
        return json{{"kind", "app"}, {"fn", fn->to_json()}, {"args", jargs}, {"meta", meta_json()}};
    }

    std::string to_string() const noexcept override {
        if (!fn) {
            return "A_<nullptr>";
        }
        return "A_" + fn->to_string();
    }
};

// Lambda expressions (e.g. fun x : Nat => x)
struct LLambda : public LExpr {
    std::vector<std::unique_ptr<LBinder>> params;
    std::unique_ptr<LExpr> body;

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
        return json{{"kind", "lambda"}, {"params", jparams}, {"body", body->to_json()}, {"meta", meta_json()}};
    }

    std::string to_string() const noexcept override {
        std::string out = "L_";
        for (const auto& param : params) {
            if (param) {
                out += param->to_string() + "_";
            } else {
                out += "<nullptr>_";
            }
        }
        return out.substr(0, out.size() - 1); // Remove trailing underscore
    }
};

// Arrow expressions, including dependent function types
// (e.g. Nat -> Nat, or ∀ x : Nat, x > 0)
struct LArrow : public LExpr {
    std::vector<std::unique_ptr<LBinder>> params;
    std::unique_ptr<LExpr> body;

    json to_json() const override {
        json jparams = json::array();
        for (const auto& param : params) {
            if (!param) {
                throw std::runtime_error("LArrow " + to_string() + " has null parameter");
            }
            jparams.push_back(param->to_json());
        }
        if (!body) {
            throw std::runtime_error("LArrow " + to_string() + " missing body");
        }
        return json{{"kind", "arrow"}, {"params", jparams}, {"body", body->to_json()}, {"meta", meta_json()}};
    }

    std::string to_string() const noexcept override {
        std::string out = "R_";
        for (const auto& param : params) {
            if (param) {
                out += param->to_string() + "_";
            } else {
                out += "<nullptr>_";
            }
        }
        return out.substr(0, out.size() - 1); // Remove trailing underscore
    }
};

// Let and have expressions, e.g. have x : Nat := 5
// In Lean 4's Expr, these also store information about the scope where the variable is used.
// However, this is not needed in Lean IR.
struct LLet : public LExpr {
    std::unique_ptr<LBinder> binder;
    std::unique_ptr<LExpr> value;

    json to_json() const override {
        if (!binder) {
            throw std::runtime_error("LLet missing binder");
        }
        if (!value) {
            throw std::runtime_error("LLet " + to_string() + " missing value");
        }
        return json{{"kind", "let"}, {"binder", binder->to_json()}, {"value", value->to_json()}, {"meta", meta_json()}};
    }

    std::string to_string() const noexcept override {
        if (!binder) {
            return "H_<nullptr>";
        }
        return "H_" + binder->to_string();
    }
};

// Integer and string literals
struct LLiteral : public LExpr {
    std::string value; // Store all literals as strings for simplicity
    json to_json() const override {
        return json{{"kind", "literal"}, {"value", value}, {"meta", meta_json()}};
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

    json to_json() const override {
        if (!expr) {
            throw std::runtime_error("LTermProof missing expr");
        }
        return json{{"kind", "term"}, {"expr", expr->to_json()}, {"meta", meta_json()}};
    }

    std::string to_string() const noexcept override {
        if (!expr)
            return "P_<nullptr>";
        return "P_" + expr->to_string();
    }
};

struct LTheorem : public LExpr {
    std::string name;
    std::vector<std::unique_ptr<LBinder>> params;
    std::unique_ptr<LExpr> type;
    std::unique_ptr<LProof> proof;

    json to_json() const override {
        json jparams = json::array();
        for (const auto& param : params) {
            if (!param) {
                throw std::runtime_error("LTheorem " + name + " has null parameter");
            }
            jparams.push_back(param->to_json());
        }
        if (!type) {
            throw std::runtime_error("LTheorem " + name + " missing type");
        }
        if (!proof) {
            throw std::runtime_error("LTheorem " + name + " missing proof");
        }
        return json{{"kind", "theorem"}, {"name", name}, {"params", jparams}, {"type", type->to_json()}, {"proof", proof->to_json()}, {"meta", meta_json()}};
    };

    std::string to_string() const noexcept override {
        return "T_" + name;
    }
};