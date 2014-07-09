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


#include "base/hash.h"
#include <string>
#include <vector>
#include <fstream>

#include "base/commandlineflags.h"
#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/file.h"
#include "google/protobuf/text_format.h"
#include "base/hash.h"
#include "glop/lp_data.h"
#include "glop/lp_solver.h"
#include "glop/lp_types.h"
#include "glop/parameters.pb.h"
#include "linear_solver/linear_solver.h"

DECLARE_double(solver_timeout_in_seconds);
DECLARE_string(solver_write_model);

namespace operations_research {

namespace {

MPSolver::ResultStatus TranslateProblemStatus(glop::ProblemStatus status) {
  switch (status) {
    case glop::ProblemStatus::OPTIMAL:
      return MPSolver::OPTIMAL;
    case glop::ProblemStatus::PRIMAL_FEASIBLE:
      return MPSolver::FEASIBLE;
    case glop::ProblemStatus::PRIMAL_INFEASIBLE:  // PASS_THROUGH_INTENDED
    case glop::ProblemStatus::DUAL_UNBOUNDED:
      return MPSolver::INFEASIBLE;
    case glop::ProblemStatus::PRIMAL_UNBOUNDED:
      return MPSolver::UNBOUNDED;
    case glop::ProblemStatus::DUAL_FEASIBLE:  // PASS_THROUGH_INTENDED
    case glop::ProblemStatus::INIT:
      return MPSolver::NOT_SOLVED;
    // TODO(user): Glop may return ProblemStatus::DUAL_INFEASIBLE or
    // ProblemStatus::INFEASIBLE_OR_UNBOUNDED.
    // Unfortunatley, the wrapper does not support this return status at this
    // point (even though Cplex and Gurobi have the equivalent). So we convert
    // it to MPSolver::ABNORMAL instead.
    case glop::ProblemStatus::DUAL_INFEASIBLE:          // PASS_THROUGH_INTENDED
    case glop::ProblemStatus::INFEASIBLE_OR_UNBOUNDED:  // PASS_THROUGH_INTENDED
    case glop::ProblemStatus::ABNORMAL:                 // PASS_THROUGH_INTENDED
    case glop::ProblemStatus::IMPRECISE:                // PASS_THROUGH_INTENDED
    case glop::ProblemStatus::INVALID_PROBLEM:
      return MPSolver::ABNORMAL;
  }
  LOG(DFATAL) << "Invalid glop::ProblemStatus " << status;
  return MPSolver::ABNORMAL;
}

MPSolver::BasisStatus TranslateVariableStatus(glop::VariableStatus status) {
  switch (status) {
    case glop::VariableStatus::FREE:
      return MPSolver::FREE;
    case glop::VariableStatus::AT_LOWER_BOUND:
      return MPSolver::AT_LOWER_BOUND;
    case glop::VariableStatus::AT_UPPER_BOUND:
      return MPSolver::AT_UPPER_BOUND;
    case glop::VariableStatus::FIXED_VALUE:
      return MPSolver::FIXED_VALUE;
    case glop::VariableStatus::BASIC:
      return MPSolver::BASIC;
  }
  LOG(DFATAL) << "Unknown variable status: " << status;
  return MPSolver::FREE;
}

MPSolver::BasisStatus TranslateConstraintStatus(glop::ConstraintStatus status) {
  switch (status) {
    case glop::ConstraintStatus::FREE:
      return MPSolver::FREE;
    case glop::ConstraintStatus::AT_LOWER_BOUND:
      return MPSolver::AT_LOWER_BOUND;
    case glop::ConstraintStatus::AT_UPPER_BOUND:
      return MPSolver::AT_UPPER_BOUND;
    case glop::ConstraintStatus::FIXED_VALUE:
      return MPSolver::FIXED_VALUE;
    case glop::ConstraintStatus::BASIC:
      return MPSolver::BASIC;
  }
  LOG(DFATAL) << "Unknown constraint status: " << status;
  return MPSolver::FREE;
}
}  // Anonymous namespace

class GLOPInterface : public MPSolverInterface {
 public:
  explicit GLOPInterface(MPSolver* const solver);
  virtual ~GLOPInterface();

  // ----- Solve -----
  virtual MPSolver::ResultStatus Solve(const MPSolverParameters& param);

  // ----- Model modifications and extraction -----
  virtual void Reset();
  virtual void SetOptimizationDirection(bool maximize);
  virtual void SetVariableBounds(int index, double lb, double ub);
  virtual void SetVariableInteger(int index, bool integer);
  virtual void SetConstraintBounds(int index, double lb, double ub);
  virtual void AddRowConstraint(MPConstraint* const ct);
  virtual void AddVariable(MPVariable* const var);
  virtual void SetCoefficient(MPConstraint* const constraint,
                              const MPVariable* const variable,
                              double new_value, double old_value);
  virtual void ClearConstraint(MPConstraint* const constraint);
  virtual void SetObjectiveCoefficient(const MPVariable* const variable,
                                       double coefficient);
  virtual void SetObjectiveOffset(double value);
  virtual void ClearObjective();

  // ------ Query statistics on the solution and the solve ------
  virtual int64 iterations() const;
  virtual int64 nodes() const;
  virtual double best_objective_bound() const;
  virtual MPSolver::BasisStatus row_status(int constraint_index) const;
  virtual MPSolver::BasisStatus column_status(int variable_index) const;

  // ----- Misc -----
  virtual bool IsContinuous() const;
  virtual bool IsLP() const;
  virtual bool IsMIP() const;

  virtual std::string SolverVersion() const;
  virtual void* underlying_solver();

  virtual void ExtractNewVariables();
  virtual void ExtractNewConstraints();
  virtual void ExtractObjective();

  virtual void SetParameters(const MPSolverParameters& param);
  virtual void SetRelativeMipGap(double value);
  virtual void SetPrimalTolerance(double value);
  virtual void SetDualTolerance(double value);
  virtual void SetPresolveMode(int value);
  virtual void SetScalingMode(int value);
  virtual void SetLpAlgorithm(int value);
  virtual bool ReadParameterFile(const std::string& filename);

 private:
  void NonIncrementalChange();

  glop::LinearProgram linear_program_;
  glop::LPSolver lp_solver_;
  std::vector<MPSolver::BasisStatus> column_status_;
  std::vector<MPSolver::BasisStatus> row_status_;
  glop::GlopParameters parameters_;
};

GLOPInterface::GLOPInterface(MPSolver* const solver)
    : MPSolverInterface(solver),
      linear_program_(),
      lp_solver_(),
      column_status_(),
      row_status_(),
      parameters_() {}

GLOPInterface::~GLOPInterface() {}

MPSolver::ResultStatus GLOPInterface::Solve(const MPSolverParameters& param) {
  // Reset extraction as Glop is not incremental yet.
  Reset();
  ExtractModel();
  SetParameters(param);

  linear_program_.SetMaximizationProblem(maximize_);
  linear_program_.CleanUp();

  // Time limit.
  if (solver_->time_limit()) {
    VLOG(1) << "Setting time limit = " << solver_->time_limit() << " ms.";
    parameters_.set_max_time_in_seconds(
        1000.0 * static_cast<double>(solver_->time_limit()));
  }

  solver_->SetSolverSpecificParametersAsString(
      solver_->solver_specific_parameter_string_);
  lp_solver_.SetParameters(parameters_);
  const glop::ProblemStatus status = lp_solver_.Solve(linear_program_);

  // The solution must be marked as synchronized even when no solution exists.
  sync_status_ = SOLUTION_SYNCHRONIZED;
  result_status_ = TranslateProblemStatus(status);
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
    column_status_.at(var_id) = TranslateVariableStatus(variable_status);
  }

  const size_t num_constraints = solver_->constraints_.size();
  row_status_.resize(num_constraints, MPSolver::FREE);
  for (int ct_id = 0; ct_id < num_constraints; ++ct_id) {
    MPConstraint* const ct = solver_->constraints_[ct_id];
    const glop::RowIndex lp_solver_ct_id(ct->index());

    const glop::Fractional dual_value =
        lp_solver_.dual_values()[lp_solver_ct_id];
    ct->set_dual_value(static_cast<double>(dual_value));

    const glop::Fractional row_activity =
        lp_solver_.constraint_activities()[lp_solver_ct_id];
    ct->set_activity(static_cast<double>(row_activity));

    const glop::ConstraintStatus constraint_status =
        lp_solver_.constraint_statuses()[lp_solver_ct_id];
    row_status_.at(ct_id) = TranslateConstraintStatus(constraint_status);
  }

  return result_status_;
}

void GLOPInterface::Reset() {
  ResetExtractionInformation();
  linear_program_.Clear();
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
    var->set_index(col.value());
    linear_program_.SetVariableBounds(col, var->lb(), var->ub());
  }
}

void GLOPInterface::ExtractNewConstraints() {
  DCHECK_EQ(0, last_constraint_index_);

  const glop::RowIndex num_rows(solver_->constraints_.size());
  for (glop::RowIndex row(0); row < num_rows; ++row) {
    MPConstraint* const ct = solver_->constraints_[row.value()];
    ct->set_index(row.value());

    const double lb = ct->lb();
    const double ub = ct->ub();
    const glop::RowIndex new_row =
        linear_program_.FindOrCreateConstraint(ct->name());
    DCHECK_EQ(new_row, row);
    linear_program_.SetConstraintBounds(row, lb, ub);

    for (CoeffEntry entry : ct->coefficients_) {
      const int var_index = entry.first->index();
      DCHECK_NE(kNoIndex, var_index);
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

void GLOPInterface::SetParameters(const MPSolverParameters& param) {
  parameters_.Clear();
  SetCommonParameters(param);
  SetScalingMode(param.GetIntegerParam(MPSolverParameters::SCALING));

  // The parameters set using SetGlopParameters() take precedence.
  parameters_.MergeFrom(param.GetGlopParameters());
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
  // now we want to use higher degault tolerances in Glop.
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
    SetDoubleParamToUnsupportedValue(MPSolverParameters::PRIMAL_TOLERANCE,
                                     value);
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

bool GLOPInterface::ReadParameterFile(const std::string& filename) {
  std::string params;
  if (!file::GetContents(filename, &params, file::Defaults()).ok()) {
    return false;
  }
  const bool ok = google::protobuf::TextFormat::MergeFromString(params, &parameters_);
  lp_solver_.SetParameters(parameters_);
  return ok;
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
