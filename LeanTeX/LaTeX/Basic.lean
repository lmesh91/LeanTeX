namespace LeanTeX
namespace LaTeX

-- Precedence level for operators, used to determine when to add parentheses.
abbrev Prec := Nat

-- A tight precedence level that forces parentheses around sub-expressions.
def tightPrec : Prec := 1024

/--
`Assoc` represents the associativity of an operator, which affects how parentheses are added when rendering expressions.
- `left`: Left-associative operators (e.g., addition, multiplication).
- `right`: Right-associative operators (e.g., exponentiation).
- `none`: Non-associative operators (e.g., comparison operators).
-/
inductive Assoc where
  | left
  | right
  | none
  deriving Repr, DecidableEq, Inhabited

/--
`Doc` represents a LaTeX document fragment based on atoms and their concatenation.
- `atom`: Represents a basic string fragment.
- `seq`: Represents a sequence of `Doc` fragments with an associated precedence level.
-/
inductive Doc where
  | atom (value : String)
  | seq (prec : Prec) (parts : Array Doc)
  deriving Repr, Inhabited

namespace Doc

-- Returns the precedence level of a `Doc`. For an atom, it returns `tightPrec`, and for a sequence, it returns the stored precedence.
def prec : Doc → Prec
  | .atom _ => tightPrec
  | .seq prec _ => prec

-- Constructs an empty `Doc` with no content.
def empty : Doc :=
  .seq tightPrec #[]

-- Constructs a `Doc` from a string by creating an atom.
def text (value : String) : Doc :=
  .atom value

-- Concatenates an array of `Doc` fragments into a single `Doc` with the specified precedence. By default, it uses `tightPrec`.
def concat (parts : Array Doc) (prec : Prec := tightPrec) : Doc :=
  .seq prec parts

-- Concatenates two `Doc` fragments into a single `Doc`.
def append (lhs rhs : Doc) : Doc :=
  .seq tightPrec #[lhs, rhs]

-- Creates a `Doc` representing a single space character.
def space : Doc :=
  .atom " "

-- Creates a `Doc` representing a delimiter by concatenating the left and right strings around the given `Doc`.
def delimited (left : String) (right : String) (doc : Doc) : Doc :=
  concat #[.atom left, doc, .atom right]

-- Creates a `Doc` representing a parenthesized expression.
def parens (doc : Doc) : Doc :=
  delimited "\\left(" "\\right)" doc

-- Creates a `Doc` representing a bracketed expression.
def brackets (doc : Doc) : Doc :=
  delimited "\\left[" "\\right]" doc

-- Creates a `Doc` representing a brace-enclosed expression.
def braces (doc : Doc) : Doc :=
  delimited "{" "}" doc

-- Creates a `Doc` representing a LaTeX command with the given name and optional arguments. The command is formatted as `\name{arg1}{arg2}...`.
def cmd (name : String) (args : Array Doc := #[]) : Doc :=
  concat <| #[.atom ("\\" ++ name)] ++ args.map braces

-- Protects a `Doc` with parentheses if the current context precedence is greater than the `Doc`'s precedence. This ensures that the rendered output maintains the correct order of operations.
def protectAt (ctxPrec : Prec) (doc : Doc) : Doc :=
  if ctxPrec > doc.prec then
    parens doc
  else
    doc

-- Creates a `Doc` representing a prefix operator applied to an argument, ensuring that parentheses are added as needed based on the precedence levels.
def prefixOp (prec : Prec) (op : Doc) (arg : Doc) : Doc :=
  concat #[protectAt prec op, space, protectAt prec arg] prec

-- Creates a `Doc` representing a postfix operator applied to an argument, ensuring that parentheses are added as needed based on the precedence levels.
def postfixOp (prec : Prec) (arg : Doc) (op : Doc) : Doc :=
  concat #[protectAt prec arg, protectAt prec op] prec

-- Creates a `Doc` representing an infix operator applied to two arguments, ensuring that parentheses are added as needed based on the precedence levels and associativity of the operator.
def infixOp (prec : Prec) (assoc : Assoc) (lhs : Doc) (op : Doc) (rhs : Doc) : Doc :=
  let lhsCtx :=
    match assoc with
    | .right => prec + 1
    | _ => prec
  let rhsCtx :=
    match assoc with
    | .left => prec + 1
    | _ => prec
  concat #[protectAt lhsCtx lhs, space, protectAt 0 op, space, protectAt rhsCtx rhs] prec

-- Convenience functions for common operator associativities.

def infixLeft (prec : Prec) (lhs : Doc) (op : Doc) (rhs : Doc) : Doc :=
  infixOp prec .left lhs op rhs

def infixRight (prec : Prec) (lhs : Doc) (op : Doc) (rhs : Doc) : Doc :=
  infixOp prec .right lhs op rhs

def infixN (prec : Prec) (lhs : Doc) (op : Doc) (rhs : Doc) : Doc :=
  infixOp prec .none lhs op rhs

-- Creates a `Doc` representing a superscript (exponentiation) of a base, ensuring that parentheses are added as needed based on the precedence levels.
def superscript (base : Doc) (exp : Doc) : Doc :=
  concat #[protectAt tightPrec base, text "^{", protectAt 0 exp, text "}"] tightPrec

-- Creates a `Doc` representing a subscript of a base, ensuring that parentheses are added as needed based on the precedence levels.
def subscript (base : Doc) (sub : Doc) : Doc :=
  concat #[protectAt tightPrec base, text "_{", protectAt 0 sub, text "}"] tightPrec

-- Renders a `Doc` to a `String` by recursively concatenating the string representations of its parts.
def render : Doc → String
  | .atom value => value
  | .seq _ parts =>
      parts.foldl (init := "") fun acc part =>
        acc ++ render part

instance : ToString Doc where
  toString := render

end Doc
end LaTeX
end LeanTeX
