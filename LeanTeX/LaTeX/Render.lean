import Lean
import LeanTeX.LaTeX.Notation

namespace LeanTeX
namespace LaTeX

def binderPrec : Prec := 1
def arrowPrec : Prec := 2
def appPrec : Prec := 70
def projPrec : Prec := 90

private structure RenderCtx where
  boundNames : Array String := #[]

private def escapeLaTeX (s : String) : String :=
  s.foldl (init := "") fun acc c =>
    acc ++ match c with
    | '_' => "\\_"
    | '{' => "\\{"
    | '}' => "\\}"
    | '#' => "\\#"
    | '$' => "\\$"
    | '%' => "\\%"
    | '&' => "\\&"
    | _ => String.singleton c

private def renderNameAtom (s : String) : Doc :=
  Doc.atom (escapeLaTeX s)

private def renderConstName (declName : Lean.Name) : Doc :=
  Doc.cmd "mathsf" #[renderNameAtom declName.toString]

private def renderLevel (u : Lean.Level) : Doc :=
  renderNameAtom (toString u)

private def renderSort (u : Lean.Level) : Doc :=
  match u with
  | .zero => Doc.cmd "mathsf" #[Doc.atom "Prop"]
  | .succ .zero => Doc.cmd "mathsf" #[Doc.atom "Type"]
  | .succ v => Doc.subscript (Doc.cmd "mathsf" #[Doc.atom "Type"]) (renderLevel v)
  | _ => Doc.subscript (Doc.cmd "mathsf" #[Doc.atom "Sort"]) (renderLevel u)

private def fallbackArgDoc (index : Nat) : Doc :=
  renderNameAtom s!"?arg_{index}"

private def freshBinderName (suggested : Lean.Name) (used : Array String) : String :=
  let base :=
    let s := suggested.toString
    if suggested.isAnonymous || s.isEmpty then "x" else s
  if !used.contains base then
    base
  else
    Id.run do
      for i in [1:used.size + 2] do
        let candidate := s!"{base}_{i}"
        if !used.contains candidate then
          return candidate
      s!"{base}_{used.size + 1}"

private def withBinder (ctx : RenderCtx) (suggested : Lean.Name) (k : RenderCtx → String → Lean.Meta.MetaM Doc) :
    Lean.Meta.MetaM Doc := do
  let binderName := freshBinderName suggested ctx.boundNames
  k { ctx with boundNames := ctx.boundNames.push binderName } binderName

private def getBVarName? (ctx : RenderCtx) (idx : Nat) : Option String :=
  if idx < ctx.boundNames.size then
    let pos := ctx.boundNames.size - 1 - idx
    some ctx.boundNames[pos]!
  else
    none

private def joinWithComma (docs : Array Doc) : Doc :=
  if docs.size = 0 then
    Doc.empty
  else
    Id.run do
      let mut parts := #[docs[0]!]
      for i in [1:docs.size] do
        parts := parts.push (Doc.atom ", ")
        parts := parts.push docs[i]!
      Doc.concat parts

private def applyDoc (fnDoc : Doc) (argDocs : Array Doc) : Doc :=
  if argDocs.isEmpty then
    fnDoc
  else
    let argsDoc := Doc.parens (joinWithComma argDocs)
    Doc.concat #[Doc.protectAt appPrec fnDoc, argsDoc] appPrec

-- We only care whether the innermost binder is used; outer loose bvars are allowed here
-- because rendering may recurse under binders before they are closed.
private partial def binderOccurs (needle : Nat) : Lean.Expr → Bool
  | .bvar idx => idx == needle
  | .app fn arg => binderOccurs needle fn || binderOccurs needle arg
  | .lam _ type body _ => binderOccurs needle type || binderOccurs (needle + 1) body
  | .forallE _ type body _ => binderOccurs needle type || binderOccurs (needle + 1) body
  | .letE _ type value body _ => binderOccurs needle type || binderOccurs needle value || binderOccurs (needle + 1) body
  | .mdata _ expr => binderOccurs needle expr
  | .proj _ _ struct => binderOccurs needle struct
  | _ => false

private def isNondependentArrow (body : Lean.Expr) : Bool :=
  !binderOccurs 0 body

private def getVisibleArgs (fn : Lean.Expr) (args : Array Lean.Expr) : Lean.Meta.MetaM (Array Lean.Expr) := do
  try
    let infos := (← Lean.Meta.getFunInfoNArgs fn args.size).paramInfo
    let mut visible := #[]
    for i in [0:args.size] do
      if h : i < infos.size then
        if infos[i].binderInfo.isExplicit then
          visible := visible.push args[i]!
      else
        visible := visible.push args[i]!
    pure visible
  catch _ =>
    pure args

mutual

  -- Applications are rendered from the elaborated spine so registry-based notation can
  -- decide whether a constant is infix, prefix, command-like, or a plain function call.
  private partial def renderApp (ctx : RenderCtx) (fn : Lean.Expr) (args : Array Lean.Expr) : Lean.Meta.MetaM Doc := do
    let fn ← Lean.instantiateMVars fn
    match fn with
    | .const declName _ =>
        match findNotation? (← Lean.getEnv) declName with
        | some spec => renderNotationApp ctx declName spec args
        | none =>
            let argDocs ← args.mapM (renderExprCore ctx)
            pure <| applyDoc (renderConstName declName) argDocs
    | _ =>
        let headDoc ← renderExprCore ctx fn
        let argDocs ← args.mapM (renderExprCore ctx)
        pure <| applyDoc headDoc argDocs

  -- Template and operator specs consume leading explicit arguments and leave any remainder
  -- to the generic application fallback so partial and over-applied terms stay renderable.
  private partial def renderNotationApp (ctx : RenderCtx) (declName : Lean.Name) (spec : NotationSpec) (args : Array Lean.Expr) :
      Lean.Meta.MetaM Doc := do
    let fallback := do
      let argDocs ← args.mapM (renderExprCore ctx)
      pure <| applyDoc (renderConstName declName) argDocs
    match spec with
    | .const doc =>
        let argDocs ← args.mapM (renderExprCore ctx)
        pure <| applyDoc doc argDocs
    | .prefix prec op =>
        if h : 0 < args.size then
          let argDoc ← renderExprCore ctx args[0]
          let rendered := Doc.prefixOp prec op argDoc
          let restDocs ← (args.extract 1 args.size).mapM (renderExprCore ctx)
          pure <| applyDoc rendered restDocs
        else
          fallback
    | .postfix prec op =>
        if h : 0 < args.size then
          let argDoc ← renderExprCore ctx args[0]
          let rendered := Doc.postfixOp prec argDoc op
          let restDocs ← (args.extract 1 args.size).mapM (renderExprCore ctx)
          pure <| applyDoc rendered restDocs
        else
          fallback
    | .infix prec assoc op =>
        if h : 1 < args.size then
          let lhsDoc ← renderExprCore ctx args[0]
          let rhsDoc ← renderExprCore ctx args[1]
          let rendered := Doc.infixOp prec assoc lhsDoc op rhsDoc
          let restDocs ← (args.extract 2 args.size).mapM (renderExprCore ctx)
          pure <| applyDoc rendered restDocs
        else
          fallback
    | .command name =>
        let argDocs ← args.mapM (renderExprCore ctx)
        pure <| Doc.cmd name argDocs
    | .template arity prec parts =>
        if h : arity ≤ args.size then
          let renderedArgs ← (args.extract 0 arity).mapM (renderExprCore ctx)
          let rendered :=
            let partDocs := parts.map fun
              | .text doc => doc
              | .arg index =>
                  if h : index < renderedArgs.size then
                    renderedArgs[index]
                  else
                    fallbackArgDoc index
            Doc.concat partDocs prec
          let restDocs ← (args.extract arity args.size).mapM (renderExprCore ctx)
          pure <| applyDoc rendered restDocs
        else
          fallback

  private partial def renderExprCore (ctx : RenderCtx) (expr : Lean.Expr) : Lean.Meta.MetaM Doc := do
    let expr ← Lean.instantiateMVars expr
    if let some n := expr.nat? then
      return Doc.atom (toString n)
    match expr with
    | .bvar idx =>
        pure <| renderNameAtom <| getBVarName? ctx idx |>.getD s!"?bvar_{idx}"
    | .fvar fvarId =>
        let decl ← fvarId.getDecl
        pure <| renderNameAtom decl.userName.toString
    | .mvar _ =>
        pure <| Doc.atom "\\_"
    | .sort u =>
        pure <| renderSort u
    | .const declName _ =>
        match findNotation? (← Lean.getEnv) declName with
        | some (.const doc) => pure doc
        | some (.command name) => pure (Doc.cmd name)
        | _ => pure (renderConstName declName)
    | .app _ _ =>
        let fn := expr.getAppFn
        let args ← getVisibleArgs fn expr.getAppArgs
        renderApp ctx fn args
    | .lam binderName binderType body _ =>
        withBinder ctx binderName fun ctx binderName => do
          let typeDoc ← renderExprCore ctx binderType
          let bodyDoc ← renderExprCore ctx body
          let binderDoc := Doc.concat #[renderNameAtom binderName, Doc.atom " : ", typeDoc]
          pure <| Doc.concat #[Doc.atom "\\lambda ", binderDoc, Doc.atom ", ", bodyDoc] binderPrec
    | .forallE binderName binderType body _ =>
        withBinder ctx binderName fun ctx binderName => do
          let typeDoc ← renderExprCore ctx binderType
          let bodyDoc ← renderExprCore ctx body
          if isNondependentArrow body then
            pure <| Doc.infixRight arrowPrec typeDoc (Doc.atom "\\to") bodyDoc
          else
            let binderDoc := Doc.concat #[renderNameAtom binderName, Doc.atom " : ", typeDoc]
            pure <| Doc.concat #[Doc.atom "\\forall ", binderDoc, Doc.atom ", ", bodyDoc] binderPrec
    | .letE binderName binderType value body _ =>
        withBinder ctx binderName fun ctx binderName => do
          let typeDoc ← renderExprCore ctx binderType
          let valueDoc ← renderExprCore ctx value
          let bodyDoc ← renderExprCore ctx body
          let lhs := Doc.concat #[
            Doc.cmd "mathsf" #[Doc.atom "let"],
            Doc.atom "\\,",
            renderNameAtom binderName,
            Doc.atom " : ",
            typeDoc,
            Doc.atom " := ",
            valueDoc
          ]
          pure <| Doc.concat #[lhs, Doc.atom "; ", bodyDoc] binderPrec
    | .lit (.natVal n) =>
        pure <| Doc.atom (toString n)
    | .lit (.strVal s) =>
        pure <| Doc.cmd "mathtt" #[Doc.atom ("\"" ++ escapeLaTeX s ++ "\"")]
    | .mdata _ expr =>
        renderExprCore ctx expr
    | .proj _ idx struct =>
        let structDoc ← renderExprCore ctx struct
        pure <| Doc.concat #[Doc.protectAt projPrec structDoc, Doc.atom s!".{idx}"] projPrec

end

def renderExpr (expr : Lean.Expr) : Lean.Meta.MetaM Doc :=
  renderExprCore {} expr

def renderExprString (expr : Lean.Expr) : Lean.Meta.MetaM String := do
  pure (← renderExpr expr).render

end LaTeX
end LeanTeX
