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

// This file defines the needed classes to efficiently perform Local Search in
// Bop.
// Local Search is a technique used to locally improve an existing solution by
// flipping a limited number of variables. To be successful the produced
// solution have to satisfy all constraints of the problem and improve the
// objective cost.
//
// The class BopLocalSearchOptimizer is the only public interface for Local
// Search in Bop. For unit-testing purpose this file also contains the four
// internal classes AssignmentAndConstraintFeasibilityMaintainer,
// OneFlipConstraintRepairer, SatWrapper and LocalSearchAssignmentIterator.
// They are implementation details and should not be used outside of bop_ls.

#ifndef OR_TOOLS_BOP_BOP_LS_H_
#define OR_TOOLS_BOP_BOP_LS_H_

#include <array>

#include "base/hash.h"

#include "base/hash.h"
#include "bop/bop_base.h"
#include "bop/bop_solution.h"
#include "bop/bop_types.h"
#include "sat/boolean_problem.pb.h"
#include "sat/sat_solver.h"

namespace operations_research {
namespace bop {

// Forward declaration.
class LocalSearchAssignmentIterator;

// This class defines a Local Search optimizer. The goal is to find a new
// solution with a better cost than the given solution by iterating on all
// assignments that can be reached in max_num_decisions decisions or less.
// The bop parameter max_number_of_explored_assignments_per_try_in_ls can be
// used to specify the number of new assignments to iterate on each time the
// method Optimize() is called. Limiting that parameter allows to reduce the
// time spent in the Optimize() method at once, and still explore all the
// reachable assignments (if Optimize() is called enough times).
// Note that due to propagation, the number of variables with a different value
// in the new solution can be greater than max_num_decisions.
class LocalSearchOptimizer : public BopOptimizerBase {
 public:
  explicit LocalSearchOptimizer(const std::string& name, int max_num_decisions);
  virtual ~LocalSearchOptimizer();

 protected:
  virtual bool RunOncePerSolution() const { return false; }
  virtual bool NeedAFeasibleSolution() const { return true; }
  virtual Status Synchronize(const ProblemState& problem_state);
  virtual Status Optimize(const BopParameters& parameters,
                          LearnedInfo* learned_info, TimeLimit* time_limit);

  // Maximum number of decisions the Local Search can take.
  // Note that there are no limit on the number of changed variables due to
  // propagation.
  const int max_num_decisions_;

  // Iterator on all reachable assignments.
  // Note that this iterator is only reseted when Synchronize() is called, i.e.
  // the iterator continue its iteration of the next assignments each time
  // Optimize() is called until everything is explored or a solution is found.
  std::unique_ptr<LocalSearchAssignmentIterator> assignment_iterator_;
};

//------------------------------------------------------------------------------
// Implementation details. The declarations of those utility classes are in
// the .h for testing reasons.
//------------------------------------------------------------------------------

// Maintains some information on a sparse set of integers in [0, n). More
// specifically this class:
// - Allows to dynamically add/remove element from the set.
// - Has a backtracking support.
// - Maintains the number of elements in the set.
// - Maintains a superset of the elements of the set that contains all the
//   modified elements.
template <typename IntType>
class BacktrackableIntegerSet {
 public:
  BacktrackableIntegerSet() {}

  // Prepares the class for integers in [0, n) and initializes the set to the
  // empty one. Note that this run in O(n). Once resized, it is better to call
  // BacktrackAll() instead of this to clear the set.
  void ClearAndResize(IntType n);

  // Changes the state of the given integer i to be either inside or outside the
  // set. Important: this should only be called with the opposite state of the
  // current one, otherwise size() will not be correct.
  void ChangeState(IntType i, bool should_be_inside);

  // Returns the current number of elements in the set.
  // Note that this is not its maximum size n.
  int size() const { return size_; }

  // Returns a superset of the current set of integers.
  const std::vector<IntType>& Superset() const { return stack_; }

  // BacktrackOneLevel() backtracks to the state the class was in when the
  // last AddBacktrackingLevel() was called. BacktrackAll() just restore the
  // class to its state just after the last ClearAndResize().
  void AddBacktrackingLevel();
  void BacktrackOneLevel();
  void BacktrackAll();

 private:
  int size_;

  // Contains the elements whose status has been changed at least once.
  std::vector<IntType> stack_;
  std::vector<bool> in_stack_;

  // Used for backtracking. Contains the size_ and the stack_.size() at the time
  // of each call to AddBacktrackingLevel() that is not yet backtracked over.
  std::vector<int> saved_sizes_;
  std::vector<int> saved_stack_sizes_;
};

// This class is used to incrementally maintain an assignment and the
// feasibility of the constraints of a given LinearBooleanProblem.
//
// The current assignment is initialized using a feasible reference solution,
// i.e. the reference solution satisfies all the constraints of the problem.
// The current assignment is updated using the Assign() method.
//
// Note that the current assignment is not a solution in the sense that it
// might not be feasible, ie. violates some constraints.
//
// The assignment can be accessed at any time using Assignment().
// The set of infeasible constraints can be accessed at any time using
// InfeasibleConstraints().
//
// Note that this class is reversible, i.e. it is possible to backtrack to
// previously added backtracking levels.
// levels. Consider for instance variable a, b, c, and d.
//      Method called                 Assigned after method call
//   1- Assign({a, b})                         a b
//   2- AddBacktrackingLevel()                 a b |
//   3- Assign({c})                            a b | c
//   4- Assign({d})                            a b | c d
//   5- BacktrackOneLevel()                    a b
//   6- Assign({c})                            a b c
//   7- BacktrackOneLevel()
class AssignmentAndConstraintFeasibilityMaintainer {
 public:
  AssignmentAndConstraintFeasibilityMaintainer(
      const LinearBooleanProblem& problem);

  static const ConstraintIndex kObjectiveConstraint;

  // Sets a new reference solution and reverts all internal structures to their
  // initial state, e.g. is_assigned_[i] == false.
  // Note that the reference solution has to be feasible.
  void SetReferenceSolution(const BopSolution& reference_solution);

  // Over-constraints the objective cost by delta. Note that calling this method
  // right after setting the reference solution, i.e. no calls to Assign(), will
  // break the objective constraint.
  void OverConstrainObjective(int64 delta);

  // Assigns all literals. That updates the assignment, the constraint values,
  // and the infeasible constraints.
  // Note that the assignment of those literals can be reverted thanks to
  // AddBacktrackingLevel() and BacktrackOneLevel().
  // Note that a variable can't be assigned twice, even for the same literal.
  void Assign(const std::vector<sat::Literal>& literals);

  // Adds a new backtracking level to specify the state that will be restored
  // by BacktrackOneLevel().
  // See the example in the class comment.
  void AddBacktrackingLevel();

  // Backtracks internal structures to the previous level defined by
  // AddBacktrackingLevel(). As a consequence the state will be exactly as
  // before the previous call to AddBacktrackingLevel().
  // Note that backtracking the initial state has no effect.
  void BacktrackOneLevel();

  // Returns true when the variable var has been set by Assign().
  bool IsAssigned(VariableIndex var) const { return is_assigned_[var]; }

  // Returns true if there is no infeasible constraint in the current state.
  bool IsFeasible() const { return infeasible_constraint_set_.size() == 0; }

  // Returns a superset of all the infeasible constraints in the current state.
  const std::vector<ConstraintIndex>& PossiblyInfeasibleConstraints() const {
    return infeasible_constraint_set_.Superset();
  }

  // Returns the number of constraints of the problem, objective included,
  // i.e. the number of constraint in the problem + 1.
  size_t NumConstraints() const { return constraint_lower_bounds_.size(); }

  // Returns the value of the var in the assignment.
  // As the assignment is initialized with the reference solution, if the
  // variable has not been assigned through Assign(), the returned value is
  // the value of the variable in the reference solution.
  bool Assignment(VariableIndex var) const { return assignment_.Value(var); }

  // Returns the assignment as a BopSolution. Note that the resulting solution
  // might not be feasible.
  const BopSolution& assignment() const { return assignment_; }

  // Returns the lower bound of the constraint.
  int64 ConstraintLowerBound(ConstraintIndex constraint) const {
    return constraint_lower_bounds_[constraint];
  }

  // Returns the upper bound of the constraint.
  int64 ConstraintUpperBound(ConstraintIndex constraint) const {
    return constraint_upper_bounds_[constraint];
  }

  // Returns the value of the constraint. The value is computed using the
  // variable values in the assignment.
  // Note that a constraint is feasible iff its value is between its two bounds
  // (inclusive).
  int64 ConstraintValue(ConstraintIndex constraint) const {
    return constraint_values_[constraint];
  }

  // Returns true if the given constraint is currently feasible.
  bool ConstraintIsFeasible(ConstraintIndex constraint) const {
    const int64 value = ConstraintValue(constraint);
    return value >= ConstraintLowerBound(constraint) &&
           value <= ConstraintUpperBound(constraint);
  }

  std::string DebugString() const;

 private:
  // Local structure to represent the sparse matrix by variable used for fast
  // update of the contraint values.
  struct ConstraintEntry {
    ConstraintEntry(ConstraintIndex c, int64 w) : constraint(c), weight(w) {}
    ConstraintIndex constraint;
    int64 weight;
  };

  ITIVector<VariableIndex, ITIVector<EntryIndex, ConstraintEntry> >
      by_variable_matrix_;
  ITIVector<ConstraintIndex, int64> constraint_lower_bounds_;
  ITIVector<ConstraintIndex, int64> constraint_upper_bounds_;
  BopSolution reference_solution_;

  BopSolution assignment_;
  ITIVector<VariableIndex, bool> is_assigned_;
  ITIVector<ConstraintIndex, int64> constraint_values_;

  std::vector<std::vector<sat::Literal> > literals_applied_stack_;
  BacktrackableIntegerSet<ConstraintIndex> infeasible_constraint_set_;

  DISALLOW_COPY_AND_ASSIGN(AssignmentAndConstraintFeasibilityMaintainer);
};

// This class is an utility class used to select which infeasible constraint to
// repair and identify one variable to flip to actually repair the constraint.
// A constraint 'lb <= sum_i(w_i * x_i) <= ub', with 'lb' the lower bound,
// 'ub' the upper bound, 'w_i' the weight of the i-th term and 'x_i' the
// boolean variable appearing in the i-th term, is infeasible for a given
// assignment iff its value 'sum_i(w_i * x_i)' is outside of the bounds.
// Repairing-a-constraint-in-one-flip means making the constraint feasible by
// just flipping the value of one unassigned variable of the current assignment
// from the AssignmentAndConstraintFeasibilityMaintainer.
// For performance reasons, the pairs weight / variable (w_i, x_i) are stored
// in a sparse manner as a vector of terms (w_i, x_i). In the following the
// TermIndex term_index refers to the position of the term in the vector.
class OneFlipConstraintRepairer {
 public:
  OneFlipConstraintRepairer(
      const LinearBooleanProblem& problem,
      const AssignmentAndConstraintFeasibilityMaintainer& maintainer);

  static const ConstraintIndex kInvalidConstraint;
  static const TermIndex kInitTerm;
  static const TermIndex kInvalidTerm;

  // Returns the index of a constraint to repair. Returns kInvalidConstraint if
  // no constraint can be repaired with only one flip.
  // Note that indices follows the convention in
  // AssignmentAndConstraintFeasibilityMaintainer, i.e. '0' corresponds to the
  // objective constraint and 'i' corresponds to the 'i - 1' constraint in
  // the problem.
  ConstraintIndex ConstraintToRepair() const;

  // Returns the index of the next term which repairs the constraint when the
  // value of its variable is flipped. This method explores terms with an
  // index strictly greater than start_term_index and then terms with an index
  // smaller than or equal to init_term_index if any.
  // Returns kInvalidTerm when no reparing terms are found.
  //
  // Note that if init_term_index == start_term_index, then all the terms will
  // be explored. Both TermIndex arguments can take values in [-1, constraint
  // size).
  TermIndex NextRepairingTerm(ConstraintIndex constraint,
                              TermIndex init_term_index,
                              TermIndex start_term_index) const;

  // Returns the literal corresponding to the term_index in the constraint.
  sat::Literal Literal(ConstraintIndex constraint, TermIndex term_index) const;

  // Local structure to represent the sparse matrix by constraint used for fast
  // lookups.
  struct ConstraintTerm {
    ConstraintTerm(VariableIndex v, int64 w) : var(v), weight(w) {}
    VariableIndex var;
    int64 weight;
  };

 private:
  // Sorts the terms of each constraints to iterate on most promising variables
  // first.
  void SortTerms(int num_variables);

  ITIVector<ConstraintIndex, ITIVector<TermIndex, ConstraintTerm> >
      by_constraint_matrix_;
  const AssignmentAndConstraintFeasibilityMaintainer& maintainer_;

  DISALLOW_COPY_AND_ASSIGN(OneFlipConstraintRepairer);
};

// This class is used to ease the connection with the SAT solver.
// TODO(user): Consider a tightener integration where more objects of the SAT
//              solver are shared, e.g. the trail stack, the assignment...
class SatWrapper {
 public:
  explicit SatWrapper(const ProblemState& problem_state);

  // Backtracks the SAT solver to its root level and resets internal structure,
  // and synchronizes with the problem state (e.g. new fixed variables).
  void Synchronize(const ProblemState& problem_state);

  // Constrains the underlying SAT solver to find a solution with a cost better
  // than the given reference_cost by at least delta.
  // Returns the newly propagated literals.
  // In other words, in case of minimization the objective is constrained to be
  // smaller than or equal to 'reference_cost - delta', and in case of
  // maximisation the objective is constrained to be greater than
  // reference_cost + delta.
  std::vector<sat::Literal> OverConstrainObjective(int64 reference_cost,
                                              int64 delta);

  // Returns true if the problem is UNSAT.
  // Note that an UNSAT problem might not be marked as UNSAT at first because
  // the SAT solver is not able to prove it; After some decisions / learned
  // conflicts, the SAT solver might be able to prove UNSAT and so this will
  // return true.
  bool unsat() const { return unsat_; }

  // Applies the decision that makes the given literal true and returns the
  // number of decisions to backtrack due to conflicts if any.
  // Two cases:
  //   - No conflicts: Returns 0 and fills the propagated_literals with the
  //     literals that have been propagated due to the decision including the
  //     the decision itself.
  //   - Conflicts: Returns the number of decisions to backtrack (the current
  //     decision included, i.e. returned value > 0) and fills the
  //     propagated_literals with the literals that the conflicts propagated.
  // Note that the decision variable should not be already assigned in SAT.
  int ApplyDecision(sat::Literal decision_literal,
                    std::vector<sat::Literal>* propagated_literals);

  // Backtracks the last decision if any.
  void BacktrackOneLevel();

  // Extracts any new information learned during the search.
  void ExtractLearnedInfo(LearnedInfo* info);

  // Returns a deterministic number that should be correlated with the time
  // spent in the SAT wrapper. The order of magnitude should be close to the
  // time in seconds.
  double deterministic_time() const;

 private:
  const LinearBooleanProblem& problem_;
  sat::SatSolver sat_solver_;
  bool unsat_;

  DISALLOW_COPY_AND_ASSIGN(SatWrapper);
};

// This class is used to iterate on all assignments that can be obtained by
// deliberately flipping 'n' variables from the reference solution, 'n' being
// smaller than or equal to max_num_decisions.
// Note that one deliberate variable flip may lead to many other flips due to
// constraint propagation, those additional flips are not counted in 'n'.
class LocalSearchAssignmentIterator {
 public:
  LocalSearchAssignmentIterator(const ProblemState& problem_state,
                                int max_num_decisions);

  // Sets whether or not a transposition table is used.
  void UseTranspositionTable(bool v) { use_transposition_table_ = v; }

  // Synchronizes the iterator with the problem state, e.g. set fixed variables,
  // set the reference solution.
  // Note that the iteration will continue where it stopped.
  void Synchronize(const ProblemState& problem_state);

  // Constrains to look for a feasible solution than improves the objective
  // cost by at least delta.
  void OverConstrainObjective(int64 reference_cost, int64 delta);

  // Move to the next assignment. Returns false when the search is finished.
  bool NextAssignment();

  // Returns true if the current assignment corresponds to a feasible solution
  // with a better objective cost than the reference solution.
  bool AssignmentIsFeasible() const {
    // TODO(user): change the second term in a DCHECK(), it shouldn't be needed.
    return maintainer_.IsFeasible() && maintainer_.assignment().IsFeasible();
  }

  // Returns the current assignment.
  // Note that this assignment is a feasible solution only when the method
  // IsFeasibleAssignment() returns true.
  const BopSolution& Assignment() const { return maintainer_.assignment(); }

  // Extracts any new informations learned during the search.
  void ExtractLearnedInfo(LearnedInfo* info);

  // Returns a deterministic number that should be correlated with the time
  // spent in the iterator. The order of magnitude should be close to the time
  // in seconds.
  double deterministic_time() const;

  std::string DebugString() const;

 private:
  // See transposition_table_ below.
  static const size_t kStoredMaxDecisions = 4;

  // Internal structure used to represent a node of the search tree during local
  // search.
  struct SearchNode {
    SearchNode()
        : constraint(OneFlipConstraintRepairer::kInvalidConstraint),
          term_index(OneFlipConstraintRepairer::kInvalidTerm) {}
    SearchNode(ConstraintIndex c, TermIndex t) : constraint(c), term_index(t) {}
    ConstraintIndex constraint;
    TermIndex term_index;
  };

  // Applies the decision. Automatically backtracks when SAT detects conflicts.
  void ApplyDecision(sat::Literal literal);

  // Adds one more decision to repair infeasible constraints.
  // Returns true in case of success.
  bool GoDeeper();

  // Backtracks and moves to the next decision in the search tree.
  void Backtrack();

  // Looks if the current decisions (in search_nodes_) plus the new one (given
  // by l) lead to a position already present in transposition_table_.
  bool NewStateIsInTranspositionTable(sat::Literal l);

  // Inserts the current set of decisions in transposition_table_.
  void InsertInTranspositionTable();

  // Initializes the given array with the current decisions in search_nodes_ and
  // by filling the other positions with 0.
  void InitializeTranspostionTableKey(
      std::array<int32, kStoredMaxDecisions>* a);

  // Looks for the next repairing term in the given constraints while skipping
  // the position already present in transposition_table_. A given TermIndex of
  // -1 means that this is the first time we explore this constraint.
  bool EnqueueNextRepairingTermIfAny(ConstraintIndex ct_to_repair,
                                     TermIndex index);

  const int max_num_decisions_;
  AssignmentAndConstraintFeasibilityMaintainer maintainer_;
  SatWrapper sat_wrapper_;
  OneFlipConstraintRepairer repairer_;
  std::vector<SearchNode> search_nodes_;
  ITIVector<ConstraintIndex, TermIndex> initial_term_index_;

  // For each set of explored decisions, we store it in this table so that we
  // don't explore decisions (a, b) and later (b, a) for instance. The decisions
  // are converted to int32, sorted and padded with 0 before beeing inserted
  // here.
  //
  // TODO(user): We may still miss some equivalent states because it is possible
  // that completely differents decisions lead to exactly the same state.
  // However this is more time consuming to detect because we must apply the
  // last decision first before trying to compare the states.
  //
  // TODO(user): Currently, we only store kStoredMaxDecisions or less decisions.
  // Ideally, this should be related to the maximum number of decision in the
  // LS, but that requires templating the whole LS optimizer.
  bool use_transposition_table_;
#if defined(_MSC_VER)
  hash_set<std::array<int32, kStoredMaxDecisions>,
           StdArrayHasher<int32, kStoredMaxDecisions>> transposition_table_;
#else
  hash_set<std::array<int32, kStoredMaxDecisions>> transposition_table_;
#endif

  // The number of explored nodes.
  int64 num_nodes_;

  // The number of skipped nodes thanks to the transposition table.
  int64 num_skipped_nodes_;

  DISALLOW_COPY_AND_ASSIGN(LocalSearchAssignmentIterator);
};

}  // namespace bop
}  // namespace operations_research
#endif  // OR_TOOLS_BOP_BOP_LS_H_
