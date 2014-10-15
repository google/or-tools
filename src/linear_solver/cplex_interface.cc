#include <stddef.h>
#include "base/hash.h"
#include <string>
#include <vector>

#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/unique_ptr.h"
#include "base/stringprintf.h"
#include "base/timer.h"
#include "base/hash.h"
#include "linear_solver/linear_solver.h"

#if defined(USE_CPLEX)
#include "ilcplex/cplex.h"

namespace operations_research {

class CplexInterface : public MPSolverInterface {
 public:
  // Constructor that takes a name for the underlying CPLEX solver.
  explicit CplexInterface(MPSolver* const solver, bool mip);
  ~CplexInterface();

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
                              double new_value, double old_value);
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
    const char* version = CPXversion(_cplexenv);
    return StringPrintf("CPLEX %s", version);
  }

  virtual void* underlying_solver() { return reinterpret_cast<void*>(_cplex); }

  virtual double ComputeExactConditionNumber() const {
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

  void ExtractOldConstraints();
  void ExtractOneConstraint(MPConstraint* const constraint);

  MPSolver::BasisStatus TransformCPLEXBasisStatus(int cplex_basis_status) const;

 private:
  cpxlp* _cplex;
  cpxenv* _cplexenv;
  bool mip_;
  int _nbVars;
  int _nbCnts;
};

// Creates a LP/MIP instance with the specified name and minimization objective.
CplexInterface::CplexInterface(MPSolver* const solver, bool mip)
    : MPSolverInterface(solver),
      _cplexenv(0),
      _cplex(0),
      _nbVars(0),
      _nbCnts(0),
      mip_(mip) {
  int status;
  _cplexenv = CPXopenCPLEX(&status);
  assert(status == 0);
  const char* name = solver_->name_.c_str();
  _cplex = CPXcreateprob(_cplexenv, &status, name);
  assert(status == 0);
  CPXchgobjsen(_cplexenv, _cplex, maximize_ ? -1 : 1);
}

CplexInterface::~CplexInterface() {
  int status;
  status = CPXfreeprob(_cplexenv, &_cplex);
  assert(status == 0);
  status = CPXcloseCPLEX(&_cplexenv);
  assert(status == 0);
}

void CplexInterface::WriteModel(const string& filename) {
  int status;
  status = CPXwriteprob(_cplexenv, _cplex, filename.c_str(), NULL);
  assert(status == 0);
}

// ------ Model modifications and extraction -----

void CplexInterface::Reset() {
  int status;
  status = CPXfreeprob(_cplexenv, &_cplex);
  assert(status == 0);
  const char* name = solver_->name_.c_str();
  _cplex = CPXcreateprob(_cplexenv, &status, name);
  assert(status == 0);
  CPXchgobjsen(_cplexenv, _cplex, maximize_ ? -1 : 1);
  ResetExtractionInformation();
}

void CplexInterface::SetOptimizationDirection(bool maximize) {
  InvalidateSolutionSynchronization();
  CPXchgobjsen(_cplexenv, _cplex, maximize ? -1 : 1);
}

void CplexInterface::SetVariableBounds(int var_index, double lb, double ub) {
  InvalidateSolutionSynchronization();
  if (var_index != kNoIndex) {
    // Not cached if the variable has been extracted
    DCHECK_LE(var_index, last_variable_index_);
    int status;
    char type_lb = 'L';
    status = CPXchgbds(_cplexenv, _cplex, 1, &var_index, &type_lb, &lb);
    assert(status == 0);
    char type_ub = 'U';
    status = CPXchgbds(_cplexenv, _cplex, 1, &var_index, &type_ub, &ub);
    assert(status == 0);
  } else {
    sync_status_ = MUST_RELOAD;
  }
}

// Modifies integrality of an extracted variable.
void CplexInterface::SetVariableInteger(int var_index, bool integer) {
  InvalidateSolutionSynchronization();
  if (mip_) {
    if (var_index != kNoIndex) {
      DCHECK_LT(var_index, CPXgetnumcols(_cplexenv, _cplex));
      int status;
      char type_var;
      if (integer) {
        type_var = 'I';
      } else {
        type_var = 'C';
      }
      status = CPXchgctype(_cplexenv, _cplex, 1, &var_index, &type_var);
      assert(status == 0);
    }
  }
}

void CplexInterface::SetConstraintBounds(int index, double lb, double ub) {
  InvalidateSolutionSynchronization();
  if (index != kNoIndex) {
    DCHECK(_cplex != NULL);
    int status;
    char sense = 'R';
    double range = ub - lb;
    double rhs = lb;
    const double infinity = solver_->infinity();
    if ((lb > -infinity) && (ub < infinity)) {
      sense = 'R';
      rhs = lb;
      range = ub - lb;
    } else if (ub == lb) {
      sense = 'E';
      rhs = lb;
      range = 0;
    } else if (ub < infinity) {
      sense = 'L';
      rhs = ub;
      range = 0;
    } else if (lb > -infinity) {
      sense = 'G';
      rhs = lb;
      range = 0;
    } else {
      rhs = 0;
      range = 0;
    }
    status = CPXchgrhs(_cplexenv, _cplex, 1, &index, &lb);
    assert(status == 0);
    status = CPXchgrngval(_cplexenv, _cplex, 1, &index, &range);
    assert(status == 0);
  } else {
    sync_status_ = MUST_RELOAD;
  }
}

void CplexInterface::AddRowConstraint(MPConstraint* const ct) {
  sync_status_ = MUST_RELOAD;
}

void CplexInterface::AddVariable(MPVariable* const ct) {
  sync_status_ = MUST_RELOAD;
}

void CplexInterface::SetCoefficient(MPConstraint* const constraint,
                                    const MPVariable* const variable,
                                    double new_value, double old_value) {
  InvalidateSolutionSynchronization();
  const int constraint_index = constraint->index();
  const int variable_index = variable->index();
  if (constraint_index != kNoIndex && variable_index != kNoIndex) {
    // The modification of the coefficient for an extracted row and
    // variable is not cached.
    DCHECK_LE(constraint_index, last_constraint_index_);
    DCHECK_LE(variable_index, last_variable_index_);
    int status = CPXchgcoef(_cplexenv, _cplex, constraint_index, variable_index,
                            new_value);
    assert(status == 0);
  } else {
    // The modification of an unextracted row or variable is cached
    // and handled in ExtractModel.
    sync_status_ = MUST_RELOAD;
  }
}

void CplexInterface::ClearConstraint(MPConstraint* const constraint) {
  InvalidateSolutionSynchronization();
  const int constraint_index = constraint->index();
  // Constraint may not have been extracted yet.
  if (constraint_index != kNoIndex) {
    for (const auto& it : constraint->coefficients_) {
      const int var_index = it.first->index();
      DCHECK_NE(kNoIndex, var_index);
      int status =
          CPXchgcoef(_cplexenv, _cplex, constraint_index, var_index, 0.0);
      assert(status == 0);
    }
  }
}

// Cached
void CplexInterface::SetObjectiveCoefficient(const MPVariable* const variable,
                                             double coefficient) {
  sync_status_ = MUST_RELOAD;
}

// Cached
void CplexInterface::SetObjectiveOffset(double value) {
  sync_status_ = MUST_RELOAD;
}

void CplexInterface::ClearObjective() {
  InvalidateSolutionSynchronization();
  int status;
  const int total_num_vars = solver_->variables_.size();
  std::unique_ptr<int[]> col_indices(new int[total_num_vars + 1]);
  std::unique_ptr<double[]> coefs(new double[total_num_vars + 1]);
  int i = 0;
  for (const auto& it : solver_->objective_->coefficients_) {
    const int var_index = it.first->index();
    // Variable may have not been extracted yet.
    if (var_index == kNoIndex) {
      DCHECK_NE(MODEL_SYNCHRONIZED, sync_status_);
    } else {
      col_indices[i] = var_index;
      assert(var_index < CPXgetnumcols(_cplexenv, _cplex));
      coefs[i] = 0;
      ++i;
    }
  }
  // // Constant term.
  // col_indices[i] = -1;
  // coefs[i] = 0.0;
  // ++i;
  // assert (i <= total_num_vars+1);

  if (i > 0) {
    status = CPXchgobj(_cplexenv, _cplex, i, col_indices.get(), coefs.get());
    assert(status == 0);
  }
}

MPSolverInterface* BuildCplexInterface(bool mip, MPSolver* const solver) {
  return new CplexInterface(solver, mip);
}

// ------ Query statistics on the solution and the solve ------

int64 CplexInterface::iterations() const {
  int iter;
  CheckSolutionIsSynchronized();
  if (mip_) {
    iter = CPXgetmipitcnt(_cplexenv, _cplex);
  } else {
    iter = CPXgetitcnt(_cplexenv, _cplex);
  }
  return static_cast<int64>(iter);
}

int64 CplexInterface::nodes() const {
  if (mip_) {
    CheckSolutionIsSynchronized();
    int nodes;
    nodes = CPXgetnodecnt(_cplexenv, _cplex);
    return static_cast<int64>(nodes);
  } else {
    LOG(FATAL) << "Number of nodes only available for discrete problems";
    return kUnknownNumberOfNodes;
  }
}

// Returns the best objective bound. Only available for discrete problems.
double CplexInterface::best_objective_bound() const {
  if (mip_) {
    CheckSolutionIsSynchronized();
    CheckBestObjectiveBoundExists();
    if (solver_->variables_.size() == 0 && solver_->constraints_.size() == 0) {
      // Special case for empty model.
      return solver_->Objective().offset();
    } else {
      int status;
      double value;
      status = CPXgetbestobjval(_cplexenv, _cplex, &value);
      assert(status == 0);
      return value;
    }
  } else {
    LOG(FATAL) << "Best objective bound only available for discrete problems";
    return 0.0;
  }

}

MPSolver::BasisStatus CplexInterface::TransformCPLEXBasisStatus(
    int cplex_basis_status) const {
  switch (cplex_basis_status) {
    case CPX_AT_LOWER:
      return MPSolver::AT_LOWER_BOUND;
    case CPX_BASIC:
      return MPSolver::BASIC;
    case CPX_AT_UPPER:
      return MPSolver::AT_UPPER_BOUND;
    case CPX_FREE_SUPER:
      return MPSolver::FREE;
    default:
      LOG(FATAL) << "Unknown CPLEX basis status";
      return MPSolver::FREE;
  }
}

// Returns the basis status of a row.
MPSolver::BasisStatus CplexInterface::row_status(int constraint_index) const {
  int solnmethod, solntype;
  int status;
  status = CPXsolninfo(_cplexenv, _cplex, &solnmethod, &solntype, NULL, NULL);
  assert(status == 0);
  if (solntype == CPX_BASIC_SOLN) {
    int nbRows = CPXgetnumrows(_cplexenv, _cplex);
    std::unique_ptr<int[]> rstat(new int[nbRows]);
    status = CPXgetbase(_cplexenv, _cplex, NULL, rstat.get());
    assert(status == 0);
    int cplex_basis_status = rstat[constraint_index];
    return TransformCPLEXBasisStatus(cplex_basis_status);
  }
  LOG(FATAL) << "Basis status only available for continuous problems";
  return MPSolver::FREE;
}

// Returns the basis status of a column.
MPSolver::BasisStatus CplexInterface::column_status(int variable_index) const {
  int solnmethod, solntype;
  int status;
  status = CPXsolninfo(_cplexenv, _cplex, &solnmethod, &solntype, NULL, NULL);
  assert(status == 0);
  if (solntype == CPX_BASIC_SOLN) {
    int nbColumns = CPXgetnumcols(_cplexenv, _cplex);
    std::unique_ptr<int[]> cstat(new int[nbColumns]);
    status = CPXgetbase(_cplexenv, _cplex, cstat.get(), NULL);
    assert(status == 0);
    int cplex_basis_status = cstat[variable_index];
    return TransformCPLEXBasisStatus(cplex_basis_status);
  }
  LOG(FATAL) << "Basis status only available for continuous problems";
  return MPSolver::FREE;
}

typedef char* charptr;

// Extracts the variables that have not been extracted yet
void CplexInterface::ExtractNewVariables() {
  int status;
  int total_num_vars = solver_->variables_.size();
  if (total_num_vars > last_variable_index_) {
    int nb_new_variables = total_num_vars - last_variable_index_;
    std::unique_ptr<double[]> lb(new double[nb_new_variables]);
    std::unique_ptr<double[]> ub(new double[nb_new_variables]);
    std::unique_ptr<char[]> ctype(new char[nb_new_variables]);
    std::unique_ptr<const char * []> colname(
        new const char* [nb_new_variables]);

    int j;
    for (j = 0; j < nb_new_variables; ++j) {
      MPVariable* const var = solver_->variables_[last_variable_index_ + j];
      var->set_index(last_variable_index_ + j);
      lb[j] = var->lb();
      ub[j] = var->ub();
      ctype[j] = var->integer() ? 'I' : 'C';
      colname[j] = var->name().empty() ? NULL : var->name().c_str();
    }

    assert(j == nb_new_variables);

    if (mip_) {
      status =
          CPXnewcols(_cplexenv, _cplex, nb_new_variables, NULL, lb.get(),
                     ub.get(), ctype.get(), const_cast<char**>(colname.get()));
      assert(status == 0);
    } else {
      status = CPXnewcols(_cplexenv, _cplex, nb_new_variables, NULL, lb.get(),
                          ub.get(), NULL, const_cast<char**>(colname.get()));
      assert(status == 0);
    }

    // Add new variables to the existing constraints.
    ExtractOldConstraints();
  }
}

// Extract again existing constraints if they contain new variables.
void CplexInterface::ExtractOldConstraints() {
  for (int i = 0; i < last_constraint_index_; ++i) {
    MPConstraint* const ct = solver_->constraints_[i];
    DCHECK_NE(kNoIndex, ct->index());
    const int size = ct->coefficients_.size();
    if (size == 0) {
      continue;
    }
    // Update the constraint's coefficients if it contains new variables.
    if (ct->ContainsNewVariables()) {
      ExtractOneConstraint(ct);
    }
  }
}

// Extract one constraint. Arrays indices and coefs must be
// preallocated to have enough space to contain the constraint's
// coefficients.
void CplexInterface::ExtractOneConstraint(MPConstraint* const constraint) {
  int max_constraint_size =
      solver_->ComputeMaxConstraintSize(0, last_constraint_index_);

  std::unique_ptr<int[]> row_indices(new int[max_constraint_size]);
  std::unique_ptr<int[]> col_indices(new int[max_constraint_size]);
  std::unique_ptr<double[]> coefs(new double[max_constraint_size]);

  int k = 0;
  for (const auto& it : constraint->coefficients_) {
    const int var_index = it.first->index();
    DCHECK_NE(kNoIndex, var_index);
    row_indices[k] = constraint->index();
    col_indices[k] = var_index;
    coefs[k] = it.second;
    ++k;
  }
  assert(k <= max_constraint_size);
  int status;
  status = CPXchgcoeflist(_cplexenv, _cplex, k, row_indices.get(),
                          col_indices.get(), coefs.get());
  assert(status == 0);
}

void CplexInterface::ExtractNewConstraints() {
  int status;
  int total_num_rows = solver_->constraints_.size();
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
    std::unique_ptr<int[]> row_indices(new int[max_row_length]);
    std::unique_ptr<int[]> col_indices(new int[max_row_length]);
    std::unique_ptr<double[]> coefs(new double[max_row_length]);

    // Add each new constraint.
    for (int i = last_constraint_index_; i < total_num_rows; ++i) {
      MPConstraint* const ct = solver_->constraints_[i];
      DCHECK_NE(kNoIndex, ct->index());
      int size = ct->coefficients_.size();
      int j = 0;
      for (const auto& it : ct->coefficients_) {
        const int index = it.first->index();
        DCHECK_NE(kNoIndex, index);
        row_indices[j] = ct->index();
        col_indices[j] = index;
        coefs[j] = it.second;
        j++;
      }
      char sense = 'R';
      double range = ct->ub() - ct->lb();
      double rhs = ct->lb();
      const double infinity = solver_->infinity();
      // const double infinity = CPX_INFBOUND;
      if ((ct->lb() > -infinity) && (ct->ub() < infinity)) {
        sense = 'R';
        rhs = ct->lb();
        range = ct->ub() - ct->lb();
      } else if (ct->ub() == ct->lb()) {
        sense = 'E';
        rhs = ct->lb();
        range = 0.;
      } else if (ct->ub() < infinity) {
        sense = 'L';
        rhs = ct->ub();
        range = 0.;
      } else if (ct->lb() > -infinity) {
        sense = 'G';
        rhs = ct->lb();
        range = 0;
      } else {
        sense = 'R';
        rhs = 0.;
        range = 0.;
      }
      char* name =
          ct->name().empty() ? NULL : const_cast<char*>(ct->name().c_str());
      status = CPXnewrows(_cplexenv, _cplex, 1, &rhs, &sense, &range, &name);
      assert(status == 0);
      status = CPXchgcoeflist(_cplexenv, _cplex, size, row_indices.get(),
                              col_indices.get(), coefs.get());
      assert(status == 0);
    }
  }
}

void CplexInterface::ExtractObjective() {
  // Linear objective: set objective coefficients for all variables
  // (some might have been modified).
  int status;
  int total_num_vars = solver_->variables_.size();
  std::unique_ptr<int[]> col_indices(new int[total_num_vars + 1]);
  std::unique_ptr<double[]> coefs(new double[total_num_vars + 1]);
  int i = 0;
  for (hash_map<const MPVariable*, double>::const_iterator
           it = solver_->objective_->coefficients_.begin();
       it != solver_->objective_->coefficients_.end(); ++it, ++i) {
    const int var_index = it->first->index();
    col_indices[i] = var_index;
    assert(var_index < CPXgetnumcols(_cplexenv, _cplex));
    coefs[i] = it->second;
  }

  // // Constant term.
  // col_indices[i] = -1;
  // coefs[i] = solver_->Objective().offset();
  // ++i;
  // assert (i <= total_num_vars+1);

  if (i > 0) {
    status = CPXchgobj(_cplexenv, _cplex, i, col_indices.get(), coefs.get());
    assert(status == 0);
  }
}

// ------ Parameters  -----

void CplexInterface::SetParameters(const MPSolverParameters& param) {
  SetCommonParameters(param);
  if (mip_) {
    SetMIPParameters(param);
  }
}

void CplexInterface::SetRelativeMipGap(double value) {
  if (mip_) {
    int status;
    status = CPXsetdblparam(_cplexenv, CPX_PARAM_EPGAP, value);
    assert(status == 0);
  } else {
    LOG(WARNING) << "The relative MIP gap is only available "
                 << "for discrete problems.";
  }
}

void CplexInterface::SetPrimalTolerance(double value) {
  // Is it the feasibility tolerance? CPX_PARAM_EPRHS?
}

void CplexInterface::SetDualTolerance(double value) {
  // Is it the optimality tolerance? CPX_PARAM_EPOPT?
}

void CplexInterface::SetPresolveMode(int value) {
  int status;
  switch (value) {
    case MPSolverParameters::PRESOLVE_OFF:
      status = CPXsetintparam(_cplexenv, CPX_PARAM_PREIND, CPX_OFF);
      assert(status == 0);
      break;
    case MPSolverParameters::PRESOLVE_ON:
      status = CPXsetintparam(_cplexenv, CPX_PARAM_PREIND, CPX_ON);
      assert(status == 0);
      break;
    default:
      SetIntegerParamToUnsupportedValue(MPSolverParameters::PRESOLVE, value);
  }
}

// Sets the scaling mode.
void CplexInterface::SetScalingMode(int value) {
  SetUnsupportedIntegerParam(MPSolverParameters::SCALING);
}

// Sets the LP algorithm : primal, dual or barrier. Note that CPLEX offers other
// LP algorithm (e.g. network) and automatic selection
void CplexInterface::SetLpAlgorithm(int value) {
  int status;
  switch (value) {
    case MPSolverParameters::DUAL:
      status = CPXsetintparam(_cplexenv, CPX_PARAM_LPMETHOD, CPX_ALG_DUAL);
      break;
    case MPSolverParameters::PRIMAL:
      status = CPXsetintparam(_cplexenv, CPX_PARAM_LPMETHOD, CPX_ALG_PRIMAL);
      break;
    case MPSolverParameters::BARRIER:
      status = CPXsetintparam(_cplexenv, CPX_PARAM_LPMETHOD, CPX_ALG_BARRIER);
      break;
    default:
      SetIntegerParamToUnsupportedValue(MPSolverParameters::LP_ALGORITHM,
                                        value);
  }
}

MPSolver::ResultStatus CplexInterface::Solve(const MPSolverParameters& param) {
  int status;

  WallTimer timer;
  timer.Start();

  if (param.GetIntegerParam(MPSolverParameters::INCREMENTALITY) ==
      MPSolverParameters::INCREMENTALITY_OFF) {
    Reset();
  }

  // Set log level.
  if (quiet_) {
    status = CPXsetintparam(_cplexenv, CPX_PARAM_SCRIND, CPX_OFF);
    assert(status == 0);
  } else {
    status = CPXsetintparam(_cplexenv, CPX_PARAM_SCRIND, CPX_ON);
    assert(status == 0);
  }

  ExtractModel();

  VLOG(1) << StringPrintf("Model build in %.3f seconds.", timer.Get());

  // WriteModelToPredefinedFiles();

  string filename = solver_->name_ + ".lp";
  WriteModel(filename);

  // Time limit.
  if (solver_->time_limit()) {
    VLOG(1) << "Setting time limit = " << solver_->time_limit() << " ms.";
    status = CPXsetdblparam(_cplexenv, CPX_PARAM_TILIM,
                            solver_->time_limit() / 1000.0);
    assert(status == 0);
  }

  // Solve
  timer.Restart();
  if (mip_) {
    status = CPXmipopt(_cplexenv, _cplex);
  } else {
    status = CPXlpopt(_cplexenv, _cplex);
  }

  if (status) {
    fprintf(stderr, "Failed to optimize MIP.\n");
  } else {
    VLOG(1) << StringPrintf("Solved in %.3f seconds.", timer.Get());
  }

  // Get the results.
  int total_num_rows = solver_->constraints_.size();
  int total_num_cols = solver_->variables_.size();
  int lpstat;
  std::unique_ptr<double[]> values(new double[total_num_cols]);
  std::unique_ptr<double[]> dual_values(new double[total_num_rows]);
  std::unique_ptr<double[]> slacks(new double[total_num_rows]);
  std::unique_ptr<double[]> reduced_costs(new double[total_num_cols]);
  if (mip_) {
    lpstat = CPXgetstat(_cplexenv, _cplex);
    if (lpstat > 0) {
      status = CPXgetobjval(_cplexenv, _cplex, &objective_value_);
      assert(status == 0);
      status = CPXgetx(_cplexenv, _cplex, values.get(), 0, total_num_cols - 1);
      assert(status == 0);
      status =
          CPXgetslack(_cplexenv, _cplex, slacks.get(), 0, total_num_rows - 1);
      assert(status == 0);
    }
  } else {
    status =
        CPXsolution(_cplexenv, _cplex, &lpstat, &objective_value_, values.get(),
                    dual_values.get(), slacks.get(), reduced_costs.get());
  }

  assert(status == 0);

  bool solutionFound = false;
  if (status == 0) {
    VLOG(1) << "objective=" << objective_value_;
    solutionFound = true;
    int total_num_vars = solver_->variables_.size();
    for (int i = 0; i < solver_->variables_.size(); ++i) {
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
      const double infinity = solver_->infinity();
      // const double infinity = CPX_INFBOUND;
      if (ct->lb() > -infinity && ct->ub() < infinity) {
        // not sure : should we use lb or ub?
        activity = ct->ub() - slacks[i];
      } else if (ct->lb() > -infinity) {
        activity = ct->lb() + slacks[i];
      } else if (ct->ub() < infinity) {
        activity = ct->ub() - slacks[i];
      }
      ct->set_activity(activity);
      if (mip_) {
        VLOG(4) << "row " << ct->index() << ": activity = " << slacks[i];
      } else {
        ct->set_dual_value(dual_values[i]);
        VLOG(4) << "row " << ct->index() << ": activity = " << slacks[i]
                << ": dual value = " << dual_values[i];
      }
    }
  }

  if (lpstat > 0) {
    std::unique_ptr<char[]> statstr(new char[CPXMESSAGEBUFSIZE + 1]);
    CPXgetstatstring(_cplexenv, lpstat, statstr.get());
    VLOG(1)
        << StringPrintf("Solution status %d (%s).\n", status, statstr.get());
  } else
    VLOG(1) << StringPrintf("Solution status %d.\n", status);

  switch (lpstat) {
    case CPX_STAT_OPTIMAL:
    case CPXMIP_OPTIMAL:
      result_status_ = MPSolver::OPTIMAL;
      break;
    case CPXMIP_OPTIMAL_TOL:
      // To be consistent with the other solvers.
      result_status_ = MPSolver::OPTIMAL;
      break;
    case CPX_STAT_INFEASIBLE:
    case CPXMIP_INFEASIBLE:
      result_status_ = MPSolver::INFEASIBLE;
      break;
    case CPX_STAT_UNBOUNDED:
    case CPXMIP_UNBOUNDED:
      result_status_ = MPSolver::UNBOUNDED;
      break;
    case CPX_STAT_INForUNBD:
    case CPXMIP_INForUNBD:
      result_status_ = MPSolver::INFEASIBLE;
      break;
    default:
      if (solutionFound) {
        result_status_ = MPSolver::FEASIBLE;
      } else {
        result_status_ = MPSolver::ABNORMAL;
      }
      break;
  }

  sync_status_ = SOLUTION_SYNCHRONIZED;
  return result_status_;
}

}       // namespace operations_research
#endif  // #if defined(USE_CPLEX)
