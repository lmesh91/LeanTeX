-- A collection of very trivial proofs
variable {p : Prop} {α : Type}

theorem truth : True :=
  True.intro

theorem ex_falso_a (h : False) : p :=
  False.elim h

theorem ex_falso_b : False → p :=
  False.elim

theorem one_plus_one_eq_two : 1 + 1 = 2 :=
  rfl

theorem implies_self : p → p :=
  id

-- todo: improve parsing of holes
theorem refl_all : ∀ x : α, x = x :=
  fun _ => rfl

theorem im_sorry : False :=
  sorry
