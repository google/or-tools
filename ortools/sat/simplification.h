// Copyright 2010-2017 Google
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

// Implementation of a pure SAT presolver. This roughly follows the paper:
//
// "Effective Preprocessing in SAT through Variable and Clause Elimination",
// Niklas Een and Armin Biere, published in the SAT 2005 proceedings.

#ifndef OR_TOOLS_SAT_SIMPLIFICATION_H_
#define OR_TOOLS_SAT_SIMPLIFICATION_H_

#include <deque>
#include <unordered_map>
#include <memory>
#include <set>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/base/macros.h"
#include "ortools/base/span.h"
#include "ortools/base/int_type.h"
#include "ortools/base/int_type_indexed_vector.h"
#include "ortools/sat/drat.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/base/adjustable_priority_queue.h"

namespace operations_research {
namespace sat {

// A simple sat postsolver.
//
// The idea is that any presolve algorithm can just update this class, and at
// the end, this class will recover a solution of the initial problem from a
// solution of the presolved problem.
class SatPostsolver {
 public:
  explicit SatPostsolver(int num_variables);

  // The postsolver will process the Add() calls in reverse order. If the given
  // clause has all its literals at false, it simply sets the literal x to true.
  // Note that x must be a literal of the given clause.
  void Add(Literal x, const gtl::Span<Literal> clause);

  // Tells the postsolver that the given literal must be true in any solution.
  // We currently check that the variable is not already fixed.
  //
  // TODO(user): this as almost the same effect as adding an unit clause, and we
  // should probably remove this to simplify the code.
  void FixVariable(Literal x);

  // This assumes that the given variable mapping has been applied to the
  // problem. All the subsequent Add() and FixVariable() will refer to the new
  // problem. During postsolve, the initial solution must also correspond to
  // this new problem. Note that if mapping[v] == -1, then the literal v is
  // assumed to be deleted.
  //
  // This can be called more than once. But each call must refer to the current
  // variables set (after all the previous mapping have been applied).
  void ApplyMapping(const ITIVector<BooleanVariable, BooleanVariable>& mapping);

  // Extracts the current assignment of the given solver and postsolve it.
  //
  // Node(fdid): This can currently be called only once (but this is easy to
  // change since only some CHECK will fail).
  std::vector<bool> ExtractAndPostsolveSolution(const SatSolver& solver);
  std::vector<bool> PostsolveSolution(const std::vector<bool>& solution);

  // Getters to the clauses managed by this class.
  int NumClauses() const { return clauses_start_.size(); }
  std::vector<Literal> Clause(int i) const {
    // TODO(user): we could avoid the copy here, but because clauses_literals_
    // is a deque, we do need a special return class and cannot juste use
    // gtl::Span<Literal> for instance.
    const int begin = clauses_start_[i];
    const int end = i + 1 < clauses_start_.size() ? clauses_start_[i + 1]
                                                  : clauses_literals_.size();
    return std::vector<Literal>(clauses_literals_.begin() + begin,
                                clauses_literals_.begin() + end);
  }

 private:
  Literal ApplyReverseMapping(Literal l);
  void Postsolve(VariablesAssignment* assignment) const;

  // The presolve can add new variables, so we need to store the number of
  // original variables in order to return a solution with the correct number
  // of variables.
  const int initial_num_variables_;
  int num_variables_;

  // Stores the arguments of the Add() calls: clauses_start_[i] is the index of
  // the first literal of the clause #i in the clauses_literals_ deque.
  std::vector<int> clauses_start_;
  std::deque<Literal> clauses_literals_;
  std::vector<Literal> associated_literal_;

  // All the added clauses will be mapped back to the initial variables using
  // this reverse mapping. This way, clauses_ and associated_literal_ are only
  // in term of the initial problem.
  ITIVector<BooleanVariable, BooleanVariable> reverse_mapping_;

  // This will stores the fixed variables value and later the postsolved
  // assignment.
  VariablesAssignment assignment_;

  DISALLOW_COPY_AND_ASSIGN(SatPostsolver);
};

// This class holds a SAT problem (i.e. a set of clauses) and the logic to
// presolve it by a series of subsumption, self-subsuming resolution, and
// variable elimination by clause distribution.
//
// Note that this does propagate unit-clauses, but probably much
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

  explicit SatPresolver(SatPostsolver* postsolver)
      : postsolver_(postsolver),
        num_trivial_clauses_(0),
        drat_writer_(nullptr) {}
  void SetParameters(const SatParameters& params) { parameters_ = params; }

  // Registers a mapping to encode equivalent literals.
  // See ProbeAndFindEquivalentLiteral().
  void SetEquivalentLiteralMapping(
      const ITIVector<LiteralIndex, LiteralIndex>& mapping) {
    equiv_mapping_ = mapping;
  }

  // Adds new clause to the SatPresolver.
  void SetNumVariables(int num_variables);
  void AddBinaryClause(Literal a, Literal b);
  void AddClause(gtl::Span<Literal> clause);

  // Presolves the problem currently loaded. Returns false if the model is
  // proven to be UNSAT during the presolving.
  //
  // TODO(user): Add support for a time limit and some kind of iterations limit
  // so that this can never take too much time.
  bool Presolve();

  // Same as Presolve() but only allow to remove BooleanVariable whose index
  // is set to true in the given vector.
  bool Presolve(const std::vector<bool>& var_that_can_be_removed);

  // All the clauses managed by this class.
  // Note that deleted clauses keep their indices (they are just empty).
  int NumClauses() const { return clauses_.size(); }
  const std::vector<Literal>& Clause(ClauseIndex ci) const {
    return clauses_[ci];
  }

  // The number of variables. This is computed automatically from the clauses
  // added to the SatPresolver.
  int NumVariables() const { return literal_to_clause_sizes_.size() / 2; }

  // After presolving, Some variables in [0, NumVariables()) have no longer any
  // clause pointing to them. This return a mapping that maps this interval to
  // [0, new_size) such that now all variables are used. The unused variable
  // will be mapped to BooleanVariable(-1).
  ITIVector<BooleanVariable, BooleanVariable> VariableMapping() const;

  // Loads the current presolved problem in to the given sat solver.
  // Note that the variables will be re-indexed according to the mapping given
  // by GetMapping() so that they form a dense set.
  //
  // IMPORTANT: This is not const because it deletes the presolver clauses as
  // they are added to the SatSolver in order to save memory. After this is
  // called, only VariableMapping() will still works.
  void LoadProblemIntoSatSolver(SatSolver* solver);

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

  // Visible for testing. Just applies the BVA step of the presolve.
  void PresolveWithBva();

  void SetDratWriter(DratWriter* drat_writer) { drat_writer_ = drat_writer; }

 private:
  // Internal function to add clauses generated during the presolve. The clause
  // must already be sorted with the default Literal order and will be cleared
  // after this call.
  void AddClauseInternal(std::vector<Literal>* clause);

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
  Literal FindLiteralWithShortestOccurrenceList(
      const std::vector<Literal>& clause);
  LiteralIndex FindLiteralWithShortestOccurrenceListExcluding(
      const std::vector<Literal>& clause, Literal to_exclude);

  // Tests and maybe perform a Simple Bounded Variable addition starting from
  // the given literal as described in the paper: "Automated Reencoding of
  // Boolean Formulas", Norbert Manthey, Marijn J. H. Heule, and Armin Biere,
  // Volume 7857 of the series Lecture Notes in Computer Science pp 102-117,
  // 2013.
  // https://www.research.ibm.com/haifa/conferences/hvc2012/papers/paper16.pdf
  //
  // This seems to have a mostly positive effect, except on the crafted problem
  // familly mugrauer_balint--GI.crafted_nxx_d6_cx_numxx where the reduction
  // is big, but apparently the problem is harder to prove UNSAT for the solver.
  void SimpleBva(LiteralIndex l);

  // Display some statistics on the current clause database.
  void DisplayStats(double elapsed_seconds);

  // The "active" variables on which we want to call CrossProduct() are kept
  // in a priority queue so that we process first the ones that occur the least
  // often in the clause database.
  void InitializePriorityQueue();
  void UpdatePriorityQueue(BooleanVariable var);
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
    BooleanVariable variable;
    double weight;
  };
  ITIVector<BooleanVariable, PQElement> var_pq_elements_;
  AdjustablePriorityQueue<PQElement> var_pq_;

  // Literal priority queue for BVA. The literals are ordered by descending
  // number of occurrences in clauses.
  void InitializeBvaPriorityQueue();
  void UpdateBvaPriorityQueue(LiteralIndex var);
  void AddToBvaPriorityQueue(LiteralIndex var);
  struct BvaPqElement {
    BvaPqElement() : heap_index(-1), literal(-1), weight(0.0) {}

    // Interface for the AdjustablePriorityQueue.
    void SetHeapIndex(int h) { heap_index = h; }
    int GetHeapIndex() const { return heap_index; }

    // Priority order.
    // The AdjustablePriorityQueue returns the largest element first.
    bool operator<(const BvaPqElement& other) const {
      return weight < other.weight;
    }

    int heap_index;
    LiteralIndex literal;
    double weight;
  };
  std::deque<BvaPqElement> bva_pq_elements_;  // deque because we add variables.
  AdjustablePriorityQueue<BvaPqElement> bva_pq_;

  // Temporary data for SimpleBva().
  std::set<LiteralIndex> m_lit_;
  std::vector<ClauseIndex> m_cls_;
  std::unordered_map<LiteralIndex, std::vector<ClauseIndex>> p_;
  std::vector<Literal> tmp_new_clause_;

  // List of clauses on which we need to call ProcessClauseToSimplifyOthers().
  // See ProcessAllClauses().
  std::vector<bool> in_clause_to_process_;
  std::deque<ClauseIndex> clause_to_process_;

  // The set of all clauses.
  // An empty clause means that it has been removed.
  std::vector<std::vector<Literal>> clauses_;  // Indexed by ClauseIndex

  // Occurrence list. For each literal, contains the ClauseIndex of the clause
  // that contains it (ordered by clause index).
  ITIVector<LiteralIndex, std::vector<ClauseIndex>> literal_to_clauses_;

  // Because we only lazily clean the occurrence list after clause deletions,
  // we keep the size of the occurrence list (without the deleted clause) here.
  ITIVector<LiteralIndex, int> literal_to_clause_sizes_;

  // Used for postsolve.
  SatPostsolver* postsolver_;

  // Equivalent literal mapping.
  ITIVector<LiteralIndex, LiteralIndex> equiv_mapping_;

  int num_trivial_clauses_;
  SatParameters parameters_;
  DratWriter* drat_writer_;

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

// Visible for testing. Returns kNoLiteralIndex except if:
// - a and b differ in only one literal.
// - For a it is the given literal l.
// In which case, returns the LiteralIndex of the literal in b that is not in a.
LiteralIndex DifferAtGivenLiteral(const std::vector<Literal>& a,
                                  const std::vector<Literal>& b, Literal l);

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

// Same as ComputeResolvant() but just returns the resolvant size.
// Returns -1 when ComputeResolvant() returns false.
int ComputeResolvantSize(Literal x, const std::vector<Literal>& a,
                         const std::vector<Literal>& b);

// Presolver that does literals probing and finds equivalent literals by
// computing the strongly connected components of the graph:
//   literal l -> literals propagated by l.
//
// Clears the mapping if there are no equivalent literals. Otherwise, mapping[l]
// is the representative of the equivalent class of l. Note that mapping[l] may
// be equal to l.
//
// The postsolver will be updated so it can recover a solution of the mapped
// problem. Note that this works on any problem the SatSolver can handle, not
// only pure SAT problem, but the returned mapping do need to be applied to all
// constraints.
void ProbeAndFindEquivalentLiteral(
    SatSolver* solver, SatPostsolver* postsolver, DratWriter* drat_writer,
    ITIVector<LiteralIndex, LiteralIndex>* mapping);

// Given a 'solver' with a problem already loaded, this will try to simplify the
// problem (i.e. presolve it) before calling solver->Solve(). In the process,
// because of the way the presolve is implemented, the underlying SatSolver may
// change (it is why we use this unique_ptr interface). In particular, the final
// variables and 'solver' state may have nothing to do with the problem
// originaly present in the solver. That said, if the problem is shown to be
// SAT, then the returned solution will be in term of the original variables.
//
// Note that the full presolve is only executed if the problem is a pure SAT
// problem with only clauses.
SatSolver::Status SolveWithPresolve(
    std::unique_ptr<SatSolver>* solver,
    std::vector<bool>* solution /* only filled if SAT */,
    DratWriter* drat_writer /* can be nullptr */);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_SIMPLIFICATION_H_
