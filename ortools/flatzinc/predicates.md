# FlatZinc Predicates and MiniZinc Rewrite Rules in OR-Tools

This document provides a complete summary of all MiniZinc (`.mzn`) rewrite
rules, solver redefinitions, and predicate declarations in
[ortools/flatzinc/mznlib](/ortools/flatzinc/mznlib),
along with their implementation status in
[checker.cc](/ortools/flatzinc/checker.cc)
and
[cp_model_fz_solver.cc](/ortools/flatzinc/cp_model_fz_solver.cc).

---

## 1. Custom OR-Tools FlatZinc Predicates (`ortools_*`)

These custom solver-specific FlatZinc predicates are generated when flattening
MiniZinc models using OR-Tools rewrite rules in `mznlib`:

| Source File | MiniZinc Rewrite Rule / Predicate | Target FlatZinc Predicate Generated |
| :--- | :--- | :--- |
| [fzn_arg_max_bool.mzn](/ortools/flatzinc/mznlib/fzn_arg_max_bool.mzn) | `fzn_maximum_arg_bool(x, z)` | `ortools_arg_max_bool(x, z, min(index_set(x)), 1)` |
| [fzn_arg_min_bool.mzn](/ortools/flatzinc/mznlib/fzn_arg_min_bool.mzn) | `fzn_minimum_arg_bool(x, z)` | `ortools_arg_max_bool(x, z, min(index_set(x)), -1)` |
| [fzn_arg_max_int.mzn](/ortools/flatzinc/mznlib/fzn_arg_max_int.mzn) | `fzn_maximum_arg_int(x, z)` | `ortools_arg_max_int(x, z, min(index_set(x)), 1)` |
| [fzn_arg_min_int.mzn](/ortools/flatzinc/mznlib/fzn_arg_min_int.mzn) | `fzn_minimum_arg_int(x, z)` | `ortools_arg_max_int(x, z, min(index_set(x)), -1)` |
| [redefinitions-2.0.2.mzn](/ortools/flatzinc/mznlib/redefinitions-2.0.2.mzn) | `array_var_bool_element_nonshifted(idx, x, c)` (const `x`) | `ortools_array_bool_element(idx, index_set(x), x, c)` |
| [redefinitions-2.0.2.mzn](/ortools/flatzinc/mznlib/redefinitions-2.0.2.mzn) | `array_var_bool_element_nonshifted(idx, x, c)` (var `x`) | `ortools_array_var_bool_element(idx, index_set(x), x, c)` |
| [redefinitions-2.0.2.mzn](/ortools/flatzinc/mznlib/redefinitions-2.0.2.mzn) | `array_var_int_element_nonshifted(idx, x, c)` (const `x`) | `ortools_array_int_element(idx, index_set(x), x, c)` |
| [redefinitions-2.0.2.mzn](/ortools/flatzinc/mznlib/redefinitions-2.0.2.mzn) | `array_var_int_element_nonshifted(idx, x, c)` (var `x`) | `ortools_array_var_int_element(idx, index_set(x), x, c)` |
| [redefinitions-2.0.2.mzn](/ortools/flatzinc/mznlib/redefinitions-2.0.2.mzn) | `array_var_set_element_nonshifted(idx, x, c)` | `ortools_array_var_set_element(idx, index_set(x), array1d(x), c)` |
| [fzn_bin_packing.mzn](/ortools/flatzinc/mznlib/fzn_bin_packing.mzn) | `fzn_bin_packing(c, bin, w)` | `ortools_bin_packing(c, bin, w)` |
| [fzn_bin_packing_capa.mzn](/ortools/flatzinc/mznlib/fzn_bin_packing_capa.mzn) | `fzn_bin_packing_capa(c, bin, w)` | `ortools_bin_packing_capa(c, bin, index_set(c), w)` |
| [fzn_bin_packing_load.mzn](/ortools/flatzinc/mznlib/fzn_bin_packing_load.mzn) | `fzn_bin_packing_load(load, bin, w)` | `ortools_bin_packing_load(load, bin, index_set(load), w)` |
| [fzn_circuit.mzn](/ortools/flatzinc/mznlib/fzn_circuit.mzn) | `fzn_circuit(x)` | `ortools_circuit(array1d(x), min(index_set(x)))` |
| [fzn_count_eq.mzn](/ortools/flatzinc/mznlib/fzn_count_eq.mzn) | `fzn_count_eq(x, y, c)` (const `y`) | `ortools_count_eq_cst(x, y, c)` |
| [fzn_count_eq.mzn](/ortools/flatzinc/mznlib/fzn_count_eq.mzn) | `fzn_count_eq(x, y, c)` (var `y`) | `ortools_count_eq(x, y, c)` |
| [fzn_cumulative_opt.mzn](/ortools/flatzinc/mznlib/fzn_cumulative_opt.mzn) | `fzn_cumulative_opt(s, d, r, b)` | `ortools_cumulative_opt(os, ds, d, r, b)` |
| [fzn_disjunctive_opt.mzn](/ortools/flatzinc/mznlib/fzn_disjunctive_opt.mzn) | `fzn_disjunctive_opt(s, d)` | `ortools_cumulative_opt(...)` (via `fzn_cumulative_opt`) |
| [fzn_disjunctive_strict_opt.mzn](/ortools/flatzinc/mznlib/fzn_disjunctive_strict_opt.mzn) | `fzn_disjunctive_strict_opt(s, d)` | `ortools_disjunctive_strict_opt(os, ds, d)` |
| [fzn_global_cardinality.mzn](/ortools/flatzinc/mznlib/fzn_global_cardinality.mzn) | `fzn_global_cardinality(x, cover, counts)` | `ortools_global_cardinality(x, cover, counts, false)` |
| [fzn_global_cardinality_closed.mzn](/ortools/flatzinc/mznlib/fzn_global_cardinality_closed.mzn) | `fzn_global_cardinality_closed(x, cover, counts)` | `ortools_global_cardinality(x, cover, counts, true)` |
| [fzn_global_cardinality_low_up.mzn](/ortools/flatzinc/mznlib/fzn_global_cardinality_low_up.mzn) | `fzn_global_cardinality_low_up(x, cover, lb, ub)` | `ortools_global_cardinality_low_up(x, cover, lb, ub, false)` |
| [fzn_global_cardinality_low_up_closed.mzn](/ortools/flatzinc/mznlib/fzn_global_cardinality_low_up_closed.mzn) | `fzn_global_cardinality_low_up_closed(x, cover, lb, ub)` | `ortools_global_cardinality_low_up(x, cover, lb, ub, true)` |
| [fzn_inverse.mzn](/ortools/flatzinc/mznlib/fzn_inverse.mzn) | `fzn_inverse(f, invf)` | `ortools_inverse(f, invf, min(index_set(f)), min(index_set(invf)))` |
| [fzn_lex_less_bool.mzn](/ortools/flatzinc/mznlib/fzn_lex_less_bool.mzn) | `fzn_lex_less_bool(x, y)` | `ortools_lex_less_bool(x, y)` |
| [fzn_lex_less_int.mzn](/ortools/flatzinc/mznlib/fzn_lex_less_int.mzn) | `fzn_lex_less_int(x, y)` | `ortools_lex_less_int(x, y)` |
| [fzn_lex_lesseq_bool.mzn](/ortools/flatzinc/mznlib/fzn_lex_lesseq_bool.mzn) | `fzn_lex_lesseq_bool(x, y)` | `ortools_lex_lesseq_bool(x, y)` |
| [fzn_lex_lesseq_int.mzn](/ortools/flatzinc/mznlib/fzn_lex_lesseq_int.mzn) | `fzn_lex_lesseq_int(x, y)` | `ortools_lex_lesseq_int(x, y)` |
| [fzn_network_flow.mzn](/ortools/flatzinc/mznlib/fzn_network_flow.mzn) | `fzn_network_flow(arc, balance, flow)` | `ortools_network_flow(array1d(arc), balance, min(index_set(balance)), flow)` |
| [fzn_network_flow_cost.mzn](/ortools/flatzinc/mznlib/fzn_network_flow_cost.mzn) | `fzn_network_flow_cost(arc, balance, weight, flow, cost)` | `ortools_network_flow_cost(array1d(arc), balance, min(index_set(balance)), flow, weight, cost)` |
| [fzn_nvalue.mzn](/ortools/flatzinc/mznlib/fzn_nvalue.mzn) | `fzn_nvalue(n, x)` | `ortools_nvalue(n, x)` |
| [fzn_nvalue_reif.mzn](/ortools/flatzinc/mznlib/fzn_nvalue_reif.mzn) | `fzn_nvalue_reif(n, x, b)` | `ortools_nvalue(c, x)` (with reification `b <-> n == c`) |
| [fzn_seq_precede_chain_int.mzn](/ortools/flatzinc/mznlib/fzn_seq_precede_chain_int.mzn) | `fzn_seq_precede_chain_int(X)` | `ortools_precede_chain_int(1..max(ub_array(X), 1), X)` |
| [fzn_value_precede_chain_int.mzn](/ortools/flatzinc/mznlib/fzn_value_precede_chain_int.mzn) | `fzn_value_precede_chain_int(T, X)` (length > 2) | `ortools_precede_chain_int(T, X)` |
| [fzn_regular.mzn](/ortools/flatzinc/mznlib/fzn_regular.mzn) | `fzn_regular(x, Q, S, d, q0, F)` | `ortools_regular(x, Q, S, array1d(d), q0, F)` |
| [fzn_subcircuit.mzn](/ortools/flatzinc/mznlib/fzn_subcircuit.mzn) | `fzn_subcircuit(x)` | `ortools_subcircuit(array1d(x), min(index_set(x)))` |
| [fzn_table_bool.mzn](/ortools/flatzinc/mznlib/fzn_table_bool.mzn) | `fzn_table_bool(x, t)` | `ortools_table_bool(x, array1d(t))` |
| [fzn_table_int.mzn](/ortools/flatzinc/mznlib/fzn_table_int.mzn) | `fzn_table_int(x, t)` | `ortools_table_int(x, array1d(t))` |

---

## 2. Standard FlatZinc Global & Built-in Predicates

These predicates are directly declared in `mznlib` as solver built-ins (without
body redefinitions) and are emitted directly to FlatZinc:

### Global Constraints (`fzn_*`)

- `fzn_all_different_int(array[int] of var int: x)` ([fzn_all_different_int.mzn](/ortools/flatzinc/mznlib/fzn_all_different_int.mzn))
- `fzn_all_different_set(array[int] of var set of int: x)` ([fzn_all_different_set.mzn](/ortools/flatzinc/mznlib/fzn_all_different_set.mzn))
- `fzn_all_disjoint(array[int] of var set of int: S)` ([fzn_all_disjoint.mzn](/ortools/flatzinc/mznlib/fzn_all_disjoint.mzn))
- `fzn_cumulative(array[int] of var int: s, array[int] of var int: d, array[int] of var int: r, var int: b)` ([fzn_cumulative.mzn](/ortools/flatzinc/mznlib/fzn_cumulative.mzn))
- `fzn_diffn(array[int] of var int: x, array[int] of var int: y, array[int] of var int: dx, array[int] of var int: dy)` ([fzn_diffn.mzn](/ortools/flatzinc/mznlib/fzn_diffn.mzn))
- `fzn_diffn_nonstrict(array[int] of var int: x, array[int] of var int: y, array[int] of var int: dx, array[int] of var int: dy)` ([fzn_diffn_nonstrict.mzn](/ortools/flatzinc/mznlib/fzn_diffn_nonstrict.mzn))
- `fzn_disjoint(var set of int: s1, var set of int: s2)` ([fzn_disjoint.mzn](/ortools/flatzinc/mznlib/fzn_disjoint.mzn))
- `fzn_disjunctive(array[int] of var int: s, array[int] of var int: d)` ([fzn_disjunctive.mzn](/ortools/flatzinc/mznlib/fzn_disjunctive.mzn))
- `fzn_disjunctive_strict(array[int] of var int: s, array[int] of var int: d)` ([fzn_disjunctive_strict.mzn](/ortools/flatzinc/mznlib/fzn_disjunctive_strict.mzn))
- `fzn_partition_set(array[int] of var set of int: S, set of int: universe)` ([fzn_partition_set.mzn](/ortools/flatzinc/mznlib/fzn_partition_set.mzn))
- `fzn_value_precede_int(int: s, int: t, array[int] of var int: x)` ([fzn_value_precede_int.mzn](/ortools/flatzinc/mznlib/fzn_value_precede_int.mzn))

### Standard Redefinitions & Half-Reified Constraints ([redefinitions-2.0.mzn](/ortools/flatzinc/mznlib/redefinitions-2.0.mzn))

- `array_int_maximum(var int: m, array[int] of var int: x)`
- `array_int_minimum(var int: m, array[int] of var int: x)`
- `array_float_maximum(var float: m, array[int] of var float: x)`
- `array_float_minimum(var float: m, array[int] of var float: x)`
- Half-reified integer comparison & linear constraints:
  - `int_eq_imp(var int: a, var int: b, var bool: r)`
  - `int_ge_imp(var int: a, var int: b, var bool: r)`
  - `int_gt_imp(var int: a, var int: b, var bool: r)`
  - `int_le_imp(var int: a, var int: b, var bool: r)`
  - `int_lt_imp(var int: a, var int: b, var bool: r)`
  - `int_ne_imp(var int: a, var int: b, var bool: r)`
  - `int_lin_eq_imp(array [int] of int: as, array [int] of var int: bs, int: c, var bool: r)`
  - `int_lin_ge_imp(array [int] of int: as, array [int] of var int: bs, int: c, var bool: r)`
  - `int_lin_gt_imp(array [int] of int: as, array [int] of var int: bs, int: c, var bool: r)`
  - `int_lin_le_imp(array [int] of int: as, array [int] of var int: bs, int: c, var bool: r)`
  - `int_lin_lt_imp(array [int] of int: as, array [int] of var int: bs, int: c, var bool: r)`
  - `int_lin_ne_imp(array [int] of int: as, array [int] of var int: bs, int: c, var bool: r)`

---

## 3. Pure Rewrite Rules (Decomposed Constraints)

These predicates are decomposed directly into primitive expressions or existing
FlatZinc constraints without introducing new solver built-ins:

- `fzn_all_equal_int(x)` ([fzn_all_equal_int.mzn](/ortools/flatzinc/mznlib/fzn_all_equal_int.mzn)) -> pairwise equality `x[min] = x[i]`
- `fzn_all_equal_set(x)` ([fzn_all_equal_set.mzn](/ortools/flatzinc/mznlib/fzn_all_equal_set.mzn)) -> pairwise set equality `x[min] = x[i]`
- `bool_clause_reif(as, bs, b)` ([redefinitions-2.0.mzn](/ortools/flatzinc/mznlib/redefinitions-2.0.mzn)) -> `clause(as, bs ++ [b]) /\ ...`
- `int_pow(x, y, r)` ([redefinitions-2.0.mzn](/ortools/flatzinc/mznlib/redefinitions-2.0.mzn)) -> `r == x*x`, `r == x*x*x`, or 2D array domain lookup
- `array_var_float_element_nonshifted(idx, x, c)` ([redefinitions-2.0.2.mzn](/ortools/flatzinc/mznlib/redefinitions-2.0.2.mzn)) -> `array_var_float_element(...)`
- **String Constraints ([nostrings.mzn](/ortools/flatzinc/mznlib/nostrings.mzn))**:
  - `str_eq`, `str_ne`, `str_nsub`, `str_range`, `str_chars`, `str_alphabet` -> mapped to integer array equality/domain constraints.
  - `str_lt`, `str_le`, `str_gr`, `str_ge` -> mapped to `lex_less`, `lex_lesseq`, `lex_greater`, `lex_greatereq`.
  - `str_gcc` -> mapped to `global_cardinality`.
  - `str_dfa` -> mapped to `regular`.
  - `str_nfa` -> mapped to `regular_nfa`.

---

## 4. Solver Annotations

- `symmetry_breaking_constraint(var bool: b)` -> `(b) :: symmetry_breaking` ([redefinitions-2.0.2.mzn](/ortools/flatzinc/mznlib/redefinitions-2.0.2.mzn))
- `redundant_constraint(var bool: b)` -> `(b) :: redundant` ([redefinitions-2.0.2.mzn](/ortools/flatzinc/mznlib/redefinitions-2.0.2.mzn))
- `annotation warm_start_array(array[int] of ann: w);` ([redefinitions-2.0.mzn](/ortools/flatzinc/mznlib/redefinitions-2.0.mzn))

---

## 5. Unused Non-Standard Predicates in C++ Codebase

This section lists all non-standard predicates (i.e. constraints that are
**not** standard FlatZinc 2.0 built-ins) supported in
[checker.cc](/ortools/flatzinc/checker.cc) or
[cp_model_fz_solver.cc](/ortools/flatzinc/cp_model_fz_solver.cc)
that are **unused** (i.e. not generated or emitted by the OR-Tools MiniZinc
library `mznlib`).

### 5.1 Legacy Global Constraints (Handled in `checker.cc` only)

These global constraints are legacy FlatZinc built-ins implemented in the
solution checker
([checker.cc](/ortools/flatzinc/checker.cc)),
but are decomposed or rewritten by `mznlib` and thus never generated by
OR-Tools MiniZinc flattening:

| Predicate Name | Where Supported | Description / Reason Unused in `mznlib` |
| :--- | :--- | :--- |
| `alldifferent_except_0` | [checker.cc](/ortools/flatzinc/checker.cc#L1895) | All non-zero integer variables are distinct. Decomposed by standard MiniZinc. |
| `among` | [checker.cc](/ortools/flatzinc/checker.cc#L1896) | Restricts count of variables taking values from a given set. Decomposed by MiniZinc. |
| `at_most_int` | [checker.cc](/ortools/flatzinc/checker.cc#L1909) | Restricts at most `n` variables to equal a specific value. |
| `count`, `count_eq`, `count_geq`, `count_gt`, `count_leq`, `count_lt`, `count_neq`, `count_reif` | [checker.cc](/ortools/flatzinc/checker.cc#L1938-L1945) | Legacy count predicates. `mznlib` rewrites count constraints to `ortools_count_eq` / `ortools_count_eq_cst` or linear constraints. |
| `diffn_k_with_sizes`, `diffn_nonstrict_k_with_sizes` | [checker.cc](/ortools/flatzinc/checker.cc#L1946-L1947) | Multi-dimensional non-overlapping box constraints. |
| `fixed_cumulative`, `var_cumulative`, `variable_cumulative` | [checker.cc](/ortools/flatzinc/checker.cc#L1949,L2062-L2063) | Legacy cumulative aliases. `mznlib` generates `fzn_cumulative` or `ortools_cumulative_opt`. |
| `regular_nfa` | [checker.cc](/ortools/flatzinc/checker.cc#L2038) | Regular language constraint using NFA transition tables. |
| `sliding_sum` | [checker.cc](/ortools/flatzinc/checker.cc#L2059) | Constrains the sum of every sliding sub-sequence of length `seq`. |
| `sort` | [checker.cc](/ortools/flatzinc/checker.cc#L2060) | Requires array `y` to be the sorted permutation of array `x`. |
| `symmetric_all_different` | [checker.cc](/ortools/flatzinc/checker.cc#L2061) | Self-inverse permutation constraint `x[x[i]] = i`. |

---

### 5.2 Legacy ArgMax, Element, and Set Predicates

| Predicate Name | Where Supported | Description / Reason Unused in `mznlib` |
| :--- | :--- | :--- |
| `maximum_arg_int`, `minimum_arg_int` | [checker.cc](/ortools/flatzinc/checker.cc#L2003,L2005) | Replaced in `mznlib` by `ortools_arg_max_int` / `ortools_arg_max_bool`. |
| `array_int_element_nonshifted` | [checker.cc](/ortools/flatzinc/checker.cc#L1902), [cp_model_fz_solver.cc](/ortools/flatzinc/cp_model_fz_solver.cc#L2605) | Legacy element constraint. `redefinitions-2.0.2.mzn` rewrites this to `ortools_array_int_element` or `ortools_array_var_int_element`. |
| `ortools_array_set_element` | [checker.cc](/ortools/flatzinc/checker.cc#L2011) | Set element constraint for constant set arrays. `redefinitions-2.0.2.mzn` generates `ortools_array_var_set_element`. |
| `int_in`, `int_not_in` | [checker.cc](/ortools/flatzinc/checker.cc#L1972,L1999) | Legacy integer set membership. Standard FlatZinc uses `set_in` / `set_in_reif`. |
| `set_in_negated` | [cp_model_fz_solver.cc](/ortools/flatzinc/cp_model_fz_solver.cc#L2588) | Internal solver constraint type produced during solver preprocessing for negated set membership. |
| `false_constraint` | [checker.cc](/ortools/flatzinc/checker.cc#L1948), [cp_model_fz_solver.cc](/ortools/flatzinc/cp_model_fz_solver.cc#L2560) | Represents an always-false constraint emitted for trivially unsatisfiable FlatZinc problems. |

---

### 5.3 Legacy Boolean Half-Reification & Implication Predicates

These half-reified boolean implications are implemented in
[checker.cc](/ortools/flatzinc/checker.cc),
but are commented out in
[redefinitions-2.0.mzn](/ortools/flatzinc/mznlib/redefinitions-2.0.mzn)
because MiniZinc decomposes half-reified booleans into standard clauses:

| Predicate Name | Where Supported | Description |
| :--- | :--- | :--- |
| `bool_eq_imp(a, b, r)` | [checker.cc](/ortools/flatzinc/checker.cc#L1912) | Implication `r -> (a = b)` |
| `bool_ge_imp(a, b, r)` | [checker.cc](/ortools/flatzinc/checker.cc#L1915) | Implication `r -> (a >= b)` |
| `bool_gt_imp(a, b, r)` | [checker.cc](/ortools/flatzinc/checker.cc#L1918) | Implication `r -> (a > b)` |
| `bool_le_imp(a, b, r)` | [checker.cc](/ortools/flatzinc/checker.cc#L1921) | Implication `r -> (a <= b)` |
| `bool_lt_imp(a, b, r)` | [checker.cc](/ortools/flatzinc/checker.cc#L1927) | Implication `r -> (a < b)` |
| `bool_ne_imp(a, b, r)` | [checker.cc](/ortools/flatzinc/checker.cc#L1930) | Implication `r -> (a != b)` |
| `bool_left_imp(a, b)` | [checker.cc](/ortools/flatzinc/checker.cc#L1924) | Implication `a -> b` (mapped to `int_le`) |
| `bool_right_imp(a, b)` | [checker.cc](/ortools/flatzinc/checker.cc#L1935) | Implication `b -> a` (mapped to `int_ge`) |

---

### 5.4 Predicates Declared in `mznlib` but Unsupported in Solvers

| Predicate Name | Declared In | Status in Solvers |
| :--- | :--- | :--- |
| `array_float_maximum`, `array_float_minimum` | [redefinitions-2.0.mzn](/ortools/flatzinc/mznlib/redefinitions-2.0.mzn#L15-L16) | Explicitly marked `% Not supported` in `mznlib`. Not implemented in CP-SAT solver or checker. |
| `ortools_array_var_set_element` | [redefinitions-2.0.2.mzn](/ortools/flatzinc/mznlib/redefinitions-2.0.2.mzn#L57) | Implemented in [checker.cc](/ortools/flatzinc/checker.cc#L2014), but not registered in [cp_model_fz_solver.cc](/ortools/flatzinc/cp_model_fz_solver.cc)'s `kConstraintMap`. |
| Commented-out half-reified constraints (`bool_clause_imp`, `bool_lin_eq_imp`, `bool_lin_le_imp`, etc.) | [redefinitions-2.0.mzn](/ortools/flatzinc/mznlib/redefinitions-2.0.mzn#L48-L75) | Commented out in `mznlib` for performance / lack of emission from MiniZinc. |

