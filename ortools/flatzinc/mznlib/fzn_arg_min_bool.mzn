predicate ortools_arg_max_bool(array [int] of var bool: x, var int: z, int: min_index, int: multiplier);

predicate fzn_minimum_arg_bool(array [int] of var bool: x, var int: z) =
  ortools_arg_max_bool(x, z, min(index_set(x)), -1);
