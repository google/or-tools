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

#include "ortools/sat/sat_inprocessing.h"

#include <algorithm>
#include <array>
#include <bitset>
#include <cmath>
#include <cstdint>
#include <deque>
#include <limits>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/cleanup/cleanup.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/inlined_vector.h"
#include "absl/functional/function_ref.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/log/vlog_is_on.h"
#include "absl/types/span.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/strong_vector.h"
#include "ortools/base/timer.h"
#include "ortools/graph_base/connected_components.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/drat_checker.h"
#include "ortools/sat/gate_utils.h"
#include "ortools/sat/linear_programming_constraint.h"
#include "ortools/sat/lrat_proof_handler.h"
#include "ortools/sat/model.h"
#include "ortools/sat/probing.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_decision.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/util.h"
#include "ortools/util/bitset.h"
#include "ortools/util/integer_pq.h"
#include "ortools/util/logging.h"
#include "ortools/util/strong_integers.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

namespace {

bool SomeLiteralAreAssigned(const VariablesAssignment& assignment,
                            absl::Span<const Literal> literals) {
  return absl::c_any_of(
      literals, [&](Literal l) { return assignment.LiteralIsAssigned(l); });
}

}  // namespace

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

  // Mainly useful for development.
  double probing_time = 0.0;
  const bool log_round_info = VLOG_IS_ON(2);

  // In the presolve, we run this first as some preprocessing technique might
  // change the problem clauses in such a way that make our heuristic gate
  // detection miss some gates. Also, when this applies the reduction in problem
  // size is huge, so it is just faster to run this early.
  //
  // TODO(user): If we remove fixed variables, on some problem like:
  // ~/SAT24/f0426369f61595aee97055965ee7e6a3-hwmcc12miters-xits-iso-6s111.sanitized.cnf.xz
  // we don't detect as much equivalences... Understand why. I suspect it is due
  // to the heuristic for ITE gate that combine two clauses of size 3 to get a
  // truth table on 4 variables. If one of them become of size 2, we might miss
  // it. Still we should be more robust to stuff like this.
  RETURN_IF_FALSE(congruence_closure_->DoOneRound(log_round_info));

  // We currently do the transformations in a given order and restart each time
  // we did something to make sure that the earlier step cannot strengthen more.
  // This might not be the best, but it is really good during development phase
  // to make sure each individual functions is as incremental and as fast as
  // possible.
  const double start_dtime = time_limit_->GetElapsedDeterministicTime();
  const double stop_dtime = start_dtime + options.deterministic_time_limit;
  while (!time_limit_->LimitReached() &&
         time_limit_->GetElapsedDeterministicTime() <= stop_dtime) {
    CHECK_EQ(sat_solver_->CurrentDecisionLevel(), 0);
    if (!LevelZeroPropagate()) return false;

    // This one is fast since only new implications or new fixed variables are
    // considered.
    RETURN_IF_FALSE(implication_graph_->RemoveDuplicatesAndFixedVariables());

    // This also prepare the stamping below so that we do that on a DAG and do
    // not consider potential new implications added by
    // RemoveFixedAndEquivalentVariables().
    RETURN_IF_FALSE(DetectEquivalencesAndStamp(options.use_transitive_reduction,
                                               log_round_info));

    // TODO(user): This should/could be integrated with the stamping since it
    // seems better to do just one loop instead of two over all clauses. Because
    // of memory access. it isn't that clear though.
    RETURN_IF_FALSE(RemoveFixedAndEquivalentVariables(log_round_info));

    // IMPORTANT: Since we only run this on pure sat problem, we can just
    // get rid of equivalent variable right away and do not need to keep them
    // in the implication_graph_ for propagation.
    //
    // This is needed for the correctness of the bounded variable elimination.
    implication_graph_->RemoveAllRedundantVariables(&postsolve_->clauses);

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

    // SAT sweeping has a small dtime limit, so do it before other heuristics
    // exhaust our budget.
    if (options.use_equivalence_sat_sweeping &&
        stop_dtime > time_limit_->GetElapsedDeterministicTime()) {
      auto inner_model_inprocessing = [&](Model* inner_model) {
        inner_model->GetOrCreate<SatParameters>()
            ->set_inprocessing_use_sat_sweeping(false);
        inner_model->GetOrCreate<Inprocessing>()->InprocessingRound();
      };
      RETURN_IF_FALSE(LevelZeroPropagate());
      RETURN_IF_FALSE(
          equivalence_sat_sweeping_->DoOneRound(inner_model_inprocessing));
      RETURN_IF_FALSE(LevelZeroPropagate());
      implication_graph_->RemoveAllRedundantVariables(&postsolve_->clauses);
    }

    // TODO(user): Combine the two? this way we don't create a full literal <->
    // clause graph twice. It might make sense to reach the BCE fix point which
    // is unique before each variable elimination.
    if (!params_.fill_tightened_domains_in_response()) {
      blocked_clause_simplifier_->DoOneRound(log_round_info);
    }

    // TODO(user): this break some binary graph invariant. Fix!
    RETURN_IF_FALSE(RemoveFixedAndEquivalentVariables(log_round_info));
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
    RETURN_IF_FALSE(failed_literal_probing_->DoOneRound(probing_options));
    probing_time += wall_timer.Get() - saved_wtime;

    if (MoreFixedVariableToClean() || MoreRedundantVariableToClean() ||
        !implication_graph_->IsDag()) {
      continue;
    }

    break;
  }

  // Tricky: It is important to clean-up any potential equivalence left in
  // case we aborted early due to the limit.
  RETURN_IF_FALSE(RemoveFixedAndEquivalentVariables(log_round_info));
  if (!LevelZeroPropagate()) return false;

  // TODO(user): Maintain the total number of literals in the watched clauses.
  SOLVER_LOG(
      logger_, "[Pure SAT presolve]",
      " num_fixed: ", FormatCounter(trail_->Index()), " num_equiv: ",
      FormatCounter(implication_graph_->num_current_equivalences() / 2), "/",
      FormatCounter(sat_solver_->NumVariables()), " num_implications: ",
      FormatCounter(implication_graph_->ComputeNumImplicationsForLog()),
      " num_watched_clauses: ",
      FormatCounter(clause_manager_->num_watched_clauses()),
      " dtime: ", time_limit_->GetElapsedDeterministicTime() - start_dtime, "/",
      options.deterministic_time_limit, " wtime: ", wall_timer.Get(),
      " non-probing time: ", (wall_timer.Get() - probing_time));
  return true;
}

bool Inprocessing::InprocessingRound() {
  DCHECK_EQ(sat_solver_->CurrentDecisionLevel(), 0);
  if (sat_solver_->ModelIsUnsat()) return false;
  WallTimer wall_timer;
  wall_timer.Start();

  const bool log_info = VLOG_IS_ON(2);
  const bool log_round_info = VLOG_IS_ON(3);
  const double start_dtime = time_limit_->GetElapsedDeterministicTime();

  // Mainly useful for development.
  double probing_time = 0.0;

  // Store the dtime of the first call (first restart) and wait for the next
  // restart.
  if (first_inprocessing_call_) {
    reference_dtime_ = start_dtime;
    first_inprocessing_call_ = false;
    return true;
  }

  // Try to spend a given ratio of time in the inprocessing.
  //
  // TODO(user): Tune the heuristic, in particular, with the current code we
  // start some inprocessing before the first search.
  const double diff = start_dtime - reference_dtime_;
  if (total_dtime_ > params_.inprocessing_dtime_ratio() * diff) {
    return true;
  }

  // LP Propagation during inprocessing can be really slow, so we temporarily
  // disable it.
  //
  // TODO(user): The LP and incremental structure will still be called though,
  // which can take some time, try to disable it more cleanly.
  std::vector<std::pair<LinearProgrammingConstraint*, bool>> saved_state;
  for (LinearProgrammingConstraint* lp : *all_lp_constraints_) {
    saved_state.push_back({lp, lp->PropagationIsEnabled()});
    lp->EnablePropagation(false);
  }
  auto cleanup = absl::MakeCleanup([&saved_state]() {
    for (const auto [lp, old_value] : saved_state) {
      lp->EnablePropagation(old_value);
    }
  });

  // We make sure we do not "pollute" the current saved polarities. We will
  // restore them at the end.
  //
  // TODO(user): We should probably also disable the variable/clauses activity
  // updates.
  decision_policy_->MaybeEnablePhaseSaving(/*save_phase=*/false);

  RETURN_IF_FALSE(implication_graph_->RemoveDuplicatesAndFixedVariables());
  RETURN_IF_FALSE(DetectEquivalencesAndStamp(true, log_round_info));
  RETURN_IF_FALSE(RemoveFixedAndEquivalentVariables(log_round_info));
  RETURN_IF_FALSE(LevelZeroPropagate());

  // Probing.
  //
  // TODO(user): right now we can't run probing if the solver is configured
  // with assumption. Fix.
  if (params_.inprocessing_probing_dtime() > 0.0 &&
      sat_solver_->AssumptionLevel() == 0) {
    const double saved_wtime = wall_timer.Get();
    ProbingOptions probing_options;
    probing_options.log_info = log_round_info;
    // Tuning for solving very easy problems very fast: do not spend more than
    // 10% of the total elapsed time in this single probing round.
    probing_options.deterministic_limit =
        std::min(params_.inprocessing_probing_dtime(),
                 time_limit_->GetElapsedDeterministicTime() / 10);
    probing_options.extract_binary_clauses = true;
    RETURN_IF_FALSE(failed_literal_probing_->DoOneRound(probing_options));
    probing_time += wall_timer.Get() - saved_wtime;
  }

  RETURN_IF_FALSE(DetectEquivalencesAndStamp(true, log_round_info));
  RETURN_IF_FALSE(RemoveFixedAndEquivalentVariables(log_round_info));
  RETURN_IF_FALSE(LevelZeroPropagate());

  RETURN_IF_FALSE(stamping_simplifier_->DoOneRound(log_round_info));
  RETURN_IF_FALSE(RemoveFixedAndEquivalentVariables(log_round_info));
  RETURN_IF_FALSE(LevelZeroPropagate());

  if (params_.inprocessing_minimization_dtime() > 0.0) {
    RETURN_IF_FALSE(vivifier_->MinimizeByPropagation(
        log_round_info,
        std::min(params_.inprocessing_minimization_dtime(),
                 time_limit_->GetElapsedDeterministicTime() / 10)));
  }

  // TODO(user): Think about the right order in this function.
  if (params_.inprocessing_use_congruence_closure()) {
    RETURN_IF_FALSE(RemoveFixedAndEquivalentVariables(log_round_info));
    RETURN_IF_FALSE(implication_graph_->RemoveDuplicatesAndFixedVariables());
    RETURN_IF_FALSE(congruence_closure_->DoOneRound(log_round_info));
  }

  RETURN_IF_FALSE(RemoveFixedAndEquivalentVariables(log_round_info));
  RETURN_IF_FALSE(SubsumeAndStrenghtenRound(log_round_info));
  RETURN_IF_FALSE(RemoveFixedAndEquivalentVariables(log_round_info));

  // TODO(user): try to enable these? The problem is that we can only remove
  // variables not used the non-pure SAT part of a model.
  if (/*DISABLES_CODE*/ (false)) {
    blocked_clause_simplifier_->DoOneRound(log_round_info);
    RETURN_IF_FALSE(bounded_variable_elimination_->DoOneRound(log_round_info));
  }
  RETURN_IF_FALSE(LevelZeroPropagate());

  if (params_.inprocessing_use_sat_sweeping()) {
    auto inner_model_inprocessing = [&](Model* inner_model) {
      inner_model->GetOrCreate<SatParameters>()
          ->set_inprocessing_use_sat_sweeping(false);
      inner_model->GetOrCreate<Inprocessing>()->InprocessingRound();
    };
    RETURN_IF_FALSE(
        equivalence_sat_sweeping_->DoOneRound(inner_model_inprocessing));
    RETURN_IF_FALSE(LevelZeroPropagate());
  }
  sat_solver_->AdvanceDeterministicTime(time_limit_);
  total_dtime_ += time_limit_->GetElapsedDeterministicTime() - start_dtime;
  if (log_info) {
    const int num_fixed = trail_->Index();
    const int num_equiv = implication_graph_->num_current_equivalences() / 2;

    FORCED_SOLVER_LOG(
        logger_, "Inprocessing.", " fixed:", FormatCounter(trail_->Index()),
        " equiv:", FormatCounter(num_equiv), " left:",
        FormatCounter(sat_solver_->NumVariables() - num_fixed - num_equiv),
        " binary:",
        FormatCounter(implication_graph_->ComputeNumImplicationsForLog()),
        " clauses:",
        FormatCounter(clause_manager_->num_watched_clauses() -
                      clause_manager_->num_removable_clauses()),
        "|", FormatCounter(clause_manager_->num_removable_clauses()), "|",
        FormatCounter(sat_solver_->counters().num_deleted_clauses), "|",
        FormatCounter(sat_solver_->num_failures()),
        " minimization:", FormatCounter(vivifier_->last_num_vivified()), "|",
        FormatCounter(vivifier_->last_num_literals_removed()),
        " dtime:", time_limit_->GetElapsedDeterministicTime() - start_dtime,
        " wtime:", wall_timer.Get(),
        " np_wtime:", (wall_timer.Get() - probing_time));
  }

  DCHECK_EQ(sat_solver_->CurrentDecisionLevel(), 0);
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
  if (!sat_solver_->Propagate()) {
    // This adds the UNSAT proof to the LRAT handler, if any.
    sat_solver_->ProcessCurrentConflict();
    return false;
  }
  return true;
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

  // Start the round.
  WallTimer wall_timer;
  wall_timer.Start();

  int64_t num_removed_literals = 0;
  int64_t num_inspected_literals = 0;

  // We use a huge "limit" in case we have some bad interactions and we need to
  // loop often to reach the fixed point.
  while (num_inspected_literals < 1e10) {
    // Test if some work is needed.
    //
    // TODO(user): If only new fixed variables are there, we can use a faster
    // function. We should also merge the code with the deletion code in
    // sat_solver_.cc, but that require some refactoring of the dependence
    // between files.
    const int64_t new_num_redundant_literals =
        implication_graph_->num_redundant_literals();
    const int64_t new_num_fixed_variables = trail_->Index();
    if (last_num_redundant_literals_ == new_num_redundant_literals &&
        last_num_fixed_variables_ == new_num_fixed_variables) {
      return true;
    }
    last_num_fixed_variables_ = new_num_fixed_variables;
    last_num_redundant_literals_ = new_num_redundant_literals;

    // We need this temporary vector for the LRAT proof settings, otherwise
    // we could just have done an in-place transformation.
    std::vector<Literal> new_clause;

    // Used to mark clause literals.
    const int num_literals(sat_solver_->NumVariables() * 2);
    util_intops::StrongVector<LiteralIndex, bool> marked(num_literals, false);

    clause_manager_->DeleteRemovedClauses();
    clause_manager_->DetachAllClauses();
    for (SatClause* clause : clause_manager_->AllClausesInCreationOrder()) {
      bool removed = false;
      bool need_rewrite = false;

      // We first do a loop to see if there is anything to do.
      for (const Literal l : clause->AsSpan()) {
        if (assignment_.LiteralIsTrue(l)) {
          clause_manager_->LazyDelete(clause,
                                      DeletionSourceForStat::FIXED_AT_TRUE);
          num_removed_literals += clause->size();
          removed = true;
          break;
        }
        if (assignment_.LiteralIsFalse(l) ||
            implication_graph_->IsRedundant(l)) {
          need_rewrite = true;
          break;
        }
      }

      num_inspected_literals += clause->size();
      if (removed || !need_rewrite) continue;
      num_inspected_literals += clause->size();

      // Rewrite the clause.
      new_clause.clear();
      tmp_proof_.clear();
      for (const Literal l : clause->AsSpan()) {
        const Literal r = implication_graph_->RepresentativeOf(l);
        if (lrat_proof_handler_ != nullptr) {
          if (!marked[r] && assignment_.LiteralIsFalse(r)) {
            tmp_proof_.push_back(ClausePtr(r.Negated()));
          }
          if (r != l) {
            tmp_proof_.push_back(ClausePtr(l.Negated(), r));
          }
        }
        if (marked[r] || assignment_.LiteralIsFalse(r)) {
          continue;
        }
        if (marked[r.NegatedIndex()] || assignment_.LiteralIsTrue(r)) {
          clause_manager_->LazyDelete(
              clause, DeletionSourceForStat::CONTAINS_L_AND_NOT_L);
          num_removed_literals += clause->size();
          removed = true;
          break;
        }
        marked[r] = true;
        new_clause.push_back(r);
      }

      // Restore marked.
      for (const Literal l : new_clause) marked[l] = false;
      if (removed) continue;

      if (lrat_proof_handler_ != nullptr) {
        tmp_proof_.push_back(ClausePtr(clause));
      }
      num_removed_literals += clause->size() - new_clause.size();
      if (!clause_manager_->InprocessingRewriteClause(clause, new_clause,
                                                      tmp_proof_)) {
        return false;
      }
    }

    // If clause became binary, make sure to clean up the relevant implication
    // lists. This should be fast in all cases since it is incremental.
    //
    // Tricky: This might fix more variables in some corner case, so we need to
    // loop to reach the "fixed point" and maintain the invariant that no clause
    // contain fixed variable.
    if (!implication_graph_->RemoveDuplicatesAndFixedVariables()) return false;
  }

  // Invariant. There should be no clause with fixed variables left.
  if (DEBUG_MODE) {
    for (SatClause* clause : clause_manager_->AllClausesInCreationOrder()) {
      CHECK(!SomeLiteralAreAssigned(trail_->Assignment(), clause->AsSpan()));
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

  // We need this temporary vector for the LRAT proof settings, otherwise
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
  std::vector<SatClause*> clauses_copy =
      clause_manager_->AllClausesInCreationOrder();
  absl::c_stable_sort(clauses_copy, [](SatClause* a, SatClause* b) {
    return a->size() < b->size();
  });
  absl::Span<SatClause*> clauses = absl::MakeSpan(clauses_copy);

  // Used to mark clause literals.
  const LiteralIndex num_literals(sat_solver_->NumVariables() * 2);
  SparseBitset<LiteralIndex> marked(num_literals);

  // Clause index in clauses.
  // TODO(user): Storing signatures here might be faster?
  util_intops::StrongVector<LiteralIndex, absl::InlinedVector<int, 6>>
      one_watcher(num_literals.value());

  // Clause signatures in the same order as clauses.
  std::vector<uint64_t> signatures(clauses.size());

  absl::flat_hash_set<int> delayed_to_subsume;
  std::vector<SatClause*> unary_or_binary;

  // Literals which can be removed, and the reason why.
  std::vector<std::pair<Literal, int>> candidates_for_removal;
  for (int clause_index = 0; clause_index < clauses.size(); ++clause_index) {
    SatClause* clause = clauses[clause_index];
    DCHECK(!SomeLiteralAreAssigned(trail_->Assignment(), clause->AsSpan()));

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
    marked.ResetAllToFalse();
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
      num_inspected_signatures += one_watcher[l].size();
      for (const int i : one_watcher[l]) {
        if ((mask & signatures[i]) != 0) continue;

        bool subsumed = true;
        bool strengthen = true;
        LiteralIndex to_remove = kNoLiteralIndex;
        num_inspected_literals += clauses[i]->size();
        for (const Literal o : clauses[i]->AsSpan()) {
          if (!marked[o]) {
            subsumed = false;
            if (to_remove == kNoLiteralIndex && marked[o.NegatedIndex()]) {
              to_remove = o.NegatedIndex();
            } else {
              strengthen = false;
              break;
            }
          }
        }
        if (subsumed) {
          ++num_subsumed_clauses;
          num_removed_literals += clause->size();
          clause_manager_->LazyDelete(
              clause, DeletionSourceForStat::SUBSUMPTION_INPROCESSING);
          removed = true;
          break;
        }
        if (strengthen) {
          CHECK_NE(kNoLiteralIndex, to_remove);
          candidates_for_removal.emplace_back(Literal(to_remove), i);
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

        bool strengthen = true;
        num_inspected_literals += clauses[i]->size();
        for (const Literal o : clauses[i]->AsSpan()) {
          if (o == l.Negated()) continue;
          if (!marked[o]) {
            strengthen = false;
            break;
          }
        }
        if (strengthen) {
          candidates_for_removal.emplace_back(l, i);
        }
      }
    }

    // Any literal here can be removed, but afterwards the others might not.
    // So we re-check each potential removal one by one.
    for (const auto [target_lit, resolvant_index] : candidates_for_removal) {
      // Check if this is still a valid resolvant.
      const SatClause* resolvant = clauses[resolvant_index];
      if (resolvant->size() > clause->size()) continue;
      if (!marked[target_lit]) continue;
      bool skip = false;
      for (const Literal l : resolvant->AsSpan()) {
        if (l == target_lit.Negated()) continue;
        if (!marked[l]) skip = true;
      }
      if (skip) continue;

      // We properly checked that target_lit appear in clause and negatively in
      // resolvant (precondition). And also that all other literals of resolvant
      // appear in clause. We can remove target_lit.
      new_clause.clear();
      for (const Literal l : clause->AsSpan()) {
        if (l == target_lit) continue;
        new_clause.push_back(l);
      }
      marked.Clear(target_lit);
      CHECK_EQ(new_clause.size() + 1, clause->size());

      // The resolvant is now subsumed by the strengthened clause.
      if (resolvant->size() == clause->size()) {
        // The resolvant was already kept forever and is subsumed by this clause
        // so it make sense to just keep it instead.
        clause_manager_->KeepClauseForever(clause);
        delayed_to_subsume.insert(resolvant_index);
      }

      num_removed_literals += clause->size() - new_clause.size();
      if (lrat_proof_handler_ != nullptr) {
        // The id shouldn't change, this is just a rewrite.
        lrat_proof_handler_->RewriteClause(
            ClausePtr(clause), new_clause,
            {ClausePtr(resolvant), ClausePtr(clause)});
      }
      clause_manager_->InprocessingTemporaryRewrite(clause, new_clause);
      if (new_clause.empty()) return false;  // UNSAT

      // Recompute signature.
      signature = 0;
      for (const Literal l : clause->AsSpan()) {
        signature |= (uint64_t{1} << (l.Variable().value() % 64));
      }
    }

    if (clause->size() <= 2) {
      // We will later "promote" them, but for now we keep them around
      // to keep subsuming with them!
      unary_or_binary.push_back(clause);
      CHECK(!clause_manager_->IsRemovable(clause));
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
        if (one_watcher[l].size() < min_size) {
          min_size = one_watcher[l].size();
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

  // Remove all element in "delayed_to_subsume". Note that even if the order is
  // non-deterministic, this should not matter, as we just remove clauses.
  //
  // TODO(user): maybe we could have done that as we discovered them, but
  // then the one_watcher might contain empty clause, and we have to deal with
  // that.
  for (const int index : delayed_to_subsume) {
    SatClause* clause = clauses[index];
    if (clause->size() <= 2) continue;
    ++num_subsumed_clauses;
    num_removed_literals += clause->size();
    clause_manager_->LazyDelete(
        clause, DeletionSourceForStat::SUBSUMPTION_INPROCESSING);
  }

  // Convert unary and binary clauses once all clauses have been processed.
  for (SatClause* clause : unary_or_binary) {
    if (!clause_manager_->InprocessingCleanUpUnaryOrBinaryClause(clause)) {
      return false;
    }
  }

  // We might have fixed variables, finish the propagation.
  if (!LevelZeroPropagate()) return false;

  // TODO(user): tune the deterministic time.
  const double dtime = static_cast<double>(num_inspected_signatures) * 1e-8 +
                       static_cast<double>(num_inspected_literals) * 5e-9;
  time_limit_->AdvanceDeterministicTime(dtime);
  LOG_IF(INFO, log_info) << "Subsume. num_removed_literals: "
                         << FormatCounter(num_removed_literals)
                         << " num_subsumed: "
                         << FormatCounter(num_subsumed_clauses)
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

  if (implication_graph_->IsEmpty()) return true;

  if (!stamps_are_already_computed_) {
    // We need a DAG so that we don't have cycle while we sample the tree.
    // TODO(user): We could probably deal with it if needed so that we don't
    // need to do equivalence detection each time we want to run this.
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

  if (implication_graph_->IsEmpty()) return true;

  implication_graph_->RemoveFixedVariables();
  if (!implication_graph_->DetectEquivalences(log_info)) return true;
  SampleTreeAndFillParent();
  if (!ComputeStamps()) return false;
  stamps_are_already_computed_ = true;

  // TODO(user): compute some dtime, it is always zero currently.
  time_limit_->AdvanceDeterministicTime(dtime_);
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
    for (int num_tries = 0; num_tries < 10; ++num_tries) {
      // We look for a random lit that implies i. For that we take a random
      // literal in the direct implications of not(i) and reverse it.
      const LiteralIndex index =
          implication_graph_->RandomImpliedLiteral(Literal(i).Negated());
      if (index == kNoLiteralIndex) break;

      const Literal candidate = Literal(index).Negated();
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

  // Adjacency list representation of the parents_ tree.
  util_intops::StrongVector<LiteralIndex, int> sizes;
  util_intops::StrongVector<LiteralIndex, int> starts;
  std::vector<LiteralIndex> children;

  // Compute sizes.
  sizes.assign(size, 0);
  for (LiteralIndex i(0); i < size; ++i) {
    if (parents_[i] == i) continue;  // leaf.
    sizes[parents_[i]]++;
  }

  // Compute starts in the children_ vector for each node.
  starts.resize(size + 1);  // We use a sentinel.
  starts[LiteralIndex(0)] = 0;
  for (LiteralIndex i(1); i <= size; ++i) {
    starts[i] = starts[i - 1] + sizes[i - 1];
  }

  // Fill children. This messes up starts_.
  children.resize(size);
  for (LiteralIndex i(0); i < size; ++i) {
    if (parents_[i] == i) continue;  // leaf.
    children[starts[parents_[i]]++] = i;
  }

  // Reset starts to correct value.
  for (LiteralIndex i(0); i < size; ++i) {
    starts[i] -= sizes[i];
  }

  if (DEBUG_MODE) {
    CHECK_EQ(starts[LiteralIndex(0)], 0);
    for (LiteralIndex i(1); i <= size; ++i) {
      CHECK_EQ(starts[i], starts[i - 1] + sizes[i - 1]);
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
        if (lrat_proof_handler_ != nullptr) {
          tmp_proof_.clear();
          AppendImplicationChain(Literal(lca), Literal(top), tmp_proof_);
          AppendImplicationChain(Literal(lca), Literal(top).Negated(),
                                 tmp_proof_);
        }
        if (!clause_manager_->InprocessingFixLiteral(Literal(lca).Negated(),
                                                     tmp_proof_)) {
          return false;
        }
      }

      const int end = starts[top + 1];  // Ok with sentinel.
      for (int j = starts[top]; j < end; ++j) {
        DCHECK_NE(top, children[j]);    // We removed leaf self-loop.
        DCHECK(!marked_[children[j]]);  // This is a tree.
        dfs_stack_.push_back(children[j]);
      }
    }
  }
  DCHECK_EQ(stamp, 2 * size);
  return true;
}

namespace {
class LratStampingHelper {
 public:
  void NewClause(absl::Span<const Literal> clause) {
    clause_ = clause;
    has_literals_to_remove_ = false;
  }

  void AddToRemove(int index, int reason, bool negated) {
    if (!has_literals_to_remove_) {
      has_literals_to_remove_ = true;
      status_.assign(clause_.size(), {0, 0, false, false});
    }
    status_[index].reason = reason;
    status_[index].negated = negated;
    status_[index].to_remove = true;
  }

  void AppendImplicationChains(
      absl::FunctionRef<void(Literal, Literal, bool)> append_chain) {
    if (!has_literals_to_remove_) return;
    // The proof for removing a literal 'a' can depend on another removed
    // literal 'b'. In this case the proof that 'b' can be removed must appear
    // before the one for 'a'. To ensure this we process them in topological
    // order.
    for (const Status& status : status_) {
      if (status.to_remove && status_[status.reason].to_remove) {
        status_[status.reason].num_children++;
      }
    }
    reverse_removal_order_.clear();
    for (int i = 0; i < status_.size(); ++i) {
      if (status_[i].to_remove && status_[i].num_children == 0) {
        reverse_removal_order_.push_back(i);
      }
    }
    int num_visited = 0;
    while (num_visited < reverse_removal_order_.size()) {
      int parent = status_[reverse_removal_order_[num_visited++]].reason;
      if (--status_[parent].num_children == 0) {
        reverse_removal_order_.push_back(parent);
      }
    }
    for (int i = reverse_removal_order_.size() - 1; i >= 0; --i) {
      const int index = reverse_removal_order_[i];
      const Status& status = status_[index];
      DCHECK(status.to_remove);
      if (status.negated) {
        append_chain(clause_[status.reason].Negated(), clause_[index].Negated(),
                     /*reversed=*/false);
      } else {
        append_chain(clause_[index], clause_[status.reason], /*reversed=*/true);
      }
    }
  }

 private:
  // The status of each literal in the current clause. The fields other than
  // `to_remove` are only used when `to_remove` is true.
  struct Status {
    // The index in the clause of the other literal explaining why this literal
    // can be removed.
    int reason;
    // The number of literals which must be removed after this one.
    int num_children;
    // If false, the proof for removing the literal is "literal => ... =>
    // clause[reason]" (reversed). If true, it is "not(clause[reason]) =>
    // not(literal)" (not reversed). These are not equivalent in practice
    // because the StampingSimplifier's parents_ tree (used to find the
    // intermediate literals in the chain) may contain an implication but not
    // its contrapositive.
    bool negated;
    // Whether the literal can be removed.
    bool to_remove;
  };

  absl::Span<const Literal> clause_;
  bool has_literals_to_remove_;
  // Same size as `clause_`, initialized lazily.
  std::vector<Status> status_;
  // The index of the literals to remove in `clause_`, in reverse removal order.
  std::vector<int> reverse_removal_order_;
};
}  // namespace

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
  LratStampingHelper lrat_helper;

  clause_manager_->DeleteRemovedClauses();
  clause_manager_->DetachAllClauses();
  tmp_proof_.clear();
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
        clause_manager_->LazyDelete(clause,
                                    DeletionSourceForStat::FIXED_AT_TRUE);
        break;
      }
      if (assignment_.LiteralIsFalse(span[i])) continue;
      entries.push_back(
          {i, false, first_stamps_[span[i]], last_stamps_[span[i]]});
      entries.push_back({i, true, first_stamps_[span[i].NegatedIndex()],
                         last_stamps_[span[i].NegatedIndex()]});
    }
    if (clause->IsRemoved()) continue;

    // The sort should be dominant.
    if (!entries.empty()) {
      const double n = static_cast<double>(entries.size());
      dtime_ += 1.5e-8 * n * std::log(n);
      std::sort(entries.begin(), entries.end());
    }

    Entry top_entry;
    top_entry.end = -1;  // Sentinel.
    to_remove.clear();
    if (lrat_proof_handler_ != nullptr) {
      lrat_helper.NewClause(span);
    }
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
              // And the clause is satisfied (so we count it as subsumed).
              if (lrat_proof_handler_ != nullptr) {
                tmp_proof_.clear();
                AppendImplicationChain(lhs, rhs, tmp_proof_);
              }
              if (!clause_manager_->InprocessingFixLiteral(span[top_entry.i],
                                                           tmp_proof_)) {
                return false;
              }
            } else {
              // span[i] => not(span[i]) so span[i] false.
              if (lrat_proof_handler_ != nullptr) {
                tmp_proof_.clear();
                AppendImplicationChain(lhs, rhs, tmp_proof_);
              }
              if (!clause_manager_->InprocessingFixLiteral(
                      span[top_entry.i].Negated(), tmp_proof_)) {
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
            clause_manager_->LazyDelete(
                clause, DeletionSourceForStat::SUBSUMPTION_INPROCESSING);
            break;
          }
        } else {
          CHECK_NE(top_entry.i, e.i);
          if (top_entry.is_negated) {
            // not(a) => not(b), we can remove b.
            to_remove.push_back(e.i);
            if (lrat_proof_handler_ != nullptr) {
              lrat_helper.AddToRemove(e.i, top_entry.i, /*negated=*/true);
            }
          } else {
            // a => b, we can remove a.
            //
            // TODO(user): Note that it is okay to still use top_entry, but we
            // might miss the removal of b if b => c. Also the paper do things
            // differently. Make sure we don't miss any simplification
            // opportunites by not changing top_entry. Same in the other
            // branches.
            to_remove.push_back(top_entry.i);
            if (lrat_proof_handler_ != nullptr) {
              lrat_helper.AddToRemove(top_entry.i, e.i, /*negated=*/false);
            }
          }
        }
      } else {
        top_entry = e;
      }
    }

    if (clause->IsRemoved()) continue;

    // Strengthen the clause.
    if (!to_remove.empty() || entries.size() < span.size()) {
      new_clause.clear();
      gtl::STLSortAndRemoveDuplicates(&to_remove);
      tmp_proof_.clear();
      int to_remove_index = 0;
      for (int i = 0; i < span.size(); ++i) {
        if (to_remove_index < to_remove.size() &&
            i == to_remove[to_remove_index]) {
          ++to_remove_index;
          continue;
        }
        if (assignment_.LiteralIsTrue(span[i])) {
          clause_manager_->LazyDelete(clause,
                                      DeletionSourceForStat::FIXED_AT_TRUE);
          continue;
        }
        if (assignment_.LiteralIsFalse(span[i])) {
          if (lrat_proof_handler_ != nullptr) {
            tmp_proof_.push_back(ClausePtr(span[i].Negated()));
          }
          continue;
        }
        new_clause.push_back(span[i]);
      }
      if (lrat_proof_handler_ != nullptr) {
        lrat_helper.AppendImplicationChains(
            [&](Literal a, Literal b, bool reversed) {
              AppendImplicationChain(a, b, tmp_proof_, reversed);
            });
        tmp_proof_.push_back(ClausePtr(clause));
      }
      num_removed_literals_ += span.size() - new_clause.size();
      if (!clause_manager_->InprocessingRewriteClause(clause, new_clause,
                                                      tmp_proof_)) {
        return false;
      }
    }
  }
  return true;
}

void StampingSimplifier::AppendImplicationChain(Literal a, Literal b,
                                                std::vector<ClausePtr>& chain,
                                                bool reversed) {
  const int initial_size = chain.size();
  Literal l = b;
  while (l != a) {
    chain.push_back(ClausePtr(Literal(parents_[l]).Negated(), Literal(l)));
    l = Literal(parents_[l]);
  }
  if (!reversed) {
    std::reverse(tmp_proof_.begin() + initial_size, tmp_proof_.end());
  }
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
    in_queue_[l] = false;
    queue_.pop_front();

    // Avoid doing too much work here on large problem.
    // Note that we still what to empty the queue.
    if (num_inspected_literals_ <= 1e9) ProcessLiteral(l);
  }

  // Release some memory.
  literal_to_clauses_.clear();

  dtime_ += 1e-8 * num_inspected_literals_;
  time_limit_->AdvanceDeterministicTime(dtime_);
  log_info |= VLOG_IS_ON(2);
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
      literal_to_clauses_[l].push_back(i);
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
    marked_[l] = true;
  }

  // TODO(user): We could also mark a small clause containing
  // current_literal.Negated(), and make sure we only include in
  // clauses_to_process clauses that resolve trivially with that clause.
  std::vector<ClauseIndex> clauses_to_process;
  for (const ClauseIndex i : literal_to_clauses_[current_literal]) {
    if (clauses_[i]->IsRemoved()) continue;

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
    marked_[l] = false;
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
      clause_manager_->LazyDelete(clauses_[i], DeletionSourceForStat::BLOCKED);
    }
  }
}

// Note that this assume that the binary clauses have already been checked.
bool BlockedClauseSimplifier::ClauseIsBlocked(
    Literal current_literal, absl::Span<const Literal> clause) {
  bool is_blocked = true;
  for (const Literal l : clause) marked_[l] = true;

  // TODO(user): For faster reprocessing of the same literal, we should move
  // all clauses that are used in a non-blocked certificate first in the list.
  for (const ClauseIndex i :
       literal_to_clauses_[current_literal.NegatedIndex()]) {
    if (clauses_[i]->IsRemoved()) continue;
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

  for (const Literal l : clause) marked_[l] = false;
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
      literal_to_clauses_[l].push_back(i);
      literal_to_num_clauses_[l]++;
    }
    num_inspected_literals_ += clauses_[i]->size();
  }

  const int saved_trail_index = trail_->Index();
  propagation_index_ = trail_->Index();

  need_to_be_updated_.clear();
  in_need_to_be_updated_.resize(num_variables);
  DCHECK(absl::c_find(in_need_to_be_updated_, true) ==
         in_need_to_be_updated_.end());
  queue_.Reserve(num_variables);
  for (BooleanVariable v(0); v < num_variables; ++v) {
    if (assignment_.VariableIsAssigned(v)) continue;
    UpdatePriorityQueue(v);
  }

  marked_.resize(num_literals);
  DCHECK(
      std::all_of(marked_.begin(), marked_.end(), [](bool b) { return !b; }));

  // TODO(user): adapt the dtime limit depending on how much we want to spend on
  // inprocessing.
  while (!time_limit_->LimitReached() && !queue_.IsEmpty() && dtime_ < 10.0) {
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
    need_to_be_updated_.clear();
  }

  if (!Propagate()) return false;
  implication_graph_->CleanupAllRemovedAndFixedVariables();

  // Remove all redundant clause containing a removed literal. This avoid to
  // re-introduce a removed literal via conflict learning.
  for (SatClause* c : clause_manager_->AllClausesInCreationOrder()) {
    bool remove = false;
    for (const Literal l : c->AsSpan()) {
      if (implication_graph_->IsRemoved(l)) {
        remove = true;
        break;
      }
    }
    if (remove)
      clause_manager_->LazyDelete(c, DeletionSourceForStat::ELIMINATED);
  }

  // Release some memory.
  literal_to_clauses_.clear();
  literal_to_num_clauses_.clear();

  time_limit_->AdvanceDeterministicTime(dtime_);
  log_info |= VLOG_IS_ON(2);
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
      literal_to_num_clauses_[l]--;
      continue;
    }
    if (assignment_.LiteralIsTrue(l)) {
      num_clauses_diff_--;
      clause_manager_->LazyDelete(sat_clause,
                                  DeletionSourceForStat::FIXED_AT_TRUE);
      return true;
    }
    resolvant_.push_back(l);
  }
  if (!clause_manager_->InprocessingRewriteClause(sat_clause, resolvant_)) {
    return false;
  }
  if (sat_clause->IsRemoved()) {
    --num_clauses_diff_;
    for (const Literal l : resolvant_) literal_to_num_clauses_[l]--;
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
    for (const ClauseIndex index : literal_to_clauses_[l]) {
      if (clauses_[index]->IsRemoved()) continue;
      num_clauses_diff_--;
      num_literals_diff_ -= clauses_[index]->size();
      clause_manager_->LazyDelete(clauses_[index],
                                  DeletionSourceForStat::ELIMINATED);
    }
    literal_to_clauses_[l].clear();
    for (const ClauseIndex index : literal_to_clauses_[l.NegatedIndex()]) {
      if (clauses_[index]->IsRemoved()) continue;
      if (!RemoveLiteralFromClause(l.Negated(), clauses_[index])) return false;
    }
    literal_to_clauses_[l.NegatedIndex()].clear();
  }
  return true;
}

// Note that we use the estimated size here to make it fast. It is okay if the
// order of elimination is not perfect... We can improve on this later.
int BoundedVariableElimination::NumClausesContaining(Literal l) {
  return literal_to_num_clauses_[l] +
         implication_graph_->DirectImplicationsEstimatedSize(l.Negated());
}

// TODO(user): Only enqueue variable that can be removed.
void BoundedVariableElimination::UpdatePriorityQueue(BooleanVariable var) {
  if (assignment_.VariableIsAssigned(var)) return;
  if (implication_graph_->IsRemoved(Literal(var, true))) return;
  if (implication_graph_->IsRedundant(Literal(var, true))) return;
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
    literal_to_num_clauses_[l]--;
    if (!in_need_to_be_updated_[l.Variable()]) {
      in_need_to_be_updated_[l.Variable()] = true;
      need_to_be_updated_.push_back(l.Variable());
    }
  }

  // Lazy deletion of the clause.
  clause_manager_->LazyDelete(sat_clause, DeletionSourceForStat::ELIMINATED);
}

void BoundedVariableElimination::DeleteAllClausesContaining(Literal literal) {
  for (const ClauseIndex i : literal_to_clauses_[literal]) {
    const auto clause = clauses_[i]->AsSpan();
    if (clause.empty()) continue;
    postsolve_->AddClauseWithSpecialLiteral(literal, clause);
    DeleteClause(clauses_[i]);
  }
  literal_to_clauses_[literal].clear();
}

void BoundedVariableElimination::AddClause(absl::Span<const Literal> clause) {
  SatClause* pt = clause_manager_->InprocessingAddClause(clause);
  if (pt == nullptr) return;

  num_clauses_diff_++;
  num_literals_diff_ += clause.size();

  const ClauseIndex index(clauses_.size());
  clauses_.push_back(pt);
  for (const Literal l : clause) {
    literal_to_num_clauses_[l]++;
    literal_to_clauses_[l].push_back(index);
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
  auto& clause_containing_lit = literal_to_clauses_[lit];
  for (int i = 0; i < clause_containing_lit.size(); ++i) {
    const ClauseIndex clause_index = clause_containing_lit[i];
    const auto clause = clauses_[clause_index]->AsSpan();
    if (clause.empty()) continue;

    if (!score_only) resolvant_.clear();
    for (const Literal l : clause) {
      if (!score_only && l != lit) resolvant_.push_back(l);
      marked_[l] = true;
    }
    DCHECK(marked_[lit]);
    num_inspected_literals_ += clause.size() + implications.size();

    // If this is true, then "clause" is subsumed by one of its resolvant and we
    // can just remove lit from it. Then it doesn't need to be acounted at all.
    bool clause_can_be_simplified = false;
    const int64_t saved_score = new_score_;

    // Resolution with binary clauses.
    for (const Literal l : implications) {
      CHECK_NE(l, lit);
      if (marked_[l.NegatedIndex()]) continue;  // trivial.
      if (marked_[l]) {
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
          if (!marked_[l]) {
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
          // TODO(user): We should have an exact equality here, except if
          // presolve is off before the clause are added to the sat solver and
          // we have duplicate literals. The code should still work but it
          // wasn't written with that in mind nor tested like this, so we should
          // just enforce the invariant.
          if (false) DCHECK_EQ(clause.size() + extra_size, other.size());
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
    for (const Literal l : clause) marked_[l] = false;

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
  DCHECK(!implication_graph_->IsRedundant(lit));
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
  for (const ClauseIndex i : literal_to_clauses_[lit]) {
    const auto c = clauses_[i]->AsSpan();
    if (!c.empty()) score += clause_weight + c.size();
    dtime_ += 1e-8 * c.size();
  }
  for (const ClauseIndex i : literal_to_clauses_[not_lit]) {
    const auto c = clauses_[i]->AsSpan();
    if (!c.empty()) score += clause_weight + c.size();
    dtime_ += 1.0e-8 * c.size();
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
  //
  // TODO(user): If filter_sat_postsolve_clauses is true, only one of the two
  // sets need to be kept for postsolve.
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

GateCongruenceClosure::~GateCongruenceClosure() {
  if (!VLOG_IS_ON(1)) return;
  shared_stats_->AddStats({
      {"GateCongruenceClosure/dtime(int)", static_cast<int64_t>(total_dtime_)},
      {"GateCongruenceClosure/walltime(int)",
       static_cast<int64_t>(total_wtime_)},
      {"GateCongruenceClosure/gates", total_gates_},
      {"GateCongruenceClosure/units", total_num_units_},
      {"GateCongruenceClosure/equivalences", total_equivalences_},
  });
}

template <int arity>
void GateCongruenceClosure::AddToTruthTable(
    SatClause* clause,
    absl::flat_hash_map<std::array<BooleanVariable, arity>, TruthTableId>&
        ids) {
  CHECK_EQ(clause->size(), arity);
  std::array<BooleanVariable, arity> key;
  SmallBitset bitmask;
  FillKeyAndBitmask(clause->AsSpan(), absl::MakeSpan(key), bitmask);

  TruthTableId next_id(truth_tables_bitset_.size());
  auto [it, inserted] = ids.insert({key, next_id});
  const TruthTableId id = it->second;
  if (inserted) {
    truth_tables_inputs_.Add(key);
    truth_tables_bitset_.push_back(bitmask);
    if (lrat_proof_handler_ != nullptr) {
      tmp_ids_.push_back(id);
      tmp_clauses_.push_back(clause);
    }
  } else {
    const SmallBitset old = truth_tables_bitset_[id];

    // Remove one value.
    truth_tables_bitset_[id] &= bitmask;
    if (lrat_proof_handler_ != nullptr && old != truth_tables_bitset_[id]) {
      tmp_ids_.push_back(id);
      tmp_clauses_.push_back(clause);
    }
  }
}

namespace {

// Given a set of feasible assignment of two variables, recover the
// corresponding binary clauses.
void AppendBinaryClausesFromTruthTable(
    absl::Span<const BooleanVariable> vars, SmallBitset table,
    std::vector<std::pair<Literal, Literal>>* binary_used) {
  DCHECK_EQ(vars.size(), 2);
  for (int b = 0; b < 4; ++b) {
    if (((table >> b) & 1) == 0) {
      binary_used->emplace_back(Literal(vars[0], (b & 1) == 0),
                                Literal(vars[1], (b & 2) == 0));
    }
  }
}

}  // namespace

// Note that this is the "hot" part of the algo, once we have the and gates,
// the congruence closure should be quite fast.
void GateCongruenceClosure::ExtractAndGatesAndFillShortTruthTables(
    PresolveTimer& timer) {
  ids3_.clear();
  ids4_.clear();
  ids5_.clear();
  truth_tables_inputs_.clear();
  truth_tables_bitset_.clear();
  truth_tables_clauses_.clear();
  tmp_ids_.clear();
  tmp_clauses_.clear();

  // We deal with binary clause a bit differently.
  //
  // Tricky: We still include binary clause between fixed literal that haven't
  // been cleaned up yet, as these are needed to really recover all gates.
  //
  // TODO(user): Ideally the detection code should be robust to that.
  // TODO(user): Maybe we should always have an hash-map of binary up to date?
  int num_fn1 = 0;
  std::vector<std::pair<Literal, Literal>> binary_used;
  for (LiteralIndex a(0); a < implication_graph_->literal_size(); ++a) {
    // TODO(user): If we know we have too many implications for the time limit
    // We should just be better of not doing that loop at all.
    if (timer.WorkLimitIsReached()) break;
    if (implication_graph_->IsRedundant(Literal(a))) continue;
    const absl::Span<const Literal> implied =
        implication_graph_->Implications(Literal(a));
    timer.TrackHashLookups(implied.size());
    for (const Literal b : implied) {
      if (implication_graph_->IsRedundant(b)) continue;

      std::array<BooleanVariable, 2> key2;
      SmallBitset bitmask;
      FillKeyAndBitmask({Literal(a).Negated(), b}, absl::MakeSpan(key2),
                        bitmask);
      auto [it, inserted] = ids2_.insert({key2, bitmask});
      if (!inserted) {
        const SmallBitset old = it->second;
        it->second &= bitmask;
        if (it->second != old) {
          // This is either fixing or equivalence!
          //
          // Doing a run of DetectEquivalences() should fix that but then
          // new clause of size 3 might become binary, and the fix point might
          // require a lot of step. So it is important to do it here.
          const SmallBitset bitset2 = it->second;
          if (lrat_proof_handler_ != nullptr) {
            binary_used.clear();
            AppendBinaryClausesFromTruthTable(key2, bitset2, &binary_used);
          }
          // If we are equivalent, we always have 2 functions.
          // But if we fix a variable (like bitset2 = 0011) we just have one.
          const int num_added =
              ProcessTruthTable(key2, bitset2, {}, binary_used);
          CHECK_GE(num_added, 1) << std::bitset<4>(bitset2);
          num_fn1 += num_added;
        }
      }
    }
  }
  timer.AddCounter("t2", ids2_.size());
  timer.AddCounter("fn1", num_fn1);

  std::vector<Literal> candidates;
  for (SatClause* clause : clause_manager_->AllClausesInCreationOrder()) {
    if (timer.WorkLimitIsReached()) break;
    if (clause->size() == 0) continue;

    if (clause->size() == 3) {
      AddToTruthTable<3>(clause, ids3_);

      // The AND gate of size 3 should be detected by the short table code, no
      // need to do the algo here which should be slower.
      continue;
    } else if (clause->size() == 4) {
      AddToTruthTable<4>(clause, ids4_);
    } else if (clause->size() == 5) {
      AddToTruthTable<5>(clause, ids5_);
    }

    // Used for an optimization below.
    int min_num_implications = std::numeric_limits<int>::max();
    Literal lit_with_less_implications;

    const int clause_size = clause->size();
    timer.TrackSimpleLoop(clause_size);
    candidates.clear();
    for (const Literal l : clause->AsSpan()) {
      // TODO(user): using Implications() only considers pure binary
      // clauses and not at_most_one. Also, if we do transitive reduction, we
      // might skip important literals here. Maybe a better alternative is
      // to detect clauses that "propagate" l back when we probe l...
      const int num_implications = implication_graph_->Implications(l).size();
      if (num_implications < min_num_implications) {
        min_num_implications = num_implications;
        lit_with_less_implications = l;
      }

      if (num_implications >= clause_size - 1) {
        candidates.push_back(l);
      }
    }
    if (candidates.empty()) continue;
    if (min_num_implications == 0) continue;

    marked_.ResetAllToFalse();
    for (const Literal l : clause->AsSpan()) marked_.Set(l);
    const auto is_clause_literal = marked_.const_view();

    // There should be no duplicate in a clause.
    // And also not lit and lit.Negated(), but we don't check that here.
    CHECK_EQ(marked_.PositionsSetAtLeastOnce().size(), clause_size);

    // These bitsets will contain the intersection of all the negated literals
    // implied by one of the clause literal. It is used for an "optimization" as
    // a literal can only be a "target" of a bool_and if it implies all other
    // literals of the clause to false. So by contraposition, any literal should
    // directly imply such a target to false.
    //
    // This relies on the fact that for any a => b directly stored in
    // BinaryImplicationGraph, we should always have not(b) => not(a). This
    // only applies to the result of Implications() though, not the one we
    // can infer by transitivity.
    //
    // We start with the variables implied by "lit_with_less_implications" and
    // at each iteration, we take the intersection with the variables implied by
    // our current "target".
    //
    // TODO(user): SparseBitset<> does not support swap.
    auto* is_potential_target = &seen_;
    auto* next_is_potential_target = &next_seen_;

    // If we don't have lit_with_less_implications => not(target) then we
    // shouldn't have target => not(lit_with_less_implications).
    {
      is_potential_target->ResetAllToFalse();
      is_potential_target->Set(lit_with_less_implications);
      const absl::Span<const Literal> implications =
          implication_graph_->Implications(lit_with_less_implications);
      timer.TrackFastLoop(implications.size());
      for (const Literal implied : implications) {
        is_potential_target->Set(implied.Negated());
      }
    }

    for (const Literal target : candidates) {
      if (!(*is_potential_target)[target]) continue;

      int count = 0;
      next_is_potential_target->ResetAllToFalse();
      const absl::Span<const Literal> implications =
          implication_graph_->Implications(target);
      timer.TrackFastLoop(implications.size());
      for (const Literal implied : implications) {
        CHECK_NE(implied.Variable(), target.Variable());

        if (is_clause_literal[implied.Negated()]) {
          // Set next_is_potential_target to the intersection of
          // is_potential_target and the one we see here.
          if ((*is_potential_target)[implied.Negated()]) {
            next_is_potential_target->Set(implied.Negated());
          }
          ++count;
        }
      }
      std::swap(is_potential_target, next_is_potential_target);

      // Target should imply all other literal in the base clause to false.
      if (count < clause_size - 1) continue;

      // Using only the "count" require that there is no duplicates. But
      // depending when this is run in the inprocessing loop, we might have
      // some. Redo a pass to double check.
      int second_count = 0;
      for (const Literal implied : implications) {
        if (implied.Variable() == target.Variable()) continue;
        if (is_clause_literal[implied.Negated()]) {
          ++second_count;
          marked_.Clear(implied.Negated());
        }
      }

      // Restore is_clause_literal.
      for (const Literal l : clause->AsSpan()) {
        marked_.Set(l);
      }
      if (second_count != clause_size - 1) continue;

      // We have an and_gate !
      // Add the detected gate (its inputs are the negation of each clause
      // literal other than the target).
      gates_target_.push_back(target.Index());
      gates_type_.push_back(kAndGateType);

      const GateId gate_id = GateId(gates_inputs_.Add({}));
      for (const Literal l : clause->AsSpan()) {
        if (l == target) continue;
        gates_inputs_.AppendToLastVector(l.NegatedIndex());
      }
      if (lrat_proof_handler_ != nullptr) {
        gates_clauses_.Add({clause});

        // Create temporary size 2 clauses for the needed binary.
        for (const Literal l : clause->AsSpan()) {
          if (l == target) continue;
          tmp_binary_clauses_.emplace_back(
              SatClause::Create({target.Negated(), l.Negated()}));
          gates_clauses_.AppendToLastVector(tmp_binary_clauses_.back().get());
        }
      }

      // Canonicalize.
      absl::Span<LiteralIndex> gate = gates_inputs_[gate_id];
      std::sort(gate.begin(), gate.end());

      // Even if we detected an and_gate from a base clause, we keep going
      // as their could me more than one. In the extreme of an "exactly_one",
      // a single base clause of size n will correspond to n and_gates !
    }
  }

  timer.AddCounter("and_gates", gates_inputs_.size());
}

int GateCongruenceClosure::CanonicalizeShortGate(GateId id) {
  // Deals with fixed input variable.
  absl::Span<LiteralIndex> inputs = gates_inputs_[id];
  int new_size = inputs.size();

  for (int i = 0; i < new_size;) {
    if (assignment_.LiteralIsAssigned(Literal(inputs[i]))) {
      new_size =
          RemoveFixedInput(i, assignment_.LiteralIsTrue(Literal(inputs[i])),
                           inputs.subspan(0, new_size), gates_type_[id]);
    } else {
      ++i;
    }
  }

  // Now canonicalize.
  new_size = CanonicalizeFunctionTruthTable(
      gates_target_[id], inputs.subspan(0, new_size), gates_type_[id]);

  // Resize and return.
  if (new_size < gates_inputs_[id].size()) {
    gates_inputs_.Shrink(id, new_size);
  }
  DCHECK_EQ(gates_type_[id] >> (1 << (gates_inputs_[id].size())), 0);
  return new_size;
}

int GateCongruenceClosure::ProcessTruthTable(
    absl::Span<const BooleanVariable> inputs, SmallBitset truth_table,
    absl::Span<const TruthTableId> ids_for_proof,
    absl::Span<const std::pair<Literal, Literal>> binary_used) {
  int num_detected = 0;
  for (int i = 0; i < inputs.size(); ++i) {
    if (!IsFunction(i, inputs.size(), truth_table)) continue;
    const int num_bits = inputs.size() - 1;
    ++num_detected;

    const GateId new_id(gates_target_.size());
    gates_target_.push_back(Literal(inputs[i], true));
    gates_inputs_.Add({});
    for (int j = 0; j < inputs.size(); ++j) {
      if (i != j) {
        gates_inputs_.AppendToLastVector(Literal(inputs[j], true));
      }
    }

    // Generate the function truth table as a type.
    // We will canonicalize it further in the main loop.
    unsigned int type = 0;
    for (int p = 0; p < (1 << num_bits); ++p) {
      // Expand from (num_bits == inputs.size() - 1) bits to (inputs.size())
      // bits.
      const int bigger_p = AddHoleAtPosition(i, p);

      if ((truth_table >> (bigger_p + (1 << i))) & 1) {
        // target is 1 at this position.
        type |= 1 << p;
        DCHECK_NE((truth_table >> bigger_p) & 1, 1);  // Proper function.
      } else {
        // Note that if there is no feasible assignment for a given p if
        // [(truth_table >> bigger_p) & 1] is not 1.
        //
        // Here we could learn smaller clause, but also we don't really care
        // what is the value of the target at that point p, so we use zero.
        //
        // TODO(user): This is not ideal if another function has a value of 1 at
        // point p, we could still merge it with this one. Shall we create two
        // gate type? or change the algo?
      }
    }

    gates_type_.push_back(type);
    if (lrat_proof_handler_ != nullptr) {
      gates_clauses_.Add({});
      for (const TruthTableId id : ids_for_proof) {
        gates_clauses_.AppendToLastVector(truth_tables_clauses_[id]);
      }
      for (const auto [a, b] : binary_used) {
        tmp_binary_clauses_.emplace_back(SatClause::Create({a, b}));
        gates_clauses_.AppendToLastVector(tmp_binary_clauses_.back().get());
      }
    }

    // Canonicalize right away to deal with corner case.
    CanonicalizeShortGate(new_id);
  }
  return num_detected;
}

namespace {

// Return a BooleanVariable from b that is not in a or kNoBooleanVariable;
BooleanVariable FindMissing(absl::Span<const BooleanVariable> vars_a,
                            absl::Span<const BooleanVariable> vars_b) {
  for (const BooleanVariable b : vars_b) {
    bool found = false;
    for (const BooleanVariable a : vars_a) {
      if (a == b) {
        found = true;
        break;
      }
    }
    if (!found) return b;
  }
  return kNoBooleanVariable;
}

}  // namespace

// TODO(user): It should be possible to extract ALL possible short gate, but
// we are not there yet.
void GateCongruenceClosure::ExtractShortGates(PresolveTimer& timer) {
  if (lrat_proof_handler_ != nullptr) {
    truth_tables_clauses_.ResetFromFlatMapping(
        tmp_ids_, tmp_clauses_,
        /*minimum_num_nodes=*/truth_tables_bitset_.size());
    CHECK_EQ(truth_tables_bitset_.size(), truth_tables_clauses_.size());
  }

  // This is used to combine two 3 arity table into one 4 arity one if
  // they share two variables.
  absl::flat_hash_map<std::array<BooleanVariable, 2>, int> binary_index_map;
  std::vector<int> flat_binary_index;
  std::vector<TruthTableId> flat_table_id;

  // Counters.
  // We only fill a subset of the entries, but that make the code shorter.
  std::vector<int> num_tables(6);
  std::vector<int> num_functions(6);

  // Note that using the indirection via TruthTableId allow this code to
  // be deterministic.
  CHECK_EQ(truth_tables_bitset_.size(), truth_tables_inputs_.size());

  // Given a table of arity 4, this merges all the information from the tables
  // of arity 3 included in it.
  int num_merges = 0;
  const auto merge3_into_4 = [this, &num_merges](
                                 absl::Span<const BooleanVariable> inputs,
                                 SmallBitset& truth_table,
                                 std::vector<TruthTableId>& ids_for_proof) {
    DCHECK_EQ(inputs.size(), 4);
    for (int i_to_remove = 0; i_to_remove < inputs.size(); ++i_to_remove) {
      int pos = 0;
      std::array<BooleanVariable, 3> key3;
      for (int i = 0; i < inputs.size(); ++i) {
        if (i == i_to_remove) continue;
        key3[pos++] = inputs[i];
      }
      const auto it = ids3_.find(key3);
      if (it == ids3_.end()) continue;
      ++num_merges;

      // Extend the bitset from the table3 so that it is expressed correctly
      // for the given inputs.
      const TruthTableId id3 = it->second;
      std::array<BooleanVariable, 4> key4;
      for (int i = 0; i < 3; ++i) key4[i] = key3[i];
      key4[3] = FindMissing(key3, inputs);
      SmallBitset bitset = truth_tables_bitset_[id3];
      bitset |= bitset << (1 << 3);  // Extend for a new variable.
      CanonicalizeTruthTable<BooleanVariable>(absl::MakeSpan(key4), bitset);
      CHECK_EQ(inputs, absl::MakeSpan(key4));
      truth_table &= bitset;

      // We need to add the corresponding clause!
      if (lrat_proof_handler_ != nullptr) {
        ids_for_proof.push_back(id3);
      }
    }
  };

  int num_merges2 = 0;
  const auto merge2_into_n =
      [this, &num_merges2](
          absl::Span<const BooleanVariable> inputs, SmallBitset& truth_table,
          std::vector<std::pair<Literal, Literal>>& binary_used) {
        for (int i = 0; i < inputs.size(); ++i) {
          for (int j = i + 1; j < inputs.size(); ++j) {
            std::array<BooleanVariable, 2> key2 = {inputs[i], inputs[j]};
            const auto it = ids2_.find(key2);
            if (it == ids2_.end()) continue;

            const SmallBitset bitset2 = it->second;
            SmallBitset bitset = bitset2;
            int new_size = 0;
            std::vector<BooleanVariable> key(inputs.size());
            key[new_size++] = inputs[i];
            key[new_size++] = inputs[j];
            for (int t = 0; t < inputs.size(); ++t) {
              if (t != i && t != j) {
                key[new_size] = inputs[t];
                bitset |= bitset << (1 << new_size);  // EXTEND
                ++new_size;
              }
            }
            CanonicalizeTruthTable<BooleanVariable>(absl::MakeSpan(key),
                                                    bitset);
            CHECK_EQ(inputs, absl::MakeSpan(key));

            const SmallBitset old = truth_table;
            truth_table &= bitset;
            if (old != truth_table) {
              AppendBinaryClausesFromTruthTable(key2, bitset2, &binary_used);
              ++num_merges2;
            }
          }
        }
      };

  // Starts by processing all existing tables.
  //
  // TODO(user): Since we deal with and_gates differently, do we need to look at
  // binary clauses here ? or that is not needed ? I think there is only two
  // kind of Boolean function on two inputs (and_gates, with any possible
  // negation) and xor_gate.
  std::vector<TruthTableId> ids_for_proof;
  std::vector<std::pair<Literal, Literal>> binary_used;
  for (TruthTableId t_id(0); t_id < truth_tables_inputs_.size(); ++t_id) {
    ids_for_proof.clear();
    ids_for_proof.push_back(t_id);
    const absl::Span<const BooleanVariable> inputs = truth_tables_inputs_[t_id];
    SmallBitset truth_table = truth_tables_bitset_[t_id];

    // TODO(user): it is unclear why this is useful. Understand a bit more the
    // set of possible Boolean functions of 2 and 3 variables and their clause
    // encoding.
    binary_used.clear();
    merge2_into_n(inputs, truth_table, binary_used);

    // Merge any size-3 table included inside a size-4 gate.
    // TODO(user): do it for larger gate too ?
    if (inputs.size() == 4) {
      merge3_into_4(inputs, truth_table, ids_for_proof);
    }

    ++num_tables[inputs.size()];
    const int num_detected =
        ProcessTruthTable(inputs, truth_table, ids_for_proof, binary_used);
    num_functions[inputs.size() - 1] += num_detected;

    // If this is not a function and of size 3, lets try to combine it with
    // another truth table of size 3 to get a table of size 4.
    if (inputs.size() == 3 && num_detected == 0) {
      for (int i = 0; i < 3; ++i) {
        std::array<BooleanVariable, 2> key{inputs[i != 0 ? 0 : 1],
                                           inputs[i != 2 ? 2 : 1]};
        DCHECK(std::is_sorted(key.begin(), key.end()));
        const auto [it, inserted] =
            binary_index_map.insert({key, binary_index_map.size()});
        flat_binary_index.push_back(it->second);
        flat_table_id.push_back(t_id);
      }
    }
  }
  gtl::STLClearObject(&binary_index_map);

  // Detects ITE gates and potentially other 3-gates from a truth table of
  // 4-entries formed by two 3-entries table. This just create a 4-entries
  // table that will be processed below.
  CompactVectorVector<int, TruthTableId> candidates;
  candidates.ResetFromFlatMapping(flat_binary_index, flat_table_id);
  gtl::STLClearObject(&flat_binary_index);
  gtl::STLClearObject(&flat_table_id);
  int num_combinations = 0;
  for (int c = 0; c < candidates.size(); ++c) {
    if (candidates[c].size() < 2) continue;
    if (candidates[c].size() > 10) continue;  // Too many? use heuristic.

    for (int a = 0; a < candidates[c].size(); ++a) {
      for (int b = a + 1; b < candidates[c].size(); ++b) {
        const absl::Span<const BooleanVariable> inputs_a =
            truth_tables_inputs_[candidates[c][a]];
        const absl::Span<const BooleanVariable> inputs_b =
            truth_tables_inputs_[candidates[c][b]];

        std::array<BooleanVariable, 4> key;
        for (int i = 0; i < 3; ++i) key[i] = inputs_a[i];
        key[3] = FindMissing(inputs_a, inputs_b);
        CHECK_NE(key[3], kNoBooleanVariable);

        // Add an all allowed entry.
        SmallBitset bitmask = GetNumBitsAtOne(4);
        CanonicalizeTruthTable<BooleanVariable>(absl::MakeSpan(key), bitmask);

        // If the key was not processed before, process it now.
        // Note that an old version created a TruthTableId for it, but that
        // waste a lot of space.
        //
        // On another hand, it is possible we process the same key up to
        // 4_choose_2 times, but this is rare...
        if (!ids4_.contains(key)) {
          ++num_combinations;
          ++num_tables[4];
          ids_for_proof.clear();
          binary_used.clear();
          merge2_into_n(key, bitmask, binary_used);
          merge3_into_4(key, bitmask, ids_for_proof);
          num_functions[3] +=
              ProcessTruthTable(key, bitmask, ids_for_proof, binary_used);
        }
      }
    }
  }

  timer.AddCounter("combine3", num_combinations);
  timer.AddCounter("merges", num_merges);
  timer.AddCounter("merges2", num_merges2);

  // Note that we only display non-zero counters.
  for (int i = 0; i < num_tables.size(); ++i) {
    timer.AddCounter(absl::StrCat("t", i), num_tables[i]);
  }
  for (int i = 0; i < num_functions.size(); ++i) {
    timer.AddCounter(absl::StrCat("fn", i), num_functions[i]);
  }
}

namespace {
// Helper class to add LRAT proofs for equivalent gate target literals.
class LratGateCongruenceHelper {
 public:
  LratGateCongruenceHelper(
      const Trail* trail, ClauseManager* clause_manager,
      LratProofHandler* lrat_proof_handler,
      const util_intops::StrongVector<GateId, LiteralIndex>& gates_target,
      const CompactVectorVector<GateId, const SatClause*>& gates_clauses,
      DenseConnectedComponentsFinder& union_find)
      : trail_(trail),
        clause_manager_(clause_manager),
        lrat_proof_handler_(lrat_proof_handler),
        gates_target_(gates_target),
        gates_clauses_(gates_clauses),
        union_find_(union_find) {}

  ~LratGateCongruenceHelper() {
    if (lrat_proof_handler_ != nullptr) {
      for (const ClausePtr clause : to_delete_) {
        lrat_proof_handler_->DeleteClause(clause);
      }
    }
  }

  // Adds direct LRAT equivalence clauses between l and its representative r, as
  // well as between each of its ancestor and r. Does nothing if r is equal to l
  // or its parent. This must be called before union_find_.FindRoot(l).
  void ShortenEquivalencesWithRepresentative(Literal l) {
    std::vector<Literal> literals;
    Literal representative;
    // Append l and its ancestors, excluding the representative, to `literals`.
    while (true) {
      if (IsRepresentative(l)) {
        representative = l;
        break;
      }
      literals.push_back(l);
      l = GetParent(l);
    }

    // Add a direct equivalence between each literal in `literals` and
    // `representative` (except the last one, which already has a direct
    // equivalence). This is done in reverse order so that the proof for each
    // equivalence can use the one for the parent.
    for (int i = literals.size() - 2; i >= 0; --i) {
      const Literal parent = literals[i + 1];
      const Literal child = literals[i];
      DCHECK(parent_equivalence_.contains(parent));
      DCHECK(parent_equivalence_.contains(child));
      GateEquivalenceClauses& parent_clauses = parent_equivalence_[parent];
      GateEquivalenceClauses& child_clauses = parent_equivalence_[child];

      const ClausePtr rep_implies_child =
          ClausePtr(SatClause::Create({representative.Negated(), child}));
      lrat_proof_handler_->AddInferredClause(
          rep_implies_child, {parent_clauses.parent_implies_child,
                              child_clauses.parent_implies_child});
      const ClausePtr child_implies_rep =
          ClausePtr(SatClause::Create({child.Negated(), representative}));
      lrat_proof_handler_->AddInferredClause(
          child_implies_rep, {child_clauses.child_implies_parent,
                              parent_clauses.child_implies_parent});

      child_clauses.parent_implies_child = rep_implies_child;
      child_clauses.child_implies_parent = child_implies_rep;
      to_delete_.push_back(rep_implies_child);
      to_delete_.push_back(child_implies_rep);
    }
    if (!literals.empty()) {
      // Make sure the parent links in union_find_ are shorten too, to keep the
      // consistency between the two data structures.
      union_find_.FindRoot(literals[0].Index().value());
    }
  }

  // Returns an LRAT clause rep(gates_target[gate_a_id]) =>
  // rep(gates_target[gate_b_id]).
  ClausePtr AddAndGateTargetImplication(GateId gate_a_id, GateId gate_b_id) {
    const Literal a = Literal(gates_target_[gate_a_id]);
    const Literal b = Literal(gates_target_[gate_b_id]);
    const Literal rep_a = GetRepresentativeWithProofSupport(a);
    const Literal rep_b = GetRepresentativeWithProofSupport(b);
    DCHECK(IsRepresentative(rep_a));
    DCHECK(IsRepresentative(rep_b));
    // Compute a sequence of clauses proving that rep(a) => rep(b).
    // The following only works for and gates.
    std::vector<ClausePtr> proof;
    // rep(a) => a:
    Append(proof, GetRepresentativeImpliesLiteralClause(a));
    // For each original input l of gate_a_id, a => l => rep(l). The original
    // inputs are the negation of each clause literal other than the target.
    // TODO(user): this can add redundant clauses if two original inputs
    // have the same representative.
    for (const Literal lit : gates_clauses_[gate_a_id][0]->AsSpan()) {
      if (lit == a) continue;
      const Literal l = lit.Negated();
      proof.push_back(ClausePtr(a.Negated(), l));
      Append(proof, GetLiteralImpliesRepresentativeClause(l));
    }
    // For each original input l of b, rep(l) => l. The original inputs are
    // the negation of each gate clause literal other than its target b.
    for (const Literal lit : gates_clauses_[gate_b_id][0]->AsSpan()) {
      if (lit == b) continue;
      const Literal l = lit.Negated();
      Append(proof, GetRepresentativeImpliesLiteralClause(l));
    }
    // The original inputs of gate_b_id imply its target b:
    proof.push_back(ClausePtr(gates_clauses_[gate_b_id][0]));
    // b => rep(b):
    Append(proof, GetLiteralImpliesRepresentativeClause(b));

    if (rep_a.Negated() == rep_b) {
      lrat_proof_handler_->AddInferredClause(ClausePtr(rep_a.Negated()), proof);
      return ClausePtr(rep_a.Negated());
    }
    const ClausePtr clause = ClausePtr(rep_a.Negated(), rep_b);
    lrat_proof_handler_->AddInferredClause(clause, proof);
    return clause;
  }

  void ClearTemporaryProof() {
    CHECK(lrat_proof_handler_ != nullptr);
    tmp_index_to_delete_.clear();
    tmp_proof_clauses_.clear();
    marked_.ClearAndResize(LiteralIndex(clause_manager_->literal_size()));
  }

  Literal GetRepresentativeWithProofSupport(Literal lit) {
    const int lit_index_as_int = lit.Index().value();
    if (union_find_.GetParent(lit_index_as_int) == lit_index_as_int) {
      return lit;
    }
    if (lrat_proof_handler_ != nullptr) {
      ShortenEquivalencesWithRepresentative(lit);
    }
    return Literal(LiteralIndex(union_find_.FindRoot(lit_index_as_int)));
  }

  // TODO(user): There is probably no need to add clauses that involve variables
  // that are no longer inputs of that gate. If for instance we showed that the
  // gate output is independent of one of the variable, it could still appear in
  // the clauses.
  void AddGateClausesToTemporaryProof(GateId id) {
    CHECK(lrat_proof_handler_ != nullptr);
    const auto& assignment = trail_->Assignment();
    for (const SatClause* clause : gates_clauses_[id]) {
      // We rewrite each clause using new equivalences or fixed literals found.
      marked_.ResetAllToFalse();
      tmp_literals_.clear();
      tmp_proof_.clear();
      bool clause_is_trivial = false;
      bool some_change = false;
      for (const Literal lit : clause->AsSpan()) {
        const Literal rep = GetRepresentativeWithProofSupport(lit);

        if (assignment.LiteralIsAssigned(rep)) {
          if (assignment.LiteralIsTrue(rep)) {
            clause_is_trivial = true;
            break;  // Not needed.
          } else {
            some_change = true;
            tmp_proof_.push_back(ClausePtr(rep.Negated()));
            if (rep != lit) {
              tmp_proof_.push_back(GetLiteralImpliesRepresentativeClause(lit));
            }
            continue;
          }
        }

        if (rep != lit) {
          some_change = true;
          // We need not(rep) => not(lit). This should be equivalent to
          // getting lit => rep. Both should work.
          tmp_proof_.push_back(GetLiteralImpliesRepresentativeClause(lit));
        }

        if (marked_[rep]) continue;
        if (marked_[rep.Negated()]) {
          clause_is_trivial = true;
          break;
        }
        marked_.Set(rep);
        tmp_literals_.push_back(rep);
      }

      // If this is the case, we shouldn't need it for the proof.
      if (clause_is_trivial) continue;

      ClausePtr new_clause =
          clause->size() == 2
              ? ClausePtr(clause->FirstLiteral(), clause->SecondLiteral())
              : ClausePtr(clause);
      if (some_change) {
        // If there is some change, we add a temporary clause id with the
        // proof to go from the original clause to this one.
        tmp_index_to_delete_.push_back(tmp_proof_clauses_.size());
        tmp_proof_.push_back(new_clause);
        new_clause = ClausePtr(tmp_literals_);
        lrat_proof_handler_->AddInferredClause(new_clause, tmp_proof_);
      }

      // Add that clause and its id to the set of clauses needed for the proof.
      tmp_proof_clauses_.push_back(new_clause);
    }
  }

  // Same as AddAndGateTargetImplication() but with truth table based gates.
  std::pair<ClausePtr, ClausePtr> AddShortGateEquivalence(
      Literal rep_a, Literal rep_b, absl::Span<const GateId> gate_ids) {
    if (lrat_proof_handler_ == nullptr) return {kNullClausePtr, kNullClausePtr};

    // Just add all clauses from both gates.
    // But note that we need to remap them.
    ClearTemporaryProof();
    for (const GateId id : gate_ids) {
      AddGateClausesToTemporaryProof(id);
    }

    // Corner case: If rep_a or rep_b are fixed, add proof for that.
    for (const Literal lit : {rep_a, rep_b}) {
      const auto& assignment = trail_->Assignment();
      if (assignment.LiteralIsAssigned(lit)) {
        const Literal true_lit =
            assignment.LiteralIsTrue(lit) ? lit : lit.Negated();
        tmp_proof_clauses_.push_back(ClausePtr(true_lit));
      }
    }

    // All clauses are now in tmp_proof_clauses_.
    // We can add both implications with proof.
    DCHECK(IsRepresentative(rep_a));
    DCHECK(IsRepresentative(rep_b));
    CHECK_NE(rep_a, rep_b);

    ClausePtr rep_a_implies_rep_b;
    ClausePtr rep_b_implies_rep_a;
    if (rep_a.Negated() == rep_b) {
      // The model is UNSAT.
      //
      // TODO(user): AddAndProveInferredClauseByEnumeration() do not like
      // duplicates, but maybe we should just handle the case to have
      // less code here?
      rep_a_implies_rep_b = ClausePtr(rep_a.Negated());
      rep_b_implies_rep_a = ClausePtr(rep_a);
      lrat_proof_handler_->AddAndProveInferredClauseByEnumeration(
          rep_a_implies_rep_b, tmp_proof_clauses_);
      lrat_proof_handler_->AddAndProveInferredClauseByEnumeration(
          rep_b_implies_rep_a, tmp_proof_clauses_);
    } else {
      rep_a_implies_rep_b = ClausePtr(rep_a.Negated(), rep_b);
      rep_b_implies_rep_a = ClausePtr(rep_b.Negated(), rep_a);
      lrat_proof_handler_->AddAndProveInferredClauseByEnumeration(
          rep_a_implies_rep_b, tmp_proof_clauses_);
      lrat_proof_handler_->AddAndProveInferredClauseByEnumeration(
          rep_b_implies_rep_a, tmp_proof_clauses_);
    }

    for (const int i : tmp_index_to_delete_) {
      // Corner case if the proof used a temporary id.
      if (tmp_proof_clauses_[i] == rep_a_implies_rep_b) continue;
      if (tmp_proof_clauses_[i] == rep_b_implies_rep_a) continue;
      lrat_proof_handler_->DeleteClause(tmp_proof_clauses_[i]);
    }
    return {rep_a_implies_rep_b, rep_b_implies_rep_a};
  }

  ClausePtr ProofForFixingLiteral(Literal to_fix, GateId id) {
    if (lrat_proof_handler_ == nullptr) return kNullClausePtr;
    CHECK(IsRepresentative(to_fix));
    ClearTemporaryProof();
    AddGateClausesToTemporaryProof(id);
    const ClausePtr to_fix_clause = ClausePtr(to_fix);
    lrat_proof_handler_->AddAndProveInferredClauseByEnumeration(
        to_fix_clause, tmp_proof_clauses_);

    for (const int i : tmp_index_to_delete_) {
      // Corner case if the proof used a temporary clause.
      if (tmp_proof_clauses_[i] == to_fix_clause) continue;
      lrat_proof_handler_->DeleteClause(tmp_proof_clauses_[i]);
    }
    return to_fix_clause;
  }

  void AddGateEquivalenceClauses(Literal child, ClausePtr child_implies_parent,
                                 ClausePtr parent_implies_child) {
    DCHECK(!parent_equivalence_.contains(child));
    parent_equivalence_[child] = {
        .parent_implies_child = parent_implies_child,
        .child_implies_parent = child_implies_parent,
    };
  }

  // Appends to `proof` the clauses "gates_target[gate_id] => l => rep" and
  // "gates_target[gate_id] => m => not(rep)", proving that the gate target
  // literal can be fixed to false (assuming this is an and gate). A
  // precondition is that two original inputs l and m with rep(l) = rep and
  // rep(m) = not(rep) must exist.
  void AppendFixAndGateTargetClauses(GateId gate_id, Literal rep,
                                     absl::InlinedVector<ClausePtr, 5>& proof) {
    const Literal target = Literal(gates_target_[gate_id]);
    LiteralIndex l_index = kNoLiteralIndex;
    LiteralIndex m_index = kNoLiteralIndex;
    // Find l and m in the original inputs (the negation of each gate clause
    // literal other than its target).
    for (const Literal lit : gates_clauses_[gate_id][0]->AsSpan()) {
      if (l_index != kNoLiteralIndex && m_index != kNoLiteralIndex) break;
      const Literal l = lit.Negated();
      const Literal rep_l = GetRepresentativeWithProofSupport(l);
      if (rep_l == rep) l_index = l.Index();
      if (rep_l == rep.Negated()) m_index = l.Index();
    }
    CHECK(l_index != kNoLiteralIndex && m_index != kNoLiteralIndex);

    // We want to prove target_rep.Negated(), we start by showing that
    // target_rep implies target.
    Append(proof, GetRepresentativeImpliesLiteralClause(target));
    proof.push_back(ClausePtr(target.Negated(), Literal(l_index)));
    Append(proof, GetLiteralImpliesRepresentativeClause(Literal(l_index)));
    proof.push_back(ClausePtr(target.Negated(), Literal(m_index)));
    Append(proof, GetLiteralImpliesRepresentativeClause(Literal(m_index)));
  }

 private:
  // The IDs of the two implications of an equivalence between two gate targets.
  struct GateEquivalenceClauses {
    ClausePtr parent_implies_child;
    ClausePtr child_implies_parent;
  };

  bool IsRepresentative(Literal l) const { return GetParent(l) == l; }

  Literal GetParent(Literal l) const {
    return Literal(LiteralIndex(union_find_.GetParent(l.Index().value())));
  }

  ClausePtr GetLiteralImpliesRepresentativeClause(Literal l) {
    ShortenEquivalencesWithRepresentative(l);
    const auto it = parent_equivalence_.find(l);
    if (it == parent_equivalence_.end()) return kNullClausePtr;
    return it->second.child_implies_parent;
  }

  ClausePtr GetRepresentativeImpliesLiteralClause(Literal l) {
    ShortenEquivalencesWithRepresentative(l);
    const auto it = parent_equivalence_.find(l);
    if (it == parent_equivalence_.end()) return kNullClausePtr;
    return it->second.parent_implies_child;
  }

  template <typename Vector>
  static void Append(Vector& clauses, ClausePtr new_clause) {
    if (new_clause != kNullClausePtr) {
      clauses.push_back(new_clause);
    }
  }

  const Trail* trail_;
  ClauseManager* clause_manager_;
  LratProofHandler* lrat_proof_handler_;
  const util_intops::StrongVector<GateId, LiteralIndex>& gates_target_;
  const CompactVectorVector<GateId, const SatClause*>& gates_clauses_;
  DenseConnectedComponentsFinder& union_find_;

  // For each gate target with a parent in `union_find_` different from itself,
  // the equivalence clauses with this parent literal.
  absl::flat_hash_map<Literal, GateEquivalenceClauses> parent_equivalence_;
  // Equivalence clauses which are not needed after the current round.
  std::vector<ClausePtr> to_delete_;

  // For AddShortGateTargetImplication().
  std::vector<int> tmp_index_to_delete_;
  std::vector<ClausePtr> tmp_proof_clauses_;

  // For the simplification of clauses using equivalences in
  // AddGateClausesToTemporaryProof().
  SparseBitset<LiteralIndex> marked_;
  std::vector<ClausePtr> tmp_proof_;
  std::vector<Literal> tmp_literals_;
};

}  // namespace

bool GateCongruenceClosure::DoOneRound(bool log_info) {
  // TODO(user): Remove this condition, it is possible there are no binary
  // and still gates!
  if (implication_graph_->IsEmpty()) return true;

  PresolveTimer timer("GateCongruenceClosure", logger_, time_limit_);
  timer.OverrideLogging(log_info);

  const int num_variables(sat_solver_->NumVariables());
  const int num_literals(num_variables * 2);
  marked_.ClearAndResize(Literal(num_literals));
  seen_.ClearAndResize(Literal(num_literals));
  next_seen_.ClearAndResize(Literal(num_literals));

  gates_target_.clear();
  gates_inputs_.clear();
  gates_type_.clear();
  gates_clauses_.clear();

  // Lets release the memory on exit.
  CHECK(tmp_binary_clauses_.empty());
  absl::Cleanup binary_cleanup = [this] { tmp_binary_clauses_.clear(); };

  ExtractAndGatesAndFillShortTruthTables(timer);
  ExtractShortGates(timer);

  // All vector have the same size.
  // Except gates_clauses_ which is only filled if we need proof.
  CHECK_EQ(gates_target_.size(), gates_type_.size());
  CHECK_EQ(gates_target_.size(), gates_inputs_.size());
  if (lrat_proof_handler_ != nullptr) {
    CHECK_EQ(gates_target_.size(), gates_clauses_.size());
  }

  // If two gates have the same type and the same inputs, their targets are
  // equivalent. We use an hash set to detect that the inputs are the same.
  absl::flat_hash_set<GateId, GateHash, GateEq> gate_set(
      /*capacity=*/gates_inputs_.size(), GateHash(&gates_type_, &gates_inputs_),
      GateEq(&gates_type_, &gates_inputs_));

  // Used to find representatives as we detect equivalent literal.
  DenseConnectedComponentsFinder union_find;
  union_find.SetNumberOfNodes(num_literals);

  // This will also be updated as we detect equivalences and allows to find
  // all the gates with a given input literal, taking into account all its
  // equivalences.
  //
  // Tricky: we need to resize this to num_literals because the union_find that
  // merges target can choose for a representative a literal that is not in the
  // set of gate inputs.
  MergeableOccurrenceList<BooleanVariable, GateId> input_var_to_gate;
  struct GetVarMapper {
    BooleanVariable operator()(LiteralIndex l) const {
      return Literal(l).Variable();
    }
  };
  input_var_to_gate.ResetFromTransposeMap<GetVarMapper>(gates_inputs_,
                                                        num_variables);

  LratGateCongruenceHelper lrat_helper(trail_, clause_manager_,
                                       lrat_proof_handler_, gates_target_,
                                       gates_clauses_, union_find);

  // Stats + make sure we run it at exit.
  int num_units = 0;
  int num_equivalences = 0;
  int num_processed = 0;
  int arity1_equivalences = 0;
  absl::Cleanup stat_cleanup = [&] {
    total_wtime_ += timer.wtime();
    total_dtime_ += timer.deterministic_time();
    total_equivalences_ += num_equivalences;
    total_num_units_ += num_units;
    timer.AddCounter("processed", num_processed);
    timer.AddCounter("units", num_units);
    timer.AddCounter("f1_equiv", arity1_equivalences);
    timer.AddCounter("equiv", num_equivalences);
  };

  // Starts with all gates in the queue.
  const int num_gates = gates_inputs_.size();
  total_gates_ += num_gates;
  std::vector<bool> in_queue(num_gates, true);
  std::vector<GateId> queue(num_gates);
  for (GateId id(0); id < num_gates; ++id) queue[id.value()] = id;

  int num_processed_fixed_variables = trail_->Index();

  const auto fix_literal = [&, this](Literal to_fix,
                                     absl::Span<const ClausePtr> proof) {
    DCHECK_EQ(to_fix, lrat_helper.GetRepresentativeWithProofSupport(to_fix));
    if (assignment_.LiteralIsTrue(to_fix)) return true;
    if (!clause_manager_->InprocessingFixLiteral(to_fix, proof)) {
      return false;
    }

    // This is quite tricky: as we fix a literal, we propagate right away
    // everything implied by it in the binary implication graph. So we need to
    // loop over all newly_fixed variable in order to properly reach the fix
    // point!
    ++num_units;
    for (; num_processed_fixed_variables < trail_->Index();
         ++num_processed_fixed_variables) {
      const Literal to_update = lrat_helper.GetRepresentativeWithProofSupport(
          (*trail_)[num_processed_fixed_variables]);
      for (const GateId gate_id : input_var_to_gate[to_update.Variable()]) {
        if (in_queue[gate_id.value()]) continue;
        queue.push_back(gate_id);
        in_queue[gate_id.value()] = true;
      }
      input_var_to_gate.ClearList(to_update.Variable());
    }
    return true;
  };

  const auto new_equivalence = [&, this](Literal a, Literal b,
                                         ClausePtr a_implies_b,
                                         ClausePtr b_implies_a) {
    if (a == b.Negated()) {
      // The model is UNSAT.
      if (lrat_proof_handler_ != nullptr) {
        DCHECK_EQ(a_implies_b, ClausePtr(a.Negated()));
        DCHECK_EQ(b_implies_a, ClausePtr(a));
        lrat_proof_handler_->AddInferredClause(ClausePtr::EmptyClausePtr(),
                                               {a_implies_b, b_implies_a});
      }
      return false;
    }
    // Lets propagate fixed variables as we find new equivalences.
    if (assignment_.LiteralIsAssigned(a)) {
      if (assignment_.LiteralIsTrue(a)) {
        return fix_literal(b, {a_implies_b, ClausePtr(a)});
      } else {
        return fix_literal(b.Negated(), {b_implies_a, ClausePtr(a.Negated())});
      }
    } else if (assignment_.LiteralIsAssigned(b)) {
      if (assignment_.LiteralIsTrue(b)) {
        return fix_literal(a, {b_implies_a, ClausePtr(b)});
      } else {
        return fix_literal(a.Negated(), {a_implies_b, ClausePtr(b.Negated())});
      }
    }
    if (lrat_proof_handler_ != nullptr) {
      DCHECK_EQ(a_implies_b, ClausePtr(a.Negated(), b));
      DCHECK_EQ(b_implies_a, ClausePtr(b.Negated(), a));
    }

    ++num_equivalences;
    DCHECK(!implication_graph_->IsRedundant(a));
    DCHECK(!implication_graph_->IsRedundant(b));
    if (!implication_graph_->AddBinaryClause(a.Negated(), b) ||
        !implication_graph_->AddBinaryClause(b.Negated(), a)) {
      return false;
    }

    BooleanVariable to_merge_var = kNoBooleanVariable;
    BooleanVariable rep_var = kNoBooleanVariable;
    for (const bool negate : {false, true}) {
      const LiteralIndex x = negate ? a.NegatedIndex() : a.Index();
      const LiteralIndex y = negate ? b.NegatedIndex() : b.Index();
      const ClausePtr x_implies_y = negate ? b_implies_a : a_implies_b;
      const ClausePtr y_implies_x = negate ? a_implies_b : b_implies_a;

      // Because x always refer to a and y to b, this should maintain
      // the invariant root(lit) = root(lit.Negated()).Negated().
      // This is checked below.
      union_find.AddEdge(x.value(), y.value());
      const LiteralIndex rep(union_find.FindRoot(y.value()));
      const LiteralIndex to_merge = rep == x ? y : x;
      if (to_merge_var == kNoBooleanVariable) {
        to_merge_var = Literal(to_merge).Variable();
        rep_var = Literal(rep).Variable();
      } else {
        // We should have the same var.
        CHECK_EQ(to_merge_var, Literal(to_merge).Variable());
        CHECK_EQ(rep_var, Literal(rep).Variable());
      }

      if (lrat_proof_handler_ != nullptr) {
        if (rep == x) {
          lrat_helper.AddGateEquivalenceClauses(Literal(y), y_implies_x,
                                                x_implies_y);
        } else {
          lrat_helper.AddGateEquivalenceClauses(Literal(x), x_implies_y,
                                                y_implies_x);
        }
      }
    }

    // Invariant.
    CHECK_EQ(
        lrat_helper.GetRepresentativeWithProofSupport(a),
        lrat_helper.GetRepresentativeWithProofSupport(a.Negated()).Negated());
    CHECK_EQ(
        lrat_helper.GetRepresentativeWithProofSupport(b),
        lrat_helper.GetRepresentativeWithProofSupport(b.Negated()).Negated());

    // Re-add to the queue all gates with touched inputs.
    //
    // TODO(user): I think we could only add the gates of "to_merge"
    // before we merge. This part of the code is quite quick in any
    // case.
    input_var_to_gate.MergeInto(to_merge_var, rep_var);
    for (const GateId gate_id : input_var_to_gate[rep_var]) {
      if (in_queue[gate_id.value()]) continue;
      queue.push_back(gate_id);
      in_queue[gate_id.value()] = true;
    }

    return true;
  };

  // Main loop.
  while (!queue.empty()) {
    ++num_processed;
    const GateId id = queue.back();
    queue.pop_back();
    CHECK(in_queue[id.value()]);
    in_queue[id.value()] = false;

    // Tricky: the hash-map might contain id not yet canonicalized. And in
    // particular might already contain the id we are processing.
    //
    // The first pass will check equivalence with the "non-canonical
    // version" and remove id if it was already there. The second will do it on
    // the canonicalized version.
    for (int pass = 0; pass < 2; ++pass) {
      GateId other_id(-1);
      bool is_equivalent = false;
      if (pass == 0) {
        const auto it = gate_set.find(id);
        if (it != gate_set.end()) {
          other_id = *it;
          if (id == other_id) {
            // This gate was already in the set, remove it before we
            // insert its potentially different canonicalized version.
            gate_set.erase(it);
          } else {
            is_equivalent = true;
          }
        }
      } else {
        const auto [it, inserted] = gate_set.insert(id);
        if (!inserted) {
          other_id = *it;
          is_equivalent = true;
        }
      }

      if (is_equivalent) {
        CHECK_NE(id, other_id);
        CHECK_GE(other_id, 0);
        CHECK_EQ(gates_type_[id], gates_type_[other_id]);
        CHECK_EQ(gates_inputs_[id], gates_inputs_[other_id]);

        input_var_to_gate.RemoveFromFutureOutput(id);

        // We detected a <=> b (or, equivalently, rep(a) <=> rep(b)).
        const Literal a(gates_target_[id]);
        const Literal b(gates_target_[other_id]);
        const Literal rep_a = lrat_helper.GetRepresentativeWithProofSupport(a);
        const Literal rep_b = lrat_helper.GetRepresentativeWithProofSupport(b);
        if (rep_a != rep_b) {
          ClausePtr rep_a_implies_rep_b = kNullClausePtr;
          ClausePtr rep_b_implies_rep_a = kNullClausePtr;
          if (lrat_proof_handler_ != nullptr) {
            if (gates_type_[id] == kAndGateType) {
              rep_a_implies_rep_b =
                  lrat_helper.AddAndGateTargetImplication(id, other_id);
              rep_b_implies_rep_a =
                  lrat_helper.AddAndGateTargetImplication(other_id, id);
            } else {
              const auto [x, y] = lrat_helper.AddShortGateEquivalence(
                  rep_a, rep_b, {id, other_id});
              rep_a_implies_rep_b = x;
              rep_b_implies_rep_a = y;
            }
          }
          if (!new_equivalence(rep_a, rep_b, rep_a_implies_rep_b,
                               rep_b_implies_rep_a)) {
            return false;
          }
        }
        break;
      }

      // Canonicalize on pass zero, otherwise loop.
      // Note that even if nothing change, we want to run canonicalize at
      // least once on the small "truth table" gates.
      //
      // Note that sorting works for and_gate and any gate that does not depend
      // on the order of its inputs. But if we add more fancy functions, we will
      // need to be careful.
      if (pass > 0) continue;

      if (gates_type_[id] == kAndGateType) {
        absl::Span<LiteralIndex> inputs = gates_inputs_[id];
        marked_.ResetAllToFalse();
        int new_size = 0;
        bool is_unit = false;
        for (const LiteralIndex l : inputs) {
          if (lrat_proof_handler_ != nullptr) {
            lrat_helper.ShortenEquivalencesWithRepresentative(Literal(l));
          }
          const LiteralIndex rep(union_find.FindRoot(l.value()));
          if (marked_[rep]) continue;

          // This only works for and-gate, if both lit and not(lit) are input,
          // then target must be false.
          if (marked_[Literal(rep).Negated()]) {
            is_unit = true;
            input_var_to_gate.RemoveFromFutureOutput(id);

            const Literal initial_to_fix = Literal(gates_target_[id]).Negated();
            const Literal to_fix =
                lrat_helper.GetRepresentativeWithProofSupport(initial_to_fix);
            if (!assignment_.LiteralIsTrue(to_fix)) {
              absl::InlinedVector<ClausePtr, 5> proof;
              if (lrat_proof_handler_ != nullptr) {
                lrat_helper.AppendFixAndGateTargetClauses(id, Literal(rep),
                                                          proof);
              }
              if (!fix_literal(to_fix, proof)) return false;
            }
            break;
          }

          marked_.Set(rep);
          inputs[new_size++] = rep;
        }

        if (is_unit) {
          break;  // Abort the passes loop.
        }

        // We need to re-sort, even if the new_size is the same, since
        // representatives might be different.
        CHECK_GT(new_size, 0);
        std::sort(inputs.begin(), inputs.begin() + new_size);
        gates_inputs_.Shrink(id, new_size);

        // Lets convert a kAndGateType to the short "type" if we can. The truth
        // table is simply a 1 on the last position (where all inputs are ones).
        // We fall back to the case below to canonicalize further.
        //
        // This is needed because while our generic and_gate use 1 clause +
        // binary, it won't detect a kAndGateType "badly" encoded with 4 ternary
        // clauses for instance:
        //
        // b & c           => a
        // not(b) & c      => not(a)
        // b & not(c)      => not(a)
        // not(b) & not(c) => not(a)
        //
        // This is even more important since "generic" short gates might get
        // simplified as we detect equivalences, and become an and_gate later.
        if (new_size > 4) continue;
        gates_type_[id] = 1 << ((1 << new_size) - 1);
      }

      // Generic "short" gates.
      // We just take the representative and re-canonicalize.
      DCHECK_GE(gates_type_[id], 0);
      DCHECK_EQ(gates_type_[id] >> (1 << (gates_inputs_[id].size())), 0);
      for (LiteralIndex& lit_ref : gates_inputs_[id]) {
        lit_ref =
            lrat_helper.GetRepresentativeWithProofSupport(Literal(lit_ref))
                .Index();
      }

      const int new_size = CanonicalizeShortGate(id);
      if (new_size == 1) {
        // We have a function of size 1! This is an equivalence.
        input_var_to_gate.RemoveFromFutureOutput(id);
        const Literal a = Literal(gates_target_[id]);
        const Literal b = Literal(gates_inputs_[id][0]);
        const Literal rep_a = lrat_helper.GetRepresentativeWithProofSupport(a);
        const Literal rep_b = lrat_helper.GetRepresentativeWithProofSupport(b);
        if (rep_a != rep_b) {
          ++arity1_equivalences;
          const auto [a_to_b, b_to_a] =
              lrat_helper.AddShortGateEquivalence(rep_a, rep_b, {id});
          if (!new_equivalence(rep_a, rep_b, a_to_b, b_to_a)) {
            return false;
          }
        }
        break;
      } else if (new_size == 0) {
        // We have a fixed function! Just fix the literal.
        input_var_to_gate.RemoveFromFutureOutput(id);
        const Literal initial_to_fix =
            (gates_type_[id] & 1) == 1 ? Literal(gates_target_[id])
                                       : Literal(gates_target_[id]).Negated();
        const Literal to_fix =
            lrat_helper.GetRepresentativeWithProofSupport(initial_to_fix);
        if (!assignment_.LiteralIsTrue(to_fix)) {
          if (!fix_literal(to_fix,
                           {lrat_helper.ProofForFixingLiteral(to_fix, id)})) {
            return false;
          }
        }
        break;
      }
    }
  }

  // DEBUG check that we reached the fix point correctly.
  if (DEBUG_MODE) {
    CHECK(queue.empty());
    gate_set.clear();
    for (GateId id(0); id < num_gates; ++id) {
      if (gates_type_[id] == kAndGateType) continue;
      if (assignment_.LiteralIsAssigned(Literal(gates_target_[id]))) continue;

      const int new_size = CanonicalizeShortGate(id);
      if (new_size == 0) {
        CHECK_EQ(gates_type_[id] & 1, 0);
        const Literal initial_to_fix = Literal(gates_target_[id]).Negated();
        const Literal to_fix =
            lrat_helper.GetRepresentativeWithProofSupport(initial_to_fix);
        CHECK(assignment_.LiteralIsTrue(to_fix));
      } else if (new_size == 1) {
        CHECK(!assignment_.LiteralIsAssigned(Literal(gates_target_[id])));
        CHECK(!assignment_.LiteralIsAssigned(Literal(gates_inputs_[id][0])));
        CHECK_EQ(lrat_helper.GetRepresentativeWithProofSupport(
                     Literal(gates_target_[id])),
                 lrat_helper.GetRepresentativeWithProofSupport(
                     Literal(gates_inputs_[id][0])))
            << id << " ";
      } else {
        const auto [it, inserted] = gate_set.insert(id);
        if (!inserted) {
          const GateId other_id = *it;
          CHECK_EQ(lrat_helper.GetRepresentativeWithProofSupport(
                       Literal(gates_target_[id])),
                   lrat_helper.GetRepresentativeWithProofSupport(
                       Literal(gates_target_[other_id])))
              << id << " " << gates_inputs_[id] << " " << other_id << " "
              << gates_inputs_[other_id];
        }
      }
    }
  }

  return true;
}

}  // namespace sat
}  // namespace operations_research
