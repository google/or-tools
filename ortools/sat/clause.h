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

// This file contains the solver internal representation of the clauses and the
// classes used for their propagation.

#ifndef OR_TOOLS_SAT_CLAUSE_H_
#define OR_TOOLS_SAT_CLAUSE_H_

#include <cstdint>
#include <deque>
#include <functional>
#include <new>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/inlined_vector.h"
#include "absl/functional/any_invocable.h"
#include "absl/log/check.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/types/span.h"
#include "ortools/base/strong_vector.h"
#include "ortools/graph/cliques.h"
#include "ortools/sat/drat_proof_handler.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/util.h"
#include "ortools/util/bitset.h"
#include "ortools/util/stats.h"
#include "ortools/util/strong_integers.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

// This is how the SatSolver stores a clause. A clause is just a disjunction of
// literals. In many places, we just use vector<literal> to encode one. But in
// the critical propagation code, we use this class to remove one memory
// indirection.
class SatClause {
 public:
  // Creates a sat clause. There must be at least 2 literals.
  // Clause with one literal fix variable directly and are never constructed.
  // Note that in practice, we use BinaryImplicationGraph for the clause of size
  // 2, so this is used for size at least 3.
  static SatClause* Create(absl::Span<const Literal> literals);

  // Non-sized delete because this is a tail-padded class.
  void operator delete(void* p) {
    ::operator delete(p);  // non-sized delete
  }

  // Number of literals in the clause.
  int size() const { return size_; }

  // We re-use the size to lazily remove clause and notify that they need to be
  // deleted. It is why this is not called empty() to emphasis that fact. Note
  // that we never create an initially empty clause, so there is no confusion
  // with an infeasible model with an empty clause inside.
  int IsRemoved() const { return size_ == 0; }

  // Allows for range based iteration: for (Literal literal : clause) {}.
  const Literal* begin() const { return &(literals_[0]); }
  const Literal* end() const { return &(literals_[size_]); }

  // Returns the first and second literals. These are always the watched
  // literals if the clause is attached in the LiteralWatchers.
  Literal FirstLiteral() const { return literals_[0]; }
  Literal SecondLiteral() const { return literals_[1]; }

  // Returns the literal that was propagated to true. This only works for a
  // clause that just propagated this literal. Otherwise, this will just returns
  // a literal of the clause.
  Literal PropagatedLiteral() const { return literals_[0]; }

  // Returns the reason for the last unit propagation of this clause. The
  // preconditions are the same as for PropagatedLiteral(). Note that we don't
  // need to include the propagated literal.
  absl::Span<const Literal> PropagationReason() const {
    return absl::Span<const Literal>(&(literals_[1]), size_ - 1);
  }

  // Returns a Span<> representation of the clause.
  absl::Span<const Literal> AsSpan() const {
    return absl::Span<const Literal>(&(literals_[0]), size_);
  }

  // Removes literals that are fixed. This should only be called at level 0
  // where a literal is fixed iff it is assigned. Aborts and returns true if
  // they are not all false.
  //
  // Note that the removed literal can still be accessed in the portion [size,
  // old_size) of literals().
  bool RemoveFixedLiteralsAndTestIfTrue(const VariablesAssignment& assignment);

  // Returns true if the clause is satisfied for the given assignment. Note that
  // the assignment may be partial, so false does not mean that the clause can't
  // be satisfied by completing the assignment.
  bool IsSatisfied(const VariablesAssignment& assignment) const;

  std::string DebugString() const;

 private:
  // The manager needs to permute the order of literals in the clause and
  // call Clear()/Rewrite.
  friend class ClauseManager;

  Literal* literals() { return &(literals_[0]); }

  // Marks the clause so that the next call to CleanUpWatchers() can identify it
  // and actually remove it. We use size_ = 0 for this since the clause will
  // never be used afterwards.
  void Clear() { size_ = 0; }

  // Rewrites a clause with another shorter one. Note that the clause shouldn't
  // be attached when this is called.
  void Rewrite(absl::Span<const Literal> new_clause) {
    size_ = 0;
    for (const Literal l : new_clause) literals_[size_++] = l;
  }

  int32_t size_;

  // This class store the literals inline, and literals_ mark the starts of the
  // variable length portion.
  Literal literals_[0];
};

// Clause information used for the clause database management. Note that only
// the clauses that can be removed have an info. The problem clauses and
// the learned one that we wants to keep forever do not have one.
struct ClauseInfo {
  double activity = 0.0;
  int32_t lbd = 0;
  bool protected_during_next_cleanup = false;
};

class BinaryImplicationGraph;

// Stores the 2-watched literals data structure.  See
// http://www.cs.berkeley.edu/~necula/autded/lecture24-sat.pdf for
// detail.
//
// This class is also responsible for owning the clause memory and all related
// information.
class ClauseManager : public SatPropagator {
 public:
  explicit ClauseManager(Model* model);

  // This type is neither copyable nor movable.
  ClauseManager(const ClauseManager&) = delete;
  ClauseManager& operator=(const ClauseManager&) = delete;

  ~ClauseManager() override;

  // Must be called before adding clauses referring to such variables.
  void Resize(int num_variables);

  // SatPropagator API.
  bool Propagate(Trail* trail) final;
  absl::Span<const Literal> Reason(const Trail& trail, int trail_index,
                                   int64_t conflict_id) const final;

  // Returns the reason of the variable at given trail_index. This only works
  // for variable propagated by this class and is almost the same as Reason()
  // with a different return format.
  SatClause* ReasonClause(int trail_index) const;

  // Adds a new clause and perform initial propagation for this clause only.
  bool AddClause(absl::Span<const Literal> literals, Trail* trail, int lbd);
  bool AddClause(absl::Span<const Literal> literals);

  // Same as AddClause() for a removable clause. This is only called on learned
  // conflict, so this should never have all its literal at false (CHECKED).
  SatClause* AddRemovableClause(absl::Span<const Literal> literals,
                                Trail* trail, int lbd);

  // Lazily detach the given clause. The deletion will actually occur when
  // CleanUpWatchers() is called. The later needs to be called before any other
  // function in this class can be called. This is DCHECKed.
  //
  // Note that we remove the clause from clauses_info_ right away.
  void LazyDetach(SatClause* clause);
  void CleanUpWatchers();

  // Detaches the given clause right away.
  //
  // TODO(user): It might be better to have a "slower" mode in
  // PropagateOnFalse() that deal with detached clauses in the watcher list and
  // is activated until the next CleanUpWatchers() calls.
  void Detach(SatClause* clause);

  // Attaches the given clause. The first two literal of the clause must
  // be unassigned and the clause must not be already attached.
  void Attach(SatClause* clause, Trail* trail);

  // Reclaims the memory of the lazily removed clauses (their size was set to
  // zero) and remove them from AllClausesInCreationOrder() this work in
  // O(num_clauses()).
  void DeleteRemovedClauses();
  int64_t num_clauses() const { return clauses_.size(); }
  const std::vector<SatClause*>& AllClausesInCreationOrder() const {
    return clauses_;
  }

  // True if removing this clause will not change the set of feasible solution.
  // This is the case for clauses that were learned during search. Note however
  // that some learned clause are kept forever (heuristics) and do not appear
  // here.
  bool IsRemovable(SatClause* const clause) const {
    return clauses_info_.contains(clause);
  }
  int64_t num_removable_clauses() const { return clauses_info_.size(); }
  absl::flat_hash_map<SatClause*, ClauseInfo>* mutable_clauses_info() {
    return &clauses_info_;
  }

  // Total number of clauses inspected during calls to PropagateOnFalse().
  int64_t num_inspected_clauses() const { return num_inspected_clauses_; }
  int64_t num_inspected_clause_literals() const {
    return num_inspected_clause_literals_;
  }

  // The number of different literals (always twice the number of variables).
  int64_t literal_size() const { return needs_cleaning_.size().value(); }

  // Number of clauses currently watched.
  int64_t num_watched_clauses() const { return num_watched_clauses_; }

  void SetDratProofHandler(DratProofHandler* drat_proof_handler) {
    drat_proof_handler_ = drat_proof_handler;
  }

  // Methods implementing pseudo-iterators over the clause database that are
  // stable across cleanups. They all return nullptr if there are no more
  // clauses.

  // Returns the next clause to minimize that has never been minimized before.
  // Note that we only minimize clauses kept forever.
  SatClause* NextNewClauseToMinimize();
  // Returns the next clause to minimize, this iterator will be reset to the
  // start so the clauses will be returned in round-robin order.
  // Note that we only minimize clauses kept forever.
  SatClause* NextClauseToMinimize();
  // Returns the next clause to probe in round-robin order.
  SatClause* NextClauseToProbe();

  // Restart the scans.
  void ResetToProbeIndex() { to_probe_index_ = 0; }
  void ResetToMinimizeIndex() { to_minimize_index_ = 0; }
  // Ensures that NextNewClauseToMinimize() returns only learned clauses.
  // This is a noop after the first call.
  void EnsureNewClauseIndexInitialized() {
    if (to_first_minimize_index_ > 0) return;
    to_first_minimize_index_ = clauses_.size();
  }

  // During an inprocessing phase, it is easier to detach all clause first,
  // then simplify and then reattach them. Note however that during these
  // two calls, it is not possible to use the solver unit-progation.
  //
  // Important: When reattach is called, we assume that none of their literal
  // are fixed, so we don't do any special checks.
  //
  // These functions can be called multiple-time and do the right things. This
  // way before doing something, you can call the corresponding function and be
  // sure to be in a good state. I.e. always AttachAllClauses() before
  // propagation and DetachAllClauses() before going to do an inprocessing pass
  // that might transform them.
  void DetachAllClauses();
  void AttachAllClauses();

  // These must only be called between [Detach/Attach]AllClauses() calls.
  void InprocessingRemoveClause(SatClause* clause);
  ABSL_MUST_USE_RESULT bool InprocessingFixLiteral(Literal true_literal);
  ABSL_MUST_USE_RESULT bool InprocessingRewriteClause(
      SatClause* clause, absl::Span<const Literal> new_clause);

  // This can return nullptr if new_clause was of size one or two as these are
  // treated differently. Note that none of the variable should be fixed in the
  // given new clause.
  SatClause* InprocessingAddClause(absl::Span<const Literal> new_clause);

  // Contains, for each literal, the list of clauses that need to be inspected
  // when the corresponding literal becomes false.
  struct Watcher {
    Watcher() = default;
    Watcher(SatClause* c, Literal b, int i = 2)
        : blocking_literal(b), start_index(i), clause(c) {}

    // Optimization. A literal from the clause that sometimes allow to not even
    // look at the clause memory when true.
    Literal blocking_literal;

    // Optimization. An index in the clause. Instead of looking for another
    // literal to watch from the start, we will start from here instead, and
    // loop around if needed. This allows to avoid bad quadratic corner cases
    // and lead to an "optimal" complexity. See "Optimal Implementation of
    // Watched Literals and more General Techniques", Ian P. Gent.
    //
    // Note that ideally, this should be part of a SatClause, so it can be
    // shared across watchers. However, since we have 32 bits for "free" here
    // because of the struct alignment, we store it here instead.
    int32_t start_index;

    SatClause* clause;
  };

  // This is exposed since some inprocessing code can heuristically exploit the
  // currently watched literal and blocking literal to do some simplification.
  const std::vector<Watcher>& WatcherListOnFalse(Literal false_literal) const {
    return watchers_on_false_[false_literal];
  }

  void SetAddClauseCallback(
      absl::AnyInvocable<void(int lbd, absl::Span<const Literal>)>
          add_clause_callback) {
    add_clause_callback_ = std::move(add_clause_callback);
  }

  // Removes the add clause callback and returns it. This can be used to
  // temporarily disable the callback.
  absl::AnyInvocable<void(int lbd, absl::Span<const Literal>)>
  TakeAddClauseCallback() {
    return std::move(add_clause_callback_);
  }

 private:
  // Attaches the given clause. This eventually propagates a literal which is
  // enqueued on the trail. Returns false if a contradiction was encountered.
  bool AttachAndPropagate(SatClause* clause, Trail* trail);

  // Launches all propagation when the given literal becomes false.
  // Returns false if a contradiction was encountered.
  bool PropagateOnFalse(Literal false_literal, Trail* trail);

  // Attaches the given clause to the event: the given literal becomes false.
  // The blocking_literal can be any literal from the clause, it is used to
  // speed up PropagateOnFalse() by skipping the clause if it is true.
  void AttachOnFalse(Literal literal, Literal blocking_literal,
                     SatClause* clause);

  // Common code between LazyDetach() and Detach().
  void InternalDetach(SatClause* clause);

  util_intops::StrongVector<LiteralIndex, std::vector<Watcher>>
      watchers_on_false_;

  // SatClause reasons by trail_index.
  std::vector<SatClause*> reasons_;

  // Indicates if the corresponding watchers_on_false_ list need to be
  // cleaned. The boolean is_clean_ is just used in DCHECKs.
  SparseBitset<LiteralIndex> needs_cleaning_;
  bool is_clean_ = true;

  BinaryImplicationGraph* implication_graph_;
  Trail* trail_;

  int64_t num_inspected_clauses_;
  int64_t num_inspected_clause_literals_;
  int64_t num_watched_clauses_;
  mutable StatsGroup stats_;

  // For DetachAllClauses()/AttachAllClauses().
  bool all_clauses_are_attached_ = true;

  // All the clauses currently in memory. This vector has ownership of the
  // pointers. We currently do not use std::unique_ptr<SatClause> because it
  // can't be used with some STL algorithms like std::partition.
  //
  // Note that the unit clauses and binary clause are not kept here.
  std::vector<SatClause*> clauses_;

  // TODO(user): If more indices are needed, switch to a generic API.
  int to_minimize_index_ = 0;
  int to_first_minimize_index_ = 0;
  int to_probe_index_ = 0;

  // Only contains removable clause.
  absl::flat_hash_map<SatClause*, ClauseInfo> clauses_info_;

  DratProofHandler* drat_proof_handler_ = nullptr;

  absl::AnyInvocable<void(int lbd, absl::Span<const Literal>)>
      add_clause_callback_ = nullptr;
};

// A binary clause. This is used by BinaryClauseManager.
struct BinaryClause {
  BinaryClause(Literal _a, Literal _b) : a(_a), b(_b) {}
  bool operator==(BinaryClause o) const { return a == o.a && b == o.b; }
  bool operator!=(BinaryClause o) const { return a != o.a || b != o.b; }
  Literal a;
  Literal b;
};

// A simple class to manage a set of binary clauses.
class BinaryClauseManager {
 public:
  BinaryClauseManager() = default;

  // This type is neither copyable nor movable.
  BinaryClauseManager(const BinaryClauseManager&) = delete;
  BinaryClauseManager& operator=(const BinaryClauseManager&) = delete;

  int NumClauses() const { return set_.size(); }

  // Adds a new binary clause to the manager and returns true if it wasn't
  // already present.
  bool Add(BinaryClause c) {
    std::pair<int, int> p(c.a.SignedValue(), c.b.SignedValue());
    if (p.first > p.second) std::swap(p.first, p.second);
    if (set_.find(p) == set_.end()) {
      set_.insert(p);
      newly_added_.push_back(c);
      return true;
    }
    return false;
  }

  // Returns the newly added BinaryClause since the last ClearNewlyAdded() call.
  const std::vector<BinaryClause>& newly_added() const { return newly_added_; }
  void ClearNewlyAdded() { newly_added_.clear(); }

 private:
  absl::flat_hash_set<std::pair<int, int>> set_;
  std::vector<BinaryClause> newly_added_;
};

// Special class to store and propagate clauses of size 2 (i.e. implication).
// Such clauses are never deleted. Together, they represent the 2-SAT part of
// the problem. Note that 2-SAT satisfiability is a polynomial problem, but
// W2SAT (weighted 2-SAT) is NP-complete.
//
// TODO(user): Most of the note below are done, but we currently only applies
// the reduction before the solve. We should consider doing more in-processing.
// The code could probably still be improved too.
//
// Note(user): All the variables in a strongly connected component are
// equivalent and can be thus merged as one. This is relatively cheap to compute
// from time to time (linear complexity). We will also get contradiction (a <=>
// not a) this way. This is done by DetectEquivalences().
//
// Note(user): An implication (a => not a) implies that a is false. I am not
// sure it is worth detecting that because if the solver assign a to true, it
// will learn that right away. I don't think we can do it faster.
//
// Note(user): The implication graph can be pruned. This is called the
// transitive reduction of a graph. For instance If a => {b,c} and b => {c},
// then there is no need to store a => {c}. The transitive reduction is unique
// on an acyclic graph. Computing it will allow for a faster propagation and
// memory reduction. It is however not cheap. Maybe simple lazy heuristics to
// remove redundant arcs are better. Note that all the learned clauses we add
// will never be redundant (but they could introduce cycles). This is done
// by ComputeTransitiveReduction().
//
// Note(user): This class natively support at most one constraints. This is
// a way to reduced significantly the memory and size of some 2-SAT instances.
// However, it is not fully exploited for pure SAT problems. See
// TransformIntoMaxCliques().
//
// Note(user): Add a preprocessor to remove duplicates in the implication lists.
// Note that all the learned clauses we add will never create duplicates.
//
// References for most of the above and more:
// - Brafman RI, "A simplifier for propositional formulas with many binary
//   clauses", IEEE Trans Syst Man Cybern B Cybern. 2004 Feb;34(1):52-9.
//   http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.28.4911
// - Marijn J. H. Heule, Matti Järvisalo, Armin Biere, "Efficient CNF
//   Simplification Based on Binary Implication Graphs", Theory and Applications
//   of Satisfiability Testing - SAT 2011, Lecture Notes in Computer Science
//   Volume 6695, 2011, pp 201-215
//   http://www.cs.helsinki.fi/u/mjarvisa/papers/heule-jarvisalo-biere.sat11.pdf
class BinaryImplicationGraph : public SatPropagator {
 public:
  explicit BinaryImplicationGraph(Model* model)
      : SatPropagator("BinaryImplicationGraph"),
        stats_("BinaryImplicationGraph"),
        time_limit_(model->GetOrCreate<TimeLimit>()),
        random_(model->GetOrCreate<ModelRandomGenerator>()),
        trail_(model->GetOrCreate<Trail>()),
        at_most_one_max_expansion_size_(
            model->GetOrCreate<SatParameters>()
                ->at_most_one_max_expansion_size()) {
    trail_->RegisterPropagator(this);
  }

  // This type is neither copyable nor movable.
  BinaryImplicationGraph(const BinaryImplicationGraph&) = delete;
  BinaryImplicationGraph& operator=(const BinaryImplicationGraph&) = delete;

  ~BinaryImplicationGraph() override {
    IF_STATS_ENABLED({
      LOG(INFO) << stats_.StatString();
      LOG(INFO) << "num_redundant_implications " << num_redundant_implications_;
    });
  }

  // SatPropagator interface.
  bool Propagate(Trail* trail) final;
  absl::Span<const Literal> Reason(const Trail& trail, int trail_index,
                                   int64_t conflict_id) const final;

  // Resizes the data structure.
  void Resize(int num_variables);

  // Returns the "size" of this class, that is 2 * num_boolean_variables as
  // updated by the larger Resize() call.
  int64_t literal_size() const { return implications_.size(); }

  // Returns true if no constraints where ever added to this class.
  bool IsEmpty() const final { return no_constraint_ever_added_; }

  // Adds the binary clause (a OR b), which is the same as (not a => b).
  // Note that it is also equivalent to (not b => a).
  //
  // Preconditions:
  // - If we are at root node, then none of the literal should be assigned.
  //   This is Checked and is there to track inefficiency as we do not need a
  //   clause in this case.
  // - If we are at a positive decision level, we will propagate something if
  //   we can. However, if both literal are false, we will just return false
  //   and do nothing. In all other case, we will return true.
  bool AddBinaryClause(Literal a, Literal b);
  bool AddImplication(Literal a, Literal b) {
    return AddBinaryClause(a.Negated(), b);
  }

  // When set, the callback will be called on ALL newly added binary clauses.
  //
  // The EnableSharing() function can be used to disable sharing temporarily for
  // the clauses that are imported from the Shared repository already.
  //
  // TODO(user): this is meant to share clause between workers, hopefully the
  // contention will not be too high. Double check and maybe add a batch version
  // were we keep new implication and add them in batches.
  void EnableSharing(bool enable) { enable_sharing_ = enable; }
  void SetAdditionCallback(std::function<void(Literal, Literal)> f) {
    add_binary_callback_ = f;
  }
  // An at most one constraint of size n is a compact way to encode n * (n - 1)
  // implications. This must only be called at level zero.
  //
  // Returns false if this creates a conflict. Currently this can only happens
  // if there is duplicate literal already assigned to true in this constraint.
  //
  // TODO(user): Our algorithm could generalize easily to at_most_ones + a list
  // of literals that will be false if one of the literal in the amo is at one.
  // It is a way to merge common list of implications.
  //
  // If the final AMO size is smaller than at_most_one_expansion_size
  // parameters, we fully expand it.
  ABSL_MUST_USE_RESULT bool AddAtMostOne(absl::Span<const Literal> at_most_one);

  // Uses the binary implication graph to minimize the given conflict by
  // removing literals that implies others. The idea is that if a and b are two
  // literals from the given conflict and a => b (which is the same as not(b) =>
  // not(a)) then a is redundant and can be removed.
  //
  // Note that removing as many literals as possible is too time consuming, so
  // we use different heuristics/algorithms to do this minimization.
  // See the binary_minimization_algorithm SAT parameter and the .cc for more
  // details about the different algorithms.
  void MinimizeConflictWithReachability(std::vector<Literal>* c);
  void MinimizeConflictExperimental(const Trail& trail,
                                    std::vector<Literal>* c);
  void MinimizeConflictFirst(const Trail& trail, std::vector<Literal>* c,
                             SparseBitset<BooleanVariable>* marked);
  void MinimizeConflictFirstWithTransitiveReduction(const Trail& trail,
                                                    std::vector<Literal>* c,
                                                    absl::BitGenRef random);

  // This must only be called at decision level 0 after all the possible
  // propagations. It:
  // - Removes the variable at true from the implications lists.
  // - Frees the propagation list of the assigned literals.
  void RemoveFixedVariables();

  // Returns false if the model is unsat, otherwise detects equivalent variable
  // (with respect to the implications only) and reorganize the propagation
  // lists accordingly.
  //
  // TODO(user): Completely get rid of such literal instead? it might not be
  // reasonable code-wise to remap our literals in all of our constraints
  // though.
  bool DetectEquivalences(bool log_info = false);

  // Returns true if DetectEquivalences() has been called and no new binary
  // clauses have been added since then. When this is true then there is no
  // cycle in the binary implication graph (modulo the redundant literals that
  // form a cycle with their representative).
  bool IsDag() const { return is_dag_; }

  // One must call DetectEquivalences() first, this is CHECKed.
  // Returns a list so that if x => y, then x is after y.
  const std::vector<LiteralIndex>& ReverseTopologicalOrder() const {
    CHECK(is_dag_);
    return reverse_topological_order_;
  }

  // Returns the list of literal "directly" implied by l. Beware that this can
  // easily change behind your back if you modify the solver state.
  const absl::InlinedVector<Literal, 6>& Implications(Literal l) const {
    return implications_[l];
  }

  // Returns the representative of the equivalence class of l (or l itself if it
  // is on its own). Note that DetectEquivalences() should have been called to
  // get any non-trival results.
  Literal RepresentativeOf(Literal l) const {
    if (l.Index() >= representative_of_.size()) return l;
    if (representative_of_[l] == kNoLiteralIndex) return l;
    return Literal(representative_of_[l]);
  }

  // Prunes the implication graph by calling first DetectEquivalences() to
  // remove cycle and then by computing the transitive reduction of the
  // remaining DAG.
  //
  // Note that this can be slow (num_literals graph traversals), so we abort
  // early if we start doing too much work.
  //
  // Returns false if the model is detected to be UNSAT (this needs to call
  // DetectEquivalences() if not already done).
  bool ComputeTransitiveReduction(bool log_info = false);

  // Another way of representing an implication graph is a list of maximal "at
  // most one" constraints, each forming a max-clique in the incompatibility
  // graph. This representation is useful for having a good linear relaxation.
  //
  // This function will transform each of the given constraint into a maximal
  // one in the underlying implication graph. Constraints that are redundant
  // after other have been expanded (i.e. included into) will be cleared.
  // Note that the order of constraints will be conserved.
  //
  // Returns false if the model is detected to be UNSAT (this needs to call
  // DetectEquivalences() if not already done).
  bool TransformIntoMaxCliques(std::vector<std::vector<Literal>>* at_most_ones,
                               int64_t max_num_explored_nodes = 1e8);

  // This is similar to TransformIntoMaxCliques() but we are just looking into
  // reducing the number of constraints. If two initial clique A and B can be
  // merged into A U B, we do it. We do not extends clique further.
  //
  // This approach should minimize the number of overall literals. It should
  // be also enough for presolve. We can extend clique even more later for
  // faster propagation or better linear relaxation.
  //
  // Note that we can do that relatively efficiently, if the candidate for
  // extension of a clique A contains clique B, then we can just extend.
  // Moreover this is a symmetric relation. And if we look at the graph of
  // possible extension (A <-> B if A U B is a valid clique), then we can
  // find maximum clique in this graph which might be relatively small.
  //
  // TODO(user): Switch to a dtime limit.
  bool MergeAtMostOnes(absl::Span<std::vector<Literal>> at_most_ones,
                       int64_t max_num_explored_nodes = 1e8,
                       double* dtime = nullptr);

  // LP clique cut heuristic. Returns a set of "at most one" constraints on the
  // given literals or their negation that are violated by the current LP
  // solution. Note that this assumes that
  //   lp_value(lit) = 1 - lp_value(lit.Negated()).
  //
  // The literal and lp_values vector are in one to one correspondence. We will
  // only generate clique with these literals or their negation.
  //
  // TODO(user): Refine the heuristic and unit test!
  const std::vector<std::vector<Literal>>& GenerateAtMostOnesWithLargeWeight(
      absl::Span<const Literal> literals, absl::Span<const double> lp_values,
      absl::Span<const double> reduced_costs);

  // Heuristically identify "at most one" between the given literals, swap
  // them around and return these amo as span inside the literals vector.
  //
  // TODO(user): Add a limit to make sure this do not take too much time.
  std::vector<absl::Span<const Literal>> HeuristicAmoPartition(
      std::vector<Literal>* literals);

  // Number of literal propagated by this class (including conflicts).
  int64_t num_propagations() const { return num_propagations_; }

  // Number of literals inspected by this class during propagation.
  int64_t num_inspections() const { return num_inspections_; }

  // MinimizeClause() stats.
  int64_t num_minimization() const { return num_minimization_; }
  int64_t num_literals_removed() const { return num_literals_removed_; }

  // Returns true if this literal is fixed or is equivalent to another literal.
  // This means that it can just be ignored in most situation.
  //
  // Note that the set (and thus number) of redundant literal can only grow over
  // time. This is because we always use the lowest index as representative of
  // an equivalent class, so a redundant literal will stay that way.
  bool IsRedundant(Literal l) const { return is_redundant_[l]; }
  int64_t num_redundant_literals() const {
    CHECK_EQ(num_redundant_literals_ % 2, 0);
    return num_redundant_literals_;
  }

  // Number of implications removed by transitive reduction.
  int64_t num_redundant_implications() const {
    return num_redundant_implications_;
  }

  // Returns the number of current implications. Note that a => b and not(b)
  // => not(a) are counted separately since they appear separately in our
  // propagation lists. The number of size 2 clauses that represent the same
  // thing is half this number. This should only be used in logs.
  int64_t ComputeNumImplicationsForLog() const {
    int64_t result = 0;
    for (const auto& list : implications_) result += list.size();
    return result;
  }

  // Extract all the binary clauses managed by this class. The Output type must
  // support an AddBinaryClause(Literal a, Literal b) function.
  //
  // Important: This currently does NOT include at most one constraints.
  //
  // TODO(user): When extracting to cp_model.proto we could be more efficient
  // by extracting bool_and constraint with many lhs terms.
  template <typename Output>
  void ExtractAllBinaryClauses(Output* out) const {
    // TODO(user): Ideally we should just never have duplicate clauses in this
    // class. But it seems we do in some corner cases, so lets not output them
    // twice.
    absl::flat_hash_set<std::pair<LiteralIndex, LiteralIndex>>
        duplicate_detection;
    for (LiteralIndex i(0); i < implications_.size(); ++i) {
      const Literal a = Literal(i).Negated();
      for (const Literal b : implications_[i]) {
        // Note(user): We almost always have both a => b and not(b) => not(a) in
        // our implications_ database. Except if ComputeTransitiveReduction()
        // was aborted early, but in this case, if only one is present, the
        // other could be removed, so we shouldn't need to output it.
        if (a < b && duplicate_detection.insert({a, b}).second) {
          out->AddBinaryClause(a, b);
        }
      }
    }
  }

  void SetDratProofHandler(DratProofHandler* drat_proof_handler) {
    drat_proof_handler_ = drat_proof_handler;
  }

  // Changes the reason of the variable at trail index to a binary reason.
  // Note that the implication "new_reason => trail_[trail_index]" should be
  // part of the implication graph.
  void ChangeReason(int trail_index, Literal new_reason) {
    CHECK(trail_->Assignment().LiteralIsTrue(new_reason));
    reasons_[trail_index] = new_reason.Negated();
    trail_->ChangeReason(trail_index, propagator_id_);
  }

  // The literals that are "directly" implied when literal is set to true. This
  // is not a full "reachability". It includes at most ones propagation. The set
  // of all direct implications is enough to describe the implications graph
  // completely.
  //
  // When doing blocked clause elimination of bounded variable elimination, one
  // only need to consider this list and not the full reachability.
  const std::vector<Literal>& DirectImplications(Literal literal);

  // Returns a random literal in DirectImplications(lhs). Note that this is
  // biased if lhs appear in some most one, but it is constant time, which is a
  // lot faster than computing DirectImplications() and then sampling from it.
  LiteralIndex RandomImpliedLiteral(Literal lhs);

  // A proxy for DirectImplications().size(), However we currently do not
  // maintain it perfectly. It is exact each time DirectImplications() is
  // called, and we update it in some situation but we don't deal with fixed
  // variables, at_most ones and duplicates implications for now.
  int DirectImplicationsEstimatedSize(Literal literal) const {
    return estimated_sizes_[literal];
  }

  // Variable elimination by replacing everything of the form a => var => b by
  // a => b. We ignore any a => a so the number of new implications is not
  // always just the product of the two direct implication list of var and
  // not(var). However, if a => var => a, then a and var are equivalent, so this
  // case will be removed if one run DetectEquivalences() before this.
  // Similarly, if a => var => not(a) then a must be false and this is detected
  // and dealt with by FindFailedLiteralAroundVar().
  bool FindFailedLiteralAroundVar(BooleanVariable var, bool* is_unsat);
  int64_t NumImplicationOnVariableRemoval(BooleanVariable var);
  void RemoveBooleanVariable(
      BooleanVariable var, std::deque<std::vector<Literal>>* postsolve_clauses);
  bool IsRemoved(Literal l) const { return is_removed_[l]; }
  void RemoveAllRedundantVariables(
      std::deque<std::vector<Literal>>* postsolve_clauses);
  void CleanupAllRemovedAndFixedVariables();

  // ExpandAtMostOneWithWeight() will increase this, so a client can put a limit
  // on this possibly expansive operation.
  void ResetWorkDone() { work_done_in_mark_descendants_ = 0; }
  int64_t WorkDone() const { return work_done_in_mark_descendants_; }

  // Returns all the literals that are implied directly or indirectly by `root`.
  // The result must be used before the next call to this function.
  absl::Span<const Literal> GetAllImpliedLiterals(Literal root);

  // Same as ExpandAtMostOne() but try to maximize the weight in the clique.
  template <bool use_weight = true>
  std::vector<Literal> ExpandAtMostOneWithWeight(
      absl::Span<const Literal> at_most_one,
      const util_intops::StrongVector<LiteralIndex, bool>& can_be_included,
      const util_intops::StrongVector<LiteralIndex, double>&
          expanded_lp_values);

  // Restarts the at_most_one iterator.
  void ResetAtMostOneIterator() { at_most_one_iterator_ = 0; }

  // Returns the next at_most_one, or a span of size 0 when finished.
  absl::Span<const Literal> NextAtMostOne();

  // Clean up implications list that might have duplicates.
  void RemoveDuplicates();

 private:
  // Mark implications_[a] for cleanup in RemoveDuplicates().
  void NotifyPossibleDuplicate(Literal a);

  // Simple wrapper to not forget to output newly fixed variable to the DRAT
  // proof if needed. This will propagate right away the implications.
  bool FixLiteral(Literal true_literal);

  // Remove any literal whose negation is marked (except the first one).
  void RemoveRedundantLiterals(std::vector<Literal>* conflict);

  // Fill is_marked_ with all the descendant of root, and returns them.
  // Note that this also use bfs_stack_.
  absl::Span<const Literal> MarkDescendants(Literal root);

  // Expands greedily the given at most one until we get a maximum clique in
  // the underlying incompatibility graph. Note that there is no guarantee that
  // if this is called with any sub-clique of the result we will get the same
  // maximal clique.
  std::vector<Literal> ExpandAtMostOne(absl::Span<const Literal> at_most_one,
                                       int64_t max_num_explored_nodes);

  // Used by TransformIntoMaxCliques() and MergeAtMostOnes().
  std::vector<std::pair<int, int>> FilterAndSortAtMostOnes(
      absl::Span<std::vector<Literal>> at_most_ones);

  // Process all at most one constraints starting at or after base_index in
  // at_most_one_buffer_. This replace literal by their representative, remove
  // fixed literals and deal with duplicates. Return false iff the model is
  // UNSAT.
  //
  // If the final AMO size is smaller than the at_most_one_expansion_size
  // parameters, we fully expand it.
  ABSL_MUST_USE_RESULT bool CleanUpAndAddAtMostOnes(int base_index);

  // To be used in DCHECKs().
  bool InvariantsAreOk();

  // Return the at most one encoded at the given start.
  // Important: this is only valid until a new at_most one is added.
  absl::Span<const Literal> AtMostOne(int start) const;

  mutable StatsGroup stats_;
  TimeLimit* time_limit_;
  ModelRandomGenerator* random_;
  Trail* trail_;
  DratProofHandler* drat_proof_handler_ = nullptr;

  // When problems do not have any implications or at_most_ones this allows to
  // reduce the number of work we do here. This will be set to true the first
  // time something is added.
  bool no_constraint_ever_added_ = true;

  // Binary reasons by trail_index. We need a deque because we kept pointers to
  // elements of this array and this can dynamically change size.
  std::deque<Literal> reasons_;

  // This is indexed by the Index() of a literal. Each list stores the
  // literals that are implied if the index literal becomes true.
  //
  // Using InlinedVector helps quite a bit because on many problems, a literal
  // only implies a few others. Note that on a 64 bits computer we get exactly
  // 6 inlined int32_t elements without extra space, and the size of the inlined
  // vector is 4 times 64 bits.
  //
  // TODO(user): We could be even more efficient since a size of int32_t is
  // enough for us and we could store in common the inlined/not-inlined size.
  util_intops::StrongVector<LiteralIndex, absl::InlinedVector<Literal, 6>>
      implications_;

  // Used by RemoveDuplicates() and NotifyPossibleDuplicate().
  util_intops::StrongVector<LiteralIndex, bool> might_have_dups_;
  std::vector<Literal> to_clean_;

  // Internal representation of at_most_one constraints. Each entry point to the
  // start of a constraint in the buffer.
  //
  // TRICKY: The first literal is actually the size of the at_most_one.
  // Most users should just use AtMostOne(start).
  //
  // When LiteralIndex is true, then all entry in the at most one
  // constraint must be false except the one referring to LiteralIndex.
  //
  // TODO(user): We could be more cache efficient by combining this with
  // implications_ in some way. Do some propagation speed benchmark.
  util_intops::StrongVector<LiteralIndex, absl::InlinedVector<int32_t, 6>>
      at_most_ones_;
  std::vector<Literal> at_most_one_buffer_;
  const int at_most_one_max_expansion_size_;
  int at_most_one_iterator_ = 0;

  // Invariant: implies_something_[l] should be true iff implications_[l] or
  // at_most_ones_[l] might be non-empty.
  //
  // For problems with a large number of variables and sparse implications_ or
  // at_most_ones_ entries, checking this is way faster during
  // MarkDescendants(). See for instance proteindesign122trx11p8.pb.gz.
  Bitset64<LiteralIndex> implies_something_;

  // Used by GenerateAtMostOnesWithLargeWeight().
  std::vector<std::vector<Literal>> tmp_cuts_;

  // Some stats.
  int64_t num_propagations_ = 0;
  int64_t num_inspections_ = 0;
  int64_t num_minimization_ = 0;
  int64_t num_literals_removed_ = 0;
  int64_t num_redundant_implications_ = 0;
  int64_t num_redundant_literals_ = 0;

  // Bitset used by MinimizeClause().
  // TODO(user): use the same one as the one used in the classic minimization
  // because they are already initialized. Moreover they contains more
  // information.
  SparseBitset<LiteralIndex> is_marked_;
  SparseBitset<LiteralIndex> tmp_bitset_;
  SparseBitset<LiteralIndex> is_simplified_;

  // Temporary stack used by MinimizeClauseWithReachability().
  std::vector<Literal> dfs_stack_;

  // Used to limit the work done by ComputeTransitiveReduction() and
  // TransformIntoMaxCliques().
  int64_t work_done_in_mark_descendants_ = 0;
  std::vector<Literal> bfs_stack_;

  // For clique cuts.
  util_intops::StrongVector<LiteralIndex, int> tmp_mapping_;
  WeightedBronKerboschBitsetAlgorithm bron_kerbosch_;

  // Used by ComputeTransitiveReduction() in case we abort early to maintain
  // the invariant checked by InvariantsAreOk(). Some of our algo
  // relies on this to be always true.
  std::vector<std::pair<Literal, Literal>> tmp_removed_;

  // Filled by DetectEquivalences().
  bool is_dag_ = false;
  std::vector<LiteralIndex> reverse_topological_order_;
  Bitset64<LiteralIndex> is_redundant_;
  util_intops::StrongVector<LiteralIndex, LiteralIndex> representative_of_;

  // For in-processing and removing variables.
  std::vector<Literal> direct_implications_;
  std::vector<Literal> direct_implications_of_negated_literal_;
  util_intops::StrongVector<LiteralIndex, bool> in_direct_implications_;
  util_intops::StrongVector<LiteralIndex, bool> is_removed_;
  util_intops::StrongVector<LiteralIndex, int> estimated_sizes_;

  // For RemoveFixedVariables().
  int num_processed_fixed_variables_ = 0;

  bool enable_sharing_ = true;
  std::function<void(Literal, Literal)> add_binary_callback_ = nullptr;
};

extern template std::vector<Literal>
BinaryImplicationGraph::ExpandAtMostOneWithWeight<true>(
    const absl::Span<const Literal> at_most_one,
    const util_intops::StrongVector<LiteralIndex, bool>& can_be_included,
    const util_intops::StrongVector<LiteralIndex, double>& expanded_lp_values);

extern template std::vector<Literal>
BinaryImplicationGraph::ExpandAtMostOneWithWeight<false>(
    const absl::Span<const Literal> at_most_one,
    const util_intops::StrongVector<LiteralIndex, bool>& can_be_included,
    const util_intops::StrongVector<LiteralIndex, double>& expanded_lp_values);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CLAUSE_H_
