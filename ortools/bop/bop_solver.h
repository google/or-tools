// Copyright 2010-2017 Google
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

#ifndef OR_TOOLS_BOP_BOP_SOLVER_H_
#define OR_TOOLS_BOP_BOP_SOLVER_H_

// Solver for Boolean Optimization Problems built on top of the SAT solver.
// To optimize a problem the solver uses several optimization strategies like
// Local Search (LS), Large Neighborhood Search (LNS), and Linear
// Programming (LP). See bop_parameters.proto to tune the strategies.
//
// Note that the BopSolver usage is limited to:
//   - Boolean variables,
//   - Linear constraints and linear optimization objective,
//   - Integral weights for both constraints and objective,
//   - Minimization.
// To deal with maximization, integral variables and floating weights, one can
// use the bop::IntegralSolver.
//
// Usage example:
//   const LinearBooleanProblem problem = BuildProblem();
//   BopSolver bop_solver(problem);
//   BopParameters bop_parameters;
//   bop_parameters.set_max_deterministic_time(10);
//   bop_solver.SetParameters(bop_parameters);
//   const BopSolveStatus solve_status = bop_solver.Solve();
//   if (solve_status == BopSolveStatus::OPTIMAL_SOLUTION_FOUND) { ... }

#include <string>
#include <vector>

#include "ortools/base/basictypes.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/base/int_type.h"
#include "ortools/base/int_type_indexed_vector.h"
#include "ortools/bop/bop_base.h"
#include "ortools/bop/bop_parameters.pb.h"
#include "ortools/bop/bop_solution.h"
#include "ortools/bop/bop_types.h"
#include "ortools/glop/lp_solver.h"
#include "ortools/sat/boolean_problem.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/util/stats.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace bop {
// Solver of Boolean Optimization Problems based on Local Search.
class BopSolver {
 public:
  explicit BopSolver(const LinearBooleanProblem& problem);
  virtual ~BopSolver();

  // Parameters management.
  void SetParameters(const BopParameters& parameters) {
    parameters_ = parameters;
  }

  // Returns the status of the optimization.
  BopSolveStatus Solve();
  BopSolveStatus Solve(const BopSolution& first_solution);

  // Runs the solver with an external time limit.
  BopSolveStatus SolveWithTimeLimit(TimeLimit* time_limit);
  BopSolveStatus SolveWithTimeLimit(const BopSolution& first_solution,
                                    TimeLimit* time_limit);

  const BopSolution& best_solution() const { return problem_state_.solution(); }
  bool GetSolutionValue(VariableIndex var_id) const {
    return problem_state_.solution().Value(var_id);
  }

  // Returns the scaled best bound.
  // In case of minimization (resp. maximization), the best bound is defined as
  // the lower bound (resp. upper bound).
  double GetScaledBestBound() const;
  double GetScaledGap() const;

 private:
  void UpdateParameters();
  BopSolveStatus InternalMonothreadSolver(TimeLimit* time_limit);
  BopSolveStatus InternalMultithreadSolver(TimeLimit* time_limit);

  const LinearBooleanProblem& problem_;
  ProblemState problem_state_;
  BopParameters parameters_;

  mutable StatsGroup stats_;
};
}  // namespace bop
}  // namespace operations_research
#endif  // OR_TOOLS_BOP_BOP_SOLVER_H_
