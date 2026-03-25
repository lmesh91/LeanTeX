import Lean
import LeanTeX.LaTeX.Render
import LeanTeX.LaTeX.Widget

namespace LeanTeX
namespace LaTeX

open Lean

syntax (name := latexStrCmd) "#latex_str " term : command
syntax (name := latexCmd) "#latex " term : command
syntax (name := guardLatexStrCmd) "#guard_latex_str " term " => " str : command

private def elabLatexTerm (declName : Lean.Name) (term : Lean.Syntax) : Lean.Elab.Command.CommandElabM String :=
  Lean.withoutModifyingEnv <| Lean.Elab.Command.runTermElabM fun _ =>
    Lean.Elab.Term.withDeclName declName do
      let e ← Lean.Elab.Term.elabTerm term none
      Lean.Elab.Term.synthesizeSyntheticMVarsNoPostponing
      Lean.Meta.check e
      let e ← Lean.Elab.Term.levelMVarToParam (← Lean.instantiateMVars e)
      if e.isSyntheticSorry then
        return ""
      renderExprString e

private def mkLatexWidgetInstance (latex : String) : Lean.Widget.WidgetInstance :=
  let props : LatexWidgetProps := { latex }
  { id := ``latexWidget
    javascriptHash := latexWidget.javascriptHash
    props := Lean.Server.RpcEncodable.rpcEncode props }

@[command_elab latexStrCmd] meta unsafe def elabLatexStr : Lean.Elab.Command.CommandElab
  | `(#latex_str%$tk $term) => do
      let rendered ← Lean.withRef tk <| elabLatexTerm `_latex_str term
      unless rendered.isEmpty do
        Lean.logInfoAt tk rendered
  | _ => Lean.Elab.throwUnsupportedSyntax

@[command_elab latexCmd] meta unsafe def elabLatex : Lean.Elab.Command.CommandElab
  | `(#latex%$tk $term) => do
      let rendered ← Lean.withRef tk <| elabLatexTerm `_latex term
      unless rendered.isEmpty do
        let wi := mkLatexWidgetInstance rendered
        Lean.logInfoAt tk <| Lean.MessageData.ofWidget wi m!"{rendered}"
  | _ => Lean.Elab.throwUnsupportedSyntax

@[command_elab guardLatexStrCmd] meta unsafe def elabGuardLatexStr : Lean.Elab.Command.CommandElab
  | `(#guard_latex_str%$tk $term => $expected:str) => do
      let rendered ← Lean.withRef tk <| elabLatexTerm `_guard_latex_str term
      let expected := expected.getString
      unless rendered == expected do
        throwErrorAt tk "unexpected LaTeX output\nexpected: {expected}\nactual:   {rendered}"
  | _ => Lean.Elab.throwUnsupportedSyntax

end LaTeX
end LeanTeX
