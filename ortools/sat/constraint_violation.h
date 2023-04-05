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
#include <vector>

#include "absl/types/span.h"
#include "ortools/sat/cp_model.pb.h"
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
  void AddEnforcementLiteral(int ct_index, int lit);
  void AddLiteral(int ct_index, int lit);
  void AddTerm(int ct_index, int var, int64_t coeff, int64_t offset = 0);
  void AddOffset(int ct_index, int64_t offset);
  void AddLinearExpression(int ct_index, const LinearExpressionProto& expr,
                           int64_t multiplier);

  // Important: this needs to be called after all constraint has been added
  // and before the class starts to be used. This is DCHECKed.
  void PrecomputeCompactView();

  // Compute activities and query violations.
  void ComputeInitialActivities(absl::Span<const int64_t> solution);
  void Update(int var, int64_t delta);
  int64_t Activity(int c) const;
  int64_t Violation(int c) const;

  // Used to DCHECK the state of the evaluator.
  bool VarIsConsistent(int var) const;

  bool AppearsInViolatedConstraints(int var) const;

  // Intersect constraint bounds with [lb..ub].
  void ReduceBounds(int c, int64_t lb, int64_t ub);

  // Model getters.
  int num_constraints() const { return num_constraints_; }

  // Returns the weighted sum of the violation.
  // Note that weights might have more entries than the number of constraints.
  double WeightedViolation(absl::Span<const double> weights) const;

  // Computes how much the weighted violation will change if var was changed
  // from its old value by += delta.
  double WeightedViolationDelta(absl::Span<const double> weights, int var,
                                int64_t delta) const;

  // The violation for each constraints is a piecewise linear function. This
  // computes and aggregate all the breakpoints for the given variable and its
  // domain.
  //
  // Note that if the domain contains less than two values, we return an empty
  // vector. This fonction is not meant to be used for such domains.
  std::vector<int64_t> SlopeBreakpoints(int var, int64_t current_value,
                                        const Domain& var_domain) const;

  // AffectedVariableOnUpdate() return the set of variables that have at least
  // one constraint in common with the given variable. One need to call
  // PrecomputeCompactView() after all the constraint have been added for
  // this to work.
  std::vector<int> AffectedVariableOnUpdate(int var);
  std::vector<int> ConstraintsToAffectedVariables(
      absl::Span<const int> ct_indices);

  double DeterministicTime() const {
    return 5e-9 * static_cast<double>(dtime_);
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

  // Constraint indexed data (static).
  int num_constraints_ = 0;
  std::vector<Domain> domains_;
  std::vector<int64_t> offsets_;

  // Variable indexed data.
  // Note that this is just used at construction and is replaced by a compact
  // view when PrecomputeCompactView() is called.
  std::vector<std::vector<Entry>> var_entries_;
  std::vector<std::vector<LiteralEntry>> literal_entries_;

  // Memory efficient column based data.
  struct VarData {
    int start = 0;
    int num_pos_literal = 0;
    int num_neg_literal = 0;
    int linear_start = 0;
    int num_linear_entries = 0;
  };
  absl::Span<const int> VarToConstraints(int var) const {
    const VarData& data = columns_[var];
    const int size =
        data.num_pos_literal + data.num_neg_literal + data.num_linear_entries;
    return absl::MakeSpan(&ct_buffer_[data.start], size);
  }
  std::vector<VarData> columns_;
  std::vector<int> ct_buffer_;
  std::vector<int64_t> coeff_buffer_;

  // Transposed view data (only variables).
  bool creation_phase_ = true;
  std::vector<int> row_buffer_;
  std::vector<absl::Span<const int>> rows_;

  // Temporary data.
  std::vector<int> tmp_row_sizes_;
  std::vector<bool> tmp_in_result_;

  // Constraint indexed data (dynamic).
  std::vector<int64_t> activities_;
  std::vector<int64_t> distances_;
  std::vector<int> num_false_enforcement_;
  std::vector<bool> row_update_will_not_impact_deltas_;

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

  // Computes the violation of a constraint.
  //
  // A violation is a positive integer value. A zero value means the constraint
  // is not violated..
  virtual int64_t Violation(absl::Span<const int64_t> solution) = 0;

  void CacheViolation(absl::Span<const int64_t> solution);

  // Cache the violation of a constraint.
  int64_t violation() const;

  const ConstraintProto& ct_proto() const;

 private:
  const ConstraintProto& ct_proto_;
  int64_t violation_ = -1;
};

// Evaluation container for the local search.
//
// TODO(user): Ideas for constraint generated moves or sequences of moves?
class LsEvaluator {
 public:
  // The model must outlive this class.
  explicit LsEvaluator(const CpModelProto& model);

  // For now, we don't support all constraints. You can still use the class, but
  // it is not because there is no violation that the solution will be feasible
  // in this case.
  //
  // TODO(user): On the miplib, some presolve add all_diff constraints which are
  // not actually needed to be maintained here to have a feasible solution. Find
  // a fix for that.
  bool ModelIsSupported() const { return model_is_supported_; }

  // Intersects the domain of the objective with [lb..ub].
  void ReduceObjectiveBounds(int64_t lb, int64_t ub);

  // Sets the current solution, and compute violations for each constraints.
  void ComputeInitialViolations(absl::Span<const int64_t> solution);

  // Recompute the violations of non linear constraints.
  void UpdateNonLinearViolations();

  // Update the value of one variable and recompute violations.
  void UpdateVariableValueAndRecomputeViolations(int var, int64_t value,
                                                 bool focus_on_linear = false);

  // Simple summation metric for the constraint and objective violations.
  int64_t SumOfViolations();

  // Returns the objective activity in the current state.
  int64_t ObjectiveActivity() const;

  // The number of "internal" constraints in the evaluator. This might not
  // be the same as the number of initial constraints of the model.
  int NumLinearConstraints() const;
  int NumNonLinearConstraints() const;
  int NumEvaluatorConstraints() const;
  int NumInfeasibleConstraints() const;

  // Returns the weighted sum of violation. The weights must have the same
  // size as NumEvaluatorConstraints().
  int64_t Violation(int c) const;
  double WeightedViolation(absl::Span<const double> weights) const;
  double WeightedViolationDelta(absl::Span<const double> weights, int var,
                                int64_t delta) const;

  LinearIncrementalEvaluator* MutableLinearEvaluator() {
    return &linear_evaluator_;
  }

  // Returns the set of variables appearing in a violated constraint.
  std::vector<int> VariablesInViolatedConstraints() const;

  // Counters: number of times UpdateVariableValueAndRecomputeViolations() has
  // been called.
  int64_t num_deltas_computed() const { return num_deltas_computed_; }

  // Access the solution stored. Useful for debugging.
  const std::vector<int64_t>& current_solution() const {
    return current_solution_;
  }

 private:
  void CompileConstraintsAndObjective();
  void BuildVarConstraintGraph();

  const CpModelProto& model_;
  LinearIncrementalEvaluator linear_evaluator_;
  std::vector<std::unique_ptr<CompiledConstraint>> constraints_;
  std::vector<std::vector<int>> var_to_constraint_graph_;
  // We need the mutable to evaluate a move.
  mutable std::vector<int64_t> current_solution_;
  bool model_is_supported_ = true;
  mutable int64_t num_deltas_computed_ = 0;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CONSTRAINT_VIOLATION_H_
