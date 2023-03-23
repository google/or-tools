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

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/types/span.h"
#include "ortools/sat/cp_model.pb.h"

namespace operations_research {
namespace sat {

bool LiteralValue(int lit, absl::Span<const int64_t> solution);
int64_t ExprValue(const LinearExpressionProto& expr,
                  absl::Span<const int64_t> solution);

class LinearIncrementalEvaluator {
 public:
  LinearIncrementalEvaluator() = default;

  // Returns the index of the new constraint.
  int NewConstraint(int64_t lb, int64_t ub);

  // Incrementaly build the constraint.
  void AddEnforcementLiteral(int ct_index, int lit);
  void AddLiteral(int ct_index, int lit);
  void AddTerm(int ct_index, int var, int64_t coeff, int64_t offset = 0);
  void AddOffset(int ct_index, int64_t offset);

  // Compute activities and query violations.
  void ComputeInitialActivities(absl::Span<const int64_t> solution);
  void Update(int var, int64_t old_value, int64_t new_value);
  int64_t Violation(int ct_index) const;

  // Update constraint bounds.
  void ResetBounds(int ct_index, int64_t lb, int64_t ub);

  // Model getters.
  int num_constraints() const { return activities_.size(); }

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

  // Model data.
  std::vector<int> num_enforcement_literals_;
  std::vector<int64_t> lower_bounds_;
  std::vector<int64_t> upper_bounds_;

  // Column major data.
  std::vector<std::vector<Entry>> var_entries_;
  std::vector<std::vector<LiteralEntry>> literal_entries_;
  std::vector<int64_t> offsets_;

  // Dynamic data.
  std::vector<int64_t> activities_;
  std::vector<int> num_true_enforcement_literals_;
};

// View of a generic (non linear) constraint for the LsEvaluator.
class CompiledConstraint {
 public:
  explicit CompiledConstraint(const ConstraintProto& proto);
  virtual ~CompiledConstraint() = default;

  // Computes the violation of a constraint.
  //
  // A violation is a positive integer value. A zero value means the constraint
  // is not violated..
  virtual int64_t Violation(absl::Span<const int64_t> solution) = 0;

  // Violations are stored in a stack for each constraint.
  int64_t current_violation() const { return violations_.back(); }
  void push_violation(int64_t v) { violations_.push_back(v); }
  void pop_violation() { violations_.pop_back(); }
  void clear_violations() { violations_.clear(); }

  const ConstraintProto& proto() const { return proto_; }

 private:
  const ConstraintProto& proto_;
  std::vector<int64_t> violations_;
};

// Evaluation container for the local search.
class LsEvaluator {
 public:
  // The model must outlive this class.
  explicit LsEvaluator(const CpModelProto& model);

  // Overwrites the bounds of the objective.
  void SetObjectiveBounds(int64_t lb, int64_t ub);

  // Sets the current solution, and compute violations for each constraints.
  void ComputeInitialViolations(absl::Span<const int64_t> solution);

  // Update the value of one variable and recompute violations.
  void UpdateVariableValueAndRecomputeViolations(int var, int64_t value);

  // Simple summation metric for the constraint and objective violations.
  int64_t SumOfViolations();

 private:
  void CompileConstraintsAndObjective();
  void BuildVarConstraintGraph();

  const CpModelProto& model_;
  LinearIncrementalEvaluator linear_evaluator_;
  std::vector<std::unique_ptr<CompiledConstraint>> constraints_;
  std::vector<std::vector<int>> var_to_constraint_graph_;
  std::vector<int64_t> current_solution_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CONSTRAINT_VIOLATION_H_
