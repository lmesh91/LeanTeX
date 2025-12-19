variable (p q r : Prop)

-- commutativity of ∧ and ∨
theorem comm_and : p ∧ q ↔ q ∧ p :=
    Iff.intro
        (fun h : p ∧ q =>
        show q ∧ p from And.intro h.right h.left)
        (fun h : q ∧ p =>
        show p ∧ q from And.intro h.right h.left)
theorem comm_or : p ∨ q ↔ q ∨ p :=
    Iff.intro
        (fun h : p ∨ q =>
        show q ∨ p from Or.elim h
            (fun hp : p => Or.intro_right q hp)
            (fun hq : q => Or.intro_left p hq))
        (fun h : q ∨ p =>
        show p ∨ q from Or.elim h
            (fun hp : q => Or.intro_right p hp)
            (fun hq : p => Or.intro_left q hq))

-- associativity of ∧ and ∨
theorem assoc_and : (p ∧ q) ∧ r ↔ p ∧ (q ∧ r) :=
    have mp :=
        (fun h : (p ∧ q) ∧ r =>
        show p ∧ (q ∧ r) from And.intro (h.left.left) (And.intro h.left.right h.right))
    have mpr :=
        (fun h : p ∧ (q ∧ r) =>
        show (p ∧ q) ∧ r from And.intro (And.intro h.left h.right.left) (h.right.right))
    Iff.intro mp mpr
theorem assoc_or : (p ∨ q) ∨ r ↔ p ∨ (q ∨ r) :=
    have mp :=
        (fun h : (p ∨ q) ∨ r =>
        show p ∨ (q ∨ r) from Or.elim h
            (fun hpq : p ∨ q =>
                Or.elim hpq
                (fun hp : p => Or.intro_left (q ∨ r) hp)
                (fun hq : q => Or.intro_right p (Or.intro_left r hq)))
            (fun hr : r => Or.intro_right p (Or.intro_right q hr))
        )
    have mpr :=
        (fun h : p ∨ (q ∨ r) =>
        show (p ∨ q) ∨ r from Or.elim h
            (fun hp : p => Or.intro_left r (Or.intro_left q hp))
            (fun hqr: q ∨ r => Or.elim hqr
                (fun hq : q => Or.intro_left r (Or.intro_right p hq))
                (fun hr : r => Or.intro_right (p ∨ q) hr)
                )
            )
    Iff.intro mp mpr

-- distributivity
theorem dist_and_or : p ∧ (q ∨ r) ↔ (p ∧ q) ∨ (p ∧ r) :=
    have mp :=
        (fun h : p ∧ (q ∨ r) => Or.elim h.right
            (fun hq : q => Or.intro_left (p ∧ r) (And.intro h.left hq))
            (fun hr : r => Or.intro_right (p ∧ q) (And.intro h.left hr))
            )
    have mpr :=
        (fun h : (p ∧ q) ∨ (p ∧ r) => Or.elim h
            (fun hpq : p ∧ q => And.intro hpq.left (Or.intro_left r hpq.right))
            (fun hpr : p ∧ r => And.intro hpr.left (Or.intro_right q hpr.right))
        )
    Iff.intro mp mpr

theorem dist_or_and : p ∨ (q ∧ r) ↔ (p ∨ q) ∧ (p ∨ r) :=
    have mp :=
        (fun h : p ∨ (q ∧ r) => Or.elim h
            (fun hp : p => And.intro (Or.intro_left q hp) (Or.intro_left r hp))
            (fun hqr : q ∧ r => And.intro (Or.intro_right p hqr.left) (Or.intro_right p hqr.right))
            )
    have mpr :=
        (fun h : (p ∨ q) ∧ (p ∨ r) => Or.elim h.left
            (fun hp : p => Or.intro_left (q ∧ r) hp)
            (fun hq : q => Or.elim h.right
                (fun hp : p => Or.intro_left (q ∧ r) hp)
                (fun hr : r => Or.intro_right p (And.intro hq hr))
                )
            )
    Iff.intro mp mpr

-- other properties
theorem imp_imp_iff_and_imp : (p → (q → r)) ↔ (p ∧ q → r) :=
    have mp :=
        (fun h : p → (q → r) =>
            fun hpq : (p ∧ q) =>
                h hpq.left hpq.right
        )
    have mpr :=
        (fun h : (p ∧ q) → r =>
            fun hp : p =>
            fun hq : q =>
            h (And.intro hp hq)
        )
    Iff.intro mp mpr
theorem dist_or_imp : ((p ∨ q) → r) ↔ (p → r) ∧ (q → r) :=
    have mp :=
        (fun h : (p ∨ q) → r => And.intro
            (fun hp : p => h (Or.intro_left q hp))
            (fun hq : q => h (Or.intro_right p hq))
        )
    have mpr :=
        (fun h : (p → r) ∧ (q → r) =>
            (fun hpq : (p ∨ q) => Or.elim hpq
                (fun hp : p => h.left hp)
                (fun hq : q => h.right hq)
            )
        )
    Iff.intro mp mpr
theorem de_morgan_or : ¬(p ∨ q) ↔ ¬p ∧ ¬q :=
    have mp :=
        (fun h : ¬(p ∨ q) =>
            have hnp : ¬p := (fun hp : p => h (Or.intro_left q hp))
            have hnq : ¬q := (fun hq : q => h (Or.intro_right p hq))
            And.intro hnp hnq
        )
    have mpr :=
        (fun h : ¬p ∧ ¬q =>
            have hnp : ¬p := h.left
            have hnq : ¬q := h.right
            (fun hpq : p ∨ q => Or.elim hpq
                (fun hp : p => hnp hp)
                (fun hq : q => hnq hq)
            )
        )
    Iff.intro mp mpr
theorem de_morgan_and : ¬p ∨ ¬q → ¬(p ∧ q) :=
    (fun h : ¬p ∨ ¬q => Or.elim h
        (fun hnp : ¬p =>
            fun hpq : (p ∧ q) => absurd hpq.left hnp
            )
        (fun hnq : ¬q =>
            fun hpq : (p ∧ q) => absurd hpq.right hnq
            )
    )
theorem not_and_not : ¬(p ∧ ¬p) :=
    fun h : p ∧ ¬p => absurd h.left h.right
theorem and_not_imp_not_imp : p ∧ ¬q → ¬(p → q) :=
    fun h : p ∧ ¬q =>
        have hp : p := h.left
        have hnq : ¬q := h.right
        fun hpq : p → q => absurd (hpq hp) hnq
theorem not_imp_imp : ¬p → (p → q) :=
    fun h : ¬p => fun hp : p => absurd hp h
theorem or_imp_imp : (¬p ∨ q) → (p → q) :=
    fun h : ¬ p ∨ q =>
        Or.elim h
        (fun hnp: ¬p => fun hp : p => absurd hp hnp)
        (fun hq : q => fun _ : p => hq)
theorem or_domination : p ∨ False ↔ p :=
    have mp :=
        fun h : p ∨ False => Or.elim h
            (fun hp : p => hp)
            (fun hf : False => False.elim hf)
    have mpr :=
        fun h : p => Or.intro_left False h
    Iff.intro mp mpr
theorem and_identity : p ∧ False ↔ False :=
    have mp := fun h : p ∧ False => h.right
    have mpr := fun h : False => False.elim h
    Iff.intro mp mpr
theorem contrapositive : (p → q) → (¬q → ¬p) :=
    fun h : p → q =>
        fun hnq : ¬q => fun hp : p => absurd (h hp) hnq

/- example : (p → q) → (¬q → ¬p) :=
fun h hnq hp => hnq (h hp) is equivalent! make sure we can handle both-/
