// Copyright 2010-2021 Google LLC
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

#include "ortools/sat/sat_inprocessing.h"

#include <cstdint>
#include <limits>

#include "absl/container/inlined_vector.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/strong_vector.h"
#include "ortools/base/timer.h"
#include "ortools/sat/probing.h"
#include "ortools/sat/sat_decision.h"

namespace operations_research {
namespace sat {

void PostsolveClauses::AddClauseWithSpecialLiteral(
    Literal literal, absl::Span<const Literal> clause) {
  bool found = false;
  clauses.emplace_back(clause.begin(), clause.end());
  for (int i = 0; i < clause.size(); ++i) {
    if (clause[i] == literal) {
      found = true;
      std::swap(clauses.back()[0], clauses.back()[i]);
      break;
    }
  }
  CHECK(found);
}

#define RETURN_IF_FALSE(f) \
  if (!(f)) return false;

bool Inprocessing::PresolveLoop(SatPresolveOptions options) {
  WallTimer wall_timer;
  wall_timer.Start();

  const bool log_info = options.log_info || VLOG_IS_ON(1);
  const bool log_round_info = VLOG_IS_ON(1);

  // Mainly useful for development.
  double probing_time = 0.0;

  // We currently do the transformations in a given order and restart each time
  // we did something to make sure that the earlier step cannot srengthen more.
  // This might not be the best, but it is really good during development phase
  // to make sure each individual functions is as incremental and as fast as
  // possible.
  const double start_dtime = time_limit_->GetElapsedDeterministicTime();
  const double stop_dtime = start_dtime + options.deterministic_time_limit;
  while (!time_limit_->LimitReached() &&
         time_limit_->GetElapsedDeterministicTime() <= stop_dtime) {
    CHECK_EQ(sat_solver_->CurrentDecisionLevel(), 0);
    if (!LevelZeroPropagate()) return false;

    // This one is fast since only newly fixed variables are considered.
    implication_graph_->RemoveFixedVariables();

    // This also prepare the stamping below so that we do that on a DAG and do
    // not consider potential new implications added by
    // RemoveFixedAndEquivalentVariables().
    RETURN_IF_FALSE(DetectEquivalencesAndStamp(options.use_transitive_reduction,
                                               log_round_info));

    // TODO(user): This should/could be integrated with the stamping since it
    // seems better to do just one loop instead of two over all clauses. Because
    // of memory access. it isn't that clear though.
    RETURN_IF_FALSE(RemoveFixedAndEquivalentVariables(log_round_info));
    RETURN_IF_FALSE(stamping_simplifier_->DoOneRound(log_round_info));

    // We wait for the fix-point to be reached before doing the other
    // simplifications below.
    if (MoreFixedVariableToClean() || MoreRedundantVariableToClean() ||
        !implication_graph_->IsDag()) {
      continue;
    }

    RETURN_IF_FALSE(SubsumeAndStrenghtenRound(log_round_info));
    if (MoreFixedVariableToClean() || MoreRedundantVariableToClean() ||
        !implication_graph_->IsDag()) {
      continue;
    }

    // TODO(user): Combine the two? this way we don't create a full literal <->
    // clause graph twice. It might make sense to reach the BCE fix point which
    // is unique before each variable elimination.
    blocked_clause_simplifier_->DoOneRound(log_round_info);
    RETURN_IF_FALSE(bounded_variable_elimination_->DoOneRound(log_round_info));
    RETURN_IF_FALSE(LevelZeroPropagate());

    // Probing.
    const double saved_wtime = wall_timer.Get();
    const double time_left =
        stop_dtime - time_limit_->GetElapsedDeterministicTime();
    if (time_left <= 0) break;
    ProbingOptions probing_options;
    probing_options.log_info = log_round_info;
    probing_options.deterministic_limit = time_left;
    probing_options.extract_binary_clauses =
        options.extract_binary_clauses_in_probing;
    RETURN_IF_FALSE(FailedLiteralProbingRound(probing_options, model_));
    probing_time += wall_timer.Get() - saved_wtime;

    if (MoreFixedVariableToClean() || MoreRedundantVariableToClean() ||
        !implication_graph_->IsDag()) {
      continue;
    }

    break;
  }

  // TODO(user): Maintain the total number of literals in the watched clauses.
  if (!LevelZeroPropagate()) return false;

  LOG_IF(INFO, log_info)
      << "Presolve."
      << " num_fixed: " << trail_->Index()
      << " num_redundant: " << implication_graph_->num_redundant_literals() / 2
      << "/" << sat_solver_->NumVariables()
      << " num_implications: " << implication_graph_->num_implications()
      << " num_watched_clauses: " << clause_manager_->num_watched_clauses()
      << " dtime: " << time_limit_->GetElapsedDeterministicTime() - start_dtime
      << "/" << options.deterministic_time_limit
      << " wtime: " << wall_timer.Get()
      << " non-probing time: " << (wall_timer.Get() - probing_time);
  return true;
}

bool Inprocessing::InprocessingRound() {
  WallTimer wall_timer;
  wall_timer.Start();

  const bool log_info = true || VLOG_IS_ON(1);
  const bool log_round_info = VLOG_IS_ON(1);

  // Mainly useful for development.
  double probing_time = 0.0;
  const double start_dtime = time_limit_->GetElapsedDeterministicTime();

  // Try to spend a given ratio of time in the inprocessing.
  if (total_dtime_ > 0.1 * start_dtime) return true;

  // We make sure we do not "pollute" the current saved polarities. We will
  // restore them at the end.
  //
  // TODO(user): We should probably also disable the variable/clauses activity
  // updates.
  decision_policy_->MaybeEnablePhaseSaving(/*save_phase=*/false);

  RETURN_IF_FALSE(DetectEquivalencesAndStamp(true, log_round_info));
  RETURN_IF_FALSE(RemoveFixedAndEquivalentVariables(log_round_info));
  RETURN_IF_FALSE(LevelZeroPropagate());

  // Probing.
  const double saved_wtime = wall_timer.Get();
  ProbingOptions probing_options;
  probing_options.log_info = log_round_info;
  probing_options.deterministic_limit = 5;
  probing_options.extract_binary_clauses = true;
  RETURN_IF_FALSE(FailedLiteralProbingRound(probing_options, model_));
  probing_time += wall_timer.Get() - saved_wtime;

  RETURN_IF_FALSE(DetectEquivalencesAndStamp(true, log_round_info));
  RETURN_IF_FALSE(RemoveFixedAndEquivalentVariables(log_round_info));
  RETURN_IF_FALSE(LevelZeroPropagate());

  RETURN_IF_FALSE(stamping_simplifier_->DoOneRound(log_round_info));
  RETURN_IF_FALSE(RemoveFixedAndEquivalentVariables(log_round_info));

  // TODO(user): Add a small wrapper function to time this.
  RETURN_IF_FALSE(LevelZeroPropagate());
  sat_solver_->MinimizeSomeClauses(/*decisions_budget=*/1000);
  RETURN_IF_FALSE(LevelZeroPropagate());

  RETURN_IF_FALSE(SubsumeAndStrenghtenRound(log_round_info));

  RETURN_IF_FALSE(RemoveFixedAndEquivalentVariables(log_round_info));
  blocked_clause_simplifier_->DoOneRound(log_round_info);
  RETURN_IF_FALSE(bounded_variable_elimination_->DoOneRound(log_round_info));
  RETURN_IF_FALSE(LevelZeroPropagate());

  total_dtime_ += time_limit_->GetElapsedDeterministicTime() - start_dtime;
  LOG_IF(INFO, log_info)
      << "Presolve."
      << " num_fixed: " << trail_->Index()
      << " num_redundant: " << implication_graph_->num_redundant_literals() / 2
      << "/" << sat_solver_->NumVariables()
      << " num_implications: " << implication_graph_->num_implications()
      << " num_watched_clauses: " << clause_manager_->num_watched_clauses()
      << " dtime: " << time_limit_->GetElapsedDeterministicTime() - start_dtime
      << " wtime: " << wall_timer.Get()
      << " non-probing time: " << (wall_timer.Get() - probing_time);

  decision_policy_->MaybeEnablePhaseSaving(/*save_phase=*/true);
  return true;
}

#undef RETURN_IF_FALSE

bool Inprocessing::MoreFixedVariableToClean() const {
  const int64_t new_num_fixed_variables = trail_->Index();
  return last_num_fixed_variables_ < new_num_fixed_variables;
}

bool Inprocessing::MoreRedundantVariableToClean() const {
  const int64_t new_num_redundant_literals =
      implication_graph_->num_redundant_literals();
  return last_num_redundant_literals_ < new_num_redundant_literals;
}

bool Inprocessing::LevelZeroPropagate() {
  CHECK_EQ(sat_solver_->CurrentDecisionLevel(), 0);
  clause_manager_->AttachAllClauses();
  return sat_solver_->Propagate();
}

// It make sense to do the pre-stamping right after the equivalence detection
// since it needs a DAG and can detect extra failed literal.
bool Inprocessing::DetectEquivalencesAndStamp(bool use_transitive_reduction,
                                              bool log_info) {
  if (!LevelZeroPropagate()) return false;
  implication_graph_->RemoveFixedVariables();
  if (!implication_graph_->IsDag()) {
    // TODO(user): consider doing the transitive reduction after each SCC.
    // It might be slow but we could try to make it more incremental to
    // compensate and it should allow further reduction.
    if (!implication_graph_->DetectEquivalences(log_info)) return false;
    if (!LevelZeroPropagate()) return false;
    if (use_transitive_reduction) {
      if (!implication_graph_->ComputeTransitiveReduction(log_info)) {
        return false;
      }
      if (!LevelZeroPropagate()) return false;
    }
  }

  if (!stamping_simplifier_->ComputeStampsForNextRound(log_info)) return false;
  return LevelZeroPropagate();
}

bool Inprocessing::RemoveFixedAndEquivalentVariables(bool log_info) {
  // Preconditions.
  //
  // TODO(user): The level zero is required because we remove fixed variables
  // but if we split this into two functions, we could rewrite clause at any
  // level.
  CHECK_EQ(sat_solver_->CurrentDecisionLevel(), 0);
  if (!LevelZeroPropagate()) return false;

  // Test if some work is needed.
  //
  // TODO(user): If only new fixed variables are there, we can use a faster
  // function. We should also merge the code with the deletion code in
  // sat_solver_.cc, but that require some refactoring of the dependence between
  // files.
  const int64_t new_num_redundant_literals =
      implication_graph_->num_redundant_literals();
  const int64_t new_num_fixed_variables = trail_->Index();
  if (last_num_redundant_literals_ == new_num_redundant_literals &&
      last_num_fixed_variables_ == new_num_fixed_variables) {
    return true;
  }
  last_num_fixed_variables_ = new_num_fixed_variables;
  last_num_redundant_literals_ = new_num_redundant_literals;

  // Start the round.
  WallTimer wall_timer;
  wall_timer.Start();

  int64_t num_removed_literals = 0;
  int64_t num_inspected_literals = 0;

  // We need this temporary vector for the DRAT proof settings, otherwise
  // we could just have done an in-place transformation.
  std::vector<Literal> new_clause;

  // Used to mark clause literals.
  const int num_literals(sat_solver_->NumVariables() * 2);
  absl::StrongVector<LiteralIndex, bool> marked(num_literals, false);

  clause_manager_->DeleteRemovedClauses();
  clause_manager_->DetachAllClauses();
  for (SatClause* clause : clause_manager_->AllClausesInCreationOrder()) {
    bool removed = false;
    bool need_rewrite = false;

    // We first do a loop to see if there is anything to do.
    for (const Literal l : clause->AsSpan()) {
      if (assignment_.LiteralIsTrue(l)) {
        // TODO(user): we should output literal to the proof right away,
        // currently if we remove clauses before fixing literal the proof is
        // wrong.
        if (!clause_manager_->InprocessingFixLiteral(l)) return false;
        clause_manager_->InprocessingRemoveClause(clause);
        num_removed_literals += clause->size();
        removed = true;
        break;
      }
      if (assignment_.LiteralIsFalse(l) || implication_graph_->IsRedundant(l)) {
        need_rewrite = true;
        break;
      }
    }

    num_inspected_literals += clause->size();
    if (removed || !need_rewrite) continue;
    num_inspected_literals += clause->size();

    // Rewrite the clause.
    new_clause.clear();
    for (const Literal l : clause->AsSpan()) {
      const Literal r = implication_graph_->RepresentativeOf(l);
      if (marked[r.Index()] || assignment_.LiteralIsFalse(r)) {
        continue;
      }
      if (marked[r.NegatedIndex()] || assignment_.LiteralIsTrue(r)) {
        clause_manager_->InprocessingRemoveClause(clause);
        num_removed_literals += clause->size();
        removed = true;
        break;
      }
      marked[r.Index()] = true;
      new_clause.push_back(r);
    }

    // Restore marked.
    for (const Literal l : new_clause) marked[l.Index()] = false;
    if (removed) continue;

    num_removed_literals += clause->size() - new_clause.size();
    if (!clause_manager_->InprocessingRewriteClause(clause, new_clause)) {
      return false;
    }
  }

  // TODO(user): find a way to auto-tune that after a run on borg...
  const double dtime = static_cast<double>(num_inspected_literals) * 1e-8;
  time_limit_->AdvanceDeterministicTime(dtime);
  LOG_IF(INFO, log_info) << "Cleanup. num_removed_literals: "
                         << num_removed_literals << " dtime: " << dtime
                         << " wtime: " << wall_timer.Get();
  return true;
}

// TODO(user): Use better work limits, see SAT09.CRAFTED.ramseycube.Q3inK12
//
// TODO(user): Be more incremental, each time a clause is added/reduced track
// which literal are impacted? Also try to do orthogonal reductions from one
// round to the next.
bool Inprocessing::SubsumeAndStrenghtenRound(bool log_info) {
  WallTimer wall_timer;
  wall_timer.Start();

  int64_t num_subsumed_clauses = 0;
  int64_t num_removed_literals = 0;
  int64_t num_inspected_signatures = 0;
  int64_t num_inspected_literals = 0;

  // We need this temporary vector for the DRAT proof settings, otherwise
  // we could just have done an in-place transformation.
  std::vector<Literal> new_clause;

  // This function needs the watcher to be detached as we might remove some
  // of the watched literals.
  //
  // TODO(user): We could do that only if we do some reduction, but this is
  // quite fast though.
  clause_manager_->DeleteRemovedClauses();
  clause_manager_->DetachAllClauses();

  // Process clause by increasing sizes.
  // TODO(user): probably faster without the size indirection.
  std::vector<SatClause*> clauses =
      clause_manager_->AllClausesInCreationOrder();
  std::sort(clauses.begin(), clauses.end(),
            [](SatClause* a, SatClause* b) { return a->size() < b->size(); });

  // Used to mark clause literals.
  const LiteralIndex num_literals(sat_solver_->NumVariables() * 2);
  SparseBitset<LiteralIndex> marked(num_literals);

  // Clause index in clauses.
  // TODO(user): Storing signatures here might be faster?
  absl::StrongVector<LiteralIndex, absl::InlinedVector<int, 6>> one_watcher(
      num_literals.value());

  // Clause signatures in the same order as clauses.
  std::vector<uint64_t> signatures(clauses.size());

  std::vector<Literal> candidates_for_removal;
  for (int clause_index = 0; clause_index < clauses.size(); ++clause_index) {
    SatClause* clause = clauses[clause_index];

    // TODO(user): Better abort limit. We could also limit the watcher sizes and
    // never look at really long clauses. Note that for an easier
    // incrementality, it is better to reach some kind of completion so we know
    // what new stuff need to be done.
    if (num_inspected_literals + num_inspected_signatures > 1e9) {
      break;
    }

    // Check for subsumption, note that this currently ignore all clauses in the
    // binary implication graphs. Stamping is doing some of that (and some also
    // happen during probing), but we could consider only direct implications
    // here and be a bit more exhaustive than what stamping do with them (at
    // least for node with many incoming and outgoing implications).
    //
    // TODO(user): Do some reduction using binary clauses. Note that only clause
    // that never propagated since last round need to be checked for binary
    // subsumption.

    // Compute hash and mark literals.
    uint64_t signature = 0;
    marked.SparseClearAll();
    for (const Literal l : clause->AsSpan()) {
      marked.Set(l.Index());
      signature |= (uint64_t{1} << (l.Variable().value() % 64));
    }

    // Look for clause that subsumes this one. Note that because we inspect
    // all one watcher lists for the literals of this clause, if a clause is
    // included inside this one, it must appear in one of these lists.
    bool removed = false;
    candidates_for_removal.clear();
    const uint64_t mask = ~signature;
    for (const Literal l : clause->AsSpan()) {
      num_inspected_signatures += one_watcher[l.Index()].size();
      for (const int i : one_watcher[l.Index()]) {
        if ((mask & signatures[i]) != 0) continue;

        bool subsumed = true;
        bool stengthen = true;
        LiteralIndex to_remove = kNoLiteralIndex;
        num_inspected_literals += clauses[i]->size();
        for (const Literal o : clauses[i]->AsSpan()) {
          if (!marked[o.Index()]) {
            subsumed = false;
            if (to_remove == kNoLiteralIndex && marked[o.NegatedIndex()]) {
              to_remove = o.NegatedIndex();
            } else {
              stengthen = false;
              break;
            }
          }
        }
        if (subsumed) {
          ++num_subsumed_clauses;
          num_removed_literals += clause->size();
          clause_manager_->InprocessingRemoveClause(clause);
          removed = true;
          break;
        }
        if (stengthen) {
          CHECK_NE(kNoLiteralIndex, to_remove);
          candidates_for_removal.push_back(Literal(to_remove));
        }
      }
      if (removed) break;
    }
    if (removed) continue;

    // For strengthenning we also need to check the negative watcher lists.
    for (const Literal l : clause->AsSpan()) {
      num_inspected_signatures += one_watcher[l.NegatedIndex()].size();
      for (const int i : one_watcher[l.NegatedIndex()]) {
        if ((mask & signatures[i]) != 0) continue;

        bool stengthen = true;
        num_inspected_literals += clauses[i]->size();
        for (const Literal o : clauses[i]->AsSpan()) {
          if (o == l.Negated()) continue;
          if (!marked[o.Index()]) {
            stengthen = false;
            break;
          }
        }
        if (stengthen) {
          candidates_for_removal.push_back(l);
        }
      }
    }

    // Any literal here can be removed, but afterwards the other might not. For
    // now we just remove the first one.
    //
    // TODO(user): remove first and see if other still removable. Alternatively
    // use a "removed" marker and redo a check for each clause that simplifies
    // this one? Or just remove the first one, and wait for next round.
    if (!candidates_for_removal.empty()) {
      new_clause.clear();
      for (const Literal l : clause->AsSpan()) {
        new_clause.push_back(l);
      }

      int new_size = 0;
      for (const Literal l : new_clause) {
        if (l == candidates_for_removal[0]) continue;
        new_clause[new_size++] = l;
      }
      CHECK_EQ(new_size + 1, new_clause.size());
      new_clause.resize(new_size);

      num_removed_literals += clause->size() - new_clause.size();
      if (!clause_manager_->InprocessingRewriteClause(clause, new_clause)) {
        return false;
      }
      if (clause->size() == 0) continue;

      // Recompute signature.
      signature = 0;
      for (const Literal l : clause->AsSpan()) {
        signature |= (uint64_t{1} << (l.Variable().value() % 64));
      }
    }

    // Register one literal to watch. Any literal works, but we choose the
    // smallest list.
    //
    // TODO(user): No need to add this clause if we know it cannot subsume
    // any new clause since last round. i.e. unchanged clause that do not
    // contains any literals of newly added clause do not need to be added
    // here. We can track two bitset in LiteralWatchers via a register
    // mechanism:
    // - literal of newly watched clauses since last clear.
    // - literal of reduced clauses since last clear.
    //
    // Important: we can only use this clause to subsume/strenghten others if
    // it cannot be deleted later.
    if (!clause_manager_->IsRemovable(clause)) {
      int min_size = std::numeric_limits<int32_t>::max();
      LiteralIndex min_literal = kNoLiteralIndex;
      for (const Literal l : clause->AsSpan()) {
        if (one_watcher[l.Index()].size() < min_size) {
          min_size = one_watcher[l.Index()].size();
          min_literal = l.Index();
        }
      }

      // TODO(user): We could/should sort the literal in this clause by
      // using literals that appear in a small number of clauses first so that
      // we maximize the chance of early abort in the critical loops above.
      //
      // TODO(user): We could also move the watched literal first so we always
      // skip it.
      signatures[clause_index] = signature;
      one_watcher[min_literal].push_back(clause_index);
    }
  }

  // We might have fixed variables, finish the propagation.
  if (!LevelZeroPropagate()) return false;

  // TODO(user): tune the deterministic time.
  const double dtime = static_cast<double>(num_inspected_signatures) * 1e-8 +
                       static_cast<double>(num_inspected_literals) * 5e-9;
  time_limit_->AdvanceDeterministicTime(dtime);
  LOG_IF(INFO, log_info) << "Subsume. num_removed_literals: "
                         << num_removed_literals
                         << " num_subsumed: " << num_subsumed_clauses
                         << " dtime: " << dtime
                         << " wtime: " << wall_timer.Get();
  return true;
}

bool StampingSimplifier::DoOneRound(bool log_info) {
  WallTimer wall_timer;
  wall_timer.Start();

  dtime_ = 0.0;
  num_subsumed_clauses_ = 0;
  num_removed_literals_ = 0;
  num_fixed_ = 0;

  if (implication_graph_->literal_size() == 0) return true;
  if (implication_graph_->num_implications() == 0) return true;

  if (!stamps_are_already_computed_) {
    // We need a DAG so that we don't have cycle while we sample the tree.
    // TODO(user): We could probably deal with it if needed so that we don't
    // need to do equivalence detetion each time we want to run this.
    implication_graph_->RemoveFixedVariables();
    if (!implication_graph_->DetectEquivalences(log_info)) return true;
    SampleTreeAndFillParent();
    if (!ComputeStamps()) return false;
  }
  stamps_are_already_computed_ = false;
  if (!ProcessClauses()) return false;

  // Note that num_removed_literals_ do not count the literals of the subsumed
  // clauses.
  time_limit_->AdvanceDeterministicTime(dtime_);
  log_info |= VLOG_IS_ON(1);
  LOG_IF(INFO, log_info) << "Stamping. num_removed_literals: "
                         << num_removed_literals_
                         << " num_subsumed: " << num_subsumed_clauses_
                         << " num_fixed: " << num_fixed_ << " dtime: " << dtime_
                         << " wtime: " << wall_timer.Get();
  return true;
}

bool StampingSimplifier::ComputeStampsForNextRound(bool log_info) {
  WallTimer wall_timer;
  wall_timer.Start();
  dtime_ = 0.0;
  num_fixed_ = 0;

  if (implication_graph_->literal_size() == 0) return true;
  if (implication_graph_->num_implications() == 0) return true;

  implication_graph_->RemoveFixedVariables();
  if (!implication_graph_->DetectEquivalences(log_info)) return true;
  SampleTreeAndFillParent();
  if (!ComputeStamps()) return false;
  stamps_are_already_computed_ = true;

  // TODO(user): compute some dtime, it is always zero currently.
  time_limit_->AdvanceDeterministicTime(dtime_);
  log_info |= VLOG_IS_ON(1);
  LOG_IF(INFO, log_info) << "Prestamping."
                         << " num_fixed: " << num_fixed_ << " dtime: " << dtime_
                         << " wtime: " << wall_timer.Get();
  return true;
}

void StampingSimplifier::SampleTreeAndFillParent() {
  const int size = implication_graph_->literal_size();
  CHECK(implication_graph_->IsDag());  // so we don't have cycle.
  parents_.resize(size);
  for (LiteralIndex i(0); i < size; ++i) {
    parents_[i] = i;  // default.
    if (implication_graph_->IsRedundant(Literal(i))) continue;
    if (assignment_.LiteralIsAssigned(Literal(i))) continue;

    // TODO(user): Better algo to not select redundant parent.
    //
    // TODO(user): if parents_[x] = y, try not to have parents_[not(y)] = not(x)
    // because this is not as useful for the simplification power.
    //
    // TODO(user): More generally, we could sample a parent while probing so
    // that we consider all hyper binary implications (in the case we don't add
    // them to the implication graph already).
    const auto& children_of_not_l =
        implication_graph_->DirectImplications(Literal(i).Negated());
    if (children_of_not_l.empty()) continue;
    for (int num_tries = 0; num_tries < 10; ++num_tries) {
      const Literal candidate =
          children_of_not_l[absl::Uniform<int>(*random_, 0,
                                               children_of_not_l.size())]
              .Negated();
      if (implication_graph_->IsRedundant(candidate)) continue;
      if (i == candidate.Index()) continue;

      // We found an interesting parent.
      parents_[i] = candidate.Index();
      break;
    }
  }
}

bool StampingSimplifier::ComputeStamps() {
  const int size = implication_graph_->literal_size();

  // Compute sizes.
  sizes_.assign(size, 0);
  for (LiteralIndex i(0); i < size; ++i) {
    if (parents_[i] == i) continue;  // leaf.
    sizes_[parents_[i]]++;
  }

  // Compute starts in the children_ vector for each node.
  starts_.resize(size + 1);  // We use a sentinel.
  starts_[LiteralIndex(0)] = 0;
  for (LiteralIndex i(1); i <= size; ++i) {
    starts_[i] = starts_[i - 1] + sizes_[i - 1];
  }

  // Fill children. This messes up starts_.
  children_.resize(size);
  for (LiteralIndex i(0); i < size; ++i) {
    if (parents_[i] == i) continue;  // leaf.
    children_[starts_[parents_[i]]++] = i;
  }

  // Reset starts to correct value.
  for (LiteralIndex i(0); i < size; ++i) {
    starts_[i] -= sizes_[i];
  }

  if (DEBUG_MODE) {
    CHECK_EQ(starts_[LiteralIndex(0)], 0);
    for (LiteralIndex i(1); i <= size; ++i) {
      CHECK_EQ(starts_[i], starts_[i - 1] + sizes_[i - 1]);
    }
  }

  // Perform a DFS from each root to compute the stamps.
  int64_t stamp = 0;
  first_stamps_.resize(size);
  last_stamps_.resize(size);
  marked_.assign(size, false);
  for (LiteralIndex i(0); i < size; ++i) {
    if (parents_[i] != i) continue;  // Not a root.
    DCHECK(!marked_[i]);
    const LiteralIndex tree_root = i;
    dfs_stack_.push_back(i);
    while (!dfs_stack_.empty()) {
      const LiteralIndex top = dfs_stack_.back();
      if (marked_[top]) {
        dfs_stack_.pop_back();
        last_stamps_[top] = stamp++;
        continue;
      }
      first_stamps_[top] = stamp++;
      marked_[top] = true;

      // Failed literal detection. If the negation of top is in the same
      // tree, we can fix the LCA of top and its negation to false.
      if (marked_[Literal(top).NegatedIndex()] &&
          first_stamps_[Literal(top).NegatedIndex()] >=
              first_stamps_[tree_root]) {
        // Find the LCA.
        const int first_stamp = first_stamps_[Literal(top).NegatedIndex()];
        LiteralIndex lca = top;
        while (first_stamps_[lca] > first_stamp) {
          lca = parents_[lca];
        }
        ++num_fixed_;
        if (!clause_manager_->InprocessingFixLiteral(Literal(lca).Negated())) {
          return false;
        }
      }

      const int end = starts_[top + 1];  // Ok with sentinel.
      for (int j = starts_[top]; j < end; ++j) {
        DCHECK_NE(top, children_[j]);    // We removed leaf self-loop.
        DCHECK(!marked_[children_[j]]);  // This is a tree.
        dfs_stack_.push_back(children_[j]);
      }
    }
  }
  DCHECK_EQ(stamp, 2 * size);
  return true;
}

bool StampingSimplifier::ProcessClauses() {
  struct Entry {
    int i;            // Index in the clause.
    bool is_negated;  // Correspond to clause[i] or clause[i].Negated();
    int start;        // Note that all start stamps are different.
    int end;
    bool operator<(const Entry& o) const { return start < o.start; }
  };
  std::vector<int> to_remove;
  std::vector<Literal> new_clause;
  std::vector<Entry> entries;
  clause_manager_->DeleteRemovedClauses();
  clause_manager_->DetachAllClauses();
  for (SatClause* clause : clause_manager_->AllClausesInCreationOrder()) {
    const auto span = clause->AsSpan();
    if (span.empty()) continue;

    // Note that we might fix literal as we perform the loop here, so we do
    // need to deal with them.
    //
    // For a and b in the clause, if not(a) => b is present, then the clause is
    // subsumed. If a => b, then a can be removed, and if not(a) => not(b) then
    // b can be removed. Nothing can be done if a => not(b).
    entries.clear();
    for (int i = 0; i < span.size(); ++i) {
      if (assignment_.LiteralIsTrue(span[i])) {
        clause_manager_->InprocessingRemoveClause(clause);
        break;
      }
      if (assignment_.LiteralIsFalse(span[i])) continue;
      entries.push_back({i, false, first_stamps_[span[i].Index()],
                         last_stamps_[span[i].Index()]});
      entries.push_back({i, true, first_stamps_[span[i].NegatedIndex()],
                         last_stamps_[span[i].NegatedIndex()]});
    }
    if (clause->empty()) continue;

    // The sort should be dominant.
    if (!entries.empty()) {
      const double n = static_cast<double>(entries.size());
      dtime_ += 1.5e-8 * n * std::log(n);
      std::sort(entries.begin(), entries.end());
    }

    Entry top_entry;
    top_entry.end = -1;  // Sentinel.
    to_remove.clear();
    for (const Entry& e : entries) {
      if (e.end < top_entry.end) {
        // We found an implication: top_entry => this entry.
        const Literal lhs = top_entry.is_negated ? span[top_entry.i].Negated()
                                                 : span[top_entry.i];
        const Literal rhs = e.is_negated ? span[e.i].Negated() : span[e.i];
        DCHECK(ImplicationIsInTree(lhs, rhs));

        if (top_entry.is_negated != e.is_negated) {
          // Failed literal?
          if (top_entry.i == e.i) {
            ++num_fixed_;
            if (top_entry.is_negated) {
              // not(span[i]) => span[i] so span[i] true.
              // And the clause is satisfied (so we count as as subsumed).
              if (!clause_manager_->InprocessingFixLiteral(span[top_entry.i])) {
                return false;
              }
            } else {
              // span[i] => not(span[i]) so span[i] false.
              if (!clause_manager_->InprocessingFixLiteral(
                      span[top_entry.i].Negated())) {
                return false;
              }
              to_remove.push_back(top_entry.i);
              continue;
            }
          }

          // not(a) => b : subsumption.
          // a => not(b), we cannot deduce anything, but it might make sense
          // to see if not(b) implies anything instead of just keeping
          // top_entry. See TODO below.
          if (top_entry.is_negated) {
            num_subsumed_clauses_++;
            clause_manager_->InprocessingRemoveClause(clause);
            break;
          }
        } else {
          CHECK_NE(top_entry.i, e.i);
          if (top_entry.is_negated) {
            // not(a) => not(b), we can remove b.
            to_remove.push_back(e.i);
          } else {
            // a => b, we can remove a.
            //
            // TODO(user): Note that it is okay to still use top_entry, but we
            // might miss the removal of b if b => c. Also the paper do things
            // differently. Make sure we don't miss any simplification
            // opportunites by not changing top_entry. Same in the other
            // branches.
            to_remove.push_back(top_entry.i);
          }
        }
      } else {
        top_entry = e;
      }
    }

    if (clause->empty()) continue;

    // Strengthen the clause.
    if (!to_remove.empty() || entries.size() < span.size()) {
      new_clause.clear();
      gtl::STLSortAndRemoveDuplicates(&to_remove);
      int to_remove_index = 0;
      for (int i = 0; i < span.size(); ++i) {
        if (to_remove_index < to_remove.size() &&
            i == to_remove[to_remove_index]) {
          ++to_remove_index;
          continue;
        }
        if (assignment_.LiteralIsTrue(span[i])) {
          clause_manager_->InprocessingRemoveClause(clause);
          continue;
        }
        if (assignment_.LiteralIsFalse(span[i])) continue;
        new_clause.push_back(span[i]);
      }
      num_removed_literals_ += span.size() - new_clause.size();
      if (!clause_manager_->InprocessingRewriteClause(clause, new_clause)) {
        return false;
      }
    }
  }
  return true;
}

void BlockedClauseSimplifier::DoOneRound(bool log_info) {
  WallTimer wall_timer;
  wall_timer.Start();

  dtime_ = 0.0;
  num_blocked_clauses_ = 0;
  num_inspected_literals_ = 0;

  InitializeForNewRound();

  while (!time_limit_->LimitReached() && !queue_.empty()) {
    const Literal l = queue_.front();
    in_queue_[l.Index()] = false;
    queue_.pop_front();
    ProcessLiteral(l);
  }

  // Release some memory.
  literal_to_clauses_.clear();

  dtime_ += 1e-8 * num_inspected_literals_;
  time_limit_->AdvanceDeterministicTime(dtime_);
  log_info |= VLOG_IS_ON(1);
  LOG_IF(INFO, log_info) << "Blocked clause. num_blocked_clauses: "
                         << num_blocked_clauses_ << " dtime: " << dtime_
                         << " wtime: " << wall_timer.Get();
}

void BlockedClauseSimplifier::InitializeForNewRound() {
  clauses_.clear();
  clause_manager_->DeleteRemovedClauses();
  clause_manager_->DetachAllClauses();
  for (SatClause* c : clause_manager_->AllClausesInCreationOrder()) {
    // We ignore redundant clause. This shouldn't cause any validity issue.
    if (clause_manager_->IsRemovable(c)) continue;

    clauses_.push_back(c);
  }
  const int num_literals = clause_manager_->literal_size();

  // TODO(user): process in order of increasing number of clause that contains
  // not(l)?
  in_queue_.assign(num_literals, true);
  for (LiteralIndex l(0); l < num_literals; ++l) {
    queue_.push_back(Literal(l));
  }

  marked_.resize(num_literals);
  DCHECK(
      std::all_of(marked_.begin(), marked_.end(), [](bool b) { return !b; }));

  // TODO(user): because we don't create new clause here we can use a flat
  // vector for literal_to_clauses_.
  literal_to_clauses_.clear();
  literal_to_clauses_.resize(num_literals);
  for (ClauseIndex i(0); i < clauses_.size(); ++i) {
    for (const Literal l : clauses_[i]->AsSpan()) {
      literal_to_clauses_[l.Index()].push_back(i);
    }
    num_inspected_literals_ += clauses_[i]->size();
  }
}

void BlockedClauseSimplifier::ProcessLiteral(Literal current_literal) {
  if (assignment_.LiteralIsAssigned(current_literal)) return;
  if (implication_graph_->IsRemoved(current_literal)) return;

  // We want to check first that this clause will resolve to trivial clause with
  // all binary containing not(current_literal). So mark all literal l so that
  // current_literal => l.
  //
  // TODO(user): We do not need to redo that each time we reprocess
  // current_literal.
  //
  // TODO(user): Ignore redundant literals. That might require pushing
  // equivalence to the postsolve stack though. Better to simply remove
  // these equivalence if we are allowed to and update the postsolve then.
  //
  // TODO(user): Make this work in the presence of at most ones.
  int num_binary = 0;
  const std::vector<Literal>& implications =
      implication_graph_->DirectImplications(current_literal);
  for (const Literal l : implications) {
    if (l == current_literal) continue;
    ++num_binary;
    marked_[l.Index()] = true;
  }

  // TODO(user): We could also mark a small clause containing
  // current_literal.Negated(), and make sure we only include in
  // clauses_to_process clauses that resolve trivially with that clause.
  std::vector<ClauseIndex> clauses_to_process;
  for (const ClauseIndex i : literal_to_clauses_[current_literal.Index()]) {
    if (clauses_[i]->empty()) continue;

    // Blocked with respect to binary clause only? all marked binary should have
    // their negation in clause.
    //
    // TODO(user): Abort if size left is too small.
    if (num_binary > 0) {
      if (clauses_[i]->size() <= num_binary) continue;
      int num_with_negation_marked = 0;
      for (const Literal l : clauses_[i]->AsSpan()) {
        if (l == current_literal) continue;
        if (marked_[l.NegatedIndex()]) {
          ++num_with_negation_marked;
        }
      }
      num_inspected_literals_ += clauses_[i]->size();
      if (num_with_negation_marked < num_binary) continue;
    }
    clauses_to_process.push_back(i);
  }

  // Clear marked.
  for (const Literal l : implications) {
    marked_[l.Index()] = false;
  }

  // TODO(user): There is a possible optimization: If we mark all literals of
  // all the clause to process, we can check that each clause containing
  // current_literal.Negated() contains at least one of these literal negated
  // other than current_literal. Otherwise none of the clause are blocked.
  //
  // TODO(user): If a clause cannot be blocked because of another clause, then
  // when we call ProcessLiteral(current_literal.Negated()) we can skip some
  // inspection.
  for (const ClauseIndex i : clauses_to_process) {
    const auto c = clauses_[i]->AsSpan();
    if (ClauseIsBlocked(current_literal, c)) {
      // Reprocess all clauses that have a negated literal in this one as
      // some might be blocked now.
      //
      // TODO(user): Maybe we can remember for which (literal, clause) pair this
      // was used as a certificate for "not-blocked" and just reprocess those,
      // but it might be memory intensive.
      for (const Literal l : c) {
        if (!in_queue_[l.NegatedIndex()]) {
          in_queue_[l.NegatedIndex()] = true;
          queue_.push_back(l.Negated());
        }
      }

      // Add the clause to the postsolving set.
      postsolve_->AddClauseWithSpecialLiteral(current_literal, c);

      // We can remove a blocked clause.
      ++num_blocked_clauses_;
      clause_manager_->InprocessingRemoveClause(clauses_[i]);
    }
  }
}

// Note that this assume that the binary clauses have already been checked.
bool BlockedClauseSimplifier::ClauseIsBlocked(
    Literal current_literal, absl::Span<const Literal> clause) {
  bool is_blocked = true;
  for (const Literal l : clause) marked_[l.Index()] = true;

  // TODO(user): For faster reprocessing of the same literal, we should move
  // all clauses that are used in a non-blocked certificate first in the list.
  for (const ClauseIndex i :
       literal_to_clauses_[current_literal.NegatedIndex()]) {
    if (clauses_[i]->empty()) continue;
    bool some_marked = false;
    for (const Literal l : clauses_[i]->AsSpan()) {
      // TODO(user): we can be faster here by only updating it at the end?
      ++num_inspected_literals_;

      if (l == current_literal.Negated()) continue;
      if (marked_[l.NegatedIndex()]) {
        some_marked = true;
        break;
      }
    }
    if (!some_marked) {
      is_blocked = false;
      break;
    }
  }

  for (const Literal l : clause) marked_[l.Index()] = false;
  return is_blocked;
}

bool BoundedVariableElimination::DoOneRound(bool log_info) {
  WallTimer wall_timer;
  wall_timer.Start();

  dtime_ = 0.0;
  num_inspected_literals_ = 0;
  num_eliminated_variables_ = 0;
  num_literals_diff_ = 0;
  num_clauses_diff_ = 0;
  num_simplifications_ = 0;
  num_blocked_clauses_ = 0;

  clauses_.clear();
  clause_manager_->DeleteRemovedClauses();
  clause_manager_->DetachAllClauses();
  for (SatClause* c : clause_manager_->AllClausesInCreationOrder()) {
    // We ignore redundant clause. This shouldn't cause any validity issue.
    // TODO(user): but we shouldn't keep clauses containing removed literals.
    // It is still valid to do so, but it should be less efficient.
    if (clause_manager_->IsRemovable(c)) continue;

    clauses_.push_back(c);
  }
  const int num_literals = clause_manager_->literal_size();
  const int num_variables = num_literals / 2;

  literal_to_clauses_.clear();
  literal_to_clauses_.resize(num_literals);
  literal_to_num_clauses_.assign(num_literals, 0);
  for (ClauseIndex i(0); i < clauses_.size(); ++i) {
    for (const Literal l : clauses_[i]->AsSpan()) {
      literal_to_clauses_[l.Index()].push_back(i);
      literal_to_num_clauses_[l.Index()]++;
    }
    num_inspected_literals_ += clauses_[i]->size();
  }

  const int saved_trail_index = trail_->Index();
  propagation_index_ = trail_->Index();

  need_to_be_updated_.clear();
  in_need_to_be_updated_.resize(num_variables);
  queue_.Reserve(num_variables);
  for (BooleanVariable v(0); v < num_variables; ++v) {
    if (assignment_.VariableIsAssigned(v)) continue;
    if (implication_graph_->IsRemoved(Literal(v, true))) continue;
    UpdatePriorityQueue(v);
  }

  marked_.resize(num_literals);
  DCHECK(
      std::all_of(marked_.begin(), marked_.end(), [](bool b) { return !b; }));

  // TODO(user): add a local dtime limit for the corner case where this take too
  // much time. We can adapt the limit depending on how much we want to spend on
  // inprocessing.
  while (!time_limit_->LimitReached() && !queue_.IsEmpty()) {
    const BooleanVariable top = queue_.Top().var;
    queue_.Pop();

    // Make sure we fix variables first if needed. Note that because new binary
    // clause might appear when we fix variables, we need a loop here.
    //
    // TODO(user): we might also find new equivalent variable l => var => l
    // here, but for now we ignore those.
    bool is_unsat = false;
    if (!Propagate()) return false;
    while (implication_graph_->FindFailedLiteralAroundVar(top, &is_unsat)) {
      if (!Propagate()) return false;
    }
    if (is_unsat) return false;

    if (!CrossProduct(top)) return false;

    for (const BooleanVariable v : need_to_be_updated_) {
      in_need_to_be_updated_[v] = false;

      // Currently we never re-add top if we just processed it.
      if (v != top) UpdatePriorityQueue(v);
    }
    in_need_to_be_updated_.clear();
    need_to_be_updated_.clear();
  }

  implication_graph_->CleanupAllRemovedVariables();

  // Remove all redundant clause containing a removed literal. This avoid to
  // re-introduce a removed literal via conflict learning.
  for (SatClause* c : clause_manager_->AllClausesInCreationOrder()) {
    if (!clause_manager_->IsRemovable(c)) continue;
    bool remove = false;
    for (const Literal l : c->AsSpan()) {
      if (implication_graph_->IsRemoved(l)) {
        remove = true;
        break;
      }
    }
    if (remove) clause_manager_->InprocessingRemoveClause(c);
  }

  // Release some memory.
  literal_to_clauses_.clear();
  literal_to_num_clauses_.clear();

  dtime_ += 1e-8 * num_inspected_literals_;
  time_limit_->AdvanceDeterministicTime(dtime_);
  log_info |= VLOG_IS_ON(1);
  LOG_IF(INFO, log_info) << "BVE."
                         << " num_fixed: "
                         << trail_->Index() - saved_trail_index
                         << " num_simplified_literals: " << num_simplifications_
                         << " num_blocked_clauses_: " << num_blocked_clauses_
                         << " num_eliminations: " << num_eliminated_variables_
                         << " num_literals_diff: " << num_literals_diff_
                         << " num_clause_diff: " << num_clauses_diff_
                         << " dtime: " << dtime_
                         << " wtime: " << wall_timer.Get();
  return true;
}

bool BoundedVariableElimination::RemoveLiteralFromClause(
    Literal lit, SatClause* sat_clause) {
  num_literals_diff_ -= sat_clause->size();
  resolvant_.clear();
  for (const Literal l : sat_clause->AsSpan()) {
    if (l == lit || assignment_.LiteralIsFalse(l)) {
      literal_to_num_clauses_[l.Index()]--;
      continue;
    }
    if (assignment_.LiteralIsTrue(l)) {
      num_clauses_diff_--;
      clause_manager_->InprocessingRemoveClause(sat_clause);
      return true;
    }
    resolvant_.push_back(l);
  }
  if (!clause_manager_->InprocessingRewriteClause(sat_clause, resolvant_)) {
    return false;
  }
  if (sat_clause->empty()) {
    --num_clauses_diff_;
    for (const Literal l : resolvant_) literal_to_num_clauses_[l.Index()]--;
  } else {
    num_literals_diff_ += sat_clause->size();
  }
  return true;
}

bool BoundedVariableElimination::Propagate() {
  for (; propagation_index_ < trail_->Index(); ++propagation_index_) {
    // Make sure we always propagate the binary clauses first.
    if (!implication_graph_->Propagate(trail_)) return false;

    const Literal l = (*trail_)[propagation_index_];
    for (const ClauseIndex index : literal_to_clauses_[l.Index()]) {
      if (clauses_[index]->empty()) continue;
      num_clauses_diff_--;
      num_literals_diff_ -= clauses_[index]->size();
      clause_manager_->InprocessingRemoveClause(clauses_[index]);
    }
    literal_to_clauses_[l.Index()].clear();
    for (const ClauseIndex index : literal_to_clauses_[l.NegatedIndex()]) {
      if (clauses_[index]->empty()) continue;
      if (!RemoveLiteralFromClause(l.Negated(), clauses_[index])) return false;
    }
    literal_to_clauses_[l.NegatedIndex()].clear();
  }
  return true;
}

// Note that we use the estimated size here to make it fast. It is okay if the
// order of elimination is not perfect... We can improve on this later.
int BoundedVariableElimination::NumClausesContaining(Literal l) {
  return literal_to_num_clauses_[l.Index()] +
         implication_graph_->DirectImplicationsEstimatedSize(l.Negated());
}

// TODO(user): Only enqueue variable that can be removed.
void BoundedVariableElimination::UpdatePriorityQueue(BooleanVariable var) {
  if (assignment_.VariableIsAssigned(var)) return;
  const int priority = -NumClausesContaining(Literal(var, true)) -
                       NumClausesContaining(Literal(var, false));
  if (queue_.Contains(var.value())) {
    queue_.ChangePriority({var, priority});
  } else {
    queue_.Add({var, priority});
  }
}

void BoundedVariableElimination::DeleteClause(SatClause* sat_clause) {
  const auto clause = sat_clause->AsSpan();

  num_clauses_diff_--;
  num_literals_diff_ -= clause.size();

  // Update literal <-> clause graph.
  for (const Literal l : clause) {
    literal_to_num_clauses_[l.Index()]--;
    if (!in_need_to_be_updated_[l.Variable()]) {
      in_need_to_be_updated_[l.Variable()] = true;
      need_to_be_updated_.push_back(l.Variable());
    }
  }

  // Lazy deletion of the clause.
  clause_manager_->InprocessingRemoveClause(sat_clause);
}

void BoundedVariableElimination::DeleteAllClausesContaining(Literal literal) {
  for (const ClauseIndex i : literal_to_clauses_[literal.Index()]) {
    const auto clause = clauses_[i]->AsSpan();
    if (clause.empty()) continue;
    postsolve_->AddClauseWithSpecialLiteral(literal, clause);
    DeleteClause(clauses_[i]);
  }
  literal_to_clauses_[literal.Index()].clear();
}

void BoundedVariableElimination::AddClause(absl::Span<const Literal> clause) {
  SatClause* pt = clause_manager_->InprocessingAddClause(clause);
  if (pt == nullptr) return;

  num_clauses_diff_++;
  num_literals_diff_ += clause.size();

  const ClauseIndex index(clauses_.size());
  clauses_.push_back(pt);
  for (const Literal l : clause) {
    literal_to_num_clauses_[l.Index()]++;
    literal_to_clauses_[l.Index()].push_back(index);
    if (!in_need_to_be_updated_[l.Variable()]) {
      in_need_to_be_updated_[l.Variable()] = true;
      need_to_be_updated_.push_back(l.Variable());
    }
  }
}

template <bool score_only, bool with_binary_only>
bool BoundedVariableElimination::ResolveAllClauseContaining(Literal lit) {
  const int clause_weight = parameters_.presolve_bve_clause_weight();

  const std::vector<Literal>& implications =
      implication_graph_->DirectImplications(lit);
  auto& clause_containing_lit = literal_to_clauses_[lit.Index()];
  for (int i = 0; i < clause_containing_lit.size(); ++i) {
    const ClauseIndex clause_index = clause_containing_lit[i];
    const auto clause = clauses_[clause_index]->AsSpan();
    if (clause.empty()) continue;

    if (!score_only) resolvant_.clear();
    for (const Literal l : clause) {
      if (!score_only && l != lit) resolvant_.push_back(l);
      marked_[l.Index()] = true;
    }
    num_inspected_literals_ += clause.size() + implications.size();

    // If this is true, then "clause" is subsumed by one of its resolvant and we
    // can just remove lit from it. Then it doesn't need to be acounted at all.
    bool clause_can_be_simplified = false;
    const int64_t saved_score = new_score_;

    // Resolution with binary clauses.
    for (const Literal l : implications) {
      CHECK_NE(l, lit);
      if (marked_[l.NegatedIndex()]) continue;  // trivial.
      if (marked_[l.Index()]) {
        clause_can_be_simplified = true;
        break;
      } else {
        if (score_only) {
          new_score_ += clause_weight + clause.size();
        } else {
          resolvant_.push_back(l);
          AddClause(resolvant_);
          resolvant_.pop_back();
        }
      }
    }

    // Resolution with non-binary clauses.
    if (!with_binary_only && !clause_can_be_simplified) {
      auto& clause_containing_not_lit = literal_to_clauses_[lit.NegatedIndex()];
      for (int j = 0; j < clause_containing_not_lit.size(); ++j) {
        if (score_only && new_score_ > score_threshold_) break;
        const ClauseIndex other_index = clause_containing_not_lit[j];
        const auto other = clauses_[other_index]->AsSpan();
        if (other.empty()) continue;
        bool trivial = false;
        int extra_size = 0;
        for (const Literal l : other) {
          // TODO(user): we can optimize this by updating it outside the loop.
          ++num_inspected_literals_;
          if (l == lit.Negated()) continue;
          if (marked_[l.NegatedIndex()]) {
            trivial = true;
            break;
          }
          if (!marked_[l.Index()]) {
            ++extra_size;
            if (!score_only) resolvant_.push_back(l);
          }
        }
        if (trivial) {
          if (!score_only) resolvant_.resize(resolvant_.size() - extra_size);
          continue;
        }

        // If this is the case, the other clause is subsumed by the resolvant.
        // We can just remove not_lit from it and ignore it.
        if (score_only && clause.size() + extra_size <= other.size()) {
          CHECK_EQ(clause.size() + extra_size, other.size());
          ++num_simplifications_;

          // Note that we update the threshold since this clause was counted in
          // it.
          score_threshold_ -= clause_weight + other.size();

          if (extra_size == 0) {
            // We have a double self-subsumption. We can just remove this
            // clause since it will be subsumed by the clause created in the
            // "clause_can_be_simplified" case below.
            DeleteClause(clauses_[other_index]);
          } else {
            if (!RemoveLiteralFromClause(lit.Negated(),
                                         clauses_[other_index])) {
              return false;
            }
            std::swap(clause_containing_not_lit[j],
                      clause_containing_not_lit.back());
            clause_containing_not_lit.pop_back();
            --j;  // Reprocess the new position.
            continue;
          }
        }

        if (extra_size == 0) {
          clause_can_be_simplified = true;
          break;
        } else {
          if (score_only) {
            // Hack. We do not want to create long clauses during BVE.
            if (clause.size() - 1 + extra_size > 100) {
              new_score_ = score_threshold_ + 1;
              break;
            }

            new_score_ += clause_weight + clause.size() - 1 + extra_size;
          } else {
            AddClause(resolvant_);
            resolvant_.resize(resolvant_.size() - extra_size);
          }
        }
      }
    }

    // Note that we need to clear marked before aborting.
    for (const Literal l : clause) marked_[l.Index()] = false;

    // In this case, we simplify and remove the clause from here.
    if (clause_can_be_simplified) {
      ++num_simplifications_;

      // Note that we update the threshold as if this was simplified before.
      new_score_ = saved_score;
      score_threshold_ -= clause_weight + clause.size();

      if (!RemoveLiteralFromClause(lit, clauses_[clause_index])) return false;
      std::swap(clause_containing_lit[i], clause_containing_lit.back());
      clause_containing_lit.pop_back();
      --i;  // Reprocess the new position.
    }

    if (score_only && new_score_ > score_threshold_) return true;

    // When this happen, then the clause is blocked (i.e. all its resolvant are
    // trivial). So even if we do not actually perform the variable elimination,
    // we can still remove this clause. Note that we treat the score as if the
    // clause was removed before.
    //
    // Tricky: The detection only work if we didn't abort the computation above,
    // so we do that after the score_threshold_ check.
    //
    // TODO(user): Also detect blocked clause for not(lit)? It is not as cheap
    // though and require more code.
    if (score_only && !with_binary_only && !clause_can_be_simplified &&
        new_score_ == saved_score) {
      ++num_blocked_clauses_;
      score_threshold_ -= clause_weight + clause.size();
      postsolve_->AddClauseWithSpecialLiteral(lit, clause);
      DeleteClause(clauses_[clause_index]);
    }
  }
  return true;
}

bool BoundedVariableElimination::CrossProduct(BooleanVariable var) {
  if (assignment_.VariableIsAssigned(var)) return true;

  const Literal lit(var, true);
  const Literal not_lit(var, false);
  {
    const int s1 = NumClausesContaining(lit);
    const int s2 = NumClausesContaining(not_lit);
    if (s1 == 0 && s2 == 0) return true;
    if (s1 > 0 && s2 == 0) {
      num_eliminated_variables_++;
      if (!clause_manager_->InprocessingFixLiteral(lit)) return false;
      DeleteAllClausesContaining(lit);
      return true;
    }
    if (s1 == 0 && s2 > 0) {
      num_eliminated_variables_++;
      if (!clause_manager_->InprocessingFixLiteral(not_lit)) return false;
      DeleteAllClausesContaining(not_lit);
      return true;
    }
    if (implication_graph_->IsRedundant(lit)) {
      // TODO(user): do that elsewhere?
      CHECK_EQ(s1, 1);
      CHECK_EQ(s2, 1);
      CHECK_EQ(implication_graph_->NumImplicationOnVariableRemoval(var), 0);
      num_eliminated_variables_++;
      implication_graph_->RemoveBooleanVariable(var, &postsolve_->clauses);
      return true;
    }

    // Heuristic. Abort if the work required to decide if var should be removed
    // seems to big.
    if (s1 > 1 && s2 > 1 && s1 * s2 > parameters_.presolve_bve_threshold()) {
      return true;
    }
  }

  // TODO(user): swap lit and not_lit for speed? it is unclear if we prefer
  // to minimize the number of clause containing lit or not_lit though. Also,
  // we might want to alternate since we also detect blocked clause containing
  // lit, but don't do it for not_lit.

  // Compute the current score.
  // TODO(user): cleanup the list lazily at the same time?
  int64_t score = 0;
  const int clause_weight = parameters_.presolve_bve_clause_weight();
  score +=
      implication_graph_->DirectImplications(lit).size() * (clause_weight + 2);
  score += implication_graph_->DirectImplications(not_lit).size() *
           (clause_weight + 2);
  for (const ClauseIndex i : literal_to_clauses_[lit.Index()]) {
    const auto c = clauses_[i]->AsSpan();
    if (!c.empty()) score += clause_weight + c.size();
  }
  for (const ClauseIndex i : literal_to_clauses_[not_lit.Index()]) {
    const auto c = clauses_[i]->AsSpan();
    if (!c.empty()) score += clause_weight + c.size();
  }

  // Compute the new score after BVE.
  // Abort as soon as it crosses the threshold.
  //
  // TODO(user): Experiment with leaving the implications graph as is. This will
  // not remove the variable completely, but it seems interesting since after
  // equivalent variable removal and failed literal probing, the cross product
  // of the implication always add a quadratic number of implication, except if
  // the in (or out) degree is zero or one.
  score_threshold_ = score;
  new_score_ = implication_graph_->NumImplicationOnVariableRemoval(var) *
               (clause_weight + 2);
  if (new_score_ > score_threshold_) return true;
  if (!ResolveAllClauseContaining</*score_only=*/true,
                                  /*with_binary_only=*/true>(not_lit)) {
    return false;
  }
  if (new_score_ > score_threshold_) return true;
  if (!ResolveAllClauseContaining</*score_only=*/true,
                                  /*with_binary_only=*/false>(lit)) {
    return false;
  }
  if (new_score_ > score_threshold_) return true;

  // Perform BVE.
  if (new_score_ > 0) {
    if (!ResolveAllClauseContaining</*score_only=*/false,
                                    /*with_binary_only=*/false>(lit)) {
      return false;
    }
    if (!ResolveAllClauseContaining</*score_only=*/false,
                                    /*with_binary_only=*/true>(not_lit)) {
      return false;
    }
  }

  ++num_eliminated_variables_;
  implication_graph_->RemoveBooleanVariable(var, &postsolve_->clauses);
  DeleteAllClausesContaining(lit);
  DeleteAllClausesContaining(not_lit);
  return true;
}

}  // namespace sat
}  // namespace operations_research
