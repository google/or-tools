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
#include "sat/clause.h"

#include <algorithm>
#include <functional>
#include "base/unique_ptr.h"
#include <string>
#include <vector>

#include "base/integral_types.h"
#include "base/logging.h"
#include "base/sysinfo.h"
#include "base/join.h"
#include "util/time_limit.h"
#include "base/stl_util.h"

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
    : is_clean_(true),
      num_inspected_clauses_(0),
      num_watched_clauses_(0),
      stats_("LiteralWatchers") {}

LiteralWatchers::~LiteralWatchers() {
  IF_STATS_ENABLED(LOG(INFO) << stats_.StatString());
}

void LiteralWatchers::Resize(int num_variables) {
  DCHECK(is_clean_);
  watchers_on_false_.resize(num_variables << 1);
  needs_cleaning_.resize(num_variables << 1, false);
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
  int new_index = 0;

  // Note(user): It sounds better to inspect the list in order, this is because
  // small clauses like binary or ternary clauses will often propagate and thus
  // stay at the beginning of the list.
  const int initial_size = watchers.size();
  for (int i = 0; i < initial_size; ++i) {
    ++num_inspected_clauses_;

    // Don't even look at the clause memory if the blocking literal is true.
    if (assignment.IsLiteralTrue(watchers[i].blocking_literal)) {
      watchers[new_index] = watchers[i];
      ++new_index;
      continue;
    }

    SatClause* clause = watchers[i].clause;
    if (!clause->PropagateOnFalse(false_literal, trail)) {
      // Conflict: All literals of this clause are false.
      memmove(&watchers[new_index], &watchers[i],
              (initial_size - i) * sizeof(Watcher));
      watchers.resize(new_index + initial_size - i);
      return false;
    }

    // Update the watched literal if clause->FirstLiteral() changed.
    // See the contract of PropagateOnFalse().
    if (clause->FirstLiteral() != false_literal) {
      AttachOnFalse(clause->FirstLiteral(), clause->SecondLiteral(), clause);
    } else {
      watchers[new_index] = Watcher(clause, clause->SecondLiteral());
      ++new_index;
    }
  }
  watchers.resize(new_index);
  return true;
}

bool LiteralWatchers::AttachAndPropagate(SatClause* clause, Trail* trail) {
  SCOPED_TIME_STAT(&stats_);
  ++num_watched_clauses_;
  UpdateStatistics(*clause, /*added=*/true);
  clause->SortLiterals(statistics_, parameters_);
  return clause->AttachAndEnqueuePotentialUnitPropagation(trail, this);
}

void LiteralWatchers::LazyDetach(SatClause* clause) {
  SCOPED_TIME_STAT(&stats_);
  --num_watched_clauses_;
  UpdateStatistics(*clause, /*added=*/false);
  clause->LazyDetach();
  is_clean_ = false;
  needs_cleaning_[clause->FirstLiteral().Index()] = true;
  needs_cleaning_[clause->SecondLiteral().Index()] = true;
}

void LiteralWatchers::CleanUpWatchers() {
  SCOPED_TIME_STAT(&stats_);
  for (int i = 0; i < needs_cleaning_.size(); ++i) {
    if (needs_cleaning_[LiteralIndex(i)]) {
      RemoveIf(&(watchers_on_false_[LiteralIndex(i)]),
               CleanUpPredicate<Watcher>);
      needs_cleaning_[LiteralIndex(i)] = false;
    }
  }
  is_clean_ = true;
}

void LiteralWatchers::UpdateStatistics(const SatClause& clause, bool added) {
  SCOPED_TIME_STAT(&stats_);
  for (const Literal literal : clause) {
    const VariableIndex var = literal.Variable();
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
}

void BinaryImplicationGraph::AddBinaryClause(Literal a, Literal b) {
  SCOPED_TIME_STAT(&stats_);
  implications_[a.Negated().Index()].push_back(b);
  implications_[b.Negated().Index()].push_back(a);
}

void BinaryImplicationGraph::AddBinaryConflict(Literal a, Literal b,
                                               Trail* trail) {
  SCOPED_TIME_STAT(&stats_);
  AddBinaryClause(a, b);
  if (trail->Assignment().IsLiteralFalse(a)) {
    trail->EnqueueWithBinaryReason(b, a);
  } else if (trail->Assignment().IsLiteralFalse(b)) {
    trail->EnqueueWithBinaryReason(a, b);
  }
}

bool BinaryImplicationGraph::PropagateOnTrue(Literal true_literal,
                                             Trail* trail) {
  SCOPED_TIME_STAT(&stats_);
  const VariablesAssignment& assignment = trail->Assignment();
  for (Literal literal : implications_[true_literal.Index()]) {
    if (assignment.IsLiteralTrue(literal)) {
      // Note(user): I tried to update the reason here if the literal was
      // enqueued after the true_literal on the trail. This property is
      // important for ComputeFirstUIPConflict() to work since it needs the
      // trail order to be a topological order for the deduction graph.
      // But the performance where not too good...
      continue;
    }

    ++num_propagations_;
    if (assignment.IsLiteralFalse(literal)) {
      // Conflict.
      temporary_clause_[0] = true_literal.Negated();
      temporary_clause_[1] = literal;
      trail->SetFailingClause(
          ClauseRef(&temporary_clause_[0], &temporary_clause_[0] + 2));
      return false;
    } else {
      // Propagation.
      trail->EnqueueWithBinaryReason(literal, true_literal.Negated());
    }
  }
  return true;
}

void BinaryImplicationGraph::MinimizeClause(const Trail& trail,
                                            std::vector<Literal>* conflict) {
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
  // TODO(user): no need to explore the unique literal of the current decision
  // level since it can't be removed.
  int index = 0;
  for (int i = 0; i < conflict->size(); ++i) {
    const Literal lit = (*conflict)[i];
    const int lit_level = trail.Info(lit.Variable()).level;
    bool keep_literal = true;
    for (Literal implied : implications_[lit.Index()]) {
      if (is_marked_[implied.Index()]) {
        DCHECK_LE(lit_level, trail.Info(implied.Variable()).level);
        if (lit_level == trail.Info(implied.Variable()).level &&
            is_removed_[implied.Index()])
          continue;
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
    const VariablesAssignment& assigment) {
  SCOPED_TIME_STAT(&stats_);
  is_marked_.ClearAndResize(LiteralIndex(implications_.size()));
  for (LiteralIndex i(0); i < implications_.size(); ++i) {
    if (assigment.IsLiteralTrue(Literal(i))) {
      // If b is true and a -> b then because not b -> not a, all the
      // implications list that contains b will be marked by this process.
      for (Literal lit : implications_[Literal(i).NegatedIndex()]) {
        is_marked_.Set(lit.NegatedIndex());
      }
      STLClearObject(&(implications_[i]));
      STLClearObject(&(implications_[Literal(i).NegatedIndex()]));
    }
  }
  for (LiteralIndex i(0); i < implications_.size(); ++i) {
    if (is_marked_[i]) {
      RemoveIf(&implications_[i],
               std::bind1st(std::mem_fun(&VariablesAssignment::IsLiteralTrue),
                            &assigment));
    }
  }
}

// ----- SatClause -----

// static
SatClause* SatClause::Create(const std::vector<Literal>& literals, ClauseType type,
                             ResolutionNode* node) {
  CHECK_GE(literals.size(), 2);
  SatClause* clause = reinterpret_cast<SatClause*>(
      ::operator new(sizeof(SatClause) + literals.size() * sizeof(Literal)));
  clause->size_ = literals.size();
  for (int i = 0; i < literals.size(); ++i) {
    clause->literals_[i] = literals[i];
  }
  clause->is_learned_ = (type == LEARNED_CLAUSE);
  clause->is_attached_ = false;
  clause->activity_ = 0.0;
  clause->lbd_ = 0;
  clause->resolution_node_ = node;
  return clause;
}

// Note that for an attached clause, removing fixed literal is okay because if
// any of them is assigned, then the clause is necessary true.
bool SatClause::RemoveFixedLiteralsAndTestIfTrue(
    const VariablesAssignment& assignment, std::vector<Literal>* removed_literals) {
  removed_literals->clear();
  DCHECK(is_attached_);
  if (assignment.IsVariableAssigned(literals_[0].Variable()) ||
      assignment.IsVariableAssigned(literals_[1].Variable())) {
    DCHECK(IsSatisfied(assignment));
    return true;
  }
  int j = 2;
  for (int i = 2; i < size_; ++i) {
    if (assignment.IsVariableAssigned(literals_[i].Variable())) {
      if (assignment.IsLiteralTrue(literals_[i])) return true;
      removed_literals->push_back(literals_[i]);
    } else {
      literals_[j] = literals_[i];
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
    const ITIVector<VariableIndex, VariableInfo>& statistics,
    const SatParameters& parameters) {
  CHECK(!IsAttached());
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
    if (!trail->Assignment().IsLiteralFalse(literals_[i])) {
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
    // the false literal with the highest decision levels.
    int max_level = trail->Info(literals_[1].Variable()).level;
    for (int i = 2; i < size_; ++i) {
      const int level = trail->Info(literals_[i].Variable()).level;
      if (level > max_level) {
        max_level = level;
        std::swap(literals_[1], literals_[i]);
      }
    }

    // If there is a propagation, make literals_[1] the propagated literal and
    // enqueue it.
    if (!trail->Assignment().IsLiteralTrue(literals_[0])) {
      std::swap(literals_[0], literals_[1]);
      trail->EnqueueWithSatClauseReason(literals_[1], this);
    }
  }

  // Attach the watchers.
  is_attached_ = true;
  demons->AttachOnFalse(literals_[0], literals_[1], this);
  demons->AttachOnFalse(literals_[1], literals_[0], this);
  return true;
}

// Propagates one watched literal becoming false. This method maintains the
// invariant that watched literals are always in position 0 and 1.
bool SatClause::PropagateOnFalse(Literal watched_literal, Trail* trail) {
  const VariablesAssignment& assignment = trail->Assignment();
  DCHECK(IsAttached());
  DCHECK_GE(size_, 2);
  DCHECK(assignment.IsLiteralFalse(watched_literal));

  // The instantiated literal should be in position 0.
  if (literals_[1] == watched_literal) {
    literals_[1] = literals_[0];
    literals_[0] = watched_literal;
  }
  DCHECK_EQ(literals_[0], watched_literal);

  // If the other watched literal is true, do nothing.
  if (assignment.IsLiteralTrue(literals_[1])) return true;

  for (int i = 2; i < size_; ++i) {
    if (assignment.IsLiteralFalse(literals_[i])) continue;

    // Note(user): If the value of literals_[i] is true, it is possible to leave
    // the watched literal unchanged. However this seems less efficient. Even if
    // we swap it with the literal at position 2 to speed up future checks.

    // literal[i] is undefined or true, it's now the new literal to watch.
    literals_[0] = literals_[i];
    literals_[i] = watched_literal;
    return true;
  }

  // Literals_[1] is either false or undefined, all other literals are false.
  if (assignment.IsLiteralFalse(literals_[1])) {
    trail->SetFailingSatClause(ToClauseRef(), this);
    trail->SetFailingResolutionNode(resolution_node_);
    return false;
  }

  // Literals_[1] is undefined, set it to true.
  trail->EnqueueWithSatClauseReason(literals_[1], this);
  return true;
}

bool SatClause::IsSatisfied(const VariablesAssignment& assignment) const {
  for (const Literal literal : *this) {
    if (assignment.IsLiteralTrue(literal)) return true;
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
