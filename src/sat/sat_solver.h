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
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "base/timer.h"
#include "base/int_type_indexed_vector.h"
#include "base/int_type.h"
#include "base/random.h"
#include "sat/pb_constraint.h"
#include "sat/sat_base.h"
#include "sat/sat_parameters.pb.h"
#include "util/bitset.h"
#include "util/stats.h"
#include "base/adjustable_priority_queue.h"

using std::string;

namespace operations_research {
namespace sat {

// Forward declarations.
// TODO(user): This cyclic dependency can be relatively easily removed.
class LiteralWatchers;

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

// Variable information. This is updated each time we attach/detach a clause.
struct VariableInfo {
  VariableInfo()
  : num_positive_clauses(0),
    num_negative_clauses(0),
    num_appearances(0),
    weighted_num_appearances(0.0) {}

  int num_positive_clauses;
  int num_negative_clauses;
  int num_appearances;
  double weighted_num_appearances;
};

// Priority queue element to support var ordering by lowest weight first.
class WeightedVarQueueElement {
 public:
  WeightedVarQueueElement()
      : heap_index_(-1), weight_(0.0), var_(-1) {}
  bool operator <(const WeightedVarQueueElement& other) const {
    return weight_ < other.weight_ ||
        (weight_ == other.weight_ && var_ < other.var_);
  }
  void SetHeapIndex(int h) { heap_index_ = h; }
  int GetHeapIndex() const { return heap_index_; }
  void set_weight(double weight) { weight_ = weight; }
  double weight() const { return weight_; }
  void set_variable(VariableIndex var) { var_ = var; }
  VariableIndex variable() const { return var_; }

 private:
  int heap_index_;
  double weight_;
  VariableIndex var_;
};

// This is how the SatSolver store a clause. A clause is just a disjunction of
// literals. In many places, we just use std::vector<literal> to encode one. However,
// the solver needs to keep a few extra fields attached to each clause.
class SatClause {
 public:
  // Creates a sat clause. There must be at least 2 literals.
  // Smaller clause are treated separatly and never constructed.
  enum ClauseType {
    PROBLEM_CLAUSE,
    LEARNED_CLAUSE,
  };
  static SatClause* Create(const std::vector<Literal>& literals, ClauseType type);

  // Number of literals in the clause.
  int Size() const { return size_; }

  // Allows for range based iteration: for (Literal literal : clause) {}.
  const Literal* const begin() const { return &(literals_[0]); }
  const Literal* const end() const { return &(literals_[size_]); }

  // Returns a ClauseRef that point to this clause.
  ClauseRef ToClauseRef() const { return ClauseRef(begin(), end()); }

  // Returns the first and second literals. These are always the watched
  // literals if the clause is attached in the LiteralWatchers.
  Literal GetFirstLiteral() const { return literals_[0]; }
  Literal GetSecondLiteral() const { return literals_[1]; }

  // Tries to simplify the clause.
  enum SimplifyStatus {
    CLAUSE_ALWAYS_TRUE,
    CLAUSE_ALWAYS_FALSE,
    CLAUSE_SUBSUMED,
    CLAUSE_ACTIVE,
  };
  SimplifyStatus Simplify();

  // Propagates watched_literal which just became false in the clause. Returns
  // false if an inconsistency was detected.
  //
  // IMPORTANT: If a new literal needs watching instead, then GetFirstLiteral()
  // will be the new watched literal, otherwise it will be equal to the given
  // watched_literal.
  bool PropagateOnFalse(Literal watched_literal, Trail* trail);

  // True if the clause is learned.
  bool IsLearned() const { return is_learned_; }

  // Returns true if the clause is satisfied for the given assignment.
  bool IsSatisfied(const VariablesAssignment& assignment) const;

  // Sorts the literals of the clause depending on the given parameters and
  // statistics. Do not call this on an attached clause.
  void SortLiterals(const ITIVector<VariableIndex, VariableInfo>& statistics,
                    const SatParameters& parameters);

  // Sets up the 2-watchers data structure. It selects two non-false literals
  // and attaches the clause to the event: one of the watched literals become
  // false. It returns false if the clause only contains literals assigned to
  // false. If only one literals is not false, it propagates it to true if it
  // is not already assigned.
  bool AttachAndEnqueuePotentialUnitPropagation(Trail* trail,
                                                LiteralWatchers* demons);

  // Modify and get the clause activity.
  void IncreaseActivity(double increase) { activity_ += increase; }
  void MultiplyActivity(double factor) { activity_ *= factor; }
  double Activity() const { return activity_; }

  // Set and get the clause LBD (Literal Blocks Distance). The LBD is not
  // computed here. See ComputeClauseLbd() in SatSolver.
  void SetLbd(int value) { lbd_ = value; }
  int Lbd() const { return lbd_; }

  // Returns true if the clause is attached to a LiteralWatchers.
  bool IsAttached() const { return is_attached_; }

  // Marks the clause so that the next call to CleanUpWatchers() can identify it
  // and actually detach it.
  void LazyDetach() { is_attached_ = false; }

  string DebugString() const;

 private:
  // The data is packed so that only 16 bytes are used for these fields.
  // Note that the max lbd is the maximum depth of the search tree (decision
  // levels), so it should fit easily in 29 bits. Note that we can also upper
  // bound it without hurting too much the clause cleaning heuristic.
  bool is_learned_: 1;
  bool is_attached_: 1;
  int lbd_  : 30;
  int size_ : 32;
  double activity_;

  // This class store the literals inline, and literals_ mark the starts of the
  // variable length portion.
  Literal literals_[0];

  DISALLOW_COPY_AND_ASSIGN(SatClause);
};

// ----- LiteralWatchers -----

// Stores the 2-watched literals data structure.  See
// http://www.cs.berkeley.edu/~necula/autded/lecture24-sat.pdf for
// detail.
class LiteralWatchers {
 public:
  LiteralWatchers();
  ~LiteralWatchers();

  // Reinit data structures at the beginning of the search.
  void Init();

  // Resizes the data structure.
  void Resize(int num_variables);

  // Attaches the given clause to the event: the given literal becomes false.
  // The blocking_literal can be any literal from the clause, it is used to
  // speed up PropagateOnTrue() by skipping the clause if it is true.
  void AttachOnFalse(Literal literal,
                     Literal blocking_literal,
                     SatClause* clause);

  // Lazily detach the given clause. The deletion will actually occur when
  // CleanUpWatchers() is called. The later needs to be called before any other
  // function in this class can be called. This is DCHECKed.
  void LazyDetach(SatClause* clause);
  void CleanUpWatchers();

  // Launches all propagation when the given literal becomes true.
  // Returns false if a contradiction was encountered.
  bool PropagateOnTrue(Literal true_literal, Trail* trail);

  // Total number of clauses inspected during calls to PropagateOnTrue().
  int64 num_inspected_clauses() const { return num_inspected_clauses_; }

 private:
  // Contains, for each literal, the list of clauses that need to be inspected
  // when the corresponding literal becomes false.
  struct Watcher {
    Watcher(SatClause* c, Literal b) : clause(c), blocking_literal(b) {}
    SatClause* clause;
    Literal blocking_literal;
  };
  ITIVector<LiteralIndex, std::vector<Watcher> > watchers_on_false_;

  // Indicates if the corresponding watchers_on_false_ list need to be
  // cleaned. The boolean is_clean_ is just used in DCHECKs.
  ITIVector<LiteralIndex, bool> needs_cleaning_;
  bool is_clean_;

  int64 num_inspected_clauses_;
  mutable StatsGroup stats_;
  DISALLOW_COPY_AND_ASSIGN(LiteralWatchers);
};

// Special class to store and propagate clauses of size 2 (i.e. implication).
// Such clauses are never deleted.
//
// TODO(user): All the variables in a strongly connected component are
// equivalent and can be thus merged as one. This is relatively cheap to compute
// from time to time (linear complexity). We will also get contradiction (a <=>
// not a) this way.
//
// TODO(user): An implication (a => not a) implies that a is false. I am not
// sure it is worth detecting that because if the solver assign a to true, it
// will learn that right away. I don't think we can do it faster.
//
// TODO(user): The implication graph can be pruned. This is called the
// transitive reduction of a graph. For instance If a => {b,c} and b => {c},
// then there is no need to store a => {c}. The transitive reduction is unique
// on an acyclic graph. Computing it will allow for a faster propagation and
// memory reduction. It is however not cheap. Maybe simple lazy heuristics to
// remove redundant arcs are better. Note that all the learned clauses we add
// will never be redundant (but they could introduce cycles).
//
// TODO(user): Add a preprocessor to remove duplicates in the implication lists.
// Note that all the learned clauses we had will never create duplicates.
//
// References for most of the above TODO and more:
// - Brafman RI, "A simplifier for propositional formulas with many binary
//   clauses", IEEE Trans Syst Man Cybern B Cybern. 2004 Feb;34(1):52-9.
//   http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.28.4911
// - Marijn J. H. Heule, Matti Järvisalo, Armin Biere, "Efficient CNF
//   Simplification Based on Binary Implication Graphs", Theory and Applications
//   of Satisfiability Testing - SAT 2011, Lecture Notes in Computer Science
//   Volume 6695, 2011, pp 201-215
//   http://www.cs.helsinki.fi/u/mjarvisa/papers/heule-jarvisalo-biere.sat11.pdf
class BinaryImplicationGraph {
 public:
  BinaryImplicationGraph()
      : num_propagations_(0), num_minimization_(0), num_literals_removed_(0),
        stats_("BinaryImplicationGraph") {}
  ~BinaryImplicationGraph() {
    IF_STATS_ENABLED(LOG(INFO) << stats_.StatString());
  }

  // Resizes the data structure.
  void Resize(int num_variables);

  // Adds the binary clause (a OR b), which is the same as (not a => b).
  // Note that it is also equivalent to (not b => a).
  void AddBinaryClause(Literal a, Literal b);

  // Same as AddBinaryClause() but enqueues a possible unit propagation.
  void AddBinaryConflict(Literal a, Literal b, Trail* trail);

  // Propagates all the direct implications of the given literal becoming true.
  // Returns false if a conflict was encountered, in which case
  // trail->SetFailingClause() will be called with the correct size 2 clause.
  // This calls trail->Enqueue() on the newly assigned literals.
  bool PropagateOnTrue(Literal true_literal, Trail* trail);

  // Uses the binary implication graph to minimize the given clause by removing
  // literals that implies others.
  //
  // TODO(user): The current algorithm is minimalist, and just look at direct
  // implication. Investigate recursive version.
  void MinimizeClause(const Trail& trail, std::vector<Literal>* clause);

  // This must only be called at decision level 0 after all the possible
  // propagations. It:
  // - Removes the variable at true from the implications lists.
  // - Frees the propagation list of the assigned literals.
  void RemoveFixedVariables(const VariablesAssignment& assigment);

  // Number of literal propagated by this class (including conflicts).
  int64 num_propagations() const { return num_propagations_; }

  // MinimizeClause() stats.
  int64 num_minimization() const { return num_minimization_; }
  int64 num_literals_removed() const { return num_literals_removed_; }

  // Returns the number of current implications.
  int64 NumberOfImplications() const {
    int num = 0;
    for (const std::vector<Literal>& v : implications_) num += v.size();
    return num / 2;
  }

 private:
  // This is indexed by the Index() of a literal. Each list stores the
  // literals that are implied if the index literal becomes true.
  ITIVector<LiteralIndex, std::vector<Literal> > implications_;

  // Holds the last conflicting binary clause.
  Literal temporary_clause_[2];

  // Some stats.
  int64 num_propagations_;
  int64 num_minimization_;
  int64 num_literals_removed_;

  // Bitset used by MinimizeClause().
  SparseBitset<LiteralIndex> is_marked_;
  SparseBitset<LiteralIndex> is_removed_;

  mutable StatsGroup stats_;
  DISALLOW_COPY_AND_ASSIGN(BinaryImplicationGraph);
};

// The main SAT solver.
// It currently implements the CDCL algorithm. See
//    http://en.wikipedia.org/wiki/Conflict_Driven_Clause_Learning
class SatSolver {
 public:
  SatSolver();
  ~SatSolver();

  // Parameters management.
  void SetParameters(const SatParameters& parameters);
  const SatParameters& parameters() const;

  // Initializes the solver to solve a new problem with the given number of
  // variables.
  //
  // TODO(user): This currently only works only on a newly created class...
  // Fix this.
  void Reset(int num_variables);

  // Adds a clause to the problem. Returns false if the clause is always false
  // and thus make the problem unsatisfiable.
  bool AddProblemClause(const std::vector<Literal>& literals);

  // Adds a pseudo-Boolean constraint to the problem. Returns false if the
  // constraint is always false and thus make the problem unsatisfiable. If the
  // constraint is always true, this detects it and does nothing.
  //
  // Note(user): There is an optimization if the same constraint is added
  // consecutively (even if the bounds are different). This is particularly
  // useful for an optimization problem when we want to constrain the objective
  // of the problem more and more. Just re-adding such constraint is relatively
  // efficient.
  //
  // TODO(user): Add error handling for overflow/underflow.
  bool AddLinearConstraint(
      bool use_lower_bound, Coefficient lower_bound,
      bool use_upper_bound, Coefficient upper_bound,
      std::vector<LiteralWithCoeff>* cst);

  // Gives a hint so the solver tries to find a solution with the given literal
  // sets to true. The weight is a number in [0,1] reflecting the relative
  // importance between multiple calls to SetAssignmentPreference().
  //
  // Note that this hint is just followed at the beginning, and as such it
  // shouldn't impact the solver performance other than make it start looking
  // for a solution in a branch that seems better for the problem at hand.
  void SetAssignmentPreference(Literal literal, double weight) {
    if (!parameters_.use_optimization_hints()) return;
    DCHECK_GE(weight, 0.0);
    DCHECK_LE(weight, 1.0);
    leave_initial_activities_unchanged_ = true;
    activities_[literal.Variable()] = weight;
    objective_weights_[literal.Index()] = 0.0;
    objective_weights_[literal.NegatedIndex()] = weight;
  }

  // Solves the problem and returns its status.
  enum Status {
    MODEL_UNSAT,
    MODEL_SAT,
    INTERNAL_ERROR,
  };
  Status Solve();

  // Returns true if a given assignment is a solution of the current problem.
  // TODO(user): This currently only check normal clauses. Fix it to include
  // binary clauses and linear constraints.
  bool IsAssignmentValid(const VariablesAssignment& assignment) const;

  // Advanced usage. The next 3 functions allow to drive the search from outside
  // the solver.

  // Starts the initial propagation and returns false if the model is already
  // detected to be UNSAT.
  bool InitialPropagation();

  // Takes a new decision (the given true_literal must be unassigned) and
  // propagates it. If it leads to conflict, the conflict is learned, and the
  // trail is automatically backtracked to a valid state. Returns false if the
  // problem is UNSAT.
  bool EnqueueDecision(Literal true_literal);

  // Restores the state to the given target decision level. The decision at that
  // level and all its propagation will not be undone. But all the trail after
  // this will be cleared. Calling this with 0 will revert the solver to the
  // sate after InitialPropagation() was called.
  void Backtrack(int target_level);

  // Returns the current choices, trail and variable assignement.
  struct ChoicePoint {
    ChoicePoint(int i, Literal d) : literal_trail_index(i), decision(d) {}
    int literal_trail_index;
    Literal decision;
  };
  const std::vector<ChoicePoint>& ChoicePoints() { return choice_points_; }
  const Trail& LiteralTrail() const { return trail_; }
  const VariablesAssignment& Assignment() const { return trail_.Assignment(); }

  // Useful information about the last Solve(). They are cleared at
  // the beginning of each Solve().
  int64 num_branches() const;
  int64 num_failures() const;
  int64 num_propagations() const;

 private:
  // Sets is_model_unsat_ to true and return false.
  bool ModelUnsat();

  // Utility function to insert spaces proportional to the search depth.
  // It is used in the pretty print of the search.
  string Indent() const;

  // Returns the decision level of a given variable.
  int DecisionLevel(VariableIndex var) const {
    return trail_.Info(var).level;
  }

  // Returns the reason for a given variable assignment. The variable must be
  // assigned (this is DCHECKed). Note that the reason clause may or may not
  // contain a literal refering to the given variable.
  //
  // Complexity remark: This is called a lot less often than Enqueue(). So it is
  // better to do as little work as possible during Enqueue() and more work
  // here. In particular, generating a reason clause lazily make sense.
  ClauseRef Reason(VariableIndex var) const;

  // Returns true if the clause is the reason for an assigned variable or was
  // the reason the last time a variable was assigned.
  //
  // Note(user): Since this is only used to delete learned clause, it sounds
  // like a good idea to keep clauses that were used as a reason even if the
  // variable is currently not assigned. This way, even if the clause cleaning
  // happen just after a restart, the logic will not change.
  //
  // This works because the literal propagated by a clause will always be in
  // the second position. See SatClause::PropagateOnFalse() and
  // SatClause::AttachAndEnqueuePotentialUnitPropagation().
  bool IsClauseUsedAsReason(SatClause* clause) const {
    const VariableIndex var = clause->GetSecondLiteral().Variable();
    return trail_.Info(var).type == AssignmentInfo::CLAUSE_PROPAGATION
        && trail_.Info(var).sat_clause == clause;
  }

  // Predicate used by ProcessNewlyFixedVariables().
  bool IsClauseAttachedOrUsedAsReason(SatClause* clause) const {
    return clause->IsAttached() || IsClauseUsedAsReason(clause);
  }

  // Predicate used by CompressLearnedClausesIfNeeded().
  bool ClauseShouldBeKept(SatClause* clause) const {
    return clause->Lbd() <= 2
        || clause->Size() <= 2
        || IsClauseUsedAsReason(clause);
  }

  // Returns false if the literal is already assigned to false.
  // Otherwise, returns true and Enqueue it if it is unassigned.
  bool TestValidityAndEnqueueIfNeeded(Literal literal);

  // This is used by all the Add*LinearConstraint() functions. It detects
  // infeasible/trivial constraints or clause constraints and takes the proper
  // action.
  bool AddLinearConstraintInternal(const std::vector<LiteralWithCoeff>& cst,
                                   Coefficient rhs,
                                   Coefficient max_value);

  // Adds a learned clause to the problem. This should be called after
  // Backtrack(). The backtrack is such that after it is applied, all the
  // literals of the learned close except one will be false. Thus the last one
  // will be implied True. This function also Enqueue() the implied literal.
  void AddLearnedClauseAndEnqueueUnitPropagation(
      const std::vector<Literal>& literals);

  // Creates a new choice point which corresponds to setting the given literal
  // to True and Enqueue() this change.
  void NewChoicePoint(Literal literal);

  // Performs propagation of the recently enqueued elements.
  bool Propagate();

  // Asks for the next decision to branch upon. This shouldn't be called if
  // there is no active variable (i.e. unassigned variable).
  Literal NextBranch();

  // Unrolls the trail until a given point. This unassign the assigned variables
  // and add them to the priority queue with the correct weight.
  void Untrail(int trail_index);

  // Preprocess the model in order to simplify it.
  bool Preprocess();

  // Simplifies the problem when new variables are assigned at level 0.
  void ProcessNewlyFixedVariables();

  // Statistics.
  void ComputeStatistics();

  // Statistics on one clause, added indicates if we are adding the
  // clause, or deleting it.
  template<typename Literals>
  void UpdateStatisticsOnOneClause(
      const Literals& literals, int size, bool added) {
    SCOPED_TIME_STAT(&stats_);
    for (const Literal literal : literals) {
      const VariableIndex var = literal.Variable();
      const int direction = added ? 1 : -1;
      statistics_[var].num_appearances += direction;
      statistics_[var].weighted_num_appearances += 1.0 / size * direction;
      if (literal.IsPositive()) {
        statistics_[var].num_positive_clauses += direction;
      } else {
        statistics_[var].num_negative_clauses += direction;
      }
    }
  }

  // Compute an initial variable ordering.
  void ComputeInitialVariableOrdering();

  // Computes what is known as the first UIP (Unique implication point) conflict
  // clause starting from the failing clause. For a definition of UIP and a
  // comparison of the different possible conflict clause computation, see the
  // reference below.
  //
  // L Zhang, CF Madigan, MH Moskewicz, S Malik, "Efficient conflict driven
  // learning in a boolean satisfiability solver" Proceedings of the 2001
  // IEEE/ACM international conference on Computer-aided design, Pages 279-285.
  // http://www.cs.tau.ac.il/~msagiv/courses/ATP/iccad2001_final.pdf
  void ComputeFirstUIPConflict(ClauseRef failing_clause,
                               std::vector<Literal>* conflict,
                               std::vector<Literal>* discarded_last_level_literals);

  // Applies some heuristics to a conflict in order to minimize its size and/or
  // replace literals by other literals from lower decision levels. The first
  // function choose which one of the other functions to call depending on the
  // parameters.
  void MinimizeConflict(std::vector<Literal>* conflict);
  void MinimizeConflictExperimental(std::vector<Literal>* conflict);
  void MinimizeConflictSimple(std::vector<Literal>* conflict);
  void MinimizeConflictRecursively(std::vector<Literal>* conflict);

  // Utility function used by MinimizeConflictRecursively().
  bool CanBeInferedFromConflictVariables(VariableIndex variable);

  // To be used in DCHECK(). Verifies some property of the conflict clause:
  // - There is an unique literal of the current decision level.
  // - All the other literals are of smaller decision level.
  // - The is no literals with a decision level of zero.
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
  template<typename LiteralList>
  int ComputeLbd(const LiteralList& literals);

  // Checks if we need to reduce the number of learned clauses and do
  // it if needed. The second function updates the learned clause limit.
  void CompressLearnedClausesIfNeeded();
  void InitLearnedClauseLimit();

  // Returns the initial weight of a variable. Higher is better. This depends on
  // the variable_ordering parameter.
  double ComputeInitialVariableWeight(VariableIndex var) const;

  // Uses num_inactive_variables_ and the current trail to update the priority
  // queue of active variables. This should be called before each NextBranch().
  void MakeAllTrailVariablesInactive();

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

  void SetNumVariables(int num_variables);

  string DebugString(const SatClause& clause) const;
  string StatusString() const;
  string RunningStatisticsString() const;

  VariableIndex num_variables_;

  // Original clauses of the problem and clauses learned during search.
  // These vector have the ownership of the pointers. We currently do not use
  // std::unique_ptr<SatClause> because it can't be used with STL algorithm
  // like std::partition.
  std::vector<SatClause*> problem_clauses_;
  std::vector<SatClause*> learned_clauses_;

  // Statistics.
  ITIVector<VariableIndex, VariableInfo> statistics_;

  // Observers of literals.
  LiteralWatchers watched_clauses_;
  BinaryImplicationGraph binary_implication_graph_;
  PbConstraints pb_constraints_;

  // The solver trail.
  Trail trail_;

  // The stack of choice points.
  std::vector<ChoicePoint> choice_points_;

  // The index of the first non-propagated literal on the trail. The first index
  // is for non-binary clauses propagation and the second index is for binary
  // clauses propagation.
  int propagation_trail_index_;
  int binary_propagation_trail_index_;

  // All the variables on the trail should be inactive, however we lazily update
  // them, so this is the number of inactive variables. It is always lower or
  // equals to processed_trail_.size(). and the first num_inactive_variables_ of
  // the trail are not in the priority queue. This is updated by Untrail() and
  // MakeAllTrailVariablesInactive().
  int num_inactive_variables_;

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

    // Clause learning /deletion stats.
    int64 num_literals_learned;
    int64 num_literals_forgotten;

    Counters()
        : num_branches(0), num_random_branches(0), num_failures(0),
          num_minimizations(0), num_literals_removed(0),
          num_literals_learned(0), num_literals_forgotten(0) { }
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

  // Current maximum size of the learned clause store and number of time
  // the learned clauses where cleaned.
  int learned_clause_limit_;
  int clause_cleanup_count_;

  // Conflicts credit to create until the next restart.
  int conflicts_until_next_restart_;
  int restart_count_;

  // Temporary members used during conflict analysis.
  SparseBitset<VariableIndex> is_marked_;
  SparseBitset<VariableIndex> is_independent_;
  std::vector<int> min_trail_index_per_level_;

  // Temporary member used by AddLinearConstraintInternal().
  std::vector<Literal> literals_scratchpad_;

  // A boolean vector used to temporarily mark decision levels.
  DEFINE_INT_TYPE(SatDecisionLevel, int);
  SparseBitset<SatDecisionLevel> is_level_marked_;

  // "cache" to avoid inspecting many times the same reason during conflict
  // analysis.
  PbReasonCache reason_cache_;

  // A random number generator.
  MTRandom random_;

  mutable StatsGroup stats_;
  DISALLOW_COPY_AND_ASSIGN(SatSolver);
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_SAT_SOLVER_H_
