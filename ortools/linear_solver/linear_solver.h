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
//
// A C++ wrapper that provides a simple and unified interface to
// several linear programming and mixed integer programming solvers:
// GLOP, GLPK, CLP, CBC, and SCIP. The wrapper can also be used in Java, C#,
// and Python via SWIG.
//
//
// -----------------------------------
//
// What is Linear Programming?
//
//   In mathematics, linear programming (LP) is a technique for optimization of
//   a linear objective function, subject to linear equality and linear
//   inequality constraints. Informally, linear programming determines the way
//   to achieve the best outcome (such as maximum profit or lowest cost) in a
//   given mathematical model and given some list of requirements represented
//   as linear equations.
//
//   The most widely used technique for solving a linear program is the Simplex
//   algorithm, devised by George Dantzig in 1947. It performs very well on
//   most instances, for which its running time is polynomial. A lot of effort
//   has been put into improving the algorithm and its implementation. As a
//   byproduct, it has however been shown that one can always construct
//   problems that take exponential time for the Simplex algorithm to solve.
//   Research has thus focused on trying to find a polynomial algorithm for
//   linear programming, or to prove that linear programming is indeed
//   polynomial.
//
//   Leonid Khachiyan first exhibited in 1979 a weakly polynomial algorithm for
//   linear programming. "Weakly polynomial" means that the running time of the
//   algorithm is in O(P(n) * 2^p) where P(n) is a polynomial of the size of the
//   problem, and p is the precision of computations expressed in number of
//   bits. With a fixed-precision, floating-point-based implementation, a weakly
//   polynomial algorithm will  thus run in polynomial time. No implementation
//   of Khachiyan's algorithm has proved efficient, but a larger breakthrough in
//   the field came in 1984 when Narendra Karmarkar introduced a new interior
//   point method for solving linear programming problems. Interior point
//   algorithms have proved efficient on very large linear programs.
//
//   Check Wikipedia for more detail:
//     http://en.wikipedia.org/wiki/Linear_programming
//
// -----------------------------------
//
// Example of a Linear Program
//
//   maximize:
//     3x + y
//   subject to:
//     1.5 x + 2 y <= 12
//     0 <= x <= 3
//     0 <= y <= 5
//
//  A linear program has:
//    1) a linear objective function
//    2) linear constraints that can be equalities or inequalities
//    3) bounds on variables that can be positive, negative, finite or
//       infinite.
//
// -----------------------------------
//
// What is Mixed Integer Programming?
//
//   Here, the constraints and the objective are still linear but
//   there are additional integrality requirements for variables. If
//   all variables are required to take integer values, then the
//   problem is called an integer program (IP). In most cases, only
//   some variables are required to be integer and the rest of the
//   variables are continuous: this is called a mixed integer program
//   (MIP). IPs and MIPs are generally NP-hard.
//
//   Integer variables can be used to model discrete decisions (build a
//   datacenter in city A or city B), logical relationships (only
//   place machines in datacenter A if we have decided to build
//   datacenter A) and approximate non-linear functions with piecewise
//   linear functions (for example, the cost of machines as a function
//   of how many machines are bought, or the latency of a server as a
//   function of its load).
//
// -----------------------------------
//
// How to use the wrapper
//
//   The user builds the model and solves it through the MPSolver class,
//   then queries the solution through the MPSolver, MPVariable and
//   MPConstraint classes. To be able to query a solution, you need the
//   following:
//   - A solution exists: MPSolver::Solve has been called and a solution
//     has been found.
//   - The model has not been modified since the last time
//     MPSolver::Solve was called. Otherwise, the solution obtained
//     before the model modification may not longer be feasible or
//     optimal.
//
// @see ../examples/linear_programming.cc for a simple LP example.
//
// @see ../examples/integer_programming.cc for a simple MIP example.
//
//   All methods cannot be called successfully in all cases. For
//   example: you cannot query a solution when no solution exists, you
//   cannot query a reduced cost value (which makes sense only on
//   continuous problems) on a discrete problem. When a method is
//   called in an unsuitable context, it aborts with a
//   LOG(FATAL).
// TODO(user): handle failures gracefully.
//
// -----------------------------------
//
// For developers: How the wrapper works
//
//   MPSolver stores a representation of the model (variables,
//   constraints and objective) in its own data structures and a
//   pointer to a MPSolverInterface that wraps the underlying solver
//   (GLOP, CBC, CLP, GLPK, or SCIP) that does the actual work. The
//   underlying solver also keeps a representation of the model in its
//   own data structures. The model representations in MPSolver and in
//   the underlying solver are kept in sync by the 'extraction'
//   mechanism: synchronously for some changes and asynchronously
//   (when MPSolver::Solve is called) for others. Synchronicity
//   depends on the modification applied and on the underlying solver.

#ifndef OR_TOOLS_LINEAR_SOLVER_LINEAR_SOLVER_H_
#define OR_TOOLS_LINEAR_SOLVER_LINEAR_SOLVER_H_

#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/timer.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/linear_solver/linear_expr.h"
#include "ortools/linear_solver/linear_solver.pb.h"


namespace operations_research {

class MPConstraint;
class MPObjective;
class MPSolverInterface;
class MPSolverParameters;
class MPVariable;

// This mathematical programming (MP) solver class is the main class
// though which users build and solve problems.
class MPSolver {
 public:
  // The type of problems (LP or MIP) that will be solved and the
  // underlying solver (GLOP, GLPK, CLP, CBC or SCIP) that will solve them.
  // This must remain consistent with MPModelRequest::OptimizationProblemType
  // (take particular care of the open-source version).
  enum OptimizationProblemType {
    // Linear programming problems.
    #ifdef USE_CLP
    CLP_LINEAR_PROGRAMMING = 0,  // Recommended default value.
    #endif
    #ifdef USE_GLPK
    GLPK_LINEAR_PROGRAMMING = 1,
    #endif
    #ifdef USE_GLOP
    GLOP_LINEAR_PROGRAMMING = 2,
    #endif
    #ifdef USE_GUROBI
    GUROBI_LINEAR_PROGRAMMING = 6,
    #endif
    #ifdef USE_CPLEX
    CPLEX_LINEAR_PROGRAMMING = 10,
    #endif

    // Integer programming problems.
    #ifdef USE_SCIP
    SCIP_MIXED_INTEGER_PROGRAMMING = 3,  // Recommended default value.
    #endif
    #ifdef USE_GLPK
    GLPK_MIXED_INTEGER_PROGRAMMING = 4,
    #endif
    #ifdef USE_CBC
    CBC_MIXED_INTEGER_PROGRAMMING = 5,
    #endif
    #if defined(USE_GUROBI)
    GUROBI_MIXED_INTEGER_PROGRAMMING = 7,
    #endif
    #if defined(USE_CPLEX)
    CPLEX_MIXED_INTEGER_PROGRAMMING = 11,
    #endif
    #if defined(USE_BOP)
    BOP_INTEGER_PROGRAMMING = 12,
    #endif
  };

  MPSolver(const std::string& name, OptimizationProblemType problem_type);
  virtual ~MPSolver();

  // Whether the given problem type is supported (this will depend on the
  // targets that you linked).
  static bool SupportsProblemType(OptimizationProblemType problem_type);

  bool IsMIP() const;

  std::string Name() const {
    return name_;  // Set at construction.
  }

  virtual OptimizationProblemType ProblemType() const {
    return problem_type_;  // Set at construction.
  }

  // Clears the objective (including the optimization direction), all
  // variables and constraints. All the other properties of the MPSolver
  // (like the time limit) are kept untouched.
  void Clear();

  // ----- Variables ------
  // Returns the number of variables.
  int NumVariables() const { return variables_.size(); }
  // Returns the array of variables handled by the MPSolver.
  // (They are listed in the order in which they were created.)
  const std::vector<MPVariable*>& variables() const { return variables_; }
  // Look up a variable by name, and return NULL if it does not exist.
  MPVariable* LookupVariableOrNull(const std::string& var_name) const;

  // Creates a variable with the given bounds, integrality requirement
  // and name. Bounds can be finite or +/- MPSolver::infinity().
  // The MPSolver owns the variable (i.e. the returned pointer is borrowed).
  // Variable names must be unique (it may crash otherwise). Empty variable
  // names are allowed, an automated variable name will then be assigned.
  MPVariable* MakeVar(double lb, double ub, bool integer, const std::string& name);
  // Creates a continuous variable.
  MPVariable* MakeNumVar(double lb, double ub, const std::string& name);
  // Creates an integer variable.
  MPVariable* MakeIntVar(double lb, double ub, const std::string& name);
  // Creates a boolean variable.
  MPVariable* MakeBoolVar(const std::string& name);

  // Creates an array of variables. All variables created have the
  // same bounds and integrality requirement. If nb <= 0, no variables are
  // created, the function crashes in non-opt mode.
  // @param name the prefix of the variable names. Variables are named
  // name0, name1, ...
  void MakeVarArray(int nb, double lb, double ub, bool integer,
                    const std::string& name_prefix, std::vector<MPVariable*>* vars);
  // Creates an array of continuous variables.
  void MakeNumVarArray(int nb, double lb, double ub, const std::string& name,
                       std::vector<MPVariable*>* vars);
  // Creates an array of integer variables.
  void MakeIntVarArray(int nb, double lb, double ub, const std::string& name,
                       std::vector<MPVariable*>* vars);
  // Creates an array of boolean variables.
  void MakeBoolVarArray(int nb, const std::string& name,
                        std::vector<MPVariable*>* vars);

  // ----- Constraints -----
  // Returns the number of constraints.
  int NumConstraints() const { return constraints_.size(); }
  // Returns the array of constraints handled by the MPSolver.
  // (They are listed in the order in which they were created.)
  const std::vector<MPConstraint*>& constraints() const { return constraints_; }
  // Look up a constraint by name, and return NULL if it does not exist.
  MPConstraint* LookupConstraintOrNull(const std::string& constraint_name) const;

  // Creates a linear constraint with given bounds. Bounds can be
  // finite or +/- MPSolver::infinity(). The MPSolver class assumes
  // ownership of the constraint.
  // @return a pointer to the newly created constraint.
  MPConstraint* MakeRowConstraint(double lb, double ub);
  // Creates a constraint with -infinity and +infinity bounds.
  MPConstraint* MakeRowConstraint();
  // Creates a named constraint with given bounds.
  MPConstraint* MakeRowConstraint(double lb, double ub, const std::string& name);
  // Creates a named constraint with -infinity and +infinity bounds.
  MPConstraint* MakeRowConstraint(const std::string& name);

  // Creates a constraint owned by MPSolver enforcing:
  // range.lower_bound() <= range.linear_expr() <= range.upper_bound()
  MPConstraint* MakeRowConstraint(const LinearRange& range);
  // As above, but also names the constraint.
  MPConstraint* MakeRowConstraint(const LinearRange& range, const std::string& name);

  // ----- Objective -----
  // Note that the objective is owned by the solver, and is initialized to
  // its default value (see the MPObjective class below) at construction.
  const MPObjective& Objective() const { return *objective_; }
  MPObjective* MutableObjective() { return objective_.get(); }

  // ----- Solve -----

  // The status of solving the problem. The straightforward translation to
  // homonymous enum values of MPSolutionResponse::Status
  // (see ./linear_solver.proto) is guaranteed by ./enum_consistency_test.cc,
  // you may rely on it.
  // TODO(user): Figure out once and for all what the status of
  // underlying solvers exactly mean, especially for feasible and
  // infeasible.
  enum ResultStatus {
    OPTIMAL,        // optimal.
    FEASIBLE,       // feasible, or stopped by limit.
    INFEASIBLE,     // proven infeasible.
    UNBOUNDED,      // proven unbounded.
    ABNORMAL,       // abnormal, i.e., error of some kind.
    MODEL_INVALID,  // the model is trivially invalid (NaN coefficients, etc).
    NOT_SOLVED = 6  // not been solved yet.
  };

  // Solves the problem using default parameter values.
  ResultStatus Solve();
  // Solves the problem using the specified parameter values.
  ResultStatus Solve(const MPSolverParameters& param);

  // Call only after calling MPSolver::Solve. Evaluates "linear_expr" for the
  // variable values at the solution found by solving.
  double SolutionValue(const LinearExpr& linear_expr) const;

  // Writes the model using the solver internal write function.  Currently only
  // available for Gurobi.
  void Write(const std::string& file_name);

  // Advanced usage: compute the "activities" of all constraints, which are the
  // sums of their linear terms. The activities are returned in the same order
  // as constraints(), which is the order in which constraints were added; but
  // you can also use MPConstraint::index() to get a constraint's index.
  std::vector<double> ComputeConstraintActivities() const;

  // Advanced usage:
  // Verifies the *correctness* of the solution: all variables must be within
  // their domains, all constraints must be satisfied, and the reported
  // objective value must be accurate.
  // Usage:
  // - This can only be called after Solve() was called.
  // - "tolerance" is interpreted as an absolute error threshold.
  // - For the objective value only, if the absolute error is too large,
  //   the tolerance is interpreted as a relative error threshold instead.
  // - If "log_errors" is true, every single violation will be logged.
  // - If "tolerance" is negative, it will be set to infinity().
  //
  // Most users should just set the --verify_solution flag and not bother
  // using this method directly.
  bool VerifySolution(double tolerance, bool log_errors) const;

  // Advanced usage: resets extracted model to solve from scratch. This won't
  // reset the parameters that were set with
  // SetSolverSpecificParametersAsString() or set_time_limit() or even clear the
  // linear program. It will just make sure that next Solve() will be as if
  // everything was reconstructed from scratch.
  void Reset();

  // Interrupts the Solve() execution to terminate processing early if possible.
  // If the underlying interface supports interruption; it does that and returns
  // true regardless of whether there's an ongoing Solve() or not. The Solve()
  // call may still linger for a while depending on the conditions.  If
  // interruption is not supported; returns false and does nothing.
  bool InterruptSolve();

  // ----- Methods using protocol buffers -----

  // Loads model from protocol buffer. Returns MPSOLVER_MODEL_IS_VALID if the
  // model is valid, and another status otherwise (currently only
  // MPSOLVER_MODEL_INVALID and MPSOLVER_INFEASIBLE). If the model isn't
  // valid, populate "error_message".
  MPSolverResponseStatus LoadModelFromProto(const MPModelProto& input_model,
                                            std::string* error_message);
  // The same as above, except that the loading keeps original variable and
  // constraint names. Caller should make sure that all variable names and
  // constraint names are unique, respectively.
  MPSolverResponseStatus LoadModelFromProtoWithUniqueNamesOrDie(
      const MPModelProto& input_model, std::string* error_message);

  // Encodes the current solution in a solution response protocol buffer.

  void FillSolutionResponseProto(MPSolutionResponse* response) const;

  // Solves the model encoded by a MPModelRequest protocol buffer and
  // fills the solution encoded as a MPSolutionResponse.
  // Note(user): This creates a temporary MPSolver and destroys it at the
  // end. If you want to keep the MPSolver alive (for debugging, or for
  // incremental solving), you should write another version of this function
  // that creates the MPSolver object on the heap and returns it.
  //
  // TODO(user): populate an error message in the response.
  static void SolveWithProto(const MPModelRequest& model_request,
                             MPSolutionResponse* response);

  // Exports model to protocol buffer.
  void ExportModelToProto(MPModelProto* output_model) const;

  // Load a solution encoded in a protocol buffer onto this solver for easy
  // access via the MPSolver interface.
  //
  // IMPORTANT: This may only be used in conjunction with ExportModel(),
  // following this example:
  //   MPSolver my_solver;
  //   ... add variables and constraints ...
  //   MPModelProto model_proto;
  //   my_solver.ExportModelToProto(&model_proto);
  //   MPSolutionResponse solver_response;
  //   // This can be replaced by a stubby call to the linear solver server.
  //   MPSolver::SolveWithProto(model_proto, &solver_response);
  //   if (solver_response.result_status() == MPSolutionResponse::OPTIMAL) {
  //     CHECK(my_solver.LoadSolutionFromProto(solver_response));
  //     ... inspect the solution using the usual API: solution_value(), etc...
  //   }
  //
  // This allows users of the pythonic API to conveniently communicate with
  // a linear solver stubby server, via the MPSolver object as a proxy.
  // See /.linear_solver_server_integration_test.py.
  //
  // The response must be in OPTIMAL or FEASIBLE status.
  // Returns false if a problem arised (typically, if it wasn't used like
  // it should be):
  // - loading a solution whose variables don't correspond to the solver's
  //   current variables
  // - loading a solution with a status other than OPTIMAL / FEASIBLE.
  // Note: the variable and objective values aren't checked. You can use
  // VerifySolution() for that.
  bool LoadSolutionFromProto(const MPSolutionResponse& response);

  // ----- Export model to files or strings -----
#ifndef ANDROID_JNI
  // Shortcuts to the homonymous MPModelProtoExporter methods, via
  // exporting to a MPModelProto with ExportModelToProto() (see above).
  bool ExportModelAsLpFormat(bool obfuscated, std::string* model_str);
  bool ExportModelAsMpsFormat(bool fixed_format, bool obfuscated,
                              std::string* model_str);
#endif
  // ----- Misc -----

  // Advanced usage: pass solver specific parameters in text format. The format
  // is solver-specific and is the same as the corresponding solver
  // configuration file format. Returns true if the operation was successful.
  //
  // TODO(user): Currently SCIP will always return true even if the format is
  // wrong (you can check the log if you suspect an issue there). This seems to
  // be a bug in SCIP though.
  bool SetSolverSpecificParametersAsString(const std::string& parameters);
  std::string GetSolverSpecificParametersAsString() const {
    return solver_specific_parameter_string_;
  }

  // Advanced usage: possible basis status values for a variable and the
  // slack variable of a linear constraint.
  enum BasisStatus {
    FREE = 0,
    AT_LOWER_BOUND,
    AT_UPPER_BOUND,
    FIXED_VALUE,
    BASIC
  };

  // Advanced usage: Incrementality. This function takes a starting basis to be
  // used in the next LP Solve() call. The statuses of a current solution can be
  // retrieved via the basis_status() function of a MPVariable or a
  // MPConstraint.
  //
  // WARNING: With Glop, you should disable presolve when using this because
  // this information will not be modified in sync with the presolve and will
  // likely not mean much on the presolved problem.
  void SetStartingLpBasis(
      const std::vector<MPSolver::BasisStatus>& variable_statuses,
      const std::vector<MPSolver::BasisStatus>& constraint_statuses);

  // Infinity. You can use -MPSolver::infinity() for negative infinity.
  static double infinity() { return std::numeric_limits<double>::infinity(); }

  // Controls (or queries) the amount of output produced by the underlying
  // solver. The output can surface to LOGs, or to stdout or stderr, depending
  // on the implementation. The amount of output will greatly vary with each
  // implementation and each problem.
  //
  // Output is suppressed by default.
  bool OutputIsEnabled() const;
  void EnableOutput();
  void SuppressOutput();

  void set_time_limit(int64 time_limit_milliseconds) {
    DCHECK_GE(time_limit_milliseconds, 0);
    time_limit_ = time_limit_milliseconds;
  }

  // In milliseconds.
  int64 time_limit() const { return time_limit_; }

  // In seconds. Note that this returns a double.
  double time_limit_in_secs() const {
    // static_cast<double> avoids a warning with -Wreal-conversion. This
    // helps catching bugs with unwanted conversions from double to ints.
    return static_cast<double>(time_limit_) / 1000.0;
  }

  // Returns wall_time() in milliseconds since the creation of the solver.
  int64 wall_time() const { return timer_.GetInMs(); }

  // Returns the number of simplex iterations.
  int64 iterations() const;

  // Returns the number of branch-and-bound nodes. Only available for
  // discrete problems.
  int64 nodes() const;

  // Returns a std::string describing the underlying solver and its version.
  std::string SolverVersion() const;

  // Advanced usage: returns the underlying solver so that the user
  // can use solver-specific features or features that are not exposed
  // in the simple API of MPSolver. This method is for advanced users,
  // use at your own risk! In particular, if you modify the model or
  // the solution by accessing the underlying solver directly, then
  // the underlying solver will be out of sync with the information
  // kept in the wrapper (MPSolver, MPVariable, MPConstraint,
  // MPObjective). You need to cast the void* returned back to its
  // original type that depends on the interface (CBC:
  // OsiClpSolverInterface*, CLP: ClpSimplex*, GLPK: glp_prob*, SCIP:
  // SCIP*).
  void* underlying_solver();

  // Advanced usage: computes the exact condition number of the
  // current scaled basis: L1norm(B) * L1norm(inverse(B)), where B is
  // the scaled basis.
  // This method requires that a basis exists: it should be called
  // after Solve. It is only available for continuous problems. It is
  // implemented for GLPK but not CLP because CLP does not provide the
  // API for doing it.
  // The condition number measures how well the constraint matrix is
  // conditioned and can be used to predict whether numerical issues
  // will arise during the solve: the model is declared infeasible
  // whereas it is feasible (or vice-versa), the solution obtained is
  // not optimal or violates some constraints, the resolution is slow
  // because of repeated singularities.
  // The rule of thumb to interpret the condition number kappa is:
  // o kappa <= 1e7: virtually no chance of numerical issues
  // o 1e7 < kappa <= 1e10: small chance of numerical issues
  // o 1e10 < kappa <= 1e13: medium chance of numerical issues
  // o kappa > 1e13: high chance of numerical issues
  // The computation of the condition number depends on the quality of
  // the LU decomposition, so it is not very accurate when the matrix
  // is ill conditioned.
  double ComputeExactConditionNumber() const;

  friend class GLPKInterface;
  friend class CLPInterface;
  friend class CBCInterface;
  friend class SCIPInterface;
  friend class GurobiInterface;
  friend class CplexInterface;
  friend class SLMInterface;
  friend class MPSolverInterface;
  friend class GLOPInterface;
  friend class BopInterface;
  friend class KnapsackInterface;

  // Debugging: verify that the given MPVariable* belongs to this solver.
  bool OwnsVariable(const MPVariable* var) const;

 private:
  // Computes the size of the constraint with the largest number of
  // coefficients with index in [min_constraint_index,
  // max_constraint_index)
  int ComputeMaxConstraintSize(int min_constraint_index,
                               int max_constraint_index) const;

  // Returns true if the model has constraints with lower bound > upper bound.
  bool HasInfeasibleConstraints() const;

  // The name of the linear programming problem.
  const std::string name_;

  // The type of the linear programming problem.
  const OptimizationProblemType problem_type_;

  // The solver interface.
  std::unique_ptr<MPSolverInterface> interface_;

  // The vector of variables in the problem.
  std::vector<MPVariable*> variables_;
  // A map from a variable's name to its index in variables_.
  std::unordered_map<std::string, int> variable_name_to_index_;
  // Whether constraints have been extracted to the underlying interface.
  std::vector<bool> variable_is_extracted_;

  // The vector of constraints in the problem.
  std::vector<MPConstraint*> constraints_;
  // A map from a constraint's name to its index in constraints_.
  std::unordered_map<std::string, int> constraint_name_to_index_;
  // Whether constraints have been extracted to the underlying interface.
  std::vector<bool> constraint_is_extracted_;

  // The linear objective function.
  std::unique_ptr<MPObjective> objective_;

  // Initial values for all or some of the problem variables that can be
  // exploited as a starting hint by a solver.
  //
  // Note(user): as of 05/05/2015, we can't use >> because of some SWIG errors.
  std::vector<std::pair<MPVariable*, double> > solution_hint_;

  // Time limit in milliseconds (0 = no limit).
  int64 time_limit_;

  WallTimer timer_;

  // Permanent storage for SetSolverSpecificParametersAsString().
  std::string solver_specific_parameter_string_;


  MPSolverResponseStatus LoadModelFromProtoInternal(
      const MPModelProto& input_model, bool clear_names, std::string* error_message);

  DISALLOW_COPY_AND_ASSIGN(MPSolver);
};

#ifndef ANDROID_JNI
inline std::ostream& operator<<(std::ostream& os,
                                MPSolver::ResultStatus status) {
  return os << MPSolverResponseStatus_Name(
             static_cast<MPSolverResponseStatus>(status));
}
#endif

// The data structure used to store the coefficients of the contraints and of
// the objective. Also define a type to facilitate iteration over them with:
//  for (CoeffEntry entry : coefficients_) { ... }
class CoeffMap : public std::unordered_map<const MPVariable*, double> {
 public:
  explicit CoeffMap(int num_buckets)
#if !defined(_MSC_VER)  // Visual C++ doesn't support this constructor
      : std::unordered_map<const MPVariable*, double>(num_buckets)
#endif  // _MSC_VER
  {
  }
};

typedef std::pair<const MPVariable*, double> CoeffEntry;

// A class to express a linear objective.
class MPObjective {
 public:
  // Clears the offset, all variables and coefficients, and the optimization
  // direction.
  void Clear();

  // Sets the coefficient of the variable in the objective. If the variable
  // does not belong to the solver, the function just returns, or crashes in
  // non-opt mode.
  void SetCoefficient(const MPVariable* const var, double coeff);
  // Gets the coefficient of a given variable in the objective (which
  // is 0 if the variable does not appear in the objective).
  double GetCoefficient(const MPVariable* const var) const;

  // Sets the constant term in the objective.
  void SetOffset(double value);
  // Gets the constant term in the objective.
  double offset() const { return offset_; }

  // Adds a constant term to the objective.
  // Note: please use the less ambiguous SetOffset() if possible!
  // TODO(user): remove this.
  void AddOffset(double value) { SetOffset(offset() + value); }

  // Resets the current objective to take the value of linear_expr, and sets
  // the objective direction to maximize if "is_maximize", otherwise minimizes.
  void OptimizeLinearExpr(const LinearExpr& linear_expr, bool is_maximize);
  void MaximizeLinearExpr(const LinearExpr& linear_expr) {
    OptimizeLinearExpr(linear_expr, true);
  }
  void MinimizeLinearExpr(const LinearExpr& linear_expr) {
    OptimizeLinearExpr(linear_expr, false);
  }

  // Adds linear_expr to the current objective, does not change the direction.
  void AddLinearExpr(const LinearExpr& linear_expr);

  // Sets the optimization direction (maximize: true or minimize: false).
  void SetOptimizationDirection(bool maximize);
  // Sets the optimization direction to minimize.
  void SetMinimization() { SetOptimizationDirection(false); }
  // Sets the optimization direction to maximize.
  void SetMaximization() { SetOptimizationDirection(true); }
  // Is the optimization direction set to maximize?
  bool maximization() const;
  // Is the optimization direction set to minimize?
  bool minimization() const;

  // Returns the objective value of the best solution found so far. It
  // is the optimal objective value if the problem has been solved to
  // optimality.
  //
  // Note: the objective value may be slightly different than what you
  // could compute yourself using MPVariable::solution_value();
  // please use the --verify_solution flag to gain confidence about the
  // numerical stability of your solution.
  double Value() const;

  // Returns the best objective bound. In case of minimization, it is
  // a lower bound on the objective value of the optimal integer
  // solution. Only available for discrete problems.
  double BestBound() const;

 private:
  friend class MPSolver;
  friend class MPSolverInterface;
  friend class CBCInterface;
  friend class CLPInterface;
  friend class GLPKInterface;
  friend class SCIPInterface;
  friend class SLMInterface;
  friend class GurobiInterface;
  friend class CplexInterface;
  friend class GLOPInterface;
  friend class BopInterface;
  friend class KnapsackInterface;

  // Constructor. An objective points to a single MPSolverInterface
  // that is specified in the constructor. An objective cannot belong
  // to several models.
  // At construction, an MPObjective has no terms (which is equivalent
  // on having a coefficient of 0 for all variables), and an offset of 0.
  explicit MPObjective(MPSolverInterface* const interface)
      : interface_(interface), coefficients_(1), offset_(0.0) {}

  MPSolverInterface* const interface_;

  // Mapping var -> coefficient.
  CoeffMap coefficients_;
  // Constant term.
  double offset_;

  DISALLOW_COPY_AND_ASSIGN(MPObjective);
};

// The class for variables of a Mathematical Programming (MP) model.
class MPVariable {
 public:
  // Returns the name of the variable.
  const std::string& name() const { return name_; }

  // Sets the integrality requirement of the variable.
  void SetInteger(bool integer);
  // Returns the integrality requirement of the variable.
  bool integer() const { return integer_; }

  // Returns the value of the variable in the current solution.
  // If the variable is integer, then the value will always be an integer
  // (the underlying solver handles floating-point values only, but this
  // function automatically rounds it to the nearest integer; see: man 3 round).
  double solution_value() const;

  // Returns the index of the variable in the MPSolver::variables_.
  int index() const { return index_; }

  // Returns the lower bound.
  double lb() const { return lb_; }
  // Returns the upper bound.
  double ub() const { return ub_; }
  // Sets the lower bound.
  void SetLB(double lb) { SetBounds(lb, ub_); }
  // Sets the upper bound.
  void SetUB(double ub) { SetBounds(lb_, ub); }
  // Sets both the lower and upper bounds.
  void SetBounds(double lb, double ub);

  // Advanced usage: unrounded solution value, i.e. it won't be rounded to the
  // nearest integer even if the variable is integer.
  double unrounded_solution_value() const;
  // Advanced usage: returns the reduced cost of the variable in the
  // current solution (only available for continuous problems).
  double reduced_cost() const;
  // Advanced usage: returns the basis status of the variable in the
  // current solution (only available for continuous problems).
  // @see MPSolver::BasisStatus.
  MPSolver::BasisStatus basis_status() const;

 protected:
  friend class MPSolver;
  friend class MPSolverInterface;
  friend class CBCInterface;
  friend class CLPInterface;
  friend class GLPKInterface;
  friend class SCIPInterface;
  friend class SLMInterface;
  friend class GurobiInterface;
  friend class CplexInterface;
  friend class GLOPInterface;
  friend class MPVariableSolutionValueTest;
  friend class BopInterface;
  friend class KnapsackInterface;

  // Constructor. A variable points to a single MPSolverInterface that
  // is specified in the constructor. A variable cannot belong to
  // several models.
  MPVariable(int index, double lb, double ub, bool integer, const std::string& name,
             MPSolverInterface* const interface)
      : index_(index),
        lb_(lb),
        ub_(ub),
        integer_(integer),
        name_(name),
        solution_value_(0.0),
        reduced_cost_(0.0),
        interface_(interface) {}

  void set_solution_value(double value) { solution_value_ = value; }
  void set_reduced_cost(double reduced_cost) { reduced_cost_ = reduced_cost; }

 private:
  const int index_;
  double lb_;
  double ub_;
  bool integer_;
  const std::string name_;
  double solution_value_;
  double reduced_cost_;
  MPSolverInterface* const interface_;
  DISALLOW_COPY_AND_ASSIGN(MPVariable);
};

// The class for constraints of a Mathematical Programming (MP) model.
// A constraint is represented as a linear equation or inequality.
class MPConstraint {
 public:
  // Returns the name of the constraint.
  const std::string& name() const { return name_; }

  // Clears all variables and coefficients. Does not clear the bounds.
  void Clear();

  // Sets the coefficient of the variable on the constraint. If the variable
  // does not belong to the solver, the function just returns, or crashes in
  // non-opt mode.
  void SetCoefficient(const MPVariable* const var, double coeff);
  // Gets the coefficient of a given variable on the constraint (which
  // is 0 if the variable does not appear in the constraint).
  double GetCoefficient(const MPVariable* const var) const;

  // Returns the lower bound.
  double lb() const { return lb_; }
  // Returns the upper bound.
  double ub() const { return ub_; }
  // Sets the lower bound.
  void SetLB(double lb) { SetBounds(lb, ub_); }
  // Sets the upper bound.
  void SetUB(double ub) { SetBounds(lb_, ub); }
  // Sets both the lower and upper bounds.
  void SetBounds(double lb, double ub);

  // Advanced usage: returns true if the constraint is "lazy" (see below).
  bool is_lazy() const { return is_lazy_; }
  // Advanced usage: sets the constraint "laziness".
  // *** This is only supported for SCIP and has no effect on other solvers. ***
  // When 'laziness' is true, the constraint is only considered by the Linear
  // Programming solver if its current solution violates the constraint.
  // In this case, the constraint is definitively added to the problem.
  // This may be useful in some MIP problems, and may have a dramatic impact
  // on performance.
  // For more info see: http://tinyurl.com/lazy-constraints.
  void set_is_lazy(bool laziness) { is_lazy_ = laziness; }

  // Returns the index of the constraint in the MPSolver::constraints_.
  int index() const { return index_; }

  // Advanced usage: returns the dual value of the constraint in the
  // current solution (only available for continuous problems).
  double dual_value() const;

  // Advanced usage: returns the basis status of the constraint (only available
  // for continuous problems). Note that if a constraint "linear_expression in
  // [lb, ub]" is transformed into "linear_expression + slack = 0" with slack in
  // [-ub, -lb], then this status is the same as the status of the slack
  // variable with AT_UPPER_BOUND and AT_LOWER_BOUND swapped.
  //
  // @see MPSolver::BasisStatus.
  MPSolver::BasisStatus basis_status() const;

 protected:
  friend class MPSolver;
  friend class MPSolverInterface;
  friend class CBCInterface;
  friend class CLPInterface;
  friend class GLPKInterface;
  friend class SCIPInterface;
  friend class SLMInterface;
  friend class GurobiInterface;
  friend class CplexInterface;
  friend class GLOPInterface;
  friend class BopInterface;
  friend class KnapsackInterface;

  // Constructor. A constraint points to a single MPSolverInterface
  // that is specified in the constructor. A constraint cannot belong
  // to several models.
  MPConstraint(int index, double lb, double ub, const std::string& name,
               MPSolverInterface* const interface)
      : coefficients_(1),
        index_(index),
        lb_(lb),
        ub_(ub),
        name_(name),
        is_lazy_(false),
        dual_value_(0.0),
        interface_(interface) {}

  void set_dual_value(double dual_value) { dual_value_ = dual_value; }

 private:
  // Returns true if the constraint contains variables that have not
  // been extracted yet.
  bool ContainsNewVariables();

  // Mapping var -> coefficient.
  CoeffMap coefficients_;

  const int index_;  // See index().

  // The lower bound for the linear constraint.
  double lb_;

  // The upper bound for the linear constraint.
  double ub_;

  // Name.
  const std::string name_;

  // True if the constraint is "lazy", i.e. the constraint is added to the
  // underlying Linear Programming solver only if it is violated.
  // By default this parameter is 'false'.
  bool is_lazy_;
  double dual_value_;
  MPSolverInterface* const interface_;
  DISALLOW_COPY_AND_ASSIGN(MPConstraint);
};

// This class stores parameter settings for LP and MIP solvers.
// Some parameters are marked as advanced: do not change their values
// unless you know what you are doing!
//
// For developers: how to add a new parameter:
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
// - In class MPSolverInterface, add a virtual method SetFoo, add it
//   to SetCommonParameters or SetMIPParameters, and implement it for
//   each solver. Sometimes, parameters need to be implemented
//   differently, see for example the INCREMENTALITY implementation.
// - Add a test in linear_solver_test.cc.
//
// TODO(user): store the parameter values in a protocol buffer
// instead. We need to figure out how to deal with the subtleties of
// the default values.
class MPSolverParameters {
 public:
  // Enumeration of parameters that take continuous values.
  enum DoubleParam {
    // Limit for relative MIP gap.
    RELATIVE_MIP_GAP = 0,
    // Advanced usage: tolerance for primal feasibility of basic
    // solutions. This does not control the integer feasibility
    // tolerance of integer solutions for MIP or the tolerance used
    // during presolve.
    PRIMAL_TOLERANCE = 1,
    // Advanced usage: tolerance for dual feasibility of basic solutions.
    DUAL_TOLERANCE = 2
  };

  // Enumeration of parameters that take integer or categorical values.
  enum IntegerParam {
    // Advanced usage: presolve mode.
    PRESOLVE = 1000,
    // Algorithm to solve linear programs.
    LP_ALGORITHM = 1001,
    // Advanced usage: incrementality from one solve to the next.
    INCREMENTALITY = 1002,
    // Advanced usage: enable or disable matrix scaling.
    SCALING = 1003
  };

  // For each categorical parameter, enumeration of possible values.
  enum PresolveValues {
    PRESOLVE_OFF = 0,  // Presolve is off.
    PRESOLVE_ON = 1    // Presolve is on.
  };

  enum LpAlgorithmValues {
    DUAL = 10,    // Dual simplex.
    PRIMAL = 11,  // Primal simplex.
    BARRIER = 12  // Barrier algorithm.
  };

  enum IncrementalityValues {
    // Start solve from scratch.
    INCREMENTALITY_OFF = 0,
    // Reuse results from previous solve as much as the underlying
    // solver allows.
    INCREMENTALITY_ON = 1
  };

  enum ScalingValues {
    SCALING_OFF = 0,  // Scaling is off.
    SCALING_ON = 1    // Scaling is on.
  };

  // @{
  // Placeholder value to indicate that a parameter is set to
  // the default value defined in the wrapper.
  static const double kDefaultDoubleParamValue;
  static const int kDefaultIntegerParamValue;
  // @}

  // @{
  // Placeholder value to indicate that a parameter is unknown.
  static const double kUnknownDoubleParamValue;
  static const int kUnknownIntegerParamValue;
  // @}

  // @{
  // Default values for parameters. Only parameters that define the
  // properties of the solution returned need to have a default value
  // (that is the same for all solvers). You can also define a default
  // value for performance parameters when you are confident it is a
  // good choice (example: always turn presolve on).
  static const double kDefaultRelativeMipGap;
  static const double kDefaultPrimalTolerance;
  static const double kDefaultDualTolerance;
  static const PresolveValues kDefaultPresolve;
  static const IncrementalityValues kDefaultIncrementality;
  // @}

  // The constructor sets all parameters to their default value.
  MPSolverParameters();

  // @{
  // Sets a parameter to a specific value.
  void SetDoubleParam(MPSolverParameters::DoubleParam param, double value);
  void SetIntegerParam(MPSolverParameters::IntegerParam param, int value);
  // @}

  // @{
  // Sets a parameter to its default value (default value defined
  // in MPSolverParameters if it exists, otherwise the default value
  // defined in the underlying solver).
  void ResetDoubleParam(MPSolverParameters::DoubleParam param);
  void ResetIntegerParam(MPSolverParameters::IntegerParam param);
  // Sets all parameters to their default value.
  void Reset();
  // @}

  // @{
  // Returns the value of a parameter.
  double GetDoubleParam(MPSolverParameters::DoubleParam param) const;
  int GetIntegerParam(MPSolverParameters::IntegerParam param) const;
  // @}


 private:
  // @{
  // Parameter value for each parameter.
  // @see DoubleParam
  // @see IntegerParam
  double relative_mip_gap_value_;
  double primal_tolerance_value_;
  double dual_tolerance_value_;
  int presolve_value_;
  int scaling_value_;
  int lp_algorithm_value_;
  int incrementality_value_;
  // @}

  // Boolean value indicating whether each parameter is set to the
  // solver's default value. Only parameters for which the wrapper
  // does not define a default value need such an indicator.
  bool lp_algorithm_is_default_;


  DISALLOW_COPY_AND_ASSIGN(MPSolverParameters);
};

// This class wraps the actual mathematical programming solvers. Each
// solver (GLOP, CLP, CBC, GLPK, SCIP) has its own interface class that
// derives from this abstract class. This class is never directly
// accessed by the user.
// @see glop_interface.cc
// @see cbc_interface.cc
// @see clp_interface.cc
// @see glpk_interface.cc
// @see scip_interface.cc
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
  static const int64 kUnknownNumberOfIterations = -1;
  // When the underlying solver does not provide the number of
  // branch-and-bound nodes.
  static const int64 kUnknownNumberOfNodes = -1;

  // Constructor. The user will access the MPSolverInterface through the
  // MPSolver passed as argument.
  explicit MPSolverInterface(MPSolver* const solver);
  virtual ~MPSolverInterface();

  // ----- Solve -----
  // Solves problem with specified parameter values. Returns true if the
  // solution is optimal.
  virtual MPSolver::ResultStatus Solve(const MPSolverParameters& param) = 0;

  // Writes the model using the solver internal write function.  Currently only
  // available for GurobiInterface.
  virtual void Write(const std::string& filename);

  // ----- Model modifications and extraction -----
  // Resets extracted model.
  virtual void Reset() = 0;

  // Sets the optimization direction (min/max).
  virtual void SetOptimizationDirection(bool maximize) = 0;

  // Modifies bounds of an extracted variable.
  virtual void SetVariableBounds(int index, double lb, double ub) = 0;

  // Modifies integrality of an extracted variable.
  virtual void SetVariableInteger(int index, bool integer) = 0;

  // Modify bounds of an extracted variable.
  virtual void SetConstraintBounds(int index, double lb, double ub) = 0;

  // Adds a linear constraint.
  virtual void AddRowConstraint(MPConstraint* const ct) = 0;

  // Add a variable.
  virtual void AddVariable(MPVariable* const var) = 0;

  // Changes a coefficient in a constraint.
  virtual void SetCoefficient(MPConstraint* const constraint,
                              const MPVariable* const variable,
                              double new_value, double old_value) = 0;

  // Clears a constraint from all its terms.
  virtual void ClearConstraint(MPConstraint* const constraint) = 0;

  // Changes a coefficient in the linear objective.
  virtual void SetObjectiveCoefficient(const MPVariable* const variable,
                                       double coefficient) = 0;

  // Changes the constant term in the linear objective.
  virtual void SetObjectiveOffset(double value) = 0;

  // Clears the objective from all its terms.
  virtual void ClearObjective() = 0;

  // ------ Query statistics on the solution and the solve ------
  // Returns the number of simplex iterations. The problem must be discrete,
  // otherwise it crashes, or returns kUnknownNumberOfIterations in NDEBUG mode.
  virtual int64 iterations() const = 0;
  // Returns the number of branch-and-bound nodes. The problem must be discrete,
  // otherwise it crashes, or returns kUnknownNumberOfNodes in NDEBUG mode.
  virtual int64 nodes() const = 0;
  // Returns the best objective bound. The problem must be discrete, otherwise
  // it crashes, or returns trivial_worst_objective_bound() in NDEBUG mode.
  virtual double best_objective_bound() const = 0;
  // A trivial objective bound: the worst possible value of the objective,
  // which will be +infinity if minimizing and -infinity if maximing.
  double trivial_worst_objective_bound() const;
  // Returns the objective value of the best solution found so far.
  double objective_value() const;

  // Returns the basis status of a row.
  virtual MPSolver::BasisStatus row_status(int constraint_index) const = 0;
  // Returns the basis status of a constraint.
  virtual MPSolver::BasisStatus column_status(int variable_index) const = 0;

  // Checks whether the solution is synchronized with the model, i.e. whether
  // the model has changed since the solution was computed last.
  // If it isn't, it crashes in NDEBUG, and returns false othwerwise.
  bool CheckSolutionIsSynchronized() const;
  // Checks whether a feasible solution exists. The behavior is similar to
  // CheckSolutionIsSynchronized() above.
  virtual bool CheckSolutionExists() const;
  // Handy shortcut to do both checks above (it is often used).
  bool CheckSolutionIsSynchronizedAndExists() const {
    return CheckSolutionIsSynchronized() && CheckSolutionExists();
  }
  // Checks whether information on the best objective bound exists. The behavior
  // is similar to CheckSolutionIsSynchronized() above.
  virtual bool CheckBestObjectiveBoundExists() const;

  // ----- Misc -----
  // Queries problem type. For simplicity, the distinction between
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

  // Returns the index of the last variable extracted.
  int last_variable_index() const { return last_variable_index_; }

  bool variable_is_extracted(int var_index) const {
    return solver_->variable_is_extracted_[var_index];
  }
  void set_variable_as_extracted(int var_index, bool extracted) {
    solver_->variable_is_extracted_[var_index] = extracted;
  }
  bool constraint_is_extracted(int ct_index) const {
    return solver_->constraint_is_extracted_[ct_index];
  }
  void set_constraint_as_extracted(int ct_index, bool extracted) {
    solver_->constraint_is_extracted_[ct_index] = extracted;
  }

  // Returns the boolean indicating the verbosity of the solver output.
  bool quiet() const { return quiet_; }
  // Sets the boolean indicating the verbosity of the solver output.
  void set_quiet(bool quiet_value) { quiet_ = quiet_value; }

  // Returns the result status of the last solve.
  MPSolver::ResultStatus result_status() const {
    CheckSolutionIsSynchronized();
    return result_status_;
  }

  // Returns a std::string describing the underlying solver and its version.
  virtual std::string SolverVersion() const = 0;

  // Returns the underlying solver.
  virtual void* underlying_solver() = 0;

  // Computes exact condition number. Only available for continuous
  // problems and only implemented in GLPK.
  virtual double ComputeExactConditionNumber() const;

  // See MPSolver::SetStartingLpBasis().
  virtual void SetStartingLpBasis(
      const std::vector<MPSolver::BasisStatus>& variable_statuses,
      const std::vector<MPSolver::BasisStatus>& constraint_statuses) {
    LOG(FATAL) << "Not supported by this solver.";
  }

  virtual bool InterruptSolve() { return false; }

  friend class MPSolver;

  // To access the maximize_ bool and the MPSolver.
  friend class MPConstraint;
  friend class MPObjective;

 protected:
  MPSolver* const solver_;
  // Indicates whether the model and the solution are synchronized.
  SynchronizationStatus sync_status_;
  // Indicates whether the solve has reached optimality,
  // infeasibility, a limit, etc.
  MPSolver::ResultStatus result_status_;
  // Optimization direction.
  bool maximize_;

  // Index in MPSolver::variables_ of last constraint extracted.
  int last_constraint_index_;
  // Index in MPSolver::constraints_ of last variable extracted.
  int last_variable_index_;

  // The value of the objective function.
  double objective_value_;

  // Boolean indicator for the verbosity of the solver output.
  bool quiet_;

  // Index of dummy variable created for empty constraints or the
  // objective offset.
  static const int kDummyVariableIndex;

  // Extracts model stored in MPSolver.
  void ExtractModel();
  // Extracts the variables that have not been extracted yet.
  virtual void ExtractNewVariables() = 0;
  // Extracts the constraints that have not been extracted yet.
  virtual void ExtractNewConstraints() = 0;
  // Extracts the objective.
  virtual void ExtractObjective() = 0;
  // Resets the extraction information.
  void ResetExtractionInformation();
  // Change synchronization status from SOLUTION_SYNCHRONIZED to
  // MODEL_SYNCHRONIZED. To be used for model changes.
  void InvalidateSolutionSynchronization();

  // Sets parameters common to LP and MIP in the underlying solver.
  void SetCommonParameters(const MPSolverParameters& param);
  // Sets MIP specific parameters in the underlying solver.
  void SetMIPParameters(const MPSolverParameters& param);
  // Sets all parameters in the underlying solver.
  virtual void SetParameters(const MPSolverParameters& param) = 0;
  // Sets an unsupported double parameter.
  void SetUnsupportedDoubleParam(MPSolverParameters::DoubleParam param) const;
  // Sets an unsupported integer parameter.
  void SetUnsupportedIntegerParam(MPSolverParameters::IntegerParam param) const;
  // Sets a supported double parameter to an unsupported value.
  void SetDoubleParamToUnsupportedValue(MPSolverParameters::DoubleParam param,
                                        double value) const;
  // Sets a supported integer parameter to an unsupported value.
  void SetIntegerParamToUnsupportedValue(MPSolverParameters::IntegerParam param,
                                         int value) const;
  // Sets each parameter in the underlying solver.
  virtual void SetRelativeMipGap(double value) = 0;
  virtual void SetPrimalTolerance(double value) = 0;
  virtual void SetDualTolerance(double value) = 0;
  virtual void SetPresolveMode(int value) = 0;

  // Pass solver specific parameters in text format. The format is
  // solver-specific and is the same as the corresponding solver configuration
  // file format. Returns true if the operation was successful.
  //
  // The default implementation of this method stores the parameters in a
  // temporary file and calls ReadParameterFile to import the parameter file
  // into the solver. Solvers that support passing the parameters directly can
  // override this method to skip the temporary file logic.
  virtual bool SetSolverSpecificParametersAsString(const std::string& parameters);

  // Reads a solver-specific file of parameters and set them.
  // Returns true if there was no errors.
  virtual bool ReadParameterFile(const std::string& filename);

  // Returns a file extension like ".tmp", this is needed because some solvers
  // require a given extension for the ReadParameterFile() filename and we need
  // to know it to generate a temporary parameter file.
  virtual std::string ValidFileExtensionForParameterFile() const;

  // Sets the scaling mode.
  virtual void SetScalingMode(int value) = 0;
  virtual void SetLpAlgorithm(int value) = 0;
};


}  // namespace operations_research

#endif  // OR_TOOLS_LINEAR_SOLVER_LINEAR_SOLVER_H_
