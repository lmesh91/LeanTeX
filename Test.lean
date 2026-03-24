import LeanTeX

open LeanTeX.LaTeX

def assertEqString (label expected actual : String) : IO Unit := do
  if actual = expected then
    pure ()
  else
    throw <| IO.userError s!"{label} failed\nexpected: {expected}\nactual:   {actual}"

def checkRender (label : String) (doc : Doc) (expected : String) : IO Unit :=
  assertEqString label expected doc.render

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

def main : IO Unit := do
  for (label, doc, expected) in tests do
    checkRender label doc expected
  IO.println s!"{tests.length} tests passed"
