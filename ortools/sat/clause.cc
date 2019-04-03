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

#include "ortools/sat/clause.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ortools/base/logging.h"
#include "ortools/base/stl_util.h"
#include "ortools/graph/strongly_connected_components.h"

namespace operations_research {
namespace sat {

namespace {

// Returns true if the given watcher list contains the given clause.
template <typename Watcher>
bool WatcherListContains(const std::vector<Watcher>& list,
                         const SatClause& candidate) {
  for (const Watcher& watcher : list) {
    if (watcher.clause == &candidate) return true;
  }
  return false;
}

// A simple wrapper to simplify the erase(std::remove_if()) pattern.
template <typename Container, typename Predicate>
void RemoveIf(Container c, Predicate p) {
  c->erase(std::remove_if(c->begin(), c->end(), p), c->end());
}

}  // namespace

// ----- LiteralWatchers -----

LiteralWatchers::LiteralWatchers(Model* model)
    : SatPropagator("LiteralWatchers"),
      num_inspected_clauses_(0),
      num_inspected_clause_literals_(0),
      num_watched_clauses_(0),
      stats_("LiteralWatchers") {
  model->GetOrCreate<Trail>()->RegisterPropagator(this);
}

LiteralWatchers::~LiteralWatchers() {
  gtl::STLDeleteElements(&clauses_);
  IF_STATS_ENABLED(LOG(INFO) << stats_.StatString());
}

void LiteralWatchers::Resize(int num_variables) {
  DCHECK(is_clean_);
  watchers_on_false_.resize(num_variables << 1);
  reasons_.resize(num_variables);
  needs_cleaning_.Resize(LiteralIndex(num_variables << 1));
}

// Note that this is the only place where we add Watcher so the DCHECK
// guarantees that there are no duplicates.
void LiteralWatchers::AttachOnFalse(Literal literal, Literal blocking_literal,
                                    SatClause* clause) {
  SCOPED_TIME_STAT(&stats_);
  DCHECK(is_clean_);
  DCHECK(!WatcherListContains(watchers_on_false_[literal.Index()], *clause));
  watchers_on_false_[literal.Index()].push_back(
      Watcher(clause, blocking_literal));
}

bool LiteralWatchers::PropagateOnFalse(Literal false_literal, Trail* trail) {
  SCOPED_TIME_STAT(&stats_);
  DCHECK(is_clean_);
  std::vector<Watcher>& watchers = watchers_on_false_[false_literal.Index()];
  const VariablesAssignment& assignment = trail->Assignment();

  // Note(user): It sounds better to inspect the list in order, this is because
  // small clauses like binary or ternary clauses will often propagate and thus
  // stay at the beginning of the list.
  auto new_it = watchers.begin();
  const auto end = watchers.end();
  while (new_it != end && assignment.LiteralIsTrue(new_it->blocking_literal)) {
    ++new_it;
  }
  for (auto it = new_it; it != end; ++it) {
    // Don't even look at the clause memory if the blocking literal is true.
    if (assignment.LiteralIsTrue(it->blocking_literal)) {
      *new_it++ = *it;
      continue;
    }
    ++num_inspected_clauses_;

    // If the other watched literal is true, just change the blocking literal.
    // Note that we use the fact that the first two literals of the clause are
    // the ones currently watched.
    Literal* literals = it->clause->literals();
    const Literal other_watched_literal(
        LiteralIndex(literals[0].Index().value() ^ literals[1].Index().value() ^
                     false_literal.Index().value()));
    if (assignment.LiteralIsTrue(other_watched_literal)) {
      *new_it = *it;
      new_it->blocking_literal = other_watched_literal;
      ++new_it;
      ++num_inspected_clause_literals_;
      continue;
    }

    // Look for another literal to watch. We go through the list in a cyclic
    // fashion from start. The first two literals can be ignored as they are the
    // watched ones.
    {
      const int start = it->start_index;
      const int size = it->clause->Size();
      DCHECK_GE(start, 2);

      int i = start;
      while (i < size && assignment.LiteralIsFalse(literals[i])) ++i;
      num_inspected_clause_literals_ += i - start + 2;
      if (i >= size) {
        i = 2;
        while (i < start && assignment.LiteralIsFalse(literals[i])) ++i;
        num_inspected_clause_literals_ += i - 2;
        if (i >= start) i = size;
      }
      if (i < size) {
        // literal[i] is unassigned or true, it's now the new literal to watch.
        // Note that by convention, we always keep the two watched literals at
        // the beginning of the clause.
        literals[0] = other_watched_literal;
        literals[1] = literals[i];
        literals[i] = false_literal;
        watchers_on_false_[literals[1].Index()].emplace_back(
            it->clause, other_watched_literal, i + 1);
        continue;
      }
    }

    // At this point other_watched_literal is either false or unassigned, all
    // other literals are false.
    if (assignment.LiteralIsFalse(other_watched_literal)) {
      // Conflict: All literals of it->clause are false.
      //
      // Note(user): we could avoid a copy here, but the conflict analysis
      // complexity will be a lot higher than this anyway.
      trail->MutableConflict()->assign(it->clause->begin(), it->clause->end());
      trail->SetFailingSatClause(it->clause);
      num_inspected_clause_literals_ += it - watchers.begin() + 1;
      watchers.erase(new_it, it);
      return false;
    } else {
      // Propagation: other_watched_literal is unassigned, set it to true and
      // put it at position 0. Note that the position 0 is important because
      // we will need later to recover the literal that was propagated from the
      // clause using this convention.
      literals[0] = other_watched_literal;
      literals[1] = false_literal;
      reasons_[trail->Index()] = it->clause;
      trail->Enqueue(other_watched_literal, propagator_id_);
      *new_it++ = *it;
    }
  }
  num_inspected_clause_literals_ += watchers.size();  // The blocking ones.
  watchers.erase(new_it, end);
  return true;
}

bool LiteralWatchers::Propagate(Trail* trail) {
  const int old_index = trail->Index();
  while (trail->Index() == old_index && propagation_trail_index_ < old_index) {
    const Literal literal = (*trail)[propagation_trail_index_++];
    if (!PropagateOnFalse(literal.Negated(), trail)) return false;
  }
  return true;
}

absl::Span<const Literal> LiteralWatchers::Reason(const Trail& trail,
                                                  int trail_index) const {
  return reasons_[trail_index]->PropagationReason();
}

SatClause* LiteralWatchers::ReasonClause(int trail_index) const {
  return reasons_[trail_index];
}

bool LiteralWatchers::AddClause(absl::Span<const Literal> literals,
                                Trail* trail) {
  SatClause* clause = SatClause::Create(literals);
  clauses_.push_back(clause);
  return AttachAndPropagate(clause, trail);
}

SatClause* LiteralWatchers::AddRemovableClause(
    const std::vector<Literal>& literals, Trail* trail) {
  SatClause* clause = SatClause::Create(literals);
  clauses_.push_back(clause);
  CHECK(AttachAndPropagate(clause, trail));
  return clause;
}

// Sets up the 2-watchers data structure. It selects two non-false literals
// and attaches the clause to the event: one of the watched literals become
// false. It returns false if the clause only contains literals assigned to
// false. If only one literals is not false, it propagates it to true if it
// is not already assigned.
bool LiteralWatchers::AttachAndPropagate(SatClause* clause, Trail* trail) {
  SCOPED_TIME_STAT(&stats_);

  const int size = clause->Size();
  Literal* literals = clause->literals();

  // Select the first two literals that are not assigned to false and put them
  // on position 0 and 1.
  int num_literal_not_false = 0;
  for (int i = 0; i < size; ++i) {
    if (!trail->Assignment().LiteralIsFalse(literals[i])) {
      std::swap(literals[i], literals[num_literal_not_false]);
      ++num_literal_not_false;
      if (num_literal_not_false == 2) {
        break;
      }
    }
  }

  // Returns false if all the literals were false.
  // This should only happen on an UNSAT problem, and there is no need to attach
  // the clause in this case.
  if (num_literal_not_false == 0) return false;

  if (num_literal_not_false == 1) {
    // To maintain the validity of the 2-watcher algorithm, we need to watch
    // the false literal with the highest decision level.
    int max_level = trail->Info(literals[1].Variable()).level;
    for (int i = 2; i < size; ++i) {
      const int level = trail->Info(literals[i].Variable()).level;
      if (level > max_level) {
        max_level = level;
        std::swap(literals[1], literals[i]);
      }
    }

    // Propagates literals[0] if it is unassigned.
    if (!trail->Assignment().LiteralIsTrue(literals[0])) {
      reasons_[trail->Index()] = clause;
      trail->Enqueue(literals[0], propagator_id_);
    }
  }

  ++num_watched_clauses_;
  AttachOnFalse(literals[0], literals[1], clause);
  AttachOnFalse(literals[1], literals[0], clause);
  return true;
}

void LiteralWatchers::Attach(SatClause* clause, Trail* trail) {
  Literal* literals = clause->literals();
  CHECK(!trail->Assignment().LiteralIsAssigned(literals[0]));
  CHECK(!trail->Assignment().LiteralIsAssigned(literals[1]));

  ++num_watched_clauses_;
  AttachOnFalse(literals[0], literals[1], clause);
  AttachOnFalse(literals[1], literals[0], clause);
}

void LiteralWatchers::InternalDetach(SatClause* clause) {
  --num_watched_clauses_;
  const size_t size = clause->Size();
  if (drat_proof_handler_ != nullptr && size > 2) {
    drat_proof_handler_->DeleteClause({clause->begin(), size});
  }
  clauses_info_.erase(clause);
  clause->LazyDetach();
}

void LiteralWatchers::LazyDetach(SatClause* clause) {
  InternalDetach(clause);
  is_clean_ = false;
  needs_cleaning_.Set(clause->FirstLiteral().Index());
  needs_cleaning_.Set(clause->SecondLiteral().Index());
}

void LiteralWatchers::Detach(SatClause* clause) {
  InternalDetach(clause);
  for (const Literal l : {clause->FirstLiteral(), clause->SecondLiteral()}) {
    needs_cleaning_.Clear(l.Index());
    RemoveIf(&(watchers_on_false_[l.Index()]), [](const Watcher& watcher) {
      return !watcher.clause->IsAttached();
    });
  }
}

void LiteralWatchers::CleanUpWatchers() {
  SCOPED_TIME_STAT(&stats_);
  for (LiteralIndex index : needs_cleaning_.PositionsSetAtLeastOnce()) {
    DCHECK(needs_cleaning_[index]);
    RemoveIf(&(watchers_on_false_[index]), [](const Watcher& watcher) {
      return !watcher.clause->IsAttached();
    });
    needs_cleaning_.Clear(index);
  }
  needs_cleaning_.NotifyAllClear();
  is_clean_ = true;
}

void LiteralWatchers::DeleteDetachedClauses() {
  DCHECK(is_clean_);

  // Update to_minimize_index_.
  if (to_minimize_index_ >= clauses_.size()) {
    to_minimize_index_ = clauses_.size();
  }
  to_minimize_index_ =
      std::stable_partition(clauses_.begin(),
                            clauses_.begin() + to_minimize_index_,
                            [](SatClause* a) { return a->IsAttached(); }) -
      clauses_.begin();

  // Do the proper deletion.
  std::vector<SatClause*>::iterator iter =
      std::stable_partition(clauses_.begin(), clauses_.end(),
                            [](SatClause* a) { return a->IsAttached(); });
  gtl::STLDeleteContainerPointers(iter, clauses_.end());
  clauses_.erase(iter, clauses_.end());
}

// ----- BinaryImplicationGraph -----

void BinaryImplicationGraph::Resize(int num_variables) {
  SCOPED_TIME_STAT(&stats_);
  implications_.resize(num_variables << 1);
  reasons_.resize(num_variables);
}

void BinaryImplicationGraph::AddBinaryClause(Literal a, Literal b) {
  SCOPED_TIME_STAT(&stats_);
  implications_[a.NegatedIndex()].push_back(b);
  implications_[b.NegatedIndex()].push_back(a);
  is_dag_ = false;
  num_implications_ += 2;
}

void BinaryImplicationGraph::AddBinaryClauseDuringSearch(Literal a, Literal b,
                                                         Trail* trail) {
  SCOPED_TIME_STAT(&stats_);
  if (num_implications_ == 0) propagation_trail_index_ = trail->Index();
  AddBinaryClause(a, b);
  if (trail->Assignment().LiteralIsFalse(a)) {
    DCHECK_EQ(trail->CurrentDecisionLevel(), trail->Info(a.Variable()).level);
    reasons_[trail->Index()] = a;
    trail->Enqueue(b, propagator_id_);
  } else if (trail->Assignment().LiteralIsFalse(b)) {
    DCHECK_EQ(trail->CurrentDecisionLevel(), trail->Info(b.Variable()).level);
    reasons_[trail->Index()] = b;
    trail->Enqueue(a, propagator_id_);
  }
}

void BinaryImplicationGraph::AddAtMostOne(
    absl::Span<const Literal> at_most_one) {
  if (at_most_one.empty()) return;
  for (const Literal a : at_most_one) {
    for (const Literal b : at_most_one) {
      if (a == b) continue;
      implications_[a.Index()].push_back(b.Negated());
    }
  }
  is_dag_ = false;
  num_implications_ += at_most_one.size() * (at_most_one.size() - 1);
}

bool BinaryImplicationGraph::PropagateOnTrue(Literal true_literal,
                                             Trail* trail) {
  SCOPED_TIME_STAT(&stats_);

  // Note(user): This update is not exactly correct because in case of conflict
  // we don't inspect that much clauses. But doing ++num_inspections_ inside the
  // loop does slow down the code by a few percent.
  num_inspections_ += implications_[true_literal.Index()].size();

  const VariablesAssignment& assignment = trail->Assignment();
  for (Literal literal : implications_[true_literal.Index()]) {
    if (assignment.LiteralIsTrue(literal)) {
      // Note(user): I tried to update the reason here if the literal was
      // enqueued after the true_literal on the trail. This property is
      // important for ComputeFirstUIPConflict() to work since it needs the
      // trail order to be a topological order for the deduction graph.
      // But the performance was not too good...
      continue;
    }

    ++num_propagations_;
    if (assignment.LiteralIsFalse(literal)) {
      // Conflict.
      *(trail->MutableConflict()) = {true_literal.Negated(), literal};
      return false;
    } else {
      // Propagation.
      reasons_[trail->Index()] = true_literal.Negated();
      trail->Enqueue(literal, propagator_id_);
    }
  }
  return true;
}

bool BinaryImplicationGraph::Propagate(Trail* trail) {
  if (num_implications_ == 0) {
    propagation_trail_index_ = trail->Index();
    return true;
  }
  while (propagation_trail_index_ < trail->Index()) {
    const Literal literal = (*trail)[propagation_trail_index_++];
    if (!PropagateOnTrue(literal, trail)) return false;
  }
  return true;
}

absl::Span<const Literal> BinaryImplicationGraph::Reason(
    const Trail& trail, int trail_index) const {
  return {&reasons_[trail_index], 1};
}

// Here, we remove all the literal whose negation are implied by the negation of
// the 1-UIP literal (which always appear first in the given conflict). Note
// that this algorithm is "optimal" in the sense that it leads to a minimized
// conflict with a backjump level as low as possible. However, not all possible
// literals are removed.
void BinaryImplicationGraph::MinimizeConflictWithReachability(
    std::vector<Literal>* conflict) {
  SCOPED_TIME_STAT(&stats_);
  dfs_stack_.clear();

  // Compute the reachability from the literal "not(conflict->front())" using
  // an iterative dfs.
  const LiteralIndex root_literal_index = conflict->front().NegatedIndex();
  is_marked_.ClearAndResize(LiteralIndex(implications_.size()));
  is_marked_.Set(root_literal_index);

  // TODO(user): This sounds like a good idea, but somehow it seems better not
  // to do that even though it is almost for free. Investigate more.
  //
  // The idea here is that since we already compute the reachability from the
  // root literal, we can use this computation to remove any implication
  // root_literal => b if there is already root_literal => a and b is reachable
  // from a.
  const bool also_prune_direct_implication_list = false;

  // We treat the direct implications differently so we can also remove the
  // redundant implications from this list at the same time.
  auto& direct_implications = implications_[root_literal_index];
  for (const Literal l : direct_implications) {
    if (is_marked_[l.Index()]) continue;
    dfs_stack_.push_back(l);
    while (!dfs_stack_.empty()) {
      const LiteralIndex index = dfs_stack_.back().Index();
      dfs_stack_.pop_back();
      if (!is_marked_[index]) {
        is_marked_.Set(index);
        for (Literal implied : implications_[index]) {
          if (!is_marked_[implied.Index()]) dfs_stack_.push_back(implied);
        }
      }
    }

    // The "trick" is to unmark 'l'. This way, if we explore it twice, it means
    // that this l is reachable from some other 'l' from the direct implication
    // list. Remarks:
    //  - We don't loose too much complexity when this happen since a literal
    //    can be unmarked only once, so in the worst case we loop twice over its
    //    children. Moreover, this literal will be pruned for later calls.
    //  - This is correct, i.e. we can't prune too many literals because of a
    //    strongly connected component. Proof by contradiction: If we take the
    //    first (in direct_implications) literal from a removed SCC, it must
    //    have marked all the others. But because they are marked, they will not
    //    be explored again and so can't mark the first literal.
    if (also_prune_direct_implication_list) {
      is_marked_.Clear(l.Index());
    }
  }

  // Now we can prune the direct implications list and make sure are the
  // literals there are marked.
  if (also_prune_direct_implication_list) {
    int new_size = 0;
    for (const Literal l : direct_implications) {
      if (!is_marked_[l.Index()]) {
        is_marked_.Set(l.Index());
        direct_implications[new_size] = l;
        ++new_size;
      }
    }
    if (new_size < direct_implications.size()) {
      num_redundant_implications_ += direct_implications.size() - new_size;
      direct_implications.resize(new_size);
    }
  }

  RemoveRedundantLiterals(conflict);
}

// Same as MinimizeConflictWithReachability() but also mark (in the given
// SparseBitset) the reachable literal already assigned to false. These literals
// will be implied if the 1-UIP literal is assigned to false, and the classic
// minimization algorithm can take advantage of that.
void BinaryImplicationGraph::MinimizeConflictFirst(
    const Trail& trail, std::vector<Literal>* conflict,
    SparseBitset<BooleanVariable>* marked) {
  SCOPED_TIME_STAT(&stats_);
  is_marked_.ClearAndResize(LiteralIndex(implications_.size()));
  dfs_stack_.clear();
  dfs_stack_.push_back(conflict->front().Negated());
  while (!dfs_stack_.empty()) {
    const Literal literal = dfs_stack_.back();
    dfs_stack_.pop_back();
    if (!is_marked_[literal.Index()]) {
      is_marked_.Set(literal.Index());
      // If the literal is assigned to false, we mark it.
      if (trail.Assignment().LiteralIsFalse(literal)) {
        marked->Set(literal.Variable());
      }
      for (Literal implied : implications_[literal.Index()]) {
        if (!is_marked_[implied.Index()]) dfs_stack_.push_back(implied);
      }
    }
  }
  RemoveRedundantLiterals(conflict);
}

// Same as MinimizeConflictFirst() but take advantage of this reachability
// computation to remove redundant implication in the implication list of the
// first UIP conflict.
void BinaryImplicationGraph::MinimizeConflictFirstWithTransitiveReduction(
    const Trail& trail, std::vector<Literal>* conflict,
    SparseBitset<BooleanVariable>* marked, random_engine_t* random) {
  SCOPED_TIME_STAT(&stats_);
  const LiteralIndex root_literal_index = conflict->front().NegatedIndex();
  is_marked_.ClearAndResize(LiteralIndex(implications_.size()));
  is_marked_.Set(root_literal_index);

  int new_size = 0;
  auto& direct_implications = implications_[root_literal_index];

  // The randomization allow to find more redundant implication since to find
  // a => b and remove b, a must be before b in direct_implications. Note that
  // a std::reverse() could work too. But randomization seems to work better.
  // Probably because it has other impact on the search tree.
  std::shuffle(direct_implications.begin(), direct_implications.end(), *random);
  dfs_stack_.clear();
  for (const Literal l : direct_implications) {
    if (is_marked_[l.Index()]) {
      // The literal is already marked! so it must be implied by one of the
      // previous literal in the direct_implications list. We can safely remove
      // it.
      continue;
    }
    direct_implications[new_size++] = l;
    dfs_stack_.push_back(l);
    while (!dfs_stack_.empty()) {
      const LiteralIndex index = dfs_stack_.back().Index();
      dfs_stack_.pop_back();
      if (!is_marked_[index]) {
        is_marked_.Set(index);
        for (Literal implied : implications_[index]) {
          if (!is_marked_[implied.Index()]) dfs_stack_.push_back(implied);
        }
      }
    }
  }
  if (new_size < direct_implications.size()) {
    num_redundant_implications_ += direct_implications.size() - new_size;
    direct_implications.resize(new_size);
  }
  RemoveRedundantLiterals(conflict);
}

void BinaryImplicationGraph::RemoveRedundantLiterals(
    std::vector<Literal>* conflict) {
  SCOPED_TIME_STAT(&stats_);
  int new_index = 1;
  for (int i = 1; i < conflict->size(); ++i) {
    if (!is_marked_[(*conflict)[i].NegatedIndex()]) {
      (*conflict)[new_index] = (*conflict)[i];
      ++new_index;
    }
  }
  if (new_index < conflict->size()) {
    ++num_minimization_;
    num_literals_removed_ += conflict->size() - new_index;
    conflict->resize(new_index);
  }
}

void BinaryImplicationGraph::MinimizeConflictExperimental(
    const Trail& trail, std::vector<Literal>* conflict) {
  SCOPED_TIME_STAT(&stats_);
  is_marked_.ClearAndResize(LiteralIndex(implications_.size()));
  is_removed_.ClearAndResize(LiteralIndex(implications_.size()));
  for (Literal lit : *conflict) {
    is_marked_.Set(lit.Index());
  }

  // Identify and remove the redundant literals from the given conflict.
  // 1/ If a -> b then a can be removed from the conflict clause.
  //    This is because not b -> not a.
  // 2/ a -> b can only happen if level(a) <= level(b).
  // 3/ Because of 2/, cycles can appear only at the same level.
  //    The vector is_removed_ is used to avoid removing all elements of a
  //    cycle. Note that this is not optimal in the sense that we may not remove
  //    a literal that can be removed.
  //
  // Note that there is no need to explore the unique literal of the highest
  // decision level since it can't be removed. Because this is a conflict, such
  // literal is always at position 0, so we start directly at 1.
  int index = 1;
  for (int i = 1; i < conflict->size(); ++i) {
    const Literal lit = (*conflict)[i];
    const int lit_level = trail.Info(lit.Variable()).level;
    bool keep_literal = true;
    for (Literal implied : implications_[lit.Index()]) {
      if (is_marked_[implied.Index()]) {
        DCHECK_LE(lit_level, trail.Info(implied.Variable()).level);
        if (lit_level == trail.Info(implied.Variable()).level &&
            is_removed_[implied.Index()]) {
          continue;
        }
        keep_literal = false;
        break;
      }
    }
    if (keep_literal) {
      (*conflict)[index] = lit;
      ++index;
    } else {
      is_removed_.Set(lit.Index());
    }
  }
  if (index < conflict->size()) {
    ++num_minimization_;
    num_literals_removed_ += conflict->size() - index;
    conflict->erase(conflict->begin() + index, conflict->end());
  }
}

void BinaryImplicationGraph::RemoveFixedVariables(
    int first_unprocessed_trail_index, const Trail& trail) {
  const VariablesAssignment& assignment = trail.Assignment();
  SCOPED_TIME_STAT(&stats_);
  is_marked_.ClearAndResize(LiteralIndex(implications_.size()));
  for (int i = first_unprocessed_trail_index; i < trail.Index(); ++i) {
    const Literal true_literal = trail[i];
    // If b is true and a -> b then because not b -> not a, all the
    // implications list that contains b will be marked by this process.
    //
    // TODO(user): This doesn't seems true if we remove implication by
    // transitive reduction.
    for (Literal lit : implications_[true_literal.NegatedIndex()]) {
      is_marked_.Set(lit.NegatedIndex());
    }
    gtl::STLClearObject(&(implications_[true_literal.Index()]));
    gtl::STLClearObject(&(implications_[true_literal.NegatedIndex()]));
  }
  for (const LiteralIndex i : is_marked_.PositionsSetAtLeastOnce()) {
    RemoveIf(&implications_[i], [&assignment](const Literal& lit) {
      return assignment.LiteralIsTrue(lit);
    });
  }
}

void BinaryImplicationGraph::RemoveDuplicates() {
  int64 num_removed = 0;
  for (auto& list : implications_) {
    const int old_size = list.size();
    gtl::STLSortAndRemoveDuplicates(&list);
    num_removed += old_size - list.size();
  }
  num_implications_ -= num_removed;
  if (num_removed > 0) {
    VLOG(1) << "Removed " << num_removed << " duplicate implications. "
            << num_implications_ << " implications left.";
  }
}

class SccWrapper {
 public:
  using Graph = gtl::ITIVector<LiteralIndex, absl::InlinedVector<Literal, 6>>;

  explicit SccWrapper(Graph* graph) : graph_(*graph) {}
  std::vector<int32> operator[](int32 node) const {
    tmp_.clear();
    for (const Literal l : graph_[LiteralIndex(node)]) {
      tmp_.push_back(l.Index().value());
    }
    return tmp_;
  }

 private:
  mutable std::vector<int32> tmp_;
  const Graph& graph_;
};

bool BinaryImplicationGraph::DetectEquivalences() {
  const int32 size(implications_.size());
  is_redundant_.resize(size, false);

  std::vector<std::vector<int32>> scc;
  FindStronglyConnectedComponents(size, SccWrapper(&implications_), &scc);

  int num_equivalences = 0;
  reverse_topological_order_.clear();
  representative_of_.assign(size, kNoLiteralIndex);
  for (std::vector<int32>& component : scc) {
    // We always take the smallest literal index (which also corresponds to the
    // smallest BooleanVariable index) as a representative. This make sure that
    // the representative of a literal l and the one of not(l) will be the
    // negation of each other. There is also reason to think that it is
    // heuristically better to use a BooleanVariable that was created first.
    std::sort(component.begin(), component.end());
    const LiteralIndex representative(component[0]);

    reverse_topological_order_.push_back(representative);
    if (component.size() == 1) {
      continue;
    }

    auto& representative_list = implications_[representative];
    for (int i = 1; i < component.size(); ++i) {
      const Literal literal = Literal(LiteralIndex(component[i]));
      is_redundant_[literal.Index()] = true;
      representative_of_[literal.Index()] = representative;

      // Detect if x <=> not(x) which means unsat. Note that we relly on the
      // fact that when sorted, they will both be consecutive in the list.
      if (Literal(LiteralIndex(component[i - 1])).Negated() == literal) {
        VLOG(1) << "Trivially UNSAT in DetectEquivalences()";
        return false;
      }

      // Merge all the lists in implications_[representative].
      // Note that we do not want representative in its own list.
      auto& ref = implications_[literal.Index()];
      for (const Literal l : ref) {
        if (l.Index() != representative) representative_list.push_back(l);
      }

      // Add representative <=> literal.
      representative_list.push_back(literal);
      ref.clear();
      ref.push_back(Literal(representative));
    }
    num_equivalences += component.size() - 1;
    gtl::STLSortAndRemoveDuplicates(&representative_list);
  }
  is_dag_ = true;
  if (num_equivalences == 0) return true;

  // Remap all the implications to only use representative.
  // Note that this also does the job of RemoveDuplicates().
  num_implications_ = 0;
  for (LiteralIndex i(0); i < size; ++i) {
    if (is_redundant_[i]) {
      num_implications_ += implications_[i].size();
      continue;
    }
    for (Literal& ref : implications_[i]) {
      const LiteralIndex rep = representative_of_[ref.Index()];
      if (rep == i) continue;
      if (rep == kNoLiteralIndex) continue;
      ref = Literal(rep);
    }
    gtl::STLSortAndRemoveDuplicates(&implications_[i]);
    num_implications_ += implications_[i].size();
  }

  VLOG(1) << num_equivalences << " redundant equivalent literals. "
          << num_implications_ << " implications left. " << implications_.size()
          << " literals.";
  return true;
}

bool BinaryImplicationGraph::ComputeTransitiveReduction() {
  if (!DetectEquivalences()) return false;
  work_done_in_mark_descendants_ = 0;

  // For each node we do a graph traversal and only keep the literals
  // at maximum distance 1. This only works because we have a DAG when ignoring
  // the "redundant" literal marked by DetectEquivalences().
  const LiteralIndex size(implications_.size());
  for (const LiteralIndex i : reverse_topological_order_) {
    CHECK(!is_redundant_[i]);
    auto& direct_implications = implications_[i];

    is_marked_.ClearAndResize(size);
    for (const Literal root : direct_implications) {
      if (is_redundant_[root.Index()]) continue;
      if (is_marked_[root.Index()]) continue;

      MarkDescendants(root);

      // We have a DAG, so root could only be marked first.
      is_marked_.Clear(root.Index());
    }

    // Only keep the non-marked literal (and the redundant one which are never
    // marked).
    int new_size = 0;
    for (const Literal l : direct_implications) {
      if (!is_marked_[l.Index()]) {
        direct_implications[new_size++] = l;
      }
    }
    const int diff = direct_implications.size() - new_size;
    direct_implications.resize(new_size);
    direct_implications.shrink_to_fit();
    num_redundant_implications_ += diff;
    num_implications_ -= diff;

    // Abort if the computation involved is too big.
    if (work_done_in_mark_descendants_ > 1e8) break;
  }

  if (num_redundant_implications_ > 0) {
    VLOG(1) << "Transitive reduction removed " << num_redundant_implications_
            << " literals. " << num_implications_ << " implications left. "
            << implications_.size() << " literals."
            << (work_done_in_mark_descendants_ > 1e8 ? " Aborted." : "");
  }
  return true;
}

namespace {

bool IntersectionIsEmpty(const std::vector<int>& a, const std::vector<int>& b) {
  DCHECK(std::is_sorted(a.begin(), a.end()));
  DCHECK(std::is_sorted(b.begin(), b.end()));
  int i = 0;
  int j = 0;
  for (; i < a.size() && j < b.size();) {
    if (a[i] == b[j]) return false;
    if (a[i] < b[j]) {
      ++i;
    } else {
      ++j;
    }
  }
  return true;
}

// Used by TransformIntoMaxCliques().
struct VectorHash {
  std::size_t operator()(const std::vector<Literal>& at_most_one) const {
    size_t hash = 0;
    for (Literal literal : at_most_one) {
      hash = util_hash::Hash(literal.Index().value(), hash);
    }
    return hash;
  }
};

}  // namespace

void BinaryImplicationGraph::TransformIntoMaxCliques(
    std::vector<std::vector<Literal>>* at_most_ones) {
  // The code below assumes a DAG.
  if (!is_dag_) DetectEquivalences();
  work_done_in_mark_descendants_ = 0;

  int num_extended = 0;
  int num_removed = 0;
  int num_added = 0;

  absl::flat_hash_set<std::vector<Literal>, VectorHash> max_cliques;
  gtl::ITIVector<LiteralIndex, std::vector<int>> max_cliques_containing(
      implications_.size());

  // We starts by processing larger constraints first.
  std::sort(at_most_ones->begin(), at_most_ones->end(),
            [](const std::vector<Literal> a, const std::vector<Literal> b) {
              return a.size() > b.size();
            });
  for (std::vector<Literal>& clique : *at_most_ones) {
    const int old_size = clique.size();

    // Remap the clique to only use representative.
    //
    // Note(user): Because we always use literal with the smallest variable
    // indices as representative, this make sure that if possible, we express
    // the clique in term of user provided variable (that are always created
    // first).
    for (Literal& ref : clique) {
      const LiteralIndex rep = representative_of_[ref.Index()];
      if (rep == kNoLiteralIndex) continue;
      ref = Literal(rep);
    }

    // Special case for clique of size 2, we don't expand them if they
    // are included in an already added clique.
    //
    // TODO(user): the second condition means the literal must be false!
    if (old_size == 2 && clique[0] != clique[1]) {
      if (!IntersectionIsEmpty(max_cliques_containing[clique[0].Index()],
                               max_cliques_containing[clique[1].Index()])) {
        ++num_removed;
        clique.clear();
        continue;
      }
    }

    // We only expand the clique as long as we didn't spend too much time.
    if (work_done_in_mark_descendants_ < 1e8) {
      clique = ExpandAtMostOne(clique);
    }
    std::sort(clique.begin(), clique.end());
    if (!gtl::InsertIfNotPresent(&max_cliques, clique)) {
      ++num_removed;
      clique.clear();
      continue;
    }

    const int clique_index = max_cliques.size();
    for (const Literal l : clique) {
      max_cliques_containing[l.Index()].push_back(clique_index);
    }
    if (clique.size() > old_size) ++num_extended;
    ++num_added;
  }

  if (num_extended > 0 || num_removed > 0 || num_added > 0) {
    VLOG(1) << "Clique Extended: " << num_extended
            << " Removed: " << num_removed << " Added: " << num_added
            << (work_done_in_mark_descendants_ > 1e8 ? " (Aborted)" : "");
  }
}

// We use dfs_stack_ but we actually do a BFS.
void BinaryImplicationGraph::MarkDescendants(Literal root) {
  dfs_stack_ = {root};
  is_marked_.Set(root.Index());
  if (is_redundant_[root.Index()]) return;
  for (int j = 0; j < dfs_stack_.size(); ++j) {
    const Literal current = dfs_stack_[j];
    for (const Literal l : implications_[current.Index()]) {
      if (!is_marked_[l.Index()] && !is_redundant_[l.Index()]) {
        dfs_stack_.push_back(l);
        is_marked_.Set(l.Index());
      }
    }
  }
  work_done_in_mark_descendants_ += dfs_stack_.size();
}

std::vector<Literal> BinaryImplicationGraph::ExpandAtMostOne(
    const absl::Span<const Literal> at_most_one) {
  std::vector<Literal> clique(at_most_one.begin(), at_most_one.end());

  // Optim.
  for (int i = 0; i < clique.size(); ++i) {
    if (implications_[clique[i].Index()].empty() ||
        is_redundant_[clique[i].Index()]) {
      return clique;
    }
  }

  std::vector<LiteralIndex> intersection;
  for (int i = 0; i < clique.size(); ++i) {
    is_marked_.ClearAndResize(LiteralIndex(implications_.size()));
    MarkDescendants(clique[i]);
    if (i == 0) {
      intersection = is_marked_.PositionsSetAtLeastOnce();
      for (const Literal l : clique) is_marked_.Clear(l.NegatedIndex());
    }

    int new_size = 0;
    is_marked_.Clear(clique[i].NegatedIndex());  // TODO(user): explain.
    for (const LiteralIndex index : intersection) {
      if (is_marked_[index]) intersection[new_size++] = index;
    }
    intersection.resize(new_size);
    if (intersection.empty()) break;

    // Expand?
    if (i + 1 == clique.size()) {
      clique.push_back(Literal(intersection.back()).Negated());
      intersection.pop_back();
    }
  }
  return clique;
}

// ----- SatClause -----

// static
SatClause* SatClause::Create(absl::Span<const Literal> literals) {
  CHECK_GE(literals.size(), 2);
  SatClause* clause = reinterpret_cast<SatClause*>(
      ::operator new(sizeof(SatClause) + literals.size() * sizeof(Literal)));
  clause->size_ = literals.size();
  for (int i = 0; i < literals.size(); ++i) {
    clause->literals_[i] = literals[i];
  }
  return clause;
}

// Note that for an attached clause, removing fixed literal is okay because if
// any of the watched literal is assigned, then the clause is necessarily true.
bool SatClause::RemoveFixedLiteralsAndTestIfTrue(
    const VariablesAssignment& assignment) {
  DCHECK(IsAttached());
  if (assignment.VariableIsAssigned(literals_[0].Variable()) ||
      assignment.VariableIsAssigned(literals_[1].Variable())) {
    DCHECK(IsSatisfied(assignment));
    return true;
  }
  int j = 2;
  while (j < size_ && !assignment.VariableIsAssigned(literals_[j].Variable())) {
    ++j;
  }
  for (int i = j; i < size_; ++i) {
    if (assignment.VariableIsAssigned(literals_[i].Variable())) {
      if (assignment.LiteralIsTrue(literals_[i])) return true;
    } else {
      std::swap(literals_[j], literals_[i]);
      ++j;
    }
  }
  size_ = j;
  return false;
}

bool SatClause::IsSatisfied(const VariablesAssignment& assignment) const {
  for (const Literal literal : *this) {
    if (assignment.LiteralIsTrue(literal)) return true;
  }
  return false;
}

std::string SatClause::DebugString() const {
  std::string result;
  for (const Literal literal : *this) {
    if (!result.empty()) result.append(" ");
    result.append(literal.DebugString());
  }
  return result;
}

}  // namespace sat
}  // namespace operations_research
