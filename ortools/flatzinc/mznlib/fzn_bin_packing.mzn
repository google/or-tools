predicate ortools_bin_packing(
  int: c,
  array [int] of var int: bin,
  array [int] of int: w,
);

predicate fzn_bin_packing(int: c, array [int] of var int: bin, array [int] of int: w) =
  ortools_bin_packing(c, bin, w);

%-----------------------------------------------------------------------------%