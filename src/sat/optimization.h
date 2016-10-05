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

// Optimization algorithms to solve a LinearBooleanProblem by using the SAT
// solver as a black-box.
//
// TODO(user): Currently, only the MINIMIZATION problem type is supported.

#ifndef OR_TOOLS_SAT_OPTIMIZATION_H_
#define OR_TOOLS_SAT_OPTIMIZATION_H_

#include "sat/boolean_problem.h"
#include "sat/integer.h"
#include "sat/model.h"
#include "sat/sat_solver.h"

namespace operations_research {
namespace sat {

// Tries to minimize the given UNSAT core with a really simple heuristic.
// The idea is to remove literals that are consequences of others in the core.
// We already know that in the initial order, no literal is propagated by the
// one before it, so we just look for propagation in the reverse order.
//
// Important: The given SatSolver must be the one that just produced the given
// core.
void MinimizeCore(SatSolver* solver, std::vector<Literal>* core);

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

// Similar algorithm as the one used by qmaxsat, this is a linear scan with the
// at-most k constraint encoded in SAT. This only works on problem with constant
// weights.
SatSolver::Status SolveWithCardinalityEncoding(
    LogBehavior log, const LinearBooleanProblem& problem, SatSolver* solver,
    std::vector<bool>* solution);

// This is an original algorithm. It is a mix between the cardinality encoding
// and the Fu & Malik algorithm. It also works on general weighted instances.
SatSolver::Status SolveWithCardinalityEncodingAndCore(
    LogBehavior log, const LinearBooleanProblem& problem, SatSolver* solver,
    std::vector<bool>* solution);

// Model-based API, for now we just provide a basic algorithm that minimize a
// given IntegerVariable by solving a sequence of decision problem.
//
// The "observer" function will be called each time a new feasible solution is
// found.
SatSolver::Status MinimizeIntegerVariableWithLinearScan(
    IntegerVariable objective_var,
    const std::function<void(const Model&)>& feasible_solution_observer,
    Model* model);

// Same as MinimizeIntegerVariableWithLinearScan() but as long as the domain of
// the variables in var_for_lazy_encoding is not a singleton when the problem is
// solved to SAT (i.e all the Boolean variables are assigned), we add a new
// literal that can constrain the non-singleton variable with the lowest lower
// bound to its lower bound. We exploit the default polarity of the solver
// to make sure the first decision when resolving from the current state will be
// to force this variable to its lower bound.
//
// Note(user): For now, we pass the set of var_for_lazy_encoding which must
// contain a set of "decision" variables from which everything else can be
// deduced. On simple model like the jobshop, passing all the variables there
// have the same effect (but we don't want to pass their negation, or we need
// to change the heuristic to not fix variable to their upper bound).
//
// TODO(user): If it is too much work to provide this set, we could add a more
// complex heuristic to decide the next IntegerLiteral that will be associated
// to a decision in the SatSolver search.
SatSolver::Status MinimizeIntegerVariableWithLinearScanAndLazyEncoding(
    bool log_info, IntegerVariable objective_var,
    const std::vector<IntegerVariable>& var_for_lazy_encoding,
    const std::function<void(const Model&)>& feasible_solution_observer,
    Model* model);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_OPTIMIZATION_H_
