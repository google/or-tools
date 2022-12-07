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

// IWYU pragma: private, include "ortools/math_opt/cpp/math_opt.h"
// IWYU pragma: friend "ortools/math_opt/cpp/.*"

#ifndef OR_TOOLS_MATH_OPT_CPP_PARAMETERS_H_
#define OR_TOOLS_MATH_OPT_CPP_PARAMETERS_H_

#include <cstdint>
#include <optional>
#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ortools/base/linked_hash_map.h"
#include "ortools/glop/parameters.pb.h"  // IWYU pragma: export
#include "ortools/gscip/gscip.pb.h"      // IWYU pragma: export
#include "ortools/math_opt/cpp/enums.h"  // IWYU pragma: export
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/solvers/gurobi.pb.h"  // IWYU pragma: export
#include "ortools/sat/sat_parameters.pb.h"       // IWYU pragma: export

namespace operations_research {
namespace math_opt {

// The solvers wrapped by MathOpt.
enum class SolverType {
  // Solving Constraint Integer Programs (SCIP) solver.
  //
  // It supports both MIPs and LPs. No dual data for LPs is returned though. To
  // solve LPs, kGlop should be preferred.
  kGscip = SOLVER_TYPE_GSCIP,

  // Gurobi solver.
  //
  // It supports both MIPs and LPs.
  kGurobi = SOLVER_TYPE_GUROBI,

  // Google's Glop linear solver.
  //
  // It only solves LPs.
  kGlop = SOLVER_TYPE_GLOP,

  // Google's CP-SAT solver.
  //
  // It supports solving IPs and can scale MIPs to solve them as IPs.
  kCpSat = SOLVER_TYPE_CP_SAT,

  // GNU Linear Programming Kit (GLPK).
  //
  // It supports both MIPs and LPs.
  //
  // Thread-safety: GLPK use thread-local storage for memory allocations. As a
  // consequence when using IncrementalSolver, the user must make sure that
  // instances are destroyed on the same thread as they are created or GLPK will
  // crash. It seems OK to call IncrementalSolver::Solve() from another thread
  // than the one used to create the Solver but it is not documented by GLPK and
  // should be avoided. Of course these limitations do not apply to the Solve()
  // function that recreates a new GLPK problem in the calling thread and
  // destroys before returning.
  //
  // When solving a LP with the presolver, a solution (and the unbound rays) are
  // only returned if an optimal solution has been found. Else nothing is
  // returned. See glpk-5.0/doc/glpk.pdf page #40 available from glpk-5.0.tar.gz
  // for details.
  kGlpk = SOLVER_TYPE_GLPK,

};

MATH_OPT_DEFINE_ENUM(SolverType, SOLVER_TYPE_UNSPECIFIED);

// Parses a flag of type SolverType.
//
// The expected values are the one returned by EnumToString().
bool AbslParseFlag(absl::string_view text, SolverType* value,
                   std::string* error);

// Unparses a flag of type SolverType.
//
// The returned values are the same as EnumToString().
std::string AbslUnparseFlag(SolverType value);

// Selects an algorithm for solving linear programs.
enum class LPAlgorithm {
  // The (primal) simplex method. Typically can provide primal and dual
  // solutions, primal/dual rays on primal/dual unbounded problems, and a basis.
  kPrimalSimplex = LP_ALGORITHM_PRIMAL_SIMPLEX,

  // The dual simplex method. Typically can provide primal and dual
  // solutions, primal/dual rays on primal/dual unbounded problems, and a basis.
  kDualSimplex = LP_ALGORITHM_DUAL_SIMPLEX,

  // The barrier method, also commonly called an interior point method (IPM).
  // Can typically give both primal and dual solutions. Some implementations can
  // also produce rays on unbounded/infeasible problems. A basis is not given
  // unless the underlying solver does "crossover" and finishes with simplex.
  kBarrier = LP_ALGORITHM_BARRIER
};

MATH_OPT_DEFINE_ENUM(LPAlgorithm, LP_ALGORITHM_UNSPECIFIED);

// Parses a flag of type LPAlgorithm.
//
// The expected values are the one returned by EnumToString().
bool AbslParseFlag(absl::string_view text, LPAlgorithm* value,
                   std::string* error);

// Unparses a flag of type LPAlgorithm.
//
// The returned values are the same as EnumToString().
std::string AbslUnparseFlag(LPAlgorithm value);

// Effort level applied to an optional task while solving (see SolveParameters
// for use).
//
// Typically used as a std::optional<Emphasis>. It used to configure a solver
// feature as follows:
//  * If a solver doesn't support the feature, only nullopt will always be
//    valid, any other setting will give an invalid argument error (some solvers
//    may also accept kOff).
//  * If the solver supports the feature:
//    - When unset, the underlying default is used.
//    - When the feature cannot be turned off, kOff will return an error.
//    - If the feature is enabled by default, the solver default is typically
//      mapped to kMedium.
//    - If the feature is supported, kLow, kMedium, kHigh, and kVeryHigh will
//      never give an error, and will map onto their best match.
enum class Emphasis {
  kOff = EMPHASIS_OFF,
  kLow = EMPHASIS_LOW,
  kMedium = EMPHASIS_MEDIUM,
  kHigh = EMPHASIS_HIGH,
  kVeryHigh = EMPHASIS_VERY_HIGH
};

MATH_OPT_DEFINE_ENUM(Emphasis, EMPHASIS_UNSPECIFIED);

// Parses a flag of type Emphasis.
//
// The expected values are the one returned by EnumToString().
bool AbslParseFlag(absl::string_view text, Emphasis* value, std::string* error);

// Unparses a flag of type Emphasis.
//
// The returned values are the same as EnumToString().
std::string AbslUnparseFlag(Emphasis value);

// Gurobi specific parameters for solving. See
//   https://www.gurobi.com/documentation/9.1/refman/parameters.html
// for a list of possible parameters.
//
// Example use:
//   GurobiParameters gurobi;
//   gurobi.param_values["BarIterLimit"] = "10";
//
// With Gurobi, the order that parameters are applied can have an impact in rare
// situations. Parameters are applied in the following order:
//  * LogToConsole is set from SolveParameters.enable_output.
//  * Any common parameters not overwritten by GurobiParameters.
//  * param_values in iteration order (insertion order).
// We set LogToConsole first because setting other parameters can generate
// output.
struct GurobiParameters {
  // Parameter name-value pairs to set in insertion order.
  gtl::linked_hash_map<std::string, std::string> param_values;

  GurobiParametersProto Proto() const;
  static GurobiParameters FromProto(const GurobiParametersProto& proto);

  bool empty() const { return param_values.empty(); }
};

// Parameters to control a single solve.
//
// Contains both parameters common to all solvers e.g. time_limit, and
// parameters for a specific solver, e.g. gscip. If a value is set in both
// common and solver specific field, the solver specific setting is used.
//
// The common parameters that are optional and unset indicate that the solver
// default is used.
//
// Solver specific parameters for solvers other than the one in use are ignored.
//
// Parameters that depends on the model (e.g. branching priority is set for
// each variable) are passed in ModelSolveParametersProto.
struct SolveParameters {
  // Enables printing the solver implementation traces. These traces are sent
  // to the standard output stream.
  //
  // Note that if the solver supports message callback and the user registers a
  // callback for it, then this parameter value is ignored and no traces are
  // printed.
  bool enable_output = false;

  // Maximum time a solver should spend on the problem.
  //
  // This value is not a hard limit, solve time may slightly exceed this value.
  // Always passed to the underlying solver, the solver default is not used.
  absl::Duration time_limit = absl::InfiniteDuration();

  // Limit on the iterations of the underlying algorithm (e.g. simplex pivots).
  // The specific behavior is dependent on the solver and algorithm used, but
  // often can give a deterministic solve limit (further configuration may be
  // needed, e.g. one thread).
  //
  // Typically supported by LP, QP, and MIP solvers, but for MIP solvers see
  // also node_limit.
  std::optional<int64_t> iteration_limit;

  // Limit on the number of subproblems solved in enumerative search (e.g.
  // branch and bound). For many solvers this can be used to deterministically
  // limit computation (further configuration may be needed, e.g. one thread).
  //
  // Typically for MIP solvers, see also iteration_limit.
  std::optional<int64_t> node_limit;

  // The solver stops early if it can prove there are no primal solutions at
  // least as good as cutoff.
  //
  // On an early stop, the solver returns termination reason kNoSolutionFound
  // and with limit kCutoff and is not required to give any extra solution
  // information. Has no effect on the return value if there is no early stop.
  //
  // It is recommended that you use a tolerance if you want solutions with
  // objective exactly equal to cutoff to be returned.
  //
  // See the user guide for more details and a comparison with best_bound_limit.
  std::optional<double> cutoff_limit;

  // The solver stops early as soon as it finds a solution at least this good,
  // with termination reason kFeasible and limit kObjective.
  std::optional<double> objective_limit;

  // The solver stops early as soon as it proves the best bound is at least this
  // good, with termination reason kFeasible or kNoSolutionFound and limit
  // kObjective.
  //
  // See the user guide for a comparison with cutoff_limit.
  std::optional<double> best_bound_limit;

  // The solver stops early after finding this many feasible solutions, with
  // termination reason kFeasible and limit kSolution. Must be greater than
  // zero if set. It is often used get the solver to stop on the first feasible
  // solution found. Note that there is no guarantee on the objective value for
  // any of the returned solutions.
  //
  // Solvers will typically not return more solutions than the solution limit,
  // but this is not enforced by MathOpt, see also b/214041169.
  //
  // Currently supported for Gurobi and SCIP, and for CP-SAT only with value 1.
  std::optional<int32_t> solution_limit;

  // If unset, use the solver default. If set, it must be >= 1.
  std::optional<int32_t> threads;

  // Seed for the pseudo-random number generator in the underlying
  // solver. Note that all solvers use pseudo-random numbers to select things
  // such as perturbation in the LP algorithm, for tie-break-up rules, and for
  // heuristic fixings. Varying this can have a noticeable impact on solver
  // behavior.
  //
  // Although all solvers have a concept of seeds, note that valid values
  // depend on the actual solver.
  // - Gurobi: [0:GRB_MAXINT] (which as of Gurobi 9.0 is 2x10^9).
  // - GSCIP:  [0:2147483647] (which is MAX_INT or kint32max or 2^31-1).
  // - GLOP:   [0:2147483647] (same as above)
  // In all cases, the solver will receive a value equal to:
  // MAX(0, MIN(MAX_VALID_VALUE_FOR_SOLVER, random_seed)).
  std::optional<int32_t> random_seed;

  // An absolute optimality tolerance (primarily) for MIP solvers.
  //
  // The absolute GAP is the absolute value of the difference between:
  //   * the objective value of the best feasible solution found,
  //   * the dual bound produced by the search.
  // The solver can stop once the absolute GAP is at most absolute_gap_tolerance
  // (when set), and return TerminationReason::kOptimal.
  //
  // Must be >= 0 if set.
  //
  // See also relative_gap_tolerance.
  std::optional<double> absolute_gap_tolerance;

  // A relative optimality tolerance (primarily) for MIP solvers.
  //
  // The relative GAP is a normalized version of the absolute GAP (defined on
  // absolute_gap_tolerance), where the normalization is solver-dependent, e.g.
  // the absolute GAP divided by the objective value of the best feasible
  // solution found.
  //
  // The solver can stop once the relative GAP is at most relative_gap_tolerance
  // (when set), and return TerminationReason::kOptimal.
  //
  // Must be >= 0 if set.
  //
  // See also absolute_gap_tolerance.
  std::optional<double> relative_gap_tolerance;

  // Maintain up to `solution_pool_size` solutions while searching. The solution
  // pool generally has two functions:
  //  (1) For solvers that can return more than one solution, this limits how
  //      many solutions will be returned.
  //  (2) Some solvers may run heuristics using solutions from the solution
  //      pool, so changing this value may affect the algorithm's path.
  // To force the solver to fill the solution pool, e.g. with the n best
  // solutions, requires further, solver specific configuration.
  std::optional<int32_t> solution_pool_size;

  // The algorithm for solving a linear program. If nullopt, use the solver
  // default algorithm.
  //
  // For problems that are not linear programs but where linear programming is
  // a subroutine, solvers may use this value. E.g. MIP solvers will typically
  // use this for the root LP solve only (and use dual simplex otherwise).
  std::optional<LPAlgorithm> lp_algorithm;

  // Effort on simplifying the problem before starting the main algorithm, or
  // the solver default effort level if unset.
  std::optional<Emphasis> presolve;

  // Effort on getting a stronger LP relaxation (MIP only) or the solver default
  // effort level if unset.
  //
  // NOTE: disabling cuts may prevent callbacks from having a chance to add cuts
  // at MIP_NODE, this behavior is solver specific.
  std::optional<Emphasis> cuts;

  // Effort in finding feasible solutions beyond those encountered in the
  // complete search procedure (MIP only), or the solver default effort level if
  // unset.
  std::optional<Emphasis> heuristics;

  // Effort in rescaling the problem to improve numerical stability, or the
  // solver default effort level if unset.
  std::optional<Emphasis> scaling;

  GScipParameters gscip;
  GurobiParameters gurobi;
  glop::GlopParameters glop;
  sat::SatParameters cp_sat;

  SolveParametersProto Proto() const;
  static absl::StatusOr<SolveParameters> FromProto(
      const SolveParametersProto& proto);
};

bool AbslParseFlag(absl::string_view text, SolveParameters* solve_parameters,
                   std::string* error);

std::string AbslUnparseFlag(SolveParameters solve_parameters);

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_CPP_PARAMETERS_H_
