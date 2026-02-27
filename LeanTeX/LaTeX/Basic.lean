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

-- This is pretty much placeholder right now
partial def exprToLaTeX (e : Lean.Expr) (bs : List Lean.Name) : Lean.Elab.TermElabM Std.Format := do
  let me ← Lean.instantiateMVars e
  match me with
  -- Bound variable: Return name from bs
  | .bvar i => do
  if hi : i < bs.length then
    return f!"{bs[i]}"
  else
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
  | .const n _ => return f!"{n}"
  -- Function application
  -- Translates to "fn(arg₁, arg₂, ..., argₙ)"
  -- TODO: specialize for individual functions
  | .app _ _ => do
    let fn : Lean.Expr := me.getAppFn
    let args : Array Lean.Expr := me.getAppArgs
    let fargs : List Std.Format ← args.toList.mapM (exprToLaTeX · bs)
    return f!"\\mathrm\{" ++ (←exprToLaTeX fn bs) ++ f!"}(" ++ Std.Format.joinSep fargs f!", " ++ f!")"
  -- Lambda: Use ↦ notation
  -- Translates to "n ∈ t ↦ b"
  | .lam n t b _ => return f!"{n} \\in " ++ (←exprToLaTeX t bs) ++ f!" \\mapsto " ++ (←exprToLaTeX b (n :: bs))
  -- ForAll: Use arrow or forall notation
  | .forallE n t b _ => do
    if isArrow me then
      -- Translates to "t ⇒ b"
      return (←exprToLaTeX t bs) ++ f!" \\implies " ++ (←exprToLaTeX b (n :: bs))
    else
      -- Translates to "∀n ∈ t, b"
      return f!"\\forall {n} \\in " ++ (←exprToLaTeX t bs) ++ f!", " ++ (←exprToLaTeX b (n :: bs))
  -- Let: Use := notation (placeholder)
  -- Translates to "n : t := v; b"
  | .letE n t v b _ => return f!"{n} : " ++ (←exprToLaTeX t bs) ++ f!" := "
                              ++ (←exprToLaTeX v bs) ++ f!"; " ++ (←exprToLaTeX b (n :: bs))
  -- Literals: Replace naturals with their values, surround strings in quotes
  | .lit l => match l with
    | .natVal n => return f!"{n}"
    | .strVal s => return f!"``{s}\""
  -- Metadata: Ignore
  | .mdata _ exp => return ←exprToLaTeX exp bs
  -- Projections: Use dot notation (placeholder)
  -- Translates to "s.i"
  | .proj _ i s => return (←exprToLaTeX s bs) ++ f!".{i}"

elab "#latex " termStx:term : command => do
  Lean.Elab.Command.liftTermElabM do
    let e ← Lean.Elab.Term.elabTerm termStx none
    let latex ← exprToLaTeX e []
    Lean.logInfo latex

end LeanTeX
