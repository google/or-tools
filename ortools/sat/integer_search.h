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
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"

namespace operations_research {
namespace sat {

// Returns decision corresponding to var at its lower bound. Returns
// kNoLiteralIndex if the variable is fixed.
LiteralIndex AtMinValue(IntegerVariable var, Model* model);

// Returns decision corresponding to var >= lb + max(1, (ub - lb) / 2). It also
// CHECKs that the variable is not fixed.
LiteralIndex GreaterOrEqualToMiddleValue(IntegerVariable var, Model* model);

// This method first tries var <= value. If this does not reduce the domain it
// tries var >= value. If that also does not reduce the domain then returns
// kNoLiteralIndex.
LiteralIndex SplitAroundGivenValue(IntegerVariable positive_var,
                                   IntegerValue value, Model* model);

// Returns decision corresponding to var <= round(lp_value). If the variable
// does not appear in the LP, this method returns kNoLiteralIndex.
LiteralIndex SplitAroundLpValue(IntegerVariable var, Model* model);

struct SolutionDetails {
  int64 solution_count = 0;
  gtl::ITIVector<IntegerVariable, IntegerValue> best_solution;

  // Loads the solution in best_solution using lower bounds from integer trail.
  void LoadFromTrail(const IntegerTrail& integer_trail);
};

// Returns decision corresponding to var <= best_solution[var]. If no solution
// has been found, this method returns kNoLiteralIndex. This was suggested in
// paper: "Solution-Based Phase Saving for CP" (2018) by Emir Demirovic,
// Geoffrey Chu, and Peter J. Stuckey
LiteralIndex SplitDomainUsingBestSolutionValue(IntegerVariable var,
                                               Model* model);

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

// Changes the value of the given decision by 'var_selection_heuristic'. We try
// to see if the decision is "associated" with an IntegerVariable, and if it is
// the case, we choose the new value by the first 'value_selection_heuristics'
// that is applicable (return value != kNoLiteralIndex). If none of the
// heuristics are applicable then the given decision by
// 'var_selection_heuristic' is returned.
std::function<LiteralIndex()> SequentialValueSelection(
    std::vector<std::function<LiteralIndex(IntegerVariable)>>
        value_selection_heuristics,
    std::function<LiteralIndex()> var_selection_heuristic, Model* model);

// Changes the value of the given decision by 'var_selection_heuristic'. The new
// value is chosen in the following way:
// 1) If the LP part is large and the LP solution is exploitable, change the
//    value to LP solution value.
// 2) Else if there is at least one solution found, change the value to the best
//    solution value.
// If none of the above heuristics are applicable then it uses the default value
// given in the decision.
std::function<LiteralIndex()> IntegerValueSelectionHeuristic(
    std::function<LiteralIndex()> var_selection_heuristic, Model* model);

// Returns the LiteralIndex advised by the underliying SAT solver.
std::function<LiteralIndex()> SatSolverHeuristic(Model* model);

// Gets the branching variable using pseudo costs and combines it with a value
// for branching.
std::function<LiteralIndex()> PseudoCost(Model* model);

// Uses the given heuristics, but when the LP relaxation has a solution, use it
// to change the polarity of the next decision. This is only done for integer
// solutions unless 'exploit_all_lp_solution' parameter is set to true. For
// integer solution the solver will check if this integer LP solution satisfy
// all the constraints.
//
// Note that we only do this if a big enough percentage of the problem variables
// appear in the LP relaxation.
std::function<LiteralIndex()> ExploitLpSolution(
    std::function<LiteralIndex()> heuristic, Model* model);

// Similar to ExploitLpSolution(). Takes LiteralIndex as base decision and
// changes change the returned decision to AtLpValue() of the underlying integer
// variable if LP solution is exploitable.
LiteralIndex ExploitLpSolution(const LiteralIndex decision, Model* model);

// Returns true if we currently have an available LP solution that is
// "exploitable" according to the current parameters.
bool LpSolutionIsExploitable(Model* model);

// Returns true if the number of variables in the linearized part is at least
// 0.5 times the total number of variables.
bool LinearizedPartIsLarge(Model* model);

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
  std::function<double()> get_external_best_objective = nullptr;
  std::function<double()> get_external_best_bound = nullptr;
  std::function<void(double, double)> set_external_best_bound = nullptr;
  bool parallel_mode = false;

  int64 UnscaledObjective(double value) const {
    return static_cast<int64>(std::round(value / scaling_factor - offset));
  }
  double ScaledObjective(int64 value) const {
    return (value + offset) * scaling_factor;
  }
};

// Callbacks that be called when the search goes back to level 0.
// Callbacks should return false if the propagation fails.
struct LevelZeroCallbackHelper {
  std::vector<std::function<bool()>> callbacks;
};

// Prints out a new optimization solution in a fixed format.
void LogNewSolution(const std::string& event_or_solution_count,
                    double time_in_seconds, double obj_lb, double obj_ub,
                    const std::string& solution_info);

// Prints out a new satisfiability solution in a fixed format.
void LogNewSatSolution(const std::string& event_or_solution_count,
                       double time_in_seconds,
                       const std::string& solution_info);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_INTEGER_SEARCH_H_
