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

#include "ortools/sat/clause.h"

#include <stddef.h>

#include <algorithm>
#include <cstdint>
#include <deque>
#include <new>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/inlined_vector.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/distributions.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/strong_vector.h"
#include "ortools/base/timer.h"
#include "ortools/graph/strongly_connected_components.h"
#include "ortools/sat/drat_proof_handler.h"
#include "ortools/sat/inclusion.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/util.h"
#include "ortools/util/bitset.h"
#include "ortools/util/stats.h"
#include "ortools/util/strong_integers.h"
#include "ortools/util/time_limit.h"
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

// ----- ClauseManager -----

ClauseManager::ClauseManager(Model* model)
    : SatPropagator("ClauseManager"),
      implication_graph_(model->GetOrCreate<BinaryImplicationGraph>()),
      trail_(model->GetOrCreate<Trail>()),
      num_inspected_clauses_(0),
      num_inspected_clause_literals_(0),
      num_watched_clauses_(0),
      stats_("ClauseManager") {
  trail_->RegisterPropagator(this);
}

ClauseManager::~ClauseManager() {
  gtl::STLDeleteElements(&clauses_);
  IF_STATS_ENABLED(LOG(INFO) << stats_.StatString());
}

void ClauseManager::Resize(int num_variables) {
  DCHECK(is_clean_);
  watchers_on_false_.resize(num_variables << 1);
  reasons_.resize(num_variables);
  needs_cleaning_.Resize(LiteralIndex(num_variables << 1));
}

// Note that this is the only place where we add Watcher so the DCHECK
// guarantees that there are no duplicates.
void ClauseManager::AttachOnFalse(Literal literal, Literal blocking_literal,
                                  SatClause* clause) {
  SCOPED_TIME_STAT(&stats_);
  DCHECK(is_clean_);
  DCHECK(!WatcherListContains(watchers_on_false_[literal], *clause));
  watchers_on_false_[literal].push_back(Watcher(clause, blocking_literal));
}

bool ClauseManager::PropagateOnFalse(Literal false_literal, Trail* trail) {
  SCOPED_TIME_STAT(&stats_);
  DCHECK(is_clean_);
  std::vector<Watcher>& watchers = watchers_on_false_[false_literal];
  const auto assignment = AssignmentView(trail->Assignment());

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
      const int size = it->clause->size();
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
        watchers_on_false_[literals[1]].emplace_back(
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

bool ClauseManager::Propagate(Trail* trail) {
  const int old_index = trail->Index();
  while (trail->Index() == old_index && propagation_trail_index_ < old_index) {
    const Literal literal = (*trail)[propagation_trail_index_++];
    if (!PropagateOnFalse(literal.Negated(), trail)) return false;
  }
  return true;
}

absl::Span<const Literal> ClauseManager::Reason(const Trail& /*trail*/,
                                                int trail_index,
                                                int64_t /*conflict_id*/) const {
  return reasons_[trail_index]->PropagationReason();
}

SatClause* ClauseManager::ReasonClause(int trail_index) const {
  return reasons_[trail_index];
}

bool ClauseManager::AddClause(absl::Span<const Literal> literals) {
  return AddClause(literals, trail_, -1);
}

bool ClauseManager::AddClause(absl::Span<const Literal> literals, Trail* trail,
                              int lbd) {
  SatClause* clause = SatClause::Create(literals);
  clauses_.push_back(clause);
  if (add_clause_callback_ != nullptr) add_clause_callback_(lbd, literals);
  return AttachAndPropagate(clause, trail);
}

SatClause* ClauseManager::AddRemovableClause(absl::Span<const Literal> literals,
                                             Trail* trail, int lbd) {
  SatClause* clause = SatClause::Create(literals);
  clauses_.push_back(clause);
  if (add_clause_callback_ != nullptr) add_clause_callback_(lbd, literals);
  CHECK(AttachAndPropagate(clause, trail));
  return clause;
}

// Sets up the 2-watchers data structure. It selects two non-false literals
// and attaches the clause to the event: one of the watched literals become
// false. It returns false if the clause only contains literals assigned to
// false. If only one literals is not false, it propagates it to true if it
// is not already assigned.
bool ClauseManager::AttachAndPropagate(SatClause* clause, Trail* trail) {
  SCOPED_TIME_STAT(&stats_);

  const int size = clause->size();
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

void ClauseManager::Attach(SatClause* clause, Trail* trail) {
  Literal* literals = clause->literals();
  DCHECK(!trail->Assignment().LiteralIsAssigned(literals[0]));
  DCHECK(!trail->Assignment().LiteralIsAssigned(literals[1]));

  ++num_watched_clauses_;
  AttachOnFalse(literals[0], literals[1], clause);
  AttachOnFalse(literals[1], literals[0], clause);
}

void ClauseManager::InternalDetach(SatClause* clause) {
  --num_watched_clauses_;
  const size_t size = clause->size();
  if (drat_proof_handler_ != nullptr && size > 2) {
    drat_proof_handler_->DeleteClause({clause->begin(), size});
  }
  clauses_info_.erase(clause);
  clause->Clear();
}

void ClauseManager::LazyDetach(SatClause* clause) {
  InternalDetach(clause);
  is_clean_ = false;
  needs_cleaning_.Set(clause->FirstLiteral());
  needs_cleaning_.Set(clause->SecondLiteral());
}

void ClauseManager::Detach(SatClause* clause) {
  InternalDetach(clause);
  for (const Literal l : {clause->FirstLiteral(), clause->SecondLiteral()}) {
    needs_cleaning_.Clear(l);
    RemoveIf(&(watchers_on_false_[l]), [](const Watcher& watcher) {
      return watcher.clause->IsRemoved();
    });
  }
}

void ClauseManager::DetachAllClauses() {
  if (!all_clauses_are_attached_) return;
  all_clauses_are_attached_ = false;

  // This is easy, and this allows to reset memory if some watcher lists where
  // really long at some point.
  is_clean_ = true;
  num_watched_clauses_ = 0;
  watchers_on_false_.clear();
}

void ClauseManager::AttachAllClauses() {
  if (all_clauses_are_attached_) return;
  all_clauses_are_attached_ = true;

  needs_cleaning_.ClearAll();  // This doesn't resize it.
  watchers_on_false_.resize(needs_cleaning_.size().value());

  DeleteRemovedClauses();
  for (SatClause* clause : clauses_) {
    ++num_watched_clauses_;
    DCHECK_GE(clause->size(), 2);
    AttachOnFalse(clause->FirstLiteral(), clause->SecondLiteral(), clause);
    AttachOnFalse(clause->SecondLiteral(), clause->FirstLiteral(), clause);
  }
}

// This one do not need the clause to be detached.
bool ClauseManager::InprocessingFixLiteral(Literal true_literal) {
  DCHECK_EQ(trail_->CurrentDecisionLevel(), 0);
  if (drat_proof_handler_ != nullptr) {
    drat_proof_handler_->AddClause({true_literal});
  }
  // TODO(user): remove the test when the DRAT issue with fixed literal is
  // resolved.
  if (!trail_->Assignment().LiteralIsTrue(true_literal)) {
    trail_->EnqueueWithUnitReason(true_literal);

    // Even when all clauses are detached, we can propagate the implication
    // graph and we do that right away.
    return implication_graph_->Propagate(trail_);
  }
  return true;
}

// TODO(user): We could do something slower if the clauses are attached like
// we do for InprocessingRewriteClause().
void ClauseManager::InprocessingRemoveClause(SatClause* clause) {
  DCHECK(!all_clauses_are_attached_);
  if (drat_proof_handler_ != nullptr) {
    drat_proof_handler_->DeleteClause(clause->AsSpan());
  }
  clauses_info_.erase(clause);
  clause->Clear();
}

bool ClauseManager::InprocessingRewriteClause(
    SatClause* clause, absl::Span<const Literal> new_clause) {
  if (new_clause.empty()) return false;  // UNSAT.

  if (DEBUG_MODE) {
    for (const Literal l : new_clause) {
      DCHECK(!trail_->Assignment().LiteralIsAssigned(l));
    }
  }

  if (new_clause.size() == 1) {
    if (!InprocessingFixLiteral(new_clause[0])) return false;
    InprocessingRemoveClause(clause);
    return true;
  }

  if (new_clause.size() == 2) {
    CHECK(implication_graph_->AddBinaryClause(new_clause[0], new_clause[1]));
    InprocessingRemoveClause(clause);
    return true;
  }

  if (drat_proof_handler_ != nullptr) {
    // We must write the new clause before we delete the old one.
    drat_proof_handler_->AddClause(new_clause);
    drat_proof_handler_->DeleteClause(clause->AsSpan());
  }

  if (all_clauses_are_attached_) {
    // We can still rewrite the clause, but it is inefficient. We first
    // detach it in a non-lazy way.
    --num_watched_clauses_;
    clause->Clear();
    for (const Literal l : {clause->FirstLiteral(), clause->SecondLiteral()}) {
      needs_cleaning_.Clear(l);
      RemoveIf(&(watchers_on_false_[l]), [](const Watcher& watcher) {
        return watcher.clause->IsRemoved();
      });
    }
  }

  clause->Rewrite(new_clause);

  // And we reattach it.
  if (all_clauses_are_attached_) Attach(clause, trail_);
  return true;
}

SatClause* ClauseManager::InprocessingAddClause(
    absl::Span<const Literal> new_clause) {
  DCHECK(!new_clause.empty());
  DCHECK(!all_clauses_are_attached_);
  if (DEBUG_MODE) {
    for (const Literal l : new_clause) {
      DCHECK(!trail_->Assignment().LiteralIsAssigned(l));
    }
  }

  if (new_clause.size() == 1) {
    // TODO(user): We should return false...
    if (!InprocessingFixLiteral(new_clause[0])) return nullptr;
    return nullptr;
  }

  if (new_clause.size() == 2) {
    implication_graph_->AddBinaryClause(new_clause[0], new_clause[1]);
    return nullptr;
  }

  SatClause* clause = SatClause::Create(new_clause);
  clauses_.push_back(clause);
  return clause;
}

void ClauseManager::CleanUpWatchers() {
  SCOPED_TIME_STAT(&stats_);
  for (const LiteralIndex index : needs_cleaning_.PositionsSetAtLeastOnce()) {
    DCHECK(needs_cleaning_[index]);
    RemoveIf(&(watchers_on_false_[index]), [](const Watcher& watcher) {
      return watcher.clause->IsRemoved();
    });
    needs_cleaning_.Clear(index);
  }
  needs_cleaning_.NotifyAllClear();
  is_clean_ = true;
}

// We also update to_minimize_index_/to_probe_index_ correctly.
//
// TODO(user): If more indices are needed, generalize the code to a vector of
// indices.
void ClauseManager::DeleteRemovedClauses() {
  DCHECK(is_clean_);

  int new_size = 0;
  const int old_size = clauses_.size();
  for (int i = 0; i < old_size; ++i) {
    if (i == to_minimize_index_) to_minimize_index_ = new_size;
    if (i == to_first_minimize_index_) to_first_minimize_index_ = new_size;
    if (i == to_probe_index_) to_probe_index_ = new_size;
    if (clauses_[i]->IsRemoved()) {
      delete clauses_[i];
    } else {
      clauses_[new_size++] = clauses_[i];
    }
  }
  clauses_.resize(new_size);

  if (to_minimize_index_ > new_size) to_minimize_index_ = new_size;
  if (to_first_minimize_index_ > new_size) to_first_minimize_index_ = new_size;
  if (to_probe_index_ > new_size) to_probe_index_ = new_size;
}

SatClause* ClauseManager::NextNewClauseToMinimize() {
  for (; to_first_minimize_index_ < clauses_.size();
       ++to_first_minimize_index_) {
    if (clauses_[to_first_minimize_index_]->IsRemoved()) continue;
    if (!IsRemovable(clauses_[to_first_minimize_index_])) {
      // If the round-robin is in-sync with the new clauses, we may as well
      // count this minimization as part of the round-robin and advance both
      // indexes.
      if (to_minimize_index_ == to_first_minimize_index_) {
        ++to_minimize_index_;
      }
      return clauses_[to_first_minimize_index_++];
    }
  }
  return nullptr;
}

SatClause* ClauseManager::NextClauseToMinimize() {
  for (; to_minimize_index_ < clauses_.size(); ++to_minimize_index_) {
    if (clauses_[to_minimize_index_]->IsRemoved()) continue;
    if (!IsRemovable(clauses_[to_minimize_index_])) {
      return clauses_[to_minimize_index_++];
    }
  }
  return nullptr;
}

SatClause* ClauseManager::NextClauseToProbe() {
  for (; to_probe_index_ < clauses_.size(); ++to_probe_index_) {
    if (clauses_[to_probe_index_]->IsRemoved()) continue;
    return clauses_[to_probe_index_++];
  }
  return nullptr;
}

// ----- BinaryImplicationGraph -----

void BinaryImplicationGraph::Resize(int num_variables) {
  SCOPED_TIME_STAT(&stats_);
  bfs_stack_.resize(num_variables << 1);
  implications_.resize(num_variables << 1);
  implies_something_.resize(num_variables << 1);
  might_have_dups_.resize(num_variables << 1);
  is_redundant_.resize(implications_.size());
  is_removed_.resize(implications_.size(), false);
  estimated_sizes_.resize(implications_.size(), 0);
  in_direct_implications_.resize(implications_.size(), false);
  reasons_.resize(num_variables);
}

void BinaryImplicationGraph::NotifyPossibleDuplicate(Literal a) {
  if (might_have_dups_[a.Index()]) return;
  might_have_dups_[a.Index()] = true;
  to_clean_.push_back(a);
}

void BinaryImplicationGraph::RemoveDuplicates() {
  for (const Literal l : to_clean_) {
    might_have_dups_[l.Index()] = false;
    gtl::STLSortAndRemoveDuplicates(&implications_[l.Index()]);
  }
  to_clean_.clear();
}

// TODO(user): Not all of the solver knows about representative literal, do
// use them here to maintains invariant? Explore this when we start cleaning our
// clauses using equivalence during search. We can easily do it for every
// conflict we learn instead of here.
bool BinaryImplicationGraph::AddBinaryClause(Literal a, Literal b) {
  SCOPED_TIME_STAT(&stats_);

  // Tricky: If this is the first clause, the propagator will be added and
  // assumed to be in a "propagated" state. This makes sure this is the case.
  if (IsEmpty()) propagation_trail_index_ = trail_->Index();

  if (drat_proof_handler_ != nullptr) {
    // TODO(user): Like this we will duplicate all binary clause from the
    // problem. However this leads to a simpler API (since we don't need to
    // special case the loading of the original clauses) and we mainly use drat
    // proof for testing anyway.
    drat_proof_handler_->AddClause({a, b});
  }

  if (is_redundant_[a.Index()]) a = Literal(representative_of_[a.Index()]);
  if (is_redundant_[b.Index()]) b = Literal(representative_of_[b.Index()]);
  if (a == b.Negated()) return true;
  if (a == b) return FixLiteral(a);

  DCHECK(!is_removed_[a]);
  DCHECK(!is_removed_[b]);
  estimated_sizes_[a.NegatedIndex()]++;
  estimated_sizes_[b.NegatedIndex()]++;
  implications_[a.NegatedIndex()].push_back(b);
  implications_[b.NegatedIndex()].push_back(a);
  implies_something_.Set(a.NegatedIndex());
  implies_something_.Set(b.NegatedIndex());
  NotifyPossibleDuplicate(a);
  NotifyPossibleDuplicate(b);
  is_dag_ = false;
  num_implications_ += 2;

  if (enable_sharing_ && add_binary_callback_ != nullptr) {
    add_binary_callback_(a, b);
  }

  const auto& assignment = trail_->Assignment();
  if (trail_->CurrentDecisionLevel() == 0) {
    DCHECK(!assignment.LiteralIsAssigned(a));
    DCHECK(!assignment.LiteralIsAssigned(b));
  } else {
    if (assignment.LiteralIsFalse(a)) {
      if (assignment.LiteralIsAssigned(b)) {
        if (assignment.LiteralIsFalse(b)) return false;
      } else {
        reasons_[trail_->Index()] = a;
        trail_->Enqueue(b, propagator_id_);
      }
    } else if (assignment.LiteralIsFalse(b)) {
      if (!assignment.LiteralIsAssigned(a)) {
        reasons_[trail_->Index()] = b;
        trail_->Enqueue(a, propagator_id_);
      }
    }
  }

  return true;
}

bool BinaryImplicationGraph::AddAtMostOne(
    absl::Span<const Literal> at_most_one) {
  DCHECK_EQ(trail_->CurrentDecisionLevel(), 0);
  if (at_most_one.size() <= 1) return true;

  // Temporarily copy the at_most_one constraint at the end of
  // at_most_one_buffer_. It will be cleaned up and added by
  // CleanUpAndAddAtMostOnes().
  const int base_index = at_most_one_buffer_.size();
  at_most_one_buffer_.push_back(Literal(LiteralIndex(at_most_one.size())));
  at_most_one_buffer_.insert(at_most_one_buffer_.end(), at_most_one.begin(),
                             at_most_one.end());

  is_dag_ = false;
  return CleanUpAndAddAtMostOnes(base_index);
}

// TODO(user): remove dupl with ClauseManager::InprocessingFixLiteral().
//
// Note that we currently do not support calling this at a positive level since
// we might loose the fixing on backtrack otherwise.
bool BinaryImplicationGraph::FixLiteral(Literal true_literal) {
  CHECK_EQ(trail_->CurrentDecisionLevel(), 0);
  if (trail_->Assignment().LiteralIsTrue(true_literal)) return true;
  if (trail_->Assignment().LiteralIsFalse(true_literal)) return false;

  if (drat_proof_handler_ != nullptr) {
    drat_proof_handler_->AddClause({true_literal});
  }

  trail_->EnqueueWithUnitReason(true_literal);
  return Propagate(trail_);
}

// This works by doing a linear scan on the at_most_one_buffer_ and
// cleaning/copying the at most ones on the fly to the beginning of the same
// buffer.
bool BinaryImplicationGraph::CleanUpAndAddAtMostOnes(int base_index) {
  const VariablesAssignment& assignment = trail_->Assignment();
  int local_end = base_index;
  const int buffer_size = at_most_one_buffer_.size();
  for (int i = base_index; i < buffer_size;) {
    // Process a new at most one.
    // It will be copied into buffer[local_start + 1, local_end].
    // With its size at buffer[local_start].
    const int local_start = local_end;

    // Update the iterator now. Even if the current at_most_one is reduced away,
    // local_start will still point to the next one, or to the end of the
    // buffer.
    if (i == at_most_one_iterator_) {
      at_most_one_iterator_ = local_start;
    }

    // We have an at_most_one starting at i, and we increment i to the next one.
    const absl::Span<const Literal> initial_amo = AtMostOne(i);
    i += initial_amo.size() + 1;

    // Reserve space for size.
    local_end++;
    bool set_all_left_to_false = false;
    for (const Literal l : initial_amo) {
      if (assignment.LiteralIsFalse(l)) continue;
      if (is_removed_[l]) continue;
      if (!set_all_left_to_false && assignment.LiteralIsTrue(l)) {
        set_all_left_to_false = true;
        continue;
      }
      at_most_one_buffer_[local_end++] = RepresentativeOf(l);
    }
    at_most_one_buffer_[local_start] =
        Literal(LiteralIndex(local_end - local_start - 1));

    // Deal with duplicates.
    // Any duplicate in an "at most one" must be false.
    bool some_duplicates = false;
    if (!set_all_left_to_false) {
      // Sort the copied amo.
      // We only want to expose a const AtMostOne() so we sort directly here.
      Literal* pt = &at_most_one_buffer_[local_start + 1];
      std::sort(pt, pt + AtMostOne(local_start).size());

      LiteralIndex previous = kNoLiteralIndex;
      bool remove_previous = false;
      int new_local_end = local_start + 1;
      for (const Literal l : AtMostOne(local_start)) {
        if (l.Index() == previous) {
          if (assignment.LiteralIsTrue(l)) return false;
          if (!assignment.LiteralIsFalse(l)) {
            if (!FixLiteral(l.Negated())) return false;
          }
          remove_previous = true;
          some_duplicates = true;
          continue;
        }

        // We need to pay attention to triplet or more of equal elements, so
        // it is why we need this boolean and can't just remove it right away.
        if (remove_previous) {
          --new_local_end;
          remove_previous = false;
        }
        previous = l.Index();
        at_most_one_buffer_[new_local_end++] = l;
      }
      if (remove_previous) --new_local_end;

      // Update local end and the amo size.
      local_end = new_local_end;
      at_most_one_buffer_[local_start] =
          Literal(LiteralIndex(local_end - local_start - 1));
    }

    // If there was some duplicates, we need to rescan to see if a literal
    // didn't become true because its negation was appearing twice!
    if (some_duplicates) {
      int new_local_end = local_start + 1;
      for (const Literal l : AtMostOne(local_start)) {
        if (assignment.LiteralIsFalse(l)) continue;
        if (!set_all_left_to_false && assignment.LiteralIsTrue(l)) {
          set_all_left_to_false = true;
          continue;
        }
        at_most_one_buffer_[new_local_end++] = l;
      }

      // Update local end and the amo size.
      local_end = new_local_end;
      at_most_one_buffer_[local_start] =
          Literal(LiteralIndex(local_end - local_start - 1));
    }

    // Deal with all false.
    if (set_all_left_to_false) {
      for (const Literal l : AtMostOne(local_start)) {
        if (assignment.LiteralIsFalse(l)) continue;
        if (assignment.LiteralIsTrue(l)) return false;
        if (!FixLiteral(l.Negated())) return false;
      }

      // Erase this at_most_one.
      local_end = local_start;
      continue;
    }

    // Create a Span<> to simplify the code below.
    const absl::Span<const Literal> at_most_one = AtMostOne(local_start);

    // We expand small sizes into implications.
    // TODO(user): Investigate what the best threshold is.
    if (at_most_one.size() <= std::max(2, at_most_one_max_expansion_size_)) {
      for (const Literal a : at_most_one) {
        for (const Literal b : at_most_one) {
          if (a == b) continue;
          implications_[a].push_back(b.Negated());
          implies_something_.Set(a);
          NotifyPossibleDuplicate(a);
        }
      }
      num_implications_ += at_most_one.size() * (at_most_one.size() - 1);

      // This will erase the at_most_one from the buffer.
      local_end = local_start;
      continue;
    }

    // Index the new at most one.
    for (const Literal l : at_most_one) {
      if (l.Index() >= at_most_ones_.size()) {
        at_most_ones_.resize(l.Index().value() + 1);
      }
      DCHECK(!is_redundant_[l]);
      at_most_ones_[l].push_back(local_start);
      implies_something_.Set(l);
    }
  }

  at_most_one_buffer_.resize(local_end);
  return true;
}

bool BinaryImplicationGraph::Propagate(Trail* trail) {
  SCOPED_TIME_STAT(&stats_);

  if (IsEmpty()) {
    propagation_trail_index_ = trail->Index();
    return true;
  }
  trail->SetCurrentPropagatorId(propagator_id_);

  const auto assignment = AssignmentView(trail->Assignment());
  const auto implies_something = implies_something_.view();
  auto* implications = implications_.data();

  while (propagation_trail_index_ < trail->Index()) {
    const Literal true_literal = (*trail)[propagation_trail_index_++];
    DCHECK(assignment.LiteralIsTrue(true_literal));
    if (!implies_something[true_literal]) continue;

    // Note(user): This update is not exactly correct because in case of
    // conflict we don't inspect that much clauses. But doing ++num_inspections_
    // inside the loop does slow down the code by a few percent.
    const absl::Span<const Literal> implied =
        implications[true_literal.Index().value()];
    num_inspections_ += implied.size();
    for (const Literal literal : implied) {
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
        trail->FastEnqueue(literal);
      }
    }

    // Propagate the at_most_one constraints.
    if (true_literal.Index() < at_most_ones_.size()) {
      for (const int start : at_most_ones_[true_literal]) {
        bool seen = false;
        for (const Literal literal : AtMostOne(start)) {
          ++num_inspections_;
          if (literal == true_literal) {
            if (DEBUG_MODE) {
              DCHECK(!seen);
              seen = true;
            }
            continue;
          }
          if (assignment.LiteralIsFalse(literal)) continue;

          ++num_propagations_;
          if (assignment.LiteralIsTrue(literal)) {
            // Conflict.
            *(trail->MutableConflict()) = {true_literal.Negated(),
                                           literal.Negated()};
            return false;
          } else {
            // Propagation.
            reasons_[trail->Index()] = true_literal.Negated();
            trail->FastEnqueue(literal.Negated());
          }
        }
      }
    }
  }

  return true;
}

absl::Span<const Literal> BinaryImplicationGraph::Reason(
    const Trail& /*trail*/, int trail_index, int64_t /*conflict_id*/) const {
  return {&reasons_[trail_index], 1};
}

// Here, we remove all the literal whose negation are implied by the negation of
// the 1-UIP literal (which always appear first in the given conflict). Note
// that this algorithm is "optimal" in the sense that it leads to a minimized
// conflict with a backjump level as low as possible. However, not all possible
// literals are removed.
//
// TODO(user): Also consider at most one?
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
    if (is_marked_[l]) continue;
    dfs_stack_.push_back(l);
    while (!dfs_stack_.empty()) {
      const LiteralIndex index = dfs_stack_.back().Index();
      dfs_stack_.pop_back();
      if (!is_marked_[index]) {
        is_marked_.Set(index);
        for (const Literal implied : implications_[index]) {
          if (!is_marked_[implied]) dfs_stack_.push_back(implied);
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
      is_marked_.Clear(l);
    }
  }

  // Now we can prune the direct implications list and make sure are the
  // literals there are marked.
  if (also_prune_direct_implication_list) {
    int new_size = 0;
    for (const Literal l : direct_implications) {
      if (!is_marked_[l]) {
        is_marked_.Set(l);
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
  DCHECK(!conflict->empty());
  is_marked_.ClearAndResize(LiteralIndex(implications_.size()));
  MarkDescendants(conflict->front().Negated());
  for (const LiteralIndex i : is_marked_.PositionsSetAtLeastOnce()) {
    // TODO(user): if this is false, then we actually have a conflict of size 2.
    // This can only happen if the binary clause was not propagated properly
    // if for instance we do chronological bactracking without re-enqueuing the
    // consequence of a binary clause.
    if (trail.Assignment().LiteralIsTrue(Literal(i))) {
      marked->Set(Literal(i).Variable());
    }
  }
  RemoveRedundantLiterals(conflict);
}

// Same as MinimizeConflictFirst() but take advantage of this reachability
// computation to remove redundant implication in the implication list of the
// first UIP conflict.
void BinaryImplicationGraph::MinimizeConflictFirstWithTransitiveReduction(
    const Trail& /*trail*/, std::vector<Literal>* conflict,
    absl::BitGenRef random) {
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
  std::shuffle(direct_implications.begin(), direct_implications.end(), random);
  dfs_stack_.clear();
  for (const Literal l : direct_implications) {
    if (is_marked_[l]) {
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
        for (const Literal implied : implications_[index]) {
          if (!is_marked_[implied]) dfs_stack_.push_back(implied);
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

// TODO(user): Also consider at most one?
void BinaryImplicationGraph::MinimizeConflictExperimental(
    const Trail& trail, std::vector<Literal>* conflict) {
  SCOPED_TIME_STAT(&stats_);
  is_marked_.ClearAndResize(LiteralIndex(implications_.size()));
  is_simplified_.ClearAndResize(LiteralIndex(implications_.size()));
  for (const Literal lit : *conflict) {
    is_marked_.Set(lit);
  }

  // Identify and remove the redundant literals from the given conflict.
  // 1/ If a -> b then a can be removed from the conflict clause.
  //    This is because not b -> not a.
  // 2/ a -> b can only happen if level(a) <= level(b).
  // 3/ Because of 2/, cycles can appear only at the same level.
  //    The vector is_simplified_ is used to avoid removing all elements of a
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
    for (const Literal implied : implications_[lit]) {
      if (is_marked_[implied]) {
        DCHECK_LE(lit_level, trail.Info(implied.Variable()).level);
        if (lit_level == trail.Info(implied.Variable()).level &&
            is_simplified_[implied]) {
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
      is_simplified_.Set(lit);
    }
  }
  if (index < conflict->size()) {
    ++num_minimization_;
    num_literals_removed_ += conflict->size() - index;
    conflict->erase(conflict->begin() + index, conflict->end());
  }
}

void BinaryImplicationGraph::RemoveFixedVariables() {
  SCOPED_TIME_STAT(&stats_);
  DCHECK_EQ(trail_->CurrentDecisionLevel(), 0);
  if (IsEmpty()) return;

  // Nothing to do if nothing changed since last call.
  const int new_num_fixed = trail_->Index();
  DCHECK_EQ(propagation_trail_index_, new_num_fixed);
  if (num_processed_fixed_variables_ == new_num_fixed) return;

  const VariablesAssignment& assignment = trail_->Assignment();
  is_marked_.ClearAndResize(LiteralIndex(implications_.size()));
  for (; num_processed_fixed_variables_ < new_num_fixed;
       ++num_processed_fixed_variables_) {
    const Literal true_literal = (*trail_)[num_processed_fixed_variables_];
    if (DEBUG_MODE) {
      // The code assumes that everything is already propagated.
      // Otherwise we will remove implications that didn't propagate yet!
      for (const Literal lit : implications_[true_literal]) {
        DCHECK(trail_->Assignment().LiteralIsTrue(lit));
      }
    }

    // If b is true and a -> b then because not b -> not a, all the
    // implications list that contains b will be marked by this process.
    // And the ones that contains not(b) should correspond to a false literal!
    //
    // TODO(user): This might not be true if we remove implication by
    // transitive reduction and the process was aborted due to the computation
    // limit. I think it will be good to maintain that invariant though,
    // otherwise fixed literals might never be removed from these lists...
    for (const Literal lit : implications_[true_literal.NegatedIndex()]) {
      if (lit.NegatedIndex() < representative_of_.size() &&
          representative_of_[lit.Negated()] != kNoLiteralIndex) {
        // We mark its representative instead.
        is_marked_.Set(representative_of_[lit.Negated()]);
      } else {
        is_marked_.Set(lit.NegatedIndex());
      }
    }
    gtl::STLClearObject(&(implications_[true_literal]));
    gtl::STLClearObject(&(implications_[true_literal.NegatedIndex()]));

    if (true_literal.Index() < at_most_ones_.size()) {
      gtl::STLClearObject(&(at_most_ones_[true_literal]));
    }
    if (true_literal.NegatedIndex() < at_most_ones_.size()) {
      gtl::STLClearObject(&(at_most_ones_[true_literal.NegatedIndex()]));
    }
  }
  for (const LiteralIndex i : is_marked_.PositionsSetAtLeastOnce()) {
    RemoveIf(&implications_[i], [&assignment](const Literal& lit) {
      return assignment.LiteralIsTrue(lit);
    });
  }

  // TODO(user): This might be a bit slow. Do not call all the time if needed,
  // this shouldn't change the correctness of the code.
  at_most_ones_.clear();
  CleanUpAndAddAtMostOnes(/*base_index=*/0);
  DCHECK(InvariantsAreOk());
}

class SccGraph {
 public:
  using Implications =
      util_intops::StrongVector<LiteralIndex, absl::InlinedVector<Literal, 6>>;
  using AtMostOnes =
      util_intops::StrongVector<LiteralIndex, absl::InlinedVector<int32_t, 6>>;
  using SccFinder =
      StronglyConnectedComponentsFinder<int32_t, SccGraph,
                                        CompactVectorVector<int32_t, int32_t>>;

  explicit SccGraph(SccFinder* finder, Implications* graph,
                    AtMostOnes* at_most_ones,
                    std::vector<Literal>* at_most_one_buffer)
      : finder_(*finder),
        implications_(*graph),
        at_most_ones_(*at_most_ones),
        at_most_one_buffer_(*at_most_one_buffer) {}

  const std::vector<int32_t>& operator[](int32_t node) const {
    tmp_.clear();
    for (const Literal l : implications_[LiteralIndex(node)]) {
      tmp_.push_back(l.Index().value());
      if (finder_.NodeIsInCurrentDfsPath(l.NegatedIndex().value())) {
        to_fix_.push_back(l);
      }
    }
    if (node < at_most_ones_.size()) {
      for (const int start : at_most_ones_[LiteralIndex(node)]) {
        if (start >= at_most_one_already_explored_.size()) {
          at_most_one_already_explored_.resize(start + 1, false);
          previous_node_to_explore_at_most_one_.resize(start + 1);
        }

        // In the presence of at_most_ones_ constraints, expanding them
        // implicitly to implications in the SCC computation can result in a
        // quadratic complexity rather than a linear one in term of the input
        // data structure size. So this test here is critical on problem with
        // large at_most ones like the "ivu06-big.mps.gz" where without it, the
        // full FindStronglyConnectedComponents() take more than on hour instead
        // of less than a second!
        if (at_most_one_already_explored_[start]) {
          // We never expand a node twice.
          const int first_node = previous_node_to_explore_at_most_one_[start];
          DCHECK_NE(node, first_node);

          if (finder_.NodeIsInCurrentDfsPath(first_node)) {
            // If the first node is not settled, then we do explore the
            // at_most_one constraint again. In "Mixed-Integer-Programming:
            // Analyzing 12 years of progress", Tobias Achterberg and Roland
            // Wunderling explains that an at most one need to be looped over at
            // most twice. I am not sure exactly how that works, so for now we
            // are not fully linear, but on actual instances, we only rarely
            // run into this case.
            //
            // Note that we change the previous node to explore at most one
            // since the current node will be settled before the old ones.
            //
            // TODO(user): avoid looping more than twice on the same at most one
            // constraints? Note that the second time we loop we have x => y =>
            // not(x), so we can already detect that x must be false which we
            // detect below.
            previous_node_to_explore_at_most_one_[start] = node;
          } else {
            // The first node is already settled and so are all its child. Only
            // not(first_node) might still need exploring.
            tmp_.push_back(
                Literal(LiteralIndex(first_node)).NegatedIndex().value());
            continue;
          }
        } else {
          at_most_one_already_explored_[start] = true;
          previous_node_to_explore_at_most_one_[start] = node;
        }

        const absl::Span<const Literal> amo =
            absl::MakeSpan(&at_most_one_buffer_[start + 1],
                           at_most_one_buffer_[start].Index().value());
        for (const Literal l : amo) {
          if (l.Index() == node) continue;
          tmp_.push_back(l.NegatedIndex().value());
          if (finder_.NodeIsInCurrentDfsPath(l.Index().value())) {
            to_fix_.push_back(l.Negated());
          }
        }
      }
    }
    work_done_ += tmp_.size();
    return tmp_;
  }

  // All these literals where detected to be true during the SCC computation.
  mutable std::vector<Literal> to_fix_;

  // For the deterministic time.
  mutable int64_t work_done_ = 0;

 private:
  const SccFinder& finder_;
  const Implications& implications_;
  const AtMostOnes& at_most_ones_;
  const std::vector<Literal>& at_most_one_buffer_;

  mutable std::vector<int32_t> tmp_;

  // Used to get a non-quadratic complexity in the presence of at most ones.
  mutable std::vector<bool> at_most_one_already_explored_;
  mutable std::vector<int> previous_node_to_explore_at_most_one_;
};

bool BinaryImplicationGraph::DetectEquivalences(bool log_info) {
  // This was already called, and no new constraint where added. Note that new
  // fixed variable cannot create new equivalence, only new binary clauses do.
  if (is_dag_) return true;
  WallTimer wall_timer;
  wall_timer.Start();
  log_info |= VLOG_IS_ON(1);

  // Lets remove all fixed variables first.
  if (!Propagate(trail_)) return false;
  RemoveFixedVariables();
  const VariablesAssignment& assignment = trail_->Assignment();
  DCHECK(InvariantsAreOk());

  // TODO(user): We could just do it directly though.
  const int32_t size(implications_.size());
  CompactVectorVector<int32_t, int32_t> scc;
  scc.reserve(size);
  double dtime = 0.0;
  {
    SccGraph::SccFinder finder;
    SccGraph graph(&finder, &implications_, &at_most_ones_,
                   &at_most_one_buffer_);
    finder.FindStronglyConnectedComponents(size, graph, &scc);
    dtime += 4e-8 * graph.work_done_;

    for (const Literal l : graph.to_fix_) {
      if (assignment.LiteralIsFalse(l)) return false;
      if (assignment.LiteralIsTrue(l)) continue;
      if (!FixLiteral(l)) return false;
    }
  }

  // The old values will still be valid.
  representative_of_.resize(size, kNoLiteralIndex);
  is_redundant_.resize(size);

  int num_equivalences = 0;
  reverse_topological_order_.clear();
  for (int index = 0; index < scc.size(); ++index) {
    const absl::Span<int32_t> component = scc[index];

    // If one is fixed then all must be fixed. Note that the reason why the
    // propagation didn't already do that and we don't always get fixed
    // component of size 1 is because of the potential newly fixed literals
    // above.
    //
    // In any case, all fixed literals are marked as redundant.
    {
      bool all_fixed = false;
      bool all_true = false;
      for (const int32_t i : component) {
        const Literal l = Literal(LiteralIndex(i));
        if (trail_->Assignment().LiteralIsAssigned(l)) {
          all_fixed = true;
          all_true = trail_->Assignment().LiteralIsTrue(l);
          break;
        }
      }
      if (all_fixed) {
        for (const int32_t i : component) {
          const Literal l = Literal(LiteralIndex(i));
          if (!is_redundant_[l]) {
            ++num_redundant_literals_;
            is_redundant_.Set(l);
            representative_of_[l] = l.Index();
          }
          const Literal to_fix = all_true ? l : l.Negated();
          if (assignment.LiteralIsFalse(to_fix)) return false;
          if (assignment.LiteralIsTrue(to_fix)) continue;
          if (!FixLiteral(l)) return false;
        }

        // Next component.
        continue;
      }
    }

    // We ignore variable that appear in no constraints.
    if (component.size() == 1 && is_removed_[LiteralIndex(component[0])]) {
      continue;
    }

    // We always take the smallest literal index (which also corresponds to the
    // smallest BooleanVariable index) as a representative. This make sure that
    // the representative of a literal l and the one of not(l) will be the
    // negation of each other. There is also reason to think that it is
    // heuristically better to use a BooleanVariable that was created first.
    std::sort(component.begin(), component.end());
    const LiteralIndex representative(component[0]);
    reverse_topological_order_.push_back(representative);

    if (component.size() == 1) {
      // Note that because we process list in reverse topological order, this
      // is only needed if there is any equivalence before this point.
      if (num_equivalences > 0) {
        auto& representative_list = implications_[representative];
        for (Literal& ref : representative_list) {
          const LiteralIndex rep = representative_of_[ref];
          if (rep == representative) continue;
          if (rep == kNoLiteralIndex) continue;
          ref = Literal(rep);
        }
        gtl::STLSortAndRemoveDuplicates(&representative_list);
      }
      continue;
    }

    // Sets the representative.
    for (int i = 1; i < component.size(); ++i) {
      const Literal literal = Literal(LiteralIndex(component[i]));
      if (!is_redundant_[literal]) {
        ++num_redundant_literals_;
        is_redundant_.Set(literal);
      }
      representative_of_[literal] = representative;

      // Detect if x <=> not(x) which means unsat. Note that we relly on the
      // fact that when sorted, they will both be consecutive in the list.
      if (Literal(LiteralIndex(component[i - 1])).Negated() == literal) {
        LOG_IF(INFO, log_info) << "Trivially UNSAT in DetectEquivalences()";
        return false;
      }
    }

    // Merge all the lists in implications_[representative].
    // Note that we do not want representative in its own list.
    auto& representative_list = implications_[representative];
    int new_size = 0;
    for (const Literal l : representative_list) {
      const Literal rep = RepresentativeOf(l);
      if (rep.Index() == representative) continue;
      representative_list[new_size++] = rep;
    }
    representative_list.resize(new_size);
    for (int i = 1; i < component.size(); ++i) {
      const Literal literal = Literal(LiteralIndex(component[i]));
      auto& ref = implications_[literal];
      for (const Literal l : ref) {
        const Literal rep = RepresentativeOf(l);
        if (rep.Index() != representative) representative_list.push_back(rep);
      }

      // Add representative <=> literal.
      //
      // Remark: this relation do not need to be added to a DRAT proof since
      // the redundant variables should never be used again for a pure SAT
      // problem.
      representative_list.push_back(literal);
      ref.clear();
      ref.push_back(Literal(representative));
    }
    gtl::STLSortAndRemoveDuplicates(&representative_list);
    num_equivalences += component.size() - 1;
  }

  is_dag_ = true;
  if (num_equivalences != 0) {
    // Remap all at most ones. Remove fixed variables, process duplicates. Note
    // that this might result in more implications when we expand small at most
    // one.
    at_most_ones_.clear();
    int saved_trail_index = propagation_trail_index_;
    CleanUpAndAddAtMostOnes(/*base_index=*/0);
    // This might have run the propagation on a few variables without taking
    // into account the AMOs. Propagate again.
    //
    // TODO(user): Maybe a better alternative is to not propagate when we fix
    // variables inside CleanUpAndAddAtMostOnes().
    if (propagation_trail_index_ != saved_trail_index) {
      propagation_trail_index_ = saved_trail_index;
      Propagate(trail_);
    }

    num_implications_ = 0;
    for (LiteralIndex i(0); i < size; ++i) {
      num_implications_ += implications_[i].size();
    }
    dtime += 2e-8 * num_implications_;
  }

  time_limit_->AdvanceDeterministicTime(dtime);
  const int num_fixed_during_scc =
      trail_->Index() - num_processed_fixed_variables_;
  RemoveFixedVariables();
  DCHECK(InvariantsAreOk());
  LOG_IF(INFO, log_info) << "SCC. " << num_equivalences
                         << " redundant equivalent literals. "
                         << num_fixed_during_scc << " fixed. "
                         << num_implications_ << " implications left. "
                         << implications_.size() << " literals."
                         << " size of at_most_one buffer = "
                         << at_most_one_buffer_.size() << "."
                         << " dtime: " << dtime
                         << " wtime: " << wall_timer.Get();
  return true;
}

// Note that as a side effect this also do a full "failed literal probing"
// using the binary implication graph only.
//
// TODO(user): Track which literal have new implications, and only process
// the antecedents of these.
bool BinaryImplicationGraph::ComputeTransitiveReduction(bool log_info) {
  DCHECK_EQ(trail_->CurrentDecisionLevel(), 0);
  if (time_limit_->LimitReached()) return true;
  if (!DetectEquivalences()) return false;

  // TODO(user): the situation with fixed variable is not really "clean".
  // Simplify the code so we are sure we don't run into issue or have to deal
  // with any of that here.
  if (time_limit_->LimitReached()) return true;
  if (!Propagate(trail_)) return false;
  RemoveFixedVariables();
  DCHECK(InvariantsAreOk());
  if (time_limit_->LimitReached()) return true;

  log_info |= VLOG_IS_ON(1);
  WallTimer wall_timer;
  wall_timer.Start();

  int64_t num_fixed = 0;
  int64_t num_new_redundant_implications = 0;
  bool aborted = false;
  tmp_removed_.clear();
  work_done_in_mark_descendants_ = 0;
  int marked_index = 0;

  // For each node we do a graph traversal and only keep the literals
  // at maximum distance 1. This only works because we have a DAG when ignoring
  // the "redundant" literal marked by DetectEquivalences(). Note that we also
  // need no duplicates in the implications list for correctness which is also
  // guaranteed by DetectEquivalences().
  //
  // TODO(user): We should be able to reuse some propagation like it is done for
  // tree-look. Once a node is processed, we just need to process a node that
  // implies it. Test if we can make this faster. Alternatively, only clear
  // a part of is_marked_ (after the first child of root in reverse topo order).
  //
  // TODO(user): Can we exploit the fact that the implication graph is a
  // skew-symmetric graph (isomorphic to its transposed) so that we do less
  // work?
  const LiteralIndex size(implications_.size());
  LiteralIndex previous = kNoLiteralIndex;
  for (const LiteralIndex root : reverse_topological_order_) {
    // In most situation reverse_topological_order_ contains no redundant, fixed
    // or removed variables. But the reverse_topological_order_ is only
    // recomputed when new binary are added to the graph, not when new variable
    // are fixed.
    if (is_redundant_[root]) continue;
    if (trail_->Assignment().LiteralIsAssigned(Literal(root))) continue;

    auto& direct_implications = implications_[root];
    if (direct_implications.empty()) continue;

    // This is a "poor" version of the tree look stuff, but it does show good
    // improvement. If we just processed one of the child of root, we don't
    // need to re-explore it.
    //
    // TODO(user): Another optim we can do is that we never need to expand
    // any node with a reverse topo order smaller or equal to the min of the
    // ones in this list.
    bool clear_previous_reachability = true;
    for (const Literal direct_child : direct_implications) {
      if (direct_child.Index() == previous) {
        clear_previous_reachability = false;
        is_marked_.Clear(previous);
        break;
      }
    }
    if (clear_previous_reachability) {
      is_marked_.ClearAndResize(size);
      marked_index = 0;
    }
    previous = root;

    for (const Literal direct_child : direct_implications) {
      if (is_redundant_[direct_child]) continue;
      if (is_marked_[direct_child]) continue;

      // This is a corner case where because of equivalent literal, root
      // appear in implications_[root], we will remove it below.
      if (direct_child.Index() == root) continue;

      // When this happens, then root must be false, we handle this just after
      // the loop.
      if (direct_child.NegatedIndex() == root) {
        is_marked_.Set(direct_child);
        break;
      }

      MarkDescendants(direct_child);

      // We have a DAG, so direct_child could only be marked first.
      is_marked_.Clear(direct_child);
    }
    DCHECK(!is_marked_[root])
        << "DetectEquivalences() should have removed cycles!";
    is_marked_.Set(root);

    // Also mark all the ones reachable through the root AMOs.
    if (root < at_most_ones_.size()) {
      auto is_marked = is_marked_.BitsetView();
      for (const int start : at_most_ones_[root]) {
        for (const Literal l : AtMostOne(start)) {
          if (l.Index() == root) continue;
          if (!is_marked[l.Negated()] && !is_redundant_[l.Negated()]) {
            is_marked_.SetUnsafe(is_marked, l.Negated());
            MarkDescendants(l.Negated());
          }
        }
      }
    }

    // Failed literal probing. If both x and not(x) are marked then root must be
    // false. Note that because we process "roots" in reverse topological order,
    // we will fix the LCA of x and not(x) first.
    const auto& marked_positions = is_marked_.PositionsSetAtLeastOnce();
    for (; marked_index < marked_positions.size(); ++marked_index) {
      const LiteralIndex i = marked_positions[marked_index];
      if (is_marked_[Literal(i).NegatedIndex()]) {
        // We tested that at the beginning.
        DCHECK(!trail_->Assignment().LiteralIsAssigned(Literal(root)));

        // We propagate right away the binary implications so that we do not
        // need to consider all antecedents of root in the transitive
        // reduction.
        ++num_fixed;
        if (!FixLiteral(Literal(root).Negated())) return false;
        break;
      }
    }

    // Note that direct_implications will be cleared by
    // RemoveFixedVariables() that will need to inspect it to completely
    // remove Literal(root) from all lists.
    if (trail_->Assignment().LiteralIsAssigned(Literal(root))) continue;

    // Only keep the non-marked literal (and the redundant one which are never
    // marked). We mark root to remove it in the corner case where it was
    // there.
    int new_size = 0;
    for (const Literal l : direct_implications) {
      if (!is_marked_[l]) {
        direct_implications[new_size++] = l;
      } else {
        tmp_removed_.push_back({Literal(root), l});
        DCHECK(!is_redundant_[l]);
      }
    }
    const int diff = direct_implications.size() - new_size;
    direct_implications.resize(new_size);
    direct_implications.shrink_to_fit();
    num_new_redundant_implications += diff;
    num_implications_ -= diff;

    // Abort if the computation involved is too big.
    if (work_done_in_mark_descendants_ > 1e8) {
      aborted = true;
      break;
    }
  }

  is_marked_.ClearAndResize(size);

  // If we aborted early, we might no longer have both a=>b and not(b)=>not(a).
  // This is not desirable has some algo relies on this invariant. We fix this
  // here.
  if (aborted) {
    absl::flat_hash_set<std::pair<LiteralIndex, LiteralIndex>> removed;
    for (const auto [a, b] : tmp_removed_) {
      removed.insert({a.Index(), b.Index()});
    }
    for (LiteralIndex i(0); i < implications_.size(); ++i) {
      int new_size = 0;
      const LiteralIndex negated_i = Literal(i).NegatedIndex();
      auto& implication = implications_[i];
      for (const Literal l : implication) {
        if (removed.contains({l.NegatedIndex(), negated_i})) continue;
        implication[new_size++] = l;
      }
      implication.resize(new_size);
    }
  }
  if (num_fixed > 0) {
    RemoveFixedVariables();
  }
  DCHECK(InvariantsAreOk());

  gtl::STLClearObject(&tmp_removed_);
  const double dtime = 1e-8 * work_done_in_mark_descendants_;
  time_limit_->AdvanceDeterministicTime(dtime);
  num_redundant_implications_ += num_new_redundant_implications;
  LOG_IF(INFO, log_info) << "Transitive reduction removed "
                         << num_new_redundant_implications << " literals. "
                         << num_fixed << " fixed. " << num_implications_
                         << " implications left. " << implications_.size()
                         << " literals." << " dtime: " << dtime
                         << " wtime: " << wall_timer.Get()
                         << (aborted ? " Aborted." : "");
  return true;
}

namespace {

int ElementInIntersectionOrMinusOne(absl::Span<const int> a,
                                    absl::Span<const int> b) {
  DCHECK(std::is_sorted(a.begin(), a.end()));
  DCHECK(std::is_sorted(b.begin(), b.end()));
  if (a.empty() || b.empty()) return -1;
  int i = 0;
  int j = 0;
  while (true) {
    if (a[i] == b[j]) return a[i];
    if (a[i] < b[j]) {
      if (++i == a.size()) return -1;
    } else {
      if (++j == b.size()) return -1;
    }
  }
}

}  // namespace

std::vector<std::pair<int, int>>
BinaryImplicationGraph::FilterAndSortAtMostOnes(
    absl::Span<std::vector<Literal>> at_most_ones) {
  // We starts by processing larger constraints first.
  // But we want the output order to be stable.
  std::vector<std::pair<int, int>> index_size_vector;
  const int num_amos = at_most_ones.size();
  index_size_vector.reserve(num_amos);
  for (int index = 0; index < num_amos; ++index) {
    std::vector<Literal>& clique = at_most_ones[index];
    if (clique.size() <= 1) continue;

    // Note(user): Because we always use literal with the smallest variable
    // indices as representative, this make sure that if possible, we express
    // the clique in term of user provided variable (that are always created
    // first).
    //
    // Remap the clique to only use representative.
    for (Literal& ref : clique) {
      DCHECK_LT(ref.Index(), representative_of_.size());
      const LiteralIndex rep = representative_of_[ref];
      if (rep == kNoLiteralIndex) continue;
      ref = Literal(rep);
    }

    // We skip anything that can be presolved further as the code below do
    // not handle duplicate well.
    //
    // TODO(user): Shall we presolve it here?
    bool skip = false;
    std::sort(clique.begin(), clique.end());
    for (int i = 1; i < clique.size(); ++i) {
      if (clique[i] == clique[i - 1] || clique[i] == clique[i - i].Negated()) {
        skip = true;
        break;
      }
    }
    if (skip) continue;

    index_size_vector.push_back({index, clique.size()});
  }
  std::stable_sort(
      index_size_vector.begin(), index_size_vector.end(),
      [](const std::pair<int, int> a, const std::pair<int, int>& b) {
        return a.second > b.second;
      });
  return index_size_vector;
}

bool BinaryImplicationGraph::TransformIntoMaxCliques(
    std::vector<std::vector<Literal>>* at_most_ones,
    int64_t max_num_explored_nodes) {
  // The code below assumes a DAG.
  if (!DetectEquivalences()) return false;
  work_done_in_mark_descendants_ = 0;

  int num_extended = 0;
  int num_removed = 0;
  int num_added = 0;

  // Data to detect inclusion of base amo into extend amo.
  std::vector<int> detector_clique_index;
  CompactVectorVector<int> storage;
  InclusionDetector detector(storage, time_limit_);
  detector.SetWorkLimit(1e9);

  std::vector<int> dense_index_to_index;
  util_intops::StrongVector<LiteralIndex, std::vector<int>>
      max_cliques_containing(implications_.size());

  const std::vector<std::pair<int, int>> index_size_vector =
      FilterAndSortAtMostOnes(absl::MakeSpan(*at_most_ones));

  absl::flat_hash_set<int> cannot_be_removed;
  std::vector<bool> was_extended(at_most_ones->size(), false);
  for (const auto& [index, old_size] : index_size_vector) {
    std::vector<Literal>& clique = (*at_most_ones)[index];
    if (time_limit_->LimitReached()) break;

    // Special case for clique of size 2, we don't expand them if they
    // are included in an already added clique.
    if (clique.size() == 2) {
      DCHECK_NE(clique[0], clique[1]);
      const int dense_index = ElementInIntersectionOrMinusOne(
          max_cliques_containing[clique[0]], max_cliques_containing[clique[1]]);
      if (dense_index >= 0) {
        const int superset_index = dense_index_to_index[dense_index];
        if (was_extended[superset_index]) {
          cannot_be_removed.insert(superset_index);
        }
        ++num_removed;
        clique.clear();
        continue;
      }
    }

    // Save the non-extended version as possible subset.
    // TODO(user): Detect on the fly is superset already exist.
    detector_clique_index.push_back(index);
    detector.AddPotentialSubset(storage.AddLiterals(clique));

    // We only expand the clique as long as we didn't spend too much time.
    if (work_done_in_mark_descendants_ < max_num_explored_nodes) {
      clique = ExpandAtMostOne(clique, max_num_explored_nodes);
    }

    // Save the extended version as possible superset.
    detector_clique_index.push_back(index);
    detector.AddPotentialSuperset(storage.AddLiterals(clique));

    // Also index clique for size 2 quick lookup.
    const int dense_index = dense_index_to_index.size();
    dense_index_to_index.push_back(index);
    for (const Literal l : clique) {
      max_cliques_containing[l].push_back(dense_index);
    }

    if (clique.size() > old_size) {
      was_extended[index] = true;
      ++num_extended;
    }
    ++num_added;
  }

  // Remove clique (before extension) that are included in an extended one.
  detector.DetectInclusions([&](int subset, int superset) {
    const int subset_index = detector_clique_index[subset];
    const int superset_index = detector_clique_index[superset];
    if (subset_index == superset_index) return;

    // Abort if one was already deleted.
    if ((*at_most_ones)[subset_index].empty()) return;
    if ((*at_most_ones)[superset_index].empty()) return;

    // If an extended clique already cover a deleted one, we cannot try to
    // remove it by looking at its non-extended version.
    if (cannot_be_removed.contains(subset_index)) return;

    ++num_removed;
    (*at_most_ones)[subset_index].clear();
    if (was_extended[superset_index]) cannot_be_removed.insert(superset_index);
  });

  if (num_extended > 0 || num_removed > 0 || num_added > 0) {
    VLOG(1) << "Clique Extended: " << num_extended
            << " Removed: " << num_removed << " Added: " << num_added
            << (work_done_in_mark_descendants_ > max_num_explored_nodes
                    ? " (Aborted)"
                    : "");
  }
  return true;
}

bool BinaryImplicationGraph::MergeAtMostOnes(
    absl::Span<std::vector<Literal>> at_most_ones,
    int64_t max_num_explored_nodes, double* dtime) {
  // The code below assumes a DAG.
  if (!DetectEquivalences()) return false;
  work_done_in_mark_descendants_ = 0;

  const std::vector<std::pair<int, int>> index_size_vector =
      FilterAndSortAtMostOnes(at_most_ones);

  // Data to detect inclusion of base amo into extend amo.
  std::vector<int> detector_clique_index;
  CompactVectorVector<int> storage;
  for (const auto& [index, old_size] : index_size_vector) {
    if (time_limit_->LimitReached()) break;
    detector_clique_index.push_back(index);
    storage.AddLiterals(at_most_ones[index]);
  }

  // We use an higher limit here as the code is faster.
  SubsetsDetector detector(storage, time_limit_);
  detector.SetWorkLimit(10 * max_num_explored_nodes);
  detector.IndexAllStorageAsSubsets();

  // Now try to expand one by one.
  //
  // TODO(user): We should process clique with elements in common together so
  // that we can reuse MarkDescendants() which is slow. We should be able to
  // "cache" a few of the last calls.
  std::vector<int> intersection;
  const int num_to_consider = index_size_vector.size();
  for (int subset_index = 0; subset_index < num_to_consider; ++subset_index) {
    const int index = index_size_vector[subset_index].first;
    std::vector<Literal>& clique = at_most_ones[index];
    if (clique.empty()) continue;  // Was deleted.

    if (work_done_in_mark_descendants_ > max_num_explored_nodes) break;
    if (detector.Stopped()) break;

    // We start with the clique in the "intersection".
    // This prefix will never change.
    int clique_i = 0;
    int next_index_to_try = 0;
    intersection.clear();
    tmp_bitset_.ClearAndResize(LiteralIndex(implications_.size()));
    for (const Literal l : clique) {
      intersection.push_back(l.Index().value());
      tmp_bitset_.Set(l);
    }

    while (true) {
      if (work_done_in_mark_descendants_ > max_num_explored_nodes) break;
      if (detector.Stopped()) break;

      // Compute the intersection of all the element (or the new ones) of this
      // clique.
      //
      // Optimization: if clique_i > 0 && intersection.size() == clique.size()
      // we already know that we performed the max possible extension.
      if (clique_i > 0 && intersection.size() == clique.size()) {
        clique_i = clique.size();
      }
      for (; clique_i < clique.size(); ++clique_i) {
        const Literal l = clique[clique_i];

        is_marked_.ClearAndResize(LiteralIndex(implications_.size()));
        MarkDescendants(l);

        if (clique_i == 0) {
          // Initially we have the clique + the negation of everything
          // propagated by l.
          for (const LiteralIndex index :
               is_marked_.PositionsSetAtLeastOnce()) {
            const Literal lit = Literal(index).Negated();
            if (!tmp_bitset_[lit]) {
              intersection.push_back(lit.Index().value());
            }
          }
        } else {
          // We intersect we the negation of everything propagated by not(l).
          // Note that we always keep the clique in case some implication where
          // not added to the graph.
          int new_size = 0;
          const int old_size = intersection.size();
          for (int i = 0; i < old_size; ++i) {
            if (i == next_index_to_try) {
              next_index_to_try = new_size;
            }
            const int index = intersection[i];
            const Literal lit = Literal(LiteralIndex(index));
            if (tmp_bitset_[lit] || is_marked_[lit.Negated()]) {
              intersection[new_size++] = index;
            }
          }
          intersection.resize(new_size);
        }

        // We can abort early as soon as there is no extra literal than the
        // initial clique.
        if (intersection.size() <= clique.size()) break;
      }

      // Should contains the original clique. If there are no more entry, then
      // we will not extend this clique. However, we still call FindSubsets() in
      // order to remove fully included ones.
      CHECK_GE(intersection.size(), clique.size());

      // Look for element included in the intersection.
      // Note that we clear element fully included at the same time.
      //
      // TODO(user): next_index_to_try help, but we might still rescan most of
      // the one-watcher list of intersection[next_index_to_try], we could be
      // a bit faster here.
      int num_extra = 0;
      detector.FindSubsets(intersection, &next_index_to_try, [&](int subset) {
        if (subset == subset_index) {
          detector.StopProcessingCurrentSubset();
          return;
        }

        num_extra = 0;
        for (const int index : storage[subset]) {
          const LiteralIndex lit_index = LiteralIndex(index);
          if (tmp_bitset_[lit_index]) continue;  // In clique.
          tmp_bitset_.Set(lit_index);
          clique.push_back(Literal(lit_index));  // extend.
          ++num_extra;
        }
        if (num_extra == 0) {
          // Fully included -- remove.
          at_most_ones[detector_clique_index[subset]].clear();
          detector.StopProcessingCurrentSubset();
          return;
        }

        detector.StopProcessingCurrentSuperset();  // Finish.
      });

      // No extension: end loop.
      if (num_extra == 0) break;
    }
  }
  if (dtime != nullptr) {
    *dtime +=
        1e-8 * work_done_in_mark_descendants_ + 1e-9 * detector.work_done();
  }
  return true;
}

template <bool use_weight>
std::vector<Literal> BinaryImplicationGraph::ExpandAtMostOneWithWeight(
    const absl::Span<const Literal> at_most_one,
    const util_intops::StrongVector<LiteralIndex, bool>& can_be_included,
    const util_intops::StrongVector<LiteralIndex, double>& expanded_lp_values) {
  std::vector<Literal> clique(at_most_one.begin(), at_most_one.end());
  std::vector<LiteralIndex> intersection;
  const int64_t old_work = work_done_in_mark_descendants_;
  for (int i = 0; i < clique.size(); ++i) {
    // Do not spend too much time here.
    if (work_done_in_mark_descendants_ - old_work > 1e8) break;

    is_marked_.ClearAndResize(LiteralIndex(implications_.size()));
    MarkDescendants(clique[i]);
    if (i == 0) {
      for (const LiteralIndex index : is_marked_.PositionsSetAtLeastOnce()) {
        if (can_be_included[Literal(index).NegatedIndex()]) {
          intersection.push_back(index);
        }
      }
      for (const Literal l : clique) is_marked_.Clear(l.NegatedIndex());
    }

    int new_size = 0;
    is_marked_.Clear(clique[i]);
    is_marked_.Clear(clique[i].NegatedIndex());
    for (const LiteralIndex index : intersection) {
      if (!is_marked_[index]) continue;
      intersection[new_size++] = index;
    }
    intersection.resize(new_size);
    if (intersection.empty()) break;

    // Expand? The negation of any literal in the intersection is a valid way
    // to extend the clique.
    if (i + 1 == clique.size()) {
      // Heuristic: use literal with largest lp value. We randomize slightly.
      int index = -1;
      double max_lp = 0.0;
      for (int j = 0; j < intersection.size(); ++j) {
        // If we don't use weight, we prefer variable that comes first.
        const double lp =
            use_weight
                ? expanded_lp_values[Literal(intersection[j]).NegatedIndex()] +
                      absl::Uniform<double>(*random_, 0.0, 1e-4)
                : can_be_included.size() - intersection[j].value();
        if (index == -1 || lp > max_lp) {
          index = j;
          max_lp = lp;
        }
      }
      if (index != -1) {
        clique.push_back(Literal(intersection[index]).Negated());
        std::swap(intersection.back(), intersection[index]);
        intersection.pop_back();
      }
    }
  }
  return clique;
}

// Make sure both version are compiled.
template std::vector<Literal>
BinaryImplicationGraph::ExpandAtMostOneWithWeight<true>(
    const absl::Span<const Literal> at_most_one,
    const util_intops::StrongVector<LiteralIndex, bool>& can_be_included,
    const util_intops::StrongVector<LiteralIndex, double>& expanded_lp_values);
template std::vector<Literal>
BinaryImplicationGraph::ExpandAtMostOneWithWeight<false>(
    const absl::Span<const Literal> at_most_one,
    const util_intops::StrongVector<LiteralIndex, bool>& can_be_included,
    const util_intops::StrongVector<LiteralIndex, double>& expanded_lp_values);

// This function and the generated cut serves two purpose:
//   1/ If a new clause of size 2 was discovered and not included in the LP, we
//     will add it.
//   2/ The more classical clique cut separation algorithm
//
// Note that once 1/ Is performed, any literal close to 1.0 in the lp shouldn't
// be in the same clique as a literal with positive weight. So for step 2, we
// only really need to consider fractional variables.
const std::vector<std::vector<Literal>>&
BinaryImplicationGraph::GenerateAtMostOnesWithLargeWeight(
    absl::Span<const Literal> literals, absl::Span<const double> lp_values,
    absl::Span<const double> reduced_costs) {
  // We only want to generate a cut with literals from the LP, not extra ones.
  const int num_literals = implications_.size();
  util_intops::StrongVector<LiteralIndex, bool> can_be_included(num_literals,
                                                                false);
  util_intops::StrongVector<LiteralIndex, double> expanded_lp_values(
      num_literals, 0.0);
  util_intops::StrongVector<LiteralIndex, double> heuristic_weights(
      num_literals, 0.0);
  const int size = literals.size();
  for (int i = 0; i < size; ++i) {
    const Literal l = literals[i];
    can_be_included[l] = true;
    can_be_included[l.NegatedIndex()] = true;

    const double value = lp_values[i];
    expanded_lp_values[l] = value;
    expanded_lp_values[l.NegatedIndex()] = 1.0 - value;

    // This is used for extending clique-cuts.
    // In most situation, we will only extend the cuts with literal at zero,
    // and we prefer "low" reduced cost first, so we negate it. Variable with
    // high reduced costs will likely stay that way and are of less interest in
    // a clique cut. At least that is my interpretation.
    //
    // TODO(user): For large problems or when we base the clique from a newly
    // added and violated 2-clique, we might consider only a subset of
    // fractional variables, so we might need to include fractional variable
    // first, but then their rc should be zero, so it should be already kind of
    // doing that.
    //
    // Remark: This seems to have a huge impact to the cut performance!
    const double rc = reduced_costs[i];
    heuristic_weights[l] = -rc;
    heuristic_weights[l.NegatedIndex()] = rc;
  }

  // We want highest sum first.
  struct Candidate {
    Literal a;
    Literal b;
    double sum;
    bool operator<(const Candidate& other) const { return sum > other.sum; }
  };
  std::vector<Candidate> candidates;

  // First heuristic. Currently we only consider violated at most one of size 2,
  // and extend them. Right now, the code is a bit slow to try too many at every
  // LP node so it is why we are defensive like this. Note also that because we
  // currently still statically add the initial implications, this will only add
  // cut based on newly learned binary clause. Or the one that were not added
  // to the relaxation in the first place.
  std::vector<Literal> fractional_literals;
  for (int i = 0; i < size; ++i) {
    Literal current_literal = literals[i];
    double current_value = lp_values[i];
    if (trail_->Assignment().LiteralIsAssigned(current_literal)) continue;
    if (is_redundant_[current_literal]) continue;

    if (current_value < 0.5) {
      current_literal = current_literal.Negated();
      current_value = 1.0 - current_value;
    }

    if (current_value < 0.99) {
      fractional_literals.push_back(current_literal);
    }

    // We consider only one candidate for each current_literal.
    LiteralIndex best = kNoLiteralIndex;
    double best_value = 0.0;
    for (const Literal l : implications_[current_literal]) {
      if (!can_be_included[l]) continue;
      const double activity =
          current_value + expanded_lp_values[l.NegatedIndex()];
      if (activity <= 1.01) continue;
      const double v = activity + absl::Uniform<double>(*random_, 0.0, 1e-4);
      if (best == kNoLiteralIndex || v > best_value) {
        best_value = v;
        best = l.NegatedIndex();
      }
    }
    if (best != kNoLiteralIndex) {
      const double activity = current_value + expanded_lp_values[best];
      candidates.push_back({current_literal, Literal(best), activity});
    }
  }

  // Do not genate too many cut at once.
  const int kMaxNumberOfCutPerCall = 10;
  std::sort(candidates.begin(), candidates.end());
  if (candidates.size() > kMaxNumberOfCutPerCall) {
    candidates.resize(kMaxNumberOfCutPerCall);
  }

  // Expand to a maximal at most one each candidates before returning them.
  // Note that we only expand using literal from the LP.
  tmp_cuts_.clear();
  for (const Candidate& candidate : candidates) {
    tmp_cuts_.push_back(ExpandAtMostOneWithWeight(
        {candidate.a, candidate.b}, can_be_included, heuristic_weights));
  }

  // Once we processed new implications, also add "proper" clique cuts.
  // We can generate a small graph and separate cut efficiently there.
  if (fractional_literals.size() > 1) {
    // Lets permute this randomly and truncate if we have too many variables.
    // Since we use bitset it is good to have a multiple of 64 there.
    //
    // TODO(user): Prefer more fractional variables.
    const int max_graph_size = 1024;
    if (fractional_literals.size() > max_graph_size) {
      std::shuffle(fractional_literals.begin(), fractional_literals.end(),
                   *random_);
      fractional_literals.resize(max_graph_size);
    }

    bron_kerbosch_.Initialize(fractional_literals.size() * 2);

    // Prepare a dense mapping.
    int i = 0;
    tmp_mapping_.resize(implications_.size(), -1);
    for (const Literal l : fractional_literals) {
      bron_kerbosch_.SetWeight(i, expanded_lp_values[l]);
      tmp_mapping_[l] = i++;
      bron_kerbosch_.SetWeight(i, expanded_lp_values[l.Negated()]);
      tmp_mapping_[l.Negated()] = i++;
    }

    // Copy the implication subgraph and remap it to a dense indexing.
    //
    // TODO(user): Treat at_most_one more efficiently. We can collect them
    // and scan each of them just once.
    for (const Literal base : fractional_literals) {
      for (const Literal l : {base, base.Negated()}) {
        const int from = tmp_mapping_[l];
        for (const Literal next : DirectImplications(l)) {
          // l => next so (l + not(next) <= 1).
          const int to = tmp_mapping_[next.Negated()];
          if (to != -1) {
            bron_kerbosch_.AddEdge(from, to);
          }
        }
      }
    }

    // Before running the algo, compute the transitive closure.
    // The graph shouldn't be too large, so this should be fast enough.
    bron_kerbosch_.TakeTransitiveClosureOfImplicationGraph();

    bron_kerbosch_.SetWorkLimit(1e8);
    bron_kerbosch_.SetMinimumWeight(1.001);
    std::vector<std::vector<int>> cliques = bron_kerbosch_.Run();

    // If we have many candidates, we will only expand the first few with
    // maximum weights.
    const int max_num_per_batch = 5;
    std::vector<std::pair<int, double>> with_weight =
        bron_kerbosch_.GetMutableIndexAndWeight();
    if (with_weight.size() > max_num_per_batch) {
      std::sort(
          with_weight.begin(), with_weight.end(),
          [](const std::pair<int, double>& a, const std::pair<int, double>& b) {
            return a.second > b.second;
          });
      with_weight.resize(max_num_per_batch);
    }

    std::vector<Literal> at_most_one;
    for (const auto [index, weight] : with_weight) {
      // Convert.
      at_most_one.clear();
      for (const int i : cliques[index]) {
        const Literal l = fractional_literals[i / 2];
        at_most_one.push_back(i % 2 == 1 ? l.Negated() : l);
      }

      // Expand and add clique.
      //
      // TODO(user): Expansion is pretty slow. Given that the base clique can
      // share literal beeing part of the same amo, we should be able to speed
      // that up, we don't want to scan an amo twice basically.
      tmp_cuts_.push_back(ExpandAtMostOneWithWeight(
          at_most_one, can_be_included, heuristic_weights));
    }

    // Clear the dense mapping
    for (const Literal l : fractional_literals) {
      tmp_mapping_[l] = -1;
      tmp_mapping_[l.Negated()] = -1;
    }
  }

  return tmp_cuts_;
}

// TODO(user): Use deterministic limit.
// TODO(user): Explore the graph instead of just looking at full amo, especially
// since we expand small ones.
std::vector<absl::Span<const Literal>>
BinaryImplicationGraph::HeuristicAmoPartition(std::vector<Literal>* literals) {
  std::vector<absl::Span<const Literal>> result;

  util_intops::StrongVector<LiteralIndex, bool> to_consider(
      implications_.size(), false);
  for (const Literal l : *literals) to_consider[l] = true;

  // Priority queue of (intersection_size, start_of_amo).
  std::priority_queue<std::pair<int, int>> pq;

  // Compute for each at most one that might overlap, its overlaping size and
  // stores its start in the at_most_one_buffer_.
  //
  // This is in O(num_literal in amo).
  absl::flat_hash_set<int> explored_amo;
  for (const Literal l : *literals) {
    if (l.Index() >= at_most_ones_.size()) continue;
    for (const int start : at_most_ones_[l]) {
      const auto [_, inserted] = explored_amo.insert(start);
      if (!inserted) continue;

      int intersection_size = 0;
      for (const Literal l : AtMostOne(start)) {
        if (to_consider[l]) ++intersection_size;
      }
      if (intersection_size > 1) {
        pq.push({intersection_size, start});
      }

      // Abort early if we are done.
      if (intersection_size == literals->size()) break;
    }
  }

  // Consume AMOs, update size.
  int num_processed = 0;
  while (!pq.empty()) {
    const auto [old_size, start] = pq.top();
    pq.pop();

    // Recompute size.
    int intersection_size = 0;
    for (const Literal l : AtMostOne(start)) {
      if (to_consider[l]) ++intersection_size;
    }
    if (intersection_size != old_size) {
      // re-add with new size.
      if (intersection_size > 1) {
        pq.push({intersection_size, start});
      }
      continue;
    }

    // Mark the literal of that at most one at false.
    for (const Literal l : AtMostOne(start)) {
      to_consider[l] = false;
    }

    // Extract the intersection by moving it in
    // [num_processed, num_processed + intersection_size).
    const int span_start = num_processed;
    for (int i = num_processed; i < literals->size(); ++i) {
      if (to_consider[(*literals)[i]]) continue;
      std::swap((*literals)[num_processed], (*literals)[i]);
      ++num_processed;
    }
    result.push_back(absl::MakeSpan(literals->data() + span_start,
                                    num_processed - span_start));
  }
  return result;
}

void BinaryImplicationGraph::MarkDescendants(Literal root) {
  auto* const stack = bfs_stack_.data();
  auto is_marked = is_marked_.BitsetView();
  auto is_redundant = is_redundant_.const_view();
  if (is_redundant[root]) return;

  int stack_size = 1;
  stack[0] = root;
  is_marked_.Set(root);
  const int amo_size = static_cast<int>(at_most_ones_.size());
  auto implies_something = implies_something_.const_view();
  for (int j = 0; j < stack_size; ++j) {
    const Literal current = stack[j];
    if (!implies_something[current]) continue;

    work_done_in_mark_descendants_ += implications_[current].size();
    for (const Literal l : implications_[current]) {
      if (!is_marked[l] && !is_redundant[l]) {
        is_marked_.SetUnsafe(is_marked, l);
        stack[stack_size++] = l;
      }
    }

    if (current.Index() >= amo_size) continue;
    for (const int start : at_most_ones_[current]) {
      work_done_in_mark_descendants_ += AtMostOne(start).size();
      for (const Literal l : AtMostOne(start)) {
        if (l == current) continue;
        if (!is_marked[l.NegatedIndex()] && !is_redundant[l.NegatedIndex()]) {
          is_marked_.SetUnsafe(is_marked, l.NegatedIndex());
          stack[stack_size++] = l.Negated();
        }
      }
    }
  }
  work_done_in_mark_descendants_ += stack_size;
}

std::vector<Literal> BinaryImplicationGraph::ExpandAtMostOne(
    const absl::Span<const Literal> at_most_one,
    int64_t max_num_explored_nodes) {
  std::vector<Literal> clique(at_most_one.begin(), at_most_one.end());

  // Optim.
  for (const Literal l : clique) {
    if (!implies_something_[l]) {
      return clique;
    }
  }

  // TODO(user): Improve algorithm. If we do a dfs, we can know if a literal
  // has no descendant in the current intersection. We can keep such literal
  // marked.
  std::vector<LiteralIndex> intersection;
  for (int i = 0; i < clique.size(); ++i) {
    if (work_done_in_mark_descendants_ > max_num_explored_nodes) break;
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

    // TODO(user): If the intersection is small compared to the members of the
    // clique left to explore, we could look at the descendants of the negated
    // intersection instead.

    // Expand?
    if (i + 1 == clique.size()) {
      clique.push_back(Literal(intersection.back()).Negated());
      intersection.pop_back();
    }
  }
  return clique;
}

// TODO(user): lazy cleanup the lists on is_removed_?
// TODO(user): Mark fixed variable as is_removed_ for faster iteration?
const std::vector<Literal>& BinaryImplicationGraph::DirectImplications(
    Literal literal) {
  DCHECK(!is_removed_[literal]);

  // Clear old state.
  for (const Literal l : direct_implications_) {
    in_direct_implications_[l] = false;
  }
  direct_implications_.clear();

  // Fill new state.
  const VariablesAssignment& assignment = trail_->Assignment();
  DCHECK(!assignment.LiteralIsAssigned(literal));
  for (const Literal l : implications_[literal]) {
    if (l == literal) continue;
    if (assignment.LiteralIsAssigned(l)) continue;
    if (!is_removed_[l] && !in_direct_implications_[l]) {
      in_direct_implications_[l] = true;
      direct_implications_.push_back(l);
    }
  }
  if (literal.Index() < at_most_ones_.size()) {
    if (is_redundant_[literal]) {
      DCHECK(at_most_ones_[literal].empty());
    }
    for (const int start : at_most_ones_[literal]) {
      for (const Literal l : AtMostOne(start)) {
        if (l == literal) continue;
        if (assignment.LiteralIsAssigned(l)) continue;
        if (!is_removed_[l] && !in_direct_implications_[l.NegatedIndex()]) {
          in_direct_implications_[l.NegatedIndex()] = true;
          direct_implications_.push_back(l.Negated());
        }
      }
    }
  }
  estimated_sizes_[literal] = direct_implications_.size();
  return direct_implications_;
}

absl::Span<const Literal> BinaryImplicationGraph::AtMostOne(int start) const {
  const int size = at_most_one_buffer_[start].Index().value();
  return absl::MakeSpan(&at_most_one_buffer_[start + 1], size);
}

LiteralIndex BinaryImplicationGraph::RandomImpliedLiteral(Literal lhs) {
  const int size1 = implications_[lhs].size();
  const int size2 =
      lhs.Index() < at_most_ones_.size() ? at_most_ones_[lhs].size() : 0;
  if (size1 + size2 == 0) return kNoLiteralIndex;

  const int choice = absl::Uniform<int>(*random_, 0, size1 + size2);
  if (choice < size1) {
    return implications_[lhs][choice].Index();
  }

  const absl::Span<const Literal> amo =
      AtMostOne(at_most_ones_[lhs][choice - size1]);
  CHECK_GE(amo.size(), 2);
  const int first_choice = absl::Uniform<int>(*random_, 0, amo.size());
  const Literal lit = amo[first_choice];
  if (lit != lhs) return lit.NegatedIndex();

  // We are unlucky and just picked the wrong literal: take a different one.
  int next_choice = absl::Uniform<int>(*random_, 0, amo.size() - 1);
  if (next_choice >= first_choice) {
    next_choice += 1;
  }
  CHECK_NE(amo[next_choice], lhs);
  return amo[next_choice].NegatedIndex();
}

bool BinaryImplicationGraph::FindFailedLiteralAroundVar(BooleanVariable var,
                                                        bool* is_unsat) {
  const int saved_index = propagation_trail_index_;
  DCHECK_EQ(propagation_trail_index_, trail_->Index());  // Propagation done.

  const VariablesAssignment& assignment = trail_->Assignment();
  if (assignment.VariableIsAssigned(var)) return false;

  const Literal literal(var, true);
  direct_implications_of_negated_literal_ =
      DirectImplications(literal.Negated());
  DirectImplications(literal);  // Fill in_direct_implications_.
  for (const Literal l : direct_implications_of_negated_literal_) {
    if (in_direct_implications_[l]) {
      // not(l) => literal => l.
      if (!FixLiteral(l)) {
        *is_unsat = true;
        return false;
      }
    }
  }

  return propagation_trail_index_ > saved_index;
}

int64_t BinaryImplicationGraph::NumImplicationOnVariableRemoval(
    BooleanVariable var) {
  const Literal literal(var, true);
  int64_t result = 0;
  direct_implications_of_negated_literal_ =
      DirectImplications(literal.Negated());
  const int64_t s1 = DirectImplications(literal).size();
  for (const Literal l : direct_implications_of_negated_literal_) {
    result += s1;

    // We should have dealt with that in FindFailedLiteralAroundVar().
    DCHECK(!in_direct_implications_[l]);

    // l => literal => l: equivalent variable!
    if (in_direct_implications_[l.NegatedIndex()]) result--;
  }
  return result;
}

// For all possible a => var => b, add a => b.
void BinaryImplicationGraph::RemoveBooleanVariable(
    BooleanVariable var, std::deque<std::vector<Literal>>* postsolve_clauses) {
  const Literal literal(var, true);
  DCHECK(!is_removed_[literal.Index()]);
  DCHECK(!is_redundant_[literal.Index()]);

  direct_implications_of_negated_literal_ =
      DirectImplications(literal.Negated());
  for (const Literal b : DirectImplications(literal)) {
    if (is_removed_[b]) continue;
    DCHECK(!is_redundant_[b]);
    estimated_sizes_[b.NegatedIndex()]--;
    for (const Literal a_negated : direct_implications_of_negated_literal_) {
      if (a_negated.Negated() == b) continue;
      if (is_removed_[a_negated]) continue;
      AddImplication(a_negated.Negated(), b);
    }
  }
  for (const Literal a_negated : direct_implications_of_negated_literal_) {
    if (is_removed_[a_negated]) continue;
    DCHECK(!is_redundant_[a_negated]);
    estimated_sizes_[a_negated.NegatedIndex()]--;
  }

  // Notify the deletion to the proof checker and the postsolve.
  // Note that we want var first in these clauses for the postsolve.
  for (const Literal b : direct_implications_) {
    if (drat_proof_handler_ != nullptr) {
      drat_proof_handler_->DeleteClause({Literal(var, false), b});
    }
    postsolve_clauses->push_back({Literal(var, false), b});
  }
  for (const Literal a_negated : direct_implications_of_negated_literal_) {
    if (drat_proof_handler_ != nullptr) {
      drat_proof_handler_->DeleteClause({Literal(var, true), a_negated});
    }
    postsolve_clauses->push_back({Literal(var, true), a_negated});
  }

  // We need to remove any occurrence of var in our implication lists, this will
  // be delayed to the CleanupAllRemovedVariables() call.
  for (const LiteralIndex index : {literal.Index(), literal.NegatedIndex()}) {
    is_removed_[index] = true;
    implications_[index].clear();
    if (!is_redundant_[index]) {
      ++num_redundant_literals_;
      is_redundant_.Set(index);
    }
  }
}

void BinaryImplicationGraph::RemoveAllRedundantVariables(
    std::deque<std::vector<Literal>>* postsolve_clauses) {
  for (LiteralIndex a(0); a < implications_.size(); ++a) {
    if (is_redundant_[a] && !is_removed_[a]) {
      postsolve_clauses->push_back(
          {Literal(a), Literal(RepresentativeOf(Literal(a))).Negated()});
      is_removed_[a] = true;
      gtl::STLClearObject(&(implications_[a]));
      continue;
    }

    int new_size = 0;
    auto& implication = implications_[a];
    for (const Literal l : implication) {
      if (!is_redundant_[l]) {
        implication[new_size++] = l;
      }
    }
    implication.resize(new_size);
  }
}

void BinaryImplicationGraph::CleanupAllRemovedAndFixedVariables() {
  const VariablesAssignment& assignment = trail_->Assignment();
  for (LiteralIndex a(0); a < implications_.size(); ++a) {
    if (is_removed_[a] || assignment.LiteralIsAssigned(Literal(a))) {
      if (DEBUG_MODE && assignment.LiteralIsTrue(Literal(a))) {
        // The code assumes that everything is already propagated.
        // Otherwise we will remove implications that didn't propagate yet!
        for (const Literal lit : implications_[a]) {
          DCHECK(trail_->Assignment().LiteralIsTrue(lit));
        }
      }

      gtl::STLClearObject(&(implications_[a]));
      continue;
    }

    int new_size = 0;
    auto& implication = implications_[a];
    for (const Literal l : implication) {
      if (!is_removed_[l] && !assignment.LiteralIsTrue(l)) {
        implication[new_size++] = l;
      }
    }
    implication.resize(new_size);
  }

  // Clean-up at most ones.
  at_most_ones_.clear();
  CleanUpAndAddAtMostOnes(/*base_index=*/0);

  // Note that to please the invariant() we also removed fixed literal above.
  DCHECK(InvariantsAreOk());
}

bool BinaryImplicationGraph::InvariantsAreOk() {
  if (time_limit_->LimitReached()) return true;
  // We check that if a => b then not(b) => not(a).
  absl::flat_hash_set<std::pair<LiteralIndex, LiteralIndex>> seen;
  int num_redundant = 0;
  int num_fixed = 0;
  for (LiteralIndex a_index(0); a_index < implications_.size(); ++a_index) {
    if (trail_->Assignment().LiteralIsAssigned(Literal(a_index))) {
      ++num_fixed;
      if (!implications_[a_index].empty()) {
        LOG(ERROR) << "Fixed literal has non-cleared implications";
        return false;
      }
      continue;
    }
    if (is_removed_[a_index]) {
      if (!implications_[a_index].empty()) {
        LOG(ERROR) << "Removed literal has non-cleared implications";
        return false;
      }
      continue;
    }
    if (is_redundant_[a_index]) {
      ++num_redundant;
      if (implications_[a_index].size() != 1) {
        LOG(ERROR)
            << "Redundant literal should only point to its representative "
            << Literal(a_index) << " => " << implications_[a_index];
        return false;
      }
    }
    for (const Literal b : implications_[a_index]) {
      seen.insert({a_index, b.Index()});
    }
  }

  // Check that reverse topo order is correct.
  util_intops::StrongVector<LiteralIndex, int> lit_to_order;
  if (is_dag_) {
    lit_to_order.assign(implications_.size(), -1);
    for (int i = 0; i < reverse_topological_order_.size(); ++i) {
      lit_to_order[reverse_topological_order_[i]] = i;
    }
  }

  VLOG(2) << "num_redundant " << num_redundant;
  VLOG(2) << "num_fixed " << num_fixed;
  for (LiteralIndex a_index(0); a_index < implications_.size(); ++a_index) {
    const LiteralIndex not_a_index = Literal(a_index).NegatedIndex();
    for (const Literal b : implications_[a_index]) {
      if (is_removed_[b]) {
        LOG(ERROR) << "A removed literal still appear! " << Literal(a_index)
                   << " => " << b;
        return false;
      }

      if (!seen.contains({b.NegatedIndex(), not_a_index})) {
        LOG(ERROR) << "We have " << Literal(a_index) << " => " << b
                   << " but not the reverse implication!";
        LOG(ERROR) << "redundant[a]: " << is_redundant_[a_index]
                   << " assigned[a]: "
                   << trail_->Assignment().LiteralIsAssigned(Literal(a_index))
                   << " removed[a]: " << is_removed_[a_index]
                   << " redundant[b]: " << is_redundant_[b] << " assigned[b]: "
                   << trail_->Assignment().LiteralIsAssigned(b)
                   << " removed[b]: " << is_removed_[b];

        return false;
      }

      // Test that if we have a dag, our topo order is correct.
      if (is_dag_ && !is_redundant_[b] && !is_redundant_[a_index]) {
        DCHECK_NE(lit_to_order[b], -1);
        DCHECK_LE(lit_to_order[b], lit_to_order[a_index]);
      }
    }
  }

  // Check the at-most ones.
  absl::flat_hash_set<std::pair<LiteralIndex, int>> lit_to_start;
  for (LiteralIndex i(0); i < at_most_ones_.size(); ++i) {
    for (const int start : at_most_ones_[i]) {
      lit_to_start.insert({i, start});
    }
  }

  for (int start = 0; start < at_most_one_buffer_.size();) {
    const absl::Span<const Literal> amo = AtMostOne(start);
    for (const Literal l : amo) {
      if (is_removed_[l]) {
        LOG(ERROR) << "A removed literal still appear in an amo " << l;
        return false;
      }
      if (!lit_to_start.contains({l, start})) {
        return false;
      }
    }
    start += amo.size() + 1;
  }

  return true;
}

absl::Span<const Literal> BinaryImplicationGraph::NextAtMostOne() {
  if (at_most_one_iterator_ >= at_most_one_buffer_.size()) {
    return absl::Span<const Literal>();
  }

  const absl::Span<const Literal> result = AtMostOne(at_most_one_iterator_);
  at_most_one_iterator_ += result.size() + 1;
  return result;
}

// ----- SatClause -----

// static
SatClause* SatClause::Create(absl::Span<const Literal> literals) {
  DCHECK_GE(literals.size(), 2);
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
  DCHECK(!IsRemoved());
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
