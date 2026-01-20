variable (p q r : Prop)

theorem contrapositive_2 : (p → q) → (¬q → ¬p) :=
    fun h hnq hp => hnq (h hp)
