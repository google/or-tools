% This file contains redefinitions of standard builtins that can be overridden
% by solvers.

predicate bool_clause_reif(array[int] of var bool: as,
                           array[int] of var bool: bs,
                           var bool: b) =
  clause(as,bs++[b]) /\
  forall (i in index_set(as)) (as[i] -> b) /\
  forall (i in index_set(bs)) (bs[i] \/ b);

predicate array_int_maximum(var int: m, array[int] of var int: x);
predicate array_int_minimum(var int: m, array[int] of var int: x);

% Not supported
predicate array_float_maximum(var float: m, array[int] of var float: x);
predicate array_float_minimum(var float: m, array[int] of var float: x);

% Define int_pow for CP solvers
% r = x ^ y
predicate int_pow( var int: x, var int: y, var int: r ) =
  if is_fixed(y) /\ 2==fix(y) then r==x*x
  elseif is_fixed(y) /\ 3==fix(y) then r==x*x*x
  else
    let {
      array[ int, int ] of int: x2y = array2d( lb(x)..ub(x), lb(y)..ub(y),
        [ pow( X, Y ) | X in lb(x)..ub(x), Y in lb(y)..ub(y) ] )
    } in
      r == x2y[ x, y ]
  endif;

% Non shifted elements
predicate array_var_bool_element_nonshifted(var int: idx,
                                            array [int] of var bool: x,
                                            var bool: c);

predicate array_var_int_element_nonshifted(var int: idx,
                                           array [int] of var int: x,
                                           var int: c);

% Include set redefinitions.
include "nosets.mzn";

% Include strings redefinitions
% include "nostring.mzn"

% Half-Reified constraints
% Some of the predicated are commented out because:
%  - we do not know if they are ever emited.
%  - not sure they will improve the solver.
% Keeping them for completeness sake.
%predicate array_bool_and_imp(array [int] of var bool: as, var bool: r);
%predicate array_bool_or_imp(array [int] of var bool: as, var bool: r);
%predicate array_bool_xor_imp(array [int] of var bool: as, var bool: r);

%predicate bool_and_imp(var bool: a, var bool: b, var bool: r);
%predicate bool_clause_imp(array [int] of var bool: as,
%                          array [int] of var bool: bs, var bool: r);
%predicate bool_ge_imp(var bool: a, var bool: b, var bool: r);
%predicate bool_gt_imp(var bool: a, var bool: b, var bool: r);
%predicate bool_le_imp(var bool: a, var bool: b, var bool: r);
%predicate bool_lt_imp(var bool: a, var bool: b, var bool: r);
%predicate bool_ne_imp(var bool: a, var bool: b, var bool: r);
%predicate bool_ne_imp(var bool: a, var bool: b, var bool: r);
%predicate bool_or_imp(var bool: a, var bool: b, var bool: r);
%predicate bool_xor_imp(var bool: a, var bool: b, var bool: r);

%predicate bool_lin_eq_imp(array [int] of int: as, array [int] of var bool: bs,
%                          var int: c, var bool: r);
%predicate bool_lin_ge_imp(array [int] of int: as, array [int] of var bool: bs,
%                          var int: c, var bool: r);
%predicate bool_lin_gt_imp(array [int] of int: as, array [int] of var bool: bs,
%                          var int: c, var bool: r);
%predicate bool_lin_le_imp(array [int] of int: as, array [int] of var bool: bs,
%                          var int: c, var bool: r);
%predicate bool_lin_lt_imp(array [int] of int: as, array [int] of var bool: bs,
%                          var int: c, var bool: r);
%predicate bool_lin_ne_imp(array [int] of int: as, array [int] of var bool: bs,
%                          var int: c, var bool: r);

predicate int_eq_imp(var int: a, var int: b, var bool: r);
predicate int_ge_imp(var int: a, var int: b, var bool: r);
predicate int_gt_imp(var int: a, var int: b, var bool: r);
predicate int_le_imp(var int: a, var int: b, var bool: r);
predicate int_lt_imp(var int: a, var int: b, var bool: r);
predicate int_ne_imp(var int: a, var int: b, var bool: r);

predicate int_lin_eq_imp(array [int] of int: as, array [int] of var int: bs,
                         int: c, var bool: r);
predicate int_lin_ge_imp(array [int] of int: as, array [int] of var int: bs,
                         int: c, var bool: r);
predicate int_lin_gt_imp(array [int] of int: as, array [int] of var int: bs,
                         int: c, var bool: r);
predicate int_lin_le_imp(array [int] of int: as, array [int] of var int: bs,
                         int: c, var bool: r);
predicate int_lin_lt_imp(array [int] of int: as, array [int] of var int: bs,
                         int: c, var bool: r);
predicate int_lin_ne_imp(array [int] of int: as, array [int] of var int: bs,
                         int: c, var bool: r);
