// Copyright 2010-2014 Google
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

#include <string>
#include <vector>
#include <fstream>

#include "ortools/base/commandlineflags.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/stringprintf.h"
#include "ortools/base/file.h"
#include "google/protobuf/text_format.h"
#include "ortools/base/hash.h"
#include "ortools/bop/bop_parameters.pb.h"
#include "ortools/bop/integral_solver.h"
#include "ortools/linear_solver/linear_solver.h"

#if defined(USE_BOP)

namespace operations_research {
namespace {

MPSolver::ResultStatus TranslateProblemStatus(bop::BopSolveStatus status) {
  switch (status) {
    case bop::BopSolveStatus::OPTIMAL_SOLUTION_FOUND:
      return MPSolver::OPTIMAL;
    case bop::BopSolveStatus::FEASIBLE_SOLUTION_FOUND:
      return MPSolver::FEASIBLE;
    case bop::BopSolveStatus::NO_SOLUTION_FOUND:
      return MPSolver::NOT_SOLVED;
    case bop::BopSolveStatus::INFEASIBLE_PROBLEM:
      return MPSolver::INFEASIBLE;
    case bop::BopSolveStatus::INVALID_PROBLEM:
      return MPSolver::ABNORMAL;
  }
  LOG(DFATAL) << "Invalid bop::BopSolveStatus";
  return MPSolver::ABNORMAL;
}

}  // Anonymous namespace

class BopInterface : public MPSolverInterface {
 public:
  explicit BopInterface(MPSolver* const solver);
  ~BopInterface() override;

  // ----- Solve -----
  MPSolver::ResultStatus Solve(const MPSolverParameters& param) override;

  // ----- Model modifications and extraction -----
  void Reset() override;
  void SetOptimizationDirection(bool maximize) override;
  void SetVariableBounds(int index, double lb, double ub) override;
  void SetVariableInteger(int index, bool integer) override;
  void SetConstraintBounds(int index, double lb, double ub) override;
  void AddRowConstraint(MPConstraint* const ct) override;
  void AddVariable(MPVariable* const var) override;
  void SetCoefficient(MPConstraint* const constraint,
                      const MPVariable* const variable, double new_value,
                      double old_value) override;
  void ClearConstraint(MPConstraint* const constraint) override;
  void SetObjectiveCoefficient(const MPVariable* const variable,
                               double coefficient) override;
  void SetObjectiveOffset(double value) override;
  void ClearObjective() override;

  // ------ Query statistics on the solution and the solve ------
  int64 iterations() const override;
  int64 nodes() const override;
  double best_objective_bound() const override;
  MPSolver::BasisStatus row_status(int constraint_index) const override;
  MPSolver::BasisStatus column_status(int variable_index) const override;

  // ----- Misc -----
  bool IsContinuous() const override;
  bool IsLP() const override;
  bool IsMIP() const override;

  std::string SolverVersion() const override;
  bool InterruptSolve() override;
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
  bool SetSolverSpecificParametersAsString(const std::string& parameters) override;

 private:
  void NonIncrementalChange();

  glop::LinearProgram linear_program_;
  bop::IntegralSolver bop_solver_;
  std::vector<MPSolver::BasisStatus> column_status_;
  std::vector<MPSolver::BasisStatus> row_status_;
  bop::BopParameters parameters_;
  double best_objective_bound_;
  bool interrupt_solver_;
};

BopInterface::BopInterface(MPSolver* const solver)
    : MPSolverInterface(solver),
      linear_program_(),
      bop_solver_(),
      column_status_(),
      row_status_(),
      parameters_(),
      interrupt_solver_(false) {}

BopInterface::~BopInterface() {}

MPSolver::ResultStatus BopInterface::Solve(const MPSolverParameters& param) {
  // Check whenever the solve has already been stopped by the user.
  if (interrupt_solver_) {
    Reset();
    return MPSolver::NOT_SOLVED;
  }

  // Reset extraction as this interface is not incremental yet.
  Reset();
  ExtractModel();
  SetParameters(param);

  linear_program_.SetMaximizationProblem(maximize_);
  linear_program_.CleanUp();

  // Time limit.
  if (solver_->time_limit()) {
    VLOG(1) << "Setting time limit = " << solver_->time_limit() << " ms.";
    parameters_.set_max_time_in_seconds(
        static_cast<double>(solver_->time_limit()) / 1000.0);
  }
  parameters_.set_log_search_progress(!quiet());

  glop::DenseRow initial_solution;
  if (!solver_->solution_hint_.empty()) {
    const int num_vars = solver_->variables_.size();
    if (solver_->solution_hint_.size() != num_vars) {
      LOG(WARNING) << "Bop currently doesn't handle partial solution hints. "
                   << "Filling the missing positions with zeros...";
    }
    initial_solution.assign(glop::ColIndex(num_vars), glop::Fractional(0.0));
    for (const std::pair<MPVariable*, double>& p : solver_->solution_hint_) {
      initial_solution[glop::ColIndex(p.first->index())] =
          glop::Fractional(p.second);
    }
  }

  solver_->SetSolverSpecificParametersAsString(
      solver_->solver_specific_parameter_string_);
  bop_solver_.SetParameters(parameters_);
  std::unique_ptr<TimeLimit> time_limit =
      TimeLimit::FromParameters(parameters_);
  time_limit->RegisterExternalBooleanAsLimit(&interrupt_solver_);
  const bop::BopSolveStatus status =
      initial_solution.empty()
          ? bop_solver_.SolveWithTimeLimit(linear_program_, time_limit.get())
          : bop_solver_.SolveWithTimeLimit(linear_program_, initial_solution,
                                           time_limit.get());

  // The solution must be marked as synchronized even when no solution exists.
  sync_status_ = SOLUTION_SYNCHRONIZED;
  result_status_ = TranslateProblemStatus(status);
  if (result_status_ == MPSolver::FEASIBLE ||
      result_status_ == MPSolver::OPTIMAL) {
    // Get the results.
    objective_value_ = bop_solver_.objective_value();
    best_objective_bound_ = bop_solver_.best_bound();

    // TODO(user): Implement the column status.
    const size_t num_vars = solver_->variables_.size();
    column_status_.resize(num_vars, MPSolver::FREE);
    for (int var_id = 0; var_id < num_vars; ++var_id) {
      MPVariable* const var = solver_->variables_[var_id];
      const glop::ColIndex lp_solver_var_id(var->index());
      const glop::Fractional solution_value =
          bop_solver_.variable_values()[lp_solver_var_id];
      var->set_solution_value(static_cast<double>(solution_value));
    }

    // TODO(user): Implement the row status.
    const size_t num_constraints = solver_->constraints_.size();
    row_status_.resize(num_constraints, MPSolver::FREE);
  }

  return result_status_;
}

void BopInterface::Reset() {
  ResetExtractionInformation();
  linear_program_.Clear();
  interrupt_solver_ = false;
}

void BopInterface::SetOptimizationDirection(bool maximize) {
  NonIncrementalChange();
}

void BopInterface::SetVariableBounds(int index, double lb, double ub) {
  NonIncrementalChange();
}

void BopInterface::SetVariableInteger(int index, bool integer) {
  NonIncrementalChange();
}

void BopInterface::SetConstraintBounds(int index, double lb, double ub) {
  NonIncrementalChange();
}

void BopInterface::AddRowConstraint(MPConstraint* const ct) {
  NonIncrementalChange();
}

void BopInterface::AddVariable(MPVariable* const var) {
  NonIncrementalChange();
}

void BopInterface::SetCoefficient(MPConstraint* const constraint,
                                  const MPVariable* const variable,
                                  double new_value, double old_value) {
  NonIncrementalChange();
}

void BopInterface::ClearConstraint(MPConstraint* const constraint) {
  NonIncrementalChange();
}

void BopInterface::SetObjectiveCoefficient(const MPVariable* const variable,
                                           double coefficient) {
  NonIncrementalChange();
}

void BopInterface::SetObjectiveOffset(double value) { NonIncrementalChange(); }

void BopInterface::ClearObjective() { NonIncrementalChange(); }

int64 BopInterface::iterations() const {
  LOG(DFATAL) << "Number of iterations not available";
  return kUnknownNumberOfIterations;
}

int64 BopInterface::nodes() const {
  LOG(DFATAL) << "Number of nodes not available";
  return kUnknownNumberOfNodes;
}

double BopInterface::best_objective_bound() const {
  if (!CheckSolutionIsSynchronized() || !CheckBestObjectiveBoundExists()) {
    return trivial_worst_objective_bound();
  }
  return best_objective_bound_;
}

MPSolver::BasisStatus BopInterface::row_status(int constraint_index) const {
  return row_status_[constraint_index];
}

MPSolver::BasisStatus BopInterface::column_status(int variable_index) const {
  return column_status_[variable_index];
}

bool BopInterface::IsContinuous() const { return false; }
bool BopInterface::IsLP() const { return false; }
bool BopInterface::IsMIP() const { return true; }

std::string BopInterface::SolverVersion() const {
  // TODO(user): Decide how to version bop.
  return "Bop-0.0";
}

bool BopInterface::InterruptSolve() {
  interrupt_solver_ = true;
  return true;
}

void* BopInterface::underlying_solver() { return &bop_solver_; }

// TODO(user): remove duplication with GlopInterface.
void BopInterface::ExtractNewVariables() {
  DCHECK_EQ(0, last_variable_index_);
  DCHECK_EQ(0, last_constraint_index_);

  const glop::ColIndex num_cols(solver_->variables_.size());
  for (glop::ColIndex col(last_variable_index_); col < num_cols; ++col) {
    MPVariable* const var = solver_->variables_[col.value()];
    const glop::ColIndex new_col =
        linear_program_.FindOrCreateVariable(var->name());
    DCHECK_EQ(new_col, col);
    set_variable_as_extracted(col.value(), true);
    linear_program_.SetVariableBounds(col, var->lb(), var->ub());
    if (var->integer()) {
      linear_program_.SetVariableType(
          col, glop::LinearProgram::VariableType::INTEGER);
    }
  }
}

// TODO(user): remove duplication with GlopInterface.
void BopInterface::ExtractNewConstraints() {
  DCHECK_EQ(0, last_constraint_index_);

  const glop::RowIndex num_rows(solver_->constraints_.size());
  for (glop::RowIndex row(0); row < num_rows; ++row) {
    MPConstraint* const ct = solver_->constraints_[row.value()];
    set_constraint_as_extracted(row.value(), true);

    const double lb = ct->lb();
    const double ub = ct->ub();
    const glop::RowIndex new_row =
        linear_program_.FindOrCreateConstraint(ct->name());
    DCHECK_EQ(new_row, row);
    linear_program_.SetConstraintBounds(row, lb, ub);

    for (CoeffEntry entry : ct->coefficients_) {
      const int var_index = entry.first->index();
      DCHECK(variable_is_extracted(var_index));
      const glop::ColIndex col(var_index);
      const double coeff = entry.second;
      linear_program_.SetCoefficient(row, col, coeff);
    }
  }
}

// TODO(user): remove duplication with GlopInterface.
void BopInterface::ExtractObjective() {
  linear_program_.SetObjectiveOffset(solver_->Objective().offset());
  for (CoeffEntry entry : solver_->objective_->coefficients_) {
    const int var_index = entry.first->index();
    const glop::ColIndex col(var_index);
    const double coeff = entry.second;
    linear_program_.SetObjectiveCoefficient(col, coeff);
  }
}

void BopInterface::SetParameters(const MPSolverParameters& param) {
  parameters_.Clear();
  SetCommonParameters(param);
}

// All these have no effect.
void BopInterface::SetPrimalTolerance(double value) {}
void BopInterface::SetDualTolerance(double value) {}
void BopInterface::SetScalingMode(int value) {}
void BopInterface::SetLpAlgorithm(int value) {}
void BopInterface::SetRelativeMipGap(double value) {}

void BopInterface::SetPresolveMode(int value) {
  switch (value) {
    case MPSolverParameters::PRESOLVE_OFF:
      // TODO(user): add this to BopParameters.
      break;
    case MPSolverParameters::PRESOLVE_ON:
      // TODO(user): add this to BopParameters.
      break;
    default:
      if (value != MPSolverParameters::kDefaultIntegerParamValue) {
        SetIntegerParamToUnsupportedValue(MPSolverParameters::PRESOLVE, value);
      }
  }
}

bool BopInterface::SetSolverSpecificParametersAsString(
    const std::string& parameters) {
  const bool ok = google::protobuf::TextFormat::MergeFromString(parameters, &parameters_);
  bop_solver_.SetParameters(parameters_);
  return ok;
}

void BopInterface::NonIncrementalChange() {
  // The current implementation is not incremental.
  sync_status_ = MUST_RELOAD;
}

// Register BOP in the global linear solver factory.
MPSolverInterface* BuildBopInterface(MPSolver* const solver) {
  return new BopInterface(solver);
}


}  // namespace operations_research
#endif  //  #if defined(USE_BOP)
