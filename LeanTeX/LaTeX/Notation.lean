import Lean
import LeanTeX.LaTeX.Basic

namespace LeanTeX
namespace LaTeX

/--
Pieces used by template-based notation specifications.
`arg i` refers to the `i`-th explicit argument in the application spine.
-/
inductive TemplatePart where
  | text (doc : Doc)
  | arg (index : Nat)
  deriving Repr, Inhabited

/--
Persistent description of how a declaration should be rendered in LaTeX.
This is pure data so it can live in environment extensions and later be
attached using attributes or commands.
-/
inductive NotationSpec where
  | const (doc : Doc)
  | prefix (prec : Prec) (op : Doc)
  | postfix (prec : Prec) (op : Doc)
  | infix (prec : Prec) (assoc : Assoc) (op : Doc)
  | command (name : String)
  | template (arity : Nat) (prec : Prec) (parts : Array TemplatePart)
  deriving Repr, Inhabited

namespace NotationSpec

def symbol (doc : Doc) : NotationSpec :=
  .const doc

end NotationSpec

structure NotationEntry where
  declName : Lean.Name
  spec : NotationSpec
  deriving Repr, Inhabited

abbrev NotationRegistry := Lean.NameMap NotationSpec

private def addNotationEntry (registry : NotationRegistry) (entry : NotationEntry) : NotationRegistry :=
  registry.insert entry.declName entry.spec

private def notationRegistryFromImports (entries : Array (Array NotationEntry)) : NotationRegistry :=
  Lean.mkStateFromImportedEntries addNotationEntry ({} : NotationRegistry) entries

builtin_initialize notationExt : Lean.SimplePersistentEnvExtension NotationEntry NotationRegistry ←
  Lean.registerSimplePersistentEnvExtension {
    name := `LeanTeX.LaTeX.notationExt
    addEntryFn := addNotationEntry
    addImportedFn := notationRegistryFromImports
  }

/--
Register a notation entry in the current environment. If the same declaration
is registered more than once, the most recent entry wins.
-/
def registerNotation (env : Lean.Environment) (declName : Lean.Name) (spec : NotationSpec) : Lean.Environment :=
  notationExt.addEntry env { declName := declName, spec := spec }

def modifyNotation (declName : Lean.Name) (spec : NotationSpec) : Lean.CoreM Unit :=
  Lean.modifyEnv fun env => registerNotation env declName spec

def getNotationRegistry (env : Lean.Environment) : NotationRegistry :=
  Lean.SimplePersistentEnvExtension.getState notationExt env

def findNotation? (env : Lean.Environment) (declName : Lean.Name) : Option NotationSpec :=
  Lean.NameMap.find? (getNotationRegistry env) declName

def containsNotation (env : Lean.Environment) (declName : Lean.Name) : Bool :=
  Lean.NameMap.contains (getNotationRegistry env) declName

/--
Entries registered in the current module, returned in insertion order.
-/
def getLocalNotationEntries (env : Lean.Environment) : Array NotationEntry :=
  Lean.SimplePersistentEnvExtension.getEntries notationExt env |>.reverse.toArray

end LaTeX
end LeanTeX
