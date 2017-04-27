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


#include <unordered_map>
#include <string>
#include <vector>
#include <fstream>

#include "ortools/base/commandlineflags.h"

#ifndef ANDROID_JNI
#include "ortools/base/commandlineflags.h"
#endif

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/stringprintf.h"

#ifndef ANDROID_JNI
#include "ortools/base/file.h"
#include "google/protobuf/text_format.h"
#endif

#include "ortools/base/hash.h"
#include "ortools/glop/lp_solver.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/linear_solver/glop_utils.h"
#include "ortools/linear_solver/linear_solver.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/util/time_limit.h"

namespace operations_research {

namespace {

}  // Anonymous namespace

class GLOPInterface : public MPSolverInterface {
 public:
  explicit GLOPInterface(MPSolver* const solver);
  ~GLOPInterface() override;

  // ----- Solve -----
  MPSolver::ResultStatus Solve(const MPSolverParameters& param) override;
  bool InterruptSolve() override;

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
  void* underlying_solver() override;

  void ExtractNewVariables() override;
  void ExtractNewConstraints() override;
  void ExtractObjective() override;

  void SetStartingLpBasis(
      const std::vector<MPSolver::BasisStatus>& variable_statuses,
      const std::vector<MPSolver::BasisStatus>& constraint_statuses) override;

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
  glop::LPSolver lp_solver_;
  std::vector<MPSolver::BasisStatus> column_status_;
  std::vector<MPSolver::BasisStatus> row_status_;
  glop::GlopParameters parameters_;
  bool interrupt_solver_;
};

GLOPInterface::GLOPInterface(MPSolver* const solver)
    : MPSolverInterface(solver),
      linear_program_(),
      lp_solver_(),
      column_status_(),
      row_status_(),
      parameters_(),
      interrupt_solver_(false) {}

GLOPInterface::~GLOPInterface() {}

MPSolver::ResultStatus GLOPInterface::Solve(const MPSolverParameters& param) {
  // Re-extract the problem from scratch. We don't support modifying the
  // LinearProgram in sync with changes done in the MPSolver.
  ResetExtractionInformation();
  linear_program_.Clear();
  interrupt_solver_ = false;
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

  solver_->SetSolverSpecificParametersAsString(
      solver_->solver_specific_parameter_string_);
  lp_solver_.SetParameters(parameters_);
  std::unique_ptr<TimeLimit> time_limit =
      TimeLimit::FromParameters(parameters_);
  time_limit->RegisterExternalBooleanAsLimit(&interrupt_solver_);
  const glop::ProblemStatus status =
      lp_solver_.SolveWithTimeLimit(linear_program_, time_limit.get());

  // The solution must be marked as synchronized even when no solution exists.
  sync_status_ = SOLUTION_SYNCHRONIZED;
  result_status_ = GlopToMPSolverResultStatus(status);
  objective_value_ = lp_solver_.GetObjectiveValue();

  const size_t num_vars = solver_->variables_.size();
  column_status_.resize(num_vars, MPSolver::FREE);
  for (int var_id = 0; var_id < num_vars; ++var_id) {
    MPVariable* const var = solver_->variables_[var_id];
    const glop::ColIndex lp_solver_var_id(var->index());

    const glop::Fractional solution_value =
        lp_solver_.variable_values()[lp_solver_var_id];
    var->set_solution_value(static_cast<double>(solution_value));

    const glop::Fractional reduced_cost =
        lp_solver_.reduced_costs()[lp_solver_var_id];
    var->set_reduced_cost(static_cast<double>(reduced_cost));

    const glop::VariableStatus variable_status =
        lp_solver_.variable_statuses()[lp_solver_var_id];
    column_status_.at(var_id) = GlopToMPSolverVariableStatus(variable_status);
  }

  const size_t num_constraints = solver_->constraints_.size();
  row_status_.resize(num_constraints, MPSolver::FREE);
  for (int ct_id = 0; ct_id < num_constraints; ++ct_id) {
    MPConstraint* const ct = solver_->constraints_[ct_id];
    const glop::RowIndex lp_solver_ct_id(ct->index());

    const glop::Fractional dual_value =
        lp_solver_.dual_values()[lp_solver_ct_id];
    ct->set_dual_value(static_cast<double>(dual_value));

    const glop::ConstraintStatus constraint_status =
        lp_solver_.constraint_statuses()[lp_solver_ct_id];
    row_status_.at(ct_id) = GlopToMPSolverConstraintStatus(constraint_status);
  }

  return result_status_;
}

bool GLOPInterface::InterruptSolve() {
  interrupt_solver_ = true;
  return true;
}

void GLOPInterface::Reset() {
  // Ignore any incremental info for the next solve. Note that the parameters
  // will not be reset as we re-read them on each Solve().
  lp_solver_.Clear();
}

void GLOPInterface::SetOptimizationDirection(bool maximize) {
  NonIncrementalChange();
}

void GLOPInterface::SetVariableBounds(int index, double lb, double ub) {
  NonIncrementalChange();
}

void GLOPInterface::SetVariableInteger(int index, bool integer) {
  LOG(WARNING) << "Glop doesn't deal with integer variables.";
}

void GLOPInterface::SetConstraintBounds(int index, double lb, double ub) {
  NonIncrementalChange();
}

void GLOPInterface::AddRowConstraint(MPConstraint* const ct) {
  NonIncrementalChange();
}

void GLOPInterface::AddVariable(MPVariable* const var) {
  NonIncrementalChange();
}

void GLOPInterface::SetCoefficient(MPConstraint* const constraint,
                                   const MPVariable* const variable,
                                   double new_value, double old_value) {
  NonIncrementalChange();
}

void GLOPInterface::ClearConstraint(MPConstraint* const constraint) {
  NonIncrementalChange();
}

void GLOPInterface::SetObjectiveCoefficient(const MPVariable* const variable,
                                            double coefficient) {
  NonIncrementalChange();
}

void GLOPInterface::SetObjectiveOffset(double value) { NonIncrementalChange(); }

void GLOPInterface::ClearObjective() { NonIncrementalChange(); }

int64 GLOPInterface::iterations() const {
  return lp_solver_.GetNumberOfSimplexIterations();
}

int64 GLOPInterface::nodes() const {
  LOG(DFATAL) << "Number of nodes only available for discrete problems";
  return kUnknownNumberOfNodes;
}

double GLOPInterface::best_objective_bound() const {
  LOG(DFATAL) << "Best objective bound only available for discrete problems";
  return trivial_worst_objective_bound();
}

MPSolver::BasisStatus GLOPInterface::row_status(int constraint_index) const {
  return row_status_[constraint_index];
}

MPSolver::BasisStatus GLOPInterface::column_status(int variable_index) const {
  return column_status_[variable_index];
}

bool GLOPInterface::IsContinuous() const { return true; }

bool GLOPInterface::IsLP() const { return true; }

bool GLOPInterface::IsMIP() const { return false; }

std::string GLOPInterface::SolverVersion() const {
  // TODO(user): Decide how to version glop. Add a GetVersion() to LPSolver.
  return "Glop-0.0";
}

void* GLOPInterface::underlying_solver() { return &lp_solver_; }

void GLOPInterface::ExtractNewVariables() {
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
  }
}

void GLOPInterface::ExtractNewConstraints() {
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

void GLOPInterface::ExtractObjective() {
  linear_program_.SetObjectiveOffset(solver_->Objective().offset());
  for (CoeffEntry entry : solver_->objective_->coefficients_) {
    const int var_index = entry.first->index();
    const glop::ColIndex col(var_index);
    const double coeff = entry.second;
    linear_program_.SetObjectiveCoefficient(col, coeff);
  }
}

void GLOPInterface::SetStartingLpBasis(
    const std::vector<MPSolver::BasisStatus>& variable_statuses,
    const std::vector<MPSolver::BasisStatus>& constraint_statuses) {
  glop::VariableStatusRow glop_variable_statuses;
  glop::ConstraintStatusColumn glop_constraint_statuses;
  for (const MPSolver::BasisStatus& status : variable_statuses) {
    glop_variable_statuses.push_back(MPSolverToGlopVariableStatus(status));
  }
  for (const MPSolver::BasisStatus& status : constraint_statuses) {
    glop_constraint_statuses.push_back(MPSolverToGlopConstraintStatus(status));
  }
  lp_solver_.SetInitialBasis(glop_variable_statuses, glop_constraint_statuses);
}

void GLOPInterface::SetParameters(const MPSolverParameters& param) {
  parameters_.Clear();
  SetCommonParameters(param);
  SetScalingMode(param.GetIntegerParam(MPSolverParameters::SCALING));

}

void GLOPInterface::SetRelativeMipGap(double value) {
  if (value != MPSolverParameters::kDefaultDoubleParamValue) {
    SetDoubleParamToUnsupportedValue(MPSolverParameters::RELATIVE_MIP_GAP,
                                     value);
  }
}

void GLOPInterface::SetPrimalTolerance(double value) {
  // TODO(user): Modify parameters_ with the correct value.
  // The problem is that this is set by default by the wrapper to 1e-7 and for
  // now we want to use higher default tolerances in Glop.
  if (value != MPSolverParameters::kDefaultDoubleParamValue) {
    SetDoubleParamToUnsupportedValue(MPSolverParameters::PRIMAL_TOLERANCE,
                                     value);
  }
}

void GLOPInterface::SetDualTolerance(double value) {
  // TODO(user): Modify parameters_ with the correct value.
  // The problem is that this is set by default by the wrapper to 1e-7 and for
  // now we want to use higher default tolerances in Glop.
  if (value != MPSolverParameters::kDefaultDoubleParamValue) {
    SetDoubleParamToUnsupportedValue(MPSolverParameters::DUAL_TOLERANCE, value);
  }
}

void GLOPInterface::SetPresolveMode(int value) {
  switch (value) {
    case MPSolverParameters::PRESOLVE_OFF:
      parameters_.set_use_preprocessing(false);
      break;
    case MPSolverParameters::PRESOLVE_ON:
      parameters_.set_use_preprocessing(true);
      break;
    default:
      if (value != MPSolverParameters::kDefaultIntegerParamValue) {
        SetIntegerParamToUnsupportedValue(MPSolverParameters::PRESOLVE, value);
      }
  }
}

void GLOPInterface::SetScalingMode(int value) {
  switch (value) {
    case MPSolverParameters::SCALING_OFF:
      parameters_.set_use_scaling(false);
      break;
    case MPSolverParameters::SCALING_ON:
      parameters_.set_use_scaling(true);
      break;
    default:
      if (value != MPSolverParameters::kDefaultIntegerParamValue) {
        SetIntegerParamToUnsupportedValue(MPSolverParameters::SCALING, value);
      }
  }
}

void GLOPInterface::SetLpAlgorithm(int value) {
  switch (value) {
    case MPSolverParameters::DUAL:
      parameters_.set_use_dual_simplex(true);
      break;
    case MPSolverParameters::PRIMAL:
      parameters_.set_use_dual_simplex(false);
      break;
    default:
      if (value != MPSolverParameters::kDefaultIntegerParamValue) {
        SetIntegerParamToUnsupportedValue(MPSolverParameters::LP_ALGORITHM,
                                          value);
      }
  }
}

bool GLOPInterface::SetSolverSpecificParametersAsString(
    const std::string& parameters) {
#ifdef ANDROID_JNI
  // NOTE(user): Android build uses protocol buffers in lite mode, and
  // parsing data from text format is not supported there. To allow solver
  // specific parameters from std::string on Android, we first need to switch to
  // non-lite version of protocol buffers.
  return false;
#else
  const bool ok = google::protobuf::TextFormat::MergeFromString(parameters, &parameters_);
  lp_solver_.SetParameters(parameters_);
  return ok;
#endif
}

void GLOPInterface::NonIncrementalChange() {
  // The current implementation is not incremental.
  sync_status_ = MUST_RELOAD;
}

// Register GLOP in the global linear solver factory.
MPSolverInterface* BuildGLOPInterface(MPSolver* const solver) {
  return new GLOPInterface(solver);
}


}  // namespace operations_research
