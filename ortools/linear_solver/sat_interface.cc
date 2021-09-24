// Copyright 2010-2021 Google LLC
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

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "ortools/base/hash.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/linear_solver/linear_solver.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/linear_solver/sat_proto_solver.h"
#include "ortools/port/proto_utils.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/lp_utils.h"
#include "ortools/sat/model.h"
#include "ortools/util/time_limit.h"

namespace operations_research {

#if defined(PROTOBUF_INTERNAL_IMPL)
using google::protobuf::Message;
#else
using google::protobuf::Message;
#endif

class SatInterface : public MPSolverInterface {
 public:
  explicit SatInterface(MPSolver* const solver);
  ~SatInterface() override;

  // ----- Solve -----
  MPSolver::ResultStatus Solve(const MPSolverParameters& param) override;
  absl::optional<MPSolutionResponse> DirectlySolveProto(
      const MPModelRequest& request, std::atomic<bool>* interrupt) override;
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

  bool AddIndicatorConstraint(MPConstraint* const ct) override { return true; }

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
  bool SetSolverSpecificParametersAsString(
      const std::string& parameters) override;
  absl::Status SetNumThreads(int num_threads) override;

 private:
  void NonIncrementalChange();

  std::atomic<bool> interrupt_solve_;
  sat::SatParameters parameters_;
  int num_threads_ = 0;
};

SatInterface::SatInterface(MPSolver* const solver)
    : MPSolverInterface(solver), interrupt_solve_(false) {}

SatInterface::~SatInterface() {}

MPSolver::ResultStatus SatInterface::Solve(const MPSolverParameters& param) {
  interrupt_solve_ = false;

  // Reset extraction as this interface is not incremental yet.
  Reset();
  ExtractModel();

  SetParameters(param);
  solver_->SetSolverSpecificParametersAsString(
      solver_->solver_specific_parameter_string_);

  // Time limit.
  if (solver_->time_limit()) {
    VLOG(1) << "Setting time limit = " << solver_->time_limit() << " ms.";
    parameters_.set_max_time_in_seconds(
        static_cast<double>(solver_->time_limit()) / 1000.0);
  }

  // Mark variables and constraints as extracted.
  for (int i = 0; i < solver_->variables_.size(); ++i) {
    set_variable_as_extracted(i, true);
  }
  for (int i = 0; i < solver_->constraints_.size(); ++i) {
    set_constraint_as_extracted(i, true);
  }

  MPModelRequest request;
  solver_->ExportModelToProto(request.mutable_model());
  request.set_solver_specific_parameters(
      EncodeSatParametersAsString(parameters_));
  request.set_enable_internal_solver_output(!quiet_);
  const absl::StatusOr<MPSolutionResponse> status_or =
      SatSolveProto(std::move(request), &interrupt_solve_);

  if (!status_or.ok()) return MPSolver::ABNORMAL;
  const MPSolutionResponse& response = status_or.value();

  // The solution must be marked as synchronized even when no solution exists.
  sync_status_ = SOLUTION_SYNCHRONIZED;
  switch (response.status()) {
    case MPSOLVER_OPTIMAL:
      result_status_ = MPSolver::OPTIMAL;
      break;
    case MPSOLVER_FEASIBLE:
      result_status_ = MPSolver::FEASIBLE;
      break;
    case MPSOLVER_INFEASIBLE:
      result_status_ = MPSolver::INFEASIBLE;
      break;
    case MPSOLVER_MODEL_INVALID:
      result_status_ = MPSolver::MODEL_INVALID;
      break;
    default:
      result_status_ = MPSolver::NOT_SOLVED;
      break;
  }

  // TODO(user): Just use LoadSolutionFromProto(), but fix that function first
  // to load everything and not just the solution.
  if (response.status() == MPSOLVER_FEASIBLE ||
      response.status() == MPSOLVER_OPTIMAL) {
    objective_value_ = response.objective_value();
    best_objective_bound_ = response.best_objective_bound();
    const size_t num_vars = solver_->variables_.size();
    for (int var_id = 0; var_id < num_vars; ++var_id) {
      MPVariable* const var = solver_->variables_[var_id];
      var->set_solution_value(response.variable_value(var_id));
    }
  }

  return result_status_;
}

absl::optional<MPSolutionResponse> SatInterface::DirectlySolveProto(
    const MPModelRequest& request, std::atomic<bool>* interrupt) {
  absl::StatusOr<MPSolutionResponse> status_or =
      SatSolveProto(request, interrupt);
  if (status_or.ok()) return std::move(status_or).value();
  if (request.enable_internal_solver_output()) {
    LOG(INFO) << "Failed SAT solve: " << status_or.status();
  }
  MPSolutionResponse response;
  // As of 2021-08, the sole non-OK status returned by SatSolveProto is an
  // INVALID_ARGUMENT error caused by invalid solver parameters.
  // TODO(user): Move that conversion to SatSolveProto, which should always
  // return a MPSolutionResponse, even for errors.
  response.set_status(absl::IsInvalidArgument(status_or.status())
                          ? MPSOLVER_MODEL_INVALID_SOLVER_PARAMETERS
                          : MPSOLVER_ABNORMAL);
  response.set_status_str(status_or.status().ToString());
  return response;
}

bool SatInterface::InterruptSolve() {
  interrupt_solve_ = true;
  return true;
}

void SatInterface::Reset() { ResetExtractionInformation(); }

void SatInterface::SetOptimizationDirection(bool maximize) {
  NonIncrementalChange();
}

void SatInterface::SetVariableBounds(int index, double lb, double ub) {
  NonIncrementalChange();
}

void SatInterface::SetVariableInteger(int index, bool integer) {
  NonIncrementalChange();
}

void SatInterface::SetConstraintBounds(int index, double lb, double ub) {
  NonIncrementalChange();
}

void SatInterface::AddRowConstraint(MPConstraint* const ct) {
  NonIncrementalChange();
}

void SatInterface::AddVariable(MPVariable* const var) {
  NonIncrementalChange();
}

void SatInterface::SetCoefficient(MPConstraint* const constraint,
                                  const MPVariable* const variable,
                                  double new_value, double old_value) {
  NonIncrementalChange();
}

void SatInterface::ClearConstraint(MPConstraint* const constraint) {
  NonIncrementalChange();
}

void SatInterface::SetObjectiveCoefficient(const MPVariable* const variable,
                                           double coefficient) {
  NonIncrementalChange();
}

void SatInterface::SetObjectiveOffset(double value) { NonIncrementalChange(); }

void SatInterface::ClearObjective() { NonIncrementalChange(); }

int64_t SatInterface::iterations() const {
  return 0;  // FIXME
}

int64_t SatInterface::nodes() const { return 0; }

MPSolver::BasisStatus SatInterface::row_status(int constraint_index) const {
  return MPSolver::BasisStatus::FREE;  // FIXME
}

MPSolver::BasisStatus SatInterface::column_status(int variable_index) const {
  return MPSolver::BasisStatus::FREE;  // FIXME
}

bool SatInterface::IsContinuous() const { return false; }
bool SatInterface::IsLP() const { return false; }
bool SatInterface::IsMIP() const { return true; }

std::string SatInterface::SolverVersion() const {
  return "SAT Based MIP Solver";
}

void* SatInterface::underlying_solver() { return nullptr; }

void SatInterface::ExtractNewVariables() { NonIncrementalChange(); }

void SatInterface::ExtractNewConstraints() { NonIncrementalChange(); }

void SatInterface::ExtractObjective() { NonIncrementalChange(); }

void SatInterface::SetParameters(const MPSolverParameters& param) {
  parameters_.set_num_search_workers(num_threads_);
  parameters_.set_linearization_level(2);
  parameters_.set_log_search_progress(!quiet_);
  SetCommonParameters(param);
}

absl::Status SatInterface::SetNumThreads(int num_threads) {
  num_threads_ = num_threads;
  return absl::OkStatus();
}

// All these have no effect.
void SatInterface::SetPrimalTolerance(double value) {}
void SatInterface::SetDualTolerance(double value) {}
void SatInterface::SetScalingMode(int value) {}
void SatInterface::SetLpAlgorithm(int value) {}
void SatInterface::SetRelativeMipGap(double value) {}

// TODO(user): Implement me.
void SatInterface::SetPresolveMode(int value) {}

bool SatInterface::SetSolverSpecificParametersAsString(
    const std::string& parameters) {
  return ProtobufTextFormatMergeFromString(parameters, &parameters_);
}

void SatInterface::NonIncrementalChange() {
  // The current implementation is not incremental.
  sync_status_ = MUST_RELOAD;
}

// Register Sat in the global linear solver factory.
MPSolverInterface* BuildSatInterface(MPSolver* const solver) {
  return new SatInterface(solver);
}

}  // namespace operations_research
