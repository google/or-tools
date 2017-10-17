// Copyright 2010-2017 Google
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

//

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
#include "ortools/base/strutil.h"
#include "ortools/base/hash.h"
#include "ortools/linear_solver/linear_solver.h"

#if defined(USE_CLP) || defined(USE_CBC)

#undef PACKAGE
#undef VERSION
#include "ClpConfig.h"
#include "ClpMessage.hpp"
#include "ClpSimplex.hpp"
#include "CoinBuild.hpp"

namespace operations_research {

class CLPInterface : public MPSolverInterface {
 public:
  // Constructor that takes a name for the underlying CLP solver.
  explicit CLPInterface(MPSolver* const solver);
  ~CLPInterface() override;

  // Sets the optimization direction (min/max).
  void SetOptimizationDirection(bool maximize) override;

  // ----- Solve -----
  // Solve the problem using the parameter values specified.
  MPSolver::ResultStatus Solve(const MPSolverParameters& param) override;

  // ----- Model modifications and extraction -----
  // Resets extracted model
  void Reset() override;

  // Modify bounds.
  void SetVariableBounds(int var_index, double lb, double ub) override;
  void SetVariableInteger(int var_index, bool integer) override;
  void SetConstraintBounds(int row_index, double lb, double ub) override;

  // Add constraint incrementally.
  void AddRowConstraint(MPConstraint* const ct) override;
  // Add variable incrementally.
  void AddVariable(MPVariable* const var) override;
  // Change a coefficient in a constraint.
  void SetCoefficient(MPConstraint* const constraint,
                      const MPVariable* const variable, double new_value,
                      double old_value) override;
  // Clear a constraint from all its terms.
  void ClearConstraint(MPConstraint* const constraint) override;

  // Change a coefficient in the linear objective.
  void SetObjectiveCoefficient(const MPVariable* const variable,
                               double coefficient) override;
  // Change the constant term in the linear objective.
  void SetObjectiveOffset(double value) override;
  // Clear the objective from all its terms.
  void ClearObjective() override;

  // ------ Query statistics on the solution and the solve ------
  // Number of simplex iterations
  int64 iterations() const override;
  // Number of branch-and-bound nodes. Only available for discrete problems.
  int64 nodes() const override;
  // Best objective bound. Only available for discrete problems.
  double best_objective_bound() const override;

  // Returns the basis status of a row.
  MPSolver::BasisStatus row_status(int constraint_index) const override;
  // Returns the basis status of a column.
  MPSolver::BasisStatus column_status(int variable_index) const override;

  // ----- Misc -----
  // Query problem type.
  bool IsContinuous() const override { return true; }
  bool IsLP() const override { return true; }
  bool IsMIP() const override { return false; }

  void ExtractNewVariables() override;
  void ExtractNewConstraints() override;
  void ExtractObjective() override;

  std::string SolverVersion() const override { return "Clp " CLP_VERSION; }

  void* underlying_solver() override {
    return reinterpret_cast<void*>(clp_.get());
  }

 private:
  // Create dummy variable to be able to create empty constraints.
  void CreateDummyVariableForEmptyConstraints();

  // Set all parameters in the underlying solver.
  void SetParameters(const MPSolverParameters& param) override;
  // Reset to their default value the parameters for which CLP has a
  // stateful API. To be called after the solve so that the next solve
  // starts from a clean parameter state.
  void ResetParameters();
  // Set each parameter in the underlying solver.
  void SetRelativeMipGap(double value) override;
  void SetPrimalTolerance(double value) override;
  void SetDualTolerance(double value) override;
  void SetPresolveMode(int value) override;
  void SetScalingMode(int value) override;
  void SetLpAlgorithm(int value) override;

  // Transforms basis status from CLP enum to MPSolver::BasisStatus.
  MPSolver::BasisStatus TransformCLPBasisStatus(
      ClpSimplex::Status clp_basis_status) const;

  std::unique_ptr<ClpSimplex> clp_;    // TODO(user) : remove pointer.
  std::unique_ptr<ClpSolve> options_;  // For parameter setting.
};

// ----- Solver -----

// Creates a LP/MIP instance with the specified name and minimization objective.
CLPInterface::CLPInterface(MPSolver* const solver)
    : MPSolverInterface(solver), clp_(new ClpSimplex), options_(new ClpSolve) {
  clp_->setStrParam(ClpProbName, solver_->name_);
  clp_->setOptimizationDirection(1);
}

CLPInterface::~CLPInterface() {}

void CLPInterface::Reset() {
  clp_.reset(new ClpSimplex);
  clp_->setOptimizationDirection(maximize_ ? -1 : 1);
  ResetExtractionInformation();
}

// ------ Model modifications and extraction -----

namespace {
// Variable indices are shifted by 1 internally because of the dummy "objective
// offset" variable (with internal index 0).
int MPSolverVarIndexToClpVarIndex(int var_index) { return var_index + 1; }
}  // namespace

// Not cached
void CLPInterface::SetOptimizationDirection(bool maximize) {
  InvalidateSolutionSynchronization();
  clp_->setOptimizationDirection(maximize ? -1 : 1);
}

void CLPInterface::SetVariableBounds(int var_index, double lb, double ub) {
  InvalidateSolutionSynchronization();
  if (variable_is_extracted(var_index)) {
    // Not cached if the variable has been extracted
    DCHECK_LT(var_index, last_variable_index_);
    clp_->setColumnBounds(MPSolverVarIndexToClpVarIndex(var_index), lb, ub);
  } else {
    sync_status_ = MUST_RELOAD;
  }
}

// Ignore as CLP does not solve models with integer variables
void CLPInterface::SetVariableInteger(int var_index, bool integer) {}

void CLPInterface::SetConstraintBounds(int index, double lb, double ub) {
  InvalidateSolutionSynchronization();
  if (constraint_is_extracted(index)) {
    // Not cached if the row has been extracted
    DCHECK_LT(index, last_constraint_index_);
    clp_->setRowBounds(index, lb, ub);
  } else {
    sync_status_ = MUST_RELOAD;
  }
}

void CLPInterface::SetCoefficient(MPConstraint* const constraint,
                                  const MPVariable* const variable,
                                  double new_value, double old_value) {
  InvalidateSolutionSynchronization();
  if (constraint_is_extracted(constraint->index()) &&
      variable_is_extracted(variable->index())) {
    // The modification of the coefficient for an extracted row and
    // variable is not cached.
    DCHECK_LE(constraint->index(), last_constraint_index_);
    DCHECK_LE(variable->index(), last_variable_index_);
    clp_->modifyCoefficient(constraint->index(),
                            MPSolverVarIndexToClpVarIndex(variable->index()),
                            new_value);
  } else {
    // The modification of an unextracted row or variable is cached
    // and handled in ExtractModel.
    sync_status_ = MUST_RELOAD;
  }
}

// Not cached
void CLPInterface::ClearConstraint(MPConstraint* const constraint) {
  InvalidateSolutionSynchronization();
  // Constraint may not have been extracted yet.
  if (!constraint_is_extracted(constraint->index())) return;
  for (CoeffEntry entry : constraint->coefficients_) {
    DCHECK(variable_is_extracted(entry.first->index()));
    clp_->modifyCoefficient(constraint->index(),
                            MPSolverVarIndexToClpVarIndex(entry.first->index()),
                            0.0);
  }
}

// Cached
void CLPInterface::SetObjectiveCoefficient(const MPVariable* const variable,
                                           double coefficient) {
  InvalidateSolutionSynchronization();
  if (variable_is_extracted(variable->index())) {
    clp_->setObjectiveCoefficient(
        MPSolverVarIndexToClpVarIndex(variable->index()), coefficient);
  } else {
    sync_status_ = MUST_RELOAD;
  }
}

// Cached
void CLPInterface::SetObjectiveOffset(double offset) {
  // Constant term. Use -offset instead of +offset because CLP does
  // not follow conventions.
  InvalidateSolutionSynchronization();
  clp_->setObjectiveOffset(-offset);
}

// Clear objective of all its terms.
void CLPInterface::ClearObjective() {
  InvalidateSolutionSynchronization();
  // Clear linear terms
  for (CoeffEntry entry : solver_->objective_->coefficients_) {
    const int mpsolver_var_index = entry.first->index();
    // Variable may have not been extracted yet.
    if (!variable_is_extracted(mpsolver_var_index)) {
      DCHECK_NE(MODEL_SYNCHRONIZED, sync_status_);
    } else {
      clp_->setObjectiveCoefficient(
          MPSolverVarIndexToClpVarIndex(mpsolver_var_index), 0.0);
    }
  }
  // Clear constant term.
  clp_->setObjectiveOffset(0.0);
}

void CLPInterface::AddRowConstraint(MPConstraint* const ct) {
  sync_status_ = MUST_RELOAD;
}

void CLPInterface::AddVariable(MPVariable* const var) {
  sync_status_ = MUST_RELOAD;
}

void CLPInterface::CreateDummyVariableForEmptyConstraints() {
  clp_->setColumnBounds(kDummyVariableIndex, 0.0, 0.0);
  clp_->setObjectiveCoefficient(kDummyVariableIndex, 0.0);
  // Workaround for peculiar signature of setColumnName. Note that we do need
  // std::string here, and not 'std::string', which aren't the same as of 2013-12
  // (this will change later).
  std::string dummy = "dummy";  // We do need to create this temporary variable.
  clp_->setColumnName(kDummyVariableIndex, dummy);
}

// Define new variables and add them to existing constraints.
void CLPInterface::ExtractNewVariables() {
  // Define new variables
  int total_num_vars = solver_->variables_.size();
  if (total_num_vars > last_variable_index_) {
    if (last_variable_index_ == 0 && last_constraint_index_ == 0) {
      // Faster extraction when nothing has been extracted yet.
      clp_->resize(0, total_num_vars + 1);
      CreateDummyVariableForEmptyConstraints();
      for (int i = 0; i < total_num_vars; ++i) {
        MPVariable* const var = solver_->variables_[i];
        set_variable_as_extracted(i, true);
        if (!var->name().empty()) {
          std::string name = var->name();
          clp_->setColumnName(MPSolverVarIndexToClpVarIndex(i), name);
        }
        clp_->setColumnBounds(MPSolverVarIndexToClpVarIndex(i), var->lb(),
                              var->ub());
      }
    } else {
      // TODO(user): This could perhaps be made slightly faster by
      // iterating through old constraints, constructing by hand the
      // column-major representation of the addition to them and call
      // clp_->addColumns. But this is good enough for now.
      // Create new variables.
      for (int j = last_variable_index_; j < total_num_vars; ++j) {
        MPVariable* const var = solver_->variables_[j];
        DCHECK(!variable_is_extracted(j));
        set_variable_as_extracted(j, true);
        // The true objective coefficient will be set later in ExtractObjective.
        double tmp_obj_coef = 0.0;
        clp_->addColumn(0, nullptr, nullptr, var->lb(), var->ub(),
                        tmp_obj_coef);
        if (!var->name().empty()) {
          std::string name = var->name();
          clp_->setColumnName(MPSolverVarIndexToClpVarIndex(j), name);
        }
      }
      // Add new variables to existing constraints.
      for (int i = 0; i < last_constraint_index_; i++) {
        MPConstraint* const ct = solver_->constraints_[i];
        const int ct_index = ct->index();
        for (CoeffEntry entry : ct->coefficients_) {
          const int mpsolver_var_index = entry.first->index();
          DCHECK(variable_is_extracted(mpsolver_var_index));
          if (mpsolver_var_index >= last_variable_index_) {
            clp_->modifyCoefficient(
                ct_index, MPSolverVarIndexToClpVarIndex(mpsolver_var_index),
                entry.second);
          }
        }
      }
    }
  }
}

// Define new constraints on old and new variables.
void CLPInterface::ExtractNewConstraints() {
  int total_num_rows = solver_->constraints_.size();
  if (last_constraint_index_ < total_num_rows) {
    // Find the length of the longest row.
    int max_row_length = 0;
    for (int i = last_constraint_index_; i < total_num_rows; ++i) {
      MPConstraint* const ct = solver_->constraints_[i];
      DCHECK(!constraint_is_extracted(ct->index()));
      set_constraint_as_extracted(ct->index(), true);
      if (ct->coefficients_.size() > max_row_length) {
        max_row_length = ct->coefficients_.size();
      }
    }
    // Make space for dummy variable.
    max_row_length = std::max(1, max_row_length);
    std::unique_ptr<int[]> indices(new int[max_row_length]);
    std::unique_ptr<double[]> coefs(new double[max_row_length]);
    CoinBuild build_object;
    // Add each new constraint.
    for (int i = last_constraint_index_; i < total_num_rows; ++i) {
      MPConstraint* const ct = solver_->constraints_[i];
      DCHECK(constraint_is_extracted(ct->index()));
      int size = ct->coefficients_.size();
      if (size == 0) {
        // Add dummy variable to be able to build the constraint.
        indices[0] = kDummyVariableIndex;
        coefs[0] = 1.0;
        size = 1;
      }
      int j = 0;
      for (CoeffEntry entry : ct->coefficients_) {
        const int mpsolver_var_index = entry.first->index();
        DCHECK(variable_is_extracted(mpsolver_var_index));
        indices[j] = MPSolverVarIndexToClpVarIndex(mpsolver_var_index);
        coefs[j] = entry.second;
        j++;
      }
      build_object.addRow(size, indices.get(), coefs.get(), ct->lb(), ct->ub());
    }
    // Add and name the rows.
    clp_->addRows(build_object);
    for (int i = last_constraint_index_; i < total_num_rows; ++i) {
      MPConstraint* const ct = solver_->constraints_[i];
      if (!ct->name().empty()) {
        std::string name = ct->name();
        clp_->setRowName(ct->index(), name);
      }
    }
  }
}

void CLPInterface::ExtractObjective() {
  // Linear objective: set objective coefficients for all variables
  // (some might have been modified)
  for (CoeffEntry entry : solver_->objective_->coefficients_) {
    clp_->setObjectiveCoefficient(
        MPSolverVarIndexToClpVarIndex(entry.first->index()), entry.second);
  }

  // Constant term. Use -offset instead of +offset because CLP does
  // not follow conventions.
  clp_->setObjectiveOffset(-solver_->Objective().offset());
}

// Extracts model and solve the LP/MIP. Returns the status of the search.
MPSolver::ResultStatus CLPInterface::Solve(const MPSolverParameters& param) {
  try {
    WallTimer timer;
    timer.Start();

    if (param.GetIntegerParam(MPSolverParameters::INCREMENTALITY) ==
        MPSolverParameters::INCREMENTALITY_OFF) {
      Reset();
    }

    // Set log level.
    CoinMessageHandler message_handler;
    clp_->passInMessageHandler(&message_handler);
    if (quiet_) {
      message_handler.setLogLevel(1, 0);
      clp_->setLogLevel(0);
    } else {
      message_handler.setLogLevel(1, 1);
      clp_->setLogLevel(1);
    }

    // Special case if the model is empty since CLP is not able to
    // handle this special case by itself.
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
      clp_->setMaximumSeconds(solver_->time_limit_in_secs());
    } else {
      clp_->setMaximumSeconds(-1.0);
    }

    // Start from a fresh set of default parameters and set them to
    // specified values.
    options_.reset(new ClpSolve);
    SetParameters(param);

    // Solve
    timer.Restart();
    clp_->initialSolve(*options_);
    VLOG(1) << StringPrintf("Solved in %.3f seconds.", timer.Get());

    // Check the status: optimal, infeasible, etc.
    int tmp_status = clp_->status();
    VLOG(1) << "clp result status: " << tmp_status;
    switch (tmp_status) {
      case CLP_SIMPLEX_FINISHED:
        result_status_ = MPSolver::OPTIMAL;
        break;
      case CLP_SIMPLEX_INFEASIBLE:
        result_status_ = MPSolver::INFEASIBLE;
        break;
      case CLP_SIMPLEX_UNBOUNDED:
        result_status_ = MPSolver::UNBOUNDED;
        break;
      case CLP_SIMPLEX_STOPPED:
        result_status_ = MPSolver::FEASIBLE;
        break;
      default:
        result_status_ = MPSolver::ABNORMAL;
        break;
    }

    if (result_status_ == MPSolver::OPTIMAL ||
        result_status_ == MPSolver::FEASIBLE) {
      // Get the results
      objective_value_ = clp_->objectiveValue();
      VLOG(1) << "objective=" << objective_value_;
      const double* const values = clp_->getColSolution();
      const double* const reduced_costs = clp_->getReducedCost();
      for (int i = 0; i < solver_->variables_.size(); ++i) {
        MPVariable* const var = solver_->variables_[i];
        const int clp_var_index = MPSolverVarIndexToClpVarIndex(var->index());
        const double val = values[clp_var_index];
        var->set_solution_value(val);
        VLOG(3) << var->name() << ": value = " << val;
        double reduced_cost = reduced_costs[clp_var_index];
        var->set_reduced_cost(reduced_cost);
        VLOG(4) << var->name() << ": reduced cost = " << reduced_cost;
      }
      const double* const dual_values = clp_->getRowPrice();
      for (int i = 0; i < solver_->constraints_.size(); ++i) {
        MPConstraint* const ct = solver_->constraints_[i];
        const int constraint_index = ct->index();
        const double dual_value = dual_values[constraint_index];
        ct->set_dual_value(dual_value);
        VLOG(4) << "row " << ct->index() << " dual value = " << dual_value;
      }
    }

    ResetParameters();
    sync_status_ = SOLUTION_SYNCHRONIZED;
    return result_status_;
  } catch (CoinError e) {
    LOG(WARNING) << "Caught exception in Coin LP: " << e.message();
    result_status_ = MPSolver::ABNORMAL;
    return result_status_;
  }
}

MPSolver::BasisStatus CLPInterface::TransformCLPBasisStatus(
    ClpSimplex::Status clp_basis_status) const {
  switch (clp_basis_status) {
    case ClpSimplex::isFree:
      return MPSolver::FREE;
    case ClpSimplex::basic:
      return MPSolver::BASIC;
    case ClpSimplex::atUpperBound:
      return MPSolver::AT_UPPER_BOUND;
    case ClpSimplex::atLowerBound:
      return MPSolver::AT_LOWER_BOUND;
    case ClpSimplex::superBasic:
      return MPSolver::FREE;
    case ClpSimplex::isFixed:
      return MPSolver::FIXED_VALUE;
    default:
      LOG(FATAL) << "Unknown CLP basis status";
      return MPSolver::FREE;
  }
}

// ------ Query statistics on the solution and the solve ------

int64 CLPInterface::iterations() const {
  if (!CheckSolutionIsSynchronized()) return kUnknownNumberOfIterations;
  return clp_->getIterationCount();
}

int64 CLPInterface::nodes() const {
  LOG(DFATAL) << "Number of nodes only available for discrete problems";
  return kUnknownNumberOfNodes;
}

double CLPInterface::best_objective_bound() const {
  LOG(DFATAL) << "Best objective bound only available for discrete problems";
  return trivial_worst_objective_bound();
}

MPSolver::BasisStatus CLPInterface::row_status(int constraint_index) const {
  DCHECK_LE(0, constraint_index);
  DCHECK_GT(last_constraint_index_, constraint_index);
  const ClpSimplex::Status clp_basis_status =
      clp_->getRowStatus(constraint_index);
  return TransformCLPBasisStatus(clp_basis_status);
}

MPSolver::BasisStatus CLPInterface::column_status(int variable_index) const {
  DCHECK_LE(0, variable_index);
  DCHECK_GT(last_variable_index_, variable_index);
  const ClpSimplex::Status clp_basis_status =
      clp_->getColumnStatus(MPSolverVarIndexToClpVarIndex(variable_index));
  return TransformCLPBasisStatus(clp_basis_status);
}

// ------ Parameters ------

void CLPInterface::SetParameters(const MPSolverParameters& param) {
  SetCommonParameters(param);
}

void CLPInterface::ResetParameters() {
  clp_->setPrimalTolerance(MPSolverParameters::kDefaultPrimalTolerance);
  clp_->setDualTolerance(MPSolverParameters::kDefaultDualTolerance);
}

void CLPInterface::SetRelativeMipGap(double value) {
  LOG(WARNING) << "The relative MIP gap is only available "
               << "for discrete problems.";
}

void CLPInterface::SetPrimalTolerance(double value) {
  clp_->setPrimalTolerance(value);
}

void CLPInterface::SetDualTolerance(double value) {
  clp_->setDualTolerance(value);
}

void CLPInterface::SetPresolveMode(int value) {
  switch (value) {
    case MPSolverParameters::PRESOLVE_OFF: {
      options_->setPresolveType(ClpSolve::presolveOff);
      break;
    }
    case MPSolverParameters::PRESOLVE_ON: {
      options_->setPresolveType(ClpSolve::presolveOn);
      break;
    }
    default: {
      SetIntegerParamToUnsupportedValue(MPSolverParameters::PRESOLVE, value);
    }
  }
}

void CLPInterface::SetScalingMode(int value) {
  SetUnsupportedIntegerParam(MPSolverParameters::SCALING);
}

void CLPInterface::SetLpAlgorithm(int value) {
  switch (value) {
    case MPSolverParameters::DUAL: {
      options_->setSolveType(ClpSolve::useDual);
      break;
    }
    case MPSolverParameters::PRIMAL: {
      options_->setSolveType(ClpSolve::usePrimal);
      break;
    }
    case MPSolverParameters::BARRIER: {
      options_->setSolveType(ClpSolve::useBarrier);
      break;
    }
    default: {
      SetIntegerParamToUnsupportedValue(MPSolverParameters::LP_ALGORITHM,
                                        value);
    }
  }
}

MPSolverInterface* BuildCLPInterface(MPSolver* const solver) {
  return new CLPInterface(solver);
}


}  // namespace operations_research
#endif  // #if defined(USE_CBC) || defined(USE_CLP)
