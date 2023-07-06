predicate fzn_disjunctive_opt(array[int] of var opt int: s,
                              array[int] of var int: d) =
        forall(i in index_set(d))(d[i] >= 0)
    /\  fzn_cumulative_opt(s, d, [1 | i in index_set(s)], 1);