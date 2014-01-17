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
#include <algorithm>
#include <string>
#include <vector>

#include "base/integral_types.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/concise_iterator.h"
#include "base/stl_util.h"
#include "sat/sat_solver.h"

namespace operations_research {
namespace sat {

// This method will compute a first UIP conflict
//   http://www.cs.tau.ac.il/~msagiv/courses/ATP/iccad2001_final.pdf
//   http://gauss.ececs.uc.edu/SAT/articles/FAIA185-0131.pdf
void SatSolver::ComputeFirstUIPConflict(
    ClauseRef failing_clause, std::vector<Literal>* conflict,
    std::vector<Literal>* discarded_last_level_literals) {
  SCOPED_TIME_STAT(&stats_);

  // This will be used to mark all the literals inspected while we process the
  // conflict and the reasons behind each of its variable assignments.
  is_marked_.ClearAndResize(num_variables_);

  conflict->clear();
  discarded_last_level_literals->clear();
  const int current_level = CurrentDecisionLevel();
  int num_literal_at_current_level_that_needs_to_be_processed = 0;
  DCHECK_GT(current_level, 0);

  // To find the 1-UIP conflict clause, we start by the failing_clause, and
  // expand each of its literal using the reason for this literal assignement to
  // false. The is_marked_ set allow us to never expand the same literal twice.
  //
  // The expansion is not done (i.e. stop) for literal that where assigned at a
  // decision level below the current one. If the level of such literal is not
  // zero, it is added to the conflict clause.
  //
  // Now, the trick is that we use the trail to expand the literal of the
  // current level in a very specific order. Namely the reverse order of the one
  // in which they where infered. We stop as soon as
  // num_literal_at_current_level_that_needs_to_be_processed is exactly one.
  //
  // This last literal will be the first UIP because by definition all the
  // propagation done at the current level will pass though it at some point.
  ClauseRef clause_to_expand = failing_clause;
  DCHECK(!clause_to_expand.IsEmpty());
  int trail_index = trail_.Index() - 1;
  while (true) {
    for (const Literal literal : clause_to_expand) {
      const VariableIndex var = literal.Variable();
      if (!is_marked_[var]) {
        is_marked_.Set(var);
        const int level = DecisionLevel(var);
        if (level == current_level) {
          ++num_literal_at_current_level_that_needs_to_be_processed;
        } else if (level > 0) {
          // Note that all these literals are currently false since the clause
          // to expand was used to infer the value of a literal at this level.
          DCHECK(trail_.Assignment().IsLiteralFalse(literal));
          conflict->push_back(literal);
        }
      }
    }

    // Find next marked literal to expand from the trail.
    DCHECK_GT(num_literal_at_current_level_that_needs_to_be_processed, 0);
    while (!is_marked_[trail_[trail_index].Variable()]) {
      --trail_index;
      DCHECK_GE(trail_index, 0);
      DCHECK_EQ(DecisionLevel(trail_[trail_index].Variable()), current_level);
    }

    if (num_literal_at_current_level_that_needs_to_be_processed == 1) {
      // We have the first UIP. Add its negation to the conflict clause.
      // This way, after backtracking to the proper level, the conflict clause
      // will be unit, and infer the negation of the UIP that caused the fail.
      conflict->push_back(trail_[trail_index].Negated());
      break;
    }

    const Literal literal = trail_[trail_index];
    discarded_last_level_literals->push_back(literal);

    // If we already encountered the same reason, we can just skip this literal
    // which is what setting clause_to_expand to the empty clause do.
    if (reason_cache_.FirstVariableWithSameReason(literal.Variable()) !=
        literal.Variable()) {
      clause_to_expand = ClauseRef();
    } else {
      clause_to_expand = Reason(literal.Variable());
      DCHECK(!clause_to_expand.IsEmpty());
    }

    --num_literal_at_current_level_that_needs_to_be_processed;
    --trail_index;
  }
}

void SatSolver::MinimizeConflict(std::vector<Literal>* conflict) {
  SCOPED_TIME_STAT(&stats_);
  const int old_size = conflict->size();
  switch (parameters_.minimization_algorithm()) {
    case SatParameters::NONE:
      return;
    case SatParameters::SIMPLE: {
      MinimizeConflictSimple(conflict);
      break;
    }
    case SatParameters::RECURSIVE: {
      MinimizeConflictRecursively(conflict);
      break;
    }
    case SatParameters::EXPERIMENTAL: {
      MinimizeConflictExperimental(conflict);
      break;
    }
  }
  if (conflict->size() < old_size) {
    ++counters_.num_minimizations;
    counters_.num_literals_removed += old_size - conflict->size();
  }
}

// This simple version just looks for any literal that is directly infered by
// other literals of the conflict. It is directly infered if the literals of its
// reason clause are either from level 0 or from the conflict itself.
//
// Note that because of the assignement struture, there is no need to process
// the literals of the conflict in order. While exploring the reason for a
// literal assignement, there will be no cycles.
void SatSolver::MinimizeConflictSimple(std::vector<Literal>* conflict) {
  SCOPED_TIME_STAT(&stats_);
  is_marked_.ClearAndResize(num_variables_);
  for (Literal literal : *conflict) {
    is_marked_.Set(literal.Variable());
  }
  int index = 0;
  const int current_level = CurrentDecisionLevel();
  for (int i = 0; i < conflict->size(); ++i) {
    const VariableIndex var = (*conflict)[i].Variable();
    bool can_be_removed = false;
    if (DecisionLevel(var) != current_level) {
      // It is important not to call Reason(var) when it can be avoided.
      const ClauseRef reason = Reason(var);
      if (!reason.IsEmpty()) {
        can_be_removed = true;
        for (Literal literal : reason) {
          if (DecisionLevel(literal.Variable()) == 0) continue;
          if (!is_marked_[literal.Variable()]) {
            can_be_removed = false;
            break;
          }
        }
      }
    }
    if (!can_be_removed) {
      (*conflict)[index] = (*conflict)[i];
      ++index;
    }
  }
  conflict->erase(conflict->begin() + index, conflict->end());
}

// This is similar to MinimizeConflictSimple() except that for each literal of
// the conflict, the literals of its reason are recursively expended using their
// reason and so on. The recusion stop until we show that the initial literal
// can be infered from the conflict variables alone, or if we show that this is
// not the case. The result of any variable expension will be cached in order
// not to be expended again.
void SatSolver::MinimizeConflictRecursively(std::vector<Literal>* conflict) {
  SCOPED_TIME_STAT(&stats_);

  // is_marked_ will contains all the conflict literals plus the literals that
  // have been shown to depends only on the conflict literals. is_independent_
  // will contains the literals that have been shown NOT to depends only on the
  // conflict literals. The too set are exclusive for non-conflict literals, but
  // a conflict literal (which is always marked) can be independent if we showed
  // that it can't be removed from the clause.
  //
  // Optimization: There is no need to call is_marked_.ClearAndResize() or to
  // mark the conflict literals since this was already done by
  // ComputeFirstUIPConflict().
  is_independent_.ClearAndResize(num_variables_);

  // min_trail_index_per_level_ will always be reset to all
  // std::numeric_limits<int>::max() at the end. This is used to prune the
  // search because any literal at a given level with an index smaller or equal
  // to min_trail_index_per_level_[level] can't be redundant.
  if (CurrentDecisionLevel() >= min_trail_index_per_level_.size()) {
    min_trail_index_per_level_.resize(CurrentDecisionLevel() + 1,
                                      std::numeric_limits<int>::max());
  }

  // Compute the number of variable at each decision levels. This will be used
  // to pruned the DFS because we know that the minimized conflict will have at
  // least one variable of each decision levels. Because such variable can't be
  // eliminated using lower decision levels variable otherwise it will have been
  // propagated.
  for (Literal literal : *conflict) {
    const VariableIndex var = literal.Variable();
    const int level = DecisionLevel(var);
    min_trail_index_per_level_[level] =
        std::min(min_trail_index_per_level_[level], trail_.Info(var).trail_index);
  }

  // Remove the redundant variable from the conflict. That is the ones that can
  // be infered by some other variables in the conflict.
  int index = 0;
  for (int i = 0; i < conflict->size(); ++i) {
    const VariableIndex var = (*conflict)[i].Variable();
    if (trail_.Info(var).trail_index <=
            min_trail_index_per_level_[DecisionLevel(var)] ||
        !CanBeInferedFromConflictVariables(var)) {
      // Mark the conflict variable as independent. Note that is_marked_[var]
      // will still be true.
      is_independent_.Set(var);
      (*conflict)[index] = (*conflict)[i];
      ++index;
    }
  }

  // Reset min_trail_index_per_level_. This works since we can never eliminate
  // all the literals from the same level.
  conflict->resize(index);
  for (Literal literal : *conflict) {
    min_trail_index_per_level_[DecisionLevel(literal.Variable())] =
        std::numeric_limits<int>::max();
  }
}

bool SatSolver::CanBeInferedFromConflictVariables(VariableIndex variable) {
  // Test for an already processed variable with the same reason.
  {
    DCHECK(is_marked_[variable]);
    const VariableIndex v = reason_cache_.FirstVariableWithSameReason(variable);
    if (v != variable) return !is_independent_[v];
  }

  // This function implement an iterative DFS from the given variable. It uses
  // the reason clause as adjacency lists. dfs_stack_ can be seens as the
  // recursive call stack of the variable we are currently processing. All its
  // adjacent variable will be pushed into variable_to_process_, and we will
  // then dequeue them one by one and process them.
  dfs_stack_.assign(1, variable);
  variable_to_process_.assign(1, variable);

  // First we expand the reason for the given variable.
  DCHECK(!Reason(variable).IsEmpty());
  for (Literal literal : Reason(variable)) {
    const VariableIndex var = literal.Variable();
    if (var == variable) continue;
    const int level = DecisionLevel(var);
    if (level == 0 || is_marked_[var]) continue;
    if (trail_.Info(var).trail_index <= min_trail_index_per_level_[level] ||
        is_independent_[var])
      return false;
    variable_to_process_.push_back(var);
  }

  // Then we start the DFS.
  while (!variable_to_process_.empty()) {
    const VariableIndex current_var = variable_to_process_.back();
    if (current_var == dfs_stack_.back()) {
      // We finished the DFS of the variable dfs_stack_.back(), this can be seen
      // as a recursive call terminating.
      if (dfs_stack_.size() > 1) {
        DCHECK(!is_marked_[current_var]);
        is_marked_.Set(current_var);
      }
      variable_to_process_.pop_back();
      dfs_stack_.pop_back();
      continue;
    }

    // If this variable became marked since the we pushed it, we can skip it.
    if (is_marked_[current_var]) {
      variable_to_process_.pop_back();
      continue;
    }

    // This case will never be encountered since we abort right away as soon
    // as an independent variable is found.
    DCHECK(!is_independent_[current_var]);

    // Test for an already processed variable with the same reason.
    {
      const VariableIndex v =
          reason_cache_.FirstVariableWithSameReason(current_var);
      if (v != current_var) {
        if (is_independent_[v]) break;
        DCHECK(is_marked_[v]);
        variable_to_process_.pop_back();
        continue;
      }
    }

    // Expand the variable. This can be seen as making a recursive call.
    dfs_stack_.push_back(current_var);
    bool abort_early = false;
    DCHECK(!Reason(current_var).IsEmpty());
    for (Literal literal : Reason(current_var)) {
      const VariableIndex var = literal.Variable();
      if (var == current_var) continue;
      const int level = DecisionLevel(var);
      if (level == 0 || is_marked_[var]) continue;
      if (trail_.Info(var).trail_index <= min_trail_index_per_level_[level] ||
          is_independent_[var]) {
        abort_early = true;
        break;
      }
      variable_to_process_.push_back(var);
    }
    if (abort_early) break;
  }

  // All the variable left on the dfs_stack_ are independent.
  for (const VariableIndex var : dfs_stack_) {
    is_independent_.Set(var);
  }
  return dfs_stack_.empty();
}

namespace {

struct WeightedVariable {
  WeightedVariable(VariableIndex v, int w) : var(v), weight(w) {}

  VariableIndex var;
  int weight;
};

// Lexical order, by larger weight, then by smaller variable number
// to break ties
struct VariableWithLargerWeightFirst {
  bool operator()(const WeightedVariable& wv1,
                  const WeightedVariable& wv2) const {
    return (wv1.weight > wv2.weight ||
            (wv1.weight == wv2.weight && wv1.var < wv2.var));
  }
};
}  // namespace.

// This function allows a conflict variable to be replaced by another variable
// not originally in the conflict. Greater reduction and backtracking can be
// achieved this way, but the effect of this is not clear.
//
// TODO(user): More investigation needed. This seems to help on the Hanoi
// problems, but degrades performance on others.
//
// TODO(user): Find a reference for this? neither minisat nor glucose do that,
// they just do MinimizeConflictRecursively() with a different implementation.
// Note that their behavior also make more sense with the way they (and we) bump
// the variable activities.
void SatSolver::MinimizeConflictExperimental(std::vector<Literal>* conflict) {
  SCOPED_TIME_STAT(&stats_);

  // First, sort the variables in the conflict by decreasing decision levels.
  // Also initialize is_marked_ to true for all conflict variables.
  is_marked_.ClearAndResize(num_variables_);
  const int current_level = CurrentDecisionLevel();
  std::vector<WeightedVariable> variables_sorted_by_level;
  for (Literal literal : *conflict) {
    const VariableIndex var = literal.Variable();
    is_marked_.Set(var);
    const int level = DecisionLevel(var);
    if (level < current_level) {
      variables_sorted_by_level.push_back(WeightedVariable(var, level));
    }
  }
  std::sort(variables_sorted_by_level.begin(), variables_sorted_by_level.end(),
       VariableWithLargerWeightFirst());

  // Then process the reason of the variable with highest level first.
  std::vector<VariableIndex> to_remove;
  for (WeightedVariable weighted_var : variables_sorted_by_level) {
    const VariableIndex var = weighted_var.var;

    // A nullptr reason means that this was a decision variable from the
    // previous levels.
    const ClauseRef reason = Reason(var);
    if (reason.IsEmpty()) continue;

    // Compute how many and which literals from the current reason do not appear
    // in the current conflict. Level 0 literals are ignored.
    std::vector<Literal> not_contained_literals;
    for (const Literal reason_literal : reason) {
      const VariableIndex reason_var = reason_literal.Variable();

      // We ignore level 0 variables.
      if (DecisionLevel(reason_var) == 0) continue;

      // We have a reason literal whose variable is not yet seen.
      // If there is more than one, break right away, we will not minimize the
      // current conflict with this variable.
      if (!is_marked_[reason_var]) {
        not_contained_literals.push_back(reason_literal);
        if (not_contained_literals.size() > 1) break;
      }
    }
    if (not_contained_literals.empty()) {
      // This variable will be deleted from the conflict. Note that we don't
      // unmark it. This is because this variable can be infered from the other
      // variables in the conflict, so it is okay to skip it when processing the
      // reasons of other variables.
      to_remove.push_back(var);
    } else if (not_contained_literals.size() == 1) {
      // Replace the literal from variable var with the only
      // not_contained_literals from the current reason.
      to_remove.push_back(var);
      is_marked_.Set(not_contained_literals.front().Variable());
      conflict->push_back(not_contained_literals.front());
    }
  }

  // Unmark the variable that should be removed from the conflict.
  for (VariableIndex var : to_remove) {
    is_marked_.Clear(var);
  }

  // Remove the now unmarked literals from the conflict.
  int index = 0;
  for (int i = 0; i < conflict->size(); ++i) {
    const Literal literal = (*conflict)[i];
    if (is_marked_[literal.Variable()]) {
      (*conflict)[index] = literal;
      ++index;
    }
  }
  conflict->erase(conflict->begin() + index, conflict->end());
}

namespace {

// Order the clause by increasing LBD (Literal Blocks Distance) first. For the
// same LBD they are ordered by decreasing activity.
bool ClauseOrdering(SatClause* a, SatClause* b) {
  if (a->Lbd() == b->Lbd()) return a->Activity() > b->Activity();
  return a->Lbd() < b->Lbd();
}

}  // namespace

void SatSolver::InitLearnedClauseLimit() {
  const int num_learned_clauses = learned_clauses_.size();
  target_number_of_learned_clauses_ =
      num_learned_clauses + parameters_.clause_cleanup_increment();
  num_learned_clause_before_cleanup_ =
      target_number_of_learned_clauses_ / parameters_.clause_cleanup_ratio() -
      num_learned_clauses;
  VLOG(1) << "reduced learned database to " << num_learned_clauses
          << " clauses. Next cleanup in " << num_learned_clause_before_cleanup_
          << " conflicts.";
}

void SatSolver::CompressLearnedClausesIfNeeded() {
  if (num_learned_clause_before_cleanup_ > 0) return;
  SCOPED_TIME_STAT(&stats_);

  // First time?
  if (learned_clauses_.size() == 0) {
    InitLearnedClauseLimit();
    return;
  }

  // Move the clause that should be kept at the beginning and sort the other
  // using the ClauseOrdering order.
  std::vector<SatClause*>::iterator clause_to_keep_end = std::partition(
      learned_clauses_.begin(), learned_clauses_.end(),
      std::bind1st(std::mem_fun(&SatSolver::ClauseShouldBeKept), this));
  std::sort(clause_to_keep_end, learned_clauses_.end(), ClauseOrdering);

  // Compute the index of the first clause to delete.
  const int num_learned_clauses = learned_clauses_.size();
  const int first_clause_to_delete =
      std::max(static_cast<int>(clause_to_keep_end - learned_clauses_.begin()),
          std::min(num_learned_clauses, target_number_of_learned_clauses_));

  // Delete all the learned clause after 'first_clause_to_delete'.
  for (int i = first_clause_to_delete; i < num_learned_clauses; ++i) {
    SatClause* clause = learned_clauses_[i];
    watched_clauses_.LazyDetach(clause);
  }
  watched_clauses_.CleanUpWatchers();
  for (int i = first_clause_to_delete; i < num_learned_clauses; ++i) {
    counters_.num_literals_forgotten += learned_clauses_[i]->Size();
    delete learned_clauses_[i];
  }
  learned_clauses_.resize(first_clause_to_delete);
  InitLearnedClauseLimit();
}

bool SatSolver::ShouldRestart() {
  SCOPED_TIME_STAT(&stats_);
  if (conflicts_until_next_restart_ != 0) return false;
  restart_count_++;
  conflicts_until_next_restart_ =
      parameters_.restart_period() * SUniv(restart_count_ + 1);
  return true;
}

void SatSolver::InitRestart() {
  SCOPED_TIME_STAT(&stats_);
  restart_count_ = 0;
  if (parameters_.restart_period() > 0) {
    DCHECK_EQ(SUniv(1), 1);
    conflicts_until_next_restart_ = parameters_.restart_period();
  } else {
    conflicts_until_next_restart_ = -1;
  }
}

}  // namespace sat
}  // namespace operations_research
