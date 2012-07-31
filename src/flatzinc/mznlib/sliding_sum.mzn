%-----------------------------------------------------------------------------%
% Requires that in each subsequence 'vs[i], ..., vs[i + seq - 1]' the sum of the
% values belongs to the interval ['low', 'up'].
%-----------------------------------------------------------------------------%

predicate sliding_sum(int: low, int: up, int: seq, array[int] of var int: vs) =
    let { int: lx = min(index_set(vs)),
          int: ux = max(index_set(vs)),
        } in
        forall (i in lx .. ux - seq + 1) (
            let {
                    var int: sum_of_l = sum(j in i..i + seq - 1) (vs[j])
            } in
                low <= sum_of_l /\ sum_of_l <= up
        );
