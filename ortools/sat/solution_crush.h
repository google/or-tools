// Copyright 2010-2025 Google LLC
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef ORTOOLS_SAT_SOLUTION_CRUSH_H_
#define ORTOOLS_SAT_SOLUTION_CRUSH_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/inlined_vector.h"
#include "absl/types/span.h"
#include "ortools/algorithms/sparse_permutation.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/diffn_util.h"
#include "ortools/sat/util.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {

// Transforms (or "crushes") solutions of the initial model into solutions of
// the presolved model.
//
// Note that partial solution crushing is not a priority: as of Jan 2025, most
// methods of this class do nothing if some solution values are missing to
// perform their work. If one just want to complete a partial solution to a full
// one for convenience, it should be relatively easy to first solve a
// feasibility model where all hinted variables are fixed, and use the solution
// to that problem as a starting hint.
//
// Note also that if the initial "solution" is incomplete or infeasible, the
// crushed "solution" might contain values outside of the domain of their
// variables. Consider for instance two constraints "b => v=1" and "!b => v=2",
// presolved into "v = b+1", with SetVarToLinearConstraintSolution called to set
// b's value from v's value. If the initial solution is infeasible, with v=0,
// this will set b to -1, which is outside of its [0,1] domain.
class SolutionCrush {
 public:
  SolutionCrush() = default;

  // SolutionCrush is neither copyable nor movable.
  SolutionCrush(const SolutionCrush&) = delete;
  SolutionCrush(SolutionCrush&&) = delete;
  SolutionCrush& operator=(const SolutionCrush&) = delete;
  SolutionCrush& operator=(SolutionCrush&&) = delete;

  bool SolutionIsLoaded() const { return solution_is_loaded_; }

  // Visible for testing.
  absl::Span<const int64_t> GetVarValues() const { return var_values_; }

  // Sets the given values in the solution. `solution` must be a map from
  // variable indices to variable values. This must be called only once, before
  // any other method.
  void LoadSolution(int num_vars,
                    const absl::flat_hash_map<int, int64_t>& solution);

  // Resizes the solution to contain `new_size` variables. Does not change the
  // value of existing variables, and does not set any value for the new
  // variables.
  // WARNING: the methods below do not automatically resize the solution. To set
  // the value of a new variable with one of them, call this method first.
  void Resize(int new_size);

  // Sets the value of `literal` to "`var`'s value == `value`". Does nothing if
  // `literal` already has a value.
  void MaybeSetLiteralToValueEncoding(int literal, int var, int64_t value);

  // Sets the value of `literal` to "`var`'s value >=/<= `value`". Does nothing
  // if `literal` already has a value.
  void MaybeSetLiteralToOrderEncoding(int literal, int var, int64_t value,
                                      bool is_le);

  // Sets the value of `var` to the value of the given linear expression, if all
  // the variables in this expression have a value. `linear` must be a list of
  // (variable index, coefficient) pairs.
  void SetVarToLinearExpression(
      int var, absl::Span<const std::pair<int, int64_t>> linear,
      int64_t offset = 0);

  // Sets the value of `var` to the value of the given linear expression.
  // The two spans must have the same size.
  void SetVarToLinearExpression(int var, absl::Span<const int> vars,
                                absl::Span<const int64_t> coeffs,
                                int64_t offset = 0);

  // Sets the value of `var` to 1 if the value of at least one literal in
  // `clause` is equal to 1 (or to 0 otherwise). `clause` must be a list of
  // literal indices.
  void SetVarToClause(int var, absl::Span<const int> clause);

  // Sets the value of `var` to 1 if the value of all the literals in
  // `conjunction` is 1 (or to 0 otherwise). `conjunction` must be a list of
  // literal indices.
  void SetVarToConjunction(int var, absl::Span<const int> conjunction);

  // Sets the value of `var` to `value` if the value of the given linear
  // expression is not in `domain` (or does nothing otherwise). `linear` must be
  // a list of (variable index, coefficient) pairs.
  void SetVarToValueIfLinearConstraintViolated(
      int var, int64_t value, absl::Span<const std::pair<int, int64_t>> linear,
      const Domain& domain);

  // Sets the value of `literal` to `value` if the value of the given linear
  // expression is not in `domain` (or does nothing otherwise). `linear` must be
  // a list of (variable index, coefficient) pairs.
  void SetLiteralToValueIfLinearConstraintViolated(
      int literal, bool value, absl::Span<const std::pair<int, int64_t>> linear,
      const Domain& domain);

  // Sets the value of `var` to `value` if the value of `condition_lit` is true.
  void SetVarToValueIf(int var, int64_t value, int condition_lit);

  // Sets the value of `var` to the value `expr` if the value of `condition_lit`
  // is true.
  void SetVarToLinearExpressionIf(int var, const LinearExpressionProto& expr,
                                  int condition_lit);

  // Sets the value of `literal` to `value` if the value of `condition_lit` is
  // true.
  void SetLiteralToValueIf(int literal, bool value, int condition_lit);

  // Sets the value of `var` to `value_if_true` if the value of all the
  // `condition_lits` literals is true, and to `value_if_false` otherwise.
  void SetVarToConditionalValue(int var, absl::Span<const int> condition_lits,
                                int64_t value_if_true, int64_t value_if_false);

  // If one literal does not have a value, and the other one does, sets the
  // value of the latter to the value of the former. If both literals have a
  // value, sets the value of `lit1` to the value of `lit2`.
  void MakeLiteralsEqual(int lit1, int lit2);

  // If `var` already has a value, updates it to be within the given domain.
  // Otherwise, if the domain is fixed, sets the value of `var` to this fixed
  // value. Otherwise does nothing.
  void SetOrUpdateVarToDomain(int var, const Domain& domain);

  // If `var` already has a value, updates it to be within the given domain.
  // There are 3 cases to consider:
  // 1/ The hinted value is in reduced_var_domain. Nothing to do.
  // 2/ The hinted value is not in the domain, and there is an escape value.
  //    Update the hinted value to the escape value, and update the encoding
  //    literals to reflect the new value of `var`.
  // 3/ The hinted value is not in the domain, and there is no escape value.
  //    Update the hinted value to be in the domain by pushing it in the given
  //    direction, and update the encoding literals to reflect the new value
  void SetOrUpdateVarToDomainWithOptionalEscapeValue(
      int var, const Domain& reduced_var_domain,
      std::optional<int64_t> unique_escape_value,
      bool push_down_when_not_in_domain,
      const absl::btree_map<int64_t, int>& encoding);

  // Updates the value of the given literals to false if their current values
  // are different (or does nothing otherwise).
  void UpdateLiteralsToFalseIfDifferent(int lit1, int lit2);

  // Decrements the value of `lit` and increments the value of `dominating_lit`
  // if their values are equal to 1 and 0, respectively.
  void UpdateLiteralsWithDominance(int lit, int dominating_lit);

  // Decrements the value of `ref` by the minimum amount necessary to be in
  // [`min_value`, `max_value`], and increments the value of one or more
  // `dominating_refs` by the same total amount (or less if it is not possible
  // to exactly match this amount), while staying within their respective
  // domains. The value of a negative reference index r is the opposite of the
  // value of the variable PositiveRef(r).
  //
  // `min_value` must be the minimum value of `ref`'s current domain D, and
  // `max_value` must be in D.
  void UpdateRefsWithDominance(
      int ref, int64_t min_value, int64_t max_value,
      absl::Span<const std::pair<int, Domain>> dominating_refs);

  // If `var`'s value != `value` finds another variable in the orbit of `var`
  // that can take that value, and permute the solution (using the symmetry
  // `generators`) so that this other variable is at position var. If no other
  // variable can be found, does nothing.
  void MaybeUpdateVarWithSymmetriesToValue(
      int var, bool value,
      absl::Span<const std::unique_ptr<SparsePermutation>> generators);

  // If at most one literal in `orbitope[row]` is equal to `value`, and if this
  // literal is in a column 'col' > `pivot_col`, swaps the value of all the
  // literals in columns 'col' and `pivot_col` (if they all have a value).
  // Otherwise does nothing.
  void MaybeSwapOrbitopeColumns(absl::Span<const std::vector<int>> orbitope,
                                int row, int pivot_col, bool value);

  // Sets the value of the i-th variable in `vars` so that the given constraint
  // "dotproduct(coeffs, vars values) = rhs" is satisfied, if all the other
  // variables have a value, and if the constraint is enforced (otherwise it
  // sets the unset variables to their `default_values`). i is equal to
  // `var_index` if set. Otherwise it is the index of the variable without a
  // value (if there is not exactly one, this method does nothing).
  void SetVarToLinearConstraintSolution(
      absl::Span<const int> enforcement_lits, std::optional<int> var_index,
      absl::Span<const int> vars, absl::Span<const int64_t> default_values,
      absl::Span<const int64_t> coeffs, int64_t rhs);

  // Sets the value of the variables in `level_vars` and in `circuit` if all the
  // variables in `reservoir` have a value. This assumes that there is one level
  // variable and one circuit node per element in `reservoir` (in the same
  // order) -- plus one last node representing the start and end of the circuit.
  void SetReservoirCircuitVars(const ReservoirConstraintProto& reservoir,
                               int64_t min_level, int64_t max_level,
                               absl::Span<const int> level_vars,
                               const CircuitConstraintProto& circuit);

  // Sets the value of `var` to "`time_i`'s value <= `time_j`'s value &&
  // `active_i`'s value == true && `active_j`'s value == true".
  void SetVarToReifiedPrecedenceLiteral(int var,
                                        const LinearExpressionProto& time_i,
                                        const LinearExpressionProto& time_j,
                                        int active_i, int active_j);

  // Sets the value of `div_var` and `prod_var` if all the variables in the
  // IntMod `ct` constraint have a value, assuming that this "target = x % m"
  // constraint is expanded into "div_var = x / m", "prod_var = div_var * m",
  // and "target = x - prod_var" constraints. If `ct` is not enforced, sets the
  // values of `div_var` and `prod_var` to `default_div_value` and
  // `default_prod_value`, respectively.
  void SetIntModExpandedVars(const ConstraintProto& ct, int div_var,
                             int prod_var, int64_t default_div_value,
                             int64_t default_prod_value);

  // Sets the value of as many variables in `prod_vars` as possible (depending
  // on how many expressions in `int_prod` have a value), assuming that the
  // `int_prod` constraint "target = x_0 * x_1 * ... * x_n" is expanded into
  // "prod_var_1 = x_0 * x1",
  // "prod_var_2 = prod_var_1 * x_2",
  //  ...,
  // "prod_var_(n-1) = prod_var_(n-2) * x_(n-1)",
  // and "target = prod_var_(n-1) * x_n" constraints.
  void SetIntProdExpandedVars(const LinearArgumentProto& int_prod,
                              absl::Span<const int> prod_vars);

  // Sets the value of `enforcement_lits` if all the variables in `lin_max`
  // have a value, assuming that the `lin_max` constraint "target = max(x_0,
  // x_1, ..., x_(n-1))" is expanded into "enforcement_lits[i] => target <= x_i"
  // constraints, with at most one enforcement value equal to true.
  // `enforcement_lits` must have as many elements as `lin_max`.
  void SetLinMaxExpandedVars(const LinearArgumentProto& lin_max,
                             absl::Span<const int> enforcement_lits);

  // Represents `var` = "automaton is in state `state` at time `time`".
  struct StateVar {
    int var;
    int time;
    int64_t state;
  };

  // Represents `var` = "automaton takes the transition labeled
  // `transition_label` from state `transition_tail` at time `time`".
  struct TransitionVar {
    int var;
    int time;
    int64_t transition_tail;
    int64_t transition_label;
  };

  // Sets the value of `state_vars` and `transition_vars` if all the variables
  // in `automaton` have a value.
  void SetAutomatonExpandedVars(
      const AutomatonConstraintProto& automaton,
      absl::Span<const StateVar> state_vars,
      absl::Span<const TransitionVar> transition_vars);

  // Represents `lit` = "for all i, the value of the i-th column var of a table
  // constraint is in the `var_values`[i] set (unless this set is empty).".
  struct TableRowLiteral {
    int lit;
    // TODO(user): use a vector of (var, value) pairs instead?
    std::vector<absl::InlinedVector<int64_t, 2>> var_values;
  };

  // Sets the value of the `new_row_lits` literals if all the variables in
  // `column_vars` and `existing_row_lits` have a value. For each `row_lits`,
  // `column_values` must have the same size as `column_vars`. This method
  // assumes that exactly one of `existing_row_lits` and `new_row_lits` must be
  // true.
  void SetTableExpandedVars(absl::Span<const int> column_vars,
                            absl::Span<const int> existing_row_lits,
                            absl::Span<const TableRowLiteral> new_row_lits);

  // Sets the value of `bucket_lits` if all the variables in `linear` have a
  // value, assuming that they are expanded from the complex linear constraint
  // (i.e. one whose domain has two or more intervals). The value of
  // `bucket_lits`[i] is set to 1 iff the value of the linear expression is in
  // the i-th interval of the domain.
  void SetLinearWithComplexDomainExpandedVars(
      const LinearConstraintProto& linear, absl::Span<const int> bucket_lits);

  // Stores the solution as a hint in the given model.
  void StoreSolutionAsHint(CpModelProto& model) const;

  // Given a list of N disjoint packing areas (each described by a union of
  // rectangles) and a list of M boxes (described by their x and y interval
  // constraints in the `model` proto), sets the value of the literals in
  // `box_in_area_lits` with whether the box i intersects area j.
  struct BoxInAreaLiteral {
    int box_index;
    int area_index;
    int literal;
  };
  void AssignVariableToPackingArea(
      const CompactVectorVector<int, Rectangle>& areas,
      const CpModelProto& model, absl::Span<const int> x_intervals,
      absl::Span<const int> y_intervals,
      absl::Span<const BoxInAreaLiteral> box_in_area_lits);

 private:
  bool HasValue(int var) const { return var_has_value_[var]; }

  int64_t GetVarValue(int var) const { return var_values_[var]; }

  bool GetLiteralValue(int lit) const {
    const int var = PositiveRef(lit);
    return RefIsPositive(lit) ? GetVarValue(var) : !GetVarValue(var);
  }

  std::optional<int64_t> GetRefValue(int ref) const {
    const int var = PositiveRef(ref);
    if (!HasValue(var)) return std::nullopt;
    return RefIsPositive(ref) ? GetVarValue(var) : -GetVarValue(var);
  }

  std::optional<int64_t> GetExpressionValue(
      const LinearExpressionProto& expr) const {
    int64_t result = expr.offset();
    for (int i = 0; i < expr.vars().size(); ++i) {
      if (expr.coeffs(i) == 0) continue;
      if (!HasValue(expr.vars(i))) return std::nullopt;
      result += expr.coeffs(i) * GetVarValue(expr.vars(i));
    }
    return result;
  }

  void SetVarValue(int var, int64_t value) {
    var_has_value_[var] = true;
    var_values_[var] = value;
  }

  void SetLiteralValue(int lit, bool value) {
    SetVarValue(PositiveRef(lit), RefIsPositive(lit) == value ? 1 : 0);
  }

  void SetRefValue(int ref, int value) {
    SetVarValue(PositiveRef(ref), RefIsPositive(ref) ? value : -value);
  }

  void PermuteVariables(const SparsePermutation& permutation);

  bool solution_is_loaded_ = false;
  std::vector<bool> var_has_value_;
  // This contains all the solution values or zero if a solution is not loaded.
  std::vector<int64_t> var_values_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_SOLUTION_CRUSH_H_
