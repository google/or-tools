// Copyright 2010-2025 Google LLC
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

#ifndef ORTOOLS_SAT_INTEGER_SEARCH_H_
#define ORTOOLS_SAT_INTEGER_SEARCH_H_

#include <stdint.h>

#include <functional>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/types/span.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/implied_bounds.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/model.h"
#include "ortools/sat/probing.h"
#include "ortools/sat/pseudo_costs.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_inprocessing.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"
#include "ortools/util/strong_integers.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

// This is used to hold the next decision the solver will take. It is either
// a pure Boolean literal decision or correspond to an IntegerLiteral one.
//
// At most one of the two options should be set.
struct BooleanOrIntegerLiteral {
  BooleanOrIntegerLiteral() = default;
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

  // Special decision functions that are constructed at loading time.
  // These are used by ConfigureSearchHeuristics() to fill the policies above.

  // Contains the search specified by the user in CpModelProto.
  std::function<BooleanOrIntegerLiteral()> user_search = nullptr;

  // Heuristic search build after introspecting the model. It can be used as
  // a replacement of the user search. This can include dedicated scheduling or
  // routing heuristics.
  std::function<BooleanOrIntegerLiteral()> heuristic_search = nullptr;

  // Default integer heuristic that will fix all integer variables.
  std::function<BooleanOrIntegerLiteral()> integer_completion_search = nullptr;

  // Fixed search, built from above building blocks.
  std::function<BooleanOrIntegerLiteral()> fixed_search = nullptr;

  // The search heuristic aims at following the given hint with minimum
  // deviation.
  std::function<BooleanOrIntegerLiteral()> hint_search = nullptr;

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
IntegerLiteral ChooseBestObjectiveValue(
    IntegerVariable var, IntegerTrail* integer_trail,
    ObjectiveDefinition* objective_definition);

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
    absl::Span<const IntegerVariable> vars, Model* model);

// Choose the variable with most fractional LP value.
std::function<BooleanOrIntegerLiteral()> MostFractionalHeuristic(Model* model);

// Variant used for LbTreeSearch experimentation. Note that each decision is in
// O(num_variables), but it is kind of ok with LbTreeSearch as we only call this
// for "new" decision, not when we move around in the tree.
std::function<BooleanOrIntegerLiteral()> BoolPseudoCostHeuristic(Model* model);
std::function<BooleanOrIntegerLiteral()> LpPseudoCostHeuristic(Model* model);

// Decision heuristic for SolveIntegerProblemWithLazyEncoding(). Like
// FirstUnassignedVarAtItsMinHeuristic() but the function will return the
// literal corresponding to the fact that the currently non-assigned variable
// with the lowest min has a value <= this min.
std::function<BooleanOrIntegerLiteral()>
UnassignedVarWithLowestMinAtItsMinHeuristic(
    absl::Span<const IntegerVariable> vars, Model* model);

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
    absl::Span<const BooleanOrIntegerVariable> vars,
    absl::Span<const IntegerValue> values, Model* model);

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

// Returns the BooleanOrIntegerLiteral advised by the underlying SAT solver.
std::function<BooleanOrIntegerLiteral()> SatSolverHeuristic(Model* model);

// Gets the branching variable using pseudo costs and combines it with a value
// for branching.
std::function<BooleanOrIntegerLiteral()> PseudoCost(Model* model);

// Simple scheduling heuristic that looks at all the no-overlap constraints
// and try to assign and perform the intervals that can be scheduled first.
std::function<BooleanOrIntegerLiteral()> SchedulingSearchHeuristic(
    Model* model);

// Compared to SchedulingSearchHeuristic() this one take decision on precedences
// between tasks. Lazily creating a precedence Boolean for the task in
// disjunction.
//
// Note that this one is meant to be used when all Boolean has been fixed, so
// more as a "completion" heuristic rather than a fixed search one.
std::function<BooleanOrIntegerLiteral()> DisjunctivePrecedenceSearchHeuristic(
    Model* model);
std::function<BooleanOrIntegerLiteral()> CumulativePrecedenceSearchHeuristic(
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
    absl::Span<const std::function<BooleanOrIntegerLiteral()>>
        incomplete_heuristics,
    const std::function<BooleanOrIntegerLiteral()>& completion_heuristic);

// An helper class to share the code used by the different kind of search.
class IntegerSearchHelper {
 public:
  explicit IntegerSearchHelper(Model* model);

  // Executes some code before a new decision.
  //
  // Tricky: return false if the model is UNSAT or if the assumptions are UNSAT.
  // One can distinguish with sat_solver->UnsatStatus().
  ABSL_MUST_USE_RESULT bool BeforeTakingDecision();

  // Calls the decision heuristics and extract a non-fixed literal.
  // Note that we do not want to copy the function here.
  //
  // Returns false if a conflict was found while trying to take a decision.
  bool GetDecision(const std::function<BooleanOrIntegerLiteral()>& f,
                   LiteralIndex* decision);

  // Inner function used by GetDecision().
  // It will create a new associated literal if needed.
  LiteralIndex GetDecisionLiteral(const BooleanOrIntegerLiteral& decision);

  // Functions passed to GetDecision() might call this to notify a conflict
  // was detected.
  void NotifyThatConflictWasFoundDuringGetDecision() {
    must_process_conflict_ = true;
  }

  // Tries to take the current decision, this might backjump. If
  // use_representative is true, the representative of the decision is taken
  // instead. Returns false if the model is UNSAT.
  bool TakeDecision(Literal decision, bool use_representative = true);

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
  BinaryImplicationGraph* binary_implication_graph_;
  IntegerTrail* integer_trail_;
  IntegerEncoder* encoder_;
  ImpliedBounds* implied_bounds_;
  Prober* prober_;
  ProductDetector* product_detector_;
  TimeLimit* time_limit_;
  PseudoCosts* pseudo_costs_;
  Inprocessing* inprocessing_;

  bool must_process_conflict_ = false;
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
  static const int kTestLimitPeriod = 20;
  static const int kLogPeriod = 5000;
  static const int kSyncPeriod = 50;

  SatSolver::Status ShaveLiteral(Literal literal);
  bool ReportStatus(SatSolver::Status status);
  void LogStatistics();
  SatSolver::Status PeriodicSyncAndCheck();

  // Variables to probe.
  std::vector<BooleanVariable> bool_vars_;
  std::vector<IntegerVariable> int_vars_;

  // Model object.
  Model* model_;
  SatSolver* sat_solver_;
  TimeLimit* time_limit_;
  BinaryImplicationGraph* binary_implication_graph_;
  ClauseManager* clause_manager_;
  Trail* trail_;
  IntegerTrail* integer_trail_;
  IntegerEncoder* encoder_;
  Inprocessing* inprocessing_;
  const SatParameters parameters_;
  LevelZeroCallbackHelper* level_zero_callbacks_;
  Prober* prober_;
  SharedResponseManager* shared_response_manager_;
  SharedBoundsManager* shared_bounds_manager_;
  ModelRandomGenerator* random_;

  // Statistics.
  int64_t num_literals_probed_ = 0;
  int64_t num_bounds_shaved_ = 0;
  int64_t num_bounds_tried_ = 0;
  int64_t num_at_least_one_probed_ = 0;
  int64_t num_at_most_one_probed_ = 0;

  // Period counters;
  int num_logs_remaining_ = 0;
  int num_syncs_remaining_ = 0;
  int num_test_limit_remaining_ = 0;

  // Shaving management.
  bool use_shaving_ = false;
  int trail_index_at_start_of_iteration_ = 0;
  int integer_trail_index_at_start_of_iteration_ = 0;

  // Current state of the probe.
  double active_limit_;
  // TODO(user): use 2 vector<bool>.
  absl::flat_hash_set<BooleanVariable> probed_bool_vars_;
  absl::flat_hash_set<LiteralIndex> shaved_literals_;
  int iteration_ = 1;
  int current_int_var_ = 0;
  int current_bool_var_ = 0;
  int current_bv1_ = 0;
  int current_bv2_ = 0;
  int random_pair_of_bool_vars_probed_ = 0;
  int random_triplet_of_bool_vars_probed_ = 0;
  std::vector<std::vector<Literal>> tmp_dnf_;
  std::vector<Literal> tmp_literals_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_INTEGER_SEARCH_H_
