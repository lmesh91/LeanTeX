import LeanTeX.LaTeX.Basic

namespace LeanTeX
namespace LaTeX

/--
A custom renderer consumes the rendered arguments of a notation application
and returns the resulting document fragment.
-/
abbrev CustomRenderer := Array Doc → Doc

end LaTeX
end LeanTeX
