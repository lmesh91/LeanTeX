theorem test : p ∧ q → q ∧ p :=
    fun h : p ∧ q =>
        show q ∧ p from And.intro h.right h.left
