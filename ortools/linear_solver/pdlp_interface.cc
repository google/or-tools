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

#if defined(USE_PDLP)

#include <atomic>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/types/optional.h"
#include "ortools/base/logging.h"
#include "ortools/linear_solver/linear_solver.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/linear_solver/proto_solver/pdlp_proto_solver.h"
#include "ortools/linear_solver/proto_solver/proto_utils.h"
#include "ortools/pdlp/solve_log.pb.h"
#include "ortools/pdlp/solvers.pb.h"
#include "ortools/port/proto_utils.h"
#include "ortools/util/lazy_mutable_copy.h"

namespace operations_research {

class PdlpInterface : public MPSolverInterface {
 public:
  explicit PdlpInterface(MPSolver* solver);
  ~PdlpInterface() override;

  // ----- Solve -----
  MPSolver::ResultStatus Solve(const MPSolverParameters& param) override;

  bool SupportsDirectlySolveProto(std::atomic<bool>* interrupt) const override {
    return true;
  }
  MPSolutionResponse DirectlySolveProto(LazyMutableCopy<MPModelRequest> request,
                                        std::atomic<bool>* interrupt) override {
    const bool log_error = request->enable_internal_solver_output();
    return ConvertStatusOrMPSolutionResponse(
        log_error, PdlpSolveProto(std::move(request),
                                  /*relax_integer_variables=*/true, interrupt));
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
  bool InterruptSolve() override;

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

  pdlp::PrimalDualHybridGradientParams parameters_;
  pdlp::SolveLog solve_log_;
  std::atomic<bool> interrupt_solver_;
};

PdlpInterface::PdlpInterface(MPSolver* const solver)
    : MPSolverInterface(solver), interrupt_solver_(false) {}

PdlpInterface::~PdlpInterface() {}

MPSolver::ResultStatus PdlpInterface::Solve(const MPSolverParameters& param) {
  // Reset extraction as this interface is not incremental yet.
  Reset();
  interrupt_solver_ = false;
  ExtractModel();

  SetParameters(param);
  if (quiet_) {
    parameters_.set_verbosity_level(0);
  } else {
    parameters_.set_verbosity_level(3);
  }

  solver_->SetSolverSpecificParametersAsString(
      solver_->solver_specific_parameter_string_);

  // Time limit.
  if (solver_->time_limit()) {
    VLOG(1) << "Setting time limit = " << solver_->time_limit() << " ms.";
    parameters_.mutable_termination_criteria()->set_time_sec_limit(
        static_cast<double>(solver_->time_limit()) / 1000.0);
  }

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
  if (!google::protobuf::TextFormat::PrintToString(
          parameters_, request.mutable_solver_specific_parameters())) {
    LOG(QFATAL) << "Error converting parameters to text format: "
                << ProtobufDebugString(parameters_);
  }
  absl::StatusOr<MPSolutionResponse> response = PdlpSolveProto(
      std::move(request), /*relax_integer_variables=*/true, &interrupt_solver_);

  if (!response.ok()) {
    LOG(ERROR) << "Unexpected error solving with PDLP: " << response.status();
    return MPSolver::ABNORMAL;
  }

  // The solution must be marked as synchronized even when no solution exists.
  sync_status_ = SOLUTION_SYNCHRONIZED;
  if (response->status() == MPSOLVER_CANCELLED_BY_USER) {
    // MPSOLVER_CANCELLED_BY_USER is only for when the solver didn't have time
    // to return a proper status, and is not convertible to an MPSolver status.
    result_status_ = MPSolver::NOT_SOLVED;
  } else {
    result_status_ = static_cast<MPSolver::ResultStatus>(response->status());
  }
  if (response->has_solver_specific_info()) {
    if (!solve_log_.ParseFromString(response->solver_specific_info())) {
      LOG(DFATAL)
          << "Unable to parse PDLP's SolveLog from solver_specific_info";
    }
  }

  if (response->status() == MPSOLVER_FEASIBLE ||
      response->status() == MPSOLVER_OPTIMAL) {
    const absl::Status result = solver_->LoadSolutionFromProto(*response);
    if (!result.ok()) {
      LOG(ERROR) << "LoadSolutionFromProto failed: " << result;
    }
  }

  return result_status_;
}

void PdlpInterface::Reset() { ResetExtractionInformation(); }

void PdlpInterface::SetOptimizationDirection(bool maximize) {
  NonIncrementalChange();
}

void PdlpInterface::SetVariableBounds(int index, double lb, double ub) {
  NonIncrementalChange();
}

void PdlpInterface::SetVariableInteger(int index, bool integer) {
  NonIncrementalChange();
}

void PdlpInterface::SetConstraintBounds(int index, double lb, double ub) {
  NonIncrementalChange();
}

void PdlpInterface::AddRowConstraint(MPConstraint* const ct) {
  NonIncrementalChange();
}

void PdlpInterface::AddVariable(MPVariable* const var) {
  NonIncrementalChange();
}

void PdlpInterface::SetCoefficient(MPConstraint* const constraint,
                                   const MPVariable* const variable,
                                   double new_value, double old_value) {
  NonIncrementalChange();
}

void PdlpInterface::ClearConstraint(MPConstraint* const constraint) {
  NonIncrementalChange();
}

void PdlpInterface::SetObjectiveCoefficient(const MPVariable* const variable,
                                            double coefficient) {
  NonIncrementalChange();
}

void PdlpInterface::SetObjectiveOffset(double value) { NonIncrementalChange(); }

void PdlpInterface::ClearObjective() { NonIncrementalChange(); }

int64_t PdlpInterface::iterations() const {
  return solve_log_.iteration_count();
}

int64_t PdlpInterface::nodes() const {
  LOG(DFATAL) << "Number of nodes only available for discrete problems";
  return MPSolverInterface::kUnknownNumberOfNodes;
}

MPSolver::BasisStatus PdlpInterface::row_status(int constraint_index) const {
  // TODO(user): While basis status isn't well defined for PDLP, we could
  // guess statuses that might be useful.
  return MPSolver::BasisStatus::FREE;
}

MPSolver::BasisStatus PdlpInterface::column_status(int variable_index) const {
  // TODO(user): While basis status isn't well defined for PDLP, we could
  // guess statuses that might be useful.
  return MPSolver::BasisStatus::FREE;
}

bool PdlpInterface::IsContinuous() const { return true; }

bool PdlpInterface::IsLP() const { return true; }

bool PdlpInterface::IsMIP() const { return false; }

std::string PdlpInterface::SolverVersion() const { return "PDLP Solver"; }

// TODO(user): Consider returning the SolveLog here, as it could be essential
// for interpreting the PDLP solution.
void* PdlpInterface::underlying_solver() { return nullptr; }

bool PdlpInterface::InterruptSolve() {
  interrupt_solver_ = true;
  return true;
}

void PdlpInterface::ExtractNewVariables() { NonIncrementalChange(); }

void PdlpInterface::ExtractNewConstraints() { NonIncrementalChange(); }

void PdlpInterface::ExtractObjective() { NonIncrementalChange(); }

void PdlpInterface::SetParameters(const MPSolverParameters& param) {
  SetCommonParameters(param);
}

absl::Status PdlpInterface::SetNumThreads(int num_threads) {
  if (num_threads < 1) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid number of threads: ", num_threads));
  }
  parameters_.set_num_threads(num_threads);
  return absl::OkStatus();
}

// These have no effect. Use SetSolverSpecificParametersAsString instead.
void PdlpInterface::SetPrimalTolerance(double value) {}
void PdlpInterface::SetDualTolerance(double value) {}
void PdlpInterface::SetScalingMode(int value) {}
void PdlpInterface::SetLpAlgorithm(int value) {}
void PdlpInterface::SetRelativeMipGap(double value) {}
void PdlpInterface::SetPresolveMode(int value) {}

bool PdlpInterface::SetSolverSpecificParametersAsString(
    const std::string& parameters) {
  return ProtobufTextFormatMergeFromString(parameters, &parameters_);
}

void PdlpInterface::NonIncrementalChange() {
  // The current implementation is not incremental.
  sync_status_ = MUST_RELOAD;
}

// Register PDLP in the global linear solver factory.
MPSolverInterface* BuildPdlpInterface(MPSolver* const solver) {
  return new PdlpInterface(solver);
}

}  // namespace operations_research
#endif  //  #if defined(USE_PDLP)
