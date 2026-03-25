import LeanTeX.LaTeX.NotationCommand

-- Provides built-in notations for common types and operations.

namespace LeanTeX
namespace LaTeX

attribute [latex command "mathbb{N}"] Nat
attribute [latex command "mathbb{Z}"] Int
attribute [latex command "mathsf{Bool}"] Bool
attribute [latex command "top"] True
attribute [latex command "bot"] False

attribute [latex infixr:90 "\\circ"] Function.comp
attribute [latex infixr:35 "\\times"] Prod
attribute [latex infixr:35 "\\times"] PProd

attribute [latex infix:50 "\\mid"] Dvd.dvd
attribute [latex infixl:65 "+"] HAdd.hAdd
attribute [latex infixl:65 "-"] HSub.hSub
-- TODO: make multiplication implicit in most cases
attribute [latex infixl:70 "\\cdot"] HMul.hMul

-- Use \frac for numerators and denominators that are not atoms
attribute [latex custom:2 (fun
| #[numerator, denominator] =>
  let isNumericLiteral (doc : Doc) :=
    let s := doc.render
    !s.isEmpty && s.toList.all Char.isDigit
  if isNumericLiteral numerator && isNumericLiteral denominator then
    Doc.infixLeft 70 numerator (Doc.atom "\\div") denominator
  else if numerator.prec >= 1023 && denominator.prec >= 1023 then
    Doc.infixLeft 70 numerator (Doc.atom "/") denominator
  else
    Doc.cmd "frac" #[numerator, denominator]
| _ => Doc.empty
)] HDiv.hDiv
attribute [latex template:80 #1:1024 "^{" #2:0 "}"] HPow.hPow
attribute [latex infixl:70 "\\mathbin{\\%}"] HMod.hMod
attribute [latex infixl:65 "++"] HAppend.hAppend
attribute [latex prefix:75 "-"] Neg.neg

attribute [latex infixl:55 "\\mathbin{\\mathtt{|||}}"] HOr.hOr
attribute [latex infixl:58 "\\mathbin{\\mathtt{^^^}}"] HXor.hXor
attribute [latex infixl:60 "\\mathbin{\\mathtt{\\&\\&\\&}}"] HAnd.hAnd
attribute [latex infixr:73 "\\bullet"] HSMul.hSMul

attribute [latex infix:50 "\\le"] LE.le
attribute [latex infix:50 "<"] LT.lt
attribute [latex infix:50 "\\ge"] GE.ge
attribute [latex infix:50 ">"] GT.gt
attribute [latex infix:50 "="] Eq
attribute [latex infix:50 "\\overset{\\mathrm{h}}{=}"] HEq
attribute [latex infix:50 "\\in"] Membership.mem

attribute [latex infix:20 "\\leftrightarrow"] Iff
attribute [latex infixr:35 "\\land"] And
attribute [latex infixr:30 "\\lor"] Or
attribute [latex prefix:40 "\\neg"] Not

attribute [latex infixr:67 "::"] List.cons

end LaTeX
end LeanTeX
