// Copyright 2010-2013 Google
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

#include "glop/lp_solver.h"

#include <stack>
#include <vector>

#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/timer.h"

#include "base/join.h"
#include "base/strutil.h"
#include "glop/lp_types.h"
#include "glop/lp_utils.h"
#include "glop/preprocessor.h"
#include "glop/proto_utils.h"
#include "glop/status.h"
#include "linear_solver/proto_tools.h"
#include "util/fp_utils.h"

DEFINE_bool(lp_solver_enable_fp_exceptions, true,
            "NaNs and division / zero produce errors. "
            "It is recommended to turn this off for production usage.");
DEFINE_bool(lp_dump_to_proto_file, false,
            "Tells whether do dump the problem to a protobuf file.");
DEFINE_bool(lp_dump_compressed_file, true,
            "Whether the proto dump file is compressed.");
DEFINE_bool(lp_dump_binary_file, false,
            "Whether the proto dump file is binary.");
DEFINE_int32(lp_dump_file_number, -1,
             "Number for the dump file, in the form name-000048.pb. "
             "If < 0, the file is automatically numbered from the number of "
             "calls to LPSolver::Solve().");
DEFINE_string(lp_dump_dir, "/tmp", "Directory where dump files are written.");
DEFINE_string(lp_dump_file_basename, "",
              "Base name for dump files. LinearProgram::name_ is used if "
              "lp_dump_file_basename is empty. If LinearProgram::name_ is "
              "empty, \"linear_program_dump_file\" is used.");

namespace operations_research {
namespace glop {
namespace {

// Writes a LinearProgram to a file if FLAGS_lp_dump_to_proto_file is true.
// The integer num is appended to the base name of the file.
// When this function is called from LPSolver::Solve(), num is usually the
// number of times Solve() was called.
// For a LinearProgram whose name is "LinPro", and num = 48, the default output
// file will be /tmp/LinPro-000048.pb.gz.
void DumpLinearProgramIfRequiredByFlags(const LinearProgram& linear_program,
                                        int num) {
  if (!FLAGS_lp_dump_to_proto_file) return;
  std::string filename = FLAGS_lp_dump_file_basename;
  if (filename.empty()) {
    if (linear_program.name().empty()) {
      filename = "linear_program_dump";
    } else {
      filename = linear_program.name();
    }
  }
  const int file_num =
      FLAGS_lp_dump_file_number >= 0 ? FLAGS_lp_dump_file_number : num;
  StringAppendF(&filename, "-%06d.pb", file_num);
  const std::string filespec = StrCat(FLAGS_lp_dump_dir, "/", filename);
  new_proto::MPModelProto proto;
  LinearProgramToMPModelProto(linear_program, &proto);
  if (!WriteProtoToFile(filespec, proto, FLAGS_lp_dump_binary_file,
                        FLAGS_lp_dump_compressed_file)) {
    LOG(DFATAL) << "Could not write " << filespec;
  }
}

}  // anonymous namespace

// --------------------------------------------------------
// LPSolver
// --------------------------------------------------------

LPSolver::LPSolver() : num_solves_(0) {}

void LPSolver::SetParameters(const GlopParameters& parameters) {
  parameters_ = parameters;
}

const GlopParameters& LPSolver::GetParameters() const { return parameters_; }

ProblemStatus LPSolver::Solve(const LinearProgram& lp) {
  TimeLimit time_limit(parameters_.max_time_in_seconds());

  ++num_solves_;
  num_revised_simplex_iterations_ = 0;
  DumpLinearProgramIfRequiredByFlags(lp, num_solves_);

  // Check some preconditions.
  if (!lp.IsCleanedUp()) {
    LOG(DFATAL) << "The columns of the given linear program should be ordered "
                << "by row and contain no zero coefficients. Call CleanUp() "
                << "on it before calling Solve().";
    ResizeSolution(lp.num_constraints(), lp.num_variables());
    return ProblemStatus::INVALID_PROBLEM;
  }
  if (!lp.IsValid()) {
    LOG(DFATAL) << "The given linear program is invalid. It contains NaNs, "
                << "infinite coefficients or invalid bounds specification. "
                << "You can construct it in debug mode to get the exact cause.";
    ResizeSolution(lp.num_constraints(), lp.num_variables());
    return ProblemStatus::INVALID_PROBLEM;
  }

  // Note that we only activate the floating-point exceptions after we are sure
  // that the program is valid. This way, if we have input NaNs, we will not
  // crash.
  ScopedFloatingPointEnv scoped_fenv;
  if (FLAGS_lp_solver_enable_fp_exceptions) {
#if !defined(_MSC_VER)
    scoped_fenv.EnableExceptions(FE_DIVBYZERO | FE_INVALID);
#endif
  }

  // Make an internal copy of the problem for the preprocessing.
  VLOG(1) << "Initial problem: " << lp.GetDimensionString();
  initial_num_entries_ = lp.num_entries();
  initial_num_rows_ = lp.num_constraints();
  initial_num_cols_ = lp.num_variables();
  current_linear_program_.PopulateFromLinearProgram(lp, /*keep_names=*/false);

  // Preprocess.
  status_ = ProblemStatus::INIT;
  RunPreprocessors(time_limit);

  // At this point, we need to initialize a ProblemSolution with the correct
  // size and status.
  ProblemSolution solution(current_linear_program_.num_constraints(),
                           current_linear_program_.num_variables());
  solution.status = status_;
  RunRevisedSimplexIfNeeded(&solution);
  PostprocessSolution(&solution);
  return LoadAndVerifySolution(lp, solution);
}

void LPSolver::Clear() {
  ResizeSolution(RowIndex(0), ColIndex(0));
  preprocessors_.clear();
  revised_simplex_.reset(nullptr);
}

ProblemStatus LPSolver::LoadAndVerifySolution(const LinearProgram& lp,
                                              const ProblemSolution& solution) {
  if (!IsProblemSolutionConsistent(lp, solution)) {
    VLOG(1) << "Inconsistency detected in the solution.";
    ResizeSolution(lp.num_constraints(), lp.num_variables());
    return ProblemStatus::ABNORMAL;
  }

  // Load the solution.
  primal_values_ = solution.primal_values;
  dual_values_ = solution.dual_values;
  variable_statuses_ = solution.variable_statuses;
  constraint_statuses_ = solution.constraint_statuses;
  status_ = solution.status;

  // Verify solution.
  if (status_ == ProblemStatus::OPTIMAL &&
      parameters_.provide_strong_optimal_guarantee()) {
    MovePrimalValuesWithinBounds(lp);
    MoveDualValuesWithinBounds(lp);
  }

  ComputeReducedCosts(lp);
  ComputeConstraintActivities(lp);

  max_absolute_primal_infeasibility_ = 0.0;
  max_absolute_dual_infeasibility_ = 0.0;
  ComputeColumnInfeasibilities(lp);
  ComputeRowInfeasibilities(lp);
  ComputeObjective(lp);
  ComputeDualObjective(lp);

  if (status_ == ProblemStatus::OPTIMAL &&
      parameters_.provide_strong_optimal_guarantee()) {
    // Note that this call may modify the solution values.
    Fractional cost_perturbation =
        GetMaxCostPerturbationToEnforceOptimality(lp);
    VLOG(1) << "Cost perturbation = " << cost_perturbation
            << " (Difference with dual infeasibility = "
            << cost_perturbation - max_absolute_dual_infeasibility_ << ")";
    if (cost_perturbation > parameters_.solution_feasibility_tolerance()) {
      VLOG(1) << "The needed cost perturbation is too high !!";
      status_ = ProblemStatus::IMPRECISE;
    }
    VLOG(1) << "Rhs perturbation = " << max_absolute_primal_infeasibility_
            << " (Same as primal infeasibility)";
  }

  // Check precision and optimality of the result. See Chvatal pp. 61-62.
  //
  // For that we check that:
  // - The primal solution is primal-feasible (within a small tolerance).
  // - The dual solution is dual-feasible (within a small tolerance).
  // - The difference between the primal and dual objectives is small.
  //
  // Note that measuring infeasibilities using an absolute error makes sense
  // because what is important is by how much a given bound was crossed.
  // However, for the objective gap, it is trickier because if the objective
  // value is for example 1E10, then we cannot hope for an absolute precision
  // better than 1E-6. Note that this relative precision is computed without
  // taking into account a possibly non-zero objective offset.
  VLOG(1) << "Max. primal infeasibility = "
          << max_absolute_primal_infeasibility_;
  VLOG(1) << "Max. dual infeasibility = " << max_absolute_dual_infeasibility_;
  VLOG(1) << "Gap = " << objective_value_ - dual_objective_value_;

  if ((status_ == ProblemStatus::OPTIMAL ||
       status_ == ProblemStatus::PRIMAL_FEASIBLE) &&
      (max_absolute_primal_infeasibility_ >
       parameters_.solution_feasibility_tolerance())) {
    VLOG(1) << "The final solution primal infeasibility is too high !!";
    status_ = ProblemStatus::IMPRECISE;
  }
  if ((status_ == ProblemStatus::OPTIMAL ||
       status_ == ProblemStatus::DUAL_FEASIBLE) &&
      (max_absolute_dual_infeasibility_ >
       parameters_.solution_feasibility_tolerance())) {
    VLOG(1) << "The final solution dual infeasibility is too high !!";
    status_ = ProblemStatus::IMPRECISE;
  }
  if (status_ == ProblemStatus::OPTIMAL) {
    if (!AreWithinAbsoluteOrRelativeTolerances(
            objective_value_, dual_objective_value_,
            parameters_.solution_objective_gap_tolerance(),
            parameters_.solution_objective_gap_tolerance())) {
      VLOG(1) << "The final solution objective gap is too high !!";
      status_ = ProblemStatus::IMPRECISE;
    }
  }

  may_have_multiple_solutions_ =
      (status_ == ProblemStatus::OPTIMAL) ?
          IsOptimalSolutionOnFacet(lp) : false;
  return status_;
}

bool LPSolver::IsOptimalSolutionOnFacet(const LinearProgram& lp) {
  // Note(user): We use the following same two tolerances for the dual and
  // primal values.
  // TODO(user): investigate whether to use the tolerances defined in
  // parameters.proto.
  const double kReducedCostTolerance = 1e-9;
  const double kBoundTolerance = 1e-7;
  const ColIndex num_cols = lp.num_variables();
  for (ColIndex col(0); col < num_cols; ++col) {
    if (variable_statuses_[col] == VariableStatus::FIXED_VALUE) continue;
    const Fractional lower_bound = lp.variable_lower_bounds()[col];
    const Fractional upper_bound = lp.variable_upper_bounds()[col];
    const Fractional value = primal_values_[col];
    if (AreWithinAbsoluteTolerance(reduced_costs_[col], 0.0,
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
    if (AreWithinAbsoluteTolerance(dual_values_[row], 0.0,
                                   kReducedCostTolerance) &&
        (AreWithinAbsoluteTolerance(activity, lower_bound, kBoundTolerance) ||
         AreWithinAbsoluteTolerance(activity, upper_bound, kBoundTolerance))) {
      return true;
    }
  }
  return false;
}

Fractional LPSolver::GetObjectiveValue() const {
  return objective_value_with_offset_;
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

void LPSolver::MovePrimalValuesWithinBounds(const LinearProgram& lp) {
  const ColIndex num_cols = lp.num_variables();
  DCHECK_EQ(num_cols, primal_values_.size());
  for (ColIndex col(0); col < num_cols; ++col) {
    const Fractional lower_bound = lp.variable_lower_bounds()[col];
    const Fractional upper_bound = lp.variable_upper_bounds()[col];
    DCHECK_LE(lower_bound, upper_bound);
    primal_values_[col] = std::min(primal_values_[col], upper_bound);
    primal_values_[col] = std::max(primal_values_[col], lower_bound);
  }
}

void LPSolver::MoveDualValuesWithinBounds(const LinearProgram& lp) {
  const RowIndex num_rows = lp.num_constraints();
  DCHECK_EQ(num_rows, dual_values_.size());
  const Fractional optimization_sign = lp.IsMaximizationProblem() ? -1.0 : 1.0;
  for (RowIndex row(0); row < num_rows; ++row) {
    const Fractional lower_bound = lp.constraint_lower_bounds()[row];
    const Fractional upper_bound = lp.constraint_upper_bounds()[row];

    // For a minimization problem, we want a lower bound.
    Fractional minimization_dual_value = optimization_sign * dual_values_[row];
    if (lower_bound == -kInfinity) {
      if (minimization_dual_value > 0.0) minimization_dual_value = 0.0;
    }
    if (upper_bound == kInfinity) {
      if (minimization_dual_value < 0.0) minimization_dual_value = 0.0;
    }
    dual_values_[row] = optimization_sign * minimization_dual_value;
  }
}

void LPSolver::ResizeSolution(RowIndex num_rows, ColIndex num_cols) {
  primal_values_.resize(num_cols, 0.0);
  reduced_costs_.resize(num_cols, 0.0);
  variable_statuses_.resize(num_cols, VariableStatus::FREE);

  dual_values_.resize(num_rows, 0.0);
  constraint_activities_.resize(num_rows, 0.0);
  constraint_statuses_.resize(num_rows, ConstraintStatus::FREE);
}

#define RUN_PREPROCESSOR(name)                                           \
  RunAndPushIfRelevant(std::unique_ptr<Preprocessor>(new name()), #name, \
                       time_limit)

void LPSolver::RunPreprocessors(const TimeLimit& time_limit) {
  if (parameters_.use_preprocessing()) {
    RUN_PREPROCESSOR(ShiftVariableBoundsPreprocessor);
    RUN_PREPROCESSOR(RemoveNearZeroEntriesPreprocessor);

    // We run it a few times because running one preprocessor may allow another
    // one to remove more stuff.
    const int kMaxNumPasses = 20;
    for (int i = 0; i < kMaxNumPasses; ++i) {
      const int old_stack_size = preprocessors_.size();
      RUN_PREPROCESSOR(FixedVariablePreprocessor);
      RUN_PREPROCESSOR(SingletonPreprocessor);
      RUN_PREPROCESSOR(ForcingAndImpliedFreeConstraintPreprocessor);
      RUN_PREPROCESSOR(FreeConstraintPreprocessor);
      RUN_PREPROCESSOR(UnconstrainedVariablePreprocessor);
      RUN_PREPROCESSOR(DoubletonEqualityRowPreprocessor);
      RUN_PREPROCESSOR(ImpliedFreePreprocessor);
      RUN_PREPROCESSOR(DoubletonFreeColumnPreprocessor);

      // Abort early if none of the preprocessors did something. Technically
      // this is true if none of the preprocessors above needs postsolving,
      // which has exactly the same meaning for these particular preprocessors.
      if (preprocessors_.size() == old_stack_size) {
        // We use i here because the last pass did nothing.
        VLOG(1) << "Reached fixed point after presolve pass #" << i;
        break;
      }
    }
    RUN_PREPROCESSOR(EmptyColumnPreprocessor);
    RUN_PREPROCESSOR(EmptyConstraintPreprocessor);

    // TODO(user): Run them in the loop above if the effect on the running time
    // is good. This needs more investigation.
    RUN_PREPROCESSOR(ProportionalColumnPreprocessor);
    RUN_PREPROCESSOR(ProportionalRowPreprocessor);

    // If DualizerPreprocessor was run, we need to do some extra preprocessing.
    // This is because it currently adds a lot of zero-cost singleton columns.
    const int old_stack_size = preprocessors_.size();
    RUN_PREPROCESSOR(DualizerPreprocessor);
    if (old_stack_size != preprocessors_.size()) {
      RUN_PREPROCESSOR(SingletonPreprocessor);
      RUN_PREPROCESSOR(FreeConstraintPreprocessor);
      RUN_PREPROCESSOR(UnconstrainedVariablePreprocessor);
      RUN_PREPROCESSOR(EmptyColumnPreprocessor);
      RUN_PREPROCESSOR(EmptyConstraintPreprocessor);
    }
  }

  // These are implemented as preprocessors, but are not controlled by the
  // use_preprocessing() parameter.
  RUN_PREPROCESSOR(SingletonColumnSignPreprocessor);
  RUN_PREPROCESSOR(ScalingPreprocessor);
}

#undef RUN_PREPROCESSOR

void LPSolver::RunAndPushIfRelevant(std::unique_ptr<Preprocessor> preprocessor,
                                    const std::string& name,
                                    const TimeLimit& time_limit) {
  RETURN_IF_NULL(preprocessor);
  WallTimer timer;
  timer.Start();
  parameters_.set_max_time_in_seconds(time_limit.GetTimeLeft());
  preprocessor->SetParameters(parameters_);
  if (status_ == ProblemStatus::INIT) {
    // No need to run the preprocessor if current_linear_program_ is empty.
    // TODO(user): without this test, the code is failing as of 2013-03-18.
    if (current_linear_program_.num_variables() == 0 &&
        current_linear_program_.num_constraints() == 0) {
      status_ = ProblemStatus::OPTIMAL;
    } else if (preprocessor->Run(&current_linear_program_)) {
      const EntryIndex new_num_entries = current_linear_program_.num_entries();
      VLOG(1) << StringPrintf(
          "%s(%fs): %d(%d) rows, %d(%d) columns, %lld(%lld) entries.",
          name.c_str(), timer.Get(),
          current_linear_program_.num_constraints().value(),
          (current_linear_program_.num_constraints() - initial_num_rows_)
              .value(),
          current_linear_program_.num_variables().value(),
          (current_linear_program_.num_variables() - initial_num_cols_).value(),
          new_num_entries.value(),
          new_num_entries.value() - initial_num_entries_.value());
      status_ = preprocessor->status();
      preprocessors_.push_back(std::move(preprocessor));
      return;
    } else {
      // Even if a preprocessor returns false (i.e. no need for postsolve), it
      // can detect an issue with the problem.
      status_ = preprocessor->status();
      if (status_ != ProblemStatus::INIT) {
        VLOG(1) << name << " detected that the problem is "
                << GetProblemStatusString(status_);
      }
    }
  }
}

void LPSolver::RunRevisedSimplexIfNeeded(ProblemSolution* solution) {
  // Note that the transpose matrix is no longer needed at this point.
  // This helps reduce the peak memory usage of the solver.
  current_linear_program_.ClearTransposeMatrix();
  if (solution->status != ProblemStatus::INIT) return;
  if (revised_simplex_ == nullptr) {
    revised_simplex_.reset(new RevisedSimplex());
  }
  revised_simplex_->SetParameters(parameters_);
  if (revised_simplex_->Solve(current_linear_program_).ok()) {
    num_revised_simplex_iterations_ = revised_simplex_->GetNumberOfIterations();
    solution->status = revised_simplex_->GetProblemStatus();

    const ColIndex num_cols = revised_simplex_->GetProblemNumCols();
    DCHECK_EQ(solution->primal_values.size(), num_cols);
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
  } else {
    VLOG(1) << "Error during the revised simplex algorithm.";
    solution->status = ProblemStatus::ABNORMAL;
  }
}

void LPSolver::PostprocessSolution(ProblemSolution* solution) {
  while (!preprocessors_.empty()) {
    preprocessors_.back()->StoreSolution(solution);
    preprocessors_.pop_back();
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
      solution.status != ProblemStatus::DUAL_FEASIBLE)
    return true;

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
        if (lb != ub || value != lb) {
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
        if (value != ub || lb == ub) {
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
        if (lb != ub) {
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

Fractional LPSolver::GetMaxCostPerturbationToEnforceOptimality(
    const LinearProgram& lp) {
  Fractional max_cost_correction = 0.0;
  const ColIndex num_cols = lp.num_variables();
  const Fractional optimization_sign = lp.IsMaximizationProblem() ? -1.0 : 1.0;
  for (ColIndex col(0); col < num_cols; ++col) {
    const Fractional variable_value = primal_values_[col];
    const Fractional lower_bound = lp.variable_lower_bounds()[col];
    const Fractional upper_bound = lp.variable_upper_bounds()[col];

    DCHECK_GE(variable_value, lower_bound);
    DCHECK_LE(variable_value, upper_bound);

    // We correct the reduced cost, so we have a minimization problem and
    // thus the dual_objective_value_ will be a lower bound of the primal
    // objective.
    const Fractional reduced_cost = optimization_sign * reduced_costs_[col];

    // Perturbation needed to move the cost to 0.0
    const Fractional cost_delta = fabs(reduced_cost);

    // We are enforcing the complementary slackness conditions, see the comment
    // in ComputeDualObjective() for more explanations. By default, we move the
    // cost to zero, and if we decide to move the variable to the bound, then we
    // continue the loop.
    if (upper_bound == kInfinity) {
      if (lower_bound != -kInfinity) {
        if (reduced_cost > 0) {
          // We move the variable to the bound OR we move the cost to zero.
          const Fractional variable_delta = primal_values_[col] - lower_bound;
          if (variable_delta < cost_delta) {
            primal_values_[col] = lower_bound;
            continue;
          }
        }
      }
    } else {
      if (lower_bound == -kInfinity) {
        if (reduced_cost < 0) {
          // We move the variable to the bound OR we move the cost to zero.
          const Fractional variable_delta = upper_bound - primal_values_[col];
          if (variable_delta < cost_delta) {
            primal_values_[col] = upper_bound;
            continue;
          }
        }
      } else {
        // Boxed variable, 3 options.
        const Fractional variable_delta_up = upper_bound - variable_value;
        const Fractional variable_delta_down = variable_value - lower_bound;
        if (variable_delta_down < cost_delta ||
            variable_delta_up < cost_delta) {
          if (variable_delta_up < variable_delta_down) {
            primal_values_[col] = upper_bound;
          } else {
            primal_values_[col] = lower_bound;
          }
          continue;
        }
      }
    }
    max_cost_correction = std::max(max_cost_correction, cost_delta);
  }
  return max_cost_correction;
}

void LPSolver::ComputeConstraintActivities(const LinearProgram& lp) {
  const RowIndex num_rows = lp.num_constraints();
  const ColIndex num_cols = lp.num_variables();
  constraint_activities_.assign(num_rows, 0.0);
  DCHECK_EQ(num_cols, primal_values_.size());
  for (ColIndex col(0); col < num_cols; ++col) {
    lp.GetSparseColumn(col)
        .AddMultipleToDenseVector(primal_values_[col], &constraint_activities_);
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

void LPSolver::ComputeObjective(const LinearProgram& lp) {
  const ColIndex num_cols = lp.num_variables();
  DCHECK_EQ(num_cols, primal_values_.size());
  KahanSum sum;
  for (ColIndex col(0); col < num_cols; ++col) {
    sum.Add(lp.objective_coefficients()[col] * primal_values_[col]);
  }
  objective_value_ = sum.Value();
  objective_value_with_offset_ = objective_value_ + lp.objective_offset();
}

void LPSolver::ComputeDualObjective(const LinearProgram& lp) {
  // By the duality theorem, the dual objective is a bound on the primal
  // objective obtained by taking the linear combinaison of the constraints
  // given by dual_values_.
  KahanSum dual_objective;

  // Compute the part coming from the row constraints.
  const RowIndex num_rows = lp.num_constraints();
  const Fractional optimization_sign = lp.IsMaximizationProblem() ? -1.0 : 1.0;
  for (RowIndex row(0); row < num_rows; ++row) {
    const Fractional lower_bound = lp.constraint_lower_bounds()[row];
    const Fractional upper_bound = lp.constraint_upper_bounds()[row];

    // We correct the optimization_sign so we have to compute a lower bound.
    const Fractional corrected_value = optimization_sign * dual_values_[row];
    if (corrected_value > 0 && lower_bound != -kInfinity) {
      dual_objective.Add(dual_values_[row] * lower_bound);
    }
    if (corrected_value < 0 && upper_bound != kInfinity) {
      dual_objective.Add(dual_values_[row] * upper_bound);
    }
  }

  // We also need to take into account the reduced costs: for a given column
  // associated to a variable x, we want to find a lower bound for c.x (where c
  // is the objective coefficient for this column). If we write a.x the linear
  // combination of the constraints at this column we have:
  //   (c + a - c) * x = a * x, and so
  //             c * x = a * x + (c - a) * x
  // Now, if we suppose for example that the reduced cost 'c - a' is positive
  // and that x is lower-bounded by 'l' then the best bound we can get is
  //   c * x >= a * x + (c - a) * l.
  //
  // Note: when summing over all variables, the left side is the primal
  // objective and the right side is the dual objective. In particular, a
  // necessary and sufficient condition for both objectives to be the same is
  // that all the single variable inequalities above be equalities.
  // This is possible only if c == a or if x is at its bound (modulo the
  // optimization_sign of the reduced cost), or both (this is one side of the
  // complementary slackness conditions, see Chvatal p. 62).
  const ColIndex num_cols = lp.num_variables();
  for (ColIndex col(0); col < num_cols; ++col) {
    const Fractional lower_bound = lp.variable_lower_bounds()[col];
    const Fractional upper_bound = lp.variable_upper_bounds()[col];

    // We correct the reduced cost, so we have a minimization problem and
    // thus the dual objective will be a lower bound of the primal objective.
    const Fractional reduced_cost = optimization_sign * reduced_costs_[col];
    Fractional correction = 0.0;
    if (upper_bound == kInfinity) {
      if (lower_bound != -kInfinity) {
        // Note that we do not do any correction if the reduced cost is
        // 'infeasible', which is the same as computing the objective of the
        // perturbed problem.
        if (reduced_cost > 0.0) {
          correction = reduced_cost * lower_bound;
        }
      }
    } else {
      if (lower_bound == -kInfinity) {
        // Same note as above.
        if (reduced_cost < 0.0) {
          correction = reduced_cost * upper_bound;
        }
      } else {
        correction =
            reduced_cost * (reduced_cost > 0 ? lower_bound : upper_bound);
      }
    }

    // We now need to apply the correction in the good direction!
    dual_objective.Add(optimization_sign * correction);
  }
  dual_objective_value_ = dual_objective.Value();
}

void LPSolver::UpdateMaxPrimalInfeasibility(Fractional update) {
  max_absolute_primal_infeasibility_ =
      std::max(max_absolute_primal_infeasibility_, update);
}

void LPSolver::UpdateMaxDualInfeasibility(Fractional update) {
  max_absolute_dual_infeasibility_ =
      std::max(max_absolute_dual_infeasibility_, update);
}

void LPSolver::ComputeRowInfeasibilities(const LinearProgram& lp) {
  RowIndex num_problematic_rows(0);
  const RowIndex num_rows = lp.num_constraints();
  const Fractional optimization_sign = lp.IsMaximizationProblem() ? -1.0 : 1.0;
  for (RowIndex row(0); row < num_rows; ++row) {
    const Fractional activity = constraint_activities_[row];
    const Fractional lower_bound = lp.constraint_lower_bounds()[row];
    const Fractional upper_bound = lp.constraint_upper_bounds()[row];
    const Fractional tolerance = parameters_.solution_feasibility_tolerance();
    if (lower_bound == upper_bound) {
      if (fabs(activity - upper_bound) > tolerance) {
        VLOG(2) << "Row " << row.value() << " has activity " << activity
                << " which is different from " << upper_bound << " by "
                << activity - upper_bound;
        ++num_problematic_rows;
      }
      UpdateMaxPrimalInfeasibility(fabs(activity - upper_bound));
      continue;
    }
    if (activity > upper_bound) {
      const Fractional row_excess = activity - upper_bound;
      if (row_excess > tolerance) {
        VLOG(2) << "Row " << row.value() << " has activity " << activity
                << ", exceeding its upper bound " << upper_bound << " by "
                << row_excess;
        ++num_problematic_rows;
      }
      UpdateMaxPrimalInfeasibility(row_excess);
    }
    if (activity < lower_bound) {
      const Fractional row_deficit = lower_bound - activity;
      if (row_deficit > tolerance) {
        VLOG(2) << "Row " << row.value() << " has activity " << activity
                << ", below its lower bound " << lower_bound << " by "
                << row_deficit;
        ++num_problematic_rows;
      }
      UpdateMaxPrimalInfeasibility(row_deficit);
    }

    // For a minimization problem, we want a lower bound.
    const Fractional minimization_dual_value =
        optimization_sign * dual_values_[row];
    if (lower_bound == -kInfinity) {
      UpdateMaxDualInfeasibility(minimization_dual_value);
    }
    if (upper_bound == kInfinity) {
      UpdateMaxDualInfeasibility(-minimization_dual_value);
    }
  }
  VLOG(1) << "Number of problematic rows " << num_problematic_rows.value();
}

void LPSolver::ComputeColumnInfeasibilities(const LinearProgram& lp) {
  const ColIndex num_cols = lp.num_variables();
  const Fractional optimization_sign = lp.IsMaximizationProblem() ? -1.0 : 1.0;
  for (ColIndex col(0); col < num_cols; ++col) {
    const Fractional lower_bound = lp.variable_lower_bounds()[col];
    const Fractional upper_bound = lp.variable_upper_bounds()[col];
    DCHECK(IsFinite(primal_values_[col]));
    DCHECK(IsFinite(reduced_costs_[col]));

    // Compute the infeasibility on the bound.
    if (lower_bound == upper_bound) {
      UpdateMaxPrimalInfeasibility(fabs(primal_values_[col] - upper_bound));
      continue;
    }
    if (primal_values_[col] > upper_bound) {
      UpdateMaxPrimalInfeasibility(primal_values_[col] - upper_bound);
    }
    if (primal_values_[col] < lower_bound) {
      UpdateMaxPrimalInfeasibility(lower_bound - primal_values_[col]);
    }

    // For a minimization problem, we want a lower bound.
    const Fractional minimization_reduced_cost =
        optimization_sign * reduced_costs_[col];
    if (lower_bound == -kInfinity) {
      UpdateMaxDualInfeasibility(minimization_reduced_cost);
    }
    if (upper_bound == kInfinity) {
      UpdateMaxDualInfeasibility(-minimization_reduced_cost);
    }
  }
}

}  // namespace glop
}  // namespace operations_research
