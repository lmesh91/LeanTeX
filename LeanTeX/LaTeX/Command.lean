import Lean
import LeanTeX.LaTeX.Render

namespace LeanTeX
namespace LaTeX

syntax (name := latexStrCmd) "#latex_str " term : command

@[command_elab latexStrCmd] meta unsafe def elabLatexStr : Lean.Elab.Command.CommandElab
  | `(#latex_str%$tk $term) =>
      Lean.withoutModifyingEnv <| Lean.Elab.Command.runTermElabM fun _ =>
        Lean.Elab.Term.withDeclName `_latex_str do
        let e ← Lean.Elab.Term.elabTerm term none
        Lean.Elab.Term.synthesizeSyntheticMVarsNoPostponing
        Lean.withRef tk <| Lean.Meta.check e
        let e ← Lean.Elab.Term.levelMVarToParam (← Lean.instantiateMVars e)
        if e.isSyntheticSorry then
          return
        let rendered ← renderExprString e
        Lean.logInfoAt tk rendered
  | _ => Lean.Elab.throwUnsupportedSyntax

end LaTeX
end LeanTeX
