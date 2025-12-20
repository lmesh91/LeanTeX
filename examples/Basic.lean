-- A collection of very trivial proofs
variable {p : Prop} {α : Type}

theorem truth : True :=
  True.intro

/-
Known issue: LeanTeX does not keep track of "h" when parsing this proof
theorem ex_falso (h : False) : p :=
  False.elim h
-/

theorem ex_falso : False → p :=
  False.elim

theorem one_plus_one_eq_two : 1 + 1 = 2 :=
  rfl

theorem implies_self : p → p :=
  id

theorem refl_all : ∀ x : α, x = x :=
  fun _ => rfl

theorem im_sorry : False :=
  sorry
