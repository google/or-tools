// Copyright 2010-2018 Google LLC
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

#ifndef OR_TOOLS_SAT_INTEGER_SEARCH_H_
#define OR_TOOLS_SAT_INTEGER_SEARCH_H_

#include <vector>

#include "ortools/sat/integer.h"
#include "ortools/sat/sat_solver.h"

namespace operations_research {
namespace sat {

// Decision heuristic for SolveIntegerProblemWithLazyEncoding(). Returns a
// function that will return the literal corresponding to the fact that the
// first currently non-fixed variable value is <= its min. The function will
// return kNoLiteralIndex if all the given variables are fixed.
//
// Note that this function will create the associated literal if needed.
std::function<LiteralIndex()> FirstUnassignedVarAtItsMinHeuristic(
    const std::vector<IntegerVariable>& vars, Model* model);

// Decision heuristic for SolveIntegerProblemWithLazyEncoding(). Like
// FirstUnassignedVarAtItsMinHeuristic() but the function will return the
// literal corresponding to the fact that the currently non-assigned variable
// with the lowest min has a value <= this min.
std::function<LiteralIndex()> UnassignedVarWithLowestMinAtItsMinHeuristic(
    const std::vector<IntegerVariable>& vars, Model* model);

// Set the first unassigned Literal/Variable to its value.
//
// TODO(user): This is currently quadratic as we scan all variables to find the
// first unassigned one. Fix. Note that this is also the case in many other
// heuristics and should be fixed.
struct BooleanOrIntegerVariable {
  BooleanVariable bool_var = kNoBooleanVariable;
  IntegerVariable int_var = kNoIntegerVariable;
};
std::function<LiteralIndex()> FollowHint(
    const std::vector<BooleanOrIntegerVariable>& vars,
    const std::vector<IntegerValue>& values, Model* model);

// Combines search heuristics in order: if the i-th one returns kNoLiteralIndex,
// ask the (i+1)-th. If every heuristic returned kNoLiteralIndex,
// returns kNoLiteralIndex.
std::function<LiteralIndex()> SequentialSearch(
    std::vector<std::function<LiteralIndex()>> heuristics);

// Returns the LiteralIndex advised by the underliying SAT solver.
std::function<LiteralIndex()> SatSolverHeuristic(Model* model);

// Uses the given heuristics, but when the LP relaxation has an integer
// solution, use it to change the polarity of the next decision so that the
// solver will check if this integer LP solution satisfy all the constraints.
std::function<LiteralIndex()> ExploitIntegerLpSolution(
    std::function<LiteralIndex()> heuristic, Model* model);

// A restart policy that restarts every k failures.
std::function<bool()> RestartEveryKFailures(int k, SatSolver* solver);

// A restart policy that uses the underlying sat solver's policy.
std::function<bool()> SatSolverRestartPolicy(Model* model);

// Appends model-owned automatic heuristics to input_heuristics in a new vector.
std::vector<std::function<LiteralIndex()>> AddModelHeuristics(
    const std::vector<std::function<LiteralIndex()>>& input_heuristics,
    Model* model);

// Concatenates each input_heuristic with a default heuristic that instantiate
// all the problem's Boolean variables, into a new vector.
std::vector<std::function<LiteralIndex()>> CompleteHeuristics(
    const std::vector<std::function<LiteralIndex()>>& incomplete_heuristics,
    const std::function<LiteralIndex()>& completion_heuristic);

// A wrapper around SatSolver::Solve that handles integer variable with lazy
// encoding. Repeatedly calls SatSolver::Solve() on the model until the given
// next_decision() function return kNoLiteralIndex or the model is proved to
// be UNSAT.
//
// Returns the status of the last call to SatSolver::Solve().
//
// Note that the next_decision() function must always return an unassigned
// literal or kNoLiteralIndex to end the search.
SatSolver::Status SolveIntegerProblemWithLazyEncoding(
    const std::vector<Literal>& assumptions,
    const std::function<LiteralIndex()>& next_decision, Model* model);

// Solves a problem with the given heuristics.
// heuristics[i] will be used with restart_policies[i] only.
SatSolver::Status SolveProblemWithPortfolioSearch(
    std::vector<std::function<LiteralIndex()>> decision_policies,
    std::vector<std::function<bool()>> restart_policies, Model* model);

// Shortcut for SolveIntegerProblemWithLazyEncoding() when there is no
// assumption and we consider all variables in their index order for the next
// search decision.
SatSolver::Status SolveIntegerProblemWithLazyEncoding(Model* model);

// Store relationship between the CpSolverResponse objective and the internal
// IntegerVariable the solver tries to minimize.
struct ObjectiveSynchronizationHelper {
  double scaling_factor = 1.0;
  double offset = 0.0;
  IntegerVariable objective_var = kNoIntegerVariable;
  std::function<double()> get_external_bound = nullptr;

  int64 UnscaledObjective(double value) const {
    return static_cast<int64>(std::round(value / scaling_factor - offset));
  }
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_INTEGER_SEARCH_H_
