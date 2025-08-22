predicate ortools_bin_packing_load(
  array [int] of var int: load,
  array [int] of var int: bin,
  set of int: load_set,
  array [int] of int: w,
);

predicate fzn_bin_packing_load(
  array [int] of var int: load,
  array [int] of var int: bin,
  array [int] of int: w,
) = ortools_bin_packing_load(load, bin, index_set(load), w);
%-----------------------------------------------------------------------------%
