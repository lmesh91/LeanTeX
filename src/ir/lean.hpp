// ir/lean.hpp - Lean IR representation
#pragma once
#include "utility/json.hpp"
#include "utility/misc.hpp"
#include <memory>
#include <string>
#include <vector>
#include <utility>
#include <unordered_set>
using json = nlohmann::json;

/*
LExpr is a base class for Lean IR expressions. 
It is very similar to Lean 4's Expr class, with some notable differences:
- All variables are ultimately solved (although other data from Lean's Expr is also included)
- Top-level declarations have their own expression types, despite not being a Lean Expr.
- Partial applications (for app, lam, forall) are converted into full applications [todo]
- Binders, which consist of a name and a type, are their own type of Expr.
*/
struct LExpr {
    // Non-owning pointer to parent expression to improve traversal of the tree
    // (children must not take ownership of their parents).
    LExpr *parent = nullptr;

    virtual ~LExpr() = default;

    // Converts the expression to JSON for debugging/output purposes
    virtual json to_json() const = 0;

    // Converts the type to a string in a way that
    // resembles Lean syntax for debugging/output purposes
    virtual std::string to_string() const noexcept = 0;
};

/*
LVar covers all types of variables in Lean IR:
- The original type is stored in the "type" field.
- Bound variables store their indices.
- Free variables and metavariables store their names.
- Solved variables store their names and are marked as solved.
*/
struct LVar : public LExpr {
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
        if (solved) {
            return name;
        }
        switch (type) {
            case Type::Bound:
                return "?b." + std::to_string(index);
            case Type::Free:
                return "?f." + name;
            case Type::Meta:
                return "?m." + name;
        }
        return "?unknown";
    }
};

/*
LLevel represents universe levels in Lean.
There are a few operations on levels, represented as different types with arguments.
- zero: Level 0
- succ: Level (arg1 + 1)
- max: Level max(arg1, arg2)
- imax: Level max(arg1, arg2), or 0 if arg2 is 0
- param/meta: Named universe levels
*/
struct LLevel {
    enum class Type {
        Zero,
        Succ,
        Max,
        IMax,
        Param,
        Meta
    };
    Type type;
    std::string name;
    std::unique_ptr<LLevel> arg1;
    std::unique_ptr<LLevel> arg2;

    // Constructors for different level types
    LLevel() : type(Type::Zero), name(""), arg1(nullptr), arg2(nullptr) {};
    LLevel(Type type, std::unique_ptr<LLevel> arg1) : type(type), name(""), arg1(std::move(arg1)), arg2(nullptr) {};
    LLevel(Type type, std::unique_ptr<LLevel> arg1, std::unique_ptr<LLevel> arg2) : type(type), name(""), arg1(std::move(arg1)), arg2(std::move(arg2)) {};
    LLevel(Type type, std::string name) : type(type), name(std::move(name)), arg1(nullptr), arg2(nullptr) {};
    
    // Gets the raw representation of the level from combining parts
    // Note: There are some slight shortcomings to this function, most notably
    // if max/imax are combined with named levels.
    std::pair<int, std::unordered_set<std::string>> raw() const {
        switch (type) {
            case Type::Zero:
                return {0, {}};
            case Type::Succ: {
                auto [lvl, names] = arg1->raw();
                return {lvl + 1, names};
            }
            case Type::Max: {
                auto [lvl1, names1] = arg1->raw();
                auto [lvl2, names2] = arg2->raw();
                lvl1 = std::max(lvl1, lvl2);
                names1.insert(names2.begin(), names2.end());
                return {lvl1, names1};
            }
            case Type::IMax: {
                auto [lvl1, names1] = arg1->raw();
                auto [lvl2, names2] = arg2->raw();
                if (lvl2 == 0 && names2.empty()) {
                    return {0, {}};
                }
                lvl1 = std::max(lvl1, lvl2);
                names1.insert(names2.begin(), names2.end());
                return {lvl1, names1};
            }
            case Type::Param:
            case Type::Meta:
                return {0, {name}};
        }
        return {0, {}};
    }

    json to_json() const {
        switch (type) {
            case Type::Zero:
                return json{{"kind", "zero"}};
            case Type::Succ:
                return json{{"kind", "succ"}, {"arg", arg1->to_json()}};
            case Type::Max:
                return json{{"kind", "max"}, {"arg1", arg1->to_json()}, {"arg2", arg2->to_json()}};
            case Type::IMax:
                return json{{"kind", "imax"}, {"arg1", arg1->to_json()}, {"arg2", arg2->to_json()}};
            case Type::Param:
                return json{{"kind", "param"}, {"name", name}};
            case Type::Meta:
                return json{{"kind", "mvar"}, {"name", name}};
        }
        return json{};
    }

    std::string to_string() const noexcept {
        auto [lvl, names] = raw();
        std::string out;
        if (names.empty()) {
            return std::to_string(lvl);
        } else {
            for (const auto& name : names) {
                out += name + "+";
            }
            if (lvl > 0) {
                out += std::to_string(lvl);
            } else {
                // remove trailing +
                if (!out.empty() && out.back() == '+') {
                    out.pop_back();
                }
            }
            return out;
        }
    }
};

/*
LSort represents type levels in Lean, which are described by universe levels.
*/
struct LSort : public LExpr {
    std::unique_ptr<LLevel> level;

    LSort(std::unique_ptr<LLevel> level) : level(std::move(level)) {};

    json to_json() const override {
        if (!level) {
            throw std::runtime_error("LSort missing level");
        }
        return json{{"kind", "sort"}, {"level", level->to_json()}};
    }

    std::string to_string() const noexcept override {
        if (!level) {
            return "Sort ?nullptr";
        }
        std::string lv_str = level->to_string();
        // Special cases
        if (lv_str == "0") {
            return "Prop";
        } else if (lv_str == "1") {
            return "Type";
        }
        return "Sort (" + level->to_string() + ")";
    }
};

/*
LConst represents constants in Lean.
This is separated from LVar as the difference between variables and constants
are crucial for translation into natural language.
*/
struct LConst : public LExpr {
    std::string name;
    std::vector<std::unique_ptr<LLevel>> levels;

    LConst(std::string name, std::vector<std::unique_ptr<LLevel>> levels) : name(std::move(name)), levels(std::move(levels)) {};

    json to_json() const override {
        json jlevels = json::array();
        for (const auto& level : levels) {
            if (!level) {
                throw std::runtime_error("LConst " + name + " has null level");
            }
            jlevels.push_back(level->to_json());
        }
        return json{{"kind", "const"}, {"name", name}, {"levels", jlevels}};
    }

    // We use the most explicit string representation for constants,
    // e.g. @Array.map.{0, 0} instead of Array.map
    std::string to_string() const noexcept override {
        std::string out = "@" + name;
        if (!levels.empty()) {
            out += ".{";
            for (size_t i = 0; i < levels.size(); ++i) {
                if (i > 0) {
                    out += ", ";
                }
                if (levels[i]) {
                    out += levels[i]->to_string();
                } else {
                    out += "?nullptr";
                }
            }
            out += "}";
        }
        return out;
    }
};

/*
LApp represents function applications in Lean.
Unlike in Lean Expr, nested applications are flattened.
*/
struct LApp : public LExpr {
    std::unique_ptr<LExpr> fn;
    std::vector<std::unique_ptr<LExpr>> args;

    LApp(std::unique_ptr<LExpr> fn, std::unique_ptr<LExpr> arg) {
        // Nest LApp functions to flatten applications
        std::unique_ptr<LApp> fn_app = fn == nullptr ? nullptr : downcast_unique<LApp>(fn);
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
};

/*
LBinder represents binder information in Lean, as used by LLambda and LForAll.
This includes a name and a type, as well as what sort of binder it is.
*/
struct LBinder : public LExpr {
    // there is no better name for this than "Info"
    // since "type" and "kind" are both taken in the JSON representation
    enum class Info { 
        Explicit,
        Implicit,
        StrictImplicit,
        InstImplicit
    };
    std::string name;
    std::unique_ptr<LExpr> type;
    Info info;
    LBinder(std::string name, std::unique_ptr<LExpr> type, Info info) : name(std::move(name)), info(info) {
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
        // treat it as inaccessible and replace with the user-friendly version "x✝.7".
        // Note that Lean 4 uses superscript numbers instead of the dot.
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
                _name = _name.substr(0, _name.find('.')) + "✝";
                if (pos != name.size() - 2 || _name[name.size() - 1] != '0') {
                    _name += name.substr(pos);
                }
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
                out += "{{" + name + " : ";
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
};


/*
LLambda represents lambda abstractions in Lean, which are done using the `fun` keyword.
Unlike in Lean Expr, nested applications are flattened.
*/
struct LLambda : public LExpr {
    std::vector<std::unique_ptr<LBinder>> binders;
    std::unique_ptr<LExpr> body;

    LLambda(std::unique_ptr<LBinder> binder, std::unique_ptr<LExpr> body) {
        std::unique_ptr<LLambda> body_lm = body == nullptr ? nullptr : downcast_unique<LLambda>(body);
        if (body_lm) {
            this->body = std::move(body_lm->body);
            this->binders.push_back(std::move(binder));
            // Place the incoming binders before the binders from the nested lambda
            for (auto& b : body_lm->binders) {
                this->binders.push_back(std::move(b));
            }
        } else {
            this->body = std::move(body);
            this->binders.push_back(std::move(binder));
        }
        if (this->body) {
            this->body->parent = this;
        }
        for (const auto& binder : this->binders) {
            if (binder) {
                binder->parent = this;
            }
        }
    };

    json to_json() const override {
        if (!body) {
            throw std::runtime_error("LLambda missing body");
        }
        json jbinders = json::array();
        for (const auto& binder : binders) {
            if (!binder) {
                throw std::runtime_error("LLambda has null binder");
            }
            jbinders.push_back(binder->to_json());
        }
        return json{{"kind", "lam"}, {"binders", jbinders}, {"body", body->to_json()}};
    }

    std::string to_string() const noexcept override {
        std::string out;
        out += "fun ";
        for (const auto& binder : binders) {
            out += binder ? binder->to_string() + " " : "(?nullptr) ";
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

/*
LForAll represents (possibly-dependent) arrows in Lean.
Unlike in Lean Expr, nested applications are flattened.
Note: The structure is identical to LLambda, but they are semantically different.
*/
struct LForAll : public LExpr {
    std::vector<std::unique_ptr<LBinder>> binders;
    std::unique_ptr<LExpr> body;

    LForAll(std::unique_ptr<LBinder> binder, std::unique_ptr<LExpr> body) {
        std::unique_ptr<LForAll> body_fa = body == nullptr ? nullptr : downcast_unique<LForAll>(body);
        if (body_fa) {
            this->body = std::move(body_fa->body);
            // Place the incoming binders before the binders from the nested forall
            this->binders.push_back(std::move(binder));
            for (auto& binder : body_fa->binders) {
                this->binders.push_back(std::move(binder));
            }
        } else {
            this->body = std::move(body);
            this->binders.push_back(std::move(binder));
        }
        // Set parent pointers (non-owning)
        if (this->body) {
            this->body->parent = this;
        };
        for (auto& binder : this->binders) {
            if (binder) {
                binder->parent = this;
            };
        }
    };

    json to_json() const override {
        if (!body) {
            throw std::runtime_error("LForAll missing body");
        }
        json jbinders = json::array();
        for (const auto& binder : binders) {
            if (!binder) {
                throw std::runtime_error("LForAll has null binder");
            }
            jbinders.push_back(binder->to_json());
        }
        return json{{"kind", "forall"}, {"binders", jbinders}, {"body", body->to_json()}};
    }

    // Arrow notation is used for LForAll
    std::string to_string() const noexcept override {
        std::string out;
        for (const auto& binder : binders) {
            out += binder ? binder->to_string() + " -> " : "(?nullptr) -> ";
        }
        if (body) {
            out += "(" + body->to_string() + ")";
        } else {
            out += "(?nullptr)";
        }
        return out;
    }
};

/*
LLet represents `let` and `have` expressions in Lean. The only difference is that
`let` expressions are only type-correct if the value is known.
Unlike LForAll anad LLambda, LLet does not flatten nested lets/haves.
*/
struct LLet : public LExpr {
    std::string name;
    std::unique_ptr<LExpr> type;
    std::unique_ptr<LExpr> value;
    std::unique_ptr<LExpr> body;
    bool nondep; // true for `have`, false for `let`; only used by to_string

    LLet(std::string name, std::unique_ptr<LExpr> type, std::unique_ptr<LExpr> value, std::unique_ptr<LExpr> body, bool nondep) : name(std::move(name)), nondep(nondep) {
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
        out += nondep ? "have " : "let ";
        out += name + " : ";
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
LLiteral represents literals (strings or natural numbers) in Lean.
*/
struct LLiteral : public LExpr {
    enum class Type {
        String,
        Nat
    };
    Type type;
    std::string str_value;
    unsigned int nat_value;

    // String literal constructor
    LLiteral(std::string value) : type(Type::String), str_value(std::move(value)), nat_value(0) {};
    // Natural number literal constructor
    LLiteral(unsigned int value) : type(Type::Nat), str_value(""), nat_value(value) {};

    json to_json() const override {
        switch (type) {
            case Type::String:
                return json{{"kind", "literal"}, {"type", "string"}, {"value", str_value}};
            case Type::Nat:
                return json{{"kind", "literal"}, {"type", "nat"}, {"value", nat_value}};
        }
        return json{};
    }

    std::string to_string() const noexcept override {
        switch (type) {
            case Type::String:
                return "\"" + str_value + "\"";
            case Type::Nat:
                return std::to_string(nat_value);
        }
        return "?unknown";
    }
};

/*
LProj represents projections in Lean, which are used as accessors for some structures with
one constructor, e.g. Prod, propositional logic.
For instance, proj Prod 0 a is the same as a.1, where a is a Prod.
The name "structE" is used to avoid a conflict with `struct`.
*/
struct LProj : LExpr {
    std::string name;
    unsigned int idx;
    std::unique_ptr<LExpr> structE;

    LProj(std::string name, unsigned int idx, std::unique_ptr<LExpr> structE) : name(std::move(name)), idx(idx) {
        // Set pointers
        if (structE) {
            this->structE = std::move(structE);
            this->structE->parent = this;
        }
    };

    json to_json() const override {
        if (!structE) {
            throw std::runtime_error("LProj missing struct");
        }
        return json{{"kind", "proj"}, {"name", name}, {"idx", idx}, {"struct", structE->to_json()}};
    }

    std::string to_string() const noexcept override {
        std::string out;
        if (structE) {
            out += "(" + structE->to_string() + ")." + std::to_string(idx + 1);
        } else {
            out += "(?nullptr)." + std::to_string(idx + 1);
        }
        return out;
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

    LTermProof(std::unique_ptr<LExpr> expr) : expr(std::move(expr)) {
        if (expr) {
            expr->parent = this;
        }
    };

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

    LTheorem(std::string name, std::vector<std::unique_ptr<LBinder>> params, std::unique_ptr<LExpr> type, std::unique_ptr<LProof> proof) : name(std::move(name)), params(std::move(params)), type(std::move(type)), proof(std::move(proof)) {
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
    };

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
        out += " := ";
        if (proof) {
            out += proof->to_string();
        } else {
            out += "?nullptr";
        }
        return out;
    }
};