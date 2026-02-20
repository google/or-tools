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

#include "ortools/glop/lp_solver.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <string>

#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/log/vlog_is_on.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "google/protobuf/text_format.h"
#include "ortools/base/macros/os_support.h"
#include "ortools/base/version.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/glop/preprocessor.h"
#include "ortools/glop/revised_simplex.h"
#include "ortools/glop/status.h"
#include "ortools/glop/variables_info.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/lp_utils.h"
#include "ortools/lp_data/proto_utils.h"
#include "ortools/port/proto_utils.h"
#include "ortools/util/fp_utils.h"
#include "ortools/util/logging.h"

#if defined(ORTOOLS_TARGET_OS_SUPPORTS_FILE)
static_assert(operations_research::kTargetOsSupportsFile);
// TODO(user): abstract this in some way to the port directory.
#include "ortools/util/file_util.h"
#else
static_assert(!operations_research::kTargetOsSupportsFile);
#endif

ABSL_FLAG(bool, lp_dump_to_proto_file, false,
          "Tells whether do dump the problem to a protobuf file.");
ABSL_FLAG(bool, lp_dump_compressed_file, true,
          "Whether the proto dump file is compressed.");
ABSL_FLAG(bool, lp_dump_binary_file, false,
          "Whether the proto dump file is binary.");
ABSL_FLAG(int, lp_dump_file_number, -1,
          "Number for the dump file, in the form name-000048.pb. "
          "If < 0, the file is automatically numbered from the number of "
          "calls to LPSolver::Solve().");
ABSL_FLAG(std::string, lp_dump_dir, "/tmp",
          "Directory where dump files are written.");
ABSL_FLAG(std::string, lp_dump_file_basename, "",
          "Base name for dump files. LinearProgram::name_ is used if "
          "lp_dump_file_basename is empty. If LinearProgram::name_ is "
          "empty, \"linear_program_dump_file\" is used.");
ABSL_FLAG(std::string, glop_params, "",
          "Override any user parameters with the value of this flag. This is "
          "interpreted as a GlopParameters proto in text format.");

namespace operations_research {
namespace glop {
namespace {

// Writes a LinearProgram to a file if FLAGS_lp_dump_to_proto_file is true. The
// integer num is appended to the base name of the file. When this function is
// called from LPSolver::Solve(), num is usually the number of times Solve() was
// called. For a LinearProgram whose name is "LinPro", and num = 48, the default
// output file will be /tmp/LinPro-000048.pb.gz.
//
// Warning: is a no-op on portable platforms (android, ios, etc).
void DumpLinearProgramIfRequiredByFlags(const LinearProgram& linear_program,
                                        int num) {
  if (!absl::GetFlag(FLAGS_lp_dump_to_proto_file)) return;
#if defined(ORTOOLS_TARGET_OS_SUPPORTS_FILE)
  static_assert(kTargetOsSupportsFile);
  std::string filename = absl::GetFlag(FLAGS_lp_dump_file_basename);
  if (filename.empty()) {
    if (linear_program.name().empty()) {
      filename = "linear_program_dump";
    } else {
      filename = linear_program.name();
    }
  }
  const int file_num = absl::GetFlag(FLAGS_lp_dump_file_number) >= 0
                           ? absl::GetFlag(FLAGS_lp_dump_file_number)
                           : num;
  absl::StrAppendFormat(&filename, "-%06d.pb", file_num);
  const std::string filespec =
      absl::StrCat(absl::GetFlag(FLAGS_lp_dump_dir), "/", filename);
  MPModelProto proto;
  LinearProgramToMPModelProto(linear_program, &proto);
  const ProtoWriteFormat write_format = absl::GetFlag(FLAGS_lp_dump_binary_file)
                                            ? ProtoWriteFormat::kProtoBinary
                                            : ProtoWriteFormat::kProtoText;
  CHECK_OK(WriteProtoToFile(filespec, proto, write_format,
                            absl::GetFlag(FLAGS_lp_dump_compressed_file)));
#else
  static_assert(!kTargetOsSupportsFile);
  LOG(WARNING) << "DumpLinearProgramIfRequiredByFlags(linear_program, num) "
                  "requested for linear_program.name()='"
               << linear_program.name() << "', num=" << num
               << " but is not implemented for this platform.";
#endif
}

}  // anonymous namespace

// --------------------------------------------------------
// LPSolver
// --------------------------------------------------------

LPSolver::LPSolver() : num_solves_(0) {}

std::string LPSolver::GlopVersion() {
  return absl::StrCat("Glop solver v", OrToolsVersionString());
}

void LPSolver::SetParameters(const GlopParameters& parameters) {
  parameters_ = parameters;
#if defined(ORTOOLS_TARGET_OS_SUPPORTS_PROTO_DESCRIPTOR)
  static_assert(kTargetOsSupportsProtoDescriptor);
  if (!absl::GetFlag(FLAGS_glop_params).empty()) {
    GlopParameters flag_params;
    CHECK(google::protobuf::TextFormat::ParseFromString(
        absl::GetFlag(FLAGS_glop_params), &flag_params));
    parameters_.MergeFrom(flag_params);
  }
#else
  static_assert(!kTargetOsSupportsProtoDescriptor);
#endif
}

const GlopParameters& LPSolver::GetParameters() const { return parameters_; }

GlopParameters* LPSolver::GetMutableParameters() { return &parameters_; }

SolverLogger& LPSolver::GetSolverLogger() { return logger_; }

ProblemStatus LPSolver::Solve(const LinearProgram& lp) {
  std::unique_ptr<TimeLimit> time_limit =
      TimeLimit::FromParameters(parameters_);
  return SolveWithTimeLimit(lp, time_limit.get());
}

ProblemStatus LPSolver::SolveWithTimeLimit(const LinearProgram& lp,
                                           TimeLimit* time_limit) {
  if (time_limit == nullptr) {
    LOG(DFATAL) << "SolveWithTimeLimit() called with a nullptr time_limit.";
    return ProblemStatus::ABNORMAL;
  }
  ++num_solves_;
  num_revised_simplex_iterations_ = 0;
  DumpLinearProgramIfRequiredByFlags(lp, num_solves_);

  // Display a warning if running in non-opt, unless we're inside a unit test.
  DLOG(WARNING)
      << "\n******************************************************************"
         "\n* WARNING: Glop will be very slow because it will use DCHECKs    *"
         "\n* to verify the results and the precision of the solver.         *"
         "\n* You can gain at least an order of magnitude speedup by         *"
         "\n* compiling with optimizations enabled and by defining NDEBUG.   *"
         "\n******************************************************************";

  // Setup the logger.
  logger_.EnableLogging(parameters_.log_search_progress());
  logger_.SetLogToStdOut(parameters_.log_to_stdout());
  if (!parameters_.log_search_progress() && VLOG_IS_ON(1)) {
    logger_.EnableLogging(true);
    logger_.SetLogToStdOut(false);
  }

  // Log some initial info about the input model.
  if (logger_.LoggingIsEnabled()) {
    SOLVER_LOG(&logger_, "");
    SOLVER_LOG(&logger_, "Initial problem: ", lp.GetDimensionString());
    SOLVER_LOG(&logger_, "Objective stats: ", lp.GetObjectiveStatsString());
    SOLVER_LOG(&logger_, "Bounds stats: ", lp.GetBoundsStatsString());
    SOLVER_LOG(&logger_, "Parameters: ", ProtobufShortDebugString(parameters_));
  }

  // Check some preconditions.
  if (!lp.IsCleanedUp()) {
    LOG(DFATAL) << "The columns of the given linear program should be ordered "
                << "by row and contain no zero coefficients. Call CleanUp() "
                << "on it before calling Solve().";
    ResizeSolution(lp.num_constraints(), lp.num_variables());
    return ProblemStatus::INVALID_PROBLEM;
  }

  // TODO(user): Unfortunately we are not really helpful with the error message
  // here. We could do a better job. However most client should talk to glop via
  // an input protocol buffer which should have better validation messages.
  if (!lp.IsValid(parameters_.max_valid_magnitude())) {
    SOLVER_LOG(&logger_,
               "The given linear program is invalid. It contains NaNs, "
               "coefficients too large or invalid bounds specification.");
    ResizeSolution(lp.num_constraints(), lp.num_variables());
    return ProblemStatus::INVALID_PROBLEM;
  }

  // Make an internal copy of the problem for the preprocessing.
  current_linear_program_.PopulateFromLinearProgram(lp);

  // Remove small entries even if presolve is off. This is mainly here to
  // avoid floating point underflow. Keeping them can break many invariant like
  // a * b == 0 iff a == 0 or b == 0.
  //
  // Note that our presolve/scaling can potentially create smaller entries than
  // this, but the scale should stay reasonable.
  //
  // TODO(user): If speed matter, we could do that as we copy the program.
  current_linear_program_.RemoveNearZeroEntries(parameters_.drop_magnitude());

  // Preprocess.
  MainLpPreprocessor preprocessor(&parameters_);
  preprocessor.SetLogger(&logger_);
  preprocessor.SetTimeLimit(time_limit);

  const bool postsolve_is_needed = preprocessor.Run(&current_linear_program_);

  if (logger_.LoggingIsEnabled()) {
    SOLVER_LOG(&logger_, "");
    SOLVER_LOG(&logger_, "Presolved problem: ",
               current_linear_program_.GetDimensionString());
    SOLVER_LOG(&logger_, "Objective stats: ",
               current_linear_program_.GetObjectiveStatsString());
    SOLVER_LOG(&logger_, "Bounds stats: ",
               current_linear_program_.GetBoundsStatsString());
  }

  // At this point, we need to initialize a ProblemSolution with the correct
  // size and status.
  ProblemSolution solution(current_linear_program_.num_constraints(),
                           current_linear_program_.num_variables());
  solution.status = preprocessor.status();
  // LoadAndVerifySolution() below updates primal_values_, dual_values_,
  // variable_statuses_ and constraint_statuses_ with the values stored in
  // solution by RunPrimalDualPathFollowingMethodIfNeeded() and
  // RunRevisedSimplexIfNeeded(), and hence clears any results stored in them
  // from a previous run. In contrast, primal_ray_, constraints_dual_ray_, and
  // variable_bounds_dual_ray_ are modified directly by
  // RunRevisedSimplexIfNeeded(), so we explicitly clear them from previous run
  // results.
  primal_ray_.clear();
  constraints_dual_ray_.clear();
  variable_bounds_dual_ray_.clear();

  // Do not launch the solver if the time limit was already reached. This might
  // mean that the pre-processors were not all run, and current_linear_program_
  // might not be in a completely safe state.
  if (!time_limit->LimitReached()) {
    RunRevisedSimplexIfNeeded(&solution, time_limit);
  }
  if (postsolve_is_needed) preprocessor.DestructiveRecoverSolution(&solution);
  const ProblemStatus status = LoadAndVerifySolution(lp, solution);
  // LOG some statistics that can be parsed by our benchmark script.
  if (logger_.LoggingIsEnabled()) {
    SOLVER_LOG(&logger_, "status: ", GetProblemStatusString(status));
    SOLVER_LOG(&logger_, "objective: ", GetObjectiveValue());
    SOLVER_LOG(&logger_, "iterations: ", GetNumberOfSimplexIterations());
    SOLVER_LOG(&logger_, "time: ", time_limit->GetElapsedTime());
    SOLVER_LOG(&logger_, "deterministic_time: ",
               time_limit->GetElapsedDeterministicTime());
    SOLVER_LOG(&logger_, "");
  }

  return status;
}

void LPSolver::Clear() {
  ResizeSolution(RowIndex(0), ColIndex(0));
  revised_simplex_.reset(nullptr);
}

void LPSolver::SetInitialBasis(
    const VariableStatusRow& variable_statuses,
    const ConstraintStatusColumn& constraint_statuses) {
  // Create the associated basis state.
  BasisState state;
  state.statuses = variable_statuses;
  for (const ConstraintStatus status : constraint_statuses) {
    // Note the change of upper/lower bound between the status of a constraint
    // and the status of its associated slack variable.
    switch (status) {
      case ConstraintStatus::FREE:
        state.statuses.push_back(VariableStatus::FREE);
        break;
      case ConstraintStatus::AT_LOWER_BOUND:
        state.statuses.push_back(VariableStatus::AT_UPPER_BOUND);
        break;
      case ConstraintStatus::AT_UPPER_BOUND:
        state.statuses.push_back(VariableStatus::AT_LOWER_BOUND);
        break;
      case ConstraintStatus::FIXED_VALUE:
        state.statuses.push_back(VariableStatus::FIXED_VALUE);
        break;
      case ConstraintStatus::BASIC:
        state.statuses.push_back(VariableStatus::BASIC);
        break;
    }
  }
  if (revised_simplex_ == nullptr) {
    revised_simplex_ = std::make_unique<RevisedSimplex>();
    revised_simplex_->SetLogger(&logger_);
  }
  revised_simplex_->LoadStateForNextSolve(state);
  if (parameters_.use_preprocessing()) {
    LOG(WARNING) << "In GLOP, SetInitialBasis() was called but the parameter "
                    "use_preprocessing is true, this will likely not result in "
                    "what you want.";
  }
}

namespace {
// Computes the "real" problem objective from the one without offset nor
// scaling.
Fractional ProblemObjectiveValue(const LinearProgram& lp, Fractional value) {
  return lp.objective_scaling_factor() * (value + lp.objective_offset());
}

// Returns the allowed error magnitude for something that should evaluate to
// value under the given tolerance.
Fractional AllowedError(Fractional tolerance, Fractional value) {
  return tolerance * std::max(Fractional(1.0), std::abs(value));
}
}  // namespace

// TODO(user): Try to also check the precision of an INFEASIBLE or UNBOUNDED
// return status.
ProblemStatus LPSolver::LoadAndVerifySolution(const LinearProgram& lp,
                                              const ProblemSolution& solution) {
  SOLVER_LOG(&logger_, "");
  SOLVER_LOG(&logger_, "Final unscaled solution:");

  if (!IsProblemSolutionConsistent(lp, solution)) {
    SOLVER_LOG(&logger_, "Inconsistency detected in the solution.");
    ResizeSolution(lp.num_constraints(), lp.num_variables());
    return ProblemStatus::ABNORMAL;
  }

  // Load the solution.
  primal_values_ = solution.primal_values;
  dual_values_ = solution.dual_values;
  variable_statuses_ = solution.variable_statuses;
  constraint_statuses_ = solution.constraint_statuses;

  ProblemStatus status = solution.status;

  // Objective before eventually moving the primal/dual values inside their
  // bounds.
  ComputeReducedCosts(lp);
  const Fractional primal_objective_value = ComputeObjective(lp);
  const Fractional dual_objective_value = ComputeDualObjective(lp);
  SOLVER_LOG(&logger_, "Primal objective (before moving primal/dual values) = ",
             absl::StrFormat(
                 "%.15E", ProblemObjectiveValue(lp, primal_objective_value)));
  SOLVER_LOG(&logger_, "Dual objective (before moving primal/dual values) = ",
             absl::StrFormat("%.15E",
                             ProblemObjectiveValue(lp, dual_objective_value)));

  // Eventually move the primal/dual values inside their bounds.
  if (status == ProblemStatus::OPTIMAL &&
      parameters_.provide_strong_optimal_guarantee()) {
    MovePrimalValuesWithinBounds(lp);
    MoveDualValuesWithinBounds(lp);
  }

  // The reported objective to the user.
  problem_objective_value_ = ProblemObjectiveValue(lp, ComputeObjective(lp));
  SOLVER_LOG(&logger_, "Primal objective (after moving primal/dual values) = ",
             absl::StrFormat("%.15E", problem_objective_value_));

  ComputeReducedCosts(lp);
  ComputeConstraintActivities(lp);

  // These will be set to true if the associated "infeasibility" is too large.
  //
  // The tolerance used is the parameter solution_feasibility_tolerance. To be
  // somewhat independent of the original problem scaling, the thresholds used
  // depend of the quantity involved and of its coordinates:
  // - tolerance * max(1.0, abs(cost[col])) when a reduced cost is infeasible.
  // - tolerance * max(1.0, abs(bound)) when a bound is crossed.
  // - tolerance for an infeasible dual value (because the limit is always 0.0).
  bool rhs_perturbation_is_too_large = false;
  bool cost_perturbation_is_too_large = false;
  bool primal_infeasibility_is_too_large = false;
  bool dual_infeasibility_is_too_large = false;
  bool primal_residual_is_too_large = false;
  bool dual_residual_is_too_large = false;

  // Computes all the infeasiblities and update the Booleans above.
  ComputeMaxRhsPerturbationToEnforceOptimality(lp,
                                               &rhs_perturbation_is_too_large);
  ComputeMaxCostPerturbationToEnforceOptimality(
      lp, &cost_perturbation_is_too_large);
  const double primal_infeasibility =
      ComputePrimalValueInfeasibility(lp, &primal_infeasibility_is_too_large);
  const double dual_infeasibility =
      ComputeDualValueInfeasibility(lp, &dual_infeasibility_is_too_large);
  const double primal_residual =
      ComputeActivityInfeasibility(lp, &primal_residual_is_too_large);
  const double dual_residual =
      ComputeReducedCostInfeasibility(lp, &dual_residual_is_too_large);

  // TODO(user): the name is not really consistent since in practice those are
  // the "residual" since the primal/dual infeasibility are zero when
  // parameters_.provide_strong_optimal_guarantee() is true.
  max_absolute_primal_infeasibility_ =
      std::max(primal_infeasibility, primal_residual);
  max_absolute_dual_infeasibility_ =
      std::max(dual_infeasibility, dual_residual);
  SOLVER_LOG(&logger_, "Max. primal infeasibility = ",
             max_absolute_primal_infeasibility_);
  SOLVER_LOG(&logger_,
             "Max. dual infeasibility = ", max_absolute_dual_infeasibility_);

  // Now that all the relevant quantities are computed, we check the precision
  // and optimality of the result. See Chvatal pp. 61-62. If any of the tests
  // fail, we return the IMPRECISE status.
  const double objective_error_ub = ComputeMaxExpectedObjectiveError(lp);
  SOLVER_LOG(&logger_, "Objective error <= ", objective_error_ub);

  if (status == ProblemStatus::OPTIMAL &&
      parameters_.provide_strong_optimal_guarantee()) {
    // If the primal/dual values were moved to the bounds, then the primal/dual
    // infeasibilities should be exactly zero (but not the residuals).
    if (primal_infeasibility != 0.0 || dual_infeasibility != 0.0) {
      LOG(ERROR) << "Primal/dual values have been moved to their bounds. "
                 << "Therefore the primal/dual infeasibilities should be "
                 << "exactly zero (but not the residuals). If this message "
                 << "appears, there is probably a bug in "
                 << "MovePrimalValuesWithinBounds() or in "
                 << "MoveDualValuesWithinBounds().";
    }
    if (rhs_perturbation_is_too_large) {
      SOLVER_LOG(&logger_, "The needed rhs perturbation is too large !!");
      if (parameters_.change_status_to_imprecise()) {
        status = ProblemStatus::IMPRECISE;
      }
    }
    if (cost_perturbation_is_too_large) {
      SOLVER_LOG(&logger_, "The needed cost perturbation is too large !!");
      if (parameters_.change_status_to_imprecise()) {
        status = ProblemStatus::IMPRECISE;
      }
    }
  }

  // Note that we compare the values without offset nor scaling. We also need to
  // compare them before we move the primal/dual values, otherwise we lose some
  // precision since the values are modified independently of each other.
  if (status == ProblemStatus::OPTIMAL) {
    if (std::abs(primal_objective_value - dual_objective_value) >
        objective_error_ub) {
      SOLVER_LOG(&logger_,
                 "The objective gap of the final solution is too large.");
      if (parameters_.change_status_to_imprecise()) {
        status = ProblemStatus::IMPRECISE;
      }
    }
  }
  if ((status == ProblemStatus::OPTIMAL ||
       status == ProblemStatus::PRIMAL_FEASIBLE) &&
      (primal_residual_is_too_large || primal_infeasibility_is_too_large)) {
    SOLVER_LOG(&logger_,
               "The primal infeasibility of the final solution is too large.");
    if (parameters_.change_status_to_imprecise()) {
      status = ProblemStatus::IMPRECISE;
    }
  }
  if ((status == ProblemStatus::OPTIMAL ||
       status == ProblemStatus::DUAL_FEASIBLE) &&
      (dual_residual_is_too_large || dual_infeasibility_is_too_large)) {
    SOLVER_LOG(&logger_,
               "The dual infeasibility of the final solution is too large.");
    if (parameters_.change_status_to_imprecise()) {
      status = ProblemStatus::IMPRECISE;
    }
  }

  may_have_multiple_solutions_ =
      (status == ProblemStatus::OPTIMAL) ? IsOptimalSolutionOnFacet(lp) : false;
  return status;
}

bool LPSolver::IsOptimalSolutionOnFacet(const LinearProgram& lp) {
  // Note(user): We use the following same two tolerances for the dual and
  // primal values.
  // TODO(user): investigate whether to use the tolerances defined in
  // parameters.proto.
  const Fractional kReducedCostTolerance = 1e-9;
  const Fractional kBoundTolerance = 1e-7;
  const ColIndex num_cols = lp.num_variables();
  for (ColIndex col(0); col < num_cols; ++col) {
    if (variable_statuses_[col] == VariableStatus::FIXED_VALUE) continue;
    const Fractional lower_bound = lp.variable_lower_bounds()[col];
    const Fractional upper_bound = lp.variable_upper_bounds()[col];
    const Fractional value = primal_values_[col];
    if (AreWithinAbsoluteTolerance(reduced_costs_[col], Fractional(0.0),
                                   kReducedCostTolerance) &&
        (AreWithinAbsoluteTolerance(value, lower_bound, kBoundTolerance) ||
         AreWithinAbsoluteTolerance(value, upper_bound, kBoundTolerance))) {
      return true;
    }
  }
  const RowIndex num_rows = lp.num_constraints();
  for (RowIndex row(0); row < num_rows; ++row) {
    if (constraint_statuses_[row] == ConstraintStatus::FIXED_VALUE) continue;
    const Fractional lower_bound = lp.constraint_lower_bounds()[row];
    const Fractional upper_bound = lp.constraint_upper_bounds()[row];
    const Fractional activity = constraint_activities_[row];
    if (AreWithinAbsoluteTolerance(dual_values_[row], Fractional(0.0),
                                   kReducedCostTolerance) &&
        (AreWithinAbsoluteTolerance(activity, lower_bound, kBoundTolerance) ||
         AreWithinAbsoluteTolerance(activity, upper_bound, kBoundTolerance))) {
      return true;
    }
  }
  return false;
}

Fractional LPSolver::GetObjectiveValue() const {
  return problem_objective_value_;
}

Fractional LPSolver::GetMaximumPrimalInfeasibility() const {
  return max_absolute_primal_infeasibility_;
}

Fractional LPSolver::GetMaximumDualInfeasibility() const {
  return max_absolute_dual_infeasibility_;
}

bool LPSolver::MayHaveMultipleOptimalSolutions() const {
  return may_have_multiple_solutions_;
}

int LPSolver::GetNumberOfSimplexIterations() const {
  return num_revised_simplex_iterations_;
}

double LPSolver::DeterministicTime() const {
  return revised_simplex_ == nullptr ? 0.0
                                     : revised_simplex_->DeterministicTime();
}

void LPSolver::MovePrimalValuesWithinBounds(const LinearProgram& lp) {
  const ColIndex num_cols = lp.num_variables();
  DCHECK_EQ(num_cols, primal_values_.size());
  Fractional error = 0.0;
  for (ColIndex col(0); col < num_cols; ++col) {
    const Fractional lower_bound = lp.variable_lower_bounds()[col];
    const Fractional upper_bound = lp.variable_upper_bounds()[col];
    DCHECK_LE(lower_bound, upper_bound);

    error = std::max(error, primal_values_[col] - upper_bound);
    error = std::max(error, lower_bound - primal_values_[col]);
    primal_values_[col] = std::min(primal_values_[col], upper_bound);
    primal_values_[col] = std::max(primal_values_[col], lower_bound);
  }
  SOLVER_LOG(&logger_, "Max. primal values move = ", error);
}

void LPSolver::MoveDualValuesWithinBounds(const LinearProgram& lp) {
  const RowIndex num_rows = lp.num_constraints();
  DCHECK_EQ(num_rows, dual_values_.size());
  const Fractional optimization_sign = lp.IsMaximizationProblem() ? -1.0 : 1.0;
  Fractional error = 0.0;
  for (RowIndex row(0); row < num_rows; ++row) {
    const Fractional lower_bound = lp.constraint_lower_bounds()[row];
    const Fractional upper_bound = lp.constraint_upper_bounds()[row];

    // For a minimization problem, we want a lower bound.
    Fractional minimization_dual_value = optimization_sign * dual_values_[row];
    if (lower_bound == -kInfinity && minimization_dual_value > 0.0) {
      error = std::max(error, minimization_dual_value);
      minimization_dual_value = 0.0;
    }
    if (upper_bound == kInfinity && minimization_dual_value < 0.0) {
      error = std::max(error, -minimization_dual_value);
      minimization_dual_value = 0.0;
    }
    dual_values_[row] = optimization_sign * minimization_dual_value;
  }
  SOLVER_LOG(&logger_, "Max. dual values move = ", error);
}

void LPSolver::ResizeSolution(RowIndex num_rows, ColIndex num_cols) {
  primal_values_.resize(num_cols, 0.0);
  reduced_costs_.resize(num_cols, 0.0);
  variable_statuses_.resize(num_cols, VariableStatus::FREE);

  dual_values_.resize(num_rows, 0.0);
  constraint_activities_.resize(num_rows, 0.0);
  constraint_statuses_.resize(num_rows, ConstraintStatus::FREE);
}

void LPSolver::RunRevisedSimplexIfNeeded(ProblemSolution* solution,
                                         TimeLimit* time_limit) {
  // Note that the transpose matrix is no longer needed at this point.
  // This helps reduce the peak memory usage of the solver.
  //
  // TODO(user): actually, once the linear_program is loaded into the internal
  // glop memory, there is no point keeping it around. Add a more complex
  // Load/Solve API to RevisedSimplex so we can completely reclaim its memory
  // right away.
  current_linear_program_.ClearTransposeMatrix();
  if (solution->status != ProblemStatus::INIT) return;
  if (revised_simplex_ == nullptr) {
    revised_simplex_ = std::make_unique<RevisedSimplex>();
    revised_simplex_->SetLogger(&logger_);
  }
  revised_simplex_->SetParameters(parameters_);
  if (revised_simplex_->Solve(current_linear_program_, time_limit).ok()) {
    num_revised_simplex_iterations_ = revised_simplex_->GetNumberOfIterations();
    solution->status = revised_simplex_->GetProblemStatus();

    // Make sure we do not copy the slacks added by revised_simplex_.
    const ColIndex num_cols = solution->primal_values.size();
    DCHECK_LE(num_cols, revised_simplex_->GetProblemNumCols());
    for (ColIndex col(0); col < num_cols; ++col) {
      solution->primal_values[col] = revised_simplex_->GetVariableValue(col);
      solution->variable_statuses[col] =
          revised_simplex_->GetVariableStatus(col);
    }
    const RowIndex num_rows = revised_simplex_->GetProblemNumRows();
    DCHECK_EQ(solution->dual_values.size(), num_rows);
    for (RowIndex row(0); row < num_rows; ++row) {
      solution->dual_values[row] = revised_simplex_->GetDualValue(row);
      solution->constraint_statuses[row] =
          revised_simplex_->GetConstraintStatus(row);
    }
    if (!parameters_.use_preprocessing() && !parameters_.use_scaling()) {
      if (solution->status == ProblemStatus::PRIMAL_UNBOUNDED) {
        primal_ray_ = revised_simplex_->GetPrimalRay();
        // Make sure we do not copy the slacks added by revised_simplex_.
        primal_ray_.resize(num_cols);
      } else if (solution->status == ProblemStatus::DUAL_UNBOUNDED) {
        constraints_dual_ray_ = revised_simplex_->GetDualRay();
        variable_bounds_dual_ray_ =
            revised_simplex_->GetDualRayRowCombination();
        // Make sure we do not copy the slacks added by revised_simplex_.
        variable_bounds_dual_ray_.resize(num_cols);
        // Revised simplex's GetDualRay is always such that GetDualRay.rhs < 0,
        // which is a cost improving direction for the dual if the primal is a
        // maximization problem (i.e. when the dual is a minimization problem).
        // Hence, we change the sign of constraints_dual_ray_ for min problems.
        //
        // Revised simplex's GetDualRayRowCombination = A^T GetDualRay and
        // we must have variable_bounds_dual_ray_ = - A^T constraints_dual_ray_.
        // Then we need to change the sign of variable_bounds_dual_ray_, but for
        // min problems this change is implicit because of the sign change of
        // constraints_dual_ray_ described above.
        if (current_linear_program_.IsMaximizationProblem()) {
          ChangeSign(&variable_bounds_dual_ray_);
        } else {
          ChangeSign(&constraints_dual_ray_);
        }
      }
    }
  } else {
    SOLVER_LOG(&logger_, "Error during the revised simplex algorithm.");
    solution->status = ProblemStatus::ABNORMAL;
  }
}

namespace {

void LogVariableStatusError(ColIndex col, Fractional value,
                            VariableStatus status, Fractional lb,
                            Fractional ub) {
  VLOG(1) << "Variable " << col << " status is "
          << GetVariableStatusString(status) << " but its value is " << value
          << " and its bounds are [" << lb << ", " << ub << "].";
}

void LogConstraintStatusError(RowIndex row, ConstraintStatus status,
                              Fractional lb, Fractional ub) {
  VLOG(1) << "Constraint " << row << " status is "
          << GetConstraintStatusString(status) << " but its bounds are [" << lb
          << ", " << ub << "].";
}

}  // namespace

bool LPSolver::IsProblemSolutionConsistent(
    const LinearProgram& lp, const ProblemSolution& solution) const {
  const RowIndex num_rows = lp.num_constraints();
  const ColIndex num_cols = lp.num_variables();
  if (solution.variable_statuses.size() != num_cols) return false;
  if (solution.constraint_statuses.size() != num_rows) return false;
  if (solution.primal_values.size() != num_cols) return false;
  if (solution.dual_values.size() != num_rows) return false;
  if (solution.status != ProblemStatus::OPTIMAL &&
      solution.status != ProblemStatus::PRIMAL_FEASIBLE &&
      solution.status != ProblemStatus::DUAL_FEASIBLE) {
    return true;
  }

  // This checks that the variable statuses verify the properties described
  // in the VariableStatus declaration.
  RowIndex num_basic_variables(0);
  for (ColIndex col(0); col < num_cols; ++col) {
    const Fractional value = solution.primal_values[col];
    const Fractional lb = lp.variable_lower_bounds()[col];
    const Fractional ub = lp.variable_upper_bounds()[col];
    const VariableStatus status = solution.variable_statuses[col];
    switch (solution.variable_statuses[col]) {
      case VariableStatus::BASIC:
        // TODO(user): Check that the reduced cost of this column is epsilon
        // close to zero.
        ++num_basic_variables;
        break;
      case VariableStatus::FIXED_VALUE:
        // TODO(user): Because of scaling, it is possible that a FIXED_VALUE
        // status (only reserved for the exact lb == ub case) is now set for a
        // variable where (ub == lb + epsilon). So we do not check here that the
        // two bounds are exactly equal. The best is probably to remove the
        // FIXED status from the API completely and report one of AT_LOWER_BOUND
        // or AT_UPPER_BOUND instead. This also allows to indicate if at
        // optimality, the objective is limited because of this variable lower
        // bound or its upper bound. Note that there are other TODOs in the
        // codebase about removing this FIXED_VALUE status.
        if (value != ub && value != lb) {
          LogVariableStatusError(col, value, status, lb, ub);
          return false;
        }
        break;
      case VariableStatus::AT_LOWER_BOUND:
        if (value != lb || lb == ub) {
          LogVariableStatusError(col, value, status, lb, ub);
          return false;
        }
        break;
      case VariableStatus::AT_UPPER_BOUND:
        // TODO(user): revert to an exact comparison once the bug causing this
        // to fail has been fixed.
        if (!AreWithinAbsoluteTolerance(value, ub, Fractional(1e-7)) ||
            lb == ub) {
          LogVariableStatusError(col, value, status, lb, ub);
          return false;
        }
        break;
      case VariableStatus::FREE:
        if (lb != -kInfinity || ub != kInfinity || value != 0.0) {
          LogVariableStatusError(col, value, status, lb, ub);
          return false;
        }
        break;
    }
  }
  for (RowIndex row(0); row < num_rows; ++row) {
    const Fractional dual_value = solution.dual_values[row];
    const Fractional lb = lp.constraint_lower_bounds()[row];
    const Fractional ub = lp.constraint_upper_bounds()[row];
    const ConstraintStatus status = solution.constraint_statuses[row];

    // The activity value is not checked since it is imprecise.
    // TODO(user): Check that the activity is epsilon close to the expected
    // value.
    switch (status) {
      case ConstraintStatus::BASIC:
        if (dual_value != 0.0) {
          VLOG(1) << "Constraint " << row << " is BASIC, but its dual value is "
                  << dual_value << " instead of 0.";
          return false;
        }
        ++num_basic_variables;
        break;
      case ConstraintStatus::FIXED_VALUE:
        // Exactly the same remark as for the VariableStatus::FIXED_VALUE case
        // above. Because of precision error, this can happen when the
        // difference between the two bounds is small and not just exactly zero.
        if (ub - lb > 1e-12) {
          LogConstraintStatusError(row, status, lb, ub);
          return false;
        }
        break;
      case ConstraintStatus::AT_LOWER_BOUND:
        if (lb == -kInfinity) {
          LogConstraintStatusError(row, status, lb, ub);
          return false;
        }
        break;
      case ConstraintStatus::AT_UPPER_BOUND:
        if (ub == kInfinity) {
          LogConstraintStatusError(row, status, lb, ub);
          return false;
        }
        break;
      case ConstraintStatus::FREE:
        if (dual_value != 0.0) {
          VLOG(1) << "Constraint " << row << " is FREE, but its dual value is "
                  << dual_value << " instead of 0.";
          return false;
        }
        if (lb != -kInfinity || ub != kInfinity) {
          LogConstraintStatusError(row, status, lb, ub);
          return false;
        }
        break;
    }
  }

  // TODO(user): We could check in debug mode (because it will be costly) that
  // the basis is actually factorizable.
  if (num_basic_variables != num_rows) {
    VLOG(1) << "Wrong number of basic variables: " << num_basic_variables;
    return false;
  }
  return true;
}

// This computes by how much the objective must be perturbed to enforce the
// following complementary slackness conditions:
// - Reduced cost is exactly zero for FREE and BASIC variables.
// - Reduced cost is of the correct sign for variables at their bounds.
Fractional LPSolver::ComputeMaxCostPerturbationToEnforceOptimality(
    const LinearProgram& lp, bool* is_too_large) {
  Fractional max_cost_correction = 0.0;
  const ColIndex num_cols = lp.num_variables();
  const Fractional optimization_sign = lp.IsMaximizationProblem() ? -1.0 : 1.0;
  const Fractional tolerance = parameters_.solution_feasibility_tolerance();
  for (ColIndex col(0); col < num_cols; ++col) {
    // We correct the reduced cost, so we have a minimization problem and
    // thus the dual objective value will be a lower bound of the primal
    // objective.
    const Fractional reduced_cost = optimization_sign * reduced_costs_[col];
    const VariableStatus status = variable_statuses_[col];
    if (status == VariableStatus::BASIC || status == VariableStatus::FREE ||
        (status == VariableStatus::AT_UPPER_BOUND && reduced_cost > 0.0) ||
        (status == VariableStatus::AT_LOWER_BOUND && reduced_cost < 0.0)) {
      max_cost_correction =
          std::max(max_cost_correction, std::abs(reduced_cost));
      *is_too_large |=
          std::abs(reduced_cost) >
          AllowedError(tolerance, lp.objective_coefficients()[col]);
    }
  }
  SOLVER_LOG(&logger_, "Max. cost perturbation = ", max_cost_correction);
  return max_cost_correction;
}

// This computes by how much the rhs must be perturbed to enforce the fact that
// the constraint activities exactly reflect their status.
Fractional LPSolver::ComputeMaxRhsPerturbationToEnforceOptimality(
    const LinearProgram& lp, bool* is_too_large) {
  Fractional max_rhs_correction = 0.0;
  const RowIndex num_rows = lp.num_constraints();
  const Fractional tolerance = parameters_.solution_feasibility_tolerance();
  for (RowIndex row(0); row < num_rows; ++row) {
    const Fractional lower_bound = lp.constraint_lower_bounds()[row];
    const Fractional upper_bound = lp.constraint_upper_bounds()[row];
    const Fractional activity = constraint_activities_[row];
    const ConstraintStatus status = constraint_statuses_[row];

    Fractional rhs_error = 0.0;
    Fractional allowed_error = 0.0;
    if (status == ConstraintStatus::AT_LOWER_BOUND || activity < lower_bound) {
      rhs_error = std::abs(activity - lower_bound);
      allowed_error = AllowedError(tolerance, lower_bound);
    } else if (status == ConstraintStatus::AT_UPPER_BOUND ||
               activity > upper_bound) {
      rhs_error = std::abs(activity - upper_bound);
      allowed_error = AllowedError(tolerance, upper_bound);
    }
    max_rhs_correction = std::max(max_rhs_correction, rhs_error);
    *is_too_large |= rhs_error > allowed_error;
  }
  SOLVER_LOG(&logger_, "Max. rhs perturbation = ", max_rhs_correction);
  return max_rhs_correction;
}

void LPSolver::ComputeConstraintActivities(const LinearProgram& lp) {
  const RowIndex num_rows = lp.num_constraints();
  const ColIndex num_cols = lp.num_variables();
  DCHECK_EQ(num_cols, primal_values_.size());
  constraint_activities_.assign(num_rows, 0.0);
  for (ColIndex col(0); col < num_cols; ++col) {
    lp.GetSparseColumn(col).AddMultipleToDenseVector(primal_values_[col],
                                                     &constraint_activities_);
  }
}

void LPSolver::ComputeReducedCosts(const LinearProgram& lp) {
  const RowIndex num_rows = lp.num_constraints();
  const ColIndex num_cols = lp.num_variables();
  DCHECK_EQ(num_rows, dual_values_.size());
  reduced_costs_.resize(num_cols, 0.0);
  for (ColIndex col(0); col < num_cols; ++col) {
    reduced_costs_[col] = lp.objective_coefficients()[col] -
                          ScalarProduct(dual_values_, lp.GetSparseColumn(col));
  }
}

double LPSolver::ComputeObjective(const LinearProgram& lp) {
  const ColIndex num_cols = lp.num_variables();
  DCHECK_EQ(num_cols, primal_values_.size());
  KahanSum sum;
  for (ColIndex col(0); col < num_cols; ++col) {
    sum.Add(lp.objective_coefficients()[col] * primal_values_[col]);
  }
  return sum.Value();
}

// By the duality theorem, the dual "objective" is a bound on the primal
// objective obtained by taking the linear combination of the constraints
// given by dual_values_.
//
// As it is written now, this has no real precise meaning since we ignore
// infeasible reduced costs. This is almost the same as computing the objective
// to the perturbed problem, but then we don't use the perturbed rhs. It is just
// here as an extra "consistency" check.
//
// Note(user): We could actually compute an EXACT lower bound for the cost of
// the non-cost perturbed problem. The idea comes from "Safe bounds in linear
// and mixed-integer linear programming", Arnold Neumaier , Oleg Shcherbina,
// Math Prog, 2003. Note that this requires having some variable bounds that may
// not be in the original problem so that the current dual solution is always
// feasible. It also involves changing the rounding mode to obtain exact
// confidence intervals on the reduced costs.
double LPSolver::ComputeDualObjective(const LinearProgram& lp) {
  KahanSum dual_objective;

  // Compute the part coming from the row constraints.
  const RowIndex num_rows = lp.num_constraints();
  const Fractional optimization_sign = lp.IsMaximizationProblem() ? -1.0 : 1.0;
  for (RowIndex row(0); row < num_rows; ++row) {
    const Fractional lower_bound = lp.constraint_lower_bounds()[row];
    const Fractional upper_bound = lp.constraint_upper_bounds()[row];

    // We correct the optimization_sign so we have to compute a lower bound.
    const Fractional corrected_value = optimization_sign * dual_values_[row];
    if (corrected_value > 0.0 && lower_bound != -kInfinity) {
      dual_objective.Add(dual_values_[row] * lower_bound);
    }
    if (corrected_value < 0.0 && upper_bound != kInfinity) {
      dual_objective.Add(dual_values_[row] * upper_bound);
    }
  }

  // For a given column associated to a variable x, we want to find a lower
  // bound for c.x (where c is the objective coefficient for this column). If we
  // write a.x the linear combination of the constraints at this column we have:
  //   (c + a - c) * x = a * x, and so
  //             c * x = a * x + (c - a) * x
  // Now, if we suppose for example that the reduced cost 'c - a' is positive
  // and that x is lower-bounded by 'lb' then the best bound we can get is
  //   c * x >= a * x + (c - a) * lb.
  //
  // Note: when summing over all variables, the left side is the primal
  // objective and the right side is a lower bound to the objective. In
  // particular, a necessary and sufficient condition for both objectives to be
  // the same is that all the single variable inequalities above be equalities.
  // This is possible only if c == a or if x is at its bound (modulo the
  // optimization_sign of the reduced cost), or both (this is one side of the
  // complementary slackness conditions, see Chvatal p. 62).
  const ColIndex num_cols = lp.num_variables();
  for (ColIndex col(0); col < num_cols; ++col) {
    const Fractional lower_bound = lp.variable_lower_bounds()[col];
    const Fractional upper_bound = lp.variable_upper_bounds()[col];

    // Correct the reduced cost, so as to have a minimization problem and
    // thus a dual objective that is a lower bound of the primal objective.
    const Fractional reduced_cost = optimization_sign * reduced_costs_[col];

    // We do not do any correction if the reduced cost is 'infeasible', which is
    // the same as computing the objective of the perturbed problem.
    Fractional correction = 0.0;
    if (variable_statuses_[col] == VariableStatus::AT_LOWER_BOUND &&
        reduced_cost > 0.0) {
      correction = reduced_cost * lower_bound;
    } else if (variable_statuses_[col] == VariableStatus::AT_UPPER_BOUND &&
               reduced_cost < 0.0) {
      correction = reduced_cost * upper_bound;
    } else if (variable_statuses_[col] == VariableStatus::FIXED_VALUE) {
      correction = reduced_cost * upper_bound;
    }
    // Now apply the correction in the right direction!
    dual_objective.Add(optimization_sign * correction);
  }
  return dual_objective.Value();
}

double LPSolver::ComputeMaxExpectedObjectiveError(const LinearProgram& lp) {
  const ColIndex num_cols = lp.num_variables();
  DCHECK_EQ(num_cols, primal_values_.size());
  const Fractional tolerance = parameters_.solution_feasibility_tolerance();
  Fractional primal_objective_error = 0.0;
  for (ColIndex col(0); col < num_cols; ++col) {
    // TODO(user): Be more precise since the non-BASIC variables are exactly at
    // their bounds, so for them the error bound is just the term magnitude
    // times std::numeric_limits<double>::epsilon() with KahanSum.
    primal_objective_error += std::abs(lp.objective_coefficients()[col]) *
                              AllowedError(tolerance, primal_values_[col]);
  }
  return primal_objective_error;
}

double LPSolver::ComputePrimalValueInfeasibility(const LinearProgram& lp,
                                                 bool* is_too_large) {
  Fractional infeasibility = 0.0;
  const Fractional tolerance = parameters_.solution_feasibility_tolerance();
  const ColIndex num_cols = lp.num_variables();
  for (ColIndex col(0); col < num_cols; ++col) {
    const Fractional lower_bound = lp.variable_lower_bounds()[col];
    const Fractional upper_bound = lp.variable_upper_bounds()[col];
    DCHECK(IsFinite(primal_values_[col]));

    if (lower_bound == upper_bound) {
      const Fractional error = std::abs(primal_values_[col] - upper_bound);
      infeasibility = std::max(infeasibility, error);
      *is_too_large |= error > AllowedError(tolerance, upper_bound);
      continue;
    }
    if (primal_values_[col] > upper_bound) {
      const Fractional error = primal_values_[col] - upper_bound;
      infeasibility = std::max(infeasibility, error);
      *is_too_large |= error > AllowedError(tolerance, upper_bound);
    }
    if (primal_values_[col] < lower_bound) {
      const Fractional error = lower_bound - primal_values_[col];
      infeasibility = std::max(infeasibility, error);
      *is_too_large |= error > AllowedError(tolerance, lower_bound);
    }
  }
  return infeasibility;
}

double LPSolver::ComputeActivityInfeasibility(const LinearProgram& lp,
                                              bool* is_too_large) {
  Fractional infeasibility = 0.0;
  int num_problematic_rows(0);
  const RowIndex num_rows = lp.num_constraints();
  const Fractional tolerance = parameters_.solution_feasibility_tolerance();
  for (RowIndex row(0); row < num_rows; ++row) {
    const Fractional activity = constraint_activities_[row];
    const Fractional lower_bound = lp.constraint_lower_bounds()[row];
    const Fractional upper_bound = lp.constraint_upper_bounds()[row];
    DCHECK(IsFinite(activity));

    if (lower_bound == upper_bound) {
      if (std::abs(activity - upper_bound) >
          AllowedError(tolerance, upper_bound)) {
        VLOG(2) << "Row " << row.value() << " has activity " << activity
                << " which is different from " << upper_bound << " by "
                << activity - upper_bound;
        ++num_problematic_rows;
      }
      infeasibility = std::max(infeasibility, std::abs(activity - upper_bound));
      continue;
    }
    if (activity > upper_bound) {
      const Fractional row_excess = activity - upper_bound;
      if (row_excess > AllowedError(tolerance, upper_bound)) {
        VLOG(2) << "Row " << row.value() << " has activity " << activity
                << ", exceeding its upper bound " << upper_bound << " by "
                << row_excess;
        ++num_problematic_rows;
      }
      infeasibility = std::max(infeasibility, row_excess);
    }
    if (activity < lower_bound) {
      const Fractional row_deficit = lower_bound - activity;
      if (row_deficit > AllowedError(tolerance, lower_bound)) {
        VLOG(2) << "Row " << row.value() << " has activity " << activity
                << ", below its lower bound " << lower_bound << " by "
                << row_deficit;
        ++num_problematic_rows;
      }
      infeasibility = std::max(infeasibility, row_deficit);
    }
  }
  if (num_problematic_rows > 0) {
    *is_too_large = true;
    VLOG(1) << "Number of infeasible rows = " << num_problematic_rows;
  }
  return infeasibility;
}

double LPSolver::ComputeDualValueInfeasibility(const LinearProgram& lp,
                                               bool* is_too_large) {
  const Fractional allowed_error = parameters_.solution_feasibility_tolerance();
  const Fractional optimization_sign = lp.IsMaximizationProblem() ? -1.0 : 1.0;
  Fractional infeasibility = 0.0;
  const RowIndex num_rows = lp.num_constraints();
  for (RowIndex row(0); row < num_rows; ++row) {
    const Fractional dual_value = dual_values_[row];
    const Fractional lower_bound = lp.constraint_lower_bounds()[row];
    const Fractional upper_bound = lp.constraint_upper_bounds()[row];
    DCHECK(IsFinite(dual_value));
    const Fractional minimization_dual_value = optimization_sign * dual_value;
    if (lower_bound == -kInfinity) {
      *is_too_large |= minimization_dual_value > allowed_error;
      infeasibility = std::max(infeasibility, minimization_dual_value);
    }
    if (upper_bound == kInfinity) {
      *is_too_large |= -minimization_dual_value > allowed_error;
      infeasibility = std::max(infeasibility, -minimization_dual_value);
    }
  }
  return infeasibility;
}

double LPSolver::ComputeReducedCostInfeasibility(const LinearProgram& lp,
                                                 bool* is_too_large) {
  const Fractional optimization_sign = lp.IsMaximizationProblem() ? -1.0 : 1.0;
  Fractional infeasibility = 0.0;
  const ColIndex num_cols = lp.num_variables();
  const Fractional tolerance = parameters_.solution_feasibility_tolerance();
  for (ColIndex col(0); col < num_cols; ++col) {
    const Fractional reduced_cost = reduced_costs_[col];
    const Fractional lower_bound = lp.variable_lower_bounds()[col];
    const Fractional upper_bound = lp.variable_upper_bounds()[col];
    DCHECK(IsFinite(reduced_cost));
    const Fractional minimization_reduced_cost =
        optimization_sign * reduced_cost;
    const Fractional allowed_error =
        AllowedError(tolerance, lp.objective_coefficients()[col]);
    if (lower_bound == -kInfinity) {
      *is_too_large |= minimization_reduced_cost > allowed_error;
      infeasibility = std::max(infeasibility, minimization_reduced_cost);
    }
    if (upper_bound == kInfinity) {
      *is_too_large |= -minimization_reduced_cost > allowed_error;
      infeasibility = std::max(infeasibility, -minimization_reduced_cost);
    }
  }
  return infeasibility;
}

}  // namespace glop
}  // namespace operations_research
