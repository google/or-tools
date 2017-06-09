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

#include "ortools/sat/clause.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/sysinfo.h"
#include "ortools/base/join.h"
#include "ortools/base/stl_util.h"
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

// Removes dettached clauses from a watcher list.
template <typename Watcher>
bool CleanUpPredicate(const Watcher& watcher) {
  return !watcher.clause->IsAttached();
}

}  // namespace

// ----- LiteralWatchers -----

LiteralWatchers::LiteralWatchers()
    : SatPropagator("LiteralWatchers"),
      is_clean_(true),
      num_inspected_clauses_(0),
      num_inspected_clause_literals_(0),
      num_watched_clauses_(0),
      stats_("LiteralWatchers") {}

LiteralWatchers::~LiteralWatchers() {
  IF_STATS_ENABLED(LOG(INFO) << stats_.StatString());
}

void LiteralWatchers::Resize(int num_variables) {
  DCHECK(is_clean_);
  watchers_on_false_.resize(num_variables << 1);
  reasons_.resize(num_variables);
  needs_cleaning_.Resize(LiteralIndex(num_variables << 1));
  statistics_.resize(num_variables);
}

// Note that this is the only place where we add Watcher so the DCHECK
// guarantees that there are no duplicates.
void LiteralWatchers::AttachOnFalse(Literal a, Literal b, SatClause* clause) {
  SCOPED_TIME_STAT(&stats_);
  DCHECK(is_clean_);
  DCHECK(!WatcherListContains(watchers_on_false_[a.Index()], *clause));
  watchers_on_false_[a.Index()].push_back(Watcher(clause, b));
}

bool LiteralWatchers::PropagateOnFalse(Literal false_literal, Trail* trail) {
  SCOPED_TIME_STAT(&stats_);
  DCHECK(is_clean_);
  std::vector<Watcher>& watchers = watchers_on_false_[false_literal.Index()];
  const VariablesAssignment& assignment = trail->Assignment();

  // Note(user): It sounds better to inspect the list in order, this is because
  // small clauses like binary or ternary clauses will often propagate and thus
  // stay at the beginning of the list.
  std::vector<Watcher>::iterator new_it = watchers.begin();
  for (std::vector<Watcher>::iterator it = watchers.begin();
       it != watchers.end(); ++it) {
    // Don't even look at the clause memory if the blocking literal is true.
    if (assignment.LiteralIsTrue(it->blocking_literal)) {
      *new_it++ = *it;
      continue;
    }
    ++num_inspected_clauses_;

    // If the other watched literal is true, just change the blocking literal.
    Literal* literals = it->clause->literals();
    const Literal other_watched_literal =
        (literals[1] == false_literal) ? literals[0] : literals[1];
    if (other_watched_literal != it->blocking_literal &&
        assignment.LiteralIsTrue(other_watched_literal)) {
      *new_it++ = Watcher(it->clause, other_watched_literal);
      ++num_inspected_clause_literals_;
      continue;
    }

    // Look for another literal to watch.
    {
      int i = 2;
      const int size = it->clause->Size();
      while (i < size && assignment.LiteralIsFalse(literals[i])) ++i;
      num_inspected_clause_literals_ += i;
      if (i < size) {
        // literal[i] is undefined or true, it's now the new literal to watch.
        // Note that by convention, we always keep the two watched literals at
        // the beginning of the clause.
        literals[0] = other_watched_literal;
        literals[1] = literals[i];
        literals[i] = false_literal;
        AttachOnFalse(literals[1], other_watched_literal, it->clause);
        continue;
      }
    }

    // At this point other_watched_literal is either false or undefined, all
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
      // Propagation: other_watched_literal is undefined, set it to true and
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
  num_inspected_clause_literals_ += watchers.size();
  watchers.erase(new_it, watchers.end());
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

gtl::Span<Literal> LiteralWatchers::Reason(const Trail& trail,
                                                  int trail_index) const {
  return reasons_[trail_index]->PropagationReason();
}

SatClause* LiteralWatchers::ReasonClause(int trail_index) const {
  return reasons_[trail_index];
}

bool LiteralWatchers::AttachAndPropagate(SatClause* clause, Trail* trail) {
  SCOPED_TIME_STAT(&stats_);
  ++num_watched_clauses_;
  // Updating the statistics for each learned clause take quite a lot of time
  // (like 6% of the total running time). So for now, we just compute the
  // statistics depending on the problem clauses. Note that this do not take
  // into account the other type of constraint though.
  //
  // TODO(user): This is actually not used a lot with the default parameters.
  // Find a better way for the "initial" polarity choice and tie breaking
  // between literal choices. Note that it seems none of the modern SAT solver
  // relies on this.
  if (!clause->IsRedundant()) UpdateStatistics(*clause, /*added=*/true);
  clause->SortLiterals(statistics_, parameters_);
  return clause->AttachAndEnqueuePotentialUnitPropagation(trail, this);
}

void LiteralWatchers::LazyDetach(SatClause* clause) {
  SCOPED_TIME_STAT(&stats_);
  --num_watched_clauses_;
  if (!clause->IsRedundant()) UpdateStatistics(*clause, /*added=*/false);
  clause->LazyDetach();
  is_clean_ = false;
  needs_cleaning_.Set(clause->FirstLiteral().Index());
  needs_cleaning_.Set(clause->SecondLiteral().Index());
}

void LiteralWatchers::CleanUpWatchers() {
  SCOPED_TIME_STAT(&stats_);
  for (LiteralIndex index : needs_cleaning_.PositionsSetAtLeastOnce()) {
    DCHECK(needs_cleaning_[index]);
    RemoveIf(&(watchers_on_false_[index]), CleanUpPredicate<Watcher>);
    needs_cleaning_.Clear(index);
  }
  needs_cleaning_.NotifyAllClear();
  is_clean_ = true;
}

void LiteralWatchers::UpdateStatistics(const SatClause& clause, bool added) {
  SCOPED_TIME_STAT(&stats_);
  for (const Literal literal : clause) {
    const BooleanVariable var = literal.Variable();
    const int direction = added ? 1 : -1;
    statistics_[var].num_appearances += direction;
    statistics_[var].weighted_num_appearances +=
        1.0 / clause.Size() * direction;
    if (literal.IsPositive()) {
      statistics_[var].num_positive_clauses += direction;
    } else {
      statistics_[var].num_negative_clauses += direction;
    }
  }
}

// ----- BinaryImplicationGraph -----

void BinaryImplicationGraph::Resize(int num_variables) {
  SCOPED_TIME_STAT(&stats_);
  implications_.resize(num_variables << 1);
  reasons_.resize(num_variables);
}

void BinaryImplicationGraph::AddBinaryClause(Literal a, Literal b) {
  SCOPED_TIME_STAT(&stats_);
  implications_[a.Negated().Index()].push_back(b);
  implications_[b.Negated().Index()].push_back(a);
  ++num_implications_;
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
      // But the performance where not too good...
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

gtl::Span<Literal> BinaryImplicationGraph::Reason(
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
  const VariablesAssignment& assigment = trail.Assignment();
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
    STLClearObject(&(implications_[true_literal.Index()]));
    STLClearObject(&(implications_[true_literal.NegatedIndex()]));
  }
  for (const LiteralIndex i : is_marked_.PositionsSetAtLeastOnce()) {
    RemoveIf(&implications_[i],
             std::bind1st(std::mem_fun(&VariablesAssignment::LiteralIsTrue),
                          &assigment));
  }
}

// ----- SatClause -----

// static
SatClause* SatClause::Create(const std::vector<Literal>& literals,
                             bool is_redundant) {
  CHECK_GE(literals.size(), 2);
  SatClause* clause = reinterpret_cast<SatClause*>(
      ::operator new(sizeof(SatClause) + literals.size() * sizeof(Literal)));
  clause->size_ = literals.size();
  for (int i = 0; i < literals.size(); ++i) {
    clause->literals_[i] = literals[i];
  }
  clause->is_redundant_ = is_redundant;
  clause->is_attached_ = false;
  return clause;
}

// Note that for an attached clause, removing fixed literal is okay because if
// any of the watched literal is assigned, then the clause is necessarily true.
bool SatClause::RemoveFixedLiteralsAndTestIfTrue(
    const VariablesAssignment& assignment) {
  DCHECK(is_attached_);
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

namespace {

// Support struct to sort literals for ordering.
struct WeightedLiteral {
  WeightedLiteral(Literal l, int w) : literal(l), weight(w) {}

  Literal literal;
  int weight;
};

// Lexical order, by smaller weight, then by smaller literal to break ties.
bool LiteralWithSmallerWeightFirst(const WeightedLiteral& wv1,
                                   const WeightedLiteral& wv2) {
  return (wv1.weight < wv2.weight) ||
         (wv1.weight == wv2.weight &&
          wv1.literal.SignedValue() < wv2.literal.SignedValue());
}

// Lexical order, by larger weight, then by smaller literal to break ties.
bool LiteralWithLargerWeightFirst(const WeightedLiteral& wv1,
                                  const WeightedLiteral& wv2) {
  return (wv1.weight > wv2.weight) ||
         (wv1.weight == wv2.weight &&
          wv1.literal.SignedValue() < wv2.literal.SignedValue());
}

}  // namespace

void SatClause::SortLiterals(
    const ITIVector<BooleanVariable, VariableInfo>& statistics,
    const SatParameters& parameters) {
  DCHECK(!IsAttached());
  const SatParameters::LiteralOrdering literal_order =
      parameters.literal_ordering();
  if (literal_order != SatParameters::LITERAL_IN_ORDER) {
    std::vector<WeightedLiteral> order;
    for (Literal literal : *this) {
      int weight = literal.IsPositive()
                       ? statistics[literal.Variable()].num_positive_clauses
                       : statistics[literal.Variable()].num_negative_clauses;
      order.push_back(WeightedLiteral(literal, weight));
    }
    switch (literal_order) {
      case SatParameters::VAR_MIN_USAGE: {
        std::sort(order.begin(), order.end(), LiteralWithSmallerWeightFirst);
        break;
      }
      case SatParameters::VAR_MAX_USAGE: {
        std::sort(order.begin(), order.end(), LiteralWithLargerWeightFirst);
        break;
      }
      default: { break; }
    }
    for (int i = 0; i < order.size(); ++i) {
      literals_[i] = order[i].literal;
    }
  }
}

bool SatClause::AttachAndEnqueuePotentialUnitPropagation(
    Trail* trail, LiteralWatchers* demons) {
  CHECK(!IsAttached());
  // Select the first two literals that are not assigned to false and put them
  // on position 0 and 1.
  int num_literal_not_false = 0;
  for (int i = 0; i < size_; ++i) {
    if (!trail->Assignment().LiteralIsFalse(literals_[i])) {
      std::swap(literals_[i], literals_[num_literal_not_false]);
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
    int max_level = trail->Info(literals_[1].Variable()).level;
    for (int i = 2; i < size_; ++i) {
      const int level = trail->Info(literals_[i].Variable()).level;
      if (level > max_level) {
        max_level = level;
        std::swap(literals_[1], literals_[i]);
      }
    }

    // Propagates literals_[0] if it is undefined.
    if (!trail->Assignment().LiteralIsTrue(literals_[0])) {
      demons->SetReasonClause(trail->Index(), this);
      trail->Enqueue(literals_[0], demons->propagator_id_);
    }
  }

  // Attach the watchers.
  is_attached_ = true;
  demons->AttachOnFalse(literals_[0], literals_[1], this);
  demons->AttachOnFalse(literals_[1], literals_[0], this);
  return true;
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
