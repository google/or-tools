% Ignore.
predicate symmetry_breaking_constraint(var bool: b) = b;

predicate redundant_constraint(var bool: b) = b;

% array_var_bool_element_nonshifted.
predicate ortools_array_var_bool_element(var int: idx,
                                         set of int: domain_of_x,
                                         array [int] of var bool: x,
                                         var bool: c);

predicate array_var_bool_element_nonshifted(var int: idx,
                                            array [int] of var bool: x,
                                            var bool: c) =
  ortools_array_var_bool_element(idx, index_set(x), x, c);

% array_var_int_element_nonshifted.
predicate ortools_array_var_int_element(var int: idx,
                                        set of int: domain_of_x,
                                        array [int] of var int: x,
                                        var int: c);

predicate array_var_int_element_nonshifted(var int: idx,
                                           array [int] of var int: x,
                                           var int: c) =
  ortools_array_var_int_element(idx, index_set(x), x, c);

predicate array_var_float_element_nonshifted(var int: idx, array[int] of var float: x, var float: c) =
  array_var_float_element((idx-(min(index_set(x))-1))::domain,array1d(x),c);

predicate array_var_set_element_nonshifted(var int: idx, array[int] of var set of int: x, var set of int: c) =
  array_var_set_element((idx-(min(index_set(x))-1))::domain,array1d(x),c);  
