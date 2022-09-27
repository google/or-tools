// Copyright 2010-2022 Google LLC
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

// Simplified bindings for the SCIP solver. This is not designed to be used
// directly by users, the API is not friendly to a modeler.  For most common
// cases, use MPSolver instead.
//
// Notable differences between gSCIP and SCIP:
//   * Unless callbacks are used, gSCIP only exposes the SCIP stage PROBLEM to
//     the user through public APIs.
//   * Instead of the stateful SCIP parameters API, parameters are passed in at
//     Solve() time and cleared at the end of solve. Parameters that effect
//     problem creation are thus not supported.
//   * gSCIP uses std::numeric_limits<double>::infinity(), rather than SCIPs
//     infinity (a default value of 1e20). Doubles with absolute value >= 1e20
//     but < inf result in an error. Changing the underlying SCIP's infinity is
//     not supported.
//   * absl::Status and absl::StatusOr are used to propagate SCIP errors (and on
//     a best effort basis, also filter out bad input to gSCIP functions).
//
// A note on error propagation and reliability:
//   Many methods on SCIP return an error code. Errors can be triggered by
// both invalid input and bugs in SCIP. We propagate these errors back to the
// user through gSCIP through Status and StatusOr. If you are solving a single
// MIP and you have previously successfully solved similar MIPs, it is unlikely
// gSCIP would return any status errors. Depending on your application, CHECK
// failing on these errors may be appropriate (e.g. a benchmark that is run by
// hand). If you are solving a very large number of MIPs (e.g. in a flume job),
// your instances are numerically challenging, or the model/data are drawn from
// an unreliable source, or you are running a server that cannot crash, you may
// want to try and process these errors instead. Note that on bad instances,
// SCIP may still crash, so highly reliable systems should run SCIP in a
// separate process.
//
// NOTE(user): much of the API uses const std::string& instead of
// absl::string_view because the underlying SCIP API needs a null terminated
// char*.
#ifndef OR_TOOLS_GSCIP_GSCIP_H_
#define OR_TOOLS_GSCIP_GSCIP_H_

#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/gscip/gscip.pb.h"
#include "ortools/gscip/gscip_message_handler.h"  // IWYU pragma: export
#include "scip/scip.h"
#include "scip/scip_prob.h"
#include "scip/type_cons.h"
#include "scip/type_scip.h"
#include "scip/type_var.h"

namespace operations_research {

using GScipSolution = absl::flat_hash_map<SCIP_VAR*, double>;

// The result of GScip::Solve(). Contains the solve status, statistics, and the
// solutions found.
struct GScipResult {
  GScipOutput gscip_output;
  // The number of solutions returned is at most GScipParameters::num_solutions.
  // They are ordered from best objective value to worst. When
  // gscip_output.status() is optimal, solutions will have at least one element.
  std::vector<GScipSolution> solutions;
  // Of the same size as solutions.
  std::vector<double> objective_values;
  // Advanced use below

  // If the problem was unbounded, a primal ray in the unbounded direction of
  // the LP relaxation should be produced.
  absl::flat_hash_map<SCIP_VAR*, double> primal_ray;
  // TODO(user): add dual support:
  //   1. The dual solution for LPs.
  //   2. The dual ray for infeasible LP/MIPs.
};

// Models the constraint lb <= a*x <= ub. Members variables and coefficients
// must have the same size.
struct GScipLinearRange {
  double lower_bound = -std::numeric_limits<double>::infinity();
  std::vector<SCIP_VAR*> variables;
  std::vector<double> coefficients;
  double upper_bound = std::numeric_limits<double>::infinity();
};

// A variable is implied integer if the integrality constraint is not required
// for the model to be valid, but the variable takes an integer value in any
// optimal solution to the problem.
enum class GScipVarType { kContinuous, kBinary, kInteger, kImpliedInteger };

struct GScipIndicatorConstraint;
struct GScipLogicalConstraintData;
// Some advanced features, defined at the end of the header file.
struct GScipQuadraticRange;
struct GScipSOSData;
struct GScipVariableOptions;

const GScipVariableOptions& DefaultGScipVariableOptions();
struct GScipConstraintOptions;

const GScipConstraintOptions& DefaultGScipConstraintOptions();
using GScipBranchingPriority = absl::flat_hash_map<SCIP_VAR*, int>;
enum class GScipHintResult;

// A thin wrapper around the SCIP solver that provides C++ bindings that are
// idiomatic for Google. Unless callbacks are used, the SCIP stage is always
// PROBLEM.
class GScip {
 public:
  // Create a new GScip (the constructor is private). The default objective
  // direction is minimization.
  static absl::StatusOr<std::unique_ptr<GScip>> Create(
      const std::string& problem_name);
  ~GScip();
  static std::string ScipVersion();

  // After Solve() the parameters are reset and SCIP stage is restored to
  // PROBLEM. "legacy_params" are in the format of legacy_scip_params.h and are
  // applied after "params". Use of "legacy_params" is discouraged.
  //
  // The returned StatusOr will contain an error only if an:
  //   * An underlying function from SCIP fails.
  //   * There is an I/O error with managing SCIP output.
  // The above cases are not mutually exclusive. If the problem is infeasible,
  // this will be reflected in the value of GScipResult::gscip_output::status.
  absl::StatusOr<GScipResult> Solve(
      const GScipParameters& params = GScipParameters(),
      const std::string& legacy_params = "",
      GScipMessageHandler message_handler = nullptr);

  // ///////////////////////////////////////////////////////////////////////////
  // Basic Model Construction
  // ///////////////////////////////////////////////////////////////////////////

  // Use true for maximization, false for minimization.
  absl::Status SetMaximize(bool is_maximize);
  absl::Status SetObjectiveOffset(double offset);

  // The returned SCIP_VAR is owned by GScip. With default options, the
  // returned variable will have the same lifetime as GScip (if instead,
  // GScipVariableOptions::keep_alive is false, SCIP may free the variable at
  // any time, see GScipVariableOptions::keep_alive for details).
  //
  // Note that SCIP will internally convert a variable of type `kInteger` with
  // bounds of [0, 1] to a variable of type `kBinary`.
  absl::StatusOr<SCIP_VAR*> AddVariable(
      double lb, double ub, double obj_coef, GScipVarType var_type,
      const std::string& var_name = "",
      const GScipVariableOptions& options = DefaultGScipVariableOptions());

  // The returned SCIP_CONS is owned by GScip. With default options, the
  // returned variable will have the same lifetime as GScip (if instead,
  // GScipConstraintOptions::keep_alive is false, SCIP may free the constraint
  // at any time, see GScipConstraintOptions::keep_alive for details).
  //
  // Can be called while creating the model or in a callback (e.g. in a
  // GScipConstraintHandler).
  absl::StatusOr<SCIP_CONS*> AddLinearConstraint(
      const GScipLinearRange& range, const std::string& name = "",
      const GScipConstraintOptions& options = DefaultGScipConstraintOptions());

  // ///////////////////////////////////////////////////////////////////////////
  // Model Queries
  // ///////////////////////////////////////////////////////////////////////////

  bool ObjectiveIsMaximize();
  double ObjectiveOffset();

  double Lb(SCIP_VAR* var);
  double Ub(SCIP_VAR* var);
  double ObjCoef(SCIP_VAR* var);
  // NOTE: The returned type may differ from the type passed to `AddVariable()`.
  GScipVarType VarType(SCIP_VAR* var);
  absl::string_view Name(SCIP_VAR* var);
  const absl::flat_hash_set<SCIP_VAR*>& variables() { return variables_; }

  // These methods works on all constraint types.
  absl::string_view Name(SCIP_CONS* constraint);
  bool IsConstraintLinear(SCIP_CONS* constraint);
  const absl::flat_hash_set<SCIP_CONS*>& constraints() { return constraints_; }

  // These methods will CHECK fail if constraint is not a linear constraint.
  absl::Span<const double> LinearConstraintCoefficients(SCIP_CONS* constraint);
  absl::Span<SCIP_VAR* const> LinearConstraintVariables(SCIP_CONS* constraint);
  double LinearConstraintLb(SCIP_CONS* constraint);
  double LinearConstraintUb(SCIP_CONS* constraint);

  // ///////////////////////////////////////////////////////////////////////////
  // Model Updates (needed for incrementalism)
  // ///////////////////////////////////////////////////////////////////////////
  // TODO(b/246342145): A crash may occur if you attempt to set a lb <= -1.0 on
  // a binary variable. SCIP can also silently change the vartype of a variable
  // after construction, so you should check it via `VarType()`.
  absl::Status SetLb(SCIP_VAR* var, double lb);
  // TODO(b/246342145): A crash may occur if you attempt to set an ub >= 2.0 on
  // a binary variable. SCIP can also silently change the vartype of a variable
  // after construction, so you should check it via `VarType()`.
  absl::Status SetUb(SCIP_VAR* var, double ub);
  absl::Status SetObjCoef(SCIP_VAR* var, double obj_coef);
  absl::Status SetVarType(SCIP_VAR* var, GScipVarType var_type);

  // Warning: you need to ensure that no constraint has a reference to this
  // variable before deleting it, or undefined behavior will occur. For linear
  // constraints, you can set the coefficient of this variable to zero to remove
  // the variable from the constriant.
  absl::Status DeleteVariable(SCIP_VAR* var);

  // Checks if SafeBulkDelete will succeed for vars, and returns a description
  // the problematic variables/constraints on a failure (the returned status
  // will not contain a propagated SCIP error). Will not modify the underyling
  // SCIP, it is safe to continue using this if an error is returned.
  absl::Status CanSafeBulkDelete(const absl::flat_hash_set<SCIP_VAR*>& vars);

  // Attempts to remove vars from all constraints and then remove vars from
  // the model. As of August 7, 2020, will fail if the model contains any
  // constraints that are not linear.
  //
  // Will call CanSafeBulkDelete above, but can also return an error Status
  // propagated from SCIP. Do not assume SCIP is in a valid state if this fails.
  absl::Status SafeBulkDelete(const absl::flat_hash_set<SCIP_VAR*>& vars);

  // These methods will CHECK fail if constraint is not a linear constraint.
  absl::Status SetLinearConstraintLb(SCIP_CONS* constraint, double lb);
  absl::Status SetLinearConstraintUb(SCIP_CONS* constraint, double ub);
  absl::Status SetLinearConstraintCoef(SCIP_CONS* constraint, SCIP_VAR* var,
                                       double value);
  absl::Status AddLinearConstraintCoef(SCIP_CONS* constraint, SCIP_VAR* var,
                                       double value);

  // Works on all constraint types. Unlike DeleteVariable, no special action is
  // required before deleting a constraint.
  absl::Status DeleteConstraint(SCIP_CONS* constraint);

  // ///////////////////////////////////////////////////////////////////////////
  // Nonlinear constraint types.
  // For now, only basic support (adding to the model) is provided. Reading and
  // updating support may be added in the future.
  // ///////////////////////////////////////////////////////////////////////////

  // Adds a constraint of the form:
  //   if z then a * x <= b
  // where z is a binary variable, x is a vector of decision variables, a is
  // vector of constants, and b is a constant. z can be negated.
  //
  // NOTE(user): options.modifiable is ignored.
  absl::StatusOr<SCIP_CONS*> AddIndicatorConstraint(
      const GScipIndicatorConstraint& indicator_constraint,
      const std::string& name = "",
      const GScipConstraintOptions& options = DefaultGScipConstraintOptions());

  // Adds a constraint of form lb <= x * Q * x + a * x <= ub.
  //
  // NOTE(user): options.modifiable and options.sticking_at_node are ignored.
  absl::StatusOr<SCIP_CONS*> AddQuadraticConstraint(
      const GScipQuadraticRange& range, const std::string& name = "",
      const GScipConstraintOptions& options = DefaultGScipConstraintOptions());

  // Adds the constraint:
  //   logical_data.resultant = AND_i logical_data.operators[i],
  // where logical_data.resultant and logical_data.operators[i] are all binary
  // variables.
  absl::StatusOr<SCIP_CONS*> AddAndConstraint(
      const GScipLogicalConstraintData& logical_data,
      const std::string& name = "",
      const GScipConstraintOptions& options = DefaultGScipConstraintOptions());

  // Adds the constraint:
  //   logical_data.resultant = OR_i logical_data.operators[i],
  // where logical_data.resultant and logical_data.operators[i] must be binary
  // variables.
  absl::StatusOr<SCIP_CONS*> AddOrConstraint(
      const GScipLogicalConstraintData& logical_data,
      const std::string& name = "",
      const GScipConstraintOptions& options = DefaultGScipConstraintOptions());

  // Adds the constraint that at most one of the variables in sos_data can be
  // nonzero. The variables can be integer or continuous. See GScipSOSData for
  // details.
  //
  // NOTE(user): options.modifiable is ignored (these constraints are not
  // modifiable).
  absl::StatusOr<SCIP_CONS*> AddSOS1Constraint(
      const GScipSOSData& sos_data, const std::string& name = "",
      const GScipConstraintOptions& options = DefaultGScipConstraintOptions());

  // Adds the constraint that at most two of the variables in sos_data can be
  // nonzero, and they must be adjacent under the ordering for sos_data. See
  // GScipSOSData for details.
  //
  // NOTE(user): options.modifiable is ignored (these constraints are not
  // modifiable).
  absl::StatusOr<SCIP_CONS*> AddSOS2Constraint(
      const GScipSOSData& sos_data, const std::string& name = "",
      const GScipConstraintOptions& options = DefaultGScipConstraintOptions());

  // ///////////////////////////////////////////////////////////////////////////
  // Advanced use
  // ///////////////////////////////////////////////////////////////////////////

  // Returns the name of the constraint handler for this constraint.
  absl::string_view ConstraintType(SCIP_CONS* constraint);

  // The proposed solution can be partial (only specify some of the variables)
  // or complete. Complete solutions will be checked for feasibility and
  // objective quality, and might be unused for these reasons. Partial solutions
  // will always be accepted.
  absl::StatusOr<GScipHintResult> SuggestHint(
      const GScipSolution& partial_solution);

  // All variables have a default branching priority of zero. Variables are
  // partitioned by their branching priority, and a fractional variable from the
  // highest partition will always be branched on.
  //
  // TODO(user): Add support for BranchingFactor as well, this is typically
  // more useful.
  absl::Status SetBranchingPriority(SCIP_VAR* var, int priority);

  // Doubles with absolute value of at least this value are invalid and result
  // in errors. Floating point actual infinities are replaced by this value in
  // SCIP calls. SCIP considers values at least this large to be infinite. When
  // querying gSCIP, if an absolute value exceeds ScipInf, it is replaced by
  // std::numeric_limits<double>::infinity().
  double ScipInf();
  static constexpr double kDefaultScipInf = 1e20;

  // WARNING(rander): no synchronization is provided between InterruptSolve()
  // and ~GScip(). These methods require mutual exclusion, the user is
  // responsible for ensuring this invariant.
  // TODO(user): should we add a lock here? Seems a little dangerous to block
  // in a destructor.
  bool InterruptSolve();

  // These should typically not be needed.
  SCIP* scip() { return scip_; }

  absl::StatusOr<bool> DefaultBoolParamValue(const std::string& parameter_name);
  absl::StatusOr<int> DefaultIntParamValue(const std::string& parameter_name);
  absl::StatusOr<int64_t> DefaultLongParamValue(
      const std::string& parameter_name);
  absl::StatusOr<double> DefaultRealParamValue(
      const std::string& parameter_name);
  absl::StatusOr<char> DefaultCharParamValue(const std::string& parameter_name);
  absl::StatusOr<std::string> DefaultStringParamValue(
      const std::string& parameter_name);

 private:
  explicit GScip(SCIP* scip);
  // Releases SCIP memory.
  absl::Status CleanUp();

  absl::Status SetParams(const GScipParameters& params,
                         const std::string& legacy_params);
  absl::Status FreeTransform();

  // Replaces +/- inf by +/- ScipInf(), fails when |d| is in [ScipInf(), inf).
  absl::StatusOr<double> ScipInfClamp(double d);

  // Returns +/- inf if |d| >= ScipInf(), otherwise returns d.
  double ScipInfUnclamp(double d);

  // Returns an error if |d| >= ScipInf().
  absl::Status CheckScipFinite(double d);

  absl::Status MaybeKeepConstraintAlive(SCIP_CONS* constraint,
                                        const GScipConstraintOptions& options);

  SCIP* scip_;
  absl::flat_hash_set<SCIP_VAR*> variables_;
  absl::flat_hash_set<SCIP_CONS*> constraints_;
};

// Advanced features below

// Models the constraint
//   lb <= x * Q * x + a * x <= ub
struct GScipQuadraticRange {
  // Models lb above.
  double lower_bound = -std::numeric_limits<double>::infinity();

  // Models a * x above. linear_variables and linear_coefficients must have the
  // same size.
  std::vector<SCIP_Var*> linear_variables;
  std::vector<double> linear_coefficients;

  // These three vectors must have the same size. Models x * Q * x as
  // sum_i quadratic_coefficients[i] * quadratic_variables1[i]
  //                                 * quadratic_variables2[i]
  //
  // Duplicate quadratic terms (e.g. i=3 encodes 4*x1*x3 and i=4 encodes
  // 8*x3*x1) are added (as if you added a single entry 12*x1*x3).
  //
  // TODO(user): investigate, the documentation seems to suggest that when
  // linear_variables[i] == quadratic_variables1[i] == quadratic_variables2[i]
  // there is some advantage.
  std::vector<SCIP_Var*> quadratic_variables1;
  std::vector<SCIP_Var*> quadratic_variables2;
  std::vector<double> quadratic_coefficients;

  // Models ub above.
  double upper_bound = std::numeric_limits<double>::infinity();
};

// Models special ordered set constraints (SOS1 and SOS2 constraints). Each
// contains a list of variables that are implicitly ordered by the provided
// weights, which must be distinct.
//   SOS1: At most one of the variables can be nonzero.
//   SOS2: At most two of the variables can be nonzero, and they must be
//         consecutive.
//
// The weights are optional, and if not provided, the ordering in "variables" is
// used.
struct GScipSOSData {
  // The list of variables where all but one or two must be zero. Can be integer
  // or continuous variables, typically their domain will contain zero. Cannot
  // be empty in a valid SOS constraint.
  std::vector<SCIP_VAR*> variables;

  // Optional, can be empty. Otherwise, must have size equal to variables, and
  // values must be distinct. Determines an "ordering" over the variables
  // (smallest weight to largest). Additionally, the numeric values of
  // the weights are used to make branching decisions in a solver specific way,
  // for details, see:
  //   * https://scip.zib.de/doc/html/cons__sos1_8c.php
  //   * https://scip.zib.de/doc/html/cons__sos2_8c.php.
  std::vector<double> weights;
};

// Models the constraint z = 1 => a * x <= b
// If negate_indicator, then instead: z = 0 => a * x <= b
struct GScipIndicatorConstraint {
  // The z variable above. The vartype must be kBinary.
  SCIP_VAR* indicator_variable = nullptr;
  bool negate_indicator = false;
  // The x variable above.
  std::vector<SCIP_Var*> variables;
  // a above. Must have the same size as x.
  std::vector<double> coefficients;
  // b above.
  double upper_bound = std::numeric_limits<double>::infinity();
};

// Data for constraint of the form resultant = f(operators), e.g.:
//   resultant = AND_i operators[i]
// For existing constraints (e.g. AND, OR) resultant and operators[i] should all
// be binary variables, this my change. See use in GScip for details.
struct GScipLogicalConstraintData {
  SCIP_VAR* resultant = nullptr;
  std::vector<SCIP_VAR*> operators;
};

enum class GScipHintResult {
  // Hint was not feasible.
  kInfeasible,
  // Hint was not good enough to keep.
  kRejected,
  // Hint was kept. Partial solutions are not checked for feasibility, they
  // are always accepted.
  kAccepted
};

// Advanced use. Options to use when creating a variable.
struct GScipVariableOptions {
  // ///////////////////////////////////////////////////////////////////////////
  // SCIP options. Descriptions are from the SCIP documentation, e.g.
  // SCIPcreateVar:
  // https://scip.zib.de/doc/html/group__PublicVariableMethods.php#ga7a37fe4dc702dadecc4186b9624e93fc
  // ///////////////////////////////////////////////////////////////////////////

  // Should var's column be present in the initial root LP?
  bool initial = true;

  // Is var's column removable from the LP (due to aging or cleanup)?
  bool removable = false;

  // ///////////////////////////////////////////////////////////////////////////
  // gSCIP options.
  // ///////////////////////////////////////////////////////////////////////////

  // If keep_alive=true, the returned variable will not to be freed until after
  // ~GScip() is called. Otherwise, the returned variable could be freed
  // internally by SCIP at any point, and it is not safe to hold a reference to
  // the returned variable.
  //
  // The primary reason to set keep_alive=false is if you are adding many
  // variables in a callback (in branch and price), and you expect that most of
  // them will be deleted.
  bool keep_alive = true;
};

// Advanced use. Options to use when creating a constraint.
struct GScipConstraintOptions {
  // ///////////////////////////////////////////////////////////////////////////
  // SCIP options. Descriptions are from the SCIP documentation, e.g.
  // SCIPcreateConsLinear:
  // https://scip.zib.de/doc/html/group__CONSHDLRS.php#gaea3b4db21fe214be5db047e08b46b50e
  // ///////////////////////////////////////////////////////////////////////////

  // Should the LP relaxation of constraint be in the initial LP? False for lazy
  // constraints (true in callbacks).
  bool initial = true;
  // Should the constraint be separated during LP processing?
  bool separate = true;
  // Should the constraint be enforced during node processing? True for model
  // constraints, false for redundant constraints.
  bool enforce = true;
  // Should the constraint be checked for feasibility? True for model
  // constraints, false for redundant constraints.
  bool check = true;
  // Should the constraint be propagated during node processing?
  bool propagate = true;
  // Is constraint only valid locally? Must be true for branching constraints.
  bool local = false;
  // Is constraint modifiable (subject to column generation)? In column
  // generation applications, set to true if pricing adds coefficients to this
  // constraint.
  bool modifiable = false;
  // Is constraint subject to aging? Set to true for own cuts which are
  // separated as constraints
  bool dynamic = false;
  // Should the relaxation be removed from the LP due to aging or cleanup? Set
  // to true for 'lazy constraints' and 'user cuts'.
  bool removable = false;
  // Should the constraint always be kept at the node where it was added, even
  // if it may be moved to a more global node? Usually set to false. Set to true
  // for constraints that represent node data.
  bool sticking_at_node = false;

  // ///////////////////////////////////////////////////////////////////////////
  // gSCIP options.
  // ///////////////////////////////////////////////////////////////////////////

  // If keep_alive=true, the returned constraint will not to be freed until
  // after ~GScip() is called. Otherwise, the returned constraint could be freed
  // internally by SCIP at any point, and it is not safe to hold a reference to
  // the returned constraint.
  //
  // The primary reason to set keep_alive=false is if you are adding many
  // constraints in a callback, and you expect that most of them will be
  // deleted.
  bool keep_alive = true;
};

}  // namespace operations_research

#endif  // OR_TOOLS_GSCIP_GSCIP_H_
