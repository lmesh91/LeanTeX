import Lean
import Lean.Elab.Command
import Lean.Elab.Term

namespace LeanTeX

-- This is pretty much placeholder right now
def exprToLaTeX (e : Lean.Expr) : Std.Format :=
  match e with
  | .bvar i => f!"bvar {i}"
  | .fvar i => f!"fvar {i.name}"
  | .mvar i => f!"mvar {i.name}"
  | .sort u => f!"{u}"
  | .const n _ => f!"{n}"
  | .app fn arg => f!"{fn} ({arg})"
  | .lam n t b _ => f!"{n} : {t} -> {b}"
  | .forallE n t b _ => f!"∀{n} : {t}, {b}"
  | .letE n t v b _ => f!"{n} : {t} := {v}; {b}"
  | .lit l => match l with
    | .natVal n => f!"{n}"
    | .strVal s => f!"\"{s}\""
  | .mdata _ _ => f!""
  | .proj _ i s => f!"{s}.{i}"

elab "#latex " termStx:term : command => do
  Lean.Elab.Command.liftTermElabM do
    let e ← Lean.Elab.Term.elabTerm termStx none
    let latex := exprToLaTeX e
    Lean.logInfo latex

end LeanTeX

#latex "Hello Lean!"
