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
//

#include <math.h>
#include <stddef.h>
#include "base/hash.h"
#include <limits>
#include <string>
#include <utility>
#include <vector>
#include <float.h>
#include <stdio.h>

#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "base/timer.h"
#include "base/hash.h"
#include "linear_solver/linear_solver.h"

#if defined(USE_SLM)

extern "C" {
  #include "sulumc.h"
}

void ExLogCallbackFunction(enum SlmStream str, const char *strprint, void *handle )
{
  /* Print with printf */
  std::cout <<strprint;
}

#define CheckReturnKey(__ret__)\
{\
  if( (__ret__) != SlmRetOk ) {\
    VLOG(0) <<"Writing problem to  : sulum_error.mps";\
    int wret = SlmWriteProblem(model_,"sulum_error.mps");\
    if( (wret) != SlmRetOk )\
    {\
      VLOG(0) <<"Error writing problem to  : sulum_error.mps : "<<(wret);\
    }\
    VLOG(0) <<"Writing solution to : sulum_error.sol";\
    wret = SlmWriteSolution(model_,"sulum_error.sol");\
    if( (wret) != SlmRetOk )\
    {\
      VLOG(0) <<"Error writing solution to  : sulum_error.sol : "<<(wret);\
    }\
    LOG(FATAL) <<"Error Sulum API call failed : at line "<<__LINE__<<" ret : "<<(__ret__);\
  }\
}

DECLARE_double(solver_timeout_in_seconds);
DECLARE_string(solver_write_model);

namespace operations_research {

// ----- SLM Solver -----

class SLMInterface : public MPSolverInterface {
 public:
  // Constructor that takes a name for the underlying slm solver.
  SLMInterface(MPSolver* const solver, bool mip);
  ~SLMInterface();

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

  // Checks whether a feasible solution exists.
  virtual bool CheckSolutionExists() const;
  // Checks whether information on the best objective bound exists.
  virtual bool CheckBestObjectiveBoundExists() const;

  // ----- Misc -----
  // Write model
  virtual void WriteModel(const std::string& filename);

  // Query problem type.
  virtual bool IsContinuous() const { return IsLP(); }
  virtual bool IsLP() const { return !mip_; }
  virtual bool IsMIP() const { return mip_; }

  virtual void ExtractNewVariables();
  virtual void ExtractNewConstraints();
  virtual void ExtractObjective();

  virtual std::string SolverVersion() const {
    int major,minor,interim;

    SlmGetSulumVersion(&major,&minor,&interim);

    return StringPrintf("SLM major : %d minor : %d interim : %d",major,minor,interim);
  }

  virtual void* underlying_solver() {
    return reinterpret_cast<void*>(model_);
  }

  virtual double ComputeExactConditionNumber() const;

 private:
  // Configure the solver's parameters.
  void ConfigureSLMParameters(const MPSolverParameters& param);

  // Set all parameters in the underlying solver.
  virtual void SetParameters(const MPSolverParameters& param);
  // Set each parameter in the underlying solver.
  virtual void SetRelativeMipGap(double value);
  virtual void SetPrimalTolerance(double value);
  virtual void SetDualTolerance(double value);
  virtual void SetPresolveMode(int value);
  virtual void SetScalingMode(int value);
  virtual void SetLpAlgorithm(int value);

  void ExtractOldConstraints();
  void ExtractOneConstraint(MPConstraint* const constraint,
                            int* const indices,
                            double* const coefs);
  // Transforms basis status from SLM integer code to MPSolver::BasisStatus.
  MPSolver::BasisStatus TransformSLMBasisStatus(SlmStatusKey slm_basis_status) const;

  // Computes the L1-norm of the current scaled basis.
  // The L1-norm |A| is defined as max_j sum_i |a_ij|
  // This method is available only for continuous problems.
  double ComputeScaledBasisL1Norm(
    int num_rows, int num_cols,
    double* row_scaling_factor, double* column_scaling_factor) const;

  // Computes the L1-norm of the inverse of the current scaled
  // basis.
  // This method is available only for continuous problems.
  double ComputeInverseScaledBasisL1Norm(
    int num_rows, int num_cols,
    double* row_scaling_factor, double* column_scaling_factor) const;

  SlmEnv_t   env_;
  SlmModel_t model_;
  bool mip_;
};

// Creates a LP/MIP instance with the specified name and minimization objective.
SLMInterface::SLMInterface(MPSolver* const solver, bool mip)
    : MPSolverInterface(solver), env_(NULL), model_(NULL), mip_(mip) {
  CheckReturnKey(SlmMakeEnv(&env_));
  CheckReturnKey(SlmMakeModel(env_,&model_));

  /* Add logging callback function */
  CheckReturnKey(SlmSetLoggingCallback(model_,&ExLogCallbackFunction,NULL));

  if(solver_->name_.empty() == false) {
    CheckReturnKey(SlmSetObjName(model_, solver_->name_.c_str()));
  }

  SetOptimizationDirection(maximize_);
}

// Frees the LP memory allocations.
SLMInterface::~SLMInterface() {
  CHECK_NOTNULL(env_);
  CHECK_NOTNULL(model_);
  CheckReturnKey(SlmFreeModel(env_,&model_));
  CheckReturnKey(SlmFreeEnv(&env_));

  env_   = NULL;
  model_ = NULL;
}

void SLMInterface::Reset() {
  CHECK_NOTNULL(model_);
  CheckReturnKey(SlmClear(model_));
  CheckReturnKey(SlmSetObjName(model_, solver_->name_.c_str()));
  CheckReturnKey(SlmSetParamDefault(model_));
  SetOptimizationDirection(maximize_);
  ResetExtractionInformation();
}

void SLMInterface::WriteModel(const std::string& filename) {
  CheckReturnKey(SlmWriteProblem(model_, filename.c_str()));
}

// ------ Model modifications and extraction -----

// Not cached
void SLMInterface::SetOptimizationDirection(bool maximize) {
  maximize_ = maximize;
  InvalidateSolutionSynchronization();
  CheckReturnKey(SlmSetIntParam(model_,SlmPrmIntObjSense,maximize_ == true ? SlmObjSenseMax : SlmObjSenseMin ));
}

void SLMInterface::SetVariableBounds(int var_index, double lb, double ub) {
  InvalidateSolutionSynchronization();
  if (var_index != kNoIndex) {
    // Not cached if the variable has been extracted.
    DCHECK(model_ != NULL);
    const double infinity = solver_->infinity();

    SlmBoundKey bk;
    double      lo = lb;
    double      up = ub;

    if (lb != -infinity) {
      if (ub != infinity) {
        if (lb == ub) {
          bk = SlmBndFx;
        } else {
          bk = SlmBndRa;
        }
      } else {
        up = SlmInfinity;
        bk = SlmBndLo;
      }
    } else if (ub != infinity) {
      lo = -SlmInfinity;
      bk = SlmBndUp;
    } else {
      lo = -SlmInfinity;
      up = SlmInfinity;
      bk = SlmBndFr;
    }

    CheckReturnKey(SlmSetKeyVarsI(model_,var_index,bk));
    CheckReturnKey(SlmSetLoVarsI(model_,var_index,lo));
    CheckReturnKey(SlmSetUpVarsI(model_,var_index,up));
  } else {
    sync_status_ = MUST_RELOAD;
  }
}

void SLMInterface::SetVariableInteger(int var_index, bool integer) {
  InvalidateSolutionSynchronization();
  if (mip_) {
    if (var_index != kNoIndex) {
      // Not cached if the variable has been extracted.
      SlmVarType type;
      CheckReturnKey(SlmGetTypeVarsI(model_,var_index,&type));

      if(type ==SlmVarTypeCont) {
        integer = false;
      }
      else {
        integer = true;
      }
    } else {
      sync_status_ = MUST_RELOAD;
    }
  }
}

void SLMInterface::SetConstraintBounds(int index, double lb, double ub) {
  InvalidateSolutionSynchronization();
  if (index != kNoIndex) {
    // Not cached if the row has been extracted
    DCHECK(model_ != NULL);
    const double infinity = solver_->infinity();
    SlmBoundKey bk;
    double      lo = lb;
    double      up = ub;

    if (lb != -infinity) {
      if (ub != infinity) {
        if (lb == ub) {
          bk = SlmBndFx;
        } else {
          bk = SlmBndRa;
        }
      } else {
        up = SlmInfinity;
        bk = SlmBndLo;
      }
    } else if (ub != infinity) {
      lo = -SlmInfinity;
      bk = SlmBndUp;
    } else {
      lo = -SlmInfinity;
      up = SlmInfinity;
      bk = SlmBndFr;
    }

    CheckReturnKey(SlmSetKeyConsI(model_,index,bk));
    CheckReturnKey(SlmSetLoConsI(model_,index,lo));
    CheckReturnKey(SlmSetUpConsI(model_,index,up));
  } else {
    sync_status_ = MUST_RELOAD;
  }
}

void SLMInterface::SetCoefficient(MPConstraint* const constraint,
                                   const MPVariable* const variable,
                                   double new_value,
                                   double old_value) {
  InvalidateSolutionSynchronization();
  const int constraint_index = constraint->index();
  const int variable_index = variable->index();
  if (constraint_index != kNoIndex && variable_index != kNoIndex) {
    // The modification of the coefficient for an extracted row and
    // variable is not cached.
    DCHECK_LE(constraint_index, last_constraint_index_);
    DCHECK_LE(variable_index, last_variable_index_);
    CheckReturnKey(SlmSetAIJ(model_,constraint_index, variable_index, new_value));
  } else {
    // The modification of an unextracted row or variable is cached
    // and handled in ExtractModel.
    sync_status_ = MUST_RELOAD;
  }
}

// Not cached
void SLMInterface::ClearConstraint(MPConstraint* const constraint) {
  InvalidateSolutionSynchronization();
  const int constraint_index = constraint->index();
  // Constraint may not have been extracted yet.
  if (constraint_index != kNoIndex) {
    CheckReturnKey(SlmSetAConsI(model_,constraint_index, 0, NULL, NULL));
  }
}

// Cached
void SLMInterface::SetObjectiveCoefficient(const MPVariable* const variable,
                                            double coefficient) {
  sync_status_ = MUST_RELOAD;
}

// Cached
void SLMInterface::SetObjectiveOffset(double value) {
  sync_status_ = MUST_RELOAD;
}

// Clear objective of all its terms (linear)
void SLMInterface::ClearObjective() {
  InvalidateSolutionSynchronization();
  for (const auto& it : solver_->objective_->coefficients_) {
    const int var_index = it.first->index();
    // Variable may have not been extracted yet.
    if (var_index == kNoIndex) {
      DCHECK_NE(MODEL_SYNCHRONIZED, sync_status_);
    } else {
      CheckReturnKey(SlmSetObjVarsI(model_,var_index, 0.0));
    }
  }
  // Constant term.
  CheckReturnKey(SlmSetObjFix(model_, 0.0));
}

void SLMInterface::AddRowConstraint(MPConstraint* const ct) {
  sync_status_ = MUST_RELOAD;
}

void SLMInterface::AddVariable(MPVariable* const var) {
  sync_status_ = MUST_RELOAD;
}

// Define new variables and add them to existing constraints.
void SLMInterface::ExtractNewVariables() {
  int total_num_vars = solver_->variables_.size();
  if (total_num_vars > last_variable_index_) {
    CheckReturnKey(SlmAddEmptyVars(model_,total_num_vars - last_variable_index_));
    for (int j = last_variable_index_; j < solver_->variables_.size(); ++j) {
      MPVariable* const var = solver_->variables_[j];
      var->set_index(j);
      if (!var->name().empty()) {
        CheckReturnKey(SlmSetNameVarsI(model_,j, var->name().c_str()));
      }

      SetVariableBounds(j, var->lb(), var->ub());
      SetVariableInteger(j, var->integer());

      // The true objective coefficient will be set later in ExtractObjective.
      double tmp_obj_coef = 0.0;
      CheckReturnKey(SlmSetObjVarsI(model_,j, tmp_obj_coef));
    }
    // Add new variables to the existing constraints.
    ExtractOldConstraints();
  }
}

// Extract again existing constraints if they contain new variables.
void SLMInterface::ExtractOldConstraints() {
  int max_constraint_size = solver_->ComputeMaxConstraintSize(
      0, last_constraint_index_);

  scoped_array<int> indices(new int[max_constraint_size ]);
  scoped_array<double> coefs(new double[max_constraint_size ]);

  for (int i = 0; i < last_constraint_index_; ++i) {
    MPConstraint* const  ct = solver_->constraints_[i];
    DCHECK_NE(kNoIndex, ct->index());
    const int size = ct->coefficients_.size();
    if (size == 0) {
      continue;
    }
    // Update the constraint's coefficients if it contains new variables.
    if (ct->ContainsNewVariables()) {
      ExtractOneConstraint(ct, indices.get(), coefs.get());
    }
  }
}

// Extract one constraint. Arrays indices and coefs must be
// preallocated to have enough space to contain the constraint's
// coefficients.
void SLMInterface::ExtractOneConstraint(MPConstraint* const constraint,
                                         int* const indices,
                                         double* const coefs) {
  int k = 0;
  for (const auto& it : constraint->coefficients_) {
    const int var_index = it.first->index();
    DCHECK_NE(kNoIndex, var_index);
    indices[k] = var_index;
    coefs[k] = it.second;
    ++k;
  }

  CheckReturnKey(SlmSetAConsI(model_,constraint->index(), k, indices, coefs));
}

// Define new constraints on old and new variables.
void SLMInterface::ExtractNewConstraints() {
  int total_num_rows = solver_->constraints_.size();
  if (last_constraint_index_ < total_num_rows) {
    // Find the length of the longest row.
    int64 newanz = 0;
    int64 oldanz = 0;

    int max_row_length = 0;
    for (int i = last_constraint_index_; i < total_num_rows; ++i) {
      MPConstraint* const ct = solver_->constraints_[i];
      DCHECK_EQ(kNoIndex, ct->index());
      ct->set_index(i);
      if (ct->coefficients_.size() > max_row_length) {
        max_row_length = ct->coefficients_.size();
      }

      newanz += ct->coefficients_.size();
    }

    int addrows = total_num_rows - last_constraint_index_;

    // Add sizes for efficiens
    CheckReturnKey(SlmGetANz64(model_,&oldanz));
    CheckReturnKey(SlmHintAMaxNz64(model_,newanz+oldanz));
    CheckReturnKey(SlmAddEmptyCons(model_,addrows));

    // Make space for dummy variable.
    max_row_length = std::max(1, max_row_length);
    scoped_array<int> indices(new int[max_row_length]);
    scoped_array<double> coefs(new double[max_row_length]);

    // Add each new constraint.
    for (int i = last_constraint_index_; i < total_num_rows; ++i) {
      MPConstraint* const  ct = solver_->constraints_[i];
      DCHECK_NE(kNoIndex, ct->index());
      int size = ct->coefficients_.size();
      int j = 0;
      for (const auto& it : ct->coefficients_) {
        const int index = it.first->index();
        DCHECK_NE(kNoIndex, index);
        indices[j] = index;
        coefs[j] = it.second;
        j++;
      }

      if( size > 0 ) {
        CheckReturnKey(SlmSetAConsI(model_,i,size,indices.get(),coefs.get()));
      }

      SetConstraintBounds(i, ct->lb(),ct->ub());

      if (!ct->name().empty()) {
        std::string std_name = ct->name();
        CheckReturnKey(SlmSetNameConsI(model_,ct->index(), std_name.c_str()));
      }
    }
  }
}

void SLMInterface::ExtractObjective() {
  // Linear objective: set objective coefficients for all variables
  // (some might have been modified).
  for (hash_map<const MPVariable*, double>::const_iterator it =
           solver_->objective_->coefficients_.begin();
       it != solver_->objective_->coefficients_.end();
       ++it) {
    CheckReturnKey(SlmSetObjVarsI(model_,it->first->index(), it->second));
  }
  // Constant term.
  CheckReturnKey(SlmSetObjFix(model_, solver_->Objective().offset()));
}

// Solve the problem using the parameter values specified.
MPSolver::ResultStatus SLMInterface::Solve(const MPSolverParameters& param) {
  WallTimer timer;
  timer.Start();

  // Note that SLM provides incrementality for LP but not for MIP.
  if (param.GetIntegerParam(MPSolverParameters::INCREMENTALITY) ==
      MPSolverParameters::INCREMENTALITY_OFF) {
    Reset();
  }

  // Set log level.
  if (quiet_) {
    CheckReturnKey(SlmSetIntParam(model_,SlmPrmIntLogLevel,0));
    CheckReturnKey(SlmSetIntParam(model_,SlmPrmIntSimLogLevel,0));
    CheckReturnKey(SlmSetIntParam(model_,SlmPrmIntLogNoModuleMessage,SlmOff));
  } else {
    CheckReturnKey(SlmSetIntParam(model_,SlmPrmIntLogLevel,5));
    CheckReturnKey(SlmSetIntParam(model_,SlmPrmIntSimLogLevel,5));
    CheckReturnKey(SlmSetIntParam(model_,SlmPrmIntLogNoModuleMessage,SlmOn));
  }

  ExtractModel();
  VLOG(1) << StringPrintf("Model built in %.3f seconds.", timer.Get());

  // Configure parameters at every solve, even when the model has not
  // been changed, in case some of the parameters such as the time
  // limit have been changed since the last solve.
  ConfigureSLMParameters(param);

  // Solve
  timer.Restart();

  CheckReturnKey(SlmSetIntParam(model_,SlmPrmIntUpdateSolQuality,SlmOn));

  CheckReturnKey(SlmOptimize(model_));

  VLOG(1) << StringPrintf("Solved in %.3f seconds.", timer.Get());

  // Get the results.
  CheckReturnKey(SlmGetDbInfo(model_,SlmInfoDbPrimObj,&objective_value_));

  VLOG(1) << "objective=" << objective_value_;
  for (int i = 0; i < solver_->variables_.size(); ++i) {
    MPVariable* const var = solver_->variables_[i];
    double val;
    CheckReturnKey(SlmGetSolPrimVarsI(model_,var->index(),&val));
    var->set_solution_value(val);
    VLOG(3) << var->name() << ": value =" << val;
    if (!mip_) {
      double reduced_cost;
      CheckReturnKey(SlmGetSolDualVarsI(model_,var->index(),&reduced_cost));
      var->set_reduced_cost(reduced_cost);
      VLOG(4) << var->name() << ": reduced cost = " << reduced_cost;
    }
  }
  for (int i = 0; i < solver_->constraints_.size(); ++i) {
    MPConstraint* const ct = solver_->constraints_[i];
    double row_activity;
    CheckReturnKey(SlmGetSolPrimConsI(model_,ct->index(),&row_activity));
    ct->set_activity(row_activity);

    if (mip_) {
      VLOG(4) << "row " << ct->index()
              << ": activity = " << row_activity;
    } else {
      double dual_value;
      CheckReturnKey(SlmGetSolDualConsI(model_,ct->index(),&dual_value));
      ct->set_dual_value(dual_value);
      VLOG(4) << "row " << ct->index()
              << ": activity = " << row_activity
              << ": dual value = " << dual_value;
    }
  }

  // Check the status: optimal, infeasible, etc.
  SlmSolStatus tmp_status;
  CheckReturnKey(SlmGetSolStatus(model_,&tmp_status));

  switch(tmp_status)
  {
    case SlmSolStatUnk :
      VLOG(1) << "slm result status: SlmSolStatUnk";
      result_status_ = MPSolver::INFEASIBLE; /* What ever that means.. */
      break;
    case SlmSolStatOpt :
      VLOG(1) << "slm result status: SlmSolStatOpt";
      result_status_ = MPSolver::OPTIMAL;
      break;
    case SlmSolStatPrimFeas :
      VLOG(1) << "slm result status: SlmSolStatPrimFeas";
      result_status_ = MPSolver::FEASIBLE; /* What ever that means.. */
      break;
    case SlmSolStatDualFeas :
      VLOG(1) << "slm result status: SlmSolStatDualFeas";
      result_status_ = MPSolver::FEASIBLE; /* What ever that means.. */
      break;
    case SlmSolStatPrimInf :
      VLOG(1) << "slm result status: SlmSolStatPrimInf";
      result_status_ = MPSolver::INFEASIBLE;
      break;
    case SlmSolStatDualInf :
      VLOG(1) << "slm result status: SlmSolStatDualInf";
      result_status_ = MPSolver::UNBOUNDED; /* Theoretically not correct, you need a primal feasible point in LP */
      break;
    case SlmSolStatIntFeas :
      VLOG(1) << "slm result status: SlmSolStatIntFeas";
      result_status_ = MPSolver::FEASIBLE;
      break;
    case SlmSolStatIntInf :
      VLOG(1) << "slm result status: SlmSolStatIntInf";
      result_status_ = MPSolver::INFEASIBLE;
      break;
  }

  sync_status_ = SOLUTION_SYNCHRONIZED;

  return result_status_;
}

MPSolver::BasisStatus
SLMInterface::TransformSLMBasisStatus(SlmStatusKey slm_basis_status) const {
  switch (slm_basis_status) {
    case SlmStaBa:
      return MPSolver::BASIC;
    case SlmStaLo:
      return MPSolver::AT_LOWER_BOUND;
    case SlmStaUp:
      return MPSolver::AT_UPPER_BOUND;
    case SlmStaSb:
      return MPSolver::FREE;
    case SlmStaFx:
      return MPSolver::FIXED_VALUE;
    default:
      LOG(FATAL) << "Unknown SLM basis status";
      return MPSolver::FREE;
  }
}

MPSolverInterface* BuildSLMInterface(MPSolver* const solver, bool mip) {
  return new SLMInterface(solver, mip);
}

// ------ Query statistics on the solution and the solve ------

int64 SLMInterface::iterations() const {
  int iter;
  CheckSolutionIsSynchronized();
  if(mip_) {
    LOG(WARNING) << "Total number of iterations is not available";
    return kUnknownNumberOfIterations;
  }
  else {
    CheckReturnKey(SlmGetIntInfo(model_,SlmInfoIntSimIter,&iter));
  }

  return static_cast<int64>(iter);
}

int64 SLMInterface::nodes() const {
  if (mip_) {
    CheckSolutionIsSynchronized();
    int nodes;
    CheckReturnKey(SlmGetIntInfo(model_,SlmInfoIntMipNodes,&nodes));
    return static_cast<int64>(nodes);
  } else {
    LOG(FATAL) << "Number of nodes only available for discrete problems";
    return kUnknownNumberOfNodes;
  }
}

double SLMInterface::best_objective_bound() const {
  if (mip_) {
    CheckSolutionIsSynchronized();
    CheckBestObjectiveBoundExists();
    if (solver_->variables_.size() == 0 && solver_->constraints_.size() == 0) {
      // Special case for empty model.
      return solver_->Objective().offset();
    } else {
      double best_objective_bound;
      CheckReturnKey(SlmGetDbInfo(model_,SlmInfoDbMipBoundLP,&best_objective_bound));
      return best_objective_bound;
    }
  } else {
    LOG(FATAL) << "Best objective bound only available for discrete problems";
    return 0.0;
  }
}

MPSolver::BasisStatus SLMInterface::row_status(int constraint_index) const {
  // + 1 because of SLM indexing convention.
  DCHECK_LE(1, constraint_index);
  DCHECK_GT(last_constraint_index_ + 1, constraint_index);
  SlmStatusKey slm_basis_status;
  CheckReturnKey(SlmGetSolKeyPrimConsI(model_,constraint_index,&slm_basis_status));
  return TransformSLMBasisStatus(slm_basis_status);
}

MPSolver::BasisStatus SLMInterface::column_status(int variable_index) const {
  // + 1 because of SLM indexing convention.
  DCHECK_LE(1, variable_index);
  DCHECK_GT(last_variable_index_ + 1, variable_index);
  SlmStatusKey slm_basis_status;
  CheckReturnKey(SlmGetSolKeyPrimVarsI(model_,variable_index,&slm_basis_status));
  return TransformSLMBasisStatus(slm_basis_status);
}

bool SLMInterface::CheckSolutionExists() const {
  if (result_status_ == MPSolver::ABNORMAL) {
    LOG(WARNING) << "Ignoring ABNORMAL status from SLM: This status may or may"
                 << " not indicate that a solution exists.";
    return false;
  } else {
    // Call default implementation
    return MPSolverInterface::CheckSolutionExists();
  }
}

bool SLMInterface::CheckBestObjectiveBoundExists() const {
  if (result_status_ == MPSolver::ABNORMAL) {
    LOG(WARNING) << "Ignoring ABNORMAL status from SLM: This status may or may"
                 << " not indicate that information is available on the best"
                 << " objective bound.";
    return false;
  } else {
    // Call default implementation
    return MPSolverInterface::CheckBestObjectiveBoundExists();
  }
}

double SLMInterface::ComputeExactConditionNumber() const {
  CHECK(IsContinuous()) <<
      "Condition number only available for continuous problems";
  CheckSolutionIsSynchronized();
  // Simplex is the only LP algorithm supported in the wrapper for
  // SLM, so when a solution exists, a basis exists.
  CheckSolutionExists();
  int num_rows;
  int num_cols;

  CheckReturnKey(SlmGetCons(model_,&num_rows));
  CheckReturnKey(SlmGetVars(model_,&num_cols));

  scoped_array<double> row_scaling_factor(new double[num_rows]);
  scoped_array<double> column_scaling_factor(new double[num_cols]);

  for (int row = 0; row < num_rows; ++row) {
    row_scaling_factor[row] = 1.0;
  }
  for (int col = 0; col < num_cols; ++col) {
    column_scaling_factor[col] = 1.0;
  }

  return
      ComputeInverseScaledBasisL1Norm(
          num_rows, num_cols,
          row_scaling_factor.get(), column_scaling_factor.get()) *
      ComputeScaledBasisL1Norm(
          num_rows, num_cols,
          row_scaling_factor.get(), column_scaling_factor.get());
}

double SLMInterface::ComputeScaledBasisL1Norm(
    int num_rows, int num_cols,
    double* row_scaling_factor, double* column_scaling_factor) const {
  double norm = 0.0;

  scoped_array<double> values(new double[num_rows]);
  scoped_array<int> indices(new int[num_rows]);
  for (int col = 0; col < num_cols; ++col) {
    SlmStatusKey slm_basis_status;

    CheckReturnKey(SlmGetSolKeyPrimVarsI(model_,col,&slm_basis_status));

    // Take into account only basic columns.
    if (slm_basis_status == SlmStaBa) {
      // Compute L1-norm of column 'col': sum_row |a_row,col|.
      int num_nz;

      CheckReturnKey(SlmGetAVarsI(model_, col,&num_nz,indices.get(), values.get()));

      double column_norm = 0.0;
      for (int k = 0; k < num_nz; k++) {
        column_norm += fabs(values[k] * row_scaling_factor[indices[k]]);
      }
      column_norm *= fabs(column_scaling_factor[col]);
      // Compute max_col column_norm
      norm = std::max(norm, column_norm);
    }
  }
  // Slack variables.
  for (int row = 0; row < num_rows; ++row) {
    SlmStatusKey slm_basis_status;

    CheckReturnKey(SlmGetSolKeyPrimConsI(model_,row,&slm_basis_status));

    // Take into account only basic slack variables.
    if (slm_basis_status == SlmStaBa) {
      // Only one non-zero coefficient: +/- 1.0 in the corresponding
      // row. The row has a scaling coefficient but the slack variable
      // is never scaled on top of that.
      const double column_norm = fabs(row_scaling_factor[row]);
      // Compute max_col column_norm
      norm = std::max(norm, column_norm);
    }
  }

  return norm;
}

double SLMInterface::ComputeInverseScaledBasisL1Norm(
    int num_rows, int num_cols,
    double* row_scaling_factor, double* column_scaling_factor) const {

  // Currently we just refactor each time
  int ret = SlmInitBasisSolves(model_);

  // Compute the LU factorization if it doesn't exist yet.
  if ( ret != SlmRetOk  ) {
    switch (ret) {
      case SlmRetBasisSingular: {
        LOG(WARNING)
            << "Not able to factorize: "
            << "the basis matrix is singular within the working precision.";
        return MPSolver::infinity();
      }
      default:
        CheckReturnKey(ret);
        break;
    }
  }

  scoped_array<double> right_hand_side(new double[num_rows]);
  scoped_array<int> basidx(new int[num_rows]);

  CheckReturnKey(SlmGetBasisHead(model_,basidx.get()));

  double norm = 0.0;

  // Iteratively solve B x = e_k, where e_k is the kth unit vector.
  // The result of this computation is the kth column of B^-1.

  for (int k = 0; k < num_rows; ++k) {
    for (int row = 0; row < num_rows; ++row) {
      right_hand_side[row] = 0.0;
    }
    right_hand_side[k] = 1.0;
    // Multiply input by inv(R).
    for (int row = 0; row < num_rows; ++row) {
      right_hand_side[row] /= row_scaling_factor[row];
    }

    CheckReturnKey(SlmSolveFtranDense(model_, right_hand_side.get()));

    // stores the result in the same vector where the right
    // hand side was provided.
    // Multiply result by inv(SB).
    for (int row = 0; row < num_rows; ++row) {
      const int k = basidx[row];
      if (k <= num_rows) {
        // Auxiliary variable.
        right_hand_side[row] *= row_scaling_factor[k];
      } else {
        // Structural variable.
        right_hand_side[row] /= column_scaling_factor[k - num_rows];
      }
    }

    // Compute sum_row |vector_row|.
    double column_norm = 0.0;
    for (int row = 0; row < num_rows; ++row) {
      column_norm += fabs(right_hand_side[row]);
    }

    // Compute max_col column_norm
    norm = std::max(norm, column_norm);
  }

  return norm;
}

// ------ Parameters ------

void SLMInterface::ConfigureSLMParameters(const MPSolverParameters& param) {
  // Time limit
  if (solver_->time_limit()) {
    VLOG(1) << "Setting time limit = " << solver_->time_limit() << " ms.";
    CheckReturnKey(SlmSetDbParam(model_,SlmPrmDbOptTimeLimit,solver_->time_limit()));
  }
  else
  {
    CheckReturnKey(SlmSetDbParam(model_,SlmPrmDbOptTimeLimit,DBL_MAX));
  }

  // Set parameters specified by the user.
  SetParameters(param);
}

void SLMInterface::SetParameters(const MPSolverParameters& param) {
  SetCommonParameters(param);
  if (mip_) {
    SetMIPParameters(param);
  }
}

void SLMInterface::SetRelativeMipGap(double value) {
  if (mip_) {
    CheckReturnKey(SlmSetDbParam(model_,SlmPrmDbMipTolRelGap, value));
  } else {
    LOG(WARNING) << "The relative MIP gap is only available "
                 << "for discrete problems.";
  }
}

void SLMInterface::SetPrimalTolerance(double value) {
  CheckReturnKey(SlmSetDbParam(model_,SlmPrmDbSimTolPrim, value));
}

void SLMInterface::SetDualTolerance(double value) {
  CheckReturnKey(SlmSetDbParam(model_,SlmPrmDbSimTolDual, value));
}

void SLMInterface::SetPresolveMode(int value) {
  switch (value) {
    case MPSolverParameters::PRESOLVE_OFF: {
      CheckReturnKey(SlmSetIntParam(model_,SlmPrmIntPresolve,SlmPreOff));
      break;
    }
    case MPSolverParameters::PRESOLVE_ON: {
      CheckReturnKey(SlmSetIntParam(model_,SlmPrmIntPresolve,SlmPreFree));
      break;
    }
    default: {
      SetIntegerParamToUnsupportedValue(MPSolverParameters::PRESOLVE, value);
    }
  }
}

void SLMInterface::SetLpAlgorithm(int value) {
  switch (value) {
    case MPSolverParameters::DUAL: {
      CheckReturnKey(SlmSetIntParam(model_,SlmPrmIntOptimizer,SlmOptDual));
      break;
    }
    case MPSolverParameters::PRIMAL: {
      CheckReturnKey(SlmSetIntParam(model_,SlmPrmIntOptimizer,SlmOptPrim));
      break;
    }
    case MPSolverParameters::BARRIER:
    default: {
      SetIntegerParamToUnsupportedValue(MPSolverParameters::LP_ALGORITHM,
                                        value);
    }
  }
}

void SLMInterface::SetScalingMode(int value) {
  SetUnsupportedIntegerParam(MPSolverParameters::SCALING);
}
}  // namespace operations_research
#endif  //  #if defined(USE_SLM)
