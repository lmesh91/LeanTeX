namespace LeanTeX
namespace LaTeX

abbrev Prec := Nat

def tightPrec : Prec := 1024

inductive Assoc where
  | left
  | right
  | none
  deriving Repr, DecidableEq, Inhabited

/--
`Doc` represents a LaTeX document fragment based on atoms and their concatenation.
-/
inductive Doc where
  | atom (value : String)
  | seq (prec : Prec) (parts : Array Doc)
  deriving Repr, Inhabited

namespace Doc

def prec : Doc -> Prec
  | .atom _ => tightPrec
  | .seq prec _ => prec

def empty : Doc :=
  .seq tightPrec #[]

def text (value : String) : Doc :=
  .atom value

def concat (parts : Array Doc) (prec : Prec := tightPrec) : Doc :=
  .seq prec parts

def append (lhs rhs : Doc) : Doc :=
  .seq tightPrec #[lhs, rhs]

def space : Doc :=
  .atom " "

def delimited (left : String) (right : String) (doc : Doc) : Doc :=
  concat #[.atom left, doc, .atom right]

def parens (doc : Doc) : Doc :=
  delimited "\\left(" "\\right)" doc

def brackets (doc : Doc) : Doc :=
  delimited "\\left[" "\\right]" doc

def braces (doc : Doc) : Doc :=
  delimited "{" "}" doc

def cmd (name : String) (args : Array Doc := #[]) : Doc :=
  concat <| #[.atom ("\\" ++ name)] ++ args.map braces

def protectAt (ctxPrec : Prec) (doc : Doc) : Doc :=
  if ctxPrec > doc.prec then
    parens doc
  else
    doc

def prefixOp (prec : Prec) (op : Doc) (arg : Doc) : Doc :=
  concat #[protectAt prec op, space, protectAt (prec + 1) arg] prec

def postfixOp (prec : Prec) (arg : Doc) (op : Doc) : Doc :=
  concat #[protectAt (prec + 1) arg, protectAt prec op] prec

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

def infixLeft (prec : Prec) (lhs : Doc) (op : Doc) (rhs : Doc) : Doc :=
  infixOp prec .left lhs op rhs

def infixRight (prec : Prec) (lhs : Doc) (op : Doc) (rhs : Doc) : Doc :=
  infixOp prec .right lhs op rhs

def infixN (prec : Prec) (lhs : Doc) (op : Doc) (rhs : Doc) : Doc :=
  infixOp prec .none lhs op rhs

def superscript (base : Doc) (exp : Doc) : Doc :=
  concat #[protectAt tightPrec base, text "^{", protectAt 0 exp, text "}"] tightPrec

def subscript (base : Doc) (sub : Doc) : Doc :=
  concat #[protectAt tightPrec base, text "_{", protectAt 0 sub, text "}"] tightPrec

def render : Doc -> String
  | .atom value => value
  | .seq _ parts =>
      parts.foldl (init := "") fun acc part =>
        acc ++ render part

instance : ToString Doc where
  toString := render

end Doc
end LaTeX
end LeanTeX
