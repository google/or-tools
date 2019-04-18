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

// This file contains all the top-level logic responsible for driving the search
// of a satisfiability integer problem. What decision we take next, which new
// Literal associated to an IntegerLiteral we create and when we restart.
//
// For an optimization problem, our algorithm solves a sequence of decision
// problem using this file as an entry point. Note that some heuristics here
// still use the objective if there is one in order to orient the search towards
// good feasible solution though.

#ifndef OR_TOOLS_SAT_INTEGER_SEARCH_H_
#define OR_TOOLS_SAT_INTEGER_SEARCH_H_

#include <vector>

#include "ortools/sat/integer.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"

namespace operations_research {
namespace sat {

// Model struct that contains the search heuristics used to find a feasible
// solution to an integer problem.
//
// This is reset by ConfigureSearchHeuristics() and used by
// SolveIntegerProblem(), see below.
struct SearchHeuristics {
  // Decision and restart heuristics. The two vectors must be of the same size
  // and restart_policies[i] will always be used in conjunction with
  // decision_policies[i].
  std::vector<std::function<LiteralIndex()>> decision_policies;
  std::vector<std::function<bool()>> restart_policies;

  // Index in the vector above that indicate the current configuration.
  int policy_index;
};

// Given a base "fixed_search" function that should mainly control in which
// order integer variables are lazily instantiated (and at what value), this
// uses the current solver parameters to set the SearchHeuristics class in the
// given model.
void ConfigureSearchHeuristics(
    const std::function<LiteralIndex()>& fixed_search, Model* model);

// For an optimization problem, this contains the internal integer objective
// to minimize and information on how to display it correctly in the logs.
struct ObjectiveDefinition {
  double scaling_factor = 1.0;
  double offset = 0.0;
  IntegerVariable objective_var = kNoIntegerVariable;

  double ScaleIntegerObjective(IntegerValue value) const {
    return (ToDouble(value) + offset) * scaling_factor;
  }
};

// Callbacks that will be called when the search goes back to level 0.
// Callbacks should return false if the propagation fails.
struct LevelZeroCallbackHelper {
  std::vector<std::function<bool()>> callbacks;
};

// Tries to find a feasible solution to the current model.
//
// This function continues from the current state of the solver and loop until
// all variables are instantiated (i.e. the next decision is kNoLiteralIndex) or
// a search limit is reached. It uses the heuristic from the SearchHeuristics
// class in the model to decide when to restart and what next decision to take.
//
// Each time a restart happen, this increment the policy index modulo the number
// of heuristics to act as a portfolio search.
SatSolver::Status SolveIntegerProblem(Model* model);

// Resets the solver to the given assumptions before calling
// SolveIntegerProblem().
SatSolver::Status ResetAndSolveIntegerProblem(
    const std::vector<Literal>& assumptions, Model* model);

// Only used in tests. Move to a test utility file.
//
// This configures the model SearchHeuristics with a simple default heuristic
// and then call ResetAndSolveIntegerProblem() without any assumptions.
SatSolver::Status SolveIntegerProblemWithLazyEncoding(Model* model);

// Returns decision corresponding to var at its lower bound. Returns
// kNoLiteralIndex if the variable is fixed.
LiteralIndex AtMinValue(IntegerVariable var, IntegerTrail* integer_trail,
                        IntegerEncoder* integer_encoder);

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
