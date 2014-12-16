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

#include <cmath>
#include <cstddef>
#include "base/hash.h"
#include <limits>
#include "base/unique_ptr.h"
#include <string>
#include <utility>
#include <vector>

#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/timer.h"
#include "base/map_util.h"
#include "linear_solver/linear_solver.h"


#if defined(USE_GUROBI)
extern "C" {
#include "gurobi_c.h"
}

#define CHECKED_GUROBI_CALL(x) CHECK_EQ(0, x)

DEFINE_int32(num_gurobi_threads, 4, "Number of threads available for Gurobi.");

namespace operations_research {

class GurobiInterface : public MPSolverInterface {
 public:
  // Constructor that takes a name for the underlying GRB solver.
  explicit GurobiInterface(MPSolver* const solver, bool mip);
  ~GurobiInterface();

  // Sets the optimization direction (min/max).
  virtual void SetOptimizationDirection(bool maximize);

  // ----- Solve -----
  // Solves the problem using the parameter values specified.
  virtual MPSolver::ResultStatus Solve(const MPSolverParameters& param);

  // ----- Model modifications and extraction -----
  // Resets extracted model
  virtual void Reset();

  // Modifies bounds.
  virtual void SetVariableBounds(int var_index, double lb, double ub);
  virtual void SetVariableInteger(int var_index, bool integer);
  virtual void SetConstraintBounds(int row_index, double lb, double ub);

  // Adds Constraint incrementally.
  void AddRowConstraint(MPConstraint* const ct);
  // Adds variable incrementally.
  void AddVariable(MPVariable* const var);
  // Changes a coefficient in a constraint.
  virtual void SetCoefficient(MPConstraint* const constraint,
                              const MPVariable* const variable,
                              double new_value, double old_value);
  // Clears a constraint from all its terms.
  virtual void ClearConstraint(MPConstraint* const constraint);
  // Changes a coefficient in the linear objective
  virtual void SetObjectiveCoefficient(const MPVariable* const variable,
                                       double coefficient);
  // Changes the constant term in the linear objective.
  virtual void SetObjectiveOffset(double value);
  // Clears the objective from all its terms.
  virtual void ClearObjective();

  // ------ Query statistics on the solution and the solve ------
  // Number of simplex or interior-point iterations
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
  // Queries problem type.
  virtual bool IsContinuous() const { return IsLP(); }
  virtual bool IsLP() const { return !mip_; }
  virtual bool IsMIP() const { return mip_; }

  virtual void ExtractNewVariables();
  virtual void ExtractNewConstraints();
  virtual void ExtractObjective();

  virtual std::string SolverVersion() const {
    int major, minor, technical;
    GRBversion(&major, &minor, &technical);
    return StringPrintf("Gurobi library version %d.%d.%d\n", major, minor,
                        technical);
  }

  virtual void* underlying_solver() { return reinterpret_cast<void*>(model_); }

  virtual double ComputeExactConditionNumber() const {
    if (!IsContinuous()) {
      LOG(DFATAL) << "ComputeExactConditionNumber not implemented for"
                  << " GUROBI_MIXED_INTEGER_PROGRAMMING";
      return 0.0;
    }

    // TODO(user,user): Not yet working.
    LOG(DFATAL) << "ComputeExactConditionNumber not implemented for"
                << " GUROBI_LINEAR_PROGRAMMING";
    return 0.0;

    // double cond = 0.0;
    // const int status = GRBgetdblattr(model_, GRB_DBL_ATTR_KAPPA, &cond);
    // if (0 == status) {
    //   return cond;
    // } else {
    //   LOG(DFATAL) << "Condition number only available for "
    //               << "continuous problems";
    //   return 0.0;
    // }
  }

 private:
  // Sets all parameters in the underlying solver.
  virtual void SetParameters(const MPSolverParameters& param);
  // Sets each parameter in the underlying solver.
  virtual void SetRelativeMipGap(double value);
  virtual void SetPrimalTolerance(double value);
  virtual void SetDualTolerance(double value);
  virtual void SetPresolveMode(int value);
  virtual void SetScalingMode(int value);
  virtual void SetLpAlgorithm(int value);

  virtual bool ReadParameterFile(const std::string& filename);
  virtual std::string ValidFileExtensionForParameterFile() const;

  MPSolver::BasisStatus TransformGRBVarBasisStatus(int gurobi_basis_status)
      const;
  MPSolver::BasisStatus TransformGRBConstraintBasisStatus(
      int gurobi_basis_status, int constraint_index) const;

 private:
  GRBmodel* model_;
  GRBenv* env_;
  bool mip_;
};

// Creates a LP/MIP instance with the specified name and minimization objective.
GurobiInterface::GurobiInterface(MPSolver* const solver, bool mip)
    : MPSolverInterface(solver), model_(0), env_(0), mip_(mip) {
  if (GRBloadenv(&env_, NULL) != 0 || env_ == NULL) {
    LOG(FATAL) << "Error: could not create environment";
  }

  CHECKED_GUROBI_CALL(GRBnewmodel(env_, &model_, solver_->name_.c_str(),
                                  0,       // numvars
                                  NULL,    // obj
                                  NULL,    // lb
                                  NULL,    // ub
                                  NULL,    // vtype
                                  NULL));  // varnanes
  CHECKED_GUROBI_CALL(
      GRBsetintattr(model_, GRB_INT_ATTR_MODELSENSE, maximize_ ? -1 : 1));

  CHECKED_GUROBI_CALL(
      GRBsetintparam(env_, GRB_INT_PAR_THREADS, FLAGS_num_gurobi_threads));
}

GurobiInterface::~GurobiInterface() {
  CHECKED_GUROBI_CALL(GRBfreemodel(model_));
  GRBfreeenv(env_);
}

// ------ Model modifications and extraction -----

void GurobiInterface::Reset() {
  CHECKED_GUROBI_CALL(GRBfreemodel(model_));
  CHECKED_GUROBI_CALL(GRBnewmodel(env_, &model_, solver_->name_.c_str(),
                                  0,       // numvars
                                  NULL,    // obj
                                  NULL,    // lb
                                  NULL,    // ub
                                  NULL,    // vtype
                                  NULL));  // varnames
  ResetExtractionInformation();
}

void GurobiInterface::SetOptimizationDirection(bool maximize) {
  sync_status_ = MUST_RELOAD;
  // TODO(user,user): Fix, not yet working.
  // InvalidateSolutionSynchronization();
  // CHECKED_GUROBI_CALL(GRBsetintattr(model_,
  //                                   GRB_INT_ATTR_MODELSENSE,
  //                                   maximize_ ? -1 : 1));
}

void GurobiInterface::SetVariableBounds(int var_index, double lb, double ub) {
  sync_status_ = MUST_RELOAD;
}

// Modifies integrality of an extracted variable.
void GurobiInterface::SetVariableInteger(int index, bool integer) {
  char current_type;
  CHECKED_GUROBI_CALL(
      GRBgetcharattrelement(model_, GRB_CHAR_ATTR_VTYPE, index, &current_type));

  if ((integer &&
       (current_type == GRB_INTEGER || current_type == GRB_BINARY)) ||
      (!integer && current_type == GRB_CONTINUOUS))
    return;

  InvalidateSolutionSynchronization();
  if (sync_status_ == MODEL_SYNCHRONIZED) {
    char type_var;
    if (integer) {
      type_var = GRB_INTEGER;
    } else {
      type_var = GRB_CONTINUOUS;
    }
    CHECKED_GUROBI_CALL(
        GRBsetcharattrelement(model_, GRB_CHAR_ATTR_VTYPE, index, type_var));
  } else {
    sync_status_ = MUST_RELOAD;
  }
}

void GurobiInterface::SetConstraintBounds(int index, double lb, double ub) {
  sync_status_ = MUST_RELOAD;
}

void GurobiInterface::AddRowConstraint(MPConstraint* const ct) {
  sync_status_ = MUST_RELOAD;
}

void GurobiInterface::AddVariable(MPVariable* const ct) {
  sync_status_ = MUST_RELOAD;
}

void GurobiInterface::SetCoefficient(MPConstraint* const constraint,
                                     const MPVariable* const variable,
                                     double new_value, double old_value) {
  sync_status_ = MUST_RELOAD;
}

void GurobiInterface::ClearConstraint(MPConstraint* const constraint) {
  sync_status_ = MUST_RELOAD;
}

void GurobiInterface::SetObjectiveCoefficient(const MPVariable* const variable,
                                              double coefficient) {
  sync_status_ = MUST_RELOAD;
}

void GurobiInterface::SetObjectiveOffset(double value) {
  sync_status_ = MUST_RELOAD;
  // TODO(user,user): make it work.
  // InvalidateSolutionSynchronization();
  // CHECKED_GUROBI_CALL(GRBsetdblattr(model_,
  //                                   GRB_DBL_ATTR_OBJCON,
  //                                   solver_->Objective().offset()));
  // CHECKED_GUROBI_CALL(GRBupdatemodel(model_));
}

void GurobiInterface::ClearObjective() { sync_status_ = MUST_RELOAD; }

// ------ Query statistics on the solution and the solve ------

int64 GurobiInterface::iterations() const {
  double iter;
  if (!CheckSolutionIsSynchronized()) return kUnknownNumberOfIterations;
  CHECKED_GUROBI_CALL(GRBgetdblattr(model_, GRB_DBL_ATTR_ITERCOUNT, &iter));
  return static_cast<int64>(iter);
}

int64 GurobiInterface::nodes() const {
  if (mip_) {
    if (!CheckSolutionIsSynchronized()) return kUnknownNumberOfNodes;
    double nodes = 0;
    CHECKED_GUROBI_CALL(GRBgetdblattr(model_, GRB_DBL_ATTR_NODECOUNT, &nodes));
    return static_cast<int64>(nodes);
  } else {
    LOG(DFATAL) << "Number of nodes only available for discrete problems.";
    return kUnknownNumberOfNodes;
  }
}

// Returns the best objective bound. Only available for discrete problems.
double GurobiInterface::best_objective_bound() const {
  if (mip_) {
    if (!CheckSolutionIsSynchronized() || !CheckBestObjectiveBoundExists()) {
      return trivial_worst_objective_bound();
    }
    if (solver_->variables_.size() == 0 && solver_->constraints_.size() == 0) {
      // Special case for empty model.
      return solver_->Objective().offset();
    } else {
      double value;
      CHECKED_GUROBI_CALL(GRBgetdblattr(model_, GRB_DBL_ATTR_OBJBOUND, &value));
      return value;
    }
  } else {
    LOG(DFATAL) << "Best objective bound only available for discrete problems.";
    return trivial_worst_objective_bound();
  }
}

MPSolver::BasisStatus GurobiInterface::TransformGRBVarBasisStatus(
    int gurobi_basis_status) const {
  switch (gurobi_basis_status) {
    case GRB_BASIC:
      return MPSolver::BASIC;
    case GRB_NONBASIC_LOWER:
      return MPSolver::AT_LOWER_BOUND;
    case GRB_NONBASIC_UPPER:
      return MPSolver::AT_UPPER_BOUND;
    case GRB_SUPERBASIC:
      return MPSolver::FREE;
    default:
      LOG(DFATAL) << "Unknown GRB basis status.";
      return MPSolver::FREE;
  }
}

MPSolver::BasisStatus GurobiInterface::TransformGRBConstraintBasisStatus(
    int gurobi_basis_status, int constraint_index) const {
  switch (gurobi_basis_status) {
    case GRB_BASIC:
      return MPSolver::BASIC;
    default: {
      // Non basic.
      double slack = 0.0;
      double tolerance = 0.0;
      CHECKED_GUROBI_CALL(GRBgetdblparam(
          GRBgetenv(model_), GRB_DBL_PAR_FEASIBILITYTOL, &tolerance));
      CHECKED_GUROBI_CALL(GRBgetdblattrelement(model_, GRB_DBL_ATTR_SLACK,
                                               constraint_index, &slack));
      char sense;
      CHECKED_GUROBI_CALL(GRBgetcharattrelement(model_, GRB_CHAR_ATTR_SENSE,
                                                constraint_index, &sense));
      VLOG(4) << "constraint " << constraint_index << " , slack = " << slack
              << " , sense = " << sense;
      if (fabs(slack) <= tolerance) {
        switch (sense) {
          case GRB_EQUAL:
          case GRB_LESS_EQUAL:
            return MPSolver::AT_UPPER_BOUND;
          case GRB_GREATER_EQUAL:
            return MPSolver::AT_LOWER_BOUND;
          default:
            return MPSolver::FREE;
        }
      } else {
        return MPSolver::FREE;
      }
    }
  }
}

// Returns the basis status of a row.
MPSolver::BasisStatus GurobiInterface::row_status(int constraint_index) const {
  int optim_status = 0;
  CHECKED_GUROBI_CALL(
      GRBgetintattr(model_, GRB_INT_ATTR_STATUS, &optim_status));
  if (optim_status != GRB_OPTIMAL && optim_status != GRB_SUBOPTIMAL) {
    LOG(DFATAL) << "Basis status only available after a solution has "
                << "been found.";
    return MPSolver::FREE;
  }
  if (mip_) {
    LOG(DFATAL) << "Basis status only available for continuous problems.";
    return MPSolver::FREE;
  }
  int gurobi_basis_status = 0;
  CHECKED_GUROBI_CALL(GRBgetintattrelement(
      model_, GRB_INT_ATTR_CBASIS, constraint_index, &gurobi_basis_status));
  return TransformGRBConstraintBasisStatus(gurobi_basis_status,
                                           constraint_index);
}

// Returns the basis status of a column.
MPSolver::BasisStatus GurobiInterface::column_status(int variable_index) const {
  int optim_status = 0;
  CHECKED_GUROBI_CALL(
      GRBgetintattr(model_, GRB_INT_ATTR_STATUS, &optim_status));
  if (optim_status != GRB_OPTIMAL && optim_status != GRB_SUBOPTIMAL) {
    LOG(DFATAL) << "Basis status only available after a solution has "
                << "been found.";
    return MPSolver::FREE;
  }
  if (mip_) {
    LOG(DFATAL) << "Basis status only available for continuous problems.";
    return MPSolver::FREE;
  }
  int gurobi_basis_status = 0;
  CHECKED_GUROBI_CALL(GRBgetintattrelement(
      model_, GRB_INT_ATTR_VBASIS, variable_index, &gurobi_basis_status));
  return TransformGRBVarBasisStatus(gurobi_basis_status);
}

// Extracts new variables.
void GurobiInterface::ExtractNewVariables() {
  CHECK(last_variable_index_ == 0 ||
        last_variable_index_ == solver_->variables_.size());
  CHECK(last_constraint_index_ == 0 ||
        last_constraint_index_ == solver_->constraints_.size());
  int total_num_vars = solver_->variables_.size();
  if (total_num_vars > last_variable_index_) {
    int num_new_variables = total_num_vars - last_variable_index_;
    std::unique_ptr<double[]> obj_coefs(new double[num_new_variables]);
    std::unique_ptr<double[]> lb(new double[num_new_variables]);
    std::unique_ptr<double[]> ub(new double[num_new_variables]);
    std::unique_ptr<char[]> ctype(new char[num_new_variables]);
    std::unique_ptr<const char * []> colname(
        new const char* [num_new_variables]);

    for (int j = 0; j < num_new_variables; ++j) {
      MPVariable* const var = solver_->variables_[last_variable_index_ + j];
      var->set_index(last_variable_index_ + j);
      lb[j] = var->lb();
      ub[j] = var->ub();
      ctype.get()[j] = var->integer() && mip_ ? GRB_INTEGER : GRB_CONTINUOUS;
      if (!var->name().empty()) {
        colname[j] = var->name().c_str();
      }
      obj_coefs[j] = solver_->objective_->GetCoefficient(var);
    }

    CHECKED_GUROBI_CALL(GRBaddvars(
        model_, num_new_variables, 0, NULL, NULL, NULL, obj_coefs.get(),
        lb.get(), ub.get(), ctype.get(), const_cast<char**>(colname.get())));
  }
  CHECKED_GUROBI_CALL(GRBupdatemodel(model_));
}

void GurobiInterface::ExtractNewConstraints() {
  CHECK(last_variable_index_ == 0 ||
        last_variable_index_ == solver_->variables_.size());
  CHECK(last_constraint_index_ == 0 ||
        last_constraint_index_ == solver_->constraints_.size());
  int total_num_rows = solver_->constraints_.size();
  if (last_constraint_index_ < total_num_rows) {
    // Find the length of the longest row.
    int max_row_length = 0;
    for (int row = last_constraint_index_; row < total_num_rows; ++row) {
      MPConstraint* const ct = solver_->constraints_[row];
      CHECK_EQ(kNoIndex, ct->index());
      ct->set_index(row);
      if (ct->coefficients_.size() > max_row_length) {
        max_row_length = ct->coefficients_.size();
      }
    }

    max_row_length = std::max(1, max_row_length);
    std::unique_ptr<int[]> col_indices(new int[max_row_length]);
    std::unique_ptr<double[]> coefs(new double[max_row_length]);

    // Add each new constraint.
    for (int row = last_constraint_index_; row < total_num_rows; ++row) {
      MPConstraint* const ct = solver_->constraints_[row];
      DCHECK_NE(kNoIndex, ct->index());
      const int size = ct->coefficients_.size();
      int col = 0;
      for (CoeffEntry entry : ct->coefficients_) {
        const int index = entry.first->index();
        DCHECK_NE(kNoIndex, index);
        col_indices[col] = index;
        coefs[col] = entry.second;
        col++;
      }
      char* const name =
          ct->name().empty() ? NULL : const_cast<char*>(ct->name().c_str());
      CHECKED_GUROBI_CALL(GRBaddrangeconstr(model_, size, col_indices.get(),
                                            coefs.get(), ct->lb(), ct->ub(),
                                            name));
    }
  }
  CHECKED_GUROBI_CALL(GRBupdatemodel(model_));
}

void GurobiInterface::ExtractObjective() {
  CHECKED_GUROBI_CALL(
      GRBsetintattr(model_, GRB_INT_ATTR_MODELSENSE, maximize_ ? -1 : 1));
  CHECKED_GUROBI_CALL(GRBsetdblattr(model_, GRB_DBL_ATTR_OBJCON,
                                    solver_->Objective().offset()));
}

// ------ Parameters  -----

void GurobiInterface::SetParameters(const MPSolverParameters& param) {
  SetCommonParameters(param);
  if (mip_) {
    SetMIPParameters(param);
  }
}

void GurobiInterface::SetRelativeMipGap(double value) {
  if (mip_) {
    CHECKED_GUROBI_CALL(
        GRBsetdblparam(GRBgetenv(model_), GRB_DBL_PAR_MIPGAP, value));
  } else {
    LOG(WARNING) << "The relative MIP gap is only available "
                 << "for discrete problems.";
  }
}

void GurobiInterface::SetPrimalTolerance(double value) {
  CHECKED_GUROBI_CALL(
      GRBsetdblparam(GRBgetenv(model_), GRB_DBL_PAR_FEASIBILITYTOL, value));
}

void GurobiInterface::SetDualTolerance(double value) {
  CHECKED_GUROBI_CALL(
      GRBsetdblparam(GRBgetenv(model_), GRB_DBL_PAR_OPTIMALITYTOL, value));
}

void GurobiInterface::SetPresolveMode(int value) {
  switch (value) {
    case MPSolverParameters::PRESOLVE_OFF: {
      CHECKED_GUROBI_CALL(
          GRBsetintparam(GRBgetenv(model_), GRB_INT_PAR_PRESOLVE, false));
      break;
    }
    case MPSolverParameters::PRESOLVE_ON: {
      CHECKED_GUROBI_CALL(
          GRBsetintparam(GRBgetenv(model_), GRB_INT_PAR_PRESOLVE, true));
      break;
    }
    default: {
      SetIntegerParamToUnsupportedValue(MPSolverParameters::PRESOLVE, value);
    }
  }
}

// Sets the scaling mode.
void GurobiInterface::SetScalingMode(int value) {
  switch (value) {
    case MPSolverParameters::SCALING_OFF:
      CHECKED_GUROBI_CALL(
          GRBsetintparam(GRBgetenv(model_), GRB_INT_PAR_SCALEFLAG, false));
      break;
    case MPSolverParameters::SCALING_ON:
      CHECKED_GUROBI_CALL(
          GRBsetintparam(GRBgetenv(model_), GRB_INT_PAR_SCALEFLAG, true));
      CHECKED_GUROBI_CALL(
          GRBsetdblparam(GRBgetenv(model_), GRB_DBL_PAR_OBJSCALE, 0.0));
      break;
    default:
      // Leave the parameters untouched.
      break;
  }
}

// Sets the LP algorithm : primal, dual or barrier. Note that GRB
// offers automatic selection
void GurobiInterface::SetLpAlgorithm(int value) {
  switch (value) {
    case MPSolverParameters::DUAL:
      CHECKED_GUROBI_CALL(GRBsetintparam(GRBgetenv(model_), GRB_INT_PAR_METHOD,
                                         GRB_METHOD_DUAL));
      break;
    case MPSolverParameters::PRIMAL:
      CHECKED_GUROBI_CALL(GRBsetintparam(GRBgetenv(model_), GRB_INT_PAR_METHOD,
                                         GRB_METHOD_PRIMAL));
      break;
    case MPSolverParameters::BARRIER:
      CHECKED_GUROBI_CALL(GRBsetintparam(GRBgetenv(model_), GRB_INT_PAR_METHOD,
                                         GRB_METHOD_BARRIER));
      break;
    default:
      SetIntegerParamToUnsupportedValue(MPSolverParameters::LP_ALGORITHM,
                                        value);
  }
}

MPSolver::ResultStatus GurobiInterface::Solve(const MPSolverParameters& param) {
  WallTimer timer;
  timer.Start();

  if (param.GetIntegerParam(MPSolverParameters::INCREMENTALITY) ==
      MPSolverParameters::INCREMENTALITY_OFF) {
    Reset();
  }

  // TODO(user,user): Support incrementality.
  if (sync_status_ == MUST_RELOAD) {
    Reset();
  }

  // Set log level.
  CHECKED_GUROBI_CALL(
      GRBsetintparam(GRBgetenv(model_), GRB_INT_PAR_OUTPUTFLAG, !quiet_));

  ExtractModel();
  // Sync solver.
  CHECKED_GUROBI_CALL(GRBupdatemodel(model_));

  VLOG(1) << StringPrintf("Model build in %.3f seconds.", timer.Get());

  // Time limit.
  if (solver_->time_limit() != 0) {
    VLOG(1) << "Setting time limit = " << solver_->time_limit() << " ms.";
    CHECKED_GUROBI_CALL(GRBsetdblparam(GRBgetenv(model_), GRB_DBL_PAR_TIMELIMIT,
                                       solver_->time_limit_in_secs()));
  }

  solver_->SetSolverSpecificParametersAsString(
      solver_->solver_specific_parameter_string_);
  SetParameters(param);

  // Solve
  timer.Restart();
  const int status = GRBoptimize(model_);

  if (status) {
    VLOG(1) << "Failed to optimize MIP." << GRBgeterrormsg(env_);
  } else {
    VLOG(1) << StringPrintf("Solved in %.3f seconds.", timer.Get());
  }

  // Get the status.
  int optimization_status = 0;
  CHECKED_GUROBI_CALL(
      GRBgetintattr(model_, GRB_INT_ATTR_STATUS, &optimization_status));
  VLOG(1) << StringPrintf("Solution status %d.\n", optimization_status);
  int solution_count = 0;
  CHECKED_GUROBI_CALL(
      GRBgetintattr(model_, GRB_INT_ATTR_SOLCOUNT, &solution_count));

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
      // TODO(user,user): We could introduce our own "infeasible or
      // unbounded" status.
      result_status_ = MPSolver::INFEASIBLE;
      break;
    default: {
      if (solution_count > 0) {
        result_status_ = MPSolver::FEASIBLE;
      } else {
        // TODO(user,user): We could introduce additional values for the
        // status: for example, stopped because of time limit.
        result_status_ = MPSolver::ABNORMAL;
      }
      break;
    }
  }

  if (solution_count > 0 && (result_status_ == MPSolver::FEASIBLE ||
                             result_status_ == MPSolver::OPTIMAL)) {
    // Get the results.
    const int total_num_rows = solver_->constraints_.size();
    const int total_num_cols = solver_->variables_.size();

    std::unique_ptr<double[]> values(new double[total_num_cols]);
    std::unique_ptr<double[]> dual_values(new double[total_num_rows]);
    std::unique_ptr<double[]> slacks(new double[total_num_rows]);
    std::unique_ptr<double[]> rhs(new double[total_num_rows]);
    std::unique_ptr<double[]> reduced_costs(new double[total_num_cols]);

    CHECKED_GUROBI_CALL(
        GRBgetdblattr(model_, GRB_DBL_ATTR_OBJVAL, &objective_value_));
    CHECKED_GUROBI_CALL(GRBgetdblattrarray(model_, GRB_DBL_ATTR_X, 0,
                                           total_num_cols, values.get()));
    CHECKED_GUROBI_CALL(GRBgetdblattrarray(model_, GRB_DBL_ATTR_SLACK, 0,
                                           total_num_rows, slacks.get()));
    CHECKED_GUROBI_CALL(GRBgetdblattrarray(model_, GRB_DBL_ATTR_RHS, 0,
                                           total_num_rows, rhs.get()));
    if (!mip_) {
      CHECKED_GUROBI_CALL(GRBgetdblattrarray(
          model_, GRB_DBL_ATTR_RC, 0, total_num_cols, reduced_costs.get()));
      CHECKED_GUROBI_CALL(GRBgetdblattrarray(
          model_, GRB_DBL_ATTR_PI, 0, total_num_rows, dual_values.get()));
    }

    VLOG(1) << "objective = " << objective_value_;
    for (int i = 0; i < solver_->variables_.size(); ++i) {
      MPVariable* const var = solver_->variables_[i];
      var->set_solution_value(values[i]);
      VLOG(3) << var->name() << ", value = " << values[i];
      if (!mip_) {
        var->set_reduced_cost(reduced_costs[i]);
        VLOG(4) << var->name() << ", reduced cost = " << reduced_costs[i];
      }
    }

    for (int i = 0; i < solver_->constraints_.size(); ++i) {
      MPConstraint* const ct = solver_->constraints_[i];
      ct->set_activity(rhs[i] - slacks[i]);
      if (mip_) {
        VLOG(4) << "row " << ct->index() << ", slack = " << slacks[i]
                << ", rhs = " << rhs[i];
      } else {
        ct->set_dual_value(dual_values[i]);
        VLOG(4) << "row " << ct->index() << ", slack = " << slacks[i]
                << ", rhs = " << rhs[i] << ", dual value = " << dual_values[i];
      }
    }
  }

  sync_status_ = SOLUTION_SYNCHRONIZED;
  GRBresetparams(GRBgetenv(model_));
  return result_status_;
}

bool GurobiInterface::ReadParameterFile(const std::string& filename) {
  // A non-zero return value indicates that a problem occurred.
  return GRBreadparams(GRBgetenv(model_), filename.c_str()) == 0;
}

std::string GurobiInterface::ValidFileExtensionForParameterFile() const {
  return ".prm";
}

MPSolverInterface* BuildGurobiInterface(bool mip, MPSolver* const solver) {
  return new GurobiInterface(solver, mip);
}

#undef CHECKED_GUROBI_CALL


}  // namespace operations_research
#endif  //  #if defined(USE_GUROBI)
