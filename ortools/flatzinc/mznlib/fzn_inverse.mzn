%-----------------------------------------------------------------------------%
% Constrains two arrays of int variables, f and invf, to represent inverse
% functions. All the values in each array must be within the index set of the
% other array.
%-----------------------------------------------------------------------------%

predicate fzn_inverse(array [int] of var int: f,
                      array [int] of var int: invf) =
  ortools_inverse(f, invf, min(index_set(f)), min(index_set(invf)));

predicate ortools_inverse(array [int] of var int: f,
                          array [int] of var int: invf,
                          int: base_f,
                          int: base_invf);

%-----------------------------------------------------------------------------%
%-----------------------------------------------------------------------------%
