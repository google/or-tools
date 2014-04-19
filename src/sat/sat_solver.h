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
// This file implements a SAT solver.
// see http://en.wikipedia.org/wiki/Boolean_satisfiability_problem
// for more detail.
// TODO(user): Expand.

#ifndef OR_TOOLS_SAT_SAT_SOLVER_H_
#define OR_TOOLS_SAT_SAT_SOLVER_H_

#include "base/unique_ptr.h"
#include <queue>
#include <string>
#include <vector>

#include "base/integral_types.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/timer.h"
#include "base/int_type_indexed_vector.h"
#include "base/int_type.h"
#include "base/random.h"
#include "sat/pb_constraint.h"
#include "sat/clause.h"
#include "sat/sat_base.h"
#include "sat/sat_parameters.pb.h"
#include "sat/symmetry.h"
#include "sat/unsat_proof.h"
#include "util/bitset.h"
#include "util/stats.h"
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
  ~SatSolver();

  // Parameters management. Note that calling SetParameters() will reset the
  // value of many heuristics. For instance:
  // - The restart strategy will be reinitialized.
  // - The random seed will be reset to the value given in parameters.
  void SetParameters(const SatParameters& parameters);
  const SatParameters& parameters() const;

  // Increases the number of variables of the current problem.
  void SetNumVariables(int num_variables);

  // Fixes a variable so that the given literal is true. This can be used to
  // solve a subproblem where some variables are fixed. Note that it is more
  // efficient to add such unit clause before all the others.
  // Returns false if the problem is detected to be UNSAT.
  bool AddUnitClause(Literal true_literal);

  // Adds a clause to the problem. Returns false if the problem is detected to
  // be UNSAT.
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

  // Add a set of Literals permutation that are assumed to be symmetries of the
  // problem. The solver will take ownership of the pointers.
  //
  // TODO(user): This currently can't be used with unsat_proof() on. Fix it.
  void AddSymmetries(std::vector<std::unique_ptr<SparsePermutation>>* generators) {
    for (int i = 0; i < generators->size(); ++i) {
      symmetry_propagator_.AddSymmetry(std::move((*generators)[i]));
    }
  }

  // Advanced usage. This is only relevant when trying to compute an unsat core.
  // All the constraints added by one of the Add*() function above when this was
  // set to true will be considered for the core. All the others will just be
  // ignored (and thus save memory during the solve). This starts with a value
  // of true.
  void SetNextConstraintsRelevanceForUnsatCore(bool value) {
    is_relevant_for_core_computation_ = value;
  }

  // Returns the number of time one of the Add*() functions was called. This
  // will also be the unique index associated to the next constraint that will
  // be added. This unique index is used by UnsatCore() to indicates what
  // constraints are part of the core.
  int NumAddedConstraints() { return num_constraints_; }

  // Gives a hint so the solver tries to find a solution with the given literal
  // sets to true. The weight is a positive number reflecting the relative
  // importance between multiple calls to SetAssignmentPreference().
  //
  // If the weight is non-zero, branching on a variable will always be done
  // according to the preference. If the weight is zero, then the branch will
  // be chosen with the current variable_branching() parameter.
  //
  // The weight is also used as a tie-breaker between variable with the same
  // activities provided that the variable_weight parameter is set to
  // DEFAULT_WEIGHT.
  void SetAssignmentPreference(Literal literal, double weight);

  // When this is called, any currently assigned variables will be treated as
  // assumption for the next Solve() call. If the solver backtrack or if
  // Backtrack() is called on a lower decision level, then the assumption will
  // be reduced to match the new level.
  //
  // If, given these assumptions, the model is UNSAT, calling Solve() will
  // returns the ASSUMPTIONS_UNSAT status. MODEL_UNSAT is reserved for the case
  // where the model is proven to be unsat without any assumptions.
  void TreatCurrentDecisionsAsAssumption();

  // Solves the problem and returns its status.
  //
  // Note that the time or conflict limit applies only to this function and
  // starts counting from the time it is called.
  //
  // This will restart from the current solver configuration. If a previous call
  // to Solve() was interupted by a conflict or time limit, calling this again
  // will resume the search exactly as it would have continued.
  enum Status {
    ASSUMPTIONS_UNSAT,
    MODEL_UNSAT,
    MODEL_SAT,
    LIMIT_REACHED,
  };
  Status Solve();

  // Returns an UNSAT core. That is a subset of the problem clauses that are
  // still UNSAT. A problem constraint of index #i is the one that was added
  // with the i-th call to one of the Add*() functions, see
  // NumAddedConstraints().
  //
  // Preconditions:
  // - Solve() must be called with the parameters unsat_proof() set to true.
  // - It must have returned MODEL_UNSAT.
  void ComputeUnsatCore(std::vector<int>* core);

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
  // - The returned value is equal to trail_.Index() after this backtracking and
  //   just before the new propagation (due to the conflict) which is also
  //   performed by this function.
  int EnqueueDecisionAndBackjumpOnConflict(Literal true_literal);

  // This function starts by calling EnqueueDecisionAndBackjumpOnConflict(). If
  // there is no conflict, it stops there. Otherwise, it tries to reapply all
  // the decisions that where backjumped over until the first one that can't be
  // taken because it is incompatible. Note that during this process, more
  // conflicts may happen and the trail may be backtracked even further.
  //
  // In any case, the new decisions stack will be the largest valid prefix
  // of the old stack. Hence, the first deleted decision will corresponds to a
  // literal that was already propagated to its opposite value.
  int EnqueueDecisionAndBacktrackOnConflict(Literal true_literal);

  // Restores the state to the given target decision level. The decision at that
  // level and all its propagation will not be undone. But all the trail after
  // this will be cleared. Calling this with 0 will revert the solver to the
  // sate after InitialPropagation() was called.
  void Backtrack(int target_level);

  // Various getters of the current solver state.
  struct Decision {
    Decision() : trail_index(-1) {}
    Decision(int i, Literal l) : trail_index(i), literal(l) {}
    int trail_index;
    Literal literal;
  };
  int CurrentDecisionLevel() const { return current_decision_level_; }
  const std::vector<Decision>& Decisions() const { return decisions_; }
  const Trail& LiteralTrail() const { return trail_; }
  const VariablesAssignment& Assignment() const { return trail_.Assignment(); }

  // Some statistics since the creation of the solver.
  int64 num_branches() const;
  int64 num_failures() const;
  int64 num_propagations() const;

 private:
  // Returns false if the thread memory is over the limit.
  bool IsMemoryLimitReached() const;

  // Sets is_model_unsat_ to true and return false.
  bool ModelUnsat();

  // Utility function to insert spaces proportional to the search depth.
  // It is used in the pretty print of the search.
  std::string Indent() const;

  // Returns the decision level of a given variable.
  int DecisionLevel(VariableIndex var) const { return trail_.Info(var).level; }

  // Returns the reason for a given variable assignment. The variable must be
  // assigned (this is DCHECKed). The interpretation is that because all the
  // literal of a reason were assigned to false, we could deduce the assignement
  // of the given variable.
  //
  // WARNING: The returned ClauseRef will be invalidated by the next call to
  // Reason().
  //
  // Complexity remark: This is called a lot less often than Enqueue(). So it is
  // better to do as little work as possible during Enqueue() and more work
  // here. In particular, generating a reason clause lazily make sense.
  ClauseRef Reason(VariableIndex var);

  // This does one step of a pseudo-Boolean resolution:
  // - The variable var has been assigned to l at a given trail_index.
  // - The reason for var propagates it to l.
  // - The conflict propagates it to not(l)
  // The goal of the operation is to combine the two constraints in order to
  // have a new conflict at a lower trail_index.
  void ResolvePBConflict(VariableIndex var,
                         MutableUpperBoundedLinearConstraint* conflict);

  // Returns true if the clause is the reason for an assigned variable or was
  // the reason the last time a variable was assigned.
  //
  // Note(user): Since this is only used to delete learned clause, it sounds
  // like a good idea to keep clauses that were used as a reason even if the
  // variable is currently not assigned. This way, even if the clause cleaning
  // happen just after a restart, the logic will not change.
  bool IsClauseUsedAsReason(SatClause* clause) const {
    const VariableIndex var = clause->PropagatedLiteral().Variable();
    return trail_.Info(var).type == AssignmentInfo::CLAUSE_PROPAGATION &&
           trail_.Info(var).sat_clause == clause;
  }

  // Predicate used by ProcessNewlyFixedVariables().
  bool IsClauseAttachedOrUsedAsReason(SatClause* clause) const {
    return clause->IsAttached() || IsClauseUsedAsReason(clause);
  }

  // Predicate used by CompressLearnedClausesIfNeeded().
  bool ClauseShouldBeKept(SatClause* clause) const {
    return clause->Lbd() <= 2 || clause->Size() <= 2 ||
           IsClauseUsedAsReason(clause);
  }

  // Add a problem clause. Not that the clause is assumed to be "cleaned", that
  // is no duplicate variables (not strictly required) and not empty.
  bool AddProblemClauseInternal(const std::vector<Literal>& literals,
                                ResolutionNode* node);

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
      const std::vector<Literal>& literals, ResolutionNode* node);

  // Creates a new decision which corresponds to setting the given literal to
  // True and Enqueue() this change.
  void NewDecision(Literal literal);

  // Performs propagation of the recently enqueued elements.
  bool Propagate();

  // Asks for the next decision to branch upon. This shouldn't be called if
  // there is no active variable (i.e. unassigned variable).
  Literal NextBranch();

  // Unrolls the trail until a given point. This unassign the assigned variables
  // and add them to the priority queue with the correct weight.
  void Untrail(int trail_index);
  void UntrailWithoutPQUpdate(int trail_index);

  // Update the resolution node associated to all the newly fixed variables so
  // each node expresses the reason why this variable was assigned. This is
  // needed because level zero variables are treated differently by the solver.
  void ProcessNewlyFixedVariableResolutionNodes();

  // Simplifies the problem when new variables are assigned at level 0.
  void ProcessNewlyFixedVariables();

  // Compute an initial variable ordering.
  void ComputeInitialVariableOrdering();

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
      ClauseRef failing_clause, int max_trail_index, std::vector<Literal>* conflict,
      std::vector<Literal>* reason_used_to_infer_the_conflict);

  // Do the full pseudo-Boolean constraint analysis. This calls multiple
  // time ResolvePBConflict() on the current conflict until we have a conflict
  // that allow us to propagate more at a lower decision level. This level
  // is the one returned in backjump_level.
  void ComputePBConflict(int max_trail_index,
                         MutableUpperBoundedLinearConstraint* conflict,
                         int* backjump_level);

  // Creates the root resolution node associated with the current constraint.
  // This will returns nullptr if the solver is not configured to compute unsat
  // core or if the current constraint is not relevant for the core computation.
  ResolutionNode* CreateRootResolutionNode();

  // Return the resolution node associated with the given variable assignment.
  ResolutionNode* ResolutionNodeForAssignment(VariableIndex var) const;

  // Creates a ResolutionNode associated to a learned conflict. Basically, the
  // node will hold the information that the learned clause can be derived from
  // the conflict clause and all the reason that where used during the
  // computation of the first uip conflict.
  ResolutionNode* CreateResolutionNode(
      ResolutionNode* failing_clause_resolution_node,
      ClauseRef reason_used_to_infer_the_conflict);

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
  bool CanBeInferedFromConflictVariables(VariableIndex variable);

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
  // levels at which the literals of the clause were assigned. This can only be
  // computed if all the literals of the clause are assigned. Note that we
  // ignore the decision level 0 whereas the definition in the paper below
  // don't:
  //
  // G. Audemard, L. Simon, "Predicting Learnt Clauses Quality in Modern SAT
  // Solver" in Twenty-first International Joint Conference on Artificial
  // Intelligence (IJCAI'09), july 2009.
  // http://www.ijcai.org/papers09/Papers/IJCAI09-074.pdf
  template <typename LiteralList>
  int ComputeLbd(const LiteralList& literals);

  // Checks if we need to reduce the number of learned clauses and do
  // it if needed. The second function updates the learned clause limit.
  void CompressLearnedClausesIfNeeded();
  void InitLearnedClauseLimit();

  // Returns the initial weight of a variable. Higher is better. This depends on
  // the variable_ordering parameter.
  double ComputeInitialVariableWeight(VariableIndex var) const;

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

  // Decides if we should restart from scratch or not. It is called after each
  // conflict generation. If it returns true, it updates
  // conflicts_until_next_restart_ as a side effect.
  bool ShouldRestart();

  // Init restart period.
  void InitRestart();

  std::string DebugString(const SatClause& clause) const;
  std::string StatusString(Status status) const;
  std::string RunningStatisticsString() const;

  VariableIndex num_variables_;

  // The number of constraints of the initial problem that where added.
  int num_constraints_;

  // Original clauses of the problem and clauses learned during search.
  // These vector have the ownership of the pointers. We currently do not use
  // std::unique_ptr<SatClause> because it can't be used with STL algorithm
  // like std::partition.
  std::vector<SatClause*> problem_clauses_;
  std::vector<SatClause*> learned_clauses_;

  // Observers of literals.
  LiteralWatchers watched_clauses_;
  BinaryImplicationGraph binary_implication_graph_;
  PbConstraints pb_constraints_;
  SymmetryPropagator symmetry_propagator_;

  // The solver trail.
  Trail trail_;

  // The current assumption level, 0 means no assumptions.
  int assumption_level_;

  // The stack of decisions taken by the solver. They are stored in [0,
  // current_decision_level_). The vector is of size num_variables_ so it can
  // store all the decisions. This is done this way because in some situation we
  // need to remember the previously taken decisions after a backtrack.
  int current_decision_level_;
  std::vector<Decision> decisions_;

  // The index of the first non-propagated literal on the trail. The first index
  // is for non-binary clauses propagation and the second index is for binary
  // clauses propagation.
  int propagation_trail_index_;
  int binary_propagation_trail_index_;

  // The size of the trail when ProcessNewlyFixedVariables() was last called.
  // Note that the trail contains only fixed literals (that is literals of
  // decision levels 0) before this point.
  int num_processed_fixed_variables_;

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

    Counters()
        : num_branches(0),
          num_random_branches(0),
          num_failures(0),
          num_minimizations(0),
          num_literals_removed(0),
          num_learned_pb_literals_(0),
          num_literals_learned(0),
          num_literals_forgotten(0) {}
  };
  Counters counters_;

  // Solver information.
  WallTimer timer_;

  // This is set to true if the model is found to be UNSAT when adding new
  // constraints.
  bool is_model_unsat_;

  // Parameters.
  SatParameters parameters_;

  // Variable ordering (priority will be adjusted dynamically). The variable in
  // the queue are said to be active. queue_elements_ holds the elements used by
  // var_ordering_ (it uses pointers).
  struct WeightedVarQueueElement {
    WeightedVarQueueElement()
        : heap_index(-1), weight(0.0), tie_breaker(0.0), variable(-1) {}

    // Interface for the AdjustablePriorityQueue.
    void SetHeapIndex(int h) { heap_index = h; }
    int GetHeapIndex() const { return heap_index; }

    // Priority order. The AdjustablePriorityQueue returns the largest element
    // first.
    bool operator<(const WeightedVarQueueElement& other) const {
      return weight < other.weight ||
            (weight == other.weight &&
              (tie_breaker < other.tie_breaker ||
              (tie_breaker == other.tie_breaker && variable < other.variable)));
    }

    int heap_index;
    double weight;
    double tie_breaker;
    VariableIndex variable;
  };
  AdjustablePriorityQueue<WeightedVarQueueElement> var_ordering_;
  ITIVector<VariableIndex, WeightedVarQueueElement> queue_elements_;

  // Increment used to bump the variable activities.
  double variable_activity_increment_;
  double clause_activity_increment_;

  // Stores variable activity.
  ITIVector<VariableIndex, double> activities_;

  // The solver will heuristically try to find an assignment with a small sum
  // of weigths. Note that all weights are positive, so the minimum is zero.
  // Moreover, out of a literal and its negation, at most one has a non-zero
  // weight. If both are zero, then the solver polarity logic will not change
  // for the corresponding variable.
  ITIVector<LiteralIndex, double> objective_weights_;

  // If true, leave the initial variable activities to their current value.
  bool leave_initial_activities_unchanged_;

  // This counter is decremented each time we learn a clause. When it reaches
  // zero, a clause cleanup is triggered. Note that only the clauses added to
  // learned_clauses_ are counted, so we exclude binary clauses if
  // parameters_.treat_binary_clauses_separately() is true.
  int num_learned_clause_before_cleanup_;
  int target_number_of_learned_clauses_;

  // Conflicts credit to create until the next restart.
  int conflicts_until_next_restart_;
  int restart_count_;

  // Temporary members used during conflict analysis.
  SparseBitset<VariableIndex> is_marked_;
  SparseBitset<VariableIndex> is_independent_;
  std::vector<int> min_trail_index_per_level_;

  // Temporary members used by CanBeInferedFromConflictVariables().
  std::vector<VariableIndex> dfs_stack_;
  std::vector<VariableIndex> variable_to_process_;

  // Temporary member used by AddLinearConstraintInternal().
  std::vector<Literal> literals_scratchpad_;

  // A boolean vector used to temporarily mark decision levels.
  DEFINE_INT_TYPE(SatDecisionLevel, int);
  SparseBitset<SatDecisionLevel> is_level_marked_;

  // Temporary vectors used by EnqueueDecisionAndBackjumpOnConflict().
  std::vector<Literal> learned_conflict_;
  std::vector<Literal> reason_used_to_infer_the_conflict_;

  // "cache" to avoid inspecting many times the same reason during conflict
  // analysis.
  VariableWithSameReasonIdentifier same_reason_identifier_;

  // Stores the resolution DAG.
  // This is only used is parameters_.unsat_proof() is true.
  UnsatProof unsat_proof_;

  // A random number generator.
  mutable MTRandom random_;

  // Temporary vector used by AddProblemClause().
  std::vector<LiteralWithCoeff> tmp_pb_constraint_;

  // List of nodes that will need to be unlocked when this class is destructed.
  // TODO(user): This is currently used for the pseudo-Boolean constraint
  // resolution nodes, and is not really clean.
  std::vector<ResolutionNode*> to_unlock_;

  // Temporary vector used by CreateResolutionNode().
  std::vector<ResolutionNode*> tmp_parents_;

  // Boolean used to include/exclude constraints from the core computation.
  bool is_relevant_for_core_computation_;

  // The current pseudo-Boolean conflict used in PB conflict analysis.
  MutableUpperBoundedLinearConstraint pb_conflict_;

  mutable StatsGroup stats_;
  DISALLOW_COPY_AND_ASSIGN(SatSolver);
};

// Returns a std::string representation of a SatSolver::Status.
std::string SatStatusString(SatSolver::Status status);

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
