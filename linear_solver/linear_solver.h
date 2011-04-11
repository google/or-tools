// Copyright 2010-2011 Google
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
//                                                      (Laurent Perron).
//
// A C++ wrapper for the GNU linear programming / mixed integer programming kit,
// Coin LP Solver, and Coin Branch and Cut Solver.

// -----------------------------------
// What is Linear Programming ?
//
//   In mathematics, linear programming (LP) is a technique for optimization of
//   a linear objective function, subject to linear equality and linear
//   inequality constraints. Informally, linear programming determines the way
//   to achieve the best outcome (such as maximum profit or lowest cost) in a
//   given mathematical model and given some list of requirements represented
//   as linear equations. The linear programming problem was first shown to be
//   solvable in polynomial time by Leonid Khachiyan in 1979, but a larger
//   theoretical and practical breakthrough in the field came in 1984 when
//   Narendra Karmarkar introduced a new interior point method for solving
//   linear programming problems.
//
// -----------------------------------
// What is Mixed Integer Programming ?
//
//   If only some of the unknown variables are required to be integers, then
//   the problem is called a mixed integer programming (MIP) problem. These are
//   generally also NP-hard.
//
// -----------------------------------
// Check Wikipedia for more detail:
//
//   http://en.wikipedia.org/wiki/Linear_programming
//
// -----------------------------------
// Example of a Linear Programming:
//
//   mimimize:
//     f1*x1+f2*x2+...fn*xn
//   subject to:
//     a1*x1+a2*x2+...an*xn >= k1
//     b1*x1+b2*x2+...bn*xn <= k2
//     c1*x1+c2*x2+...cn*xn =  k3
//     ......
//     u1 <= x1 <= v1
//     u2 <= x2 <= v2
//     .....
//
//  As can be seen, Linear Programming has:
//    1) linear objective function
//    2) linear constraint
//
//  Note:
//    The objective function is linear and convex.
//    The constraints form a convex space if feasible.
//
// -----------------------------------

#ifndef LINEAR_SOLVER_LINEAR_SOLVER_H_
#define LINEAR_SOLVER_LINEAR_SOLVER_H_


#include <limits>
#include <string>
#include <vector>

#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/scoped_ptr.h"
#include "base/timer.h"
#include "base/strutil.h"
#include "base/sparsetable.h"
#include "linear_solver/linear_solver.pb.h"

namespace operations_research {

class MPModelProto;
class MPSolverInterface;
class MPSolverParameters;

// A class to express a variable that will appear in a constraint.
class MPVariable {
 public:
  const string& name() const { return name_; }

  void SetInteger(bool integer);
  bool integer() const { return integer_; }

  double solution_value() const;
  // Only available for continuous problems.
  double reduced_cost() const;

  int index() const { return index_; }

  double lb() const { return lb_; }
  double ub() const { return ub_; }
  void SetLB(double lb) { SetBounds(lb, ub_); }
  void SetUB(double ub) { SetBounds(lb_, ub); }
  void SetBounds(double lb, double ub);
 protected:
  friend class MPSolver;
  friend class MPSolverInterface;
  friend class CBCInterface;
  friend class CLPInterface;
  friend class GLPKInterface;

  MPVariable(double lb, double ub, bool integer, const string& name,
             MPSolverInterface* const interface)
      : lb_(lb), ub_(ub), integer_(integer), name_(name), index_(-1),
        solution_value_(0.0), reduced_cost_(0.0), interface_(interface) {}

  void set_index(int index) { index_ = index; }
  void set_solution_value(double value) { solution_value_ = value; }
  void set_reduced_cost(double reduced_cost) { reduced_cost_ = reduced_cost; }
 private:
  double lb_;
  double ub_;
  bool integer_;
  const string name_;
  int index_;
  double solution_value_;
  double reduced_cost_;
  MPSolverInterface* const interface_;
  DISALLOW_COPY_AND_ASSIGN(MPVariable);
};

// A class to express constraints for a linear programming problem. A
// constraint is represented as a linear equation/inequality.
class MPConstraint {
 public:
  const string& name() const { return name_; }

  // Clears all variables and coefficients.
  void Clear();

  // Add (var * coeff) to the current constraint.
  void AddTerm(MPVariable* const var, double coeff);
  // Add var to the current constraint.
  void AddTerm(MPVariable* const var);
  // Set the coefficient of the variable on the constraint.
  void SetCoefficient(MPVariable* const var, double coeff);

  double lb() const { return lb_; }
  double ub() const { return ub_; }
  void SetLB(double lb) { SetBounds(lb, ub_); }
  void SetUB(double ub) { SetBounds(lb_, ub); }
  void SetBounds(double lb, double ub);

  // Only available for continuous problems.
  double dual_value() const;

  int index() const { return index_; }
 protected:
  friend class MPSolver;
  friend class MPSolverInterface;
  friend class CBCInterface;
  friend class CLPInterface;
  friend class GLPKInterface;

  // Creates a constraint and updates the pointer to its MPSolverInterface.
  MPConstraint(double lb,
               double ub,
               const string& name,
               MPSolverInterface* const interface)
      : lb_(lb), ub_(ub), name_(name), index_(-1), dual_value_(0.0),
        interface_(interface) {}

  void set_index(int index) { index_ = index; }
  void set_dual_value(double dual_value) { dual_value_ = dual_value; }
 private:
  // Returns true if the constraint contains variables that have not
  // been extracted yet.
  bool ContainsNewVariables();

  // Mapping var -> coefficient.
  hash_map<MPVariable*, double> coefficients_;

  // The lower bound for the linear constraint.
  double lb_;
  // The upper bound for the linear constraint.
  double ub_;
  // Name.
  const string name_;
  int index_;
  double dual_value_;
  MPSolverInterface* const interface_;
  DISALLOW_COPY_AND_ASSIGN(MPConstraint);
};

// A class to express a linear objective function
class MPObjective {
 public:
  // Clears all variables and coefficients.
  void Clear();

  // Add (var * coeff) to the objective
  void AddTerm(MPVariable* const var, double coeff);
  // Add var to the objective
  void AddTerm(MPVariable* const var);
  // Set the coefficient of the variable in the objective
  void SetCoefficient(MPVariable* const var, double coeff);

  // Add constant term to the objective.
  void AddOffset(double value);
  // Set constant term in the objective.
  void SetOffset(double value);

 private:
  friend class MPSolver;
  friend class MPSolverInterface;
  friend class CBCInterface;
  friend class CLPInterface;
  friend class GLPKInterface;

  // Creates an objective and updates the pointer to its parent 'MPSolver'.
  explicit MPObjective(MPSolverInterface* const interface)
      : offset_(0.0), interface_(interface) {}

  // Mapping var -> coefficient.
  hash_map<MPVariable*, double> coefficients_;
  // Constant term.
  double offset_;

  MPSolverInterface* const interface_;
  DISALLOW_COPY_AND_ASSIGN(MPObjective);
};

class MPSolver {
 public:

  // The LP/MIP problem type.
  enum OptimizationProblemType {
#if defined(USE_GLPK)
    GLPK_LINEAR_PROGRAMMING,
    GLPK_MIXED_INTEGER_PROGRAMMING,
#endif
#if defined(USE_CLP)
    CLP_LINEAR_PROGRAMMING,
#endif
#if defined(USE_CBC)
    CBC_MIXED_INTEGER_PROGRAMMING,
#endif
  };

  enum ResultStatus {
    OPTIMAL,     // optimal
    FEASIBLE,    // feasible, or stopped by limit.
    INFEASIBLE,  // proven infeasible
    UNBOUNDED,   // unbounded
    ABNORMAL,    // abnormal, i.e., error of some kind.
    NOT_SOLVED   // not been solved yet.
  };

  enum LoadStatus {
    NO_ERROR,
    DUPLICATE_VARIABLE_ID,
    UNKNOWN_VARIABLE_ID
  };

  // Constructor that takes a name for the underlying solver.
  MPSolver(const string& name, OptimizationProblemType problem_type);
  virtual ~MPSolver();

  // ----- Load model from protobuf -----
#ifndef SWIG
  // TODO(user): fix swig support.
  LoadStatus Load(const MPModelProto& model);
#endif

  // ----- Init and Clear -----
  void Init() {}  // To remove.
  void Clear();

  // ----- Variables ------
  // Returns the number of variables.
  int NumVariables() const { return variables_.size(); }
  const vector<MPVariable*>& variables() const { return variables_; }

  // Create a variable with the given bounds.
  MPVariable* MakeVar(double lb, double ub, bool integer, const string& name);
  MPVariable* MakeNumVar(double lb, double ub, const string& name);
  MPVariable* MakeIntVar(double lb, double ub, const string& name);
  MPVariable* MakeBoolVar(const string& name);

  void MakeVarArray(int nb,
                    double lb,
                    double ub,
                    bool integer,
                    const string& name,
                    vector<MPVariable*>* vars);
  void MakeNumVarArray(int nb,
                       double lb,
                       double ub,
                       const string& name,
                       vector<MPVariable*>* vars);
  void MakeIntVarArray(int nb,
                       double lb,
                       double ub,
                       const string& name,
                       vector<MPVariable*>* vars);
  void MakeBoolVarArray(int nb,
                        const string& name,
                        vector<MPVariable*>* vars);

  // ----- Constraints -----
  // Returns the number of constraints.
  int NumConstraints() const { return constraints_.size(); }

  // Returns a pointer to a newly created constraint for the linear programming
  // problem. The MPSolver class assumes ownership of the constraint.
  MPConstraint* MakeRowConstraint(double lb, double ub);
  MPConstraint* MakeRowConstraint();
  MPConstraint* MakeRowConstraint(double lb, double ub, const string& name);
  MPConstraint* MakeRowConstraint(const string& name);

  // ----- Objective -----

  // Return the objective value of the best solution found so far. It
  // is the optimal objective value if the problem has been solved to
  // optimality.
  double objective_value() const;

  // Returns the best objective bound. In case of minimization, it is
  // a lower bound on the objective value of the optimal integer
  // solution. Only available for discrete problems.
  double best_objective_bound() const;

  // Clear objective.
  void ClearObjective();
  // Add var * coeff to the objective function.
  void AddObjectiveTerm(MPVariable* const var, double coeff);
  // Add var to the objective function.
  void AddObjectiveTerm(MPVariable* const var);
  // Set the objective coefficient of a variable in the objective.
  void SetObjectiveCoefficient(MPVariable* const var, double coeff);
  // Add constant term to the objective.
  void AddObjectiveOffset(double value);
  // Set constant term in the objective.
  void SetObjectiveOffset(double value);


  // Sets the optimization direction (min/max).
  void SetOptimizationDirection(bool maximize);
  // Minimizing or maximizing?
  bool Maximization();
  // Minimizing or maximizing?
  bool Minimization();

  // Set minimization mode.
  void SetMinimization() { SetOptimizationDirection(false); }
  // Set maximization mode.
  void SetMaximization() { SetOptimizationDirection(true); }

  // ----- Solve -----
  // Solve the problem using default parameter values.
  ResultStatus Solve();
  // Solve the problem using the parameter values specified.
  ResultStatus Solve(const MPSolverParameters& param);

  // Reset extracted model to solve from scratch
  void Reset();

  // Misc.
  static double infinity() {
    return std::numeric_limits<double>::infinity();
  }

  // SuppressOutput.
  void SuppressOutput();

  // Set the name of the file where the solver writes out the model
  void set_write_model_filename(const string &filename) {
    write_model_filename_ = filename;
  }

  string write_model_filename() const {
    return write_model_filename_;
  }

  // Return true if filename ends in ".lp"
  bool IsLPFormat(const string &filename) {
    return HasSuffixString (filename, ".lp");
  }

  // Set Time limit in ms. (0 = no limit).
  void set_time_limit(int64 time_limit) {
    DCHECK_GE(time_limit, 0);
    time_limit_ = time_limit;
  }

  int64 time_limit() const {
    return time_limit_;
  }

  // wall_time() in ms since the creation of the solver.
  int64 wall_time() const {
    return timer_.GetInMs();
  }

  // Number of simplex iterations
  int64 iterations() const;
  // Number of branch-and-bound nodes. Only available for discrete problems.
  int64 nodes() const;

  // Check validity of a name.
  bool CheckNameValidity(const string& name);
  // Check validity of all variables and constraints names.
  bool CheckAllNamesValidity();

  // return a string describing the engine used.
  string SolverVersion() const;

  friend class GLPKInterface;
  friend class CLPInterface;
  friend class CBCInterface;
  friend class MPSolverInterface;

 private:
  // Compute the size of the constraint with the largest number of
  // coefficients with index in [min_constraint_index,
  // max_constraint_index)
  int ComputeMaxConstraintSize(int min_constraint_index,
                               int max_constraint_index) const;

  // The name of the linear programming problem.
  const string name_;

  // The solver interface.
  scoped_ptr<MPSolverInterface> interface_;

  // vector of problem variables.
  vector<MPVariable*> variables_;
  hash_set<string> variables_names_;

  // The list of constraints for the problem.
  vector<MPConstraint*> constraints_;
  hash_set<string> constraints_names_;

  // The linear objective function
  MPObjective linear_objective_;


  // Time limit in ms.
  int64 time_limit_;

  // Name of the file where the solver writes out the model when Solve
  // is called. If empty, no file is written.
  string write_model_filename_;

  WallTimer timer_;

  DISALLOW_COPY_AND_ASSIGN(MPSolver);
};

// This class stores parameter settings for LP and MIP solvers.
// How to add a new parameter:
// - Add the new Foo parameter in the DoubleParam or IntegerParam enum.
// - If it is a categorical param, add a FooValues enum.
// - Decide if the wrapper should define a default value for it: yes
//   if it controls the properties of the solution (example:
//   tolerances) or if it consistently improves performance, no
//   otherwise. If yes, define kDefaultFoo.
// - Add a foo_value_ member and, if no default value is defined, a
//   foo_is_default_ member.
// - Add code to handle Foo in Set...Param, Reset...Param,
//   Get...Param, Reset and the constructor.
// - In class MPSolverInterface, add a virtual method SetFoo and
//   implement it for each solver.
// - Add a test in linear_solver_test.cc.
class MPSolverParameters {
 public:
  // Enumeration of parameters that take continuous values.
  enum DoubleParam {
    RELATIVE_MIP_GAP = 0,        // Limit for relative MIP gap.
  };

  // Enumeration of parameters that take integer or categorical values.
  enum IntegerParam {
    PRESOLVE = 1000,        // Presolve mode.
    LP_ALGORITHM = 1001     // Algorithm to solve linear programs.
  };

  // For each categorical parameter, enumeration of possible values.
  enum PresolveValues {
    PRESOLVE_OFF = 0,  // Presolve is off.
    PRESOLVE_ON = 1    // Presolve is on.
  };

  enum LpAlgorithmValues {
    DUAL = 10,      // Dual simplex.
    PRIMAL = 11,    // Primal simplex.
    BARRIER = 12    // Barrier algorithm.
  };

  // Values to indicate that a parameter is set to the solver's
  // default value.
  static const double kDefaultDoubleParamValue;
  static const int kDefaultIntegerParamValue;
  // Values to indicate that a parameter is unknown.
  static const double kUnknownDoubleParamValue;
  static const int kUnknownIntegerParamValue;

  // Default values for parameters. Only parameters that define the
  // properties of the solution returned need to have a default value
  // (that is the same for all solvers). You can also define a default
  // value for performance parameters when you are confident it is a
  // good choice (example: always turn presolve on).
  static const double kDefaultRelativeMipGap;
  static const PresolveValues kDefaultPresolve;

  // The constructor sets all parameters to their default value.
  MPSolverParameters();

  // Set parameter to a specific value.
  void SetDoubleParam(MPSolverParameters::DoubleParam param, double value);
  void SetIntegerParam(MPSolverParameters::IntegerParam param, int value);
  // Reset parameter to the default value.
  void ResetDoubleParam(MPSolverParameters::DoubleParam param);
  void ResetIntegerParam(MPSolverParameters::IntegerParam param);
  // Set all parameters to their default value.
  void Reset();
  // Get parameter's value.
  double GetDoubleParam(MPSolverParameters::DoubleParam param) const;
  int GetIntegerParam(MPSolverParameters::IntegerParam param) const;

 private:
  // Parameter value for each parameter.
  double relative_mip_gap_value_;
  int presolve_value_;
  int lp_algorithm_value_;
  // Boolean value indicating whether each parameter is set to the
  // solver's default value. Only parameters for which the wrapper
  // does not define a default value need such an indicator.
  bool lp_algorithm_is_default_;

  DISALLOW_COPY_AND_ASSIGN(MPSolverParameters);
};

// This class serves as a proxy to open sources linear solver.
class MPSolverInterface {
 public:
  enum SynchronizationStatus {
    // The underlying solver (CLP, GLPK, ...) and MPSolver are not in
    // sync for the model nor for the solution.
    MUST_RELOAD,
    // The underlying solver and MPSolver are in sync for the model
    // but not for the solution: the model has changed since the
    // solution was computed last.
    MODEL_SYNCHRONIZED,
    // The underlying solver and MPSolver are in sync for the model and
    // the solution.
    SOLUTION_SYNCHRONIZED
  };

  // When the underlying solver does not provide the number of simplex
  // iterations.
  static const int64 kUnknownNumberOfIterations;
  // When the underlying solver does not provide the number of simplex
  // nodes.
  static const int64 kUnknownNumberOfNodes;
  // When the index of a variable or constraint has not been assigned yet.
  static const int kNoIndex;

  // Constructor that takes a name for the underlying glpk solver.
  explicit MPSolverInterface(MPSolver* const solver);
  virtual ~MPSolverInterface();

  // ----- Solve -----
  // Solves problem with default parameter values. Returns true if the
  // solution is optimal. Calls WriteModelToPredefinedFiles as a
  // temporary solution to allow the user to write the model to a
  // file.
  MPSolver::ResultStatus Solve();
  // Same as Solve(), except it uses the specified parameter values.
  virtual MPSolver::ResultStatus Solve(const MPSolverParameters& param) = 0;

  // ----- Model modifications and extraction -----
  // Resets extracted model
  virtual void Reset() = 0;

  // Sets the optimization direction (min/max).
  virtual void SetOptimizationDirection(bool minimize) = 0;

  // Modify bounds of an extracted variable.
  virtual void SetVariableBounds(int index, double lb, double ub) = 0;

  // Modify integrality of an extracted variable.
  virtual void SetVariableInteger(int index, bool integer) = 0;

  // Modify bounds of an extracted variable.
  virtual void SetConstraintBounds(int index, double lb, double ub) = 0;

  // Add a constraint.
  virtual void AddRowConstraint(MPConstraint* const ct) = 0;

  // Add a variable.
  virtual void AddVariable(MPVariable* const var) = 0;

  // Change a coefficient in a constraint.
  virtual void SetCoefficient(MPConstraint* const constraint,
                              MPVariable* const variable,
                              double coefficient) = 0;

  // Clear a constraint from all its terms.
  virtual void ClearConstraint(MPConstraint* const constraint) = 0;

  // Change a coefficient in the linear objective.
  virtual void SetObjectiveCoefficient(MPVariable* const variable,
                                       double coefficient) = 0;

  // Change the constant term in the linear objective.
  virtual void SetObjectiveOffset(double value) = 0;

  // Clear the objective from all its terms.
  virtual void ClearObjective() = 0;

  // ------ Query statistics on the solution and the solve ------
  // Number of simplex iterations
  virtual int64 iterations() const = 0;
  // Number of branch-and-bound nodes
  virtual int64 nodes() const = 0;
  // Best objective bound. Only available for discrete problems.
  virtual double best_objective_bound() const = 0;
  // Objective value of the best solution found so far.
  double objective_value() const;

  // Checks whether the solution is synchronized with the model,
  // i.e. whether the model has changed since the solution was
  // computed last.
  void CheckSolutionIsSynchronized() const;
  // Checks whether a feasible solution exists.
  virtual void CheckSolutionExists() const;
  // Checks whether information on the best objective bound exists.
  virtual void CheckBestObjectiveBoundExists() const;

  // ----- Misc -----
  // Write model to file.
  virtual void WriteModel(const string& filename) = 0;

  // SuppressOutput.
  virtual void SuppressOutput() = 0;

  // Query problem type. For simplicity, the distinction between
  // continuous and discrete is based on the declaration of the user
  // when the solver is created (example: GLPK_LINEAR_PROGRAMMING
  // vs. GLPK_MIXED_INTEGER_PROGRAMMING), not on the actual content of
  // the model.
  // Returns true if the problem is continuous.
  virtual bool IsContinuous() const = 0;
  // Returns true if the problem is continuous and linear.
  virtual bool IsLP() const = 0;
  // Returns true if the problem is discrete and linear.
  virtual bool IsMIP() const = 0;

  int last_variable_index() const {
    return last_variable_index_;
  }
  // Returns the result status of the last solve.
  MPSolver::ResultStatus result_status() const {
    CheckSolutionIsSynchronized();
    return result_status_;
  }
  // Returns a string describing the solver.
  virtual string SolverVersion() const = 0;

  friend class MPSolver;
 protected:
  MPSolver* const solver_;
  // Indicates whether the model and the solution are synchronized.
  SynchronizationStatus sync_status_;
  // Indicates whether the solve has reached optimality,
  // infeasibility, a limit, etc.
  MPSolver::ResultStatus result_status_;
  bool maximize_;

  // Index of last constraint extracted
  int last_constraint_index_;
  // Index of last variable extracted
  int last_variable_index_;

  // The value of the objective function.
  double objective_value_;

  // Index of dummy variable created for empty constraints or the
  // objective offset.
  static const int kDummyVariableIndex;

  // Writes out the model to a file specified by the
  // --solver_write_model command line argument or
  // MPSolver::set_write_model_filename.
  // The file is written by each solver interface (CBC, CLP, GLPK) and
  // each behaves a little differently.
  // If filename ends in ".lp", then the file is written in the
  // LP format (except for the CLP solver that does not support the LP
  // format). In all other cases it is written in the MPS format.
  void WriteModelToPredefinedFiles();

  // Extracts model stored in MPSolver
  void ExtractModel();
  virtual void ExtractNewVariables() = 0;
  virtual void ExtractNewConstraints() = 0;
  virtual void ExtractObjective() = 0;
  void ResetExtractionInformation();
  // Change synchronization status from SOLUTION_SYNCHRONIZED to
  // MODEL_SYNCHRONIZED. To be used for model changes.
  void InvalidateSolutionSynchronization();

  // Set parameters common to LP and MIP in the underlying solver.
  void SetCommonParameters(const MPSolverParameters& param);
  // Set MIP specific parameters in the underlying solver.
  void SetMIPParameters(const MPSolverParameters& param);
  // Set all parameters in the underlying solver.
  virtual void SetParameters(const MPSolverParameters& param) = 0;
  // Set an unsupported parameter.
  void SetUnsupportedDoubleParam(MPSolverParameters::DoubleParam param) const;
  void SetUnsupportedIntegerParam(MPSolverParameters::IntegerParam param) const;
  // Set a supported parameter to an unsupported value.
  void SetDoubleParamToUnsupportedValue(MPSolverParameters::DoubleParam param,
                                        int value) const;
  void SetIntegerParamToUnsupportedValue(MPSolverParameters::IntegerParam param,
                                        double value) const;
  // Set each parameter in the underlying solver.
  virtual void SetRelativeMipGap(double value) = 0;
  virtual void SetPresolveMode(int value) = 0;
  virtual void SetLpAlgorithm(int value) = 0;
};

}  // namespace operations_research

#endif  // LINEAR_SOLVER_LINEAR_SOLVER_H_
