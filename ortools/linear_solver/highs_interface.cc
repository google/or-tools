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

#if defined(USE_HIGHS)

#include <atomic>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "ortools/base/logging.h"
#include "ortools/linear_solver/linear_solver.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/linear_solver/proto_solver/highs_proto_solver.h"
#include "ortools/linear_solver/proto_solver/proto_utils.h"
#include "ortools/util/lazy_mutable_copy.h"

namespace operations_research {

class HighsInterface : public MPSolverInterface {
 public:
  explicit HighsInterface(MPSolver* solver, bool solve_as_a_mip);
  ~HighsInterface() override;

  // ----- Solve -----
  MPSolver::ResultStatus Solve(const MPSolverParameters& param) override;

  // ----- Directly solve proto is supported without interrupt ---
  bool SupportsDirectlySolveProto(std::atomic<bool>* interrupt) const override {
    return interrupt == nullptr;
  }
  MPSolutionResponse DirectlySolveProto(LazyMutableCopy<MPModelRequest> request,
                                        std::atomic<bool>* interrupt) override {
    DCHECK_EQ(interrupt, nullptr);
    const bool log_error = request->enable_internal_solver_output();
    return ConvertStatusOrMPSolutionResponse(
        log_error, HighsSolveProto(std::move(request)));
  }

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
  bool SetSolverSpecificParametersAsString(
      const std::string& parameters) override;
  absl::Status SetNumThreads(int num_threads) override;

 private:
  void NonIncrementalChange();

  const bool solve_as_a_mip_;
  std::optional<HighsSolveInfo> solve_info_;
};

HighsInterface::HighsInterface(MPSolver* const solver, bool solve_as_a_mip)
    : MPSolverInterface(solver), solve_as_a_mip_(solve_as_a_mip) {}

HighsInterface::~HighsInterface() {}

MPSolver::ResultStatus HighsInterface::Solve(const MPSolverParameters& param) {
  // Reset extraction as this interface is not incremental yet.
  Reset();
  ExtractModel();

  // Mark variables and constraints as extracted.
  for (int i = 0; i < solver_->variables_.size(); ++i) {
    set_variable_as_extracted(i, true);
  }
  for (int i = 0; i < solver_->constraints_.size(); ++i) {
    set_constraint_as_extracted(i, true);
  }

  MPModelProto model_proto;
  solver_->ExportModelToProto(&model_proto);
  MPModelRequest request;
  *request.mutable_model() = std::move(model_proto);
  request.set_solver_type(solve_as_a_mip_
                              ? MPModelRequest::HIGHS_MIXED_INTEGER_PROGRAMMING
                              : MPModelRequest::HIGHS_LINEAR_PROGRAMMING);

  SetParameters(param);
  request.set_enable_internal_solver_output(!quiet_);
  request.set_solver_specific_parameters(
      solver_->solver_specific_parameter_string_);
  if (solver_->time_limit()) {
    request.set_solver_time_limit_seconds(
        static_cast<double>(solver_->time_limit()) / 1000.0);
  }

  // Set parameters.
  solve_info_ = HighsSolveInfo();
  absl::StatusOr<MPSolutionResponse> response =
      HighsSolveProto(std::move(request), &*solve_info_);

  if (!response.ok()) {
    LOG(ERROR) << "Unexpected error solving with Highs: " << response.status();
    return MPSolver::ABNORMAL;
  }

  // The solution must be marked as synchronized even when no solution exists.
  sync_status_ = SOLUTION_SYNCHRONIZED;
  result_status_ = static_cast<MPSolver::ResultStatus>(response->status());

  if (response->status() == MPSOLVER_FEASIBLE ||
      response->status() == MPSOLVER_OPTIMAL) {
    const absl::Status result = solver_->LoadSolutionFromProto(*response);
    if (!result.ok()) {
      LOG(ERROR) << "LoadSolutionFromProto failed: " << result;
    }
  }

  return result_status_;
}

void HighsInterface::Reset() {
  ResetExtractionInformation();
  solve_info_.reset();
}

void HighsInterface::SetOptimizationDirection(bool maximize) {
  NonIncrementalChange();
}

void HighsInterface::SetVariableBounds(int index, double lb, double ub) {
  NonIncrementalChange();
}

void HighsInterface::SetVariableInteger(int index, bool integer) {
  NonIncrementalChange();
}

void HighsInterface::SetConstraintBounds(int index, double lb, double ub) {
  NonIncrementalChange();
}

void HighsInterface::AddRowConstraint(MPConstraint* const ct) {
  NonIncrementalChange();
}

void HighsInterface::AddVariable(MPVariable* const var) {
  NonIncrementalChange();
}

void HighsInterface::SetCoefficient(MPConstraint* const constraint,
                                    const MPVariable* const variable,
                                    double new_value, double old_value) {
  NonIncrementalChange();
}

void HighsInterface::ClearConstraint(MPConstraint* const constraint) {
  NonIncrementalChange();
}

void HighsInterface::SetObjectiveCoefficient(const MPVariable* const variable,
                                             double coefficient) {
  NonIncrementalChange();
}

void HighsInterface::SetObjectiveOffset(double value) {
  NonIncrementalChange();
}

void HighsInterface::ClearObjective() { NonIncrementalChange(); }

int64_t HighsInterface::iterations() const {
  return 0;  // FIXME.
}

int64_t HighsInterface::nodes() const {
  QCHECK(solve_info_.has_value())
      << "Number of nodes only available after solve";
  return solve_info_->mip_node_count;
}

MPSolver::BasisStatus HighsInterface::row_status(int constraint_index) const {
  // TODO(user): While basis status isn't well defined for PDLP, we could
  // guess statuses that might be useful.
  return MPSolver::BasisStatus::FREE;
}

MPSolver::BasisStatus HighsInterface::column_status(int variable_index) const {
  // TODO(user): While basis status isn't well defined for PDLP, we could
  // guess statuses that might be useful.
  return MPSolver::BasisStatus::FREE;
}

bool HighsInterface::IsContinuous() const { return true; }

bool HighsInterface::IsLP() const { return true; }

bool HighsInterface::IsMIP() const { return solve_as_a_mip_; }

std::string HighsInterface::SolverVersion() const { return "PDLP Solver"; }

// TODO(user): Consider returning the SolveLog here, as it could be essential
// for interpreting the PDLP solution.
void* HighsInterface::underlying_solver() { return nullptr; }

void HighsInterface::ExtractNewVariables() { NonIncrementalChange(); }

void HighsInterface::ExtractNewConstraints() { NonIncrementalChange(); }

void HighsInterface::ExtractObjective() { NonIncrementalChange(); }

void HighsInterface::SetParameters(const MPSolverParameters& param) {
  SetCommonParameters(param);
}

absl::Status HighsInterface::SetNumThreads(int num_threads) {
  if (num_threads < 1) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid number of threads: ", num_threads));
  }
  // parameters_.set_num_threads(num_threads);
  return absl::OkStatus();
}

// These have no effect. Use SetSolverSpecificParametersAsString instead.
void HighsInterface::SetPrimalTolerance(double value) {}
void HighsInterface::SetDualTolerance(double value) {}
void HighsInterface::SetScalingMode(int value) {}
void HighsInterface::SetLpAlgorithm(int value) {}
void HighsInterface::SetRelativeMipGap(double value) {}
void HighsInterface::SetPresolveMode(int value) {}

bool HighsInterface::SetSolverSpecificParametersAsString(
    const std::string& parameters) {
  // return ProtobufTextFormatMergeFromString(parameters, &parameters_);
  return false;
}

void HighsInterface::NonIncrementalChange() {
  // The current implementation is not incremental.
  sync_status_ = MUST_RELOAD;
}

// Register PDLP in the global linear solver factory.
MPSolverInterface* BuildHighsInterface(bool mip, MPSolver* const solver) {
  return new HighsInterface(solver, mip);
}

}  // namespace operations_research
#endif  //  #if defined(USE_HIGHS)
