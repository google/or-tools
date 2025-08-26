predicate ortools_bin_packing_capa(
  array [int] of int: c,
  array [int] of var int: bin,
  set of int: c_set,
  array [int] of int: w,
);

predicate fzn_bin_packing_capa(
  array [int] of int: c,
  array [int] of var int: bin,
  array [int] of int: w,
) = ortools_bin_packing_capa(c, bin, index_set(c), w);

%-----------------------------------------------------------------------------%