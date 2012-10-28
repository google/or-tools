// Copyright 2010-2012 Google
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

#include <math.h>
#include <stddef.h>
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
#include "base/concise_iterator.h"
#include "base/hash.h"
#include "base/map-util.h"
#include "linear_solver/linear_solver.h"

#if defined(USE_GRB)
extern "C" {
#include "gurobi_c.h"
}

namespace operations_research {

class GRBInterface : public MPSolverInterface {
 public:
  // Constructor that takes a name for the underlying GRB solver.
  explicit GRBInterface(MPSolver* const solver, bool mip);
  ~GRBInterface();

  // Sets the optimization direction (min/max).
  virtual void SetOptimizationDirection(bool maximize);

  // ----- Solve -----
  // Solve the problem using the parameter values specified.
  virtual MPSolver::ResultStatus Solve(const MPSolverParameters& param);

  // ----- Model modifications and extraction -----
  // Resets extracted model
  virtual void Reset();

  // Modify bounds.
  virtual void SetVariableBounds(int var_index, double lb, double ub);
  virtual void SetVariableInteger(int var_index, bool integer);
  virtual void SetConstraintBounds(int row_index, double lb, double ub);

  // Add Constraint incrementally.
  void AddRowConstraint(MPConstraint* const ct);
  // Add variable incrementally.
  void AddVariable(MPVariable* const var);
  // Change a coefficient in a constraint.
  virtual void SetCoefficient(MPConstraint* const constraint,
                              const MPVariable* const variable,
                              double new_value,
                              double old_value);
  // Clear a constraint from all its terms.
  virtual void ClearConstraint(MPConstraint* const constraint);
  // Change a coefficient in the linear objective
  virtual void SetObjectiveCoefficient(const MPVariable* const variable,
                                       double coefficient);
  // Change the constant term in the linear objective.
  virtual void SetObjectiveOffset(double value);
  // Clear the objective from all its terms.
  virtual void ClearObjective();

  // ------ Query statistics on the solution and the solve ------
  // Number of simplex iterations
  virtual int64 iterations() const;
  // Number of branch-and-bound nodes. Only available for discrete problems.
  virtual int64 nodes() const;
  // Best objective bound. Only available for discrete problems.
  virtual double best_objective_bound() const;

  // Returns the basis status of a row.
  virtual MPSolver::BasisStatus row_status(int constraint_index) const;
  // Returns the basis status of a column.
  virtual MPSolver::BasisStatus column_status(int variable_index) const;

  // ----- Misc -----
  // Write model
  virtual void WriteModel(const string& filename);

  // Query problem type.
  virtual bool IsContinuous() const { return IsLP(); }
  virtual bool IsLP() const { return !mip_; }
  virtual bool IsMIP() const { return mip_; }

  virtual void ExtractNewVariables();
  virtual void ExtractNewConstraints();
  virtual void ExtractObjective();

  virtual string SolverVersion() const {
    int major, minor, technical;
    GRBversion(&major, &minor, &technical);
    return StringPrintf("Gurobi library version %d.%d.%d\n",
                        major,
                        minor,
                        technical);
  }

  virtual void* underlying_solver() {
    return reinterpret_cast<void*>(model_);
  }

  virtual double ComputeExactConditionNumber() const {
    // TODO(user): Implement me.
    LOG(FATAL) << "Condition number only available for continuous problems";
    return 0.0;
  }

 private:
  // Set all parameters in the underlying solver.
  virtual void SetParameters(const MPSolverParameters& param);
  // Set each parameter in the underlying solver.
  virtual void SetRelativeMipGap(double value);
  virtual void SetPrimalTolerance(double value);
  virtual void SetDualTolerance(double value);
  virtual void SetPresolveMode(int value);
  virtual void SetScalingMode(int value);
  virtual void SetLpAlgorithm(int value);

  void ExtractOneConstraint(MPConstraint* const constraint);

  MPSolver::BasisStatus TransformGRBBasisStatus(int gurobi_basis_status) const;

 private:
  GRBmodel* model_;
  GRBenv* env_;
  bool mip_;
  int _nbVars;
  int _nbCnts;
};

// Creates a LP/MIP instance with the specified name and minimization objective.
GRBInterface::GRBInterface(MPSolver* const solver, bool mip)
    : MPSolverInterface(solver)
    , env_(0)
    , model_(0)
    , _nbVars(0)
    , _nbCnts(0)
    , mip_(mip) {
  int status = GRBloadenv(&env_, "model_interface.log");;
  if (status ||  env_ == NULL) {
    fprintf(stderr, "Error: could not create environment\n");
    exit(1);
  }
  status = GRBnewmodel(env_,
                       &model_,
                       solver_->name_.c_str(),
                       0,
                       NULL,
                       NULL,
                       NULL,
                       NULL,
                       NULL);
  CHECK_EQ(status, 0);
  status = GRBsetintattr(model_, "ModelSense", maximize_ ? -1 : 1);
  DCHECK_EQ(0, status);
}

GRBInterface::~GRBInterface() {
  int status;
  status = GRBfreemodel(model_);
  DCHECK_EQ(0, status);
  GRBfreeenv(env_);
}

void GRBInterface::WriteModel(const string& filename) {
  const int status = GRBwrite(model_, filename.c_str());
  DCHECK_EQ(0, status);
}

// ------ Model modifications and extraction -----

void GRBInterface::Reset() {
  int status;
  status = GRBfreemodel(model_);
  DCHECK_EQ(status, 0);
  status = GRBnewmodel(env_,
                      &model_,
                      solver_->name_.c_str(),
                      0,
                      NULL,
                      NULL,
                      NULL,
                      NULL,
                      NULL);
  CHECK_EQ(status, 0);
  status = GRBsetintattr(model_, "ModelSense", maximize_ ? -1 : 1);
  DCHECK_EQ(0, status);
  ResetExtractionInformation();
}

void GRBInterface::SetOptimizationDirection(bool maximize) {
  InvalidateSolutionSynchronization();
  if (sync_status_ = MODEL_SYNCHRONIZED) {
    const int status = GRBsetintattr(model_, "ModelSense", maximize_ ? -1 : 1);
    DCHECK_EQ(0, status);
  }
  else {
    sync_status_ = MUST_RELOAD;
  }
}

void GRBInterface::SetVariableBounds(int var_index, double lb, double ub) {
  InvalidateSolutionSynchronization();
  int status;
  if (sync_status_ == MODEL_SYNCHRONIZED) {
    if (lb > -solver_->infinity()) {
      status = GRBsetdblattrelement(model_, GRB_DBL_ATTR_LB, var_index, lb);
      DCHECK_EQ(0, status);
    }
    if (ub < solver_->infinity()) {
      status = GRBsetdblattrelement(model_, GRB_DBL_ATTR_UB, var_index, ub);
      DCHECK_EQ(0, status);
    }
  } else {
    sync_status_ = MUST_RELOAD;
  }
}

// Modifies integrality of an extracted variable.
void GRBInterface::SetVariableInteger(int index, bool integer) {
  int status;
  char current_type;
  status = GRBgetcharattrelement(model_,
                                 GRB_CHAR_ATTR_VTYPE,
                                 index,
                                 &current_type);
  DCHECK_EQ(0, status);

  if ((integer && current_type == 'I')
      || (!integer && current_type == 'C'))
    return;

  InvalidateSolutionSynchronization();
  if (sync_status_ == MODEL_SYNCHRONIZED) {
    char type_var;
    if (integer) {
      type_var = 'I';
    } else {
      type_var = 'C';
    }
    status = GRBsetcharattrelement(model_,
                                   GRB_CHAR_ATTR_VTYPE,
                                   index,
                                   type_var);
    DCHECK_EQ(0, status);
  } else {
    sync_status_ = MUST_RELOAD;
  }
}

void GRBInterface::SetConstraintBounds(int index, double lb, double ub) {
  sync_status_ = MUST_RELOAD;
}

void GRBInterface::AddRowConstraint(MPConstraint* const ct) {
  sync_status_ = MUST_RELOAD;
}

void GRBInterface::AddVariable(MPVariable* const ct) {
  sync_status_ = MUST_RELOAD;
}

void GRBInterface::SetCoefficient(MPConstraint* const constraint,
                                  const MPVariable* const variable,
                                  double new_value,
                                  double old_value) {
  sync_status_ = MUST_RELOAD;
}

void GRBInterface::ClearConstraint(MPConstraint* const constraint) {
  sync_status_ = MUST_RELOAD;
}

void GRBInterface::SetObjectiveCoefficient(const MPVariable* const variable,
                                           double coefficient) {
  sync_status_ = MUST_RELOAD;
}

void GRBInterface::SetObjectiveOffset(double value) {
  sync_status_ = MUST_RELOAD;
}

void GRBInterface::ClearObjective() {
  sync_status_ = MUST_RELOAD;
}

MPSolverInterface* BuildGRBInterface(MPSolver* const solver, bool mip) {
  return new GRBInterface(solver, mip);
}

// ------ Query statistics on the solution and the solve ------

int64 GRBInterface::iterations() const {
  double iter;
  CheckSolutionIsSynchronized();
  const int status = GRBgetdblattr(model_, GRB_DBL_ATTR_ITERCOUNT, &iter);
  DCHECK_EQ(0, status);
  return static_cast<int64>(iter);
}

int64 GRBInterface::nodes() const {
  if (mip_) {
    CheckSolutionIsSynchronized();
    double nodes = 0;
    const int status = GRBgetdblattr(model_, GRB_DBL_ATTR_NODECOUNT, &nodes);
    DCHECK_EQ(0, status);
    return static_cast<int64>(nodes);
  } else {
    LOG(FATAL) << "Number of nodes only available for discrete problems";
    return kUnknownNumberOfNodes;
  }
}

// Returns the best objective bound. Only available for discrete problems.
double GRBInterface::best_objective_bound() const {
  if (mip_) {
    CheckSolutionIsSynchronized();
    CheckBestObjectiveBoundExists();
    if (solver_->variables_.size() == 0 && solver_->constraints_.size() == 0) {
      // Special case for empty model.
      return solver_->Objective().offset();
    } else {
      int status;
      double value;
      status = GRBgetdblattr(model_, GRB_DBL_ATTR_OBJBOUND, &value);
      DCHECK_EQ(0, status);
      return value;
    }
  } else {
    LOG(FATAL) << "Best objective bound only available for discrete problems";
    return 0.0;
  }

}

MPSolver::BasisStatus
GRBInterface::TransformGRBBasisStatus(int gurobi_basis_status) const {
  switch (gurobi_basis_status) {
    case GRB_NONBASIC_LOWER:
      return MPSolver::AT_LOWER_BOUND;
    case GRB_BASIC:
      return MPSolver::BASIC;
    case GRB_NONBASIC_UPPER:
      return MPSolver::AT_UPPER_BOUND;
    case GRB_SUPERBASIC:
      return MPSolver::FREE;
    default:
      LOG(FATAL) << "Unknown GRB basis status";
      return MPSolver::FREE;
  }
}

// Returns the basis status of a row.
MPSolver::BasisStatus GRBInterface::row_status(int constraint_index) const {
  int optim_status = 0;
  int status = GRBgetintattr(model_, GRB_INT_ATTR_STATUS, &optim_status);
  if (optim_status == GRB_LOADED ||
      optim_status == GRB_INFEASIBLE ||
      optim_status == GRB_INF_OR_UNBD ||
      optim_status == GRB_UNBOUNDED ||
      mip_) {
    // FIXME: Are these the only cases?
    LOG(FATAL) << "Basis status only available for continuous problems";
    return MPSolver::FREE;
  }
  int gurobi_basis_status = 0;
  status = GRBgetintattrelement(model_,
                                GRB_INT_ATTR_CBASIS,
                                constraint_index,
                                &gurobi_basis_status);
  DCHECK_EQ(0, status);
  return TransformGRBBasisStatus(gurobi_basis_status);
}

// Returns the basis status of a column.
MPSolver::BasisStatus GRBInterface::column_status(int variable_index) const {
  int optim_status = 0;
  int status = GRBgetintattr(model_, GRB_INT_ATTR_STATUS, &optim_status);
  if (optim_status == GRB_LOADED ||
      optim_status == GRB_INFEASIBLE ||
      optim_status == GRB_INF_OR_UNBD ||
      optim_status == GRB_UNBOUNDED ||
      mip_) {
    // FIXME: Are these the only cases?
    LOG(FATAL) << "Basis status only available for continuous problems";
    return MPSolver::FREE;
  }
  int gurobi_basis_status = 0;
  status = GRBgetintattrelement(model_,
                                GRB_INT_ATTR_VBASIS,
                                variable_index,
                                &gurobi_basis_status);
  DCHECK_EQ(0, status);
  return TransformGRBBasisStatus(gurobi_basis_status);
}

// status the variables that have not been extracted yet
void GRBInterface::ExtractNewVariables() {
  CHECK_EQ(0, last_variable_index_);
  int status = 0;
  int total_num_vars = solver_->variables_.size();
  if (total_num_vars > last_variable_index_) {
    int nb_new_variables = total_num_vars - last_variable_index_;
    scoped_array<double> obj_coefs(new double[nb_new_variables]);
    scoped_array<double> lb(new double[nb_new_variables]);
    scoped_array<double> ub(new double[nb_new_variables]);
    scoped_array<char> ctype(new char[nb_new_variables]);
    scoped_array<const char*> colname(new const char*[nb_new_variables]);

    for (int j = 0; j < nb_new_variables; ++j) {
      MPVariable* const var = solver_->variables_[last_variable_index_+j];
      var->set_index(last_variable_index_ + j);
      lb[j] = var->lb();
      ub[j] = var->ub();
      ctype.get()[j] = var->integer() && mip_ ? 'I' : 'C';
      if (!var->name().empty()) {
	colname[j] = var->name().c_str();
      }
      obj_coefs[j] = FindWithDefault(solver_->objective_->coefficients_,
                                     var,
                                     0.0);
    }

    status = GRBaddvars(model_,
                        nb_new_variables,
                        0,
                        NULL,
                        NULL,
                        NULL,
                        obj_coefs.get(),
                        lb.get(),
                        ub.get(),
                        ctype.get(),
                        const_cast<char**>(colname.get()));
    DCHECK_EQ(0, status);
  }
  GRBupdatemodel(model_);
}

void GRBInterface::ExtractNewConstraints() {
  int status = 0;
  int total_num_rows = solver_->constraints_.size();
  CHECK_EQ(last_variable_index_, 0);  // always from scratch.
  if (last_constraint_index_ < total_num_rows) {
    // Find the length of the longest row.
    int max_row_length = 0;
    for (int i = last_constraint_index_; i < total_num_rows; ++i) {
      MPConstraint* const ct = solver_->constraints_[i];
      DCHECK_EQ(kNoIndex, ct->index());
      ct->set_index(i);
      if (ct->coefficients_.size() > max_row_length) {
        max_row_length = ct->coefficients_.size();
      }
    }

    int addrows = total_num_rows - last_constraint_index_;

    max_row_length = std::max(1, max_row_length);
    scoped_array<int> col_indices(new int[max_row_length]);
    scoped_array<double> coefs(new double[max_row_length]);

    // Add each new constraint.
    for (int i = last_constraint_index_; i < total_num_rows; ++i) {
      MPConstraint* const ct = solver_->constraints_[i];
      DCHECK_NE(kNoIndex, ct->index());
      const int size = ct->coefficients_.size();
      int j = 0;
      for (ConstIter<hash_map<const MPVariable*, double> > it(
               ct->coefficients_);
           !it.at_end(); ++it) {
        const int index = it->first->index();
        DCHECK_NE(kNoIndex, index);
        col_indices[j] = index;
        coefs[j] = it->second;
        j++;
      }
      char* const name =
          ct->name().empty() ? NULL : const_cast<char*>(ct->name().c_str());
      status = GRBaddrangeconstr(model_,
                                 size,
                                 col_indices.get(),
                                 coefs.get(),
                                 ct->lb(),
                                 ct->ub(),
                                 name);
      CHECK_EQ(0, status) << GRBgeterrormsg(env_);
    }
  }
  status = GRBupdatemodel(model_);
  DCHECK_EQ(status, 0);
}

void GRBInterface::ExtractObjective() {
  int status = GRBupdatemodel(model_);
  DCHECK_EQ(status, 0);
}

// ------ Parameters  -----

void GRBInterface::SetParameters(const MPSolverParameters& param) {
  SetCommonParameters(param);
  if (mip_) {
    SetMIPParameters(param);
  }
}

void GRBInterface::SetRelativeMipGap(double value) {
  if (mip_) {
    int status;
    status = GRBsetdblattr (model_, GRB_DBL_PAR_MIPGAP, value);
    DCHECK_EQ(0, status);
  } else {
    LOG(WARNING) << "The relative MIP gap is only available "
                 << "for discrete problems.";
  }
}

void GRBInterface::SetPrimalTolerance(double value) {
  // FIXME
}

void GRBInterface::SetDualTolerance(double value) {
  // FIXME
}

void GRBInterface::SetPresolveMode(int value) {
  // FIXME
  SetIntegerParamToUnsupportedValue(MPSolverParameters::PRESOLVE, value);
}

// Sets the scaling mode.
void GRBInterface::SetScalingMode(int value) {
  // FIXME
  SetUnsupportedIntegerParam(MPSolverParameters::SCALING);
}

// Sets the LP algorithm : primal, dual or barrier. Note that GRB offers other LP algorithm (e.g. network) and automatic selection
void GRBInterface::SetLpAlgorithm(int value) {
  int status;
  switch (value) {
    case MPSolverParameters::DUAL:
      status = GRBsetintparam(env_, GRB_INT_PAR_METHOD, GRB_METHOD_DUAL);
      break;
    case MPSolverParameters::PRIMAL:
      status = GRBsetintparam(env_, GRB_INT_PAR_METHOD, GRB_METHOD_PRIMAL);
      break;
    case MPSolverParameters::BARRIER:
      status = GRBsetintparam(env_, GRB_INT_PAR_METHOD, GRB_METHOD_BARRIER);
      break;
    default:
      SetIntegerParamToUnsupportedValue(MPSolverParameters::LP_ALGORITHM,
                                        value);
  }
}


MPSolver::ResultStatus GRBInterface::Solve(const MPSolverParameters& param) {
  int status = 0;

  WallTimer timer;
  timer.Start();

  if (param.GetIntegerParam(MPSolverParameters::INCREMENTALITY) ==
      MPSolverParameters::INCREMENTALITY_OFF) {
    Reset();
  }

  // Set log level.
  if (quiet_) {
    status = GRBsetintparam(GRBgetenv(model_), GRB_INT_PAR_OUTPUTFLAG, 0);
    DCHECK_EQ(0, status);
  } else {
    status = GRBsetintparam(GRBgetenv(model_), GRB_INT_PAR_OUTPUTFLAG, 1);
    DCHECK_EQ(0, status);
  }

  ExtractModel();
  VLOG(1) << StringPrintf("Model build in %.3f seconds.", timer.Get());

  WriteModelToPredefinedFiles();

  status = GRBwrite(model_, "test.lp");
  DCHECK_EQ(0, status);

  // Time limit.
  if (solver_->time_limit()) {
    VLOG(1) << "Setting time limit = " << solver_->time_limit() << " ms.";
    status = GRBsetdblparam (env_,
                             GRB_DBL_PAR_TIMELIMIT,
                             solver_->time_limit() / 1000.0);
    DCHECK_EQ(0, status);
  }

  // Solve
  timer.Restart();
  status = GRBoptimize(model_);

  if (status) {
    VLOG(1) << "Failed to optimize MIP." << GRBgeterrormsg(env_);
  } else {
    VLOG(1) << StringPrintf("Solved in %.3f seconds.", timer.Get());
  }

  // Get the results.
  int total_num_rows = solver_->constraints_.size();
  int total_num_cols = solver_->variables_.size();
  scoped_array<double> values(new double[total_num_cols]);
  scoped_array<double> dual_values(new double[total_num_rows]);
  scoped_array<double> slacks(new double[total_num_rows]);
  scoped_array<double> reduced_costs(new double[total_num_cols]);
  int optimization_status = 0;
  status = GRBgetintattr(model_, GRB_INT_ATTR_STATUS, &optimization_status);
  if (optimization_status == GRB_OPTIMAL) {
    // TODO(user): Improve me, get feasible solution.
    status = GRBgetdblattr(model_, GRB_DBL_ATTR_OBJVAL, &objective_value_);
    DCHECK_EQ(0, status);
    status = GRBgetdblattrarray(model_,
                                GRB_DBL_ATTR_X,
                                0,
                                total_num_cols,
                                values.get());
    DCHECK_EQ(0, status);
    status = GRBgetdblattrarray(model_,
                                GRB_DBL_ATTR_SLACK,
                                0,
                                total_num_rows,
                                slacks.get());
    DCHECK_EQ(0, status);
    if (!mip_) {
      status = GRBgetdblattrarray(model_,
                                  GRB_DBL_ATTR_RC,
                                  0,
                                  total_num_cols,
                                  reduced_costs.get());
      DCHECK_EQ(0, status);
      status = GRBgetdblattrarray(model_,
                                  GRB_DBL_ATTR_PI,
                                  0,
                                  total_num_rows,
                                  dual_values.get());
      DCHECK_EQ(0, status);
    }
  }

  bool solution_found = false;
  if (optimization_status == GRB_OPTIMAL) {
    VLOG(1) << "objective = " << objective_value_;
    solution_found = true;
    int total_num_vars = solver_->variables_.size();
    for (int i=0; i < solver_->variables_.size(); ++i) {
      MPVariable* const var = solver_->variables_[i];
      var->set_solution_value(values[i]);
      VLOG(3) << var->name() << ": value =" << values[i];
      if (!mip_) {
	var->set_reduced_cost(reduced_costs[i]);
	VLOG(4) << var->name() << ": reduced cost = " << reduced_costs[i];
      }
    }
    for (int i = 0; i < solver_->constraints_.size(); ++i) {
      MPConstraint* const ct = solver_->constraints_[i];
      double activity = 0;
      if (ct->lb() > -solver_->infinity() && ct->ub() < solver_->infinity()) {
	// don't know what to use : lb or ub?
	activity = ct->ub() - slacks[i];
      }
      else if (ct->lb() > -solver_->infinity()) {
	activity = ct->lb() + slacks[i];
      }
      else if (ct->ub() < solver_->infinity()) {
	activity = ct->ub() - slacks[i];
      }
      ct->set_activity(activity);
      if (mip_) {
	VLOG(4) << "row " << ct->index()
		<< ": activity = " << slacks[i];
      } else {
	ct->set_dual_value(dual_values[i]);
	VLOG(4) << "row " << ct->index()
		<< ": activity = " << slacks[i]
		<< ": dual value = " << dual_values[i];
      }
    }
  }

  VLOG(1) << StringPrintf("Solution status %d.\n", optimization_status);

  switch (optimization_status) {
    case GRB_OPTIMAL:
      result_status_ = MPSolver::OPTIMAL;
      break;
    case GRB_INFEASIBLE:
      result_status_ = MPSolver::INFEASIBLE;
      break;
    case GRB_UNBOUNDED:
      result_status_ = MPSolver::UNBOUNDED;
      break;
    case GRB_INF_OR_UNBD:
      // TODO(user): We could introduce our own "infeasible or
      // unbounded" status.
      result_status_ = MPSolver::INFEASIBLE;
      break;
    default: {
      if (solution_found) {
        result_status_ = MPSolver::FEASIBLE;
      } else {
        // TODO(user): We could introduce additional values for the
        // status: for example, stopped because of time limit.
        result_status_ = MPSolver::ABNORMAL;
      }
      break;
    }
  }

  sync_status_ = SOLUTION_SYNCHRONIZED;
  return result_status_;
}
}  // namespace operations_research
#endif  //  #if defined(USE_GRB)
