import LeanTeX

open LeanTeX.LaTeX

@[latex custom:2 (fun
  | #[x, y] => LeanTeX.LaTeX.Doc.cmd "frac" #[x, y]
  | _ => LeanTeX.LaTeX.Doc.atom "\\mathsf{bad_inline_div}")]
def testInlineDiv (x y : Nat) : Nat :=
  Nat.div x y

@[latex template:80 x:1024 "^{" y:0 "}"]
def testPow (x y : Nat) : Nat :=
  Nat.pow x y

#guard_latex_str (2 + 2) => "2 + 2"
#guard_latex_str (1 = 2) => "1 = 2"
#guard_latex_str (True ∧ False) => "\\top \\land \\bot"
#guard_latex_str (¬ True ↔ False) => "\\neg \\top \\leftrightarrow \\bot"
#guard_latex_str (testInlineDiv (1 + 2) (3 + 4)) => "\\frac{1 + 2}{3 + 4}"
#guard_latex_str (6 / 3) => "6 \\div 3"
#guard_latex_str ((1 + 2) / (3 + 4)) => "\\frac{1 + 2}{3 + 4}"
#guard_latex_str (testPow (1 + 2) 3) => "\\left(1 + 2\\right)^{3}"

def assertEqString (label expected actual : String) : IO Unit := do
  if actual = expected then
    pure ()
  else
    throw <| IO.userError s!"{label} failed\nexpected: {expected}\nactual:   {actual}"

def checkRender (label : String) (doc : Doc) (expected : String) : IO Unit :=
  assertEqString label expected doc.render

def mkImportedEnv (modules : Array Lean.Name) : IO Lean.Environment := do
  Lean.initSearchPath (← Lean.findSysroot)
  Lean.importModules (modules.map fun module => { module := module }) {} (loadExts := true)

def mkTestEnv : IO Lean.Environment :=
  mkImportedEnv #[`Init]

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
  , ("prefix chain", neg (Doc.atom "-") (neg (Doc.atom "-") a), "- - a")
  , ("postfix", succ a (Doc.atom "!"), "a!")
  , ("postfix chain", succ (succ a (Doc.atom "!")) (Doc.atom "!"), "a!!")
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
  let divSpec : NotationSpec := .custom 2 `LeanTeX.LaTeX.division

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

  let env4 := registerNotation env3 `HDiv.hDiv divSpec
  assertSpecEqCounted count "custom renderer lookup" (some divSpec) (findNotation? env4 `HDiv.hDiv)
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

  let envPow := registerNotation env `Nat.pow <|
    .template 2 80 #[
      .arg 0 (some tightPrec),
      .text (Doc.atom "^{"),
      .arg 1 (some 0),
      .text (Doc.atom "}")
    ]
  let addExpr := Lean.mkAppN (Lean.mkConst `Nat.add) #[Lean.mkNatLit 1, Lean.mkNatLit 2]
  let envPowAdd := registerNotation envPow `Nat.add (.infix 65 .left (Doc.atom "+"))
  checkExprRender "template precedence override" envPowAdd
    (renderExprString (Lean.mkAppN (Lean.mkConst `Nat.pow) #[addExpr, Lean.mkNatLit 3]))
    "\\left(1 + 2\\right)^{3}"

  checkExprRender "arrow fallback" env
    (renderExprString (Lean.Expr.forallE `x (Lean.mkConst `Nat) (Lean.mkConst `Nat) .default))
    "\\mathsf{Nat} \\to \\mathsf{Nat}"

  checkExprRender "dependent forall" env
    (renderExprString (Lean.Expr.forallE `x (Lean.mkConst `Nat) (.bvar 0) .default))
    "\\forall x : \\mathsf{Nat}, x"

  checkExprRender "lambda freshening" env
    (renderExprString (Lean.Expr.lam `x (Lean.mkConst `Nat)
      (Lean.Expr.lam `x (Lean.mkConst `Nat) (.bvar 0) .default) .default))
    "\\lambda x : \\mathsf{Nat}, \\lambda x_{1} : \\mathsf{Nat}, x_{1}"

  checkExprRender "let fallback" env
    (renderExprString (Lean.Expr.letE `x (Lean.mkConst `Nat) (Lean.mkNatLit 1) (.bvar 0) false))
    "\\mathsf{let}\\,x : \\mathsf{Nat} := 1; x"

  checkExprRender "fvar fallback" env
    (Lean.Meta.withLocalDecl `x .default (Lean.mkConst `Nat) fun x => renderExprString x)
    "x"
  checkExprRender "fvar subscript name" env
    (Lean.Meta.withLocalDecl `x_12 .default (Lean.mkConst `Nat) fun x => renderExprString x)
    "x_{12}"
  checkExprRender "fvar unicode subscript name" env
    (Lean.Meta.withLocalDecl `x₁₂ .default (Lean.mkConst `Nat) fun x => renderExprString x)
    "x_{12}"

  count.get

def main : IO Unit := do
  for (label, doc, expected) in tests do
    checkRender label doc expected
  let notationCount ← notationTests
  let exprCount ← exprTests
  IO.println s!"{tests.length + notationCount + exprCount} tests passed"
