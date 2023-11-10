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

// A very simple branch-and-bound solver for MIPs, using MathOpt to solve the LP
// relaxation at every node.
//
// This example:
//  * Demonstrates incremental solving with MathOpt.
//  * Shows how to process various termination reasons for an LP solver.
//  * Can be used a skeleton for a custom branch and bound.
//
// This implementation of branch and bound does not do cut generation, does not
// have any primal heuristics, and uses a very naive branching rule (most
// fractional variable). It cannot solve large problems, it just demonstrates a
// few techniques.
#ifndef OR_TOOLS_MATH_OPT_EXAMPLES_CPP_BRANCH_AND_BOUND_H_
#define OR_TOOLS_MATH_OPT_EXAMPLES_CPP_BRANCH_AND_BOUND_H_

#include <ostream>

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace operations_research_example {

// Configuration for SolveWithBranchAndBound().
struct BranchAndBoundParameters {
  // Used to solve the underlying LP relaxation of the model (when all integer
  // variables are made continuous). The solver selected must be able to solve
  // the LP relaxation for a given input model (e.g. use OSQP if the problem is
  // quadratic and convex).
  operations_research::math_opt::SolverType lp_solver =
      operations_research::math_opt::SolverType::kGlop;

  // If progress should be printed to standard output.
  bool enable_output = false;

  // The criteria on solution quality for termination.
  //
  // Let obj* be the objective value of the best solution found, and bound* be
  // the dual bound found from search. For minimization (bound* <= obj*), we
  // stop when:
  //  obj* - bound* <= abs_gap_tolerance + rel_gap_tolerance * obj*.
  double abs_gap_tolerance = 1.0e-4;

  // Se abs_gap_tolerance for details.
  double rel_gap_tolerance = 1.0e-4;

  // A solution found the LP solver is feasible for the integrality constraints
  // if every integer variable takes a value within
  // `integrality_absolute_tolerance` of some integer.
  double integrality_absolute_tolerance = 1.0e-5;

  // A limit on how long to run the solver for.
  absl::Duration time_limit = absl::InfiniteDuration();
};

// The result of SolveWithBranchAndBound().
struct SimpleSolveResult {
  // The reason a BranchAndBoundSolve terminated.
  enum class Reason {
    // Solved the problem to optimality
    kOptimal,
    // Found a feasible solution, but hit a limit (e.g., time limit).
    kFeasible,
    // Hit a limit (e.g., time limit) without finding any solution.
    kNoSolution,
    // The problem was provably primal infeasible.
    kInfeasible,
    // A primal ray was found, or the LP solver returned infeasible or
    // unbounded.
    kInfeasibleOrUnbounded,
    // Something went wrong, including an imprecise LP solve.
    kError
  };

  // A summary of the resources used during SolveWithBranchAndBound().
  struct Stats {
    // How long the function took.
    absl::Duration solve_time;

    // The number of simplex pivots to solve the root LP relaxation.
    int64_t root_pivots = 0;

    // The number of simplex pivots for all nodes in the search tree (excluding
    // the root LP relaxation).
    int64_t tree_pivots = 0;

    // The number of nodes in the search tree created.
    int64_t nodes_created = 0;

    // The number of nodes in the search tree processed.
    int64_t nodes_closed = 0;
  };

  // The reason SolveWithBranchAndBound() stopped for this solve.
  Reason termination_reason = Reason::kNoSolution;

  // The best solution found in the search, or empty if no solution was found.
  //
  // For problems with at least one variable, will be non-empty iff
  // `termination_reason` is kFeasible or kOptimal.
  absl::flat_hash_map<operations_research::math_opt::Variable, double>
      variable_values;

  // The objective value of the best solution found.
  //
  // Is +inf for minimization and -inf for maximization if no solution is found.
  double primal_bound = 0.0;

  // A bound on objective value of any solution for this problem.
  //
  // For minimization, the bound is less than `primal_bound` (up to tolerances),
  // and -inf if no bound is found.
  //
  // For maximization, the bound is greater than `primal_bound` (up to
  // tolerances) and is +inf if no bound is found.
  double dual_bound = 0.0;

  // A summary of the resources used during this SolveWithBranchAndBound().
  Stats stats;
};

// Solves the optimization problem `model` with the branch and bound algorithm.
//
// The LP relaxation of `model` is taken by simply converting all integer
// variables to continuous variables. If the underlying solver (from
// `params.lp_solver`) supports this model, the function will succeed. Note that
// no special action is taken to relax SOS constraints, so if Gurobi is your
// underlying solver, you will solve a MIP at each node.
//
// Callers must ensure that the underlying solver for the LP relaxation is
// linked in their binary.
absl::StatusOr<SimpleSolveResult> SolveWithBranchAndBound(
    const operations_research::math_opt::Model& model,
    const BranchAndBoundParameters& params = {});

////////////////////////////////////////////////////////////////////////////////
// Implementation details/visible for testing
////////////////////////////////////////////////////////////////////////////////

std::ostream& operator<<(std::ostream& ostr, SimpleSolveResult::Reason reason);
std::ostream& operator<<(std::ostream& ostr,
                         const SimpleSolveResult::Stats& stats);

// Has termination reason kNoSolutionFound, trivial primal and dual bounds,
// and empty variable_values.
SimpleSolveResult TrivialSolveResult(bool is_maximize,
                                     const SimpleSolveResult::Stats& stats);

// Has termination reason kError, trivial primal and dual bounds, and empty
// variable_values.
SimpleSolveResult ErrorSolveResult(bool is_maximize,
                                   const SimpleSolveResult::Stats& stats);

// Has termination reason kInfeasible, trivial primal and dual bounds, and empty
// variable_values.
SimpleSolveResult InfeasibleSolveResult(bool is_maximize,
                                        const SimpleSolveResult::Stats& stats);

// Has termination reason kInfeasibleOrUnbounded, trivial primal and dual
// bounds, and empty variable_values.
SimpleSolveResult InfeasibleOrUnboundedSolveResult(
    bool is_maximize, const SimpleSolveResult::Stats& stats);

}  // namespace operations_research_example

#endif  // OR_TOOLS_MATH_OPT_EXAMPLES_CPP_BRANCH_AND_BOUND_H_
