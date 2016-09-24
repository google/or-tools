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

// This file implements a SAT solver.
// see http://en.wikipedia.org/wiki/Boolean_satisfiability_problem
// for more detail.
// TODO(user): Expand.

#ifndef OR_TOOLS_SAT_SAT_SOLVER_H_
#define OR_TOOLS_SAT_SAT_SOLVER_H_

#include "base/hash.h"
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "base/integral_types.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/timer.h"
#include "google/protobuf/text_format.h"
#include "base/int_type.h"
#include "base/int_type_indexed_vector.h"
#include "base/map_util.h"
#include "sat/clause.h"
#include "sat/drat.h"
#include "sat/model.h"
#include "sat/pb_constraint.h"
#include "sat/sat_parameters.pb.h"
#include "util/bitset.h"
#include "util/running_stat.h"
#include "util/stats.h"
#include "util/time_limit.h"
#include "base/random.h"
#include "base/adjustable_priority_queue.h"

namespace operations_research {
namespace sat {

// A constant used by the EnqueueDecision*() API.
const int kUnsatTrailIndex = -1;

// The main SAT solver.
// It currently implements the CDCL algorithm. See
//    http://en.wikipedia.org/wiki/Conflict_Driven_Clause_Learning
class SatSolver {
 public:
  SatSolver();
  explicit SatSolver(Trail* trail);
  ~SatSolver();

  static SatSolver* CreateInModel(Model* model) {
    Trail* trail = model->GetOrCreate<Trail>();
    SatSolver* solver = new SatSolver(trail);
    model->TakeOwnership(solver);
    return solver;
  }

  // Parameters management. Note that calling SetParameters() will reset the
  // value of many heuristics. For instance:
  // - The restart strategy will be reinitialized.
  // - The random seed will be reset to the value given in parameters.
  // - The time limit will be reset and counted from there.
  void SetParameters(const SatParameters& parameters);
  const SatParameters& parameters() const;

  // Increases the number of variables of the current problem.
  //
  // TODO(user): Rename to IncreaseNumVariablesTo() until we support removing
  // variables...
  void SetNumVariables(int num_variables);
  int NumVariables() const { return num_variables_.value(); }
  BooleanVariable NewBooleanVariable() {
    const int num_vars = NumVariables();

    // We need to be able to encode the variable as a literal.
    CHECK_LT(2 * num_vars, std::numeric_limits<int32>::max());
    SetNumVariables(num_vars + 1);
    return BooleanVariable(num_vars);
  }

  // Fixes a variable so that the given literal is true. This can be used to
  // solve a subproblem where some variables are fixed. Note that it is more
  // efficient to add such unit clause before all the others.
  // Returns false if the problem is detected to be UNSAT.
  bool AddUnitClause(Literal true_literal);

  // Same as AddProblemClause() below, but for small clauses.
  //
  // TODO(user): Remove this and AddUnitClause() when initializer lists can be
  // used in the open-source code like in AddClause({a, b}).
  bool AddBinaryClause(Literal a, Literal b);
  bool AddTernaryClause(Literal a, Literal b, Literal c);

  // Adds a clause to the problem. Returns false if the problem is detected to
  // be UNSAT.
  //
  // TODO(user): Rename this to AddClause().
  bool AddProblemClause(const std::vector<Literal>& literals);

  // Adds a pseudo-Boolean constraint to the problem. Returns false if the
  // problem is detected to be UNSAT. If the constraint is always true, this
  // detects it and does nothing.
  //
  // Note(user): There is an optimization if the same constraint is added
  // consecutively (even if the bounds are different). This is particularly
  // useful for an optimization problem when we want to constrain the objective
  // of the problem more and more. Just re-adding such constraint is relatively
  // efficient.
  //
  // OVERFLOW: The sum of the absolute value of all the coefficients
  // in the constraint must not overflow. This is currently CHECKed().
  // TODO(user): Instead of failing, implement an error handling code.
  bool AddLinearConstraint(bool use_lower_bound, Coefficient lower_bound,
                           bool use_upper_bound, Coefficient upper_bound,
                           std::vector<LiteralWithCoeff>* cst);

  // Returns true if the model is UNSAT. Note that currently the status is
  // "sticky" and once this happen, nothing else can be done with the solver.
  //
  // Thanks to this function, a client can safely ignore the return value of any
  // Add*() functions. If one of them return false, then IsModelUnsat() will
  // return true.
  bool IsModelUnsat() const { return is_model_unsat_; }

  // Adds and registers the given propagator with the sat solver. Note that
  // during propagation, they will be called in the order they where added.
  void AddPropagator(std::unique_ptr<Propagator> propagator);
  void AddLastPropagator(std::unique_ptr<Propagator> propagator);

  // Gives a hint so the solver tries to find a solution with the given literal
  // set to true. Currently this take precedence over the phase saving heuristic
  // and a variable with a preference will always be branched on according to
  // this preference.
  //
  // The weight is used as a tie-breaker between variable with the same
  // activities. Larger weight will be selected first. A weight of zero is the
  // default value for the other variables.
  //
  // Note(user): Having a lot of different weights may slow down the priority
  // queue operations if there is millions of variables.
  void SetAssignmentPreference(Literal literal, double weight);

  // Returns the vector of the current assignment preferences.
  std::vector<std::pair<Literal, double>> AllPreferences() const;

  // Reinitializes the decision heuristics (which variables to choose with which
  // polarity) according to the current parameters. Note that this also resets
  // the activity of the variables to 0.
  void ResetDecisionHeuristic();
  void ResetDecisionHeuristicAndSetAllPreferences(
      const std::vector<std::pair<Literal, double>>& prefs);

  // Solves the problem and returns its status.
  // An empty problem is considered to be SAT.
  //
  // Note that the conflict limit applies only to this function and starts
  // counting from the time it is called.
  //
  // This will restart from the current solver configuration. If a previous call
  // to Solve() was interrupted by a conflict or time limit, calling this again
  // will resume the search exactly as it would have continued.
  //
  // Note that if a time limit has been defined earlier, using the SAT
  // parameters or the SolveWithTimeLimit() method, it is still in use in this
  // solve. To override the timelimit one should reset the parameters or use the
  // SolveWithTimeLimit() with a new time limit.
  enum Status {
    ASSUMPTIONS_UNSAT,
    MODEL_UNSAT,
    MODEL_SAT,
    LIMIT_REACHED,
  };
  Status Solve();

  // Same as Solve(), but with a given time limit. Note that this is slightly
  // redundant with the max_time_in_seconds() and
  // max_time_in_deterministic_seconds() parameters, but because SetParameters()
  // resets more than just the time limit, it is useful to have this more
  // specific api.
  Status SolveWithTimeLimit(TimeLimit* time_limit);

  // Simple interface to solve a problem under the given assumptions. This
  // simply ask the solver to solve a problem given a set of variables fixed to
  // a given value (the assumptions). Compared to simply calling AddUnitClause()
  // and fixing the variables once and for all, this allow to backtrack over the
  // assumptions and thus exploit the incrementally between subsequent solves.
  //
  // This function backtrack over all the current decision, tries to enqueue the
  // given assumptions, sets the assumption level accordingly and finally calls
  // Solve().
  //
  // If, given these assumptions, the model is UNSAT, this returns the
  // ASSUMPTIONS_UNSAT status. MODEL_UNSAT is reserved for the case where the
  // model is proven to be unsat without any assumptions.
  //
  // If ASSUMPTIONS_UNSAT is returned, it is possible to get a "core" of unsat
  // assumptions by calling GetLastIncompatibleDecisions().
  Status ResetAndSolveWithGivenAssumptions(const std::vector<Literal>& assumptions);

  // Changes the assumption level. All the decisions below this level will be
  // treated as assumptions by the next Solve(). Note that this may impact some
  // heuristics, like the LBD value of a clause.
  void SetAssumptionLevel(int assumption_level);

  // Returns the current assumption level. Note that if a solve was done since
  // the last SetAssumptionLevel(), then the returned level may be lower than
  // the one that was set. This is because some assumptions may now be
  // consequences of others before them due to the newly learned clauses.
  int AssumptionLevel() const { return assumption_level_; }

  // This can be called just after SolveWithAssumptions() returned
  // ASSUMPTION_UNSAT or after EnqueueDecisionAndBacktrackOnConflict() leaded
  // to a conflict. It returns a subsequence (in the correct order) of the
  // previously enqueued decisions that cannot be taken together without making
  // the problem UNSAT.
  std::vector<Literal> GetLastIncompatibleDecisions();

  // Advanced usage. The next 3 functions allow to drive the search from outside
  // the solver.

  // Takes a new decision (the given true_literal must be unassigned) and
  // propagates it. Returns the trail index of the first newly propagated
  // literal. If there is a conflict and the problem is detected to be UNSAT,
  // returns kUnsatTrailIndex.
  //
  // A client can determine if there is a conflict by checking if the
  // CurrentDecisionLevel() was increased by 1 or not.
  //
  // If there is a conflict, the given decision is not applied and:
  // - The conflict is learned.
  // - The decisions are potentially backtracked to the first decision that
  //   propagates more variables because of the newly learned conflict.
  // - The returned value is equal to trail_->Index() after this backtracking
  //   and just before the new propagation (due to the conflict) which is also
  //   performed by this function.
  int EnqueueDecisionAndBackjumpOnConflict(Literal true_literal);

  // This function starts by calling EnqueueDecisionAndBackjumpOnConflict(). If
  // there is no conflict, it stops there. Otherwise, it tries to reapply all
  // the decisions that where backjumped over until the first one that can't be
  // taken because it is incompatible. Note that during this process, more
  // conflicts may happen and the trail may be backtracked even further.
  //
  // In any case, the new decisions stack will be the largest valid "prefix"
  // of the old stack. Note that decisions that are now consequence of the ones
  // before them will no longer be decisions.
  //
  // Note(user): This function can be called with an already assigned literal,
  // in which case, it will just do nothing.
  int EnqueueDecisionAndBacktrackOnConflict(Literal true_literal);

  // Tries to enqueue the given decision and performs the propagation.
  // Returns true if no conflict occured. Otherwise, returns false and restores
  // the solver to the state just before this was called.
  //
  // Note(user): With this function, the solver doesn't learn anything.
  bool EnqueueDecisionIfNotConflicting(Literal true_literal);

  // Restores the state to the given target decision level. The decision at that
  // level and all its propagation will not be undone. But all the trail after
  // this will be cleared. Calling this with 0 will revert all the decisions and
  // only the fixed variables will be left on the trail.
  void Backtrack(int target_level);

  // Advanced usage. This is meant to restore the solver to a "proper" state
  // after a solve was interupted due to a limit reached.
  //
  // Without assumption (i.e. if AssumptionLevel() is 0), this will revert all
  // decisions and make sure that all the fixed literals are propagated. In
  // presence of assumptions, this will either backtrack to the assumption level
  // or re-enqueue any assumptions that may have been backtracked over due to
  // conflits resolution. In both cases, the propagation is finished.
  //
  // Note that this may prove the model to be UNSAT (check IsModelUnsat()).
  void RestoreSolverToAssumptionLevel();

  // Extract the current problem clauses. The Output type must support the two
  // functions:
  //  - void AddBinaryClause(Literal a, Literal b);
  //  - void AddClause(ClauseRef clause);
  //
  // TODO(user): also copy the learned clauses?
  template <typename Output>
  void ExtractClauses(Output* out) {
    CHECK(!IsModelUnsat());
    Backtrack(0);
    out->SetNumVariables(NumVariables());

    // It is important to process the newly fixed variables, so they are not
    // present in the clauses we export.
    if (num_processed_fixed_variables_ < trail_->Index()) {
      ProcessNewlyFixedVariables();
    }
    DeleteDetachedClauses();

    // Note(user): Putting the binary clauses first help because the presolver
    // currently process the clauses in order.
    binary_implication_graph_.ExtractAllBinaryClauses(out);
    for (SatClause* clause : clauses_) {
      if (!clause->IsRedundant()) {
        out->AddClause(ClauseRef(clause->begin(), clause->end()));
      }
    }
  }

  // Functions to manage the set of learned binary clauses.
  // Only clauses added/learned when TrackBinaryClause() is true are managed.
  void TrackBinaryClauses(bool value) { track_binary_clauses_ = value; }
  bool AddBinaryClauses(const std::vector<BinaryClause>& clauses);
  const std::vector<BinaryClause>& NewlyAddedBinaryClauses();
  void ClearNewlyAddedBinaryClauses();

  struct Decision {
    Decision() : trail_index(-1) {}
    Decision(int i, Literal l) : trail_index(i), literal(l) {}
    int trail_index;
    Literal literal;
  };

  // Note that the Decisions() vector is always of size NumVariables(), and that
  // only the first CurrentDecisionLevel() entries have a meaning.
  const std::vector<Decision>& Decisions() const { return decisions_; }
  int CurrentDecisionLevel() const { return current_decision_level_; }
  const Trail& LiteralTrail() const { return *trail_; }
  const VariablesAssignment& Assignment() const { return trail_->Assignment(); }

  // Some statistics since the creation of the solver.
  int64 num_branches() const;
  int64 num_failures() const;
  int64 num_propagations() const;

  // A deterministic number that should be correlated with the time spent in
  // the Solve() function. The order of magnitude should be close to the time
  // in seconds.
  double deterministic_time() const;

  // Only used for debugging. Save the current assignment in debug_assignment_.
  // The idea is that if we know that a given assignment is satisfiable, then
  // all the learned clauses or PB constraints must be satisfiable by it. In
  // debug mode, and after this is called, all the learned clauses are tested to
  // satisfy this saved assignement.
  void SaveDebugAssignment();

  // Returns true iff the loaded problem only contains clauses.
  bool ProblemIsPureSat() const { return problem_is_pure_sat_; }

  void SetDratWriter(DratWriter* drat_writer) { drat_writer_ = drat_writer; }

  // This function is here to deal with the case where a SAT/CP model is found
  // to be trivially UNSAT while the user is constructing the model. Instead of
  // having to test the status of all the lines adding a constraint, one can
  // just check if the solver is not UNSAT once the model is constructed. Note
  // that we usually log a warning on the first constraint that caused a
  // "trival" unsatisfiability.
  void NotifyThatModelIsUnsat() { is_model_unsat_ = true; }

  // TODO(user): This internal function should probably not be exposed here.
  void AddBinaryClauseDuringSearch(Literal a, Literal b) {
    // The new clause should not propagate.
    CHECK(!trail_->Assignment().LiteralIsFalse(a));
    CHECK(!trail_->Assignment().LiteralIsFalse(b));
    binary_implication_graph_.AddBinaryClause(a, b);
  }

  // Performs propagation of the recently enqueued elements.
  // Mainly visible for testing.
  bool Propagate();

 private:
  // Calls Propagate() and returns true if no conflict occured. Otherwise,
  // learns the conflict, backtracks, enqueues the consequence of the learned
  // conflict and returns false.
  bool PropagateAndStopAfterOneConflictResolution();

  // All Solve() functions end up calling this one.
  Status SolveInternal(TimeLimit* time_limit);

  // Adds a binary clause to the BinaryImplicationGraph and to the
  // BinaryClauseManager when track_binary_clauses_ is true.
  void AddBinaryClauseInternal(Literal a, Literal b);

  // See SaveDebugAssignment(). Note that these functions only consider the
  // variables at the time the debug_assignment_ was saved. If new variables
  // where added since that time, they will be considered unassigned.
  bool ClauseIsValidUnderDebugAssignement(const std::vector<Literal>& clause) const;
  bool PBConstraintIsValidUnderDebugAssignment(
      const std::vector<LiteralWithCoeff>& cst, const Coefficient rhs) const;

  // Logs the given status if parameters_.log_search_progress() is true.
  // Also returns it.
  Status StatusWithLog(Status status);

  // Main function called from SolveWithAssumptions() or from Solve() with an
  // assumption_level of 0 (meaning no assumptions).
  Status SolveInternal(int assumption_level);

  // Applies the previous decisions (which are still on decisions_), in order,
  // starting from the one at the current decision level. Stops at the one at
  // decisions_[level] or on the first decision already propagated to "false"
  // and thus incompatible.
  //
  // Note that during this process, conflicts may arise which will lead to
  // backjumps. In this case, we will simply keep reapplying decisions from the
  // last one backtracked over and so on.
  //
  // Returns MODEL_STAT if no conflict occured, MODEL_UNSAT if the model was
  // proven unsat and ASSUMPTION_UNSAT otherwise. In the last case the first non
  // taken old decision will be propagated to false by the ones before.
  //
  // first_propagation_index will be filled with the trail index of the first
  // newly propagated literal, or with -1 if MODEL_UNSAT is returned.
  Status ReapplyDecisionsUpTo(int level, int* first_propagation_index);

  // Returns false if the thread memory is over the limit.
  bool IsMemoryLimitReached() const;

  // Sets is_model_unsat_ to true and return false.
  bool SetModelUnsat();

  // Utility function to insert spaces proportional to the search depth.
  // It is used in the pretty print of the search.
  std::string Indent() const;

  // Returns the decision level of a given variable.
  int DecisionLevel(BooleanVariable var) const {
    return trail_->Info(var).level;
  }

  // Returns the relevant pointer if the given variable was propagated by the
  // constraint in question. This is used to bump the activity of the learned
  // clauses or pb constraints.
  SatClause* ReasonClauseOrNull(BooleanVariable var) const;
  UpperBoundedLinearConstraint* ReasonPbConstraintOrNull(
      BooleanVariable var) const;

  // This does one step of a pseudo-Boolean resolution:
  // - The variable var has been assigned to l at a given trail_index.
  // - The reason for var propagates it to l.
  // - The conflict propagates it to not(l)
  // The goal of the operation is to combine the two constraints in order to
  // have a new conflict at a lower trail_index.
  //
  // Returns true if the reason for var was a normal clause. In this case,
  // the *slack is updated to its new value.
  bool ResolvePBConflict(BooleanVariable var,
                         MutableUpperBoundedLinearConstraint* conflict,
                         Coefficient* slack);

  // Returns true iff the clause is the reason for an assigned variable.
  //
  // TODO(user): With our current data structures, we could also return true
  // for clauses that where just used as a reason (like just before an untrail).
  // This may be beneficial, but should properly be defined so that we can
  // have the same behavior if we change the implementation.
  bool ClauseIsUsedAsReason(SatClause* clause) const {
    const BooleanVariable var = clause->PropagatedLiteral().Variable();
    return trail_->Info(var).trail_index < trail_->Index() &&
           (*trail_)[trail_->Info(var).trail_index].Variable() == var &&
           ReasonClauseOrNull(var) == clause;
  }

  // Add a problem clause. Not that the clause is assumed to be "cleaned", that
  // is no duplicate variables (not strictly required) and not empty.
  bool AddProblemClauseInternal(const std::vector<Literal>& literals);

  // This is used by all the Add*LinearConstraint() functions. It detects
  // infeasible/trivial constraints or clause constraints and takes the proper
  // action.
  bool AddLinearConstraintInternal(const std::vector<LiteralWithCoeff>& cst,
                                   Coefficient rhs, Coefficient max_value);

  // Adds a learned clause to the problem. This should be called after
  // Backtrack(). The backtrack is such that after it is applied, all the
  // literals of the learned close except one will be false. Thus the last one
  // will be implied True. This function also Enqueue() the implied literal.
  void AddLearnedClauseAndEnqueueUnitPropagation(
      const std::vector<Literal>& literals, bool must_be_kept);

  // Creates a new decision which corresponds to setting the given literal to
  // True and Enqueue() this change.
  void EnqueueNewDecision(Literal literal);

  // Returns true if everything has been propagated.
  bool PropagationIsDone() const;

  // Update the propagators_ list with the relevant propagators.
  void InitializePropagators();

  // Asks for the next decision to branch upon. This shouldn't be called if
  // there is no active variable (i.e. unassigned variable).
  Literal NextBranch();

  // Unrolls the trail until a given point. This unassign the assigned variables
  // and add them to the priority queue with the correct weight.
  void Untrail(int target_trail_index);

  // Deletes all the clauses that are detached.
  void DeleteDetachedClauses();

  // Simplifies the problem when new variables are assigned at level 0.
  void ProcessNewlyFixedVariables();

  // Compute an initial variable ordering.
  void InitializeVariableOrdering();

  // Returns the maximum trail_index of the literals in the given clause.
  // All the literals must be assigned. Returns -1 if the clause is empty.
  int ComputeMaxTrailIndex(ClauseRef clause) const;

  // Computes what is known as the first UIP (Unique implication point) conflict
  // clause starting from the failing clause. For a definition of UIP and a
  // comparison of the different possible conflict clause computation, see the
  // reference below.
  //
  // The conflict will have only one literal at the highest decision level, and
  // this literal will always be the first in the conflict vector.
  //
  // L Zhang, CF Madigan, MH Moskewicz, S Malik, "Efficient conflict driven
  // learning in a boolean satisfiability solver" Proceedings of the 2001
  // IEEE/ACM international conference on Computer-aided design, Pages 279-285.
  // http://www.cs.tau.ac.il/~msagiv/courses/ATP/iccad2001_final.pdf
  void ComputeFirstUIPConflict(
      int max_trail_index, std::vector<Literal>* conflict,
      std::vector<Literal>* reason_used_to_infer_the_conflict,
      std::vector<SatClause*>* subsumed_clauses);

  // Fills literals with all the literals in the reasons of the literals in the
  // given input. The output vector will have no duplicates and will not contain
  // the literals already present in the input.
  void ComputeUnionOfReasons(const std::vector<Literal>& input,
                             std::vector<Literal>* literals);

  // Given an assumption (i.e. literal) currently assigned to false, this will
  // returns the set of all assumptions that caused this particular assignment.
  //
  // This is useful to get a small set of assumptions that can't be all
  // satisfied together.
  void FillUnsatAssumptions(Literal false_assumption,
                            std::vector<Literal>* unsat_assumptions);

  // Do the full pseudo-Boolean constraint analysis. This calls multiple
  // time ResolvePBConflict() on the current conflict until we have a conflict
  // that allow us to propagate more at a lower decision level. This level
  // is the one returned in backjump_level.
  void ComputePBConflict(int max_trail_index, Coefficient initial_slack,
                         MutableUpperBoundedLinearConstraint* conflict,
                         int* backjump_level);

  // Applies some heuristics to a conflict in order to minimize its size and/or
  // replace literals by other literals from lower decision levels. The first
  // function choose which one of the other functions to call depending on the
  // parameters.
  //
  // Precondidtion: is_marked_ should be set to true for all the variables of
  // the conflict. It can also contains false non-conflict variables that
  // are implied by the negation of the 1-UIP conflict literal.
  void MinimizeConflict(std::vector<Literal>* conflict,
                        std::vector<Literal>* reason_used_to_infer_the_conflict);
  void MinimizeConflictExperimental(std::vector<Literal>* conflict);
  void MinimizeConflictSimple(std::vector<Literal>* conflict);
  void MinimizeConflictRecursively(std::vector<Literal>* conflict);

  // Utility function used by MinimizeConflictRecursively().
  bool CanBeInferedFromConflictVariables(BooleanVariable variable);

  // To be used in DCHECK(). Verifies some property of the conflict clause:
  // - There is an unique literal with the highest decision level.
  // - This literal appears in the first position.
  // - All the other literals are of smaller decision level.
  // - Ther is no literal with a decision level of zero.
  bool IsConflictValid(const std::vector<Literal>& literals);

  // Given the learned clause after a conflict, this computes the correct
  // backtrack level to call Backtrack() with.
  int ComputeBacktrackLevel(const std::vector<Literal>& literals);

  // The LBD (Literal Blocks Distance) is the number of different decision
  // levels at which the literals of the clause were assigned. Note that we
  // ignore the decision level 0 whereas the definition in the paper below
  // doesn't:
  //
  // G. Audemard, L. Simon, "Predicting Learnt Clauses Quality in Modern SAT
  // Solver" in Twenty-first International Joint Conference on Artificial
  // Intelligence (IJCAI'09), july 2009.
  // http://www.ijcai.org/papers09/Papers/IJCAI09-074.pdf
  //
  // IMPORTANT: All the literals of the clause must be assigned, and the first
  // literal must be of the highest decision level. This will be the case for
  // all the reason clauses.
  template <typename LiteralList>
  int ComputeLbd(const LiteralList& literals);

  // Checks if we need to reduce the number of learned clauses and do
  // it if needed. Also updates the learned clause limit for the next cleanup.
  void CleanClauseDatabaseIfNeeded();

  // Bumps the activity of all variables appearing in the conflict.
  // See VSIDS decision heuristic: Chaff: Engineering an Efficient SAT Solver.
  // M.W. Moskewicz et al. ANNUAL ACM IEEE DESIGN AUTOMATION CONFERENCE 2001.
  //
  // The second argument implements the Glucose strategy to reward good
  // variables. Variables from the last decision level and with a reason of LBD
  // lower than this limit and learned are bumped twice. Note that setting
  // bump_again_lbd_limit to 0 disable this feature.
  void BumpVariableActivities(const std::vector<Literal>& literals,
                              int bump_again_lbd_limit);

  // Rescales activity value of all variables when one of them reached the max.
  void RescaleVariableActivities(double scaling_factor);

  // Updates the increment used for activity bumps. This is basically the same
  // as decaying all the variable activities, but it is a lot more efficient.
  void UpdateVariableActivityIncrement();

  // Activity managment for clauses. This work the same way at the ones for
  // variables, but with different parameters.
  void BumpReasonActivities(const std::vector<Literal>& literals);
  void BumpClauseActivity(SatClause* clause);
  void RescaleClauseActivities(double scaling_factor);
  void UpdateClauseActivityIncrement();

  // Reinitializes the polarity of all the variables with an index greater than
  // or equal to the given one.
  void ResetPolarity(BooleanVariable from);

  // Init restart period.
  void InitRestart();

  std::string DebugString(const SatClause& clause) const;
  std::string StatusString(Status status) const;
  std::string RunningStatisticsString() const;

  BooleanVariable num_variables_;

  // All the clauses managed by the solver (initial and learned). This vector
  // has ownership of the pointers. We currently do not use
  // std::unique_ptr<SatClause> because it can't be used with some STL
  // algorithms like std::partition.
  //
  // Note that the unit clauses are not kept here and if the parameter
  // treat_binary_clauses_separately is true, the binary clause are not kept
  // here either.
  std::vector<SatClause*> clauses_;

  // Clause information used for the clause database management.
  // Note that only the clauses that can be removed need to appear here.
  struct ClauseInfo {
    double activity = 0.0;
    int32 lbd = 0;
    bool protected_during_next_cleanup = false;
  };
  hash_map<SatClause*, ClauseInfo> clauses_info_;

  // Internal propagators. We keep them here because we need more than the
  // Propagator interface for them.
  LiteralWatchers clauses_propagator_;
  BinaryImplicationGraph binary_implication_graph_;
  PbConstraints pb_constraints_;

  // Ordered list of propagators used by Propagate()/Untrail().
  std::vector<Propagator*> propagators_;

  // Ordered list of propagators added with AddPropagator().
  std::vector<std::unique_ptr<Propagator>> external_propagators_;
  std::unique_ptr<Propagator> last_propagator_;

  // Keep track of all binary clauses so they can be exported.
  bool track_binary_clauses_;
  BinaryClauseManager binary_clauses_;

  // The solver trail.
  Trail* trail_;

  // This is used by the non-model constructor to properly cleanup trail_.
  std::unique_ptr<Trail> owned_trail_;

  // Used for debugging only. See SaveDebugAssignment().
  VariablesAssignment debug_assignment_;

  // The stack of decisions taken by the solver. They are stored in [0,
  // current_decision_level_). The vector is of size num_variables_ so it can
  // store all the decisions. This is done this way because in some situation we
  // need to remember the previously taken decisions after a backtrack.
  int current_decision_level_;
  std::vector<Decision> decisions_;

  // The trail index after the last Backtrack() call or before the last
  // EnqueueNewDecision() call.
  int last_decision_or_backtrack_trail_index_;

  // The assumption level. See SolveWithAssumptions().
  int assumption_level_;

  // The size of the trail when ProcessNewlyFixedVariables() was last called.
  // Note that the trail contains only fixed literals (that is literals of
  // decision levels 0) before this point.
  int num_processed_fixed_variables_;
  double deterministic_time_of_last_fixed_variables_cleanup_;

  // Tracks various information about the solver progress.
  struct Counters {
    int64 num_branches;
    int64 num_random_branches;
    int64 num_failures;

    // Minimization stats.
    int64 num_minimizations;
    int64 num_literals_removed;

    // PB constraints.
    int64 num_learned_pb_literals_;

    // Clause learning /deletion stats.
    int64 num_literals_learned;
    int64 num_literals_forgotten;
    int64 num_subsumed_clauses;

    Counters()
        : num_branches(0),
          num_random_branches(0),
          num_failures(0),
          num_minimizations(0),
          num_literals_removed(0),
          num_learned_pb_literals_(0),
          num_literals_learned(0),
          num_literals_forgotten(0),
          num_subsumed_clauses(0) {}
  };
  Counters counters_;

  // Solver information.
  WallTimer timer_;

  // This is set to true if the model is found to be UNSAT when adding new
  // constraints.
  bool is_model_unsat_;

  // Parameters.
  SatParameters parameters_;

  // Variable ordering (priority will be adjusted dynamically). queue_elements_
  // holds the elements used by var_ordering_ (it uses pointers).
  //
  // Note that we recover the variable that a WeightedVarQueueElement refers to
  // by its position in the queue_elements_ vector, and we can recover the later
  // using (pointer - &queue_elements_[0]).
  struct WeightedVarQueueElement {
    WeightedVarQueueElement() : heap_index(-1), tie_breaker(0.0), weight(0.0) {}

    // Interface for the AdjustablePriorityQueue.
    void SetHeapIndex(int h) { heap_index = h; }
    int GetHeapIndex() const { return heap_index; }

    // Priority order. The AdjustablePriorityQueue returns the largest element
    // first.
    //
    // Note(user): We used to also break ties using the variable index, however
    // this has two drawbacks:
    // - On problem with many variables, this slow down quite a lot the priority
    //   queue operations (which do as little work as possible and hence benefit
    //   from having the majority of elements with a priority of 0).
    // - It seems to be a bad heuristics. One reason could be that the priority
    //   queue will automatically diversify the choice of the top variables
    //   amongst the ones with the same priority.
    //
    // Note(user): For the same reason as explained above, it is probably a good
    // idea not to have too many different values for the tie_breaker field. I
    // am not even sure we should have such a field...
    bool operator<(const WeightedVarQueueElement& other) const {
      return weight < other.weight ||
             (weight == other.weight && (tie_breaker < other.tie_breaker));
    }

    int32 heap_index;
    float tie_breaker;

    // TODO(user): Experiment with float. In the rest of the code, we use
    // double, but maybe we don't need that much precision. Using float here may
    // save memory and make the PQ operations faster.
    double weight;
  };
  COMPILE_ASSERT(sizeof(WeightedVarQueueElement) == 16,
                 ERROR_WeightedVarQueueElement_is_not_well_compacted);

  bool var_ordering_is_initialized_;
  AdjustablePriorityQueue<WeightedVarQueueElement> var_ordering_;
  ITIVector<BooleanVariable, WeightedVarQueueElement> queue_elements_;

  // This is used for the branching heuristic described in "Learning Rate Based
  // Branching Heuristic for SAT solvers", J.H.Liang, V. Ganesh, P. Poupart,
  // K.Czarnecki, SAT 2016.
  //
  // The entries are sorted by trail index, and one can get the number of
  // conflicts during which a variable at a given trail index i was assigned by
  // summing the entry.count for all entries with a trail index greater than i.
  struct NumConflictsStackEntry {
    int trail_index;
    int64 count;
  };
  std::vector<NumConflictsStackEntry> num_conflicts_stack_;

  // Whether the priority of the given variable needs to be updated in
  // var_ordering_. Note that this is only accessed for assigned variables and
  // that for efficiency it is indexed by trail indices. If
  // pq_need_update_for_var_at_trail_index_[trail_->Info(var).trail_index] is
  // true when we untrail var, then either var need to be inserted in the queue,
  // or we need to notify that its priority has changed.
  BitQueue64 pq_need_update_for_var_at_trail_index_;

  // Increment used to bump the variable activities.
  double variable_activity_increment_;
  double clause_activity_increment_;

  // Stores variable activity and the number of time each variable was "bumped".
  // The later is only used with the ERWA heuristic.
  ITIVector<BooleanVariable, double> activities_;
  ITIVector<BooleanVariable, int64> num_bumps_;

  // Used by NextBranch() to choose the polarity of the next decision. For the
  // phase saving, the last polarity is stored in trail_->Info(var).
  bool decision_heuristic_is_initialized_;
  ITIVector<BooleanVariable, bool> var_use_phase_saving_;
  ITIVector<BooleanVariable, bool> var_polarity_;
  ITIVector<BooleanVariable, double> weighted_sign_;

  // If true, leave the initial variable activities to their current value.
  bool leave_initial_activities_unchanged_;

  // This counter is decremented each time we learn a clause that can be
  // deleted. When it reaches zero, a clause cleanup is triggered.
  int num_learned_clause_before_cleanup_;

  // Conflicts credit to create until the next restart.
  int conflicts_until_next_restart_;
  int restart_count_;
  int luby_count_;

  // Conflicts credit until the next strategy change.
  int conflicts_until_next_strategy_change_;
  int strategy_change_conflicts_;
  int strategy_counter_;

  // Temporary members used during conflict analysis.
  SparseBitset<BooleanVariable> is_marked_;
  SparseBitset<BooleanVariable> is_independent_;
  SparseBitset<BooleanVariable> tmp_mark_;
  std::vector<int> min_trail_index_per_level_;

  // Temporary members used by CanBeInferedFromConflictVariables().
  std::vector<BooleanVariable> dfs_stack_;
  std::vector<BooleanVariable> variable_to_process_;

  // Temporary member used by AddLinearConstraintInternal().
  std::vector<Literal> literals_scratchpad_;

  // A boolean vector used to temporarily mark decision levels.
  DEFINE_INT_TYPE(SatDecisionLevel, int);
  SparseBitset<SatDecisionLevel> is_level_marked_;

  // Temporary vectors used by EnqueueDecisionAndBackjumpOnConflict().
  std::vector<Literal> learned_conflict_;
  std::vector<Literal> reason_used_to_infer_the_conflict_;
  std::vector<Literal> extra_reason_literals_;
  std::vector<SatClause*> subsumed_clauses_;

  // "cache" to avoid inspecting many times the same reason during conflict
  // analysis.
  VariableWithSameReasonIdentifier same_reason_identifier_;

  // A random number generator.
  mutable MTRandom random_;

  // Temporary vector used by AddProblemClause().
  std::vector<LiteralWithCoeff> tmp_pb_constraint_;

  // Boolean used to include/exclude constraints from the core computation.
  bool is_relevant_for_core_computation_;

  // The current pseudo-Boolean conflict used in PB conflict analysis.
  MutableUpperBoundedLinearConstraint pb_conflict_;

  // Running average used by some restart algorithms.
  RunningAverage dl_running_average_;
  RunningAverage lbd_running_average_;
  RunningAverage trail_size_running_average_;

  // The solver time limit.
  std::unique_ptr<TimeLimit> time_limit_;

  // The deterministic time when the time limit was updated.
  // As the deterministic time in the time limit has to be advanced manually,
  // it is necessary to keep track of the last time the time was advanced.
  double deterministic_time_at_last_advanced_time_limit_;

  // This is true iff the loaded problem only contains clauses.
  bool problem_is_pure_sat_;

  DratWriter* drat_writer_;

  mutable StatsGroup stats_;
  DISALLOW_COPY_AND_ASSIGN(SatSolver);
};

inline std::function<void(Model*)> BooleanLinearConstraint(
    int64 lower_bound, int64 upper_bound, std::vector<LiteralWithCoeff>* cst) {
  return [=](Model* model) {
    model->GetOrCreate<SatSolver>()->AddLinearConstraint(
        /*use_lower_bound=*/true, Coefficient(lower_bound),
        /*use_upper_bound=*/true, Coefficient(upper_bound), cst);
  };
}

inline std::function<void(Model*)> ExactlyOneConstraint(
    const std::vector<Literal>& literals) {
  return [=](Model* model) {
    std::vector<LiteralWithCoeff> cst;
    for (const Literal l : literals) {
      cst.push_back(LiteralWithCoeff(l, Coefficient(1)));
    }
    model->GetOrCreate<SatSolver>()->AddLinearConstraint(
        /*use_lower_bound=*/true, Coefficient(1),
        /*use_upper_bound=*/true, Coefficient(1), &cst);
  };
}

inline std::function<void(Model*)> ClauseConstraint(
    const std::vector<Literal>& literals) {
  return [=](Model* model) {
    std::vector<LiteralWithCoeff> cst;
    for (const Literal l : literals) {
      cst.push_back(LiteralWithCoeff(l, Coefficient(1)));
    }
    model->GetOrCreate<SatSolver>()->AddLinearConstraint(
        /*use_lower_bound=*/true, Coefficient(1),
        /*use_upper_bound=*/false, Coefficient(1), &cst);
  };
}

// The a => b constraint.
inline std::function<void(Model*)> Implication(Literal a, Literal b) {
  return [=](Model* model) {
    model->GetOrCreate<SatSolver>()->AddBinaryClause(a.Negated(), b);
  };
}

// This can be used to enumerate all the solutions. After each SAT call to
// Solve(), calling this will reset the solver and exclude the current solution
// so that the next call to Solve() will give a new solution or UNSAT is there
// is no more new solutions.
inline std::function<void(Model*)> ExcludeCurrentSolutionAndBacktrack() {
  return [=](Model* model) {
    SatSolver* sat_solver = model->GetOrCreate<SatSolver>();

    // Note that we only exclude the current decisions, which is an efficient
    // way to not get the same SAT assignment.
    std::vector<Literal> exlude_solution;
    for (int i = 0; i < sat_solver->CurrentDecisionLevel(); ++i) {
      exlude_solution.push_back(sat_solver->Decisions()[i].literal.Negated());
    }
    sat_solver->Backtrack(0);
    model->Add(ClauseConstraint(exlude_solution));
  };
}

inline std::function<SatParameters(Model*)> NewSatParameters(std::string params) {
  return [=](Model* model) {
    sat::SatParameters parameters;
    if (!params.empty()) {
      CHECK(google::protobuf::TextFormat::ParseFromString(params, &parameters)) << params;
      model->GetOrCreate<SatSolver>()->SetParameters(parameters);
    }
    return parameters;
  };
}

inline std::function<int64(const Model&)> ValueOf(Literal l) {
  return [=](const Model& model) {
    return model.Get<SatSolver>()->Assignment().LiteralIsTrue(l);
  };
}

// Returns a std::string representation of a SatSolver::Status.
std::string SatStatusString(SatSolver::Status status);
inline std::ostream& operator<<(std::ostream& os, SatSolver::Status status) {
  os << SatStatusString(status);
  return os;
}

// Returns the ith element of the strategy S^univ proposed by M. Luby et al. in
// Optimal Speedup of Las Vegas Algorithms, Information Processing Letters 1993.
// This is used to decide the number of conflicts allowed before the next
// restart. This method, used by most SAT solvers, is usually referenced as
// Luby.
// Returns 2^{k-1} when i == 2^k - 1
//    and  SUniv(i - 2^{k-1} + 1) when 2^{k-1} <= i < 2^k - 1.
// The sequence is defined for i > 0 and starts with:
//   {1, 1, 2, 1, 1, 2, 4, 1, 1, 2, 1, 1, 2, 4, 8, ...}
inline int SUniv(int i) {
  DCHECK_GT(i, 0);
  while (i > 2) {
    const int most_significant_bit_position =
        MostSignificantBitPosition64(i + 1);
    if ((1 << most_significant_bit_position) == i + 1) {
      return 1 << (most_significant_bit_position - 1);
    }
    i -= (1 << most_significant_bit_position) - 1;
  }
  return 1;
}

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_SAT_SOLVER_H_
