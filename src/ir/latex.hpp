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
#include <unordered_set>
struct TExpr {
    // Non-owning pointer to parent expression to improve traversal of the tree
    // (children must not take ownership of their parents).
    TExpr *parent = nullptr;

    virtual ~TExpr() = default;

    // Converts the expression to JSON for debugging/output purposes
    virtual json to_json() const = 0;

    // Converts the type to a string in a way that
    // resembles Lean (tactic mode) syntax for debugging/output purposes
    virtual std::string to_string() const noexcept = 0;

    // Converts the type to a string in a way that
    // resembles Latex strings for natural language output
    virtual std::string to_latex() const noexcept = 0;
};

/*
TVar covers all types of variables in Latex IR:
- The original type is stored in the "type" field.
- Bound variables store their indices.
- Free variables and metavariables store their names.
- Solved variables store their names and are marked as solved.
*/
struct TVar : public TExpr {
    enum class Type {
        Bound,
        Free,
        Meta
    };
    Type type;
    std::string name;
    bool solved;
    unsigned int index;

    // Bound variable constructor
    LVar(int index) : type(Type::Bound), name(""), solved(false), index(index) {};
    // Free variable or metavariable constructor
    LVar(Type type, std::string name) : type(type), name(std::move(name)), solved(false), index(-1) {};

    // Mark the variable as solved with a given name
    // This should be called for every LVar object when converting to Lean IR,
    // once the original name of the variable has been figured out.
    bool solve(std::string solution_name) {
        if (solved) {
            return false;
        }
        name = std::move(solution_name);
        solved = true;
        return true;
    };

    json to_json() const override {
        if (solved) {
            return json{{"kind", "var"}, {"name", name}};
        }
        switch (type) {
            case Type::Bound:
                return json{{"kind", "bvar"}, {"index", index}};
            case Type::Free:
                return json{{"kind", "fvar"}, {"name", name}};
            case Type::Meta:
                return json{{"kind", "mvar"}, {"name", name}};
        }
        return json{};
    }
    std::string to_string() const noexcept override {
        std::string _name = name;
        // If the name ends with a dot followed by one or more digits (e.g. "x._@._internal._hyg.7"),
        // treat it as inaccessible and replace with the user-friendly version "x!7".
        auto pos = _name.rfind('.');
        if (pos != std::string::npos && pos + 1 < _name.size()) {
            bool all_digits = true;
            for (size_t i = pos + 1; i < _name.size(); ++i) {
                if (!std::isdigit(static_cast<unsigned char>(_name[i]))) {
                    all_digits = false;
                    break;
                }
            }
            if (all_digits) {
                _name = _name.substr(0, _name.find('.')) + "!" + _name.substr(pos+1);
            }
        }
        if (solved) {
            return _name;
        }
        switch (type) {
            case Type::Bound:
                return "?b." + std::to_string(index);
            case Type::Free:
                return "?f." + _name;
            case Type::Meta:
                return "?m." + _name;
        }
        return "?unknown";
    }

    std::string to_latex() const noexcept override {
        return latexify(this->to_string());
    }
};

/*
TConst represents constants in Lean.
This is separated from TVar as the difference between variables and constants
are crucial for translation into natural language.
*/
struct TConst : public TExpr {
    std::string name;

    TConst(std::string name) : name(std::move(name)) {};

    json to_json() const override {
        return json{{"kind", "const"}, {"name", name}};
    }

    // We use the most explicit string representation for constants,
    // e.g. @Array.map.{0, 0} instead of Array.map
    std::string to_string() const noexcept override {
        std::string out = "@" + name;
        return out;
    }
    std::string to_latex() const noexcept override {
        return this->to_string(); // this would usually be for a function name
        // for now, return the regular to_string to be parsed specifically based on the function name
    }
};

/*
TApp represents function applications in Lean.
Unlike in Lean Expr, nested applications are flattened.
*/
struct TApp : public TExpr {
    std::unique_ptr<TExpr> fn;
    std::vector<std::unique_ptr<TExpr>> args;

    TApp(std::unique_ptr<TExpr> fn, std::unique_ptr<TExpr> arg) {
        // Nest LApp functions to flatten applications
        std::unique_ptr<TApp> fn_app = fn == nullptr ? nullptr : downcast_unique<TApp>(fn);
        if (fn_app) {
            this->fn = std::move(fn_app->fn);
            this->args = std::move(fn_app->args);
        } else {
            this->fn = std::move(fn);
        }
        this->args.push_back(std::move(arg));
        // Set parent pointers (non-owning)
        if (this->fn) {
            this->fn->parent = this;
        }
        for (const auto& arg : this->args) {
            if (arg) {
                arg->parent = this;
            }
        }
    };

    json to_json() const override {
        if (!fn) {
            throw std::runtime_error("LApp missing function");
        }
        json jargs = json::array();
        for (const auto& arg : args) {
            if (!arg) {
                throw std::runtime_error("LApp has null argument");
            }
            jargs.push_back(arg->to_json());
        }
        return json{{"kind", "app"}, {"fn", fn->to_json()}, {"args", jargs}};
    }

    std::string to_string() const noexcept override {
        std::string out;
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

    std::string to_latex() const noexcept override {
        // this is where we would handle basic function parsing like And a b -> a \land b
        // todo - implement this using json files to access and store translations
        // for now, just return the to_string
        return this->to_string();
    }
};

/*
TBinder represents binder information in Lean, as used by (later function blocks)
This includes a name and a type, as well as what sort of binder it is.
*/
struct TBinder : public TExpr {
    // there is no better name for this than "Info"
    // since "type" and "kind" are both taken in the JSON representation
    enum class Info {
        Explicit,
        Implicit,
        StrictImplicit,
        InstImplicit
    };
    std::string name;
    std::unique_ptr<TExpr> type;
    Info info;
    TBinder(std::string name, std::unique_ptr<TExpr> type, Info info) : name(std::move(name)), info(info) {
        // Set pointers
        if (type) {
            this->type = std::move(type);
            this->type->parent = this;
        };
    };

    json to_json() const override {
        if (!type) {
            throw std::runtime_error("LBinder missing type");
        }
        std::string info_str;
        switch (info) {
            case Info::Explicit:
                info_str = "explicit";
                break;
            case Info::Implicit:
                info_str = "implicit";
                break;
            case Info::StrictImplicit:
                info_str = "strict_implicit";
                break;
            case Info::InstImplicit:
                info_str = "inst_implicit";
                break;
        }
        return json{{"kind", "binder"}, {"name", name}, {"type", type->to_json()}, {"info", info_str}};
    }

    std::string to_string() const noexcept override {
        std::string _name = name;
        // If the name ends with a dot followed by one or more digits (e.g. "x._@._internal._hyg.7"),
        // treat it as inaccessible and replace with the user-friendly version "x!7".
        auto pos = _name.rfind('.');
        if (pos != std::string::npos && pos + 1 < _name.size()) {
            bool all_digits = true;
            for (size_t i = pos + 1; i < _name.size(); ++i) {
                if (!std::isdigit(static_cast<unsigned char>(_name[i]))) {
                    all_digits = false;
                    break;
                }
            }
            if (all_digits) {
                _name = _name.substr(0, _name.find('.')) + "!" + _name.substr(pos+1);
            }
        }

        std::string out;
        switch (info) {
            case Info::Explicit:
                out += "(" + _name + " : ";
                break;
            case Info::Implicit:
                out += "{" + _name + " : ";
                break;
            case Info::StrictImplicit:
                out += "{{" + _name + " : ";
                break;
            case Info::InstImplicit:
                out += "[" + _name + " : ";
                break;
        }
        if (type) {
            out += type->to_string();
        } else {
            out += "?nullptr";
        }
        switch (info) {
            case Info::Explicit:
                out += ")";
                break;
            case Info::Implicit:
                out += "}";
                break;
            case Info::StrictImplicit:
                out += "}}";
                break;
            case Info::InstImplicit:
                out += "]";
                break;
        }
        return out;
    }

    std::string to_latex() const noexcept override {
        // latex for TBinder
        return type->to_latex(); // return the latex version of the type for processing
    }
};


/*
TLet represents `have` expressions in Latex IR.
TLet does not flatten nested lets/haves.
*/
struct TLet : public TExpr {
    std::string name;
    std::unique_ptr<TExpr> type;
    std::unique_ptr<TExpr> value;
    std::unique_ptr<TExpr> body;

    TLet(std::string name, std::unique_ptr<TExpr> type, std::unique_ptr<TExpr> value, std::unique_ptr<TExpr> body) : name(std::move(name)) {
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
    std::string to_latex() const noexcept override {
        // the first statement type in Latex IR
        // we need to be able to convert have hq : q := And.right h
        // to q (type) by And.right h (value)

        // for now, we just print the type and value
        std::string out;
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
        return out;
    }
};
/*
 * TIntro - A representation of an "intro" statement - simply intro attached to one or more TBinders
 */
struct TIntro : public TExpr {
    std::vector<std::unique_ptr<TBinder>> params;
    TIntro(std::vector<std::unique_ptr<TBinder>> params) : params(std::move(params)) {
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
    std::string to_latex() const noexcept override {
        // todo - full latex for intro statements
        std::string out = "Suppose that ";
        for (const auto& param : params) {
            if (param) {
                out += param->to_latex() + " ";
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

struct TGoal : public TExpr {
    std::vector<std::unique_ptr<TBinder>> params;
    std::vector<std::unique_ptr<TExpr>> statements;
    TGoal(std::vector<std::unique_ptr<TBinder>> params, std::vector<std::unique_ptr<TExpr>> statements) : params(std::move(params)), statements(std::move(statements)) {
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
    std::string to_latex() const noexcept override {
        // todo - full latex for goal statements
        std::string out = "To show that ";
        for (const auto& param : params) {
            if (param) {
                out += param->to_latex() + " ";
            } else {
                out += "(?nullptr) ";
            }
        }
        for (const auto& statement : statements) {
            if (statement) {
                out += statement->to_latex();
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
struct TApply : public TExpr {
    std::unique_ptr<TApp> fn;
    TApply(std::unique_ptr<TApp> fn) : fn(std::move(fn)) {
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
    std::string to_latex() const noexcept override {
        return fn->to_latex();
    }
};

/*
 * TExact statements are identical to TApply statements, but are semantically different
 * as they are intended to represent statements that end right on variables/constants
 */
struct TExact : public TExpr {
    std::unique_ptr<TApp> fn;
    TExact(std::unique_ptr<TApp> fn) : fn(std::move(fn)) {
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
    std::string to_latex() const noexcept override {
        return fn->to_latex();
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
    std::string to_latex() const noexcept override {
        return expr->to_latex();
    }
};
/* A theorem.
 * e.x. theorem ex_falso type False -> p apply False.elim
 */
struct TTheorem : public TExpr {
    std::string name;
    std::unique_ptr<TExpr> type;
    std::unique_ptr<TProof> proof;

    TTheorem(std::string name, std::unique_ptr<TExpr> type, std::unique_ptr<TProof> proof): name(std::move(name)), type(std::move(type)), proof(std::move(proof)) {
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