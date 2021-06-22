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

#if defined(USE_SCIP)

#include <stddef.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "absl/types/optional.h"
#include "ortools/base/cleanup.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/hash.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/status_macros.h"
#include "ortools/base/timer.h"
#include "ortools/gscip/legacy_scip_params.h"
#include "ortools/linear_solver/linear_solver.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/linear_solver/linear_solver_callback.h"
#include "ortools/linear_solver/scip_callback.h"
#include "ortools/linear_solver/scip_helper_macros.h"
#include "ortools/linear_solver/scip_proto_solver.h"
#include "scip/cons_indicator.h"
#include "scip/scip.h"
#include "scip/scip_copy.h"
#include "scip/scip_param.h"
#include "scip/scip_prob.h"
#include "scip/scipdefplugins.h"

ABSL_FLAG(bool, scip_feasibility_emphasis, false,
          "When true, emphasize search towards feasibility. This may or "
          "may not result in speedups in some problems.");

namespace operations_research {
namespace {
// See the class ScipConstraintHandlerForMPCallback below.
struct EmptyStruct {};
}  // namespace

class ScipConstraintHandlerForMPCallback;

class SCIPInterface : public MPSolverInterface {
 public:
  explicit SCIPInterface(MPSolver* solver);
  ~SCIPInterface() override;

  void SetOptimizationDirection(bool maximize) override;
  MPSolver::ResultStatus Solve(const MPSolverParameters& param) override;
  absl::optional<MPSolutionResponse> DirectlySolveProto(
      const MPModelRequest& request) override;
  void Reset() override;

  void SetVariableBounds(int var_index, double lb, double ub) override;
  void SetVariableInteger(int var_index, bool integer) override;
  void SetConstraintBounds(int row_index, double lb, double ub) override;

  void AddRowConstraint(MPConstraint* ct) override;
  bool AddIndicatorConstraint(MPConstraint* ct) override;
  void AddVariable(MPVariable* var) override;
  void SetCoefficient(MPConstraint* constraint, const MPVariable* variable,
                      double new_value, double old_value) override;
  void ClearConstraint(MPConstraint* constraint) override;
  void SetObjectiveCoefficient(const MPVariable* variable,
                               double coefficient) override;
  void SetObjectiveOffset(double value) override;
  void ClearObjective() override;
  void BranchingPriorityChangedForVariable(int var_index) override;

  int64_t iterations() const override;
  int64_t nodes() const override;
  MPSolver::BasisStatus row_status(int constraint_index) const override {
    LOG(DFATAL) << "Basis status only available for continuous problems";
    return MPSolver::FREE;
  }
  MPSolver::BasisStatus column_status(int variable_index) const override {
    LOG(DFATAL) << "Basis status only available for continuous problems";
    return MPSolver::FREE;
  }

  bool IsContinuous() const override { return false; }
  bool IsLP() const override { return false; }
  bool IsMIP() const override { return true; }

  void ExtractNewVariables() override;
  void ExtractNewConstraints() override;
  void ExtractObjective() override;

  std::string SolverVersion() const override {
    return absl::StrFormat("SCIP %d.%d.%d [LP solver: %s]", SCIPmajorVersion(),
                           SCIPminorVersion(), SCIPtechVersion(),
                           SCIPlpiGetSolverName());
  }

  bool InterruptSolve() override {
    const absl::MutexLock lock(&hold_interruptions_mutex_);
    if (scip_ == nullptr) {
      LOG_IF(DFATAL, status_.ok()) << "scip_ is null is unexpected here, since "
                                      "status_ did not report any error";
      return true;
    }
    return SCIPinterruptSolve(scip_) == SCIP_OKAY;
  }

  void* underlying_solver() override { return reinterpret_cast<void*>(scip_); }

  // MULTIPLE SOLUTIONS SUPPORT
  // The default behavior of scip is to store the top incidentally generated
  // integer solutions in the solution pool.  The default maximum size is 100.
  // This can be adjusted by setting the param limits/maxsol. There is no way
  // to ensure that the pool will actually be full.
  //
  // You can also ask SCIP to enumerate all feasible solutions. Combined with
  // an equality or inequality constraint on the objective (after solving once
  // to find the optimal solution), you can use this to find all high quality
  // solutions. See https://scip.zib.de/doc/html/COUNTER.php. This behavior is
  // not supported directly through MPSolver, but in theory can be controlled
  // entirely through scip parameters.
  bool NextSolution() override;

  // CALLBACK SUPPORT:
  //  * We support MPSolver's callback API via MPCallback.
  //    See ./linear_solver_callback.h.
  //  * We also support SCIP's more general callback interface, built on
  //    'constraint handlers'. See ./scip_callback.h and test, these are added
  //    directly to the underlying SCIP object, bypassing SCIPInterface.
  // The former works by calling the latter. See go/scip-callbacks for
  // a complete documentation of this design.

  // MPCallback API
  void SetCallback(MPCallback* mp_callback) override;
  bool SupportsCallbacks() const override { return true; }

 private:
  void SetParameters(const MPSolverParameters& param) override;
  void SetRelativeMipGap(double value) override;
  void SetPrimalTolerance(double value) override;
  void SetDualTolerance(double value) override;
  void SetPresolveMode(int presolve) override;
  void SetScalingMode(int scaling) override;
  void SetLpAlgorithm(int lp_algorithm) override;

  // SCIP parameters allow to lower and upper bound the number of threads used
  // (via "parallel/minnthreads" and "parallel/maxnthread", respectively). Here,
  // we interpret "num_threads" to mean "parallel/maxnthreads", as this is what
  // most clients probably want to do. To change "parallel/minnthreads" use
  // SetSolverSpecificParametersAsString(). However, one must change
  // "parallel/maxnthread" with SetNumThreads() because only this will inform
  // the interface to run SCIPsolveConcurrent() instead of SCIPsolve() which is
  // necessery to enable multi-threading.
  absl::Status SetNumThreads(int num_threads) override;

  bool SetSolverSpecificParametersAsString(
      const std::string& parameters) override;

  void SetUnsupportedIntegerParam(
      MPSolverParameters::IntegerParam param) override;
  void SetIntegerParamToUnsupportedValue(MPSolverParameters::IntegerParam param,
                                         int value) override;
  // How many solutions SCIP found.
  int SolutionCount();
  // Copy sol from SCIP to MPSolver.
  void SetSolution(SCIP_SOL* solution);

  absl::Status CreateSCIP();
  // Deletes variables and constraints from scip_ and reset scip_ to null. If
  // return_scip is false, deletes the SCIP object; if true, returns it (but
  // scip_ is still set to null).
  SCIP* DeleteSCIP(bool return_scip = false);

  // SCIP has many internal checks (many of which are numerical) that can fail
  // during various phases: upon startup, when loading the model, when solving,
  // etc. Often, the user is meant to stop at the first error, but since most
  // of the linear solver interface API doesn't support "error reporting", we
  // store a potential error status here.
  // If this status isn't OK, then most operations will silently be cancelled.
  absl::Status status_;

  SCIP* scip_;
  std::vector<SCIP_VAR*> scip_variables_;
  std::vector<SCIP_CONS*> scip_constraints_;
  int current_solution_index_ = 0;
  MPCallback* callback_ = nullptr;
  std::unique_ptr<ScipConstraintHandlerForMPCallback> scip_constraint_handler_;
  // See ScipConstraintHandlerForMPCallback below.
  EmptyStruct constraint_data_for_handler_;
  bool branching_priority_reset_ = false;
  bool callback_reset_ = false;

  // Mutex that is held to prevent InterruptSolve() to call SCIPinterruptSolve()
  // when scip_ is being built. It also prevents rebuilding scip_ until
  // SCIPinterruptSolve() has returned.
  mutable absl::Mutex hold_interruptions_mutex_;
};

class ScipConstraintHandlerForMPCallback
    : public ScipConstraintHandler<EmptyStruct> {
 public:
  explicit ScipConstraintHandlerForMPCallback(MPCallback* mp_callback);

  std::vector<CallbackRangeConstraint> SeparateFractionalSolution(
      const ScipConstraintHandlerContext& context, const EmptyStruct&) override;

  std::vector<CallbackRangeConstraint> SeparateIntegerSolution(
      const ScipConstraintHandlerContext& context, const EmptyStruct&) override;

  MPCallback* const mp_callback() const { return mp_callback_; }

 private:
  std::vector<CallbackRangeConstraint> SeparateSolution(
      const ScipConstraintHandlerContext& context,
      const bool at_integer_solution);

  MPCallback* const mp_callback_;
};

#define RETURN_IF_ALREADY_IN_ERROR_STATE                             \
  do {                                                               \
    if (!status_.ok()) {                                             \
      VLOG_EVERY_N(1, 10) << "Early abort: SCIP is in error state."; \
      return;                                                        \
    }                                                                \
  } while (false)

#define RETURN_AND_STORE_IF_SCIP_ERROR(x) \
  do {                                    \
    status_ = SCIP_TO_STATUS(x);          \
    if (!status_.ok()) return;            \
  } while (false)

SCIPInterface::SCIPInterface(MPSolver* solver)
    : MPSolverInterface(solver), scip_(nullptr) {
  status_ = CreateSCIP();
}

SCIPInterface::~SCIPInterface() { DeleteSCIP(); }

void SCIPInterface::Reset() {
  // We hold calls to SCIPinterruptSolve() until the new scip_ is fully built.
  const absl::MutexLock lock(&hold_interruptions_mutex_);

  // Remove existing one but keep it alive to copy parameters from it.
  SCIP* old_scip = DeleteSCIP(/*return_scip=*/true);
  const auto scip_deleter = absl::MakeCleanup(
      [&old_scip]() { CHECK_EQ(SCIPfree(&old_scip), SCIP_OKAY); });

  scip_constraint_handler_.reset();
  ResetExtractionInformation();

  // Install the new one.
  status_ = CreateSCIP();
  if (!status_.ok()) {
    return;
  }

  // Copy all existing parameters from the previous SCIP to the new one. This
  // ensures that if a user calls multiple times
  // SetSolverSpecificParametersAsString() and then Reset() is called, we still
  // take into account all parameters. Note though that at the end of Solve(),
  // parameters are reset so after Solve() has been called, only the last set
  // parameters are kept.
  RETURN_AND_STORE_IF_SCIP_ERROR(SCIPcopyParamSettings(old_scip, scip_));
}

absl::Status SCIPInterface::CreateSCIP() {
  RETURN_IF_SCIP_ERROR(SCIPcreate(&scip_));
  RETURN_IF_SCIP_ERROR(SCIPincludeDefaultPlugins(scip_));
  // Set the emphasis to enum SCIP_PARAMEMPHASIS_FEASIBILITY. Do not print
  // the new parameter (quiet = true).
  if (absl::GetFlag(FLAGS_scip_feasibility_emphasis)) {
    RETURN_IF_SCIP_ERROR(SCIPsetEmphasis(scip_, SCIP_PARAMEMPHASIS_FEASIBILITY,
                                         /*quiet=*/true));
  }
  // Default clock type. We use wall clock time because getting CPU user seconds
  // involves calling times() which is very expensive.
  // NOTE(user): Also, time limit based on CPU user seconds is *NOT* thread
  // safe. We observed that different instances of SCIP running concurrently
  // in different threads consume the time limit *together*. E.g., 2 threads
  // running SCIP with time limit 10s each will both terminate after ~5s.
  RETURN_IF_SCIP_ERROR(
      SCIPsetIntParam(scip_, "timing/clocktype", SCIP_CLOCKTYPE_WALL));
  RETURN_IF_SCIP_ERROR(SCIPcreateProb(scip_, solver_->name_.c_str(), nullptr,
                                      nullptr, nullptr, nullptr, nullptr,
                                      nullptr, nullptr));
  RETURN_IF_SCIP_ERROR(SCIPsetObjsense(
      scip_, maximize_ ? SCIP_OBJSENSE_MAXIMIZE : SCIP_OBJSENSE_MINIMIZE));
  return absl::OkStatus();
}

SCIP* SCIPInterface::DeleteSCIP(bool return_scip) {
  // NOTE(user): DeleteSCIP() shouldn't "give up" mid-stage if it fails, since
  // it might be the user's chance to reset the solver to start fresh without
  // errors. The current code isn't perfect, since some CHECKs() remain, but
  // hopefully they'll never be triggered in practice.
  CHECK(scip_ != nullptr);
  for (int i = 0; i < scip_variables_.size(); ++i) {
    CHECK_EQ(SCIPreleaseVar(scip_, &scip_variables_[i]), SCIP_OKAY);
  }
  scip_variables_.clear();
  for (int j = 0; j < scip_constraints_.size(); ++j) {
    CHECK_EQ(SCIPreleaseCons(scip_, &scip_constraints_[j]), SCIP_OKAY);
  }
  scip_constraints_.clear();

  SCIP* old_scip = scip_;
  scip_ = nullptr;
  if (!return_scip) {
    CHECK_EQ(SCIPfree(&old_scip), SCIP_OKAY);
  }
  return old_scip;
}

// Not cached.
void SCIPInterface::SetOptimizationDirection(bool maximize) {
  RETURN_IF_ALREADY_IN_ERROR_STATE;
  InvalidateSolutionSynchronization();
  RETURN_AND_STORE_IF_SCIP_ERROR(SCIPfreeTransform(scip_));
  RETURN_AND_STORE_IF_SCIP_ERROR(SCIPsetObjsense(
      scip_, maximize ? SCIP_OBJSENSE_MAXIMIZE : SCIP_OBJSENSE_MINIMIZE));
}

void SCIPInterface::SetVariableBounds(int var_index, double lb, double ub) {
  RETURN_IF_ALREADY_IN_ERROR_STATE;
  InvalidateSolutionSynchronization();
  if (variable_is_extracted(var_index)) {
    // Not cached if the variable has been extracted.
    DCHECK_LT(var_index, last_variable_index_);
    RETURN_AND_STORE_IF_SCIP_ERROR(SCIPfreeTransform(scip_));
    RETURN_AND_STORE_IF_SCIP_ERROR(
        SCIPchgVarLb(scip_, scip_variables_[var_index], lb));
    RETURN_AND_STORE_IF_SCIP_ERROR(
        SCIPchgVarUb(scip_, scip_variables_[var_index], ub));
  } else {
    sync_status_ = MUST_RELOAD;
  }
}

void SCIPInterface::SetVariableInteger(int var_index, bool integer) {
  RETURN_IF_ALREADY_IN_ERROR_STATE;
  InvalidateSolutionSynchronization();
  if (variable_is_extracted(var_index)) {
    // Not cached if the variable has been extracted.
    RETURN_AND_STORE_IF_SCIP_ERROR(SCIPfreeTransform(scip_));
#if (SCIP_VERSION >= 210)
    SCIP_Bool infeasible = false;
    RETURN_AND_STORE_IF_SCIP_ERROR(SCIPchgVarType(
        scip_, scip_variables_[var_index],
        integer ? SCIP_VARTYPE_INTEGER : SCIP_VARTYPE_CONTINUOUS, &infeasible));
#else
    RETURN_AND_STORE_IF_SCIP_ERROR(SCIPchgVarType(
        scip_, scip_variables_[var_index],
        integer ? SCIP_VARTYPE_INTEGER : SCIP_VARTYPE_CONTINUOUS));
#endif  // SCIP_VERSION >= 210
  } else {
    sync_status_ = MUST_RELOAD;
  }
}

void SCIPInterface::SetConstraintBounds(int index, double lb, double ub) {
  RETURN_IF_ALREADY_IN_ERROR_STATE;
  InvalidateSolutionSynchronization();
  if (constraint_is_extracted(index)) {
    // Not cached if the row has been extracted.
    DCHECK_LT(index, last_constraint_index_);
    RETURN_AND_STORE_IF_SCIP_ERROR(SCIPfreeTransform(scip_));
    RETURN_AND_STORE_IF_SCIP_ERROR(
        SCIPchgLhsLinear(scip_, scip_constraints_[index], lb));
    RETURN_AND_STORE_IF_SCIP_ERROR(
        SCIPchgRhsLinear(scip_, scip_constraints_[index], ub));
  } else {
    sync_status_ = MUST_RELOAD;
  }
}

void SCIPInterface::SetCoefficient(MPConstraint* constraint,
                                   const MPVariable* variable, double new_value,
                                   double old_value) {
  RETURN_IF_ALREADY_IN_ERROR_STATE;
  InvalidateSolutionSynchronization();
  if (variable_is_extracted(variable->index()) &&
      constraint_is_extracted(constraint->index())) {
    // The modification of the coefficient for an extracted row and
    // variable is not cached.
    DCHECK_LT(constraint->index(), last_constraint_index_);
    DCHECK_LT(variable->index(), last_variable_index_);
    // SCIP does not allow to set a coefficient directly, so we add the
    // difference between the new and the old value instead.
    RETURN_AND_STORE_IF_SCIP_ERROR(SCIPfreeTransform(scip_));
    RETURN_AND_STORE_IF_SCIP_ERROR(SCIPaddCoefLinear(
        scip_, scip_constraints_[constraint->index()],
        scip_variables_[variable->index()], new_value - old_value));
  } else {
    // The modification of an unextracted row or variable is cached
    // and handled in ExtractModel.
    sync_status_ = MUST_RELOAD;
  }
}

// Not cached
void SCIPInterface::ClearConstraint(MPConstraint* constraint) {
  RETURN_IF_ALREADY_IN_ERROR_STATE;
  InvalidateSolutionSynchronization();
  const int constraint_index = constraint->index();
  // Constraint may not have been extracted yet.
  if (!constraint_is_extracted(constraint_index)) return;
  for (const auto& entry : constraint->coefficients_) {
    const int var_index = entry.first->index();
    const double old_coef_value = entry.second;
    DCHECK(variable_is_extracted(var_index));
    RETURN_AND_STORE_IF_SCIP_ERROR(SCIPfreeTransform(scip_));
    // Set coefficient to zero by subtracting the old coefficient value.
    RETURN_AND_STORE_IF_SCIP_ERROR(
        SCIPaddCoefLinear(scip_, scip_constraints_[constraint_index],
                          scip_variables_[var_index], -old_coef_value));
  }
}

// Cached
void SCIPInterface::SetObjectiveCoefficient(const MPVariable* variable,
                                            double coefficient) {
  sync_status_ = MUST_RELOAD;
}

// Cached
void SCIPInterface::SetObjectiveOffset(double value) {
  sync_status_ = MUST_RELOAD;
}

// Clear objective of all its terms.
void SCIPInterface::ClearObjective() {
  RETURN_IF_ALREADY_IN_ERROR_STATE;
  sync_status_ = MUST_RELOAD;

  InvalidateSolutionSynchronization();
  RETURN_AND_STORE_IF_SCIP_ERROR(SCIPfreeTransform(scip_));
  // Clear linear terms
  for (const auto& entry : solver_->objective_->coefficients_) {
    const int var_index = entry.first->index();
    // Variable may have not been extracted yet.
    if (!variable_is_extracted(var_index)) {
      DCHECK_NE(MODEL_SYNCHRONIZED, sync_status_);
    } else {
      RETURN_AND_STORE_IF_SCIP_ERROR(
          SCIPchgVarObj(scip_, scip_variables_[var_index], 0.0));
    }
  }
  // Note: we don't clear the objective offset here because it's not necessary
  // (it's always reset anyway in ExtractObjective) and we sometimes run into
  // crashes when clearing the whole model (see
  // http://test/OCL:253365573:BASE:253566457:1560777456754:e181f4ab).
  // It's not worth to spend time investigating this issue.
}

void SCIPInterface::BranchingPriorityChangedForVariable(int var_index) {
  // As of 2019-05, SCIP does not support setting branching priority for
  // variables in models that have already been solved. Therefore, we force
  // reset the model when setting the priority on an already extracted variable.
  // Note that this is a more drastic step than merely changing the sync_status.
  // This may be slightly conservative, as it is technically possible that
  // the extraction has occurred without a call to Solve().
  if (variable_is_extracted(var_index)) {
    branching_priority_reset_ = true;
  }
}

void SCIPInterface::AddRowConstraint(MPConstraint* ct) {
  sync_status_ = MUST_RELOAD;
}

bool SCIPInterface::AddIndicatorConstraint(MPConstraint* ct) {
  sync_status_ = MUST_RELOAD;
  return true;
}

void SCIPInterface::AddVariable(MPVariable* var) { sync_status_ = MUST_RELOAD; }

void SCIPInterface::ExtractNewVariables() {
  RETURN_IF_ALREADY_IN_ERROR_STATE;
  int total_num_vars = solver_->variables_.size();
  if (total_num_vars > last_variable_index_) {
    RETURN_AND_STORE_IF_SCIP_ERROR(SCIPfreeTransform(scip_));
    // Define new variables
    for (int j = last_variable_index_; j < total_num_vars; ++j) {
      MPVariable* const var = solver_->variables_[j];
      DCHECK(!variable_is_extracted(j));
      set_variable_as_extracted(j, true);
      SCIP_VAR* scip_var = nullptr;
      // The true objective coefficient will be set later in ExtractObjective.
      double tmp_obj_coef = 0.0;
      RETURN_AND_STORE_IF_SCIP_ERROR(SCIPcreateVar(
          scip_, &scip_var, var->name().c_str(), var->lb(), var->ub(),
          tmp_obj_coef,
          var->integer() ? SCIP_VARTYPE_INTEGER : SCIP_VARTYPE_CONTINUOUS, true,
          false, nullptr, nullptr, nullptr, nullptr, nullptr));
      RETURN_AND_STORE_IF_SCIP_ERROR(SCIPaddVar(scip_, scip_var));
      scip_variables_.push_back(scip_var);
      const int branching_priority = var->branching_priority();
      if (branching_priority != 0) {
        const int index = var->index();
        RETURN_AND_STORE_IF_SCIP_ERROR(SCIPchgVarBranchPriority(
            scip_, scip_variables_[index], branching_priority));
      }
    }
    // Add new variables to existing constraints.
    for (int i = 0; i < last_constraint_index_; i++) {
      MPConstraint* const ct = solver_->constraints_[i];
      for (const auto& entry : ct->coefficients_) {
        const int var_index = entry.first->index();
        DCHECK(variable_is_extracted(var_index));
        if (var_index >= last_variable_index_) {
          // The variable is new, so we know the previous coefficient
          // value was 0 and we can directly add the coefficient.
          RETURN_AND_STORE_IF_SCIP_ERROR(
              SCIPaddCoefLinear(scip_, scip_constraints_[i],
                                scip_variables_[var_index], entry.second));
        }
      }
    }
  }
}

void SCIPInterface::ExtractNewConstraints() {
  RETURN_IF_ALREADY_IN_ERROR_STATE;
  int total_num_rows = solver_->constraints_.size();
  if (last_constraint_index_ < total_num_rows) {
    RETURN_AND_STORE_IF_SCIP_ERROR(SCIPfreeTransform(scip_));
    // Find the length of the longest row.
    int max_row_length = 0;
    for (int i = last_constraint_index_; i < total_num_rows; ++i) {
      MPConstraint* const ct = solver_->constraints_[i];
      DCHECK(!constraint_is_extracted(i));
      set_constraint_as_extracted(i, true);
      if (ct->coefficients_.size() > max_row_length) {
        max_row_length = ct->coefficients_.size();
      }
    }
    std::unique_ptr<SCIP_VAR*[]> vars(new SCIP_VAR*[max_row_length]);
    std::unique_ptr<double[]> coeffs(new double[max_row_length]);
    // Add each new constraint.
    for (int i = last_constraint_index_; i < total_num_rows; ++i) {
      MPConstraint* const ct = solver_->constraints_[i];
      DCHECK(constraint_is_extracted(i));
      const int size = ct->coefficients_.size();
      int j = 0;
      for (const auto& entry : ct->coefficients_) {
        const int var_index = entry.first->index();
        DCHECK(variable_is_extracted(var_index));
        vars[j] = scip_variables_[var_index];
        coeffs[j] = entry.second;
        j++;
      }
      SCIP_CONS* scip_constraint = nullptr;
      const bool is_lazy = ct->is_lazy();
      if (ct->indicator_variable() != nullptr) {
        const int ind_index = ct->indicator_variable()->index();
        DCHECK(variable_is_extracted(ind_index));
        SCIP_VAR* ind_var = scip_variables_[ind_index];
        if (ct->indicator_value() == 0) {
          RETURN_AND_STORE_IF_SCIP_ERROR(
              SCIPgetNegatedVar(scip_, scip_variables_[ind_index], &ind_var));
        }

        if (ct->ub() < std::numeric_limits<double>::infinity()) {
          RETURN_AND_STORE_IF_SCIP_ERROR(SCIPcreateConsIndicator(
              scip_, &scip_constraint, ct->name().c_str(), ind_var, size,
              vars.get(), coeffs.get(), ct->ub(),
              /*initial=*/!is_lazy,
              /*separate=*/true,
              /*enforce=*/true,
              /*check=*/true,
              /*propagate=*/true,
              /*local=*/false,
              /*dynamic=*/false,
              /*removable=*/is_lazy,
              /*stickingatnode=*/false));
          RETURN_AND_STORE_IF_SCIP_ERROR(SCIPaddCons(scip_, scip_constraint));
          scip_constraints_.push_back(scip_constraint);
        }
        if (ct->lb() > -std::numeric_limits<double>::infinity()) {
          for (int i = 0; i < size; ++i) {
            coeffs[i] *= -1;
          }
          RETURN_AND_STORE_IF_SCIP_ERROR(SCIPcreateConsIndicator(
              scip_, &scip_constraint, ct->name().c_str(), ind_var, size,
              vars.get(), coeffs.get(), -ct->lb(),
              /*initial=*/!is_lazy,
              /*separate=*/true,
              /*enforce=*/true,
              /*check=*/true,
              /*propagate=*/true,
              /*local=*/false,
              /*dynamic=*/false,
              /*removable=*/is_lazy,
              /*stickingatnode=*/false));
          RETURN_AND_STORE_IF_SCIP_ERROR(SCIPaddCons(scip_, scip_constraint));
          scip_constraints_.push_back(scip_constraint);
        }
      } else {
        // See
        // http://scip.zib.de/doc/html/cons__linear_8h.php#aa7aed137a4130b35b168812414413481
        // for an explanation of the parameters.
        RETURN_AND_STORE_IF_SCIP_ERROR(SCIPcreateConsLinear(
            scip_, &scip_constraint, ct->name().c_str(), size, vars.get(),
            coeffs.get(), ct->lb(), ct->ub(),
            /*initial=*/!is_lazy,
            /*separate=*/true,
            /*enforce=*/true,
            /*check=*/true,
            /*propagate=*/true,
            /*local=*/false,
            /*modifiable=*/false,
            /*dynamic=*/false,
            /*removable=*/is_lazy,
            /*stickingatnode=*/false));
        RETURN_AND_STORE_IF_SCIP_ERROR(SCIPaddCons(scip_, scip_constraint));
        scip_constraints_.push_back(scip_constraint);
      }
    }
  }
}

void SCIPInterface::ExtractObjective() {
  RETURN_IF_ALREADY_IN_ERROR_STATE;
  RETURN_AND_STORE_IF_SCIP_ERROR(SCIPfreeTransform(scip_));
  // Linear objective: set objective coefficients for all variables (some might
  // have been modified).
  for (const auto& entry : solver_->objective_->coefficients_) {
    const int var_index = entry.first->index();
    const double obj_coef = entry.second;
    RETURN_AND_STORE_IF_SCIP_ERROR(
        SCIPchgVarObj(scip_, scip_variables_[var_index], obj_coef));
  }

  // Constant term: change objective offset.
  RETURN_AND_STORE_IF_SCIP_ERROR(SCIPaddOrigObjoffset(
      scip_, solver_->Objective().offset() - SCIPgetOrigObjoffset(scip_)));
}

#define RETURN_ABNORMAL_IF_BAD_STATUS             \
  do {                                            \
    if (!status_.ok()) {                          \
      LOG_IF(INFO, solver_->OutputIsEnabled())    \
          << "Invalid SCIP status: " << status_;  \
      return result_status_ = MPSolver::ABNORMAL; \
    }                                             \
  } while (false)

#define RETURN_ABNORMAL_IF_SCIP_ERROR(x) \
  do {                                   \
    RETURN_ABNORMAL_IF_BAD_STATUS;       \
    status_ = SCIP_TO_STATUS(x);         \
    RETURN_ABNORMAL_IF_BAD_STATUS;       \
  } while (false);

MPSolver::ResultStatus SCIPInterface::Solve(const MPSolverParameters& param) {
  // "status_" may encode a variety of failure scenarios, many of which would
  // correspond to another MPResultStatus than ABNORMAL, but since SCIP is a
  // moving target, we use the most likely error code here (abnormalities,
  // often numeric), and rely on the user enabling output to see more details.
  RETURN_ABNORMAL_IF_BAD_STATUS;

  WallTimer timer;
  timer.Start();

  // Note that SCIP does not provide any incrementality.
  // TODO(user): Is that still true now (2018) ?
  if (param.GetIntegerParam(MPSolverParameters::INCREMENTALITY) ==
          MPSolverParameters::INCREMENTALITY_OFF ||
      branching_priority_reset_ || callback_reset_) {
    Reset();
    branching_priority_reset_ = false;
    callback_reset_ = false;
  }

  // Set log level.
  SCIPsetMessagehdlrQuiet(scip_, quiet_);

  // Special case if the model is empty since SCIP expects a non-empty model.
  if (solver_->variables_.empty() && solver_->constraints_.empty()) {
    sync_status_ = SOLUTION_SYNCHRONIZED;
    result_status_ = MPSolver::OPTIMAL;
    objective_value_ = solver_->Objective().offset();
    best_objective_bound_ = solver_->Objective().offset();
    return result_status_;
  }

  ExtractModel();
  VLOG(1) << absl::StrFormat("Model built in %s.",
                             absl::FormatDuration(timer.GetDuration()));
  if (scip_constraint_handler_ != nullptr) {
    // When the value of `callback_` is changed, `callback_reset_` is set and
    // code above you call Reset() that should have cleared
    // `scip_constraint_handler_`. Here we assert that if this has not happened
    // then `callback_` value has not changed.
    CHECK_EQ(scip_constraint_handler_->mp_callback(), callback_);
  } else if (callback_ != nullptr) {
    scip_constraint_handler_ =
        absl::make_unique<ScipConstraintHandlerForMPCallback>(callback_);
    RegisterConstraintHandler<EmptyStruct>(scip_constraint_handler_.get(),
                                           scip_);
    AddCallbackConstraint<EmptyStruct>(scip_, scip_constraint_handler_.get(),
                                       "mp_solver_callback_constraint_for_scip",
                                       &constraint_data_for_handler_,
                                       ScipCallbackConstraintOptions());
  }

  // Time limit.
  if (solver_->time_limit() != 0) {
    VLOG(1) << "Setting time limit = " << solver_->time_limit() << " ms.";
    RETURN_ABNORMAL_IF_SCIP_ERROR(
        SCIPsetRealParam(scip_, "limits/time", solver_->time_limit_in_secs()));
  } else {
    RETURN_ABNORMAL_IF_SCIP_ERROR(SCIPresetParam(scip_, "limits/time"));
  }

  // We first set our internal MPSolverParameters from param and then set any
  // user specified internal solver, ie. SCIP, parameters via
  // solver_specific_parameter_string_.
  // Default MPSolverParameters can override custom parameters (for example for
  // presolving) and therefore we apply MPSolverParameters first.
  SetParameters(param);
  solver_->SetSolverSpecificParametersAsString(
      solver_->solver_specific_parameter_string_);

  // Use the solution hint if any.
  if (!solver_->solution_hint_.empty()) {
    SCIP_SOL* solution;
    bool is_solution_partial = false;
    const int num_vars = solver_->variables_.size();
    if (solver_->solution_hint_.size() != num_vars) {
      // We start by creating an empty partial solution.
      RETURN_ABNORMAL_IF_SCIP_ERROR(
          SCIPcreatePartialSol(scip_, &solution, nullptr));
      is_solution_partial = true;
    } else {
      // We start by creating the all-zero solution.
      RETURN_ABNORMAL_IF_SCIP_ERROR(SCIPcreateSol(scip_, &solution, nullptr));
    }

    // Fill the other variables from the given solution hint.
    for (const std::pair<const MPVariable*, double>& p :
         solver_->solution_hint_) {
      RETURN_ABNORMAL_IF_SCIP_ERROR(SCIPsetSolVal(
          scip_, solution, scip_variables_[p.first->index()], p.second));
    }

    if (!is_solution_partial) {
      SCIP_Bool is_feasible;
      RETURN_ABNORMAL_IF_SCIP_ERROR(SCIPcheckSol(
          scip_, solution, /*printreason=*/false, /*completely=*/true,
          /*checkbounds=*/true, /*checkintegrality=*/true, /*checklprows=*/true,
          &is_feasible));
      VLOG(1) << "Solution hint is "
              << (is_feasible ? "FEASIBLE" : "INFEASIBLE");
    }

    // TODO(user): I more or less copied this from the SCIPreadSol() code that
    // reads a solution from a file. I am not sure what SCIPisTransformed() is
    // or what is the difference between the try and add version. In any case
    // this seems to always call SCIPaddSolFree() for now and it works.
    SCIP_Bool is_stored;
    if (!is_solution_partial && SCIPisTransformed(scip_)) {
      RETURN_ABNORMAL_IF_SCIP_ERROR(SCIPtrySolFree(
          scip_, &solution, /*printreason=*/false, /*completely=*/true,
          /*checkbounds=*/true, /*checkintegrality=*/true, /*checklprows=*/true,
          &is_stored));
    } else {
      RETURN_ABNORMAL_IF_SCIP_ERROR(
          SCIPaddSolFree(scip_, &solution, &is_stored));
    }
  }

  // Solve.
  timer.Restart();
  RETURN_ABNORMAL_IF_SCIP_ERROR(solver_->GetNumThreads() > 1
                                    ? SCIPsolveConcurrent(scip_)
                                    : SCIPsolve(scip_));
  VLOG(1) << absl::StrFormat("Solved in %s.",
                             absl::FormatDuration(timer.GetDuration()));
  current_solution_index_ = 0;
  // Get the results.
  SCIP_SOL* const solution = SCIPgetBestSol(scip_);
  if (solution != nullptr) {
    // If optimal or feasible solution is found.
    SetSolution(solution);
  } else {
    VLOG(1) << "No feasible solution found.";
  }

  // Check the status: optimal, infeasible, etc.
  SCIP_STATUS scip_status = SCIPgetStatus(scip_);
  switch (scip_status) {
    case SCIP_STATUS_OPTIMAL:
      result_status_ = MPSolver::OPTIMAL;
      break;
    case SCIP_STATUS_GAPLIMIT:
      // To be consistent with the other solvers.
      result_status_ = MPSolver::OPTIMAL;
      break;
    case SCIP_STATUS_INFEASIBLE:
      result_status_ = MPSolver::INFEASIBLE;
      break;
    case SCIP_STATUS_UNBOUNDED:
      result_status_ = MPSolver::UNBOUNDED;
      break;
    case SCIP_STATUS_INFORUNBD:
      // TODO(user): We could introduce our own "infeasible or
      // unbounded" status.
      result_status_ = MPSolver::INFEASIBLE;
      break;
    default:
      if (solution != nullptr) {
        result_status_ = MPSolver::FEASIBLE;
      } else if (scip_status == SCIP_STATUS_TIMELIMIT ||
                 scip_status == SCIP_STATUS_TOTALNODELIMIT) {
        result_status_ = MPSolver::NOT_SOLVED;
      } else {
        result_status_ = MPSolver::ABNORMAL;
      }
      break;
  }

  RETURN_ABNORMAL_IF_SCIP_ERROR(SCIPresetParams(scip_));

  sync_status_ = SOLUTION_SYNCHRONIZED;
  return result_status_;
}

void SCIPInterface::SetSolution(SCIP_SOL* solution) {
  objective_value_ = SCIPgetSolOrigObj(scip_, solution);
  best_objective_bound_ = SCIPgetDualbound(scip_);
  VLOG(1) << "objective=" << objective_value_
          << ", bound=" << best_objective_bound_;
  for (int i = 0; i < solver_->variables_.size(); ++i) {
    MPVariable* const var = solver_->variables_[i];
    const int var_index = var->index();
    const double val =
        SCIPgetSolVal(scip_, solution, scip_variables_[var_index]);
    var->set_solution_value(val);
    VLOG(3) << var->name() << "=" << val;
  }
}

absl::optional<MPSolutionResponse> SCIPInterface::DirectlySolveProto(
    const MPModelRequest& request) {
  // ScipSolveProto doesn't solve concurrently.
  if (solver_->GetNumThreads() > 1) return absl::nullopt;

  const auto status_or = ScipSolveProto(request);
  if (status_or.ok()) return status_or.value();
  // Special case: if something is not implemented yet, fall back to solving
  // through MPSolver.
  if (absl::IsUnimplemented(status_or.status())) return absl::nullopt;

  if (request.enable_internal_solver_output()) {
    LOG(INFO) << "Invalid SCIP status: " << status_or.status();
  }
  MPSolutionResponse response;
  response.set_status(MPSOLVER_NOT_SOLVED);
  response.set_status_str(status_or.status().ToString());
  return response;
}

int SCIPInterface::SolutionCount() { return SCIPgetNSols(scip_); }

bool SCIPInterface::NextSolution() {
  // Make sure we have successfully solved the problem and not modified it.
  if (!CheckSolutionIsSynchronizedAndExists()) {
    return false;
  }
  if (current_solution_index_ + 1 >= SolutionCount()) {
    return false;
  }
  current_solution_index_++;
  SCIP_SOL** all_solutions = SCIPgetSols(scip_);
  SetSolution(all_solutions[current_solution_index_]);
  return true;
}

int64_t SCIPInterface::iterations() const {
  // NOTE(user): As of 2018-12 it doesn't run in the stubby server, and is
  // a specialized call, so it's ok to crash if the status is broken.
  if (!CheckSolutionIsSynchronized()) return kUnknownNumberOfIterations;
  return SCIPgetNLPIterations(scip_);
}

int64_t SCIPInterface::nodes() const {
  // NOTE(user): Same story as iterations(): it's OK to crash here.
  if (!CheckSolutionIsSynchronized()) return kUnknownNumberOfNodes;
  // This is the total number of nodes used in the solve, potentially across
  // multiple branch-and-bound trees. Use limits/totalnodes (rather than
  // limits/nodes) to control this value.
  return SCIPgetNTotalNodes(scip_);
}

void SCIPInterface::SetParameters(const MPSolverParameters& param) {
  SetCommonParameters(param);
  SetMIPParameters(param);
}

void SCIPInterface::SetRelativeMipGap(double value) {
  // NOTE(user): We don't want to call RETURN_IF_ALREADY_IN_ERROR_STATE here,
  // because even if the solver is in an error state, the user might be setting
  // some parameters and then "restoring" the solver to a non-error state by
  // calling Reset(), which should *not* reset the parameters.
  // So we want the parameter-setting functions to be resistant to being in an
  // error state, essentially. What we do is:
  // - we call the parameter-setting function anyway (I'm assuming that SCIP
  //   won't crash even if we're in an error state. I did *not* verify this).
  // - if that call yielded an error *and* we weren't already in an error state,
  //   set the state to that error we just got.
  const auto status =
      SCIP_TO_STATUS(SCIPsetRealParam(scip_, "limits/gap", value));
  if (status_.ok()) status_ = status;
}

void SCIPInterface::SetPrimalTolerance(double value) {
  // See the NOTE on SetRelativeMipGap().
  const auto status =
      SCIP_TO_STATUS(SCIPsetRealParam(scip_, "numerics/feastol", value));
  if (status_.ok()) status_ = status;
}

void SCIPInterface::SetDualTolerance(double value) {
  const auto status =
      SCIP_TO_STATUS(SCIPsetRealParam(scip_, "numerics/dualfeastol", value));
  if (status_.ok()) status_ = status;
}

void SCIPInterface::SetPresolveMode(int presolve) {
  // See the NOTE on SetRelativeMipGap().
  switch (presolve) {
    case MPSolverParameters::PRESOLVE_OFF: {
      const auto status =
          SCIP_TO_STATUS(SCIPsetIntParam(scip_, "presolving/maxrounds", 0));
      if (status_.ok()) status_ = status;
      return;
    }
    case MPSolverParameters::PRESOLVE_ON: {
      const auto status =
          SCIP_TO_STATUS(SCIPsetIntParam(scip_, "presolving/maxrounds", -1));
      if (status_.ok()) status_ = status;
      return;
    }
    default: {
      SetIntegerParamToUnsupportedValue(MPSolverParameters::PRESOLVE, presolve);
      return;
    }
  }
}

void SCIPInterface::SetScalingMode(int scaling) {
  SetUnsupportedIntegerParam(MPSolverParameters::SCALING);
}

// Only the root LP algorithm is set as setting the node LP to a
// non-default value rarely is beneficial. The node LP algorithm could
// be set as well with "lp/resolvealgorithm".
void SCIPInterface::SetLpAlgorithm(int lp_algorithm) {
  // See the NOTE on SetRelativeMipGap().
  switch (lp_algorithm) {
    case MPSolverParameters::DUAL: {
      const auto status =
          SCIP_TO_STATUS(SCIPsetCharParam(scip_, "lp/initalgorithm", 'd'));
      if (status_.ok()) status_ = status;
      return;
    }
    case MPSolverParameters::PRIMAL: {
      const auto status =
          SCIP_TO_STATUS(SCIPsetCharParam(scip_, "lp/initalgorithm", 'p'));
      if (status_.ok()) status_ = status;
      return;
    }
    case MPSolverParameters::BARRIER: {
      // Barrier with crossover.
      const auto status =
          SCIP_TO_STATUS(SCIPsetCharParam(scip_, "lp/initalgorithm", 'p'));
      if (status_.ok()) status_ = status;
      return;
    }
    default: {
      SetIntegerParamToUnsupportedValue(MPSolverParameters::LP_ALGORITHM,
                                        lp_algorithm);
      return;
    }
  }
}

void SCIPInterface::SetUnsupportedIntegerParam(
    MPSolverParameters::IntegerParam param) {
  MPSolverInterface::SetUnsupportedIntegerParam(param);
  if (status_.ok()) {
    status_ = absl::InvalidArgumentError(absl::StrFormat(
        "Tried to set unsupported integer parameter %d", param));
  }
}

void SCIPInterface::SetIntegerParamToUnsupportedValue(
    MPSolverParameters::IntegerParam param, int value) {
  MPSolverInterface::SetIntegerParamToUnsupportedValue(param, value);
  if (status_.ok()) {
    status_ = absl::InvalidArgumentError(absl::StrFormat(
        "Tried to set integer parameter %d to unsupported value %d", param,
        value));
  }
}

absl::Status SCIPInterface::SetNumThreads(int num_threads) {
  if (SetSolverSpecificParametersAsString(
          absl::StrFormat("parallel/maxnthreads = %d\n", num_threads))) {
    return absl::OkStatus();
  }
  return absl::InternalError(
      "Could not set parallel/maxnthreads, which may "
      "indicate that SCIP API has changed.");
}

bool SCIPInterface::SetSolverSpecificParametersAsString(
    const std::string& parameters) {
  const absl::Status s =
      LegacyScipSetSolverSpecificParameters(parameters, scip_);
  if (!s.ok()) {
    LOG(WARNING) << "Failed to set SCIP parameter string: " << parameters
                 << ", error is: " << s;
  }
  return s.ok();
}

class ScipMPCallbackContext : public MPCallbackContext {
 public:
  ScipMPCallbackContext(const ScipConstraintHandlerContext* scip_context,
                        bool at_integer_solution)
      : scip_context_(scip_context),
        at_integer_solution_(at_integer_solution) {}

  MPCallbackEvent Event() override {
    if (at_integer_solution_) {
      return MPCallbackEvent::kMipSolution;
    }
    return MPCallbackEvent::kMipNode;
  }

  bool CanQueryVariableValues() override {
    return !scip_context_->is_pseudo_solution();
  }

  double VariableValue(const MPVariable* variable) override {
    CHECK(CanQueryVariableValues());
    return scip_context_->VariableValue(variable);
  }

  void AddCut(const LinearRange& cutting_plane) override {
    CallbackRangeConstraint constraint;
    constraint.is_cut = true;
    constraint.range = cutting_plane;
    constraint.local = false;
    constraints_added_.push_back(std::move(constraint));
  }

  void AddLazyConstraint(const LinearRange& lazy_constraint) override {
    CallbackRangeConstraint constraint;
    constraint.is_cut = false;
    constraint.range = lazy_constraint;
    constraint.local = false;
    constraints_added_.push_back(std::move(constraint));
  }

  double SuggestSolution(
      const absl::flat_hash_map<const MPVariable*, double>& solution) override {
    LOG(FATAL) << "SuggestSolution() not currently supported for SCIP.";
  }

  double GetRelativeMipGap() override {
      SCIP* scip = scip_context_->scip();
      SCIP_SET* set = scip->set;

      return SCIPcomputeGap(set->num_epsilon, set->num_infinity, SCIPgetPrimalbound(scip), SCIPgetDualbound(scip));
  }

  bool HasValidMipSolution() override {
      SCIP* scip = scip_context_->scip();
      return SCIPgetBestSol(scip) != 0;
  }

  bool IsNewSolution() {
      return HasValidMipSolution() && scip_context_->IsNewSolution();
  }

  int64_t NumExploredNodes() override {
    // scip_context_->NumNodesProcessed() returns:
    //   0 before the root node is solved, e.g. if a heuristic finds a solution.
    //   1 at the root node
    //   > 1 after the root node.
    // The NumExploredNodes spec requires that we return 0 at the root node,
    // (this is consistent with gurobi).  Below is a bandaid to try and make the
    // behavior consistent, although some information is lost.
    return std::max(int64_t{0}, scip_context_->NumNodesProcessed() - 1);
  }

  const std::vector<CallbackRangeConstraint>& constraints_added() {
    return constraints_added_;
  }

 private:
  const ScipConstraintHandlerContext* scip_context_;
  bool at_integer_solution_;
  // second value of pair is true for cuts and false for lazy constraints.
  std::vector<CallbackRangeConstraint> constraints_added_;
};

ScipConstraintHandlerForMPCallback::ScipConstraintHandlerForMPCallback(
    MPCallback* mp_callback)
    : ScipConstraintHandler<EmptyStruct>(
          // MOE(begin-strip):
          {/*name=*/"mp_solver_constraint_handler",
           /*description=*/
           "A single constraint handler for all MPSolver models."}
          // MOE(end-strip-and-replace): ScipConstraintHandlerDescription()
          ),
      mp_callback_(mp_callback) {}

std::vector<CallbackRangeConstraint>
ScipConstraintHandlerForMPCallback::SeparateFractionalSolution(
    const ScipConstraintHandlerContext& context, const EmptyStruct&) {
  return SeparateSolution(context, /*at_integer_solution=*/false);
}

std::vector<CallbackRangeConstraint>
ScipConstraintHandlerForMPCallback::SeparateIntegerSolution(
    const ScipConstraintHandlerContext& context, const EmptyStruct&) {
  return SeparateSolution(context, /*at_integer_solution=*/true);
}

std::vector<CallbackRangeConstraint>
ScipConstraintHandlerForMPCallback::SeparateSolution(
    const ScipConstraintHandlerContext& context,
    const bool at_integer_solution) {
  ScipMPCallbackContext mp_context(&context, at_integer_solution);
  mp_callback_->RunCallback(&mp_context);
  return mp_context.constraints_added();
}

void SCIPInterface::SetCallback(MPCallback* mp_callback) {
  if (callback_ != nullptr) {
    callback_reset_ = true;
  }
  callback_ = mp_callback;
}

MPSolverInterface* BuildSCIPInterface(MPSolver* const solver) {
  return new SCIPInterface(solver);
}

}  // namespace operations_research
#endif  //  #if defined(USE_SCIP)

#undef RETURN_AND_STORE_IF_SCIP_ERROR
#undef RETURN_IF_ALREADY_IN_ERROR_STATE
#undef RETURN_ABNORMAL_IF_BAD_STATUS
#undef RETURN_ABNORMAL_IF_SCIP_ERROR
