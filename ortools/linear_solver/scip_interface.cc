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

#if defined(USE_SCIP)

#include <stddef.h>
#include <algorithm>
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>

#include "ortools/base/commandlineflags.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/stringprintf.h"
#include "ortools/base/timer.h"
#include "scip/scip.h"
#include "scip/scipdefplugins.h"
#include "ortools/base/hash.h"
#include "ortools/linear_solver/linear_solver.h"

// Our own version of SCIP_CALL to do error management.
// TODO(user): The error management could be improved, especially
// for the Solve method. We should return an error status (did the
// solver encounter problems?) and let the user query the result
// status (optimal, infeasible, ...) with a separate method. This is a
// common API for solvers. The API change in all existing code might
// not be worth it.
#define ORTOOLS_SCIP_CALL(x) CHECK_EQ(SCIP_OKAY, x)

DEFINE_bool(scip_feasibility_emphasis, false,
            "When true, emphasize search towards feasibility. This may or "
            "may not result in speedups in some problems.");

namespace operations_research {

class SCIPInterface : public MPSolverInterface {
 public:
  explicit SCIPInterface(MPSolver* solver);
  ~SCIPInterface() override;

  void SetOptimizationDirection(bool maximize) override;
  MPSolver::ResultStatus Solve(const MPSolverParameters& param) override;
  void Reset() override;

  void SetVariableBounds(int var_index, double lb, double ub) override;
  void SetVariableInteger(int var_index, bool integer) override;
  void SetConstraintBounds(int row_index, double lb, double ub) override;

  void AddRowConstraint(MPConstraint* ct) override;
  void AddVariable(MPVariable* var) override;
  void SetCoefficient(MPConstraint* constraint, const MPVariable* variable,
                      double new_value, double old_value) override;
  void ClearConstraint(MPConstraint* constraint) override;
  void SetObjectiveCoefficient(const MPVariable* variable,
                               double coefficient) override;
  void SetObjectiveOffset(double value) override;
  void ClearObjective() override;

  int64 iterations() const override;
  int64 nodes() const override;
  double best_objective_bound() const override;
  MPSolver::BasisStatus row_status(int constraint_index) const override {
    LOG(FATAL) << "Basis status only available for continuous problems";
    return MPSolver::FREE;
  }
  MPSolver::BasisStatus column_status(int variable_index) const override {
    LOG(FATAL) << "Basis status only available for continuous problems";
    return MPSolver::FREE;
  }

  bool IsContinuous() const override { return false; }
  bool IsLP() const override { return false; }
  bool IsMIP() const override { return true; }

  void ExtractNewVariables() override;
  void ExtractNewConstraints() override;
  void ExtractObjective() override;

  std::string SolverVersion() const override {
    return StringPrintf("SCIP %d.%d.%d [LP solver: %s]", SCIPmajorVersion(),
                        SCIPminorVersion(), SCIPtechVersion(),
                        SCIPlpiGetSolverName());
  }

  bool InterruptSolve() override {
    if (scip_ != nullptr) SCIPinterruptSolve(scip_);
    return true;
  }

  void* underlying_solver() override { return reinterpret_cast<void*>(scip_); }

 private:
  void SetParameters(const MPSolverParameters& param) override;
  void SetRelativeMipGap(double value) override;
  void SetPrimalTolerance(double value) override;
  void SetDualTolerance(double value) override;
  void SetPresolveMode(int value) override;
  void SetScalingMode(int value) override;
  void SetLpAlgorithm(int value) override;

  bool ReadParameterFile(const std::string& filename) override;
  std::string ValidFileExtensionForParameterFile() const override;

  void CreateSCIP();
  void DeleteSCIP();

  SCIP* scip_;
  SCIP_VAR* objective_offset_variable_;
  std::vector<SCIP_VAR*> scip_variables_;
  std::vector<SCIP_CONS*> scip_constraints_;
};

SCIPInterface::SCIPInterface(MPSolver* solver)
    : MPSolverInterface(solver), scip_(nullptr) {
  CreateSCIP();
}

SCIPInterface::~SCIPInterface() { DeleteSCIP(); }

void SCIPInterface::Reset() {
  DeleteSCIP();
  CreateSCIP();
  ResetExtractionInformation();
}

void SCIPInterface::CreateSCIP() {
  ORTOOLS_SCIP_CALL(SCIPcreate(&scip_));
  ORTOOLS_SCIP_CALL(SCIPincludeDefaultPlugins(scip_));
  // Set the emphasis to enum SCIP_PARAMEMPHASIS_FEASIBILITY. Do not print
  // the new parameter (quiet = true).
  if (FLAGS_scip_feasibility_emphasis) {
    ORTOOLS_SCIP_CALL(SCIPsetEmphasis(scip_, SCIP_PARAMEMPHASIS_FEASIBILITY,
                                      /*quiet=*/true));
  }
  // Default clock type. We use wall clock time because getting CPU user seconds
  // involves calling times() which is very expensive.
  ORTOOLS_SCIP_CALL(
      SCIPsetIntParam(scip_, "timing/clocktype", SCIP_CLOCKTYPE_WALL));
  ORTOOLS_SCIP_CALL(SCIPcreateProb(scip_, solver_->name_.c_str(), nullptr,
                                   nullptr, nullptr, nullptr, nullptr, nullptr,
                                   nullptr));
  ORTOOLS_SCIP_CALL(SCIPsetObjsense(
      scip_, maximize_ ? SCIP_OBJSENSE_MAXIMIZE : SCIP_OBJSENSE_MINIMIZE));
  // SCIPaddObjoffset cannot be used at the problem building stage. So we handle
  // the objective offset by creating a dummy variable.
  objective_offset_variable_ = nullptr;
  // The true objective coefficient will be set in ExtractObjective.
  double dummy_obj_coef = 0.0;
  ORTOOLS_SCIP_CALL(SCIPcreateVar(scip_, &objective_offset_variable_, "dummy",
                                  1.0, 1.0, dummy_obj_coef,
                                  SCIP_VARTYPE_CONTINUOUS, true, false, nullptr,
                                  nullptr, nullptr, nullptr, nullptr));
  ORTOOLS_SCIP_CALL(SCIPaddVar(scip_, objective_offset_variable_));
}

void SCIPInterface::DeleteSCIP() {
  CHECK(scip_ != nullptr);
  ORTOOLS_SCIP_CALL(SCIPreleaseVar(scip_, &objective_offset_variable_));
  for (int i = 0; i < scip_variables_.size(); ++i) {
    ORTOOLS_SCIP_CALL(SCIPreleaseVar(scip_, &scip_variables_[i]));
  }
  scip_variables_.clear();
  for (int j = 0; j < scip_constraints_.size(); ++j) {
    ORTOOLS_SCIP_CALL(SCIPreleaseCons(scip_, &scip_constraints_[j]));
  }
  scip_constraints_.clear();
  ORTOOLS_SCIP_CALL(SCIPfree(&scip_));
  scip_ = nullptr;
}

// Not cached.
void SCIPInterface::SetOptimizationDirection(bool maximize) {
  InvalidateSolutionSynchronization();
  ORTOOLS_SCIP_CALL(SCIPfreeTransform(scip_));
  ORTOOLS_SCIP_CALL(SCIPsetObjsense(
      scip_, maximize ? SCIP_OBJSENSE_MAXIMIZE : SCIP_OBJSENSE_MINIMIZE));
}

void SCIPInterface::SetVariableBounds(int var_index, double lb, double ub) {
  InvalidateSolutionSynchronization();
  if (variable_is_extracted(var_index)) {
    // Not cached if the variable has been extracted.
    DCHECK_LT(var_index, last_variable_index_);
    ORTOOLS_SCIP_CALL(SCIPfreeTransform(scip_));
    ORTOOLS_SCIP_CALL(SCIPchgVarLb(scip_, scip_variables_[var_index], lb));
    ORTOOLS_SCIP_CALL(SCIPchgVarUb(scip_, scip_variables_[var_index], ub));
  } else {
    sync_status_ = MUST_RELOAD;
  }
}

void SCIPInterface::SetVariableInteger(int var_index, bool integer) {
  InvalidateSolutionSynchronization();
  if (variable_is_extracted(var_index)) {
    // Not cached if the variable has been extracted.
    ORTOOLS_SCIP_CALL(SCIPfreeTransform(scip_));
#if (SCIP_VERSION >= 210)
    SCIP_Bool infeasible = false;
    ORTOOLS_SCIP_CALL(SCIPchgVarType(
        scip_, scip_variables_[var_index],
        integer ? SCIP_VARTYPE_INTEGER : SCIP_VARTYPE_CONTINUOUS, &infeasible));
#else
    ORTOOLS_SCIP_CALL(SCIPchgVarType(
        scip_, scip_variables_[var_index],
        integer ? SCIP_VARTYPE_INTEGER : SCIP_VARTYPE_CONTINUOUS));
#endif  // SCIP_VERSION >= 210
  } else {
    sync_status_ = MUST_RELOAD;
  }
}

void SCIPInterface::SetConstraintBounds(int index, double lb, double ub) {
  InvalidateSolutionSynchronization();
  if (constraint_is_extracted(index)) {
    // Not cached if the row has been extracted.
    DCHECK_LT(index, last_constraint_index_);
    ORTOOLS_SCIP_CALL(SCIPfreeTransform(scip_));
    ORTOOLS_SCIP_CALL(SCIPchgLhsLinear(scip_, scip_constraints_[index], lb));
    ORTOOLS_SCIP_CALL(SCIPchgRhsLinear(scip_, scip_constraints_[index], ub));
  } else {
    sync_status_ = MUST_RELOAD;
  }
}

void SCIPInterface::SetCoefficient(MPConstraint* constraint,
                                   const MPVariable* variable, double new_value,
                                   double old_value) {
  InvalidateSolutionSynchronization();
  if (variable_is_extracted(variable->index()) &&
      constraint_is_extracted(constraint->index())) {
    // The modification of the coefficient for an extracted row and
    // variable is not cached.
    DCHECK_LT(constraint->index(), last_constraint_index_);
    DCHECK_LT(variable->index(), last_variable_index_);
    // SCIP does not allow to set a coefficient directly, so we add the
    // difference between the new and the old value instead.
    ORTOOLS_SCIP_CALL(SCIPfreeTransform(scip_));
    ORTOOLS_SCIP_CALL(SCIPaddCoefLinear(
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
  InvalidateSolutionSynchronization();
  const int constraint_index = constraint->index();
  // Constraint may not have been extracted yet.
  if (!constraint_is_extracted(constraint_index)) return;
  for (CoeffEntry entry : constraint->coefficients_) {
    const int var_index = entry.first->index();
    const double old_coef_value = entry.second;
    DCHECK(variable_is_extracted(var_index));
    ORTOOLS_SCIP_CALL(SCIPfreeTransform(scip_));
    // Set coefficient to zero by substracting the old coefficient value.
    ORTOOLS_SCIP_CALL(
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
  InvalidateSolutionSynchronization();
  ORTOOLS_SCIP_CALL(SCIPfreeTransform(scip_));
  // Clear linear terms
  for (CoeffEntry entry : solver_->objective_->coefficients_) {
    const int var_index = entry.first->index();
    // Variable may have not been extracted yet.
    if (!variable_is_extracted(var_index)) {
      DCHECK_NE(MODEL_SYNCHRONIZED, sync_status_);
    } else {
      ORTOOLS_SCIP_CALL(SCIPchgVarObj(scip_, scip_variables_[var_index], 0.0));
    }
  }
  // Constant term: change objective offset variable.
  ORTOOLS_SCIP_CALL(SCIPchgVarObj(scip_, objective_offset_variable_, 0.0));
}

void SCIPInterface::AddRowConstraint(MPConstraint* ct) {
  sync_status_ = MUST_RELOAD;
}

void SCIPInterface::AddVariable(MPVariable* var) { sync_status_ = MUST_RELOAD; }

void SCIPInterface::ExtractNewVariables() {
  int total_num_vars = solver_->variables_.size();
  if (total_num_vars > last_variable_index_) {
    ORTOOLS_SCIP_CALL(SCIPfreeTransform(scip_));
    // Define new variables
    for (int j = last_variable_index_; j < total_num_vars; ++j) {
      MPVariable* const var = solver_->variables_[j];
      DCHECK(!variable_is_extracted(j));
      set_variable_as_extracted(j, true);
      SCIP_VAR* scip_var = nullptr;
      // The true objective coefficient will be set later in ExtractObjective.
      double tmp_obj_coef = 0.0;
      ORTOOLS_SCIP_CALL(SCIPcreateVar(
          scip_, &scip_var, var->name().c_str(), var->lb(), var->ub(),
          tmp_obj_coef,
          var->integer() ? SCIP_VARTYPE_INTEGER : SCIP_VARTYPE_CONTINUOUS, true,
          false, nullptr, nullptr, nullptr, nullptr, nullptr));
      ORTOOLS_SCIP_CALL(SCIPaddVar(scip_, scip_var));
      scip_variables_.push_back(scip_var);
    }
    // Add new variables to existing constraints.
    for (int i = 0; i < last_constraint_index_; i++) {
      MPConstraint* const ct = solver_->constraints_[i];
      for (CoeffEntry entry : ct->coefficients_) {
        const int var_index = entry.first->index();
        DCHECK(variable_is_extracted(var_index));
        if (var_index >= last_variable_index_) {
          // The variable is new, so we know the previous coefficient
          // value was 0 and we can directly add the coefficient.
          ORTOOLS_SCIP_CALL(SCIPaddCoefLinear(scip_, scip_constraints_[i],
                                              scip_variables_[var_index],
                                              entry.second));
        }
      }
    }
  }
}

void SCIPInterface::ExtractNewConstraints() {
  int total_num_rows = solver_->constraints_.size();
  if (last_constraint_index_ < total_num_rows) {
    ORTOOLS_SCIP_CALL(SCIPfreeTransform(scip_));
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
    std::unique_ptr<SCIP_VAR* []> vars(new SCIP_VAR*[max_row_length]);
    std::unique_ptr<double[]> coefs(new double[max_row_length]);
    // Add each new constraint.
    for (int i = last_constraint_index_; i < total_num_rows; ++i) {
      MPConstraint* const ct = solver_->constraints_[i];
      DCHECK(constraint_is_extracted(i));
      const int size = ct->coefficients_.size();
      int j = 0;
      for (CoeffEntry entry : ct->coefficients_) {
        const int var_index = entry.first->index();
        DCHECK(variable_is_extracted(var_index));
        vars[j] = scip_variables_[var_index];
        coefs[j] = entry.second;
        j++;
      }
      SCIP_CONS* scip_constraint = nullptr;
      const bool is_lazy = ct->is_lazy();
      // See
      // http://scip.zib.de/doc/html/cons__linear_8h.php#aa7aed137a4130b35b168812414413481
      // for an explanation of the parameters.
      ORTOOLS_SCIP_CALL(SCIPcreateConsLinear(
          scip_, &scip_constraint, ct->name().empty() ? "" : ct->name().c_str(),
          size, vars.get(), coefs.get(), ct->lb(), ct->ub(),
          !is_lazy,  // 'initial' parameter.
          true,      // 'separate' parameter.
          true,      // 'enforce' parameter.
          true,      // 'check' parameter.
          true,      // 'propagate' parameter.
          false,     // 'local' parameter.
          false,     // 'modifiable' parameter.
          false,     // 'dynamic' parameter.
          is_lazy,   // 'removable' parameter.
          false));   // 'stickingatnode' parameter.
      ORTOOLS_SCIP_CALL(SCIPaddCons(scip_, scip_constraint));
      scip_constraints_.push_back(scip_constraint);
    }
  }
}

void SCIPInterface::ExtractObjective() {
  ORTOOLS_SCIP_CALL(SCIPfreeTransform(scip_));
  // Linear objective: set objective coefficients for all variables (some might
  // have been modified).
  for (CoeffEntry entry : solver_->objective_->coefficients_) {
    const int var_index = entry.first->index();
    const double obj_coef = entry.second;
    ORTOOLS_SCIP_CALL(
        SCIPchgVarObj(scip_, scip_variables_[var_index], obj_coef));
  }

  // Constant term: change objective offset variable.
  ORTOOLS_SCIP_CALL(SCIPchgVarObj(scip_, objective_offset_variable_,
                                  solver_->Objective().offset()));
}

MPSolver::ResultStatus SCIPInterface::Solve(const MPSolverParameters& param) {
  WallTimer timer;
  timer.Start();

  // Note that SCIP does not provide any incrementality.
  if (param.GetIntegerParam(MPSolverParameters::INCREMENTALITY) ==
      MPSolverParameters::INCREMENTALITY_OFF) {
    Reset();
  }

  // Set log level.
  SCIPsetMessagehdlrQuiet(scip_, quiet_);

  // Special case if the model is empty since SCIP expects a non-empty model.
  if (solver_->variables_.empty() && solver_->constraints_.empty()) {
    sync_status_ = SOLUTION_SYNCHRONIZED;
    result_status_ = MPSolver::OPTIMAL;
    objective_value_ = solver_->Objective().offset();
    return result_status_;
  }

  ExtractModel();
  VLOG(1) << StringPrintf("Model built in %.3f seconds.", timer.Get());

  // Time limit.
  if (solver_->time_limit() != 0) {
    VLOG(1) << "Setting time limit = " << solver_->time_limit() << " ms.";
    ORTOOLS_SCIP_CALL(
        SCIPsetRealParam(scip_, "limits/time", solver_->time_limit_in_secs()));
  } else {
    ORTOOLS_SCIP_CALL(SCIPresetParam(scip_, "limits/time"));
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
      ORTOOLS_SCIP_CALL(SCIPcreatePartialSol(scip_, &solution, nullptr));
      is_solution_partial = true;
    } else {
      // We start by creating the all-zero solution.
      ORTOOLS_SCIP_CALL(SCIPcreateSol(scip_, &solution, nullptr));
    }

    // The variable representing the objective offset should always be one!!
    // See CreateSCIP().
    ORTOOLS_SCIP_CALL(
        SCIPsetSolVal(scip_, solution, objective_offset_variable_, 1.0));

    // Fill the other variables from the given solution hint.
    for (const std::pair<MPVariable*, double>& p : solver_->solution_hint_) {
      ORTOOLS_SCIP_CALL(SCIPsetSolVal(
          scip_, solution, scip_variables_[p.first->index()], p.second));
    }

    if (!is_solution_partial) {
      SCIP_Bool is_feasible;
      ORTOOLS_SCIP_CALL(SCIPcheckSol(
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
      ORTOOLS_SCIP_CALL(SCIPtrySolFree(
          scip_, &solution, /*printreason=*/false, /*completely=*/true,
          /*checkbounds=*/true, /*checkintegrality=*/true, /*checklprows=*/true,
          &is_stored));
    } else {
      ORTOOLS_SCIP_CALL(SCIPaddSolFree(scip_, &solution, &is_stored));
    }
  }

  // Solve.
  timer.Restart();
  if (SCIPsolve(scip_) != SCIP_OKAY) {
    result_status_ = MPSolver::ABNORMAL;
    return result_status_;
  }
  VLOG(1) << StringPrintf("Solved in %.3f seconds.", timer.Get());

  // Get the results.
  SCIP_SOL* const solution = SCIPgetBestSol(scip_);
  if (solution != nullptr) {
    // If optimal or feasible solution is found.
    objective_value_ = SCIPgetSolOrigObj(scip_, solution);
    VLOG(1) << "objective=" << objective_value_;
    for (int i = 0; i < solver_->variables_.size(); ++i) {
      MPVariable* const var = solver_->variables_[i];
      const int var_index = var->index();
      const double val =
          SCIPgetSolVal(scip_, solution, scip_variables_[var_index]);
      var->set_solution_value(val);
      VLOG(3) << var->name() << "=" << val;
    }
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
      } else if (scip_status == SCIP_STATUS_TIMELIMIT) {
        result_status_ = MPSolver::NOT_SOLVED;
      } else {
        result_status_ = MPSolver::ABNORMAL;
      }
      break;
  }

  ORTOOLS_SCIP_CALL(SCIPresetParams(scip_));

  sync_status_ = SOLUTION_SYNCHRONIZED;
  return result_status_;
}

int64 SCIPInterface::iterations() const {
  if (!CheckSolutionIsSynchronized()) return kUnknownNumberOfIterations;
  return SCIPgetNLPIterations(scip_);
}

int64 SCIPInterface::nodes() const {
  if (!CheckSolutionIsSynchronized()) return kUnknownNumberOfNodes;
  // TODO(user): or is it SCIPgetNTotalNodes?
  return SCIPgetNNodes(scip_);
}

double SCIPInterface::best_objective_bound() const {
  if (!CheckSolutionIsSynchronized() || !CheckBestObjectiveBoundExists()) {
    return trivial_worst_objective_bound();
  }
  if (solver_->variables_.empty() && solver_->constraints_.empty()) {
    // Special case for empty model.
    return solver_->Objective().offset();
  } else {
    return SCIPgetDualbound(scip_);
  }
}

void SCIPInterface::SetParameters(const MPSolverParameters& param) {
  SetCommonParameters(param);
  SetMIPParameters(param);
}

void SCIPInterface::SetRelativeMipGap(double value) {
  ORTOOLS_SCIP_CALL(SCIPsetRealParam(scip_, "limits/gap", value));
}

void SCIPInterface::SetPrimalTolerance(double value) {
  ORTOOLS_SCIP_CALL(SCIPsetRealParam(scip_, "numerics/feastol", value));
}

void SCIPInterface::SetDualTolerance(double value) {
  ORTOOLS_SCIP_CALL(SCIPsetRealParam(scip_, "numerics/dualfeastol", value));
}

void SCIPInterface::SetPresolveMode(int value) {
  switch (value) {
    case MPSolverParameters::PRESOLVE_OFF: {
      ORTOOLS_SCIP_CALL(SCIPsetIntParam(scip_, "presolving/maxrounds", 0));
      break;
    }
    case MPSolverParameters::PRESOLVE_ON: {
      ORTOOLS_SCIP_CALL(SCIPsetIntParam(scip_, "presolving/maxrounds", -1));
      break;
    }
    default: {
      SetIntegerParamToUnsupportedValue(MPSolverParameters::PRESOLVE, value);
    }
  }
}

void SCIPInterface::SetScalingMode(int value) {
  SetUnsupportedIntegerParam(MPSolverParameters::SCALING);
}

// Only the root LP algorithm is set as setting the node LP to a
// non-default value rarely is beneficial. The node LP algorithm could
// be set as well with "lp/resolvealgorithm".
void SCIPInterface::SetLpAlgorithm(int value) {
  switch (value) {
    case MPSolverParameters::DUAL: {
      ORTOOLS_SCIP_CALL(SCIPsetCharParam(scip_, "lp/initalgorithm", 'd'));
      break;
    }
    case MPSolverParameters::PRIMAL: {
      ORTOOLS_SCIP_CALL(SCIPsetCharParam(scip_, "lp/initalgorithm", 'p'));
      break;
    }
    case MPSolverParameters::BARRIER: {
      // Barrier with crossover.
      ORTOOLS_SCIP_CALL(SCIPsetCharParam(scip_, "lp/initalgorithm", 'p'));
      break;
    }
    default: {
      SetIntegerParamToUnsupportedValue(MPSolverParameters::LP_ALGORITHM,
                                        value);
    }
  }
}

bool SCIPInterface::ReadParameterFile(const std::string& filename) {
  return SCIPreadParams(scip_, filename.c_str()) == SCIP_OKAY;
}

std::string SCIPInterface::ValidFileExtensionForParameterFile() const {
  return ".set";
}

MPSolverInterface* BuildSCIPInterface(MPSolver* solver) {
  return new SCIPInterface(solver);
}


}  // namespace operations_research
#endif  //  #if defined(USE_SCIP)

#undef ORTOOLS_SCIP_CALL
