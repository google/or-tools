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
#include <functional>
#include <limits>
#include <memory>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "ortools/sat/cp_model.pb.h"

namespace operations_research {
namespace sat {

class LsEvaluator;

bool LiteralValue(int lit, absl::Span<const int64_t> solution);
int64_t ExprValue(const LinearExpressionProto& expr,
                  absl ::Span<const int64_t> solution);

class LinearIncrementalEvaluator {
 public:
  struct Entry {
    int ct_index;
    int64_t coefficient;
  };

  struct LiteralEntry {
    int ct_index;
    bool positive;
  };

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
  void Update(int var, int64_t old_value, absl::Span<const int64_t> solution);
  int64_t Violation(int ct_index) const;

  // Manage the objective.
  void ResetObjectiveBounds(int64_t lb, int64_t ub);

  // Model getters.
  int num_constraints() const { return activities_.size(); }

 private:
  // Model data.
  // TODO(user): We should store the constraint proto here instead of the
  // enforcement literals. Just a bit problematic with the objective.
  std::vector<std::vector<int>> enforcement_literals_;
  std::vector<int64_t> lower_bounds_;
  std::vector<int64_t> upper_bounds_;

  // Column major data.
  std::vector<std::vector<Entry>> var_entries_;
  std::vector<std::vector<LiteralEntry>> literal_entries_;
  std::vector<int64_t> offsets_;

  // Dynamic data.
  std::vector<int64_t> activities_;
  std::vector<bool> enforced_;
};

// View of a generic (non linear) constraint for the LsEvaluator.
class CompiledConstraint {
 public:
  explicit CompiledConstraint() = default;
  virtual ~CompiledConstraint() = default;

  // Evaluation.
  virtual int64_t Evaluate(absl::Span<const int64_t> solution) = 0;

  // Utilities to compute the var <-> constraint graph.
  virtual void CallOnEachVariable(std::function<void(int)> func) const = 0;

  // Violations are stored in a stack for each constraint.
  int64_t current_violation() const { return violations_.back(); }
  void push_violation(int64_t v) { violations_.push_back(v); }
  void pop_violation() { violations_.pop_back(); }
  void clear_violations() { violations_.clear(); }

 private:
  std::vector<int64_t> violations_;
};

// The violation of a lin_max constraint is:
// - the sum(max(0, expr_value - target_value) forall expr)
// - min(target_value - expr_value for all expr) if the above sum is 0
class CompiledLinMaxConstraint : public CompiledConstraint {
 public:
  explicit CompiledLinMaxConstraint(const LinearArgumentProto& proto);
  ~CompiledLinMaxConstraint() override = default;

  int64_t Evaluate(absl::Span<const int64_t> solution) override;
  void CallOnEachVariable(std::function<void(int)> func) const override;

 private:
  const LinearArgumentProto& proto_;
};

// The violation of an int_prod constraint is
//     abs(target_value - prod(expr value)).
class CompiledProductConstraint : public CompiledConstraint {
 public:
  explicit CompiledProductConstraint(const LinearArgumentProto& proto);
  ~CompiledProductConstraint() override = default;

  int64_t Evaluate(absl::Span<const int64_t> solution) override;
  void CallOnEachVariable(std::function<void(int)> func) const override;

 private:
  const LinearArgumentProto& proto_;
};

// Evaluation container for the local search.
class LsEvaluator {
 public:
  explicit LsEvaluator(const CpModelProto& model);

  // Assigns the variable domains from variables_only to the current storage.
  void UpdateVariableDomains(const CpModelProto& variables_only);

  // Compiles the current model into a efficient representation.
  void CompileModel();

  // Overwrites the bounds of the objective.
  void SetObjectiveBounds(int64_t lb, int64_t ub);

  // Sets the current solution, and compute violations for each constraints.
  void ComputeInitialViolations(absl::Span<const int64_t> solution);

  // Update the value of one variable and recompute violations.
  void UpdateVariableValueAndRecomputeViolations(int var, int64_t value);

  // Simple summation metric for the constraint and objective violations.
  int64_t SumOfViolations();

  // Getters for variables using the domains set with UpdateVariableDomains().
  bool VariableIsFixed(int ref) const;
  int64_t VariableMin(int var) const;
  int64_t VariableMax(int var) const;
  int64_t VariableValue(int var) const;
  bool LiteralValue(int lit) const;
  bool LiteralIsFalse(int lit) const;
  bool LiteralIsTrue(int lit) const;

 private:
  void CompileConstraintsAndObjective();
  void BuildVarConstraintGraph();

  CpModelProto model_;
  LinearIncrementalEvaluator linear_evaluator_;
  std::vector<std::unique_ptr<CompiledConstraint>> constraints_;
  std::vector<std::vector<int>> var_to_constraint_graph_;
  std::vector<int64_t> current_solution_;
  absl::flat_hash_set<int> tmp_vars_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CONSTRAINT_VIOLATION_H_
