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
//

#include "base/hash.h"
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "base/timer.h"
#include "base/hash.h"
#include "linear_solver/linear_solver.h"

#if defined(USE_CBC)

#undef PACKAGE
#undef VERSION
#include "coin/CbcConfig.h"
#include "coin/CbcMessage.hpp"
#include "coin/CbcModel.hpp"
#include "coin/CoinModel.hpp"
#include "coin/OsiClpSolverInterface.hpp"

// Heuristics

DECLARE_double(solver_timeout_in_seconds);
DECLARE_string(solver_write_model);

namespace operations_research {

class CBCInterface : public MPSolverInterface {
 public:
  // Constructor that takes a name for the underlying glpk solver.
  explicit CBCInterface(MPSolver* const solver);
  virtual ~CBCInterface();

  // ----- Reset -----
  virtual void Reset();

  // Sets the optimization direction (min/max).
  virtual void SetOptimizationDirection(bool maximize);

  // ----- Solve -----
  // Solve the problem using the parameter values specified.
  virtual MPSolver::ResultStatus Solve(const MPSolverParameters& param);

  // TODO(user): separate the solve from the model extraction.
  virtual void ExtractModel() {}

  // Query problem type.
  virtual bool IsContinuous() const { return false; }
  virtual bool IsLP() const { return false; }
  virtual bool IsMIP() const { return true; }

  // Modify bounds.
  virtual void SetVariableBounds(int var_index, double lb, double ub);
  virtual void SetVariableInteger(int var_index, bool integer);
  virtual void SetConstraintBounds(int row_index, double lb, double ub);

  // Add constraint incrementally.
  void AddRowConstraint(MPConstraint* const ct);
  // Add variable incrementally.
  void AddVariable(MPVariable* const var);
  // Change a coefficient in a constraint.
  virtual void SetCoefficient(MPConstraint* const constraint,
                              const MPVariable* const variable,
                              double new_value,
                              double old_value) {
    sync_status_ = MUST_RELOAD;
  }
  // Clear a constraint from all its terms.
  virtual void ClearConstraint(MPConstraint* const constraint) {
    sync_status_ = MUST_RELOAD;
  }

  // Change a coefficient in the linear objective.
  virtual void SetObjectiveCoefficient(const MPVariable* const variable,
                                       double coefficient) {
    sync_status_ = MUST_RELOAD;
  }
  // Change the constant term in the linear objective.
  virtual void SetObjectiveOffset(double value) {
    sync_status_ = MUST_RELOAD;
  }
  // Clear the objective from all its terms.
  virtual void ClearObjective() {
    sync_status_ = MUST_RELOAD;
  }

  // Number of simplex iterations
  virtual int64 iterations() const;
  // Number of branch-and-bound nodes. Only available for discrete problems.
  virtual int64 nodes() const;
  // Best objective bound. Only available for discrete problems.
  virtual double best_objective_bound() const;

  // Returns the basis status of a row.
  virtual MPSolver::BasisStatus row_status(int constraint_index) const {
    LOG(FATAL) << "Basis status only available for continuous problems";
    return MPSolver::FREE;
  }
  // Returns the basis status of a column.
  virtual MPSolver::BasisStatus column_status(int variable_index) const {
    LOG(FATAL) << "Basis status only available for continuous problems";
    return MPSolver::FREE;
  }

  virtual void ExtractNewVariables() {}
  virtual void ExtractNewConstraints() {}
  virtual void ExtractObjective() {}

  virtual string SolverVersion() const {
    return "Cbc " CBC_VERSION;
  }

  // TODO(user): Maybe we should expose the CbcModel build from osi_
  // instead, but a new CbcModel is built every time Solve is called,
  // so it is not possible right now.
  virtual void* underlying_solver() {
    return reinterpret_cast<void*>(&osi_);
  }

 private:
  // Reset best objective bound to +/- infinity depending on the
  // optimization direction.
  void ResetBestObjectiveBound();

  // Set all parameters in the underlying solver.
  virtual void SetParameters(const MPSolverParameters& param);
  // Set each parameter in the underlying solver.
  virtual void SetRelativeMipGap(double value);
  virtual void SetPrimalTolerance(double value);
  virtual void SetDualTolerance(double value);
  virtual void SetPresolveMode(int value);
  virtual void SetScalingMode(int value);
  virtual void SetLpAlgorithm(int value);

  OsiClpSolverInterface osi_;
  // TODO(user): remove and query number of iterations directly from CbcModel
  int64 iterations_;
  int64 nodes_;
  double best_objective_bound_;
  // Special way to handle the relative MIP gap parameter.
  double relative_mip_gap_;
};

// ----- Solver -----

// Creates a LP/MIP instance with the specified name and minimization objective.
CBCInterface::CBCInterface(MPSolver* const solver)
    : MPSolverInterface(solver),
      iterations_(0), nodes_(0),
      best_objective_bound_(-std::numeric_limits<double>::infinity()),
      relative_mip_gap_(MPSolverParameters::kDefaultRelativeMipGap) {
  osi_.setStrParam(OsiProbName, solver_->name_);
  osi_.setObjSense(1);
}

CBCInterface::~CBCInterface() {}

// Reset the solver.
void CBCInterface::Reset() {
  osi_.reset();
  osi_.setObjSense(maximize_ ? -1 : 1);
  osi_.setStrParam(OsiProbName, solver_->name_);
  ResetExtractionInformation();
}

void CBCInterface::ResetBestObjectiveBound() {
  if (maximize_) {
    best_objective_bound_ = std::numeric_limits<double>::infinity();
  } else {
    best_objective_bound_ = -std::numeric_limits<double>::infinity();
  }
}

void CBCInterface::SetOptimizationDirection(bool maximize) {
  InvalidateSolutionSynchronization();
  if (sync_status_ == MODEL_SYNCHRONIZED) {
    osi_.setObjSense(maximize ? -1 : 1);
  } else {
    sync_status_ = MUST_RELOAD;
  }
}

void CBCInterface::SetVariableBounds(int var_index, double lb, double ub) {
  InvalidateSolutionSynchronization();
  if (sync_status_ == MODEL_SYNCHRONIZED) {
    osi_.setColBounds(var_index, lb, ub);
  } else {
    sync_status_ = MUST_RELOAD;
  }
}

void CBCInterface::SetVariableInteger(int var_index, bool integer) {
  InvalidateSolutionSynchronization();
  // TODO(user) : Check if this is actually a change.
  if (sync_status_ == MODEL_SYNCHRONIZED) {
    if (integer) {
      osi_.setInteger(var_index);
    } else {
      osi_.setContinuous(var_index);
    }
  } else {
    sync_status_ = MUST_RELOAD;
  }
}

void CBCInterface::SetConstraintBounds(int index, double lb, double ub) {
  InvalidateSolutionSynchronization();
  if (sync_status_ == MODEL_SYNCHRONIZED) {
    osi_.setRowBounds(index, lb, ub);
  } else {
    sync_status_ = MUST_RELOAD;
  }
}

void CBCInterface::AddRowConstraint(MPConstraint* const ct) {
  sync_status_ = MUST_RELOAD;
}

void CBCInterface::AddVariable(MPVariable* const var) {
  sync_status_ = MUST_RELOAD;
}

// Solve the LP/MIP. Returns true only if the optimal solution was revealed.
// Returns the status of the search.
MPSolver::ResultStatus CBCInterface::Solve(const MPSolverParameters& param) {
  WallTimer timer;
  timer.Start();

  // Note that CBC does not provide any incrementality.
  if (param.GetIntegerParam(MPSolverParameters::INCREMENTALITY) ==
      MPSolverParameters::INCREMENTALITY_OFF) {
    Reset();
  }

  // Special case if the model is empty since CBC is not able to
  // handle this special case by itself.
  if (solver_->variables_.size() == 0 && solver_->constraints_.size() == 0) {
    sync_status_ = SOLUTION_SYNCHRONIZED;
    result_status_ = MPSolver::OPTIMAL;
    objective_value_ = solver_->Objective().offset();
    best_objective_bound_ = solver_->Objective().offset();
    return result_status_;
  }

  // Finish preparing the problem.
  // Define variables.
  switch (sync_status_) {
    case MUST_RELOAD: {
      Reset();
      CoinModel build;
      // Create dummy variable for objective offset.
      build.addColumn(0, NULL, NULL, 1.0, 1.0,
                      solver_->Objective().offset(), "dummy", false);
      const int nb_vars = solver_->variables_.size();
      for (int i = 0; i < nb_vars; ++i) {
        MPVariable* const var = solver_->variables_[i];
        var->set_index(i + 1);  // offset by 1 because of dummy variable.
        const double obj_coeff = solver_->Objective().GetCoefficient(var);
        if (var->name().empty()) {
          build.addColumn(0, NULL, NULL, var->lb(), var->ub(), obj_coeff,
                          NULL, var->integer());
        } else {
          build.addColumn(0, NULL, NULL, var->lb(), var->ub(), obj_coeff,
                          var->name().c_str(), var->integer());
        }
      }

      // Define constraints.
      int max_row_length = 0;
      int constraint_index = 0;
      for (int i = 0; i < solver_->constraints_.size(); ++i) {
        MPConstraint* const ct = solver_->constraints_[i];
        ct->set_index(constraint_index++);
        if (ct->coefficients_.size() > max_row_length) {
          max_row_length = ct->coefficients_.size();
        }
      }
      scoped_ptr<int[]> indices(new int[max_row_length]);
      scoped_ptr<double[]> coefs(new double[max_row_length]);

      for (int i = 0; i < solver_->constraints_.size(); ++i) {
        MPConstraint* const  ct = solver_->constraints_[i];
        const int size = ct->coefficients_.size();
        int j = 0;
        for (CoeffEntry entry : ct->coefficients_) {
          const int index = entry.first->index();
          DCHECK_NE(kNoIndex, index);
          indices[j] = index;
          coefs[j] = entry.second;
          j++;
        }
        if (ct->name().empty()) {
          build.addRow(size, indices.get(), coefs.get(), ct->lb(), ct->ub());
        } else {
          build.addRow(size, indices.get(), coefs.get(), ct->lb(), ct->ub(),
                       ct->name().c_str());
        }
      }
      osi_.loadFromCoinModel(build);
      break;
    }
    case MODEL_SYNCHRONIZED: {
      break;
    }
    case SOLUTION_SYNCHRONIZED: {
      break;
    }
  }

  // Changing optimization direction through OSI so that the model file
  // (written through OSI) has the correct optimization duration.
  osi_.setObjSense(maximize_ ? -1 : 1);

  sync_status_ = MODEL_SYNCHRONIZED;
  VLOG(1) << StringPrintf("Model built in %.3f seconds.", timer.Get());

  ResetBestObjectiveBound();

  // Solve
  CbcModel model(osi_);

  // Set log level.
  CoinMessageHandler message_handler;
  model.passInMessageHandler(&message_handler);
  if (quiet_) {
    message_handler.setLogLevel(0, 0);  // Coin messages
    message_handler.setLogLevel(1, 0);  // Clp messages
    message_handler.setLogLevel(2, 0);  // Presolve messages
    message_handler.setLogLevel(3, 0);  // Cgl messages
  } else {
    message_handler.setLogLevel(0, 1);  // Coin messages
    message_handler.setLogLevel(1, 0);  // Clp messages
    message_handler.setLogLevel(2, 0);  // Presolve messages
    message_handler.setLogLevel(3, 1);  // Cgl messages
  }

  // Time limit.
  if (solver_->time_limit() != 0) {
    VLOG(1) << "Setting time limit = " << solver_->time_limit() << " ms.";
    model.setMaximumSeconds(solver_->time_limit_in_secs());
  }

  // And solve.
  timer.Restart();

  // Here we use the default function from the command-line CBC solver.
  // This enables to activate all the features and get the same performance
  // as the CBC stand-alone executable. The syntax is ugly, however.
  SetParameters(param);
  // Always turn presolve on (it's the CBC default and it consistently
  // improves performance).
  model.setTypePresolve(0);
  // Special way to set the relative MIP gap parameter as it cannot be set
  // through callCbc.
  model.setAllowableFractionGap(relative_mip_gap_);
  int return_status = callCbc("-solve", model);
  const int kBadReturnStatus = 777;
  CHECK_NE(kBadReturnStatus, return_status);  // Should never happen according
                                              // to the CBC source

  VLOG(1) << StringPrintf("Solved in %.3f seconds.", timer.Get());

  // Check the status: optimal, infeasible, etc.
  int tmp_status = model.status();

  VLOG(1) << "cbc result status: " << tmp_status;
  /* Final status of problem
     (info from cbc/.../CbcSolver.cpp,
      See http://cs?q="cbc+status"+file:CbcSolver.cpp)
     Some of these can be found out by is...... functions
     -1 before branchAndBound
     0 finished - check isProvenOptimal or isProvenInfeasible to see
     if solution found
     (or check value of best solution)
     1 stopped - on maxnodes, maxsols, maxtime
     2 difficulties so run was abandoned
     (5 event user programmed event occurred)
  */
  switch (tmp_status) {
    case 0:
      // Order of tests counts; if model.isContinuousUnbounded() returns true,
      // then so does model.isProvenInfeasible()!
      if (model.isProvenOptimal()) {
        result_status_ = MPSolver::OPTIMAL;
      } else if (model.isContinuousUnbounded()) {
        result_status_ = MPSolver::UNBOUNDED;
      } else if (model.isProvenInfeasible()) {
        result_status_ = MPSolver::INFEASIBLE;
      } else {
        LOG(FATAL)
            << "Unknown solver status! Secondary status: "
            << model.secondaryStatus();
      }
      break;
    case 1:
      result_status_ = MPSolver::FEASIBLE;
      break;
    default:
      result_status_ = MPSolver::ABNORMAL;
      break;
  }

  if (result_status_ == MPSolver::OPTIMAL ||
      result_status_ == MPSolver::FEASIBLE) {
    // Get the results
    objective_value_ = model.getObjValue();
    VLOG(1) << "objective=" << objective_value_;
    const double* const values = model.bestSolution();
    if (values != NULL) {
      // if optimal or feasible solution is found.
      for (int i = 0; i < solver_->variables_.size(); ++i) {
        MPVariable* const var = solver_->variables_[i];
        const int var_index = var->index();
        const double val = values[var_index];
        var->set_solution_value(val);
        VLOG(3) << var->name() << "=" << val;
      }
    } else {
      VLOG(1) << "No feasible solution found.";
    }

    const double* const row_activities = model.getRowActivity();
    if (row_activities != NULL) {
      for (int i = 0; i < solver_->constraints_.size(); ++i) {
        MPConstraint* const ct = solver_->constraints_[i];
        const int constraint_index = ct->index();
        const double row_activity = row_activities[constraint_index];
        ct->set_activity(row_activity);
        VLOG(4) << "row " << ct->index()
                << ": activity = " << row_activity;
      }
    }
  }

  iterations_ = model.getIterationCount();
  nodes_ = model.getNodeCount();
  best_objective_bound_ = model.getBestPossibleObjValue();
  VLOG(1) << "best objective bound=" << best_objective_bound_;

  sync_status_ = SOLUTION_SYNCHRONIZED;

  return result_status_;
}

// ------ Query statistics on the solution and the solve ------

int64 CBCInterface::iterations() const {
  if (!CheckSolutionIsSynchronized()) return kUnknownNumberOfNodes;
  return iterations_;
}

int64 CBCInterface::nodes() const {
  if (!CheckSolutionIsSynchronized()) return kUnknownNumberOfIterations;
  return nodes_;
}

double CBCInterface::best_objective_bound() const {
  if (!CheckSolutionIsSynchronized() || !CheckBestObjectiveBoundExists()) {
    return trivial_worst_objective_bound();
  }
  return best_objective_bound_;
}

// ----- Parameters -----

// The support for parameters in CBC is intentionally sparse. There is
// a memory leak in callCbc that prevents to pass parameters through
// it, so handling parameters would require an comprehensive rewrite
// of the code. I will improve the parameter support only if there is
// a relevant use case.

void CBCInterface::SetParameters(const MPSolverParameters& param) {
  SetCommonParameters(param);
  SetMIPParameters(param);
}

void CBCInterface::SetRelativeMipGap(double value) {
  relative_mip_gap_ = value;
}

void CBCInterface::SetPrimalTolerance(double value) {
  // Skip the warning for the default value as it coincides with
  // the default value in CBC.
  if (value != MPSolverParameters::kDefaultPrimalTolerance) {
    SetUnsupportedDoubleParam(MPSolverParameters::PRIMAL_TOLERANCE);
  }
}

void CBCInterface::SetDualTolerance(double value) {
  // Skip the warning for the default value as it coincides with
  // the default value in CBC.
  if (value != MPSolverParameters::kDefaultDualTolerance) {
    SetUnsupportedDoubleParam(MPSolverParameters::DUAL_TOLERANCE);
  }
}

void CBCInterface::SetPresolveMode(int value) {
  switch (value) {
    case MPSolverParameters::PRESOLVE_ON: {
      // CBC presolve is always on.
      break;
    }
    default: {
      SetUnsupportedIntegerParam(MPSolverParameters::PRESOLVE);
    }
  }
}

void CBCInterface::SetScalingMode(int value) {
  SetUnsupportedIntegerParam(MPSolverParameters::SCALING);
}

void CBCInterface::SetLpAlgorithm(int value) {
  SetUnsupportedIntegerParam(MPSolverParameters::LP_ALGORITHM);
}

MPSolverInterface* BuildCBCInterface(MPSolver* const solver) {
  return new CBCInterface(solver);
}


}  // namespace operations_research
#endif  // #if defined(USE_CBC)
