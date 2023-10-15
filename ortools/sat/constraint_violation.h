// Copyright 2010-2022 Google LLC
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

#ifndef OR_TOOLS_SAT_CONSTRAINT_VIOLATION_H_
#define OR_TOOLS_SAT_CONSTRAINT_VIOLATION_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "absl/types/span.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {

int64_t ExprValue(const LinearExpressionProto& expr,
                  absl::Span<const int64_t> solution);

class LinearIncrementalEvaluator {
 public:
  LinearIncrementalEvaluator() = default;

  // Returns the index of the new constraint.
  int NewConstraint(Domain domain);

  // Incrementaly build the constraint.
  //
  // In case of duplicate variables on the same constraint, the code assumes
  // constraints are built in-order as it checks for duplication.
  //
  // Note that the code assume that a Boolean variable DO NOT appear both in the
  // enforcement list and in the constraint. Otherwise the update will just be
  // wrong. This should be enforced by our presolve.
  void AddEnforcementLiteral(int ct_index, int lit);
  void AddLiteral(int ct_index, int lit, int64_t coeff = 1);
  void AddTerm(int ct_index, int var, int64_t coeff, int64_t offset = 0);
  void AddOffset(int ct_index, int64_t offset);
  void AddLinearExpression(int ct_index, const LinearExpressionProto& expr,
                           int64_t multiplier);

  // Important: this needs to be called after all constraint has been added
  // and before the class starts to be used. This is DCHECKed.
  void PrecomputeCompactView(absl::Span<const int64_t> var_max_variation);

  // Compute activities and update them.
  void ComputeInitialActivities(absl::Span<const int64_t> solution);
  void Update(int var, int64_t delta,
              std::vector<std::pair<int, int64_t>>* violation_deltas = nullptr);

  // Function specific to the feasibility jump heuristic.
  // Note that the score of the changed variable will not be updated correctly!
  void UpdateVariableAndScores(
      int var, int64_t delta, absl::Span<const double> weights,
      absl::Span<const int64_t> jump_deltas, absl::Span<double> jump_scores,
      std::vector<std::pair<int, int64_t>>* violation_deltas = nullptr);

  // Also for feasibility jump.
  void UpdateScoreOnWeightUpdate(int c, absl::Span<const int64_t> jump_deltas,
                                 absl::Span<double> var_to_score_change);

  // Variables whose score might have decreased since the last clear for at
  // least one jump value.
  //
  // Note that because we reason on a per-constraint basis, this is actually
  // independent of the set of positive constraint weight used. We just list
  // variable for which the contribution to the violation of one constraint
  // might have decreased for any jump value.
  void ClearAffectedVariables();
  const std::vector<int>& VariablesAffectedByLastUpdate() const {
    return last_affected_variables_;
  }

  // Query violation.
  int64_t Activity(int c) const;
  int64_t Violation(int c) const;
  bool IsViolated(int c) const;
  bool AppearsInViolatedConstraints(int var) const;

  // Used to DCHECK the state of the evaluator.
  bool VarIsConsistent(int var) const;

  // Intersect constraint bounds with [lb..ub].
  // It returns true if a reduction of the domain took place.
  bool ReduceBounds(int c, int64_t lb, int64_t ub);

  // Model getters.
  int num_constraints() const { return num_constraints_; }

  // Returns the weighted sum of the violation.
  // Note that weights might have more entries than the number of constraints.
  double WeightedViolation(absl::Span<const double> weights) const;

  // Computes how much the weighted violation will change if var was changed
  // from its old value by += delta.
  double WeightedViolationDelta(absl::Span<const double> weights, int var,
                                int64_t delta) const;

  // The violation for each constraint is a piecewise linear function. This
  // computes and aggregates all the breakpoints for the given variable and its
  // domain.
  //
  // Note that if the domain contains less than two values, we return an empty
  // vector. This function is not meant to be used for such domains.
  std::vector<int64_t> SlopeBreakpoints(int var, int64_t current_value,
                                        const Domain& var_domain) const;

  // Checks if the jump value of a variable is always optimal.
  bool ViolationChangeIsConvex(int var) const;

  double DeterministicTime() const {
    return 5e-9 * static_cast<double>(dtime_);
  }

  int64_t ObjectiveDelta(int var, int64_t delta) const {
    if (var >= var_entries_.size()) return 0;
    const std::vector<Entry>& data = var_entries_[var];
    if (data.empty()) return 0;
    if (data[0].ct_index != 0) return 0;
    return data[0].coefficient * delta;
  }

  absl::Span<const int> ConstraintToVars(int c) const {
    const SpanData& data = rows_[c];
    const int size =
        data.num_pos_literal + data.num_neg_literal + data.num_linear_entries;
    if (size == 0) return {};
    return absl::MakeSpan(&row_var_buffer_[data.start], size);
  }

 private:
  // Cell in the sparse matrix.
  struct Entry {
    int ct_index;
    int64_t coefficient;
  };

  // Column-view of the enforcement literals.
  struct LiteralEntry {
    int ct_index;
    bool positive;  // bool_var or its negation.
  };

  struct SpanData {
    int start = 0;
    int num_pos_literal = 0;
    int num_neg_literal = 0;
    int linear_start = 0;
    int num_linear_entries = 0;
  };

  absl::Span<const int> VarToConstraints(int var) const {
    if (var >= columns_.size()) return {};
    const SpanData& data = columns_[var];
    const int size =
        data.num_pos_literal + data.num_neg_literal + data.num_linear_entries;
    if (size == 0) return {};
    return absl::MakeSpan(&ct_buffer_[data.start], size);
  }

  void ComputeAndCacheDistance(int ct_index);

  // Incremental row-based update.
  void UpdateScoreOnNewlyEnforced(int c, double weight,
                                  absl::Span<const int64_t> jump_deltas,
                                  absl::Span<double> jump_scores);
  void UpdateScoreOnNewlyUnenforced(int c, double weight,
                                    absl::Span<const int64_t> jump_deltas,
                                    absl::Span<double> jump_scores);
  void UpdateScoreOfEnforcementIncrease(int c, double score_change,
                                        absl::Span<const int64_t> jump_deltas,
                                        absl::Span<double> jump_scores);
  void UpdateScoreOnActivityChange(int c, double weight, int64_t activity_delta,
                                   absl::Span<const int64_t> jump_deltas,
                                   absl::Span<double> jump_scores);

  // Constraint indexed data (static).
  int num_constraints_ = 0;
  std::vector<Domain> domains_;
  std::vector<int64_t> offsets_;

  // Variable indexed data.
  // Note that this is just used at construction and is replaced by a compact
  // view when PrecomputeCompactView() is called.
  bool creation_phase_ = true;
  std::vector<std::vector<Entry>> var_entries_;
  std::vector<std::vector<LiteralEntry>> literal_entries_;

  // Memory efficient column based data (static).
  std::vector<SpanData> columns_;
  std::vector<int> ct_buffer_;
  std::vector<int64_t> coeff_buffer_;

  // Memory efficient row based data (static).
  std::vector<SpanData> rows_;
  std::vector<int> row_var_buffer_;
  std::vector<int64_t> row_coeff_buffer_;

  // In order to avoid scanning long constraint we compute for each of them
  // the maximum activity variation of one variable (max-min) * abs(coeff).
  // If the current activity plus this is still feasible, then the constraint
  // do not need to be scanned.
  std::vector<int64_t> row_max_variations_;

  // Temporary data.
  std::vector<int> tmp_row_sizes_;
  std::vector<int> tmp_row_num_positive_literals_;
  std::vector<int> tmp_row_num_negative_literals_;
  std::vector<int> tmp_row_num_linear_entries_;

  // Constraint indexed data (dynamic).
  std::vector<bool> is_violated_;
  std::vector<int64_t> activities_;
  std::vector<int64_t> distances_;
  std::vector<int> num_false_enforcement_;

  // Code to update the scores on a variable change.
  std::vector<int64_t> old_distances_;
  std::vector<int> old_num_false_enforcement_;
  std::vector<int64_t> cached_deltas_;
  std::vector<double> cached_scores_;

  std::vector<bool> in_last_affected_variables_;
  std::vector<int> last_affected_variables_;

  mutable size_t dtime_ = 0;
};

// View of a generic (non linear) constraint for the LsEvaluator.
//
// TODO(user): Do we add a Update(solution, var, new_value) method ?
// TODO(user): Do we want to support Update(solutions, vars, new_values) ?
class CompiledConstraint {
 public:
  explicit CompiledConstraint(const ConstraintProto& ct_proto);
  virtual ~CompiledConstraint() = default;

  // Recomputes the violation of the constraint from scratch.
  void InitializeViolation(absl::Span<const int64_t> solution);

  // Update the violation with the new value.
  void PerformMove(int var, int64_t old_value,
                   absl::Span<const int64_t> solution_with_new_value);

  // Computes the violation of a constraint.
  //
  // A violation is a positive integer value. A zero value means the constraint
  // is not violated.
  virtual int64_t ComputeViolation(absl::Span<const int64_t> solution) = 0;

  // Returns the delta if var changes from old_value to solution[var].
  virtual int64_t ViolationDelta(
      int var, int64_t old_value,
      absl::Span<const int64_t> solution_with_new_value);

  // Getters.
  const ConstraintProto& ct_proto() const { return ct_proto_; }
  int64_t violation() const { return violation_; }

 private:
  const ConstraintProto& ct_proto_;
  int64_t violation_;
};

// Evaluation container for the local search.
//
// TODO(user): Ideas for constraint generated moves or sequences of moves?

// Note(user): This class do not handle ALL constraint yet. So it is not because
// there is no violation here that the solution will be feasible. It is
// important to check feasibility once this is used. Note that in practice, we
// can be lucky, and feasible on a subset of hard constraint is enough.
class LsEvaluator {
 public:
  // The cp_model must outlive this class.
  LsEvaluator(const CpModelProto& cp_model, const SatParameters& params);
  LsEvaluator(const CpModelProto& cp_model, const SatParameters& params,
              const std::vector<bool>& ignored_constraints,
              const std::vector<ConstraintProto>& additional_constraints);

  // Intersects the domain of the objective with [lb..ub].
  // It returns true if a reduction of the domain took place.
  bool ReduceObjectiveBounds(int64_t lb, int64_t ub);

  // Overwrites the current solution.
  void OverwriteCurrentSolution(absl::Span<const int64_t> solution);

  // Computes the violations of all constraints.
  void ComputeAllViolations();

  // Recompute the violations of non linear constraints.
  void UpdateAllNonLinearViolations();

  // Sets the value of the variable in the current solution.
  // It must be called after UpdateLinearScores().
  void UpdateVariableValue(int var, int64_t new_value);

  // Recomputes the violations of all impacted non linear constraints.
  void UpdateNonLinearViolations(int var, int64_t new_value);

  // Function specific to the linear only feasibility jump.
  void UpdateLinearScores(int var, int64_t value,
                          absl::Span<const double> weights,
                          absl::Span<const int64_t> jump_deltas,
                          absl::Span<double> jump_scores);
  const std::vector<int>& VariablesAffectedByLastLinearUpdate() const {
    return linear_evaluator_.VariablesAffectedByLastUpdate();
  }

  // Simple summation metric for the constraint and objective violations.
  int64_t SumOfViolations();

  // Returns the objective activity in the current state.
  int64_t ObjectiveActivity() const;

  int64_t ObjectiveDelta(int var, int64_t delta) const {
    return cp_model_.has_objective()
               ? linear_evaluator_.ObjectiveDelta(var, delta)
               : 0;
  }

  // The number of "internal" constraints in the evaluator. This might not
  // be the same as the number of initial constraints of the model.
  int NumLinearConstraints() const;
  int NumNonLinearConstraints() const;
  int NumEvaluatorConstraints() const;
  int NumInfeasibleConstraints() const;

  // Returns the weighted sum of violation. The weights must have the same
  // size as NumEvaluatorConstraints().
  int64_t Violation(int c) const;
  bool IsViolated(int c) const;
  double WeightedViolation(absl::Span<const double> weights) const;
  double WeightedViolationDelta(absl::Span<const double> weights, int var,
                                int64_t delta) const;
  // Ignores the violations of the linear constraints.
  double WeightedNonLinearViolationDelta(absl::Span<const double> weights,
                                         int var, int64_t delta) const;

  const LinearIncrementalEvaluator& LinearEvaluator() {
    return linear_evaluator_;
  }
  LinearIncrementalEvaluator* MutableLinearEvaluator() {
    return &linear_evaluator_;
  }

  // List of the currently violated constraints.
  // - It is initialized by RecomputeViolatedList()
  // - And incrementally maintained by UpdateVariableValue()
  //
  // The order depends on the algorithm used and shouldn't be relied on.
  void RecomputeViolatedList(bool linear_only);
  const std::vector<int>& ViolatedConstraints() const {
    return violated_constraints_;
  }
  // Returns the number of constraints in ViolatedConstraints containing `var`.
  int NumViolatedConstraintsForVar(int var) const {
    return num_violated_constraint_per_var_[var];
  }
  // Indicates if the computed jump value is always the best choice.
  bool VariableOnlyInLinearConstraintWithConvexViolationChange(int var) const;

  // Access the solution stored.
  const std::vector<int64_t>& current_solution() const {
    return current_solution_;
  }

  std::vector<int64_t>* mutable_current_solution() {
    return &current_solution_;
  }

  const std::vector<std::pair<int, int64_t>>& last_update_violation_changes()
      const {
    return last_update_violation_changes_;
  }

  absl::Span<const int> ConstraintToVars(int c) const {
    if (c < NumLinearConstraints()) {
      return linear_evaluator_.ConstraintToVars(c);
    }
    return absl::MakeConstSpan(constraint_to_vars_[c - NumLinearConstraints()]);
  }

  // Note that the constraint indexing is different here than in the other
  // functions.
  absl::Span<const int> VarToGeneralConstraints(int var) const {
    return var_to_constraints_[var];
  }
  absl::Span<const int> GeneralConstraintToVars(int general_c) const {
    return constraint_to_vars_[general_c];
  }

  // TODO(user): Properly account all big time consumers.
  double DeterministicTime() const {
    return linear_evaluator_.DeterministicTime();
  }

 private:
  void CompileConstraintsAndObjective(
      const std::vector<bool>& ignored_constraints,
      const std::vector<ConstraintProto>& additional_constraints);

  void CompileOneConstraint(const ConstraintProto& ct_proto);
  void BuildVarConstraintGraph();
  void UpdateViolatedList(int c);

  const CpModelProto& cp_model_;
  const SatParameters& params_;
  CpModelProto expanded_constraints_;
  LinearIncrementalEvaluator linear_evaluator_;
  std::vector<std::unique_ptr<CompiledConstraint>> constraints_;
  std::vector<std::vector<int>> var_to_constraints_;
  std::vector<std::vector<int>> constraint_to_vars_;
  std::vector<bool> jump_value_optimal_;

  // We need the mutable to evaluate a move by temporarily modifying solution.
  mutable std::vector<int64_t> current_solution_;

  // List of violated constraints.
  // Invariants:
  // - pos_in_violated_constraints_[c] == -1 iff c not int violated_constraints_
  // - violated_constraints_[pos_in_violated_constraints_[c]] = c
  std::vector<int> pos_in_violated_constraints_;
  std::vector<int> violated_constraints_;
  std::vector<int> num_violated_constraint_per_var_;

  // Constraint index and violation delta for the last update.
  std::vector<std::pair<int, int64_t>> last_update_violation_changes_;
};

// ================================
// Individual compiled constraints.
// ================================

// The violation of a bool_xor constraint is 0 or 1.
class CompiledBoolXorConstraint : public CompiledConstraint {
 public:
  explicit CompiledBoolXorConstraint(const ConstraintProto& ct_proto);
  ~CompiledBoolXorConstraint() override = default;

  int64_t ComputeViolation(absl::Span<const int64_t> solution) override;
  int64_t ViolationDelta(
      int /*var*/, int64_t /*old_value*/,
      absl::Span<const int64_t> solution_with_new_value) override;
};

// The violation of a lin_max constraint is:
// - the sum(max(0, expr_value - target_value) forall expr). This part will be
//   maintained by the linear part.
// - target_value - max(expressions) if positive.
class CompiledLinMaxConstraint : public CompiledConstraint {
 public:
  explicit CompiledLinMaxConstraint(const ConstraintProto& ct_proto);
  ~CompiledLinMaxConstraint() override = default;

  int64_t ComputeViolation(absl::Span<const int64_t> solution) override;
};

// The violation of an int_prod constraint is
//     abs(value(target) - prod(value(expr)).
class CompiledIntProdConstraint : public CompiledConstraint {
 public:
  explicit CompiledIntProdConstraint(const ConstraintProto& ct_proto);
  ~CompiledIntProdConstraint() override = default;

  int64_t ComputeViolation(absl::Span<const int64_t> solution) override;
};

// The violation of an int_div constraint is
//     abs(value(target) - value(expr0) / value(expr1)).
class CompiledIntDivConstraint : public CompiledConstraint {
 public:
  explicit CompiledIntDivConstraint(const ConstraintProto& ct_proto);
  ~CompiledIntDivConstraint() override = default;

  int64_t ComputeViolation(absl::Span<const int64_t> solution) override;
};

// The violation of a all_diff is the number of unordered pairs of expressions
// with the same value.
class CompiledAllDiffConstraint : public CompiledConstraint {
 public:
  explicit CompiledAllDiffConstraint(const ConstraintProto& ct_proto);
  ~CompiledAllDiffConstraint() override = default;

  int64_t ComputeViolation(absl::Span<const int64_t> solution) override;

 private:
  std::vector<int64_t> values_;
};

// The violation of a no_overlap is the sum of overloads over time.
class CompiledNoOverlapConstraint : public CompiledConstraint {
 public:
  explicit CompiledNoOverlapConstraint(const ConstraintProto& ct_proto,
                                       const CpModelProto& cp_model);
  ~CompiledNoOverlapConstraint() override = default;

  int64_t ComputeViolation(absl::Span<const int64_t> solution) override;

 private:
  const CpModelProto& cp_model_;
  std::vector<std::pair<int64_t, int64_t>> events_;
};

// The violation of a cumulative is the sum of overloads over time.
class CompiledCumulativeConstraint : public CompiledConstraint {
 public:
  explicit CompiledCumulativeConstraint(const ConstraintProto& ct_proto,
                                        const CpModelProto& cp_model);
  ~CompiledCumulativeConstraint() override = default;

  int64_t ComputeViolation(absl::Span<const int64_t> solution) override;

 private:
  const CpModelProto& cp_model_;
  std::vector<std::pair<int64_t, int64_t>> events_;
};

// The violation of a no_overlap is the sum of overloads over time.
class CompiledNoOverlap2dConstraint : public CompiledConstraint {
 public:
  explicit CompiledNoOverlap2dConstraint(const ConstraintProto& ct_proto,
                                         const CpModelProto& cp_model);
  ~CompiledNoOverlap2dConstraint() override = default;

  int64_t ComputeViolation(absl::Span<const int64_t> solution) override;

 private:
  int64_t ComputeOverlapArea(absl::Span<const int64_t> solution, int i,
                             int j) const;
  const CpModelProto& cp_model_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CONSTRAINT_VIOLATION_H_
