import Lean
import LeanTeX.LaTeX.Notation

namespace LeanTeX
namespace LaTeX

open Lean Parser

/-
Here, we define syntax for LaTeX specification attributes,
and elaboration code to turn them into `NotationSpec`s
that can be registered in the environment.

These are used in the `latex` attribute, which is the main
user-facing entry point for specifying LaTeX renderings of declarations. '

Here are the supported specifications:
- `const <string>`: specifies a constant rendering.
- `prefix:prec <string>`: specifies a prefix operator
- `postfix:prec <string>`: specifies a postfix operator
- `infix:prec <string>`: specifies a non-associative infix operator
- `infixl:prec <string>`: specifies a left-associative infix operator
- `infixr:prec <string>`: specifies a right-associative infix
- `command <string>`: specifies a command-like rendering with no arguments
- `template:[arity:]prec <template parts>`: specifies a rendering using a template, where the arity is optionally inferred from the template parts

The template parts can be:
- string literals, which are rendered verbatim
- named arguments, which refer to explicit binders in the declaration by name
- indexed arguments, which refer to explicit binders by their position in the declaration (starting at 1)
- optional precedence overrides for either kind of argument, written as `x:51` or `#2:51`

As an example, the following attributes are equivalent:
```lean
def foo (x : Nat) (y : Nat) : Nat := x + y
@attribute [latex template:50 x "+" y:51] foo
@attribute [latex template:2:50 #1 "+" #2:51] foo
```
-/

declare_syntax_cat latexTemplatePart
declare_syntax_cat latexSpec

syntax (name := latexTemplateTextPart) str : latexTemplatePart
syntax (name := latexTemplateNamedArgPart) ident : latexTemplatePart
syntax (name := latexTemplateIndexedArgPart) "#" num : latexTemplatePart
syntax (name := latexTemplateNamedArgWithPrecPart) ident ":" num : latexTemplatePart
syntax (name := latexTemplateIndexedArgWithPrecPart) "#" num ":" num : latexTemplatePart

syntax (name := latexConstSpec) "const " str : latexSpec
syntax (name := latexPrefixSpec) "prefix:" num str : latexSpec
syntax (name := latexPostfixSpec) "postfix:" num str : latexSpec
syntax (name := latexInfixSpec) "infix:" num str : latexSpec
syntax (name := latexInfixLeftSpec) "infixl:" num str : latexSpec
syntax (name := latexInfixRightSpec) "infixr:" num str : latexSpec
syntax (name := latexCommandSpec) "command " str : latexSpec
syntax (name := latexTemplateInferSpec) "template:" num (ppSpace latexTemplatePart)+ : latexSpec
syntax (name := latexTemplateExplicitSpec) "template:" num ":" num (ppSpace latexTemplatePart)+ : latexSpec

private def atomDoc (txt : Lean.TSyntax `str) : Doc :=
  Doc.atom txt.getString

-- Helper functions for inferring template arity and validating template parts against the arity.

private def getExplicitBinderIndices (declName : Lean.Name) : Lean.CoreM (Lean.NameMap Nat) := do
  let info ← Lean.getConstInfo declName
  let rec loop (type : Lean.Expr) (nextIndex : Nat) (acc : Lean.NameMap Nat) : Lean.NameMap Nat :=
    match type with
    | .forallE binderName _ body binderInfo =>
        let acc :=
          if binderInfo.isExplicit && !binderName.isAnonymous && !acc.contains binderName then
            acc.insert binderName nextIndex
          else
            acc
        let nextIndex := if binderInfo.isExplicit then nextIndex + 1 else nextIndex
        loop body nextIndex acc
    | _ =>
        acc
  pure <| loop info.type 0 {}

private def getTemplateParts (stx : Lean.Syntax) (argPos : Nat) : Array Lean.Syntax :=
  (Lean.Syntax.getArg stx argPos).getArgs

private def validateTemplateArity (arity : Nat) (parts : Array TemplatePart) : Except String Unit := do
  for part in parts do
    match part with
    | .text _ => pure ()
    | .arg index _ =>
        if index >= arity then
          throw s!"template argument #{index + 1} exceeds arity {arity}"

private def inferTemplateArity (parts : Array TemplatePart) : Nat :=
  parts.foldl (init := 0) fun arity part =>
    match part with
    | .text _ => arity
    | .arg index _ => max arity (index + 1)

private def decodeTemplateArgIndex (binderIndices : Lean.NameMap Nat) (stx : Lean.Syntax) :
    Except String Nat := do
  if Lean.Syntax.getKind stx == `LeanTeX.LaTeX.latexTemplateNamedArgPart ||
      Lean.Syntax.getKind stx == `LeanTeX.LaTeX.latexTemplateNamedArgWithPrecPart then
    let binderName := (⟨Lean.Syntax.getArg stx 0⟩ : Lean.TSyntax `ident).getId.eraseMacroScopes
    match binderIndices.find? binderName with
    | some index => pure index
    | none => throw s!"unknown explicit parameter `{binderName}` in LaTeX template"
  else if Lean.Syntax.getKind stx == `LeanTeX.LaTeX.latexTemplateIndexedArgPart ||
      Lean.Syntax.getKind stx == `LeanTeX.LaTeX.latexTemplateIndexedArgWithPrecPart then
    let humanIndex := (⟨Lean.Syntax.getArg stx 1⟩ : Lean.TSyntax `num).getNat
    if humanIndex == 0 then
      throw "template argument indices start at #1"
    pure (humanIndex - 1)
  else
    throw "invalid LaTeX template argument"

private def decodeTemplateArgPrec? (stx : Lean.Syntax) : Option Prec :=
  if Lean.Syntax.getKind stx == `LeanTeX.LaTeX.latexTemplateNamedArgWithPrecPart then
    some (⟨Lean.Syntax.getArg stx 2⟩ : Lean.TSyntax `num).getNat
  else if Lean.Syntax.getKind stx == `LeanTeX.LaTeX.latexTemplateIndexedArgWithPrecPart then
    some (⟨Lean.Syntax.getArg stx 3⟩ : Lean.TSyntax `num).getNat
  else
    none

private def decodeLatexTemplatePart (binderIndices : Lean.NameMap Nat) (stx : Lean.Syntax) :
    Except String TemplatePart := do
  if Lean.Syntax.getKind stx == `LeanTeX.LaTeX.latexTemplateTextPart then
    pure <| .text (atomDoc ⟨Lean.Syntax.getArg stx 0⟩)
  else if Lean.Syntax.getKind stx == `LeanTeX.LaTeX.latexTemplateNamedArgPart ||
      Lean.Syntax.getKind stx == `LeanTeX.LaTeX.latexTemplateIndexedArgPart ||
      Lean.Syntax.getKind stx == `LeanTeX.LaTeX.latexTemplateNamedArgWithPrecPart ||
      Lean.Syntax.getKind stx == `LeanTeX.LaTeX.latexTemplateIndexedArgWithPrecPart then
    let index ← decodeTemplateArgIndex binderIndices stx
    pure <| .arg index (decodeTemplateArgPrec? stx)
  else
    throw "invalid LaTeX template part"

private def decodeLatexTemplateParts (binderIndices : Lean.NameMap Nat) (parts : Array Lean.Syntax) :
    Except String (Array TemplatePart) :=
  parts.mapM (decodeLatexTemplatePart binderIndices)

private def isLatexSpecKind (kind : Lean.Name) : Bool :=
  kind == `LeanTeX.LaTeX.latexConstSpec ||
  kind == `LeanTeX.LaTeX.latexPrefixSpec ||
  kind == `LeanTeX.LaTeX.latexPostfixSpec ||
  kind == `LeanTeX.LaTeX.latexInfixSpec ||
  kind == `LeanTeX.LaTeX.latexInfixLeftSpec ||
  kind == `LeanTeX.LaTeX.latexInfixRightSpec ||
  kind == `LeanTeX.LaTeX.latexCommandSpec ||
  kind == `LeanTeX.LaTeX.latexTemplateInferSpec ||
  kind == `LeanTeX.LaTeX.latexTemplateExplicitSpec

private partial def findLatexSpec? (stx : Lean.Syntax) : Option Lean.Syntax :=
  if isLatexSpecKind (Lean.Syntax.getKind stx) then
    some stx
  else
    match stx with
    | Lean.Syntax.node _ _ args =>
        args.findSome? findLatexSpec?
    | _ =>
        none

private def elabNotationSpec (declName : Lean.Name) (stx : Lean.Syntax) : Lean.CoreM (Except String NotationSpec) := do
  if Lean.Syntax.getKind stx == `LeanTeX.LaTeX.latexConstSpec then
    return pure <| .const (atomDoc ⟨Lean.Syntax.getArg stx 1⟩)
  else if Lean.Syntax.getKind stx == `LeanTeX.LaTeX.latexPrefixSpec then
    return pure <| .prefix (⟨Lean.Syntax.getArg stx 1⟩ : Lean.TSyntax `num).getNat (atomDoc ⟨Lean.Syntax.getArg stx 2⟩)
  else if Lean.Syntax.getKind stx == `LeanTeX.LaTeX.latexPostfixSpec then
    return pure <| .postfix (⟨Lean.Syntax.getArg stx 1⟩ : Lean.TSyntax `num).getNat (atomDoc ⟨Lean.Syntax.getArg stx 2⟩)
  else if Lean.Syntax.getKind stx == `LeanTeX.LaTeX.latexInfixSpec then
    return pure <| .infix (⟨Lean.Syntax.getArg stx 1⟩ : Lean.TSyntax `num).getNat .none (atomDoc ⟨Lean.Syntax.getArg stx 2⟩)
  else if Lean.Syntax.getKind stx == `LeanTeX.LaTeX.latexInfixLeftSpec then
    return pure <| .infix (⟨Lean.Syntax.getArg stx 1⟩ : Lean.TSyntax `num).getNat .left (atomDoc ⟨Lean.Syntax.getArg stx 2⟩)
  else if Lean.Syntax.getKind stx == `LeanTeX.LaTeX.latexInfixRightSpec then
    return pure <| .infix (⟨Lean.Syntax.getArg stx 1⟩ : Lean.TSyntax `num).getNat .right (atomDoc ⟨Lean.Syntax.getArg stx 2⟩)
  else if Lean.Syntax.getKind stx == `LeanTeX.LaTeX.latexCommandSpec then
    return pure <| .command (⟨Lean.Syntax.getArg stx 1⟩ : Lean.TSyntax `str).getString
  else if Lean.Syntax.getKind stx == `LeanTeX.LaTeX.latexTemplateInferSpec then
    let prec : Lean.TSyntax `num := ⟨Lean.Syntax.getArg stx 1⟩
    let binderIndices ← getExplicitBinderIndices declName
    let parts := getTemplateParts stx 2
    let parts ←
      match decodeLatexTemplateParts binderIndices parts with
      | .ok parts => pure parts
      | .error msg => return .error msg
    let arity := inferTemplateArity parts
    return do
      validateTemplateArity arity parts
      pure <| .template arity prec.getNat parts
  else if Lean.Syntax.getKind stx == `LeanTeX.LaTeX.latexTemplateExplicitSpec then
    let arity : Lean.TSyntax `num := ⟨Lean.Syntax.getArg stx 1⟩
    let prec : Lean.TSyntax `num := ⟨Lean.Syntax.getArg stx 3⟩
    let binderIndices ← getExplicitBinderIndices declName
    let parts := getTemplateParts stx 4
    let parts ←
      match decodeLatexTemplateParts binderIndices parts with
      | .ok parts => pure parts
      | .error msg => return .error msg
    return do
      validateTemplateArity arity.getNat parts
      pure <| .template arity.getNat prec.getNat parts
  else
    return throw "invalid LaTeX notation specification"

syntax (name := latex) "latex " latexSpec : attr

private def addLatexAttr (declName : Lean.Name) (stx : Lean.Syntax) (kind : Lean.AttributeKind) :
    Lean.AttrM Unit := do
  unless kind == .global do
    Lean.throwAttrMustBeGlobal `latex kind
  let some specStx := findLatexSpec? stx
    | throwError "invalid `[latex]` attribute"
  let spec ←
    match (← elabNotationSpec declName specStx) with
    | .ok spec => pure spec
    | .error msg => throwError msg
  Lean.modifyEnv fun env => registerNotation env declName spec

initialize
  Lean.registerBuiltinAttribute {
    ref := `LeanTeX.LaTeX.addLatexAttr
    name := `latex
    descr := "register a LaTeX rendering specification for a declaration"
    add := addLatexAttr
  }

end LaTeX
end LeanTeX
