import Lean
import Lean.Elab.Command
import Lean.Elab.Term
import Lean.Meta


namespace LeanTeX

-- Helper functions for exprToLaTeX

-- Helper for isArrow
-- `b` is the de Brujin index of the binder from this arrow
def isArrowH (e : Lean.Expr) (b : Nat) : Bool :=
  match e with
  | .bvar idx => idx ≠ b
  | .app fn arg => isArrowH fn b ∧ isArrowH arg b
  | .lam _ type body _ => isArrowH type b ∧ isArrowH body (b+1)
  | .forallE _ type body _ => isArrowH type b ∧ isArrowH body (b+1)
  | .letE _ type value body _ => isArrowH type b ∧ isArrowH value b ∧ isArrowH body (b+1)
  | .mdata _ expr => isArrowH expr b
  | .proj _ _ struct => isArrowH struct b
  | _ => True

-- isArrow determines if a function is dependent or not
-- We use this implementation instead of Expr.isArrow so that it
-- works for nested binders, where there could be loose bound variables.
def isArrow (e : Lean.Expr) : Bool :=
  match e with
  | .forallE _ _ body _ => isArrowH body 0
  | _ => True

-- Wraps an expression in `\left` and `\right` parentheses.
def texBracket (l : String) (f : Std.Format) (r : String) : Std.Format :=
  Std.Format.bracket ("\\left" ++ l) f ("\\right" ++ r)

def texParen (f : Std.Format) : Std.Format :=
  texBracket "(" f ")"

-- Constants representing priorities of things handled by expressions
def lambdaPrec : Nat := 0
def arrowPrec : Nat := 5 -- Slightly higher due to ⟹ notation in LaTeX
def forAllPrec : Nat := 0
def letPrec : Nat := 0
def projPrec : Nat := 0

-- This is pretty much placeholder right now
partial def exprToLaTeX (e : Lean.Expr) (prec : Nat) (bs : List Lean.Name) : Lean.Elab.TermElabM Std.Format := do
  let me ← Lean.instantiateMVars e
  match me with
  -- Bound variable: Return name from bs
  | .bvar i => do
    if hi : i < bs.length then
      return f!"{bs[i]}"
    else -- Loose bound variable
      return f!"b_\{{i-bs.length}}"
    -- Free variable: Return user-facing name
  | .fvar i => do
    have decl : Lean.LocalDecl := ←i.getDecl
    return f!"{decl.userName}"
  -- Metavariable: Return an underscore
  | .mvar i => return f!"\\_"
  -- Sort: Currently specialize for Prop and Type
  | .sort u => do
    if me.isProp then
      return f!"\\mathbb\{P}" --ℙ
    else if me.isType0 then
      return f!"\\mathbb\{T}" --𝕋
    else
      return f!"\\mathbb\{S}_\{{u}}" --𝕊ᵤ
  -- Constant: Return name
  | .const n _ => return f!"\\mathrm\{{n}}"
  -- Function application
  -- Translates to "fn(arg₁, arg₂, ..., argₙ)"
  -- TODO: specialize for individual functions
  | .app _ _ => do
    let fn : Lean.Expr := me.getAppFn
    let args : Array Lean.Expr := me.getAppArgs
    let fargs : List Std.Format ← args.toList.mapM (exprToLaTeX · prec bs)
    return (←exprToLaTeX fn prec bs) ++ texParen (Std.Format.joinSep fargs f!", ")
  -- All basic connectives are left-associative
  -- by default and have low precedence
  -- Lambda: Use ↦ notation
  -- Translates to "n ∈ t ↦ b"
  | .lam n t b _ => do
    let lhs := f!"{n} \\in " ++ (←exprToLaTeX t (lambdaPrec+1) bs)
    let rhs ← exprToLaTeX b lambdaPrec (n :: bs)
    let out := lhs ++ f!" \\mapsto " ++ rhs
    if prec > lambdaPrec then
      return texParen out
    else
      return out
  -- ForAll: Use arrow or forall notation
  | .forallE n t b _ => do
    if isArrow me then
      -- Translates to "t ⟹ b"
      let lhs ← exprToLaTeX t (arrowPrec+1) bs
      let rhs ← exprToLaTeX b arrowPrec (n :: bs)
      let out := lhs ++ f!" \\implies " ++ rhs
      if prec > arrowPrec then
        return texParen out
      else
        return out
    else
      -- Translates to "∀n ∈ t, b"
      let lhs := f!"\\forall {n} \\in " ++ (←exprToLaTeX t (forAllPrec+1) bs)
      let rhs ← exprToLaTeX b forAllPrec (n :: bs)
      let out := lhs ++ f!", " ++ rhs
      if prec > forAllPrec then
        return texParen out
      else
        return out
  -- Let: Use := notation (placeholder)
  -- Translates to "n : t := v; b"
  | .letE n t v b _ => do
    let lhs := f!"{n} : " ++ (←exprToLaTeX t (letPrec+1) bs) ++ f!" := " ++ (←exprToLaTeX v (letPrec+1) bs)
    let rhs ← exprToLaTeX b letPrec (n :: bs)
    let out := lhs ++ f!"; " ++ rhs
    if prec > letPrec then
      return texParen out
    else
      return out
  -- Literals: Replace naturals with their values, surround strings in quotes
  | .lit l => match l with
    | .natVal n => return f!"{n}"
    | .strVal s => return f!"``{s}\""
  -- Metadata: Ignore
  | .mdata _ exp => return ←exprToLaTeX exp prec bs
  -- Projections: Use dot notation (placeholder)
  -- Translates to "s.i"
  | .proj _ i s => return (←exprToLaTeX s projPrec bs) ++ f!".{i}"

elab "#latex " termStx:term : command => do
  Lean.Elab.Command.liftTermElabM do
    let e ← Lean.Elab.Term.elabTerm termStx none
    let latex ← exprToLaTeX e 0 []
    Lean.logInfo latex

end LeanTeX
