import Lean
import LeanTeX.LaTeX.Notation
import LeanTeX.LaTeX.CustomRenderer

namespace LeanTeX
namespace LaTeX

-- Defines the precedence of builtin operations used in the `Expr` tree.
def binderPrec : Prec := 1
def arrowPrec : Prec := 25
def appPrec : Prec := 1023
def projPrec : Prec := 1023

-- The rendering context tracks the names of bound variables in scope so
-- they can be rendered as names instead of de Bruijn indices.
private structure RenderCtx where
  boundNames : Array String := #[]

-- Escapes special characters in a string so it can be safely rendered in LaTeX.
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

/--
Render one identifier character, escaping ASCII characters that are special in LaTeX.
-/
private def renderNameChar (c : Char) : Doc :=
  Doc.atom (escapeLaTeX (String.singleton c))

/--
Render one identifier chunk with no underscore-based subscript splitting.
-/
private def renderNameChunk (s : String) : Doc :=
  Doc.concat <| s.toList.map renderNameChar |>.toArray

/--
Translate Lean's Unicode subscript identifier characters back to the
corresponding baseline characters.
-/
private def unicodeSubscriptChar? (c : Char) : Option String :=
  match c with
  | 'ᵢ' => some "i"
  | 'ᵣ' => some "r"
  | 'ᵤ' => some "u"
  | 'ᵥ' => some "v"
  | 'ᵦ' => some "β"
  | 'ᵧ' => some "γ"
  | 'ᵨ' => some "ρ"
  | 'ᵩ' => some "φ"
  | 'ᵪ' => some "χ"
  | '₀' => some "0"
  | '₁' => some "1"
  | '₂' => some "2"
  | '₃' => some "3"
  | '₄' => some "4"
  | '₅' => some "5"
  | '₆' => some "6"
  | '₇' => some "7"
  | '₈' => some "8"
  | '₉' => some "9"
  | 'ₐ' => some "a"
  | 'ₑ' => some "e"
  | 'ₒ' => some "o"
  | 'ₓ' => some "x"
  | 'ₔ' => some "ə"
  | 'ₕ' => some "h"
  | 'ₖ' => some "k"
  | 'ₗ' => some "l"
  | 'ₘ' => some "m"
  | 'ₙ' => some "n"
  | 'ₚ' => some "p"
  | 'ₛ' => some "s"
  | 'ₜ' => some "t"
  | 'ⱼ' => some "j"
  | _ => none

private inductive NameToken where
  | normal (value : String)
  | subscript (value : String)

private inductive NameParseMode where
  | normal
  | asciiSubscript
  | unicodeSubscript

private def pushNameToken? (tokens : Array NameToken) (mode : NameParseMode) (value : String) :
    Option (Array NameToken) :=
  if value.isEmpty then
    match mode with
    | .normal => some tokens
    | .asciiSubscript | .unicodeSubscript => none
  else
    let token :=
      match mode with
      | .normal => NameToken.normal value
      | .asciiSubscript | .unicodeSubscript => NameToken.subscript value
    some (tokens.push token)

/--
Parse name text into ordinary chunks and subscript chunks. ASCII underscores
consume the following whole chunk (`x_12a` -> `x_{12a}`), while Unicode
subscript characters only consume the contiguous Unicode subscript run
(`x₁₂a` -> `x_{12}a`).
-/
private def parseNameTokens? (s : String) : Option (Array NameToken) :=
  let rec loop (chars : List Char) (tokens : Array NameToken) (mode : NameParseMode) (buf : String) :
      Option (Array NameToken) :=
    match chars with
    | [] => pushNameToken? tokens mode buf
    | c :: chars =>
        if c == '_' then
          match pushNameToken? tokens mode buf with
          | some tokens => loop chars tokens .asciiSubscript ""
          | none => none
        else
          match unicodeSubscriptChar? c with
          | some sub =>
              match mode with
              | .normal =>
                  match pushNameToken? tokens mode buf with
                  | some tokens => loop chars tokens .unicodeSubscript sub
                  | none => none
              | .asciiSubscript | .unicodeSubscript =>
                  loop chars tokens mode (buf ++ sub)
          | none =>
              match mode with
              | .unicodeSubscript =>
                  match pushNameToken? tokens mode buf with
                  | some tokens => loop chars tokens .normal (String.singleton c)
                  | none => none
              | .normal | .asciiSubscript =>
                  loop chars tokens mode (buf.push c)
  loop s.toList #[] .normal ""

private def renderNameTokens? (tokens : Array NameToken) : Option Doc :=
  match tokens.toList with
  | [] => some Doc.empty
  | .normal base :: rest =>
      some <| rest.foldl (init := renderNameChunk base) fun acc token =>
        match token with
        | .normal value => Doc.concat #[acc, renderNameChunk value]
        | .subscript value => Doc.subscript acc (renderNameChunk value)
  | .subscript _ :: _ => none

-- Parse names like `x_12` and `x₁₂` as subscripts.
private def renderNameAtom (s : String) : Doc :=
  match parseNameTokens? s >>= renderNameTokens? with
  | some doc => doc
  | none => renderNameChunk s

-- Renders a constant name as a LaTeX atom, using \mathsf to distinguish it from variables.
private def renderConstName (declName : Lean.Name) : Doc :=
  let s := declName.toString
  let doc := renderNameAtom s
  if s.toList.all (fun c => c.toNat < 128) then
    Doc.cmd "mathsf" #[doc]
  else
    doc

-- Converts a universe level as a LaTeX using its string representation.
private def renderLevel (u : Lean.Level) : Doc :=
  renderNameAtom (toString u)

-- Renders a universe level as a LaTeX atom.
private def renderSort (u : Lean.Level) : Doc :=
  match u with
  | .zero => Doc.cmd "mathsf" #[Doc.atom "Prop"]
  | .succ .zero => Doc.cmd "mathsf" #[Doc.atom "Type"]
  | .succ v => Doc.subscript (Doc.cmd "mathsf" #[Doc.atom "Type"]) (renderLevel v)
  | _ => Doc.subscript (Doc.cmd "mathsf" #[Doc.atom "Sort"]) (renderLevel u)

-- Renders an argument placeholder for a notation template when the index is out of bounds.
private def fallbackArgDoc (index : Nat) : Doc :=
  renderNameAtom s!"?arg_{index}"

-- Provides a fresh binder name based on a suggested name and the names already in use in the context.
-- This ensures that binder names are always unique.
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

-- Renders a binder by generating a fresh name and passing it to a continuation.
private def withBinder (ctx : RenderCtx) (suggested : Lean.Name) (k : RenderCtx → String → Lean.Meta.MetaM Doc) :
    Lean.Meta.MetaM Doc := do
  let binderName := freshBinderName suggested ctx.boundNames
  k { ctx with boundNames := ctx.boundNames.push binderName } binderName

-- Attempts to fetch the name of a bound variable from the context; returns none if the index is out of bounds and should be rendered as a de Bruijn index.
private def getBVarName? (ctx : RenderCtx) (idx : Nat) : Option String :=
  if idx < ctx.boundNames.size then
    let pos := ctx.boundNames.size - 1 - idx
    some ctx.boundNames[pos]!
  else
    none

-- Joins a list of `Doc`s with commas, returning an empty `Doc` if the list is empty.
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

-- Creates a `Doc` representing function application given a function and a list of arguments.
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

-- Determines whether a forall expression is non-dependent (arrow) or dependent (forall).
private def isNondependentArrow (body : Lean.Expr) : Bool :=
  !binderOccurs 0 body

-- Extracts the visible (explicitly bound) arguments from a function application.
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

-- The main rendering functions are mutually recursive
mutual

  -- Uses the notation specification to render an application of a constant.
  -- If there are remaining arguments after this process, they are rendered with a fallback.
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
              | .arg index partPrec? =>
                  if h : index < renderedArgs.size then
                    match partPrec? with
                    | some partPrec => Doc.protectAt partPrec renderedArgs[index]
                    | none => renderedArgs[index]
                  else
                    fallbackArgDoc index
            Doc.concat partDocs prec
          let restDocs ← (args.extract arity args.size).mapM (renderExprCore ctx)
          pure <| applyDoc rendered restDocs
        else
          fallback
    | .custom arity rendererName =>
        if h : arity ≤ args.size then
          let renderedArgs ← (args.extract 0 arity).mapM (renderExprCore ctx)
          try
              let customRenderer ← unsafe Lean.evalConst CustomRenderer rendererName
              let rendered := customRenderer renderedArgs
              let restDocs ← (args.extract arity args.size).mapM (renderExprCore ctx)
              pure <| applyDoc rendered restDocs
          catch _ =>
              fallback
        else
          fallback
    | .customExpr arity rendererExpr =>
        if h : arity ≤ args.size then
          let renderedArgs ← (args.extract 0 arity).mapM (renderExprCore ctx)
          try
              let customRenderer ← unsafe Lean.Meta.evalExpr CustomRenderer (Lean.mkConst ``CustomRenderer) rendererExpr
              let rendered := customRenderer renderedArgs
              let restDocs ← (args.extract arity args.size).mapM (renderExprCore ctx)
              pure <| applyDoc rendered restDocs
          catch _ =>
              fallback
        else
          fallback

  -- Core function for converting an expression to a `Doc`.
  private partial def renderExprCore (ctx : RenderCtx) (expr : Lean.Expr) : Lean.Meta.MetaM Doc := do
    let expr ← Lean.instantiateMVars expr
    if let some n := expr.nat? then
      return Doc.atom (toString n)
    match expr with
    -- Bound variables are rendered using their names from the context when possible, and de Bruijn indices otherwise.
    | .bvar idx =>
        pure <| renderNameAtom <| getBVarName? ctx idx |>.getD s!"?bvar_{idx}"
    -- Free variables are rendered using their user-facing name.
    | .fvar fvarId =>
        let decl ← fvarId.getDecl
        pure <| renderNameAtom decl.userName.toString
    -- Metavariables are rendered as underscores.
    | .mvar _ =>
        pure <| Doc.atom "\\_"
    -- Sorts are rendered using \mathsf{Prop}, \mathsf{Type}, or \mathsf{Sort} with subscripts for universe levels.
    | .sort u =>
        pure <| renderSort u
    -- Constants are rendered using their notation if available, and otherwise using their name with \mathsf.
    | .const declName _ =>
        match findNotation? (← Lean.getEnv) declName with
        | some (.const doc) => pure doc
        | some (.command name) => pure (Doc.cmd name)
        | _ => pure (renderConstName declName)
    -- Applications are rendered from the elaborated spine so registry-based notation can decide how to render them.
    | .app _ _ =>
        let fn := expr.getAppFn
        let args ← getVisibleArgs fn expr.getAppArgs
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
    -- Binders are rendered by generating a fresh name for the bound variable and passing it to a continuation that renders the type and body with the new name in scope.
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
    -- Literals are rendered using their string representation, with strings escaped and wrapped in \mathtt to distinguish them from variables.
    | .lit (.natVal n) =>
        pure <| Doc.atom (toString n)
    | .lit (.strVal s) =>
        pure <| Doc.cmd "mathtt" #[Doc.atom ("\"" ++ escapeLaTeX s ++ "\"")]
    -- Metadata is ignored for rendering purposes, so we just render the underlying expression.
    | .mdata _ expr =>
        renderExprCore ctx expr
    -- Projections are rendered using the struct notation for projections, with the struct rendered and the field name appended with a dot.
    | .proj _ idx struct =>
        let structDoc ← renderExprCore ctx struct
        pure <| Doc.concat #[Doc.protectAt projPrec structDoc, Doc.atom s!".{idx}"] projPrec

end

-- The main entry point for rendering an expression, which initializes the rendering context and calls the core rendering function.
def renderExpr (expr : Lean.Expr) : Lean.Meta.MetaM Doc :=
  renderExprCore {} expr

-- A convenience function for rendering an expression directly to a string.
def renderExprString (expr : Lean.Expr) : Lean.Meta.MetaM String := do
  pure (← renderExpr expr).render

end LaTeX
end LeanTeX
