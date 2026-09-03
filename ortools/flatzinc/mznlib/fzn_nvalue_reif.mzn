predicate ortools_nvalue(var int:n, array[int] of var int: x);

predicate fzn_nvalue_reif(var int: n, array [int] of var int: x, var bool: b) =
  let {
    int: lx = lb_array(x);
    int: ux = ub_array(x);
    var 1..ux-lx+1: c;
  } in b <-> n == c /\ ortools_nvalue(c, x)