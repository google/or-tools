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

#include "ortools/sat/vivification.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/cleanup/cleanup.h"
#include "absl/container/btree_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/types/span.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/util.h"
#include "ortools/util/logging.h"

namespace operations_research::sat {

bool Vivifier::MinimizeByPropagation(bool log_info, double dtime_budget,
                                     bool minimize_new_clauses_only) {
  PresolveTimer timer("Vivification", logger_, time_limit_);
  timer.OverrideLogging(log_info || VLOG_IS_ON(2));

  sat_solver_->AdvanceDeterministicTime(time_limit_);
  const double threshold =
      time_limit_->GetElapsedDeterministicTime() + dtime_budget;

  // TODO(user): Fix that. For now the solver cannot be used properly to
  // minimize clauses if assumption_level_ is not zero.
  if (sat_solver_->AssumptionLevel() > 0) return true;

  // Tricky: we don't want TryToMinimizeClause() to delete to_minimize
  // while we are processing it.
  sat_solver_->BlockClauseDeletion(true);
  absl::Cleanup unblock_clause_deletion = [&] {
    sat_solver_->BlockClauseDeletion(false);
  };

  const auto old_counter = counters_;
  const int num_resets = clause_manager_->NumToMinimizeIndexResets();
  while (!time_limit_->LimitReached()) {
    // Abort if we used our budget.
    sat_solver_->AdvanceDeterministicTime(time_limit_);
    if (time_limit_->GetElapsedDeterministicTime() >= threshold) break;

    // Also abort if we did more than one loop over all the clause.
    if (clause_manager_->NumToMinimizeIndexResets() > num_resets + 1) break;

    // First minimize clauses that where never minimized before.
    {
      SatClause* to_minimize = clause_manager_->NextNewClauseToMinimize();
      if (to_minimize != nullptr) {
        if (!TryToMinimizeClause(to_minimize)) return false;
        continue;
      }
      if (minimize_new_clauses_only) break;  // We are done.
    }

    SatClause* clause = clause_manager_->NextClauseToMinimize();
    if (clause != nullptr) {
      if (!TryToMinimizeClause(clause)) return false;
    }
  }

  // Note(user): In some corner cases, the function above might find a
  // feasible assignment. I think it is okay to ignore this special case
  // that should only happen on trivial problems and just reset the solver.
  const bool result = sat_solver_->ResetToLevelZero();

  last_num_vivified_ =
      counters_.num_clauses_vivified - old_counter.num_clauses_vivified;
  last_num_literals_removed_ =
      counters_.num_removed_literals - old_counter.num_removed_literals;
  timer.AddCounter("num_vivifed", last_num_vivified_);
  timer.AddCounter("literals_removed", last_num_literals_removed_);
  timer.AddCounter("loops", clause_manager_->NumToMinimizeIndexResets());

  clause_manager_->DeleteRemovedClauses();
  return result;
}

void Vivifier::KeepAllClausesUsedToInfer(BooleanVariable variable) {
  CHECK(assignment_.VariableIsAssigned(variable));
  if (trail_->Info(variable).level == 0) return;
  int trail_index = trail_->Info(variable).trail_index;
  std::vector<bool> is_marked(trail_index + 1, false);  // move to local member.
  is_marked[trail_index] = true;
  int num = 1;
  for (; num > 0 && trail_index >= 0; --trail_index) {
    if (!is_marked[trail_index]) continue;
    is_marked[trail_index] = false;
    --num;

    const BooleanVariable var = (*trail_)[trail_index].Variable();
    SatClause* clause = clause_manager_->ReasonClauseOrNull(var);
    if (clause != nullptr) {
      clause_manager_->KeepClauseForever(clause);
    }
    if (trail_->AssignmentType(var) == AssignmentType::kSearchDecision) {
      continue;
    }
    for (const Literal l : trail_->Reason(var)) {
      const AssignmentInfo& info = trail_->Info(l.Variable());
      if (info.level == 0) continue;
      if (!is_marked[info.trail_index]) {
        is_marked[info.trail_index] = true;
        ++num;
      }
    }
  }
}

bool Vivifier::SubsumptionIsInteresting(BooleanVariable variable,
                                        int max_size) {
  // TODO(user): other id should probably be safe as long as we do not delete
  // the propagators. Note that symmetry is tricky since we would need to keep
  // the symmetric clause around in KeepAllClauseUsedToInfer().
  const int binary_id = binary_clauses_->PropagatorId();
  const int clause_id = clause_manager_->PropagatorId();

  CHECK(assignment_.VariableIsAssigned(variable));
  if (trail_->Info(variable).level == 0) return true;
  int trail_index = trail_->Info(variable).trail_index;
  std::vector<bool> is_marked(trail_index + 1, false);  // move to local member.
  is_marked[trail_index] = true;
  int num = 1;
  int num_clause_to_mark_as_non_deletable = 0;
  for (; num > 0 && trail_index >= 0; --trail_index) {
    if (!is_marked[trail_index]) continue;
    is_marked[trail_index] = false;
    --num;

    const BooleanVariable var = (*trail_)[trail_index].Variable();
    const int type = trail_->AssignmentType(var);
    if (type == AssignmentType::kSearchDecision) continue;
    if (type != binary_id && type != clause_id) return false;
    SatClause* clause = clause_manager_->ReasonClauseOrNull(var);
    if (clause != nullptr && clause_manager_->IsRemovable(clause)) {
      if (clause->size() > max_size) {
        return false;
      }
      if (++num_clause_to_mark_as_non_deletable > 1) return false;
    }
    for (const Literal l : trail_->Reason(var)) {
      const AssignmentInfo& info = trail_->Info(l.Variable());
      if (info.level == 0) continue;
      if (!is_marked[info.trail_index]) {
        is_marked[info.trail_index] = true;
        ++num;
      }
    }
  }
  return num_clause_to_mark_as_non_deletable <= 1;
}

// This implements "vivification" as described in
// https://doi.org/10.1016/j.artint.2019.103197, with one significant tweak:
// we sort each clause by current trail index before trying to minimize it so
// that we can reuse the trail from previous calls in case there are overlaps.
bool Vivifier::TryToMinimizeClause(SatClause* clause) {
  CHECK(clause != nullptr);
  if (clause->empty()) return true;
  ++counters_.num_clauses_vivified;

  // TODO(user): Make sure clause do not contain any redundant literal before
  // we try to minimize it.
  std::vector<Literal> candidate;
  candidate.reserve(clause->size());

  // Some literals of the clause which are fixed to false or true when
  // propagating some other literals of the clause. Only used if
  // lrat_proof_handler_ is not null.
  std::vector<Literal> fixed_false_literals;
  LiteralIndex fixed_true_literal = kNoLiteralIndex;

  // Note that CP-SAT presolve detects clauses that share n-1 literals and
  // transforms them into (n-1 enforcement) => (1 literal per clause). We
  // currently do not support that internally, but these clauses will still
  // likely be loaded one after the other, so there is a high chance that if we
  // call TryToMinimizeClause() on consecutive clauses, there will be a long
  // prefix in common!
  //
  // TODO(user): Exploit this more by choosing a good minimization order?
  int longest_valid_prefix = 0;
  if (sat_solver_->CurrentDecisionLevel() > 0) {
    candidate.resize(clause->size());

    // Insert any compatible decisions into their correct place in candidate
    const auto& decisions = trail_->Decisions();
    for (Literal lit : *clause) {
      if (!assignment_.LiteralIsFalse(lit)) continue;
      const AssignmentInfo& info = trail_->Info(lit.Variable());
      if (info.level <= 0 || info.level > clause->size()) continue;
      if (decisions[info.level - 1].literal == lit.Negated()) {
        candidate[info.level - 1] = lit;
      }
    }
    // Then compute the matching prefix and discard the rest
    for (int i = 0; i < candidate.size(); ++i) {
      if (candidate[i] != Literal()) {
        ++longest_valid_prefix;
      } else {
        break;
      }
    }
    counters_.num_reused += longest_valid_prefix;
    candidate.resize(longest_valid_prefix);
  }
  // Then do a second pass to add the remaining literals in order.
  for (Literal lit : *clause) {
    const AssignmentInfo& info = trail_->Info(lit.Variable());
    // Skip if this literal is already in the prefix.
    if (info.level >= 1 && info.level <= longest_valid_prefix &&
        candidate[info.level - 1] == lit) {
      continue;
    }
    candidate.push_back(lit);
  }
  CHECK_EQ(candidate.size(), clause->size());

  if (!sat_solver_->BacktrackAndPropagateReimplications(longest_valid_prefix)) {
    return false;
  }

  absl::btree_set<LiteralIndex> moved_last;
  while (!sat_solver_->ModelIsUnsat()) {
    // We want each literal in candidate to appear last once in our propagation
    // order. We want to do that while maximizing the reutilization of the
    // current assignment prefix, that is minimizing the number of
    // decision/progagation we need to perform.
    const int target_level = MoveOneUnprocessedLiteralLast(
        moved_last, sat_solver_->CurrentDecisionLevel(), &candidate);
    if (target_level == -1) break;
    if (!sat_solver_->BacktrackAndPropagateReimplications(target_level)) {
      return false;
    }
    fixed_false_literals.clear();
    fixed_true_literal = kNoLiteralIndex;

    while (sat_solver_->CurrentDecisionLevel() < candidate.size()) {
      if (time_limit_->LimitReached()) return true;
      const int level = sat_solver_->CurrentDecisionLevel();
      const Literal literal = candidate[level];
      // Remove false literals
      if (assignment_.LiteralIsFalse(literal)) {
        if (lrat_proof_handler_ != nullptr) {
          fixed_false_literals.push_back(literal);
        }
        candidate[level] = candidate.back();
        candidate.pop_back();
        continue;
      } else if (assignment_.LiteralIsTrue(literal)) {
        const int variable_level = trail_->Info(literal.Variable()).level;
        if (variable_level == 0) {
          DCHECK(lrat_proof_handler_ == nullptr ||
                 trail_->GetUnitClauseId(literal.Variable()) != kNoClauseId);
          counters_.num_true++;
          counters_.num_removed_literals += clause->size();
          clause_manager_->LazyDelete(clause,
                                      DeletionSourceForStat::FIXED_AT_TRUE);
          return true;
        }

        if (parameters_.inprocessing_minimization_use_conflict_analysis()) {
          // Replace the clause with the reason for the literal being true, plus
          // the literal itself.
          candidate.clear();
          for (const Literal l : sat_solver_->GetDecisionsFixing({literal})) {
            candidate.push_back(l.Negated());
          }
        } else {
          candidate.resize(variable_level);
        }
        fixed_true_literal = literal.Index();
        candidate.push_back(literal);

        // If a (true) literal wasn't propagated by this clause, then we know
        // that this clause is subsumed by other clauses in the database, so we
        // can remove it so long as the subsumption is due to non-removable
        // clauses. If we can subsume this clause by making only 1 additional
        // clause permanent and that clause is no longer than this one, we will
        // do so.
        if (parameters_.subsume_during_vivification() &&
            clause_manager_->ReasonClauseOrNull(literal.Variable()) != clause &&
            SubsumptionIsInteresting(literal.Variable(), candidate.size())) {
          counters_.num_subsumed++;
          counters_.num_removed_literals += clause->size();
          KeepAllClausesUsedToInfer(literal.Variable());
          clause_manager_->LazyDelete(
              clause, DeletionSourceForStat::SUBSUMPTION_VIVIFY);
          return true;
        }

        break;
      } else {
        ++counters_.num_decisions;
        sat_solver_->EnqueueDecisionAndBackjumpOnConflict(literal.Negated());
        if (sat_solver_->ModelIsUnsat()) return false;
        if (clause->IsRemoved()) return true;

        if (sat_solver_->CurrentDecisionLevel() < level) {
          ++counters_.num_conflicts;

          // There was a conflict, consider the conflicting literal next so we
          // should be able to exploit the conflict in the next iteration.
          // TODO(user): I *think* this is sufficient to ensure pushing
          // the same literal to the new trail fails, immediately on the next
          // iteration, if not we may be able to analyse the last failure and
          // skip some propagation steps?
          std::swap(candidate[level],
                    candidate[sat_solver_->CurrentDecisionLevel()]);
        }
      }
    }
    if (candidate.empty()) {
      sat_solver_->NotifyThatModelIsUnsat();
      return false;
    }

    // TODO(user): To use this, we need to proove and rewrite the clause
    // on each of its modification.
    if (!parameters_.inprocessing_minimization_use_all_orderings()) break;
    moved_last.insert(candidate.back().Index());
  }

  // Returns if we don't have any minimization.
  if (candidate.size() == clause->size()) return true;

  std::vector<ClauseId> clause_ids;
  if (lrat_proof_handler_ != nullptr) {
    DCHECK(fixed_true_literal != kNoLiteralIndex ||
           !fixed_false_literals.empty());
    clause_ids.clear();
    if (fixed_true_literal != kNoLiteralIndex) {
      // If some literals of the minimized clause fix another to true, we just
      // need the propagating clauses to prove this (assuming that all the
      // minimized clause literals are false will lead to a conflict on this
      // 'fixed to true' literal).
      clause_manager_->AppendClauseIdsFixing({Literal(fixed_true_literal)},
                                             &clause_ids);
    } else {
      // If some literals of the minimized clause fix those that have been
      // removed to false, the propagating clauses and the original one prove
      // this (assuming that all the minimized clause literals are false will
      // lead to all the literals of the original clause fixed to false, which
      // is a conflict with the original clause).
      clause_manager_->AppendClauseIdsFixing(fixed_false_literals, &clause_ids);
      clause_ids.push_back(clause_manager_->GetClauseId(clause));
    }
  }

  // Reverse the candidate so that the first two literals are appropriate
  // watchers.
  absl::c_reverse(candidate);
  // All but the first literal of the new clause should be false.
  DCHECK(absl::c_all_of(absl::MakeSpan(candidate).subspan(1), [&](Literal l) {
    return assignment_.LiteralIsFalse(l);
  }));
  if (candidate.size() == 1) {
    sat_solver_->BacktrackAndPropagateReimplications(0);
  } else if (assignment_.LiteralIsFalse(candidate[1]) &&
             (!assignment_.LiteralIsTrue(candidate[0]) ||
              trail_->AssignmentLevel(candidate[1]) <
                  trail_->AssignmentLevel(candidate[0]))) {
    // Backtrack if the new clause would propagate earlier than the current
    // reason. This should be a very rare edge case, but it can happen if both
    // conflicts and clause cleanup occur during minimization: some literal in
    // the clause that was propagated false by some decisions might no longer be
    // propagated by the same decisions after backjumping because the clause
    // that propagated it was removed.
    // Note we backtrack to 1 level before this would propagate because we don't
    // actually support propagating the new clause during rewrite, and the
    // propagation would probably be useless.
    sat_solver_->BacktrackAndPropagateReimplications(
        std::max(0, trail_->AssignmentLevel(candidate[1])));
  }
  if (sat_solver_->CurrentDecisionLevel() == 0) {
    // Ensure nothing is fixed at level 0 in case more propagation happened
    // after backtracking.
    OpenSourceEraseIf(candidate,
                      [&](Literal l) { return assignment_.LiteralIsFalse(l); });
    if (absl::c_any_of(clause->AsSpan(), [&](Literal l) {
          return assignment_.LiteralIsTrue(l);
        })) {
      counters_.num_removed_literals += clause->size();
      clause_manager_->LazyDelete(clause, DeletionSourceForStat::FIXED_AT_TRUE);
      return true;
    }
  }

  counters_.num_removed_literals += clause->size() - candidate.size();
  if (!clause_manager_->InprocessingRewriteClause(clause, candidate,
                                                  clause_ids)) {
    sat_solver_->NotifyThatModelIsUnsat();
    return false;
  }

  // Adding a unit clause can cause additional propagation. There is also an
  // edge case where we added the first binary clause of the model by
  // strenghtening a normal clause.
  return sat_solver_->FinishPropagation();
}

}  // namespace operations_research::sat
