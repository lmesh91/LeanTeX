-- A collection of very trivial proofs
variable {p : Prop} {α : Type}

theorem truth : True :=
  True.intro

-- Known issue: LeanTeX does not get that "h" is of type "False" here
theorem ex_falso (h : False) : p :=
  False.elim h

theorem one_plus_one_eq_two : 1 + 1 = 2 :=
  rfl

theorem implies_self : p → p :=
  id

theorem refl_all : ∀ x : α, x = x :=
  fun _ => rfl

theorem im_sorry : False :=
  sorry
