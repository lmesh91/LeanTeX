theorem true : True := True.intro

theorem imp_true (p : Prop) : p → True :=
  fun _ => True.intro

theorem and_mk {p q : Prop} : p → q → p ∧ q :=
  fun hp hq => And.intro hp hq

theorem comm_and_one_way {p q : Prop} : p ∧ q → q ∧ p :=
  fun h =>
    And.intro (And.right h) (And.left h)

theorem comm_and_test : p ∧ q ↔ q ∧ p :=
    Iff.intro
        (fun h : p ∧ q =>
        show q ∧ p from And.intro h.right h.left)
        (fun h : q ∧ p =>
        show p ∧ q from And.intro h.right h.left)
