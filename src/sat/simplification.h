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

// Implementation of a pure SAT presolver. This roughtly follows the paper:
//
// "Effective Preprocessing in SAT through Variable and Clause Elimination",
// Niklas Een and Armin Biere, published in the SAT 2005 proceedings.

#ifndef OR_TOOLS_SAT_SIMPLIFICATION_H_
#define OR_TOOLS_SAT_SIMPLIFICATION_H_

#include <deque>
#include <vector>

#include "sat/sat_base.h"
#include "sat/sat_parameters.pb.h"

#include "base/adjustable_priority_queue.h"

namespace operations_research {
namespace sat {

// A really simple postsolver that process the clause added in reverse order.
// If a clause has all its literals at false, it simply sets the literal
// that was registered with the clause to true.
class SatPostsolver {
 public:
  SatPostsolver() {}
  void Add(Literal x, std::vector<Literal>* clause) {
    CHECK(!clause->empty());
    clauses_.push_back(std::vector<Literal>());
    clauses_.back().swap(*clause);
    associated_literal_.push_back(x);
  }
  void Postsolve(VariablesAssignment* assignment) const;

 private:
  std::vector<std::vector<Literal>> clauses_;
  std::vector<Literal> associated_literal_;
  DISALLOW_COPY_AND_ASSIGN(SatPostsolver);
};

// This class hold a SAT problem (i.e. a set of clauses) and the logic to
// presolve it by a series of subsumption, self-subsuming resolution and
// variable elimination by clause distribution.
//
// Note(user): Note that this does propagate unit-clauses, but probably a lot
// less efficiently than the propagation code in the SAT solver. So it is better
// to use a SAT solver to fix variables before using this class.
//
// TODO(user): Interact more with a SAT solver to reuse its propagation logic.
//
// TODO(user): Forbid the removal of some variables. This way we can presolve
// only the clause part of a general Boolean problem by not removing variables
// appearing in pseudo-Boolean constraints.
class SatPresolver {
 public:
  // TODO(user): use IntType? not sure because that complexify the code, and
  // it is not really needed here.
  typedef int32 ClauseIndex;

  SatPresolver() {}
  void SetParameters(const SatParameters& params) { parameters_ = params; }

  // Adds new clause to the SatPresolver.
  void AddBinaryClause(Literal a, Literal b);
  void AddClause(ClauseRef clause);

  // Presolves the problem currently loaded. Returns false if the model is
  // proven to be UNSAT during the presolving.
  //
  // TODO(user): Add support for a time limit and some kind of iterations limit
  // so that this can never take too much time.
  bool Presolve();

  // Postsolve a SAT assignment of the presolved problem.
  void Postsolve(VariablesAssignment* assignemnt) const {
    postsolver_.Postsolve(assignemnt);
  }

  // All the clauses managed by this class.
  // Note that deleted clauses keep their indices (they are just empty).
  int NumClauses() const { return clauses_.size(); }
  const std::vector<Literal>& Clause(ClauseIndex ci) const { return clauses_[ci]; }

  // Allow to iterate over the clauses managed by this class.
  std::vector<std::vector<Literal>>::const_iterator begin() const {
    return clauses_.begin();
  }
  std::vector<std::vector<Literal>>::const_iterator end() const { return clauses_.end(); }

  // The number of variables. This is computed automatically from the clauses
  // added to the SatPresolver.
  int NumVariables() const { return literal_to_clauses_.size() / 2; }

  // After presolving, Some variables in [0, NumVariables()) have no longer any
  // clause pointing to them. This return a mapping that maps this interval to
  // [0, *new_size) such that now all variables are used. The unused variable
  // will be mapped to VariableIndex(-1).
  ITIVector<VariableIndex, VariableIndex> GetMapping(int* new_size) const;

  // Visible for Testing. Takes a given clause index and looks for clause that
  // can be subsumed or strengthened using this clause. Returns false if the
  // model is proven to be unsat.
  bool ProcessClauseToSimplifyOthers(ClauseIndex clause_index);

  // Visible for testing. Tries to eliminate x by clause distribution.
  // This is also known as bounded variable elimination.
  //
  // It is always possible to remove x by resolving each clause containing x
  // with all the clauses containing not(x). Hence the cross-product name. Note
  // that this function only do that if the number of clauses is reduced.
  bool CrossProduct(Literal x);

 private:
  // Clause removal function.
  void Remove(ClauseIndex ci);
  void RemoveAndRegisterForPostsolve(ClauseIndex ci, Literal x);
  void RemoveAndRegisterForPostsolveAllClauseContaining(Literal x);

  // Call ProcessClauseToSimplifyOthers() on all the clauses in
  // clause_to_process_ and empty the list afterwards. Note that while some
  // clauses are processed, new ones may be added to the list. Returns false if
  // the problem is shown to be UNSAT.
  bool ProcessAllClauses();

  // Finds the literal from the clause that occur the less in the clause
  // database.
  Literal FindLiteralWithShortestOccurenceList(const std::vector<Literal>& clause);

  // Display some statistics on the current clause database.
  void DisplayStats(double elapsed_seconds);

  // The "active" variables on which we want to call CrossProduct() are kept
  // in a priority queue so that we process first the ones that occur the least
  // often in the clause database.
  void InitializePriorityQueue();
  void UpdatePriorityQueue(VariableIndex var);
  struct PQElement {
    PQElement() : heap_index(-1), variable(-1), weight(0.0) {}

    // Interface for the AdjustablePriorityQueue.
    void SetHeapIndex(int h) { heap_index = h; }
    int GetHeapIndex() const { return heap_index; }

    // Priority order. The AdjustablePriorityQueue returns the largest element
    // first, but our weight goes this other way around (smaller is better).
    bool operator<(const PQElement& other) const {
      return weight > other.weight;
    }

    int heap_index;
    VariableIndex variable;
    double weight;
  };
  ITIVector<VariableIndex, PQElement> var_pq_elements_;
  AdjustablePriorityQueue<PQElement> var_pq_;

  // List of clauses on which we need to call ProcessClauseToSimplifyOthers().
  // See ProcessAllClauses().
  std::vector<bool> in_clause_to_process_;
  std::deque<ClauseIndex> clause_to_process_;

  // The set of all clauses.
  // An empty clause means that it has been removed.
  std::vector<std::vector<Literal>> clauses_;  // Indexed by ClauseIndex

  // Occurence list. For each literal, contains the ClauseIndex of the clause
  // that contains it (ordered by clause index).
  ITIVector<LiteralIndex, std::vector<ClauseIndex>> literal_to_clauses_;

  // Because we only lazily clean the occurence list after clause deletions,
  // we keep the size of the occurence list (without the deleted clause) here.
  ITIVector<LiteralIndex, int> literal_to_clauses_sizes_;

  // Used for postsolve.
  SatPostsolver postsolver_;

  SatParameters parameters_;
  DISALLOW_COPY_AND_ASSIGN(SatPresolver);
};

// Visible for testing. Returns true iff:
// - a subsume b (subsumption): the clause a is a subset of b, in which case
//   opposite_literal is set to -1.
// - b is strengthened by self-subsumption using a (self-subsuming resolution):
//   the clause a with one of its literal negated is a subset of b, in which
//   case opposite_literal is set to this negated literal index. Moreover, this
//   opposite_literal is then removed from b.
bool SimplifyClause(const std::vector<Literal>& a, std::vector<Literal>* b,
                    LiteralIndex* opposite_literal);

// Visible for testing. Computes the resolvant of 'a' and 'b' obtained by
// performing the resolution on 'x'. If the resolvant is trivially true this
// returns false, otherwise it returns true and fill 'out' with the resolvant.
//
// Note that the resolvant is just 'a' union 'b' with the literals 'x' and
// not(x) removed. The two clauses are assumed to be sorted, and the computed
// resolvant will also be sorted.
//
// This is the basic operation when a variable is eliminated by clause
// distribution.
bool ComputeResolvant(Literal x, const std::vector<Literal>& a,
                      const std::vector<Literal>& b, std::vector<Literal>* out);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_SIMPLIFICATION_H_
