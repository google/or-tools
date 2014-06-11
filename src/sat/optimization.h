// Copyright 2010-2013 Google
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
// Optimization algorithms to solve a LinearBooleanProblem by using the SAT
// solver as a black-box.
//
// TODO(user): Currently, only the MINIMIZATION problem type is supported.

#ifndef OR_TOOLS_SAT_OPTIMIZATION_H_
#define OR_TOOLS_SAT_OPTIMIZATION_H_

#include "sat/boolean_problem.h"
#include "sat/sat_solver.h"

namespace operations_research {
namespace sat {

// Randomizes the decision heuristic of the given SatParameters.
// This is what SolveWithRandomParameters() below currently use.
void RandomizeDecisionHeuristic(MTRandom* random, SatParameters* parameters);

// Because the Solve*() functions below are also used in scripts that requires a
// special output format, we use this to tell them whether or not to use the
// default logging framework or simply stdout. Most users should just use
// DEFAULT_LOG.
enum LogBehavior { DEFAULT_LOG, STDOUT_LOG };

// All the Solve*() functions below reuse the SatSolver::Status with a slightly
// different meaning:
// - MODEL_SAT: The problem has been solved to optimality.
// - MODEL_UNSAT: Same meaning, the decision version is already unsat.
// - LIMIT_REACHED: we may have some feasible solution (if solution is
//   non-empty), but the optimality is not proved.

// Implements the "Fu & Malik" algorithm described in:
// Zhaohui Fu, Sharad Malik, "On solving the Partial MAX-SAT problem", 2006,
// International Conference on Theory and Applications of Satisfiability
// Testing. (SAT’06), LNCS 4121.
//
// This algorithm requires all the objective weights to be the same (CHECKed)
// and currently only works on minization problems. The problem is assumed to be
// already loaded into the given solver.
//
// TODO(user): double-check the correctness if the objective coefficients are
// negative.
SatSolver::Status SolveWithFuMalik(LogBehavior log,
                                   const LinearBooleanProblem& problem,
                                   SatSolver* solver, std::vector<bool>* solution);

// The WPM1 algorithm is a generalization of the Fu & Malik algorithm to
// weighted problems. Note that if all objective weights are the same, this is
// almost the same as SolveWithFuMalik() but the encoding of the constraints is
// slightly different.
//
// Ansotegui, C., Bonet, M.L., Levy, J.: Solving (weighted) partial MaxSAT
// through satisﬁability testing. In: Proc. of the 12th Int. Conf. on Theory and
// Applications of Satisﬁability Testing (SAT’09). pp. 427–440 (2009)
SatSolver::Status SolveWithWPM1(LogBehavior log,
                                const LinearBooleanProblem& problem,
                                SatSolver* solver, std::vector<bool>* solution);

// Solves num_times the decision version of the given problem with different
// random parameters. Keep the best solution (regarding the objective) and
// returns it in solution. The problem is assumed to be already loaded into the
// given solver.
SatSolver::Status SolveWithRandomParameters(LogBehavior log,
                                            const LinearBooleanProblem& problem,
                                            int num_times, SatSolver* solver,
                                            std::vector<bool>* solution);

// Starts by solving the decision version of the given LinearBooleanProblem and
// then simply add a constraint to find a lower objective that the current best
// solution and repeat until the problem becomes unsat.
//
// The problem is assumed to be already loaded into the given solver. If
// solution is initially a feasible solution, the search will starts from there.
// solution will be updated with the best solution found so far.
SatSolver::Status SolveWithLinearScan(LogBehavior log,
                                      const LinearBooleanProblem& problem,
                                      SatSolver* solver,
                                      std::vector<bool>* solution);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_OPTIMIZATION_H_
