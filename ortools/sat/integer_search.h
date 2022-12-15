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

#include <stdint.h>

#include <functional>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/time/time.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/implied_bounds.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/linear_programming_constraint.h"
#include "ortools/sat/model.h"
#include "ortools/sat/probing.h"
#include "ortools/sat/pseudo_costs.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/synchronization.h"
#include "ortools/util/strong_integers.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

// This is used to hold the next decision the solver will take. It is either
// a pure Boolean literal decision or correspond to an IntegerLiteral one.
//
// At most one of the two options should be set.
struct BooleanOrIntegerLiteral {
  BooleanOrIntegerLiteral() {}
  explicit BooleanOrIntegerLiteral(LiteralIndex index)
      : boolean_literal_index(index) {}
  explicit BooleanOrIntegerLiteral(IntegerLiteral i_lit)
      : integer_literal(i_lit) {}

  bool HasValue() const {
    return boolean_literal_index != kNoLiteralIndex ||
           integer_literal.var != kNoIntegerVariable;
  }

  LiteralIndex boolean_literal_index = kNoLiteralIndex;
  IntegerLiteral integer_literal = IntegerLiteral();
};

// Model struct that contains the search heuristics used to find a feasible
// solution to an integer problem.
//
// This is reset by ConfigureSearchHeuristics() and used by
// SolveIntegerProblem(), see below.
struct SearchHeuristics {
  // Decision and restart heuristics. The two vectors must be of the same size
  // and restart_policies[i] will always be used in conjunction with
  // decision_policies[i].
  std::vector<std::function<BooleanOrIntegerLiteral()>> decision_policies;
  std::vector<std::function<bool()>> restart_policies;

  // Index in the vectors above that indicate the current configuration.
  int policy_index;

  // Two special decision functions that are constructed at loading time.
  // These are used by ConfigureSearchHeuristics() to fill the policies above.
  std::function<BooleanOrIntegerLiteral()> fixed_search = nullptr;
  std::function<BooleanOrIntegerLiteral()> hint_search = nullptr;

  // This is currently only filled and used by PARTIAL_FIXED_SEARCH.
  // It contains only the part specified in the input cp model proto.
  std::function<BooleanOrIntegerLiteral()> user_search = nullptr;

  // Some search strategy need to take more than one decision at once. They can
  // set this function that will be called on the next decision. It will be
  // automatically deleted the first time it returns an empty decision.
  std::function<BooleanOrIntegerLiteral()> next_decision_override = nullptr;
};

// Given a base "fixed_search" function that should mainly control in which
// order integer variables are lazily instantiated (and at what value), this
// uses the current solver parameters to set the SearchHeuristics class in the
// given model.
void ConfigureSearchHeuristics(Model* model);

// Callbacks that will be called when the search goes back to level 0.
// Callbacks should return false if the propagation fails.
struct LevelZeroCallbackHelper {
  std::vector<std::function<bool()>> callbacks;
};

// Resets the solver to the given assumptions before calling
// SolveIntegerProblem().
SatSolver::Status ResetAndSolveIntegerProblem(
    const std::vector<Literal>& assumptions, Model* model);

// Only used in tests. Move to a test utility file.
//
// This configures the model SearchHeuristics with a simple default heuristic
// and then call ResetAndSolveIntegerProblem() without any assumptions.
SatSolver::Status SolveIntegerProblemWithLazyEncoding(Model* model);

// Returns decision corresponding to var at its lower bound.
// Returns an invalid literal if the variable is fixed.
IntegerLiteral AtMinValue(IntegerVariable var, IntegerTrail* integer_trail);

// If a variable appear in the objective, branch on its best objective value.
IntegerLiteral ChooseBestObjectiveValue(IntegerVariable var, Model* model);

// Returns decision corresponding to var >= lb + max(1, (ub - lb) / 2). It also
// CHECKs that the variable is not fixed.
IntegerLiteral GreaterOrEqualToMiddleValue(IntegerVariable var,
                                           IntegerTrail* integer_trail);

// This method first tries var <= value. If this does not reduce the domain it
// tries var >= value. If that also does not reduce the domain then returns
// an invalid literal.
IntegerLiteral SplitAroundGivenValue(IntegerVariable var, IntegerValue value,
                                     Model* model);

// Returns decision corresponding to var <= round(lp_value). If the variable
// does not appear in the LP, this method returns an invalid literal.
IntegerLiteral SplitAroundLpValue(IntegerVariable var, Model* model);

// Returns decision corresponding to var <= best_solution[var]. If no solution
// has been found, this method returns a literal with kNoIntegerVariable. This
// was suggested in paper: "Solution-Based Phase Saving for CP" (2018) by Emir
// Demirovic, Geoffrey Chu, and Peter J. Stuckey.
IntegerLiteral SplitDomainUsingBestSolutionValue(IntegerVariable var,
                                                 Model* model);

// Decision heuristic for SolveIntegerProblemWithLazyEncoding(). Returns a
// function that will return the literal corresponding to the fact that the
// first currently non-fixed variable value is <= its min. The function will
// return kNoLiteralIndex if all the given variables are fixed.
//
// Note that this function will create the associated literal if needed.
std::function<BooleanOrIntegerLiteral()> FirstUnassignedVarAtItsMinHeuristic(
    const std::vector<IntegerVariable>& vars, Model* model);

// Decision heuristic for SolveIntegerProblemWithLazyEncoding(). Like
// FirstUnassignedVarAtItsMinHeuristic() but the function will return the
// literal corresponding to the fact that the currently non-assigned variable
// with the lowest min has a value <= this min.
std::function<BooleanOrIntegerLiteral()>
UnassignedVarWithLowestMinAtItsMinHeuristic(
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
std::function<BooleanOrIntegerLiteral()> FollowHint(
    const std::vector<BooleanOrIntegerVariable>& vars,
    const std::vector<IntegerValue>& values, Model* model);

// Combines search heuristics in order: if the i-th one returns kNoLiteralIndex,
// ask the (i+1)-th. If every heuristic returned kNoLiteralIndex,
// returns kNoLiteralIndex.
std::function<BooleanOrIntegerLiteral()> SequentialSearch(
    std::vector<std::function<BooleanOrIntegerLiteral()>> heuristics);

// Changes the value of the given decision by 'var_selection_heuristic'. We try
// to see if the decision is "associated" with an IntegerVariable, and if it is
// the case, we choose the new value by the first 'value_selection_heuristics'
// that is applicable. If none of the heuristics are applicable then the given
// decision by 'var_selection_heuristic' is returned.
std::function<BooleanOrIntegerLiteral()> SequentialValueSelection(
    std::vector<std::function<IntegerLiteral(IntegerVariable)>>
        value_selection_heuristics,
    std::function<BooleanOrIntegerLiteral()> var_selection_heuristic,
    Model* model);

// Changes the value of the given decision by 'var_selection_heuristic'
// according to various value selection heuristics. Looks at the code to know
// exactly what heuristic we use.
std::function<BooleanOrIntegerLiteral()> IntegerValueSelectionHeuristic(
    std::function<BooleanOrIntegerLiteral()> var_selection_heuristic,
    Model* model);

// Returns the BooleanOrIntegerLiteral advised by the underliying SAT solver.
std::function<BooleanOrIntegerLiteral()> SatSolverHeuristic(Model* model);

// Gets the branching variable using pseudo costs and combines it with a value
// for branching.
std::function<BooleanOrIntegerLiteral()> PseudoCost(Model* model);

// Simple scheduling heuristic that looks at all the no-overlap constraints
// and try to assign and perform the intervals that can be scheduled first.
std::function<BooleanOrIntegerLiteral()> SchedulingSearchHeuristic(
    Model* model);

// Returns true if the number of variables in the linearized part represent
// a large enough proportion of all the problem variables.
bool LinearizedPartIsLarge(Model* model);

// A restart policy that restarts every k failures.
std::function<bool()> RestartEveryKFailures(int k, SatSolver* solver);

// A restart policy that uses the underlying sat solver's policy.
std::function<bool()> SatSolverRestartPolicy(Model* model);

// Concatenates each input_heuristic with a default heuristic that instantiate
// all the problem's Boolean variables, into a new vector.
std::vector<std::function<BooleanOrIntegerLiteral()>> CompleteHeuristics(
    const std::vector<std::function<BooleanOrIntegerLiteral()>>&
        incomplete_heuristics,
    const std::function<BooleanOrIntegerLiteral()>& completion_heuristic);

// Specialized search that will continuously probe Boolean variables and bounds
// of integer variables.
SatSolver::Status ContinuousProbing(
    const std::vector<BooleanVariable>& bool_vars,
    const std::vector<IntegerVariable>& int_vars, Model* model);

// An helper class to share the code used by the different kind of search.
class IntegerSearchHelper {
 public:
  explicit IntegerSearchHelper(Model* model);

  // Executes some code before a new decision.
  // Returns false if model is UNSAT.
  bool BeforeTakingDecision();

  // Calls the decision heuristics and extract a non-fixed literal.
  // Note that we do not want to copy the function here.
  LiteralIndex GetDecision(const std::function<BooleanOrIntegerLiteral()>& f);

  // Tries to take the current decision, this might backjump.
  // Returns false if the model is UNSAT.
  bool TakeDecision(Literal decision);

  // Tries to find a feasible solution to the current model.
  //
  // This function continues from the current state of the solver and loop until
  // all variables are instantiated (i.e. the next decision is kNoLiteralIndex)
  // or a search limit is reached. It uses the heuristic from the
  // SearchHeuristics class in the model to decide when to restart and what next
  // decision to take.
  //
  // Each time a restart happen, this increment the policy index modulo the
  // number of heuristics to act as a portfolio search.
  SatSolver::Status SolveIntegerProblem();

 private:
  const SatParameters& parameters_;
  Model* model_;
  SatSolver* sat_solver_;
  IntegerTrail* integer_trail_;
  IntegerEncoder* encoder_;
  ImpliedBounds* implied_bounds_;
  Prober* prober_;
  ProductDetector* product_detector_;
  TimeLimit* time_limit_;
  PseudoCosts* pseudo_costs_;
  IntegerVariable objective_var_ = kNoIntegerVariable;
};

// This class will loop continuously on model variables and try to probe/shave
// its bounds.
class ContinuousProber {
 public:
  // The model_proto is just used to construct the lists of variable to probe.
  ContinuousProber(const CpModelProto& model_proto, Model* model);

  // Starts or continues probing variables and their bounds.
  // It returns:
  //   - SatSolver::INFEASIBLE if the problem is proven infeasible.
  //   - SatSolver::FEASIBLE when a feasible solution is found
  //   - SatSolver::LIMIT_REACHED if the limit stored in the model is reached
  // Calling Probe() after it has returned FEASIBLE or LIMIT_REACHED will resume
  // probing from its previous state.
  SatSolver::Status Probe();

 private:
  bool ImportFromSharedClasses();
  SatSolver::Status ShaveLiteral(Literal literal);
  bool ReportStatus(const SatSolver::Status status);
  void LogStatistics();

  // Variables to probe.
  std::vector<BooleanVariable> bool_vars_;
  std::vector<IntegerVariable> int_vars_;

  // Model object.
  Model* model_;
  SatSolver* sat_solver_;
  TimeLimit* time_limit_;
  Trail* trail_;
  IntegerTrail* integer_trail_;
  IntegerEncoder* encoder_;
  const SatParameters parameters_;
  LevelZeroCallbackHelper* level_zero_callbacks_;
  Prober* prober_;
  SharedResponseManager* shared_response_manager_;
  SharedBoundsManager* shared_bounds_manager_;

  // Statistics.
  int64_t num_literals_probed_ = 0;
  int64_t num_bounds_shaved_ = 0;
  int64_t num_bounds_tried_ = 0;

  // Current state of the probe.
  double active_limit_;
  // TODO(user): use 2 vector<bool>.
  absl::flat_hash_set<BooleanVariable> probed_bool_vars_;
  absl::flat_hash_set<LiteralIndex> probed_literals_;
  int iteration_ = 1;
  absl::Time last_logging_time_;
  int current_int_var_ = 0;
  int current_bool_var_ = 0;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_INTEGER_SEARCH_H_
