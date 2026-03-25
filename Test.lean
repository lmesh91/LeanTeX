import LeanTeX

open LeanTeX.LaTeX

def assertEqString (label expected actual : String) : IO Unit := do
  if actual = expected then
    pure ()
  else
    throw <| IO.userError s!"{label} failed\nexpected: {expected}\nactual:   {actual}"

def checkRender (label : String) (doc : Doc) (expected : String) : IO Unit :=
  assertEqString label expected doc.render

def mkTestEnv : IO Lean.Environment := do
  Lean.initSearchPath (← Lean.findSysroot)
  Lean.importModules #[{ module := `Init }] {} (loadExts := true)

def runMetaWithEnv (env : Lean.Environment) (x : Lean.Meta.MetaM α) : IO α := do
  let coreCtx : Lean.Core.Context := {
    fileName := "<test>"
    fileMap := Lean.FileMap.ofString ""
  }
  let coreState : Lean.Core.State := { env := env }
  let (a, _, _) ← x.toIO coreCtx coreState
  pure a

def assert (label : String) (cond : Bool) : IO Unit := do
  unless cond do
    throw <| IO.userError s!"{label} failed"

def assertEq [BEq α] [ToString α] (label : String) (expected actual : α) : IO Unit := do
  if expected == actual then
    pure ()
  else
    throw <| IO.userError s!"{label} failed\nexpected: {expected}\nactual:   {actual}"

def assertSpecEq (label : String) (expected actual : Option NotationSpec) : IO Unit := do
  let expectedStr := s!"{repr expected}"
  let actualStr := s!"{repr actual}"
  unless expectedStr == actualStr do
    throw <| IO.userError s!"{label} failed\nexpected: {repr expected}\nactual:   {repr actual}"

def assertCounted (count : IO.Ref Nat) (label : String) (cond : Bool) : IO Unit := do
  assert label cond
  count.modify (· + 1)

def assertEqCounted [BEq α] [ToString α] (count : IO.Ref Nat) (label : String) (expected actual : α) : IO Unit := do
  assertEq label expected actual
  count.modify (· + 1)

def assertSpecEqCounted (count : IO.Ref Nat) (label : String) (expected actual : Option NotationSpec) : IO Unit := do
  assertSpecEq label expected actual
  count.modify (· + 1)

def tests : List (String × Doc × String) :=
  let a := Doc.atom "a"
  let b := Doc.atom "b"
  let c := Doc.atom "c"
  let i := Doc.atom "i"
  let one := Doc.atom "1"
  let two := Doc.atom "2"
  let plus := Doc.atom "+"
  let times := Doc.atom "\\cdot"
  let implies := Doc.atom "\\implies"
  let add := Doc.infixLeft 10
  let mul := Doc.infixLeft 20
  let imp := Doc.infixRight 5
  let succ := Doc.postfixOp 30
  let neg := Doc.prefixOp 30
  [ ("atom", Doc.atom "x", "x")
  , ("concat", Doc.concat #[a, Doc.space, b], "a b")
  , ("delimited", Doc.parens a, "\\left(a\\right)")
  , ("command", Doc.cmd "frac" #[a, b], "\\frac{a}{b}")
  , ("prefix", neg (Doc.atom "-") a, "- a")
  , ("postfix", succ a (Doc.atom "!"), "a!")
  , ("infix precedence", add a plus (mul b times c), "a + b \\cdot c")
  , ("infix parenthesize lhs", mul (add a plus b) times c, "\\left(a + b\\right) \\cdot c")
  , ("left assoc chain", add (add a plus b) plus c, "a + b + c")
  , ("left assoc rhs parens", add a plus (add b plus c), "a + \\left(b + c\\right)")
  , ("right assoc chain", imp a implies (imp b implies c), "a \\implies b \\implies c")
  , ("right assoc lhs parens", imp (imp a implies b) implies c, "\\left(a \\implies b\\right) \\implies c")
  , ("superscript", Doc.superscript (add a plus b) two, "\\left(a + b\\right)^{2}")
  , ("subscript", Doc.subscript a (add i plus one), "a_{i + 1}")
  ]

def notationTests : IO Nat := do
  let count ← IO.mkRef 0
  let env0 ← Lean.mkEmptyEnvironment
  assertCounted count "empty registry" (!(containsNotation env0 `Nat.add))
  assertCounted count "empty lookup" ((findNotation? env0 `Nat.add).isNone)

  let addSpec : NotationSpec := .infix 10 .left (Doc.atom "+")
  let succSpec : NotationSpec := .postfix 30 (Doc.atom "!")
  let addOverride : NotationSpec := .command "operatorname"

  let env1 := registerNotation env0 `Nat.add addSpec
  assertCounted count "contains inserted notation" (containsNotation env1 `Nat.add)
  assertSpecEqCounted count "lookup inserted notation" (some addSpec) (findNotation? env1 `Nat.add)

  let env2 := registerNotation env1 `Nat.succ succSpec
  assertSpecEqCounted count "second lookup preserved first" (some addSpec) (findNotation? env2 `Nat.add)
  assertSpecEqCounted count "second lookup added second" (some succSpec) (findNotation? env2 `Nat.succ)

  let env3 := registerNotation env2 `Nat.add addOverride
  assertSpecEqCounted count "override wins" (some addOverride) (findNotation? env3 `Nat.add)

  let localEntries := getLocalNotationEntries env3
  assertEqCounted count "local entry count" 3 localEntries.size
  assertEqCounted count "local entry order 1" `Nat.add localEntries[0]!.declName
  assertEqCounted count "local entry order 2" `Nat.succ localEntries[1]!.declName
  assertEqCounted count "local entry order 3" `Nat.add localEntries[2]!.declName
  count.get

def exprTests : IO Nat := do
  let count ← IO.mkRef 0
  let env ← mkTestEnv

  let checkExprRender (label : String) (env : Lean.Environment) (renderM : Lean.Meta.MetaM String)
      (expected : String) : IO Unit := do
    let actual ← runMetaWithEnv env renderM
    assertEqString label expected actual
    count.modify (· + 1)

  checkExprRender "const fallback" env (renderExprString (Lean.mkConst `Nat)) "\\mathsf{Nat}"
  checkExprRender "app fallback" env (renderExprString (Lean.mkApp (Lean.mkConst `Nat.succ) (Lean.mkNatLit 3)))
    "\\mathsf{Nat.succ}\\left(3\\right)"

  let envAdd := registerNotation env `Nat.add (.infix 10 .left (Doc.atom "+"))
  checkExprRender "infix notation" envAdd
    (renderExprString (Lean.mkAppN (Lean.mkConst `Nat.add) #[Lean.mkNatLit 1, Lean.mkNatLit 2]))
    "1 + 2"
  checkExprRender "partial infix fallback" envAdd
    (renderExprString (Lean.mkApp (Lean.mkConst `Nat.add) (Lean.mkNatLit 1)))
    "\\mathsf{Nat.add}\\left(1\\right)"

  let envSucc := registerNotation env `Nat.succ (.command "sin")
  checkExprRender "command notation" envSucc
    (renderExprString (Lean.mkApp (Lean.mkConst `Nat.succ) (Lean.mkNatLit 3)))
    "\\sin{3}"

  let envMul := registerNotation env `Nat.mul <|
    .template 2 20 #[
      .arg 0,
      .text (Doc.atom " \\star "),
      .arg 1
    ]
  checkExprRender "template notation" envMul
    (renderExprString (Lean.mkAppN (Lean.mkConst `Nat.mul) #[Lean.mkNatLit 2, Lean.mkNatLit 3]))
    "2 \\star 3"

  checkExprRender "arrow fallback" env
    (renderExprString (Lean.Expr.forallE `x (Lean.mkConst `Nat) (Lean.mkConst `Nat) .default))
    "\\mathsf{Nat} \\to \\mathsf{Nat}"

  checkExprRender "dependent forall" env
    (renderExprString (Lean.Expr.forallE `x (Lean.mkConst `Nat) (.bvar 0) .default))
    "\\forall x : \\mathsf{Nat}, x"

  checkExprRender "lambda freshening" env
    (renderExprString (Lean.Expr.lam `x (Lean.mkConst `Nat)
      (Lean.Expr.lam `x (Lean.mkConst `Nat) (.bvar 0) .default) .default))
    "\\lambda x : \\mathsf{Nat}, \\lambda x\\_1 : \\mathsf{Nat}, x\\_1"

  checkExprRender "let fallback" env
    (renderExprString (Lean.Expr.letE `x (Lean.mkConst `Nat) (Lean.mkNatLit 1) (.bvar 0) false))
    "\\mathsf{let}\\,x : \\mathsf{Nat} := 1; x"

  checkExprRender "fvar fallback" env
    (Lean.Meta.withLocalDecl `x .default (Lean.mkConst `Nat) fun x => renderExprString x)
    "x"

  count.get

def main : IO Unit := do
  for (label, doc, expected) in tests do
    checkRender label doc expected
  let notationCount ← notationTests
  let exprCount ← exprTests
  IO.println s!"{tests.length + notationCount + exprCount} tests passed"
