%-----------------------------------------------------------------------------%
% Constrains the elements of 'x' to define a circuit where 'x[i] = j' means
% that 'j' is the successor of 'i'.
%-----------------------------------------------------------------------------%

predicate fzn_circuit(array[int] of var int: x) =
  ortools_circuit(array1d(x), min(index_set(x)));

predicate ortools_circuit(array[int] of var int: x, int: base);

%-----------------------------------------------------------------------------%
%-----------------------------------------------------------------------------%
