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

// Interface to dedicated knapsack solvers covering multi-dimensional 0-1
// knapsacks.
// Current solvers handle only integer coefficients so a scaling phase is
// performed before solving the problem.
// TODO(user): handle timeouts, compute row and column statuses.

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "absl/base/attributes.h"
#include "ortools/algorithms/knapsack_solver.h"
#include "ortools/linear_solver/linear_solver.h"
#include "ortools/util/fp_utils.h"

namespace operations_research {

class KnapsackInterface : public MPSolverInterface {
 public:
  explicit KnapsackInterface(MPSolver* solver);
  ~KnapsackInterface() override;

  // ----- Solve -----
  MPSolver::ResultStatus Solve(const MPSolverParameters& param) override;

  // ----- Model modifications and extraction -----
  void Reset() override;
  void SetOptimizationDirection(bool maximize) override;
  void SetVariableBounds(int index, double lb, double ub) override;
  void SetVariableInteger(int index, bool integer) override;
  void SetConstraintBounds(int index, double lb, double ub) override;
  void AddRowConstraint(MPConstraint* ct) override;
  void AddVariable(MPVariable* var) override;
  void SetCoefficient(MPConstraint* constraint, const MPVariable* variable,
                      double new_value, double old_value) override;
  void ClearConstraint(MPConstraint* constraint) override;
  void SetObjectiveCoefficient(const MPVariable* variable,
                               double coefficient) override;
  void SetObjectiveOffset(double value) override;
  void ClearObjective() override;

  // ------ Query statistics on the solution and the solve ------
  int64_t iterations() const override;
  int64_t nodes() const override;
  MPSolver::BasisStatus row_status(int constraint_index) const override;
  MPSolver::BasisStatus column_status(int variable_index) const override;

  // ----- Misc -----
  bool IsContinuous() const override;
  bool IsLP() const override;
  bool IsMIP() const override;

  std::string SolverVersion() const override;
  void* underlying_solver() override;

  void ExtractNewVariables() override;
  void ExtractNewConstraints() override;
  void ExtractObjective() override;

  void SetParameters(const MPSolverParameters& param) override;
  void SetRelativeMipGap(double value) override;
  void SetPrimalTolerance(double value) override;
  void SetDualTolerance(double value) override;
  void SetPresolveMode(int value) override;
  void SetScalingMode(int value) override;
  void SetLpAlgorithm(int value) override;

 private:
  bool IsKnapsackModel() const;
  bool IsVariableFixedToValue(const MPVariable* var, double value) const;
  bool IsVariableFixed(const MPVariable* var) const;
  double GetVariableValueFromSolution(const MPVariable* var) const;
  void NonIncrementalChange() { sync_status_ = MUST_RELOAD; }

  std::unique_ptr<KnapsackSolver> knapsack_solver_;
  std::vector<int64_t> profits_;
  std::vector<std::vector<int64_t>> weights_;
  std::vector<int64_t> capacities_;
};

KnapsackInterface::KnapsackInterface(MPSolver* solver)
    : MPSolverInterface(solver) {}

KnapsackInterface::~KnapsackInterface() {}

MPSolver::ResultStatus KnapsackInterface::Solve(
    const MPSolverParameters& param) {
  Reset();
  if (!IsKnapsackModel()) {
    LOG(ERROR) << "Model is not a knapsack model";
    result_status_ = MPSolver::MODEL_INVALID;
    return MPSolver::MODEL_INVALID;
  }
  ExtractModel();
  SetParameters(param);
  sync_status_ = SOLUTION_SYNCHRONIZED;
  // TODO(user): Refine Analysis of the model to choose better solvers.
  KnapsackSolver::SolverType solver_type =
      KnapsackSolver::KNAPSACK_MULTIDIMENSION_BRANCH_AND_BOUND_SOLVER;
  if (profits_.size() <= 64 && capacities_.size() == 1) {
    solver_type = KnapsackSolver::KNAPSACK_64ITEMS_SOLVER;
  }
  knapsack_solver_ =
      std::make_unique<KnapsackSolver>(solver_type, "linear_solver");
  const double time_limit_seconds =
      solver_->time_limit()
          ? (static_cast<double>(solver_->time_limit()) / 1000.0)
          : std::numeric_limits<double>::infinity();
  knapsack_solver_->set_time_limit(time_limit_seconds);
  knapsack_solver_->Init(profits_, weights_, capacities_);
  knapsack_solver_->Solve();
  result_status_ = knapsack_solver_->IsSolutionOptimal() ? MPSolver::OPTIMAL
                                                         : MPSolver::FEASIBLE;
  objective_value_ = solver_->objective_->offset();
  for (int var_id = 0; var_id < solver_->variables_.size(); ++var_id) {
    MPVariable* const var = solver_->variables_[var_id];
    const double value = GetVariableValueFromSolution(var);
    objective_value_ += value * solver_->objective_->GetCoefficient(var);
    var->set_solution_value(value);
  }
  return result_status_;
}

void KnapsackInterface::Reset() {
  ResetExtractionInformation();
  profits_.clear();
  weights_.clear();
  capacities_.clear();
  knapsack_solver_.reset(nullptr);
}

void KnapsackInterface::SetOptimizationDirection(bool maximize) {
  NonIncrementalChange();
}

void KnapsackInterface::SetVariableBounds(int index, double lb, double ub) {
  NonIncrementalChange();
}

void KnapsackInterface::SetVariableInteger(int index, bool integer) {
  NonIncrementalChange();
}

void KnapsackInterface::SetConstraintBounds(int index, double lb, double ub) {
  NonIncrementalChange();
}

void KnapsackInterface::AddRowConstraint(MPConstraint* const ct) {
  NonIncrementalChange();
}

void KnapsackInterface::AddVariable(MPVariable* const var) {
  NonIncrementalChange();
}

void KnapsackInterface::SetCoefficient(MPConstraint* const constraint,
                                       const MPVariable* const variable,
                                       double new_value, double old_value) {
  NonIncrementalChange();
}

void KnapsackInterface::ClearConstraint(MPConstraint* const constraint) {
  NonIncrementalChange();
}

void KnapsackInterface::SetObjectiveCoefficient(
    const MPVariable* const variable, double coefficient) {
  NonIncrementalChange();
}

void KnapsackInterface::SetObjectiveOffset(double value) {
  NonIncrementalChange();
}

void KnapsackInterface::ClearObjective() { NonIncrementalChange(); }

int64_t KnapsackInterface::iterations() const { return 0; }

int64_t KnapsackInterface::nodes() const { return kUnknownNumberOfNodes; }

MPSolver::BasisStatus KnapsackInterface::row_status(
    int constraint_index) const {
  // TODO(user): set properly.
  return MPSolver::FREE;
}

MPSolver::BasisStatus KnapsackInterface::column_status(
    int variable_index) const {
  // TODO(user): set properly.
  return MPSolver::FREE;
}

bool KnapsackInterface::IsContinuous() const { return false; }

bool KnapsackInterface::IsLP() const { return false; }

bool KnapsackInterface::IsMIP() const { return true; }

std::string KnapsackInterface::SolverVersion() const {
  return "knapsack_solver-0.0";
}

void* KnapsackInterface::underlying_solver() { return knapsack_solver_.get(); }

void KnapsackInterface::ExtractNewVariables() {
  DCHECK_EQ(0, last_variable_index_);
  for (int column = 0; column < solver_->variables_.size(); ++column) {
    set_variable_as_extracted(column, true);
  }
}

void KnapsackInterface::ExtractNewConstraints() {
  DCHECK_EQ(0, last_constraint_index_);
  weights_.resize(solver_->constraints_.size());
  capacities_.resize(solver_->constraints_.size(),
                     std::numeric_limits<int64_t>::max());
  for (int row = 0; row < solver_->constraints_.size(); ++row) {
    MPConstraint* const ct = solver_->constraints_[row];
    double fixed_usage = 0.0;
    set_constraint_as_extracted(row, true);
    std::vector<double> coefficients(solver_->variables_.size() + 1, 0.0);
    for (const auto& entry : ct->coefficients_) {
      const int var_index = entry.first->index();
      DCHECK(variable_is_extracted(var_index));
      if (IsVariableFixedToValue(entry.first, 1.0)) {
        fixed_usage += entry.second;
      } else if (!IsVariableFixedToValue(entry.first, 0.0)) {
        coefficients[var_index] = entry.second;
      }
    }
    // Removing the contribution of variables fixed to 1 from the constraint
    // upper bound. All fixed variables have a zero coefficient.
    const double capacity = ct->ub() - fixed_usage;
    // Adding upper bound to the coefficients to scale.
    coefficients[solver_->variables_.size()] = capacity;
    double relative_error = 0.0;
    double scaling_factor = 0.0;
    GetBestScalingOfDoublesToInt64(coefficients,
                                   std::numeric_limits<int64_t>::max(),
                                   &scaling_factor, &relative_error);
    const int64_t gcd =
        ComputeGcdOfRoundedDoubles(coefficients, scaling_factor);
    std::vector<int64_t> scaled_coefficients(solver_->variables_.size(), 0);
    for (const auto& entry : ct->coefficients_) {
      if (!IsVariableFixed(entry.first)) {
        scaled_coefficients[entry.first->index()] =
            static_cast<int64_t>(round(scaling_factor * entry.second)) / gcd;
      }
    }
    weights_[row].swap(scaled_coefficients);
    capacities_[row] =
        static_cast<int64_t>(round(scaling_factor * capacity)) / gcd;
  }
}

void KnapsackInterface::ExtractObjective() {
  std::vector<double> coefficients(solver_->variables_.size(), 0.0);
  for (const auto& entry : solver_->objective_->coefficients_) {
    // Whether fixed to 0 or 1, fixed variables are removed from the
    // profit function, which for the current implementation means their
    // coefficient is set to 0.
    if (!IsVariableFixed(entry.first)) {
      coefficients[entry.first->index()] = entry.second;
    }
  }
  double relative_error = 0.0;
  double scaling_factor = 0.0;
  GetBestScalingOfDoublesToInt64(coefficients,
                                 std::numeric_limits<int64_t>::max(),
                                 &scaling_factor, &relative_error);
  const int64_t gcd = ComputeGcdOfRoundedDoubles(coefficients, scaling_factor);
  std::vector<int64_t> scaled_coefficients(solver_->variables_.size(), 0);
  for (const auto& entry : solver_->objective_->coefficients_) {
    scaled_coefficients[entry.first->index()] =
        static_cast<int64_t>(round(scaling_factor * entry.second)) / gcd;
  }
  profits_.swap(scaled_coefficients);
}

void KnapsackInterface::SetParameters(const MPSolverParameters& param) {
  SetCommonParameters(param);
}

void KnapsackInterface::SetRelativeMipGap(double value) {}

void KnapsackInterface::SetPrimalTolerance(double value) {}

void KnapsackInterface::SetDualTolerance(double value) {}

void KnapsackInterface::SetPresolveMode(int value) {}

void KnapsackInterface::SetScalingMode(int value) {}

void KnapsackInterface::SetLpAlgorithm(int value) {}

bool KnapsackInterface::IsKnapsackModel() const {
  // Check variables are boolean.
  for (int column = 0; column < solver_->variables_.size(); ++column) {
    MPVariable* const var = solver_->variables_[column];
    if (var->lb() <= -1.0 || var->ub() >= 2.0 || !var->integer()) {
      return false;
    }
  }
  // Check objective coefficients are positive.
  for (const auto& entry : solver_->objective_->coefficients_) {
    if (entry.second < 0) {
      return false;
    }
  }
  // Check constraints are knapsack constraints.
  for (int row = 0; row < solver_->constraints_.size(); ++row) {
    MPConstraint* const ct = solver_->constraints_[row];
    if (ct->lb() > 0.0) {
      return false;
    }
    for (const auto& entry : ct->coefficients_) {
      if (entry.second < 0) {
        return false;
      }
    }
  }
  // Check we are maximizing.
  return maximize_;
}

bool KnapsackInterface::IsVariableFixedToValue(const MPVariable* var,
                                               double value) const {
  const double lb_round_up = ceil(var->lb());
  return value == lb_round_up && floor(var->ub()) == lb_round_up;
}

bool KnapsackInterface::IsVariableFixed(const MPVariable* var) const {
  return IsVariableFixedToValue(var, 0.0) || IsVariableFixedToValue(var, 1.0);
}

double KnapsackInterface::GetVariableValueFromSolution(
    const MPVariable* var) const {
  return !IsVariableFixedToValue(var, 0.0) &&
                 (knapsack_solver_->BestSolutionContains(var->index()) ||
                  IsVariableFixedToValue(var, 1.0))
             ? 1.0
             : 0.0;
}

namespace {

// See MpSolverInterfaceFactoryRepository for details.
const void* const kRegisterKnapsack ABSL_ATTRIBUTE_UNUSED = [] {
  MPSolverInterfaceFactoryRepository::GetInstance()->Register(
      [](MPSolver* const solver) { return new KnapsackInterface(solver); },
      MPSolver::KNAPSACK_MIXED_INTEGER_PROGRAMMING);
  return nullptr;
}();

}  // namespace

}  // namespace operations_research
