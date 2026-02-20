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

#include "ortools/sat/probing.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/cleanup/cleanup.h"
#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/log/vlog_is_on.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/strong_vector.h"
#include "ortools/base/timer.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/implied_bounds.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/lrat_proof_handler.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/util.h"
#include "ortools/util/bitset.h"
#include "ortools/util/logging.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/strong_integers.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

// Holds a copy of the trail and of propagator reasons in order to be able to
// build LRAT proofs after the trail has been modified.
class TrailCopy {
 public:
  TrailCopy(const Trail& trail, const ClauseManager& clause_manager,
            const LratProofHandler* lrat_proof_handler)
      : trail_(trail),
        clause_manager_(clause_manager),
        lrat_proof_handler_(lrat_proof_handler) {}

  // Updates this trail copy with the current trail state. Only trail elements
  // starting from `start_index` are copied.
  void CopyTrail(int start_index) {
    start_index_ = start_index;
    new_trail_index_.resize(trail_.NumVariables());
    new_trail_literals_.clear();
    new_trail_info_.clear();
    const int trail_size_delta = trail_.Index() - start_index;
    new_trail_literals_.reserve(trail_size_delta);
    new_trail_info_.reserve(trail_size_delta);
    for (int i = 0; i < trail_size_delta; ++i) {
      const Literal literal = trail_[start_index_ + i];
      const BooleanVariable var = literal.Variable();
      const AssignmentInfo& info = trail_.Info(var);
      const int assignment_type = trail_.AssignmentType(var);
      const ClausePtr reason_clause = clause_manager_.ReasonClausePtr(literal);
      new_trail_index_[var] = start_index_ + i;
      new_trail_literals_.push_back(literal);
      new_trail_info_.emplace_back(info.level, assignment_type, reason_clause);
    }

    const int num_decisions = trail_.CurrentDecisionLevel();
    decisions_.clear();
    decisions_.reserve(num_decisions);
    for (int i = 0; i < num_decisions; ++i) {
      decisions_.push_back(trail_.Decisions()[i].literal);
    }
  }

  // Same as ClauseManager::AppendClausesFixing(), but adapted to work on this
  // trail copy instead of on the real trail.
  void AppendClausesFixing(absl::Span<const Literal> literals,
                           std::vector<ClausePtr>* clauses,
                           LiteralIndex decision) {
    // Mark the literals whose reason must be expanded, and put them in a heap.
    tmp_mark_.ClearAndResize(
        BooleanVariable(start_index_ + new_trail_index_.size()));
    marked_trail_indices_heap_.clear();
    for (const Literal lit : literals) {
      tmp_mark_.Set(lit.Variable());
      marked_trail_indices_heap_.push_back(GetTrailIndex(lit.Variable()));
    }
    absl::c_make_heap(marked_trail_indices_heap_);
    const int current_level = decisions_.size();

    // The min level of the expanded literals.
    int min_level = current_level;

    // Unit clauses must come first. We put them in clauses directly. We put
    // the others in non_unit_clauses and append them to clauses at the end.
    std::vector<ClausePtr>& non_unit_clauses =
        tmp_clauses_for_append_clauses_fixing_;
    non_unit_clauses.clear();

    while (!marked_trail_indices_heap_.empty()) {
      absl::c_pop_heap(marked_trail_indices_heap_);
      const int trail_index = marked_trail_indices_heap_.back();
      marked_trail_indices_heap_.pop_back();
      const Literal marked_literal = GetTrailLiteral(trail_index);
      const TrailInfo& trail_info = GetTrailInfo(trail_index);

      // Stop at decisions, at literals fixed at root, and at literals implied
      // by the decision at their level.
      const int level = trail_info.level;
      if (level > 0) min_level = std::min(min_level, level);
      if (trail_info.assignment_type == AssignmentType::kSearchDecision) {
        continue;
      }
      if (level == 0) {
        clauses->push_back(trail_info.reason_clause);
        continue;
      }
      const Literal level_decision = decisions_[level - 1];
      ClausePtr clause = kNullClausePtr;
      if (lrat_proof_handler_->HasBinaryClause(level_decision.Negated(),
                                               marked_literal)) {
        clause = ClausePtr(level_decision.Negated(), marked_literal);
      }
      if (clause != kNullClausePtr) {
        non_unit_clauses.push_back(clause);
        continue;
      }

      // Mark all the literals of its reason.
      for (const Literal literal : trail_info.reason_clause.GetLiterals()) {
        const BooleanVariable var = literal.Variable();
        if (!tmp_mark_[var]) {
          const int trail_index = GetTrailIndex(var);
          const TrailInfo info = GetTrailInfo(trail_index);
          tmp_mark_.Set(var);
          if (info.level > 0) {
            marked_trail_indices_heap_.push_back(trail_index);
            absl::c_push_heap(marked_trail_indices_heap_);
          } else {
            clauses->push_back(info.reason_clause);
          }
        }
      }
      non_unit_clauses.push_back(trail_info.reason_clause);
    }

    // Add the implication chain from `decision` to all the decisions found
    // during the expansion.
    if (Literal(decision) != decisions_[current_level - 1]) {
      // If `decision` is not the last decision, it must directly imply it.
      clauses->push_back(ClausePtr(Literal(decision).Negated(),
                                   decisions_[current_level - 1]));
    }
    for (int level = current_level - 1; level >= min_level; --level) {
      clauses->push_back(
          ClausePtr(decisions_[level].Negated(), decisions_[level - 1]));
    }

    clauses->insert(clauses->end(), non_unit_clauses.rbegin(),
                    non_unit_clauses.rend());
  }

 private:
  struct TrailInfo {
    uint32_t level;
    int assignment_type;
    ClausePtr reason_clause;
  };

  int GetTrailIndex(BooleanVariable var) const {
    int trail_index = trail_.Info(var).trail_index;
    if (trail_index < start_index_) return trail_index;
    return new_trail_index_[var];
  }

  Literal GetTrailLiteral(int trail_index) const {
    if (trail_index < start_index_) return trail_[trail_index];
    return new_trail_literals_[trail_index - start_index_];
  }

  TrailInfo GetTrailInfo(int trail_index) const {
    if (trail_index < start_index_) {
      const Literal literal = trail_[trail_index];
      const BooleanVariable var = literal.Variable();
      return {.level = trail_.Info(var).level,
              .assignment_type = trail_.AssignmentType(var),
              .reason_clause = clause_manager_.ReasonClausePtr(literal)};
    }
    return new_trail_info_[trail_index - start_index_];
  }

  const Trail& trail_;
  const ClauseManager& clause_manager_;
  const LratProofHandler* lrat_proof_handler_;

  // Trail elements starting from this index are read from the new_xxx fields.
  // Others are read from the original trail_.
  int start_index_;
  util_intops::StrongVector<BooleanVariable, int> new_trail_index_;
  std::vector<Literal> new_trail_literals_;
  std::vector<TrailInfo> new_trail_info_;
  std::vector<Literal> decisions_;

  SparseBitset<BooleanVariable> tmp_mark_;
  std::vector<int> marked_trail_indices_heap_;
  std::vector<ClausePtr> tmp_clauses_for_append_clauses_fixing_;
};

Prober::Prober(Model* model)
    : trail_(*model->GetOrCreate<Trail>()),
      assignment_(model->GetOrCreate<SatSolver>()->Assignment()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()),
      implied_bounds_(model->GetOrCreate<ImpliedBounds>()),
      product_detector_(model->GetOrCreate<ProductDetector>()),
      sat_solver_(model->GetOrCreate<SatSolver>()),
      time_limit_(model->GetOrCreate<TimeLimit>()),
      implication_graph_(model->GetOrCreate<BinaryImplicationGraph>()),
      clause_manager_(model->GetOrCreate<ClauseManager>()),
      lrat_proof_handler_(model->Mutable<LratProofHandler>()),
      trail_copy_(new TrailCopy(trail_, *clause_manager_, lrat_proof_handler_)),
      logger_(model->GetOrCreate<SolverLogger>()) {}

Prober::~Prober() { delete trail_copy_; }

bool Prober::ProbeBooleanVariables(const double deterministic_time_limit) {
  const int num_variables = sat_solver_->NumVariables();
  const VariablesAssignment& assignment = sat_solver_->Assignment();
  std::vector<BooleanVariable> bool_vars;
  for (BooleanVariable b(0); b < num_variables; ++b) {
    if (assignment.VariableIsAssigned(b)) continue;
    const Literal literal(b, true);
    if (implication_graph_->RepresentativeOf(literal) != literal) {
      continue;
    }
    bool_vars.push_back(b);
  }
  return ProbeBooleanVariables(deterministic_time_limit, bool_vars);
}

bool Prober::ProbeOneVariableInternal(BooleanVariable b) {
  new_integer_bounds_.clear();
  propagated_.ResetAllToFalse();
  // We block clause deletion since we compute some LRAT proofs in a delayed
  // way, and we need to make sure the clauses that were used are still around.
  sat_solver_->BlockClauseDeletion(true);
  absl::Cleanup unblock_clause_deletion = [&] {
    sat_solver_->BlockClauseDeletion(false);
  };
  for (const Literal decision : {Literal(b, true), Literal(b, false)}) {
    if (assignment_.LiteralIsAssigned(decision)) continue;

    ++num_decisions_;
    CHECK_EQ(sat_solver_->CurrentDecisionLevel(), 0);
    const int saved_index = trail_.Index();
    if (sat_solver_->EnqueueDecisionAndBackjumpOnConflict(decision) ==
        kUnsatTrailIndex) {
      return false;
    }
    sat_solver_->AdvanceDeterministicTime(time_limit_);

    if (sat_solver_->ModelIsUnsat()) return false;
    if (sat_solver_->CurrentDecisionLevel() == 0) continue;
    if (trail_.Index() > saved_index) {
      if (callback_ != nullptr) callback_(decision);
    }

    if (!implied_bounds_->ProcessIntegerTrail(decision)) return false;
    product_detector_->ProcessTrailAtLevelOne();
    integer_trail_->AppendNewBounds(&new_integer_bounds_);
    to_fix_at_true_.clear();
    new_literals_implied_by_decision_.clear();
    new_implied_or_fixed_literals_.clear();
    for (int i = saved_index + 1; i < trail_.Index(); ++i) {
      const Literal l = trail_[i];

      // We mark on the first pass (b.IsPositive()) and check on the second.
      bool added_to_new_implied_of_fixed_literals = false;
      if (decision.IsPositive()) {
        propagated_.Set(l.Index());
      } else {
        if (propagated_[l]) {
          to_fix_at_true_.push_back(l);
          if (lrat_proof_handler_ != nullptr) {
            new_implied_or_fixed_literals_.push_back(l);
            added_to_new_implied_of_fixed_literals = true;
          }
        }
      }

      // Anything not propagated by the BinaryImplicationGraph is a "new"
      // binary clause. This is because the BinaryImplicationGraph has the
      // highest priority of all propagators.
      if (trail_.AssignmentType(l.Variable()) !=
          implication_graph_->PropagatorId()) {
        if (l == decision.Negated()) {
          to_fix_at_true_.push_back(l);
        } else if (l != decision) {
          new_literals_implied_by_decision_.push_back(l);
        }
        if (lrat_proof_handler_ != nullptr && l != decision &&
            !added_to_new_implied_of_fixed_literals) {
          new_implied_or_fixed_literals_.push_back(l);
        }
      }
    }

    if (lrat_proof_handler_ != nullptr) {
      for (const Literal l : new_implied_or_fixed_literals_) {
        tmp_proof_.clear();
        clause_manager_->AppendClausesFixing({l}, &tmp_proof_,
                                             decision.Index());
        if (l == decision.Negated()) {
          lrat_proof_handler_->AddInferredClause(ClausePtr(l), tmp_proof_);
        } else {
          lrat_proof_handler_->AddInferredClause(
              ClausePtr(decision.Negated(), l), tmp_proof_);
        }
        num_lrat_clauses_++;
        num_lrat_proof_clauses_ += tmp_proof_.size();
      }
      if (decision.IsPositive()) {
        trail_copy_->CopyTrail(saved_index);
      } else {
        for (const Literal l : to_fix_at_true_) {
          if (lrat_proof_handler_->HasBinaryClause(decision, l)) continue;
          tmp_proof_.clear();
          trail_copy_->AppendClausesFixing({l}, &tmp_proof_,
                                           decision.NegatedIndex());
          lrat_proof_handler_->AddInferredClause(ClausePtr(decision, l),
                                                 tmp_proof_);
          num_lrat_clauses_++;
          num_lrat_proof_clauses_ += tmp_proof_.size();
        }
      }
    }

    // Fix variables and add new binary clauses.
    if (!sat_solver_->ResetToLevelZero()) return false;
    DCHECK_GE(trail_.Index(), saved_index);
    for (const Literal l : to_fix_at_true_) {
      if (lrat_proof_handler_ != nullptr && l != decision.Negated()) {
        lrat_proof_handler_->AddInferredClause(
            ClausePtr(l),
            {ClausePtr(decision.Negated(), l), ClausePtr(decision, l)});
        num_lrat_clauses_++;
        num_lrat_proof_clauses_ += 2;
      }
      if (!clause_manager_->InprocessingAddUnitClause(l)) {
        return false;
      }
    }
    if (!sat_solver_->FinishPropagation()) return false;
    for (const Literal l : new_literals_implied_by_decision_) {
      // Some variables can be fixed by the above loop.
      if (trail_.Assignment().LiteralIsAssigned(decision)) break;
      if (trail_.Assignment().LiteralIsAssigned(l)) continue;
      num_new_binary_++;
      // Tricky: by default AddBinaryClause() can delete the LRAT original
      // clause and create a new one between the representatives. But the
      // original clause, registered in tmp_binary_clauses_, can be needed in
      // the next iteration for the proof of a new fixed literal. Hence we must
      // not delete it here. Instead, it is deleted at the end of this method,
      // with the other non-longer needed clauses.
      // TODO(user): can we maintain a one to one correspondence of clauses
      // in LRAT and in the binary implication graph to avoid this?
      if (!implication_graph_->AddBinaryClause(
              decision.Negated(), l,
              /*delete_non_representative_id=*/false)) {
        return false;
      }
    }
    if (!sat_solver_->FinishPropagation()) return false;
  }
  if (lrat_proof_handler_ != nullptr) {
    num_unneeded_lrat_clauses_ +=
        lrat_proof_handler_->DeleteTemporaryBinaryClauses();
  }

  // We have at most two lower bounds for each variables (one for b==0 and one
  // for b==1), so the min of the two is a valid level zero bound! More
  // generally, the domain of a variable can be intersected with the union
  // of the two propagated domains. This also allow to detect "holes".
  //
  // TODO(user): More generally, for any clauses (b or not(b) is one), we
  // could probe all the literal inside, and for any integer variable, we can
  // take the union of the propagated domain as a new domain.
  //
  // TODO(user): fix binary variable in the same way? It might not be as
  // useful since probing on such variable will also fix it. But then we might
  // abort probing early, so it might still be good.
  std::sort(new_integer_bounds_.begin(), new_integer_bounds_.end(),
            [](IntegerLiteral a, IntegerLiteral b) { return a.var < b.var; });

  // This is used for the hole detection.
  IntegerVariable prev_var = kNoIntegerVariable;
  IntegerValue lb_max = kMinIntegerValue;
  IntegerValue ub_min = kMaxIntegerValue;
  new_integer_bounds_.push_back(IntegerLiteral());  // Sentinel.

  const int limit = new_integer_bounds_.size();
  for (int i = 0; i < limit; ++i) {
    const IntegerVariable var = new_integer_bounds_[i].var;

    // Hole detection.
    if (i > 0 && PositiveVariable(var) != prev_var) {
      if (ub_min + 1 < lb_max) {
        // The variable cannot take value in (ub_min, lb_max) !
        //
        // TODO(user): do not create domain with a complexity that is too
        // large?
        const Domain old_domain =
            integer_trail_->InitialVariableDomain(prev_var);
        const Domain new_domain = old_domain.IntersectionWith(
            Domain(ub_min.value() + 1, lb_max.value() - 1).Complement());
        if (new_domain != old_domain) {
          ++num_new_holes_;
          if (!integer_trail_->UpdateInitialDomain(prev_var, new_domain)) {
            return false;
          }
        }
      }

      // Reinitialize.
      lb_max = kMinIntegerValue;
      ub_min = kMaxIntegerValue;
    }

    prev_var = PositiveVariable(var);
    if (VariableIsPositive(var)) {
      lb_max = std::max(lb_max, new_integer_bounds_[i].bound);
    } else {
      ub_min = std::min(ub_min, -new_integer_bounds_[i].bound);
    }

    // Bound tightening.
    if (i == 0 || new_integer_bounds_[i - 1].var != var) continue;
    const IntegerValue new_bound = std::min(new_integer_bounds_[i - 1].bound,
                                            new_integer_bounds_[i].bound);
    if (new_bound > integer_trail_->LowerBound(var)) {
      ++num_new_integer_bounds_;
      if (!integer_trail_->Enqueue(
              IntegerLiteral::GreaterOrEqual(var, new_bound), {}, {})) {
        return false;
      }
    }
  }

  // We might have updated some integer domain, let's propagate.
  return sat_solver_->FinishPropagation();
}

bool Prober::ProbeOneVariable(BooleanVariable b) {
  // Resize the propagated sparse bitset.
  const int num_variables = sat_solver_->NumVariables();
  propagated_.ClearAndResize(LiteralIndex(2 * num_variables));

  // Reset the solver in case it was already used.
  if (!sat_solver_->ResetToLevelZero()) return false;

  const int initial_num_fixed = sat_solver_->LiteralTrail().Index();
  if (!ProbeOneVariableInternal(b)) return false;

  // Statistics
  const int num_fixed = sat_solver_->LiteralTrail().Index();
  num_new_literals_fixed_ += num_fixed - initial_num_fixed;
  return true;
}

bool Prober::ProbeBooleanVariables(
    const double deterministic_time_limit,
    absl::Span<const BooleanVariable> bool_vars) {
  WallTimer wall_timer;
  wall_timer.Start();

  // Reset statistics.
  num_decisions_ = 0;
  num_new_binary_ = 0;
  num_new_holes_ = 0;
  num_new_integer_bounds_ = 0;
  num_new_literals_fixed_ = 0;
  num_lrat_clauses_ = 0;
  num_lrat_proof_clauses_ = 0;
  num_unneeded_lrat_clauses_ = 0;

  // Resize the propagated sparse bitset.
  const int num_variables = sat_solver_->NumVariables();
  propagated_.ClearAndResize(LiteralIndex(2 * num_variables));

  // Reset the solver in case it was already used.
  if (!sat_solver_->ResetToLevelZero()) return false;

  const int initial_num_fixed = sat_solver_->LiteralTrail().Index();
  const double initial_deterministic_time =
      time_limit_->GetElapsedDeterministicTime();
  const double limit = initial_deterministic_time + deterministic_time_limit;

  bool limit_reached = false;
  int num_probed = 0;

  for (const BooleanVariable b : bool_vars) {
    const Literal literal(b, true);
    if (implication_graph_->RepresentativeOf(literal) != literal) {
      continue;
    }

    // TODO(user): Instead of an hard deterministic limit, we should probably
    // use a lower one, but reset it each time we have found something useful.
    if (time_limit_->LimitReached() ||
        time_limit_->GetElapsedDeterministicTime() > limit) {
      limit_reached = true;
      break;
    }

    // Propagate b=1 and then b=0.
    ++num_probed;
    if (!ProbeOneVariableInternal(b)) {
      return false;
    }
  }

  // Update stats.
  const int num_fixed = sat_solver_->LiteralTrail().Index();
  num_new_literals_fixed_ = num_fixed - initial_num_fixed;

  // Display stats.
  if (logger_->LoggingIsEnabled()) {
    const double time_diff =
        time_limit_->GetElapsedDeterministicTime() - initial_deterministic_time;
    SOLVER_LOG(logger_, "[Probing] deterministic_time: ", time_diff,
               " (limit: ", deterministic_time_limit,
               ") wall_time: ", wall_timer.Get(), " (",
               (limit_reached ? "Aborted " : ""), num_probed, "/",
               bool_vars.size(), ")");
    if (num_new_literals_fixed_ > 0) {
      SOLVER_LOG(logger_,
                 "[Probing]  - new fixed Boolean: ", num_new_literals_fixed_,
                 " (", FormatCounter(num_fixed), "/",
                 FormatCounter(sat_solver_->NumVariables()), ")");
    }
    if (num_new_holes_ > 0) {
      SOLVER_LOG(logger_, "[Probing]  - new integer holes: ",
                 FormatCounter(num_new_holes_));
    }
    if (num_new_integer_bounds_ > 0) {
      SOLVER_LOG(logger_, "[Probing]  - new integer bounds: ",
                 FormatCounter(num_new_integer_bounds_));
    }
    if (num_new_binary_ > 0) {
      SOLVER_LOG(logger_, "[Probing]  - new binary clause: ",
                 FormatCounter(num_new_binary_));
    }
    if (num_lrat_clauses_ > 0) {
      SOLVER_LOG(logger_, "[Probing]  - new LRAT clauses: ",
                 FormatCounter(num_lrat_clauses_));
    }
    if (num_lrat_proof_clauses_ > 0) {
      SOLVER_LOG(logger_, "[Probing]  - new LRAT proof clauses: ",
                 FormatCounter(num_lrat_proof_clauses_));
    }
    if (num_unneeded_lrat_clauses_ > 0) {
      SOLVER_LOG(logger_, "[Probing]  - unneeded LRAT clauses: ",
                 FormatCounter(num_unneeded_lrat_clauses_));
    }
  }

  return true;
}

bool Prober::ProbeDnf(absl::string_view name,
                      absl::Span<const std::vector<Literal>> dnf,
                      DnfType dnf_type, const SatClause* dnf_clause) {
  if (dnf.size() <= 1) return true;

  // dnf_clause could be deleted as a side effect of probing, but is needed for
  // LRAT in FixProbedDnfLiterals(). To avoid this we block clause deletion
  // during probing.
  sat_solver_->BlockClauseDeletion(true);
  absl::Cleanup unblock_clause_deletion = [&] {
    sat_solver_->BlockClauseDeletion(false);
  };

  // Reset the solver in case it was already used.
  if (!sat_solver_->ResetToLevelZero()) return false;

  always_propagated_bounds_.clear();
  always_propagated_literals_.clear();
  int num_valid_conjunctions = 0;
  for (absl::Span<const Literal> conjunction : dnf) {
    // TODO(user): instead of going back to level zero, we could backtrack
    // to level 'n', where n is the length of the longest prefix shared between
    // the current conjunction and the previous one (more or less -- conjunction
    // literals which are already assigned or lead to a conflict do not
    // translate to a decision). For a kAtLeastOneCombination DNF with 8
    // conjunctions, this would reduce the number of enqueues from 8*3=24 to 14.
    if (!sat_solver_->ResetToLevelZero()) return false;
    if (num_valid_conjunctions > 0 && always_propagated_bounds_.empty() &&
        always_propagated_literals_.empty()) {
      // We can exit safely as nothing will be propagated.
      return true;
    }

    bool conjunction_is_valid = true;
    const int root_trail_index = trail_.Index();
    const int root_integer_trail_index = integer_trail_->Index();
    for (const Literal& lit : conjunction) {
      if (assignment_.LiteralIsAssigned(lit)) {
        if (assignment_.LiteralIsTrue(lit)) continue;
        conjunction_is_valid = false;
        break;
      }
      const int decision_level_before_enqueue =
          sat_solver_->CurrentDecisionLevel();
      sat_solver_->EnqueueDecisionAndBackjumpOnConflict(lit);
      sat_solver_->AdvanceDeterministicTime(time_limit_);
      const int decision_level_after_enqueue =
          sat_solver_->CurrentDecisionLevel();
      ++num_decisions_;

      if (sat_solver_->ModelIsUnsat()) return false;
      // If the literal has been pushed without any conflict, the level should
      // have been increased.
      if (decision_level_after_enqueue <= decision_level_before_enqueue) {
        conjunction_is_valid = false;
        break;
      }
      // TODO(user): Can we use the callback_?
    }
    if (conjunction_is_valid) {
      ++num_valid_conjunctions;
    } else {
      continue;
    }

    // Process propagated literals.
    new_propagated_literals_.clear();
    for (int i = root_trail_index; i < trail_.Index(); ++i) {
      const LiteralIndex literal_index = trail_[i].Index();
      if (num_valid_conjunctions == 1 ||
          always_propagated_literals_.contains(literal_index)) {
        new_propagated_literals_.insert(literal_index);
      }
    }
    std::swap(new_propagated_literals_, always_propagated_literals_);

    // Process propagated integer bounds.
    new_integer_bounds_.clear();
    integer_trail_->AppendNewBoundsFrom(root_integer_trail_index,
                                        &new_integer_bounds_);
    new_propagated_bounds_.clear();
    for (const IntegerLiteral entry : new_integer_bounds_) {
      const auto it = always_propagated_bounds_.find(entry.var);
      if (num_valid_conjunctions == 1) {  // First loop.
        new_propagated_bounds_[entry.var] = entry.bound;
      } else if (it != always_propagated_bounds_.end()) {
        new_propagated_bounds_[entry.var] = std::min(entry.bound, it->second);
      }
    }
    std::swap(new_propagated_bounds_, always_propagated_bounds_);
  }

  if (!sat_solver_->ResetToLevelZero()) return false;
  // Fix literals implied by the dnf.
  const int previous_num_literals_fixed = num_new_literals_fixed_;
  if (lrat_proof_handler_ != nullptr) {
    if (!FixProbedDnfLiterals(dnf, always_propagated_literals_, dnf_type,
                              dnf_clause)) {
      return false;
    }
  } else {
    for (const LiteralIndex literal_index : always_propagated_literals_) {
      const Literal lit(literal_index);
      if (assignment_.LiteralIsTrue(lit)) continue;
      ++num_new_literals_fixed_;
      if (!sat_solver_->AddUnitClause(lit)) return false;
    }
  }

  // Fix integer bounds implied by the dnf.
  int previous_num_integer_bounds = num_new_integer_bounds_;
  for (const auto& [var, bound] : always_propagated_bounds_) {
    if (bound > integer_trail_->LowerBound(var)) {
      ++num_new_integer_bounds_;
      if (!integer_trail_->Enqueue(IntegerLiteral::GreaterOrEqual(var, bound),
                                   {}, {})) {
        return false;
      }
    }
  }

  if (!sat_solver_->FinishPropagation()) return false;

  if (num_new_integer_bounds_ > previous_num_integer_bounds ||
      num_new_literals_fixed_ > previous_num_literals_fixed) {
    VLOG(1) << "ProbeDnf(" << name << ", num_fixed_literals="
            << num_new_literals_fixed_ - previous_num_literals_fixed
            << ", num_pushed_integer_bounds="
            << num_new_integer_bounds_ - previous_num_integer_bounds
            << ", num_valid_conjunctions=" << num_valid_conjunctions << "/"
            << dnf.size() << ")";
  }

  return true;
}

namespace {
// Sets `implication` to the clause "conjunction => literal". Returns true if
// `conjunction` does not contain `literal`. Otherwise ("conjunction => literal"
// is a tautology), returns false and leaves `implication` in an undefined
// state.
bool GetConjunctionImpliesLiteralClause(absl::Span<const Literal> conjunction,
                                        Literal literal,
                                        std::vector<Literal>& implication) {
  implication.clear();
  for (const Literal lit : conjunction) {
    if (lit == literal) return false;
    if (lit.Negated() == literal) continue;
    implication.push_back(lit.Negated());
  }
  implication.push_back(literal);
  return true;
}
}  // namespace

bool Prober::FixProbedDnfLiterals(
    absl::Span<const std::vector<Literal>> dnf,
    const absl::btree_set<LiteralIndex>& propagated_literals, DnfType dnf_type,
    const SatClause* dnf_clause) {
  if (propagated_literals.empty()) return true;

  // For each propagated literal (in propagated_literals order), and for each
  // conjunction, a temporary LRAT clause "conjunction => propagated literal"
  // (or kNullClausePtr if "conjunction" contains "propagated_literal", if the
  // clause has not been created yet, or has been deleted).
  CompactVectorVector<int, ClausePtr>& propagation_clauses = tmp_dnf_clauses_;
  propagation_clauses.clear();
  propagation_clauses.reserve(propagated_literals.size() * dnf.size());
  for (int i = 0; i < propagated_literals.size(); ++i) {
    propagation_clauses.Add({});
    for (int j = 0; j < dnf.size(); ++j) {
      propagation_clauses.AppendToLastVector(kNullClausePtr);
    }
  }
  // Delete the temporary LRAT clauses.
  auto cleanup = absl::MakeCleanup([&] {
    int i = 0;
    for (const LiteralIndex literal_index : propagated_literals) {
      const Literal propagated_lit(literal_index);
      const absl::Span<ClausePtr> clauses = propagation_clauses[i++];
      for (int j = 0; j < dnf.size(); ++j) {
        const ClausePtr clause = clauses[j];
        if (clause == kNullClausePtr) continue;
        lrat_proof_handler_->DeleteClause(clause);
      }
    }
  });

  // Redo the loop that was done in ProbeDnf() to compute the LRAT proofs of the
  // propagated literals. This allows computing proofs only for those literals
  // (on the other hand, we need to redo the propagations). Another method might
  // be to make copies of the trail (one per conjunction), but how to handle
  // backjump on conflict in this case?.
  for (int conjunction_index = 0; conjunction_index < dnf.size();
       ++conjunction_index) {
    absl::Span<const Literal> conjunction = dnf[conjunction_index];
    // TODO(user): same comment as in ProbeDnf().
    if (!sat_solver_->ResetToLevelZero()) return false;

    // Enqueue the literals of `conjunction` one by one.
    // The first literal of `conjunction` which is propagated to false, if any,
    // and a temporary LRAT clause proving that the previous literals of
    // `conjunction` imply this.
    LiteralIndex first_false_literal = kNoLiteralIndex;
    ClausePtr first_false_literal_clause = kNullClausePtr;
    auto cleanup = absl::MakeCleanup([&] {
      if (first_false_literal_clause != kNullClausePtr) {
        lrat_proof_handler_->DeleteClause(first_false_literal_clause);
      }
    });

    tmp_literals_.clear();
    for (const Literal lit : conjunction) {
      tmp_literals_.push_back(lit.Negated());
      if (assignment_.LiteralIsAssigned(lit)) {
        if (assignment_.LiteralIsTrue(lit)) continue;
        first_false_literal = lit.Index();
        first_false_literal_clause = ClausePtr(tmp_literals_);
        tmp_proof_.clear();
        clause_manager_->AppendClausesFixing({lit.Negated()}, &tmp_proof_);
        lrat_proof_handler_->AddInferredClause(first_false_literal_clause,
                                               tmp_proof_);
        break;
      }

      // If enqueuing `lit` causes a conflict, the previous literals of
      // `conjunction` imply not(lit). Use the learned conflict to prove that.
      auto conflict_callback = [&](ClausePtr conflict,
                                   absl::Span<const Literal> conflict_clause) {
        if (first_false_literal != kNoLiteralIndex) return;
        first_false_literal = lit.Index();
        first_false_literal_clause = ClausePtr(tmp_literals_);
        tmp_proof_.clear();
        clause_manager_->AppendClausesFixing(conflict_clause, &tmp_proof_);
        tmp_proof_.push_back(conflict);
        lrat_proof_handler_->AddInferredClause(first_false_literal_clause,
                                               tmp_proof_);
      };
      sat_solver_->EnqueueDecisionAndBackjumpOnConflict(lit, conflict_callback);

      if (sat_solver_->ModelIsUnsat()) return false;
      if (first_false_literal != kNoLiteralIndex) break;
    }

    // Use the trail to compute the LRAT proofs that `conjunction` implies the
    // propagated literals.
    int i = 0;
    for (const LiteralIndex literal_index : propagated_literals) {
      const Literal propagated_lit(literal_index);
      absl::Span<ClausePtr> propagations = propagation_clauses[i++];
      // Create the clause "conjunction => propagated_lit".
      if (!GetConjunctionImpliesLiteralClause(conjunction, propagated_lit,
                                              tmp_literals_)) {
        // The clause is a tautology.
        continue;
      }
      // Compute its proof.
      tmp_proof_.clear();
      if (first_false_literal_clause != kNullClausePtr) {
        // If some literals of `conjunction` imply that another one is false,
        // the corresponding LRAT clause is sufficient to prove that
        // `conjunction` is false and thus that "conjunction => propagated_lit".
        tmp_proof_.push_back(first_false_literal_clause);
      } else {
        // TODO(user): processing the propagated literals in trail order
        // and reusing the previous proofs to compute new ones
        // could reduce the algorithmic complexity here.
        clause_manager_->AppendClausesFixing({propagated_lit}, &tmp_proof_);
      }
      // Add the inferred clause to the LratProofHandler.
      const ClausePtr clause = ClausePtr(tmp_literals_);
      lrat_proof_handler_->AddInferredClause(clause, tmp_proof_);
      propagations[conjunction_index] = clause;
    }
  }

  if (!sat_solver_->ResetToLevelZero()) return false;

  // Fix literals implied by the dnf.
  int i = 0;
  for (const LiteralIndex literal_index : propagated_literals) {
    const Literal propagated_lit(literal_index);
    absl::Span<ClausePtr> propagations = propagation_clauses[i++];
    if (assignment_.LiteralIsTrue(propagated_lit)) continue;

    ++num_new_literals_fixed_;
    switch (dnf_type) {
      case DnfType::kAtLeastOne:
        // `propagations` contains the clauses "not(l_i) OR propagated_lit"
        // for each literal l_i of the dnf. Together with the unit clauses for
        // the already assigned literals of the original clause, and the clause
        // itself, they prove that propagated_lit is true.
        CHECK_NE(dnf_clause, nullptr);
        tmp_proof_.clear();
        for (const ClausePtr clause : propagations) {
          if (clause == kNullClausePtr) continue;
          tmp_proof_.push_back(clause);
        }
        for (const Literal lit : dnf_clause->AsSpan()) {
          if (assignment_.LiteralIsAssigned(lit)) {
            tmp_proof_.push_back(ClausePtr(
                assignment_.GetTrueLiteralForAssignedVariable(lit.Variable())));
          }
        }
        tmp_proof_.push_back(ClausePtr(dnf_clause));
        if (!clause_manager_->InprocessingFixLiteral(propagated_lit,
                                                     tmp_proof_)) {
          return false;
        }
        break;
      case DnfType::kAtLeastOneOrZero:
        // `propagations` contains the clauses "not(l_i) OR propagated_lit"
        // (for each single literal conjunction), and "l1 OR ... OR ln OR
        // propagated_lit", in this order. These are sufficient to prove that
        // propagated_lit is true.
        tmp_proof_.clear();
        for (const ClausePtr clause : propagations) {
          if (clause == kNullClausePtr) continue;
          tmp_proof_.push_back(clause);
        }
        if (!clause_manager_->InprocessingFixLiteral(propagated_lit,
                                                     tmp_proof_)) {
          return false;
        }
        break;
      case DnfType::kAtLeastOneCombination:
        if (!FixLiteralImpliedByAnAtLeastOneCombinationDnf(dnf, propagations,
                                                           propagated_lit)) {
          return false;
        }
        break;
    }
  }
  return true;
}

bool Prober::FixLiteralImpliedByAnAtLeastOneCombinationDnf(
    absl::Span<const std::vector<Literal>> conjunctions,
    absl::Span<ClausePtr> clauses, Literal propagated_lit) {
  const int num_clauses = clauses.size();
  CHECK_EQ(conjunctions.size(), num_clauses);
  // Combine the clauses 2 by 2 repeatedly, to remove one literal from each
  // conjunction at each step, until we get the unit clause `propagated_lit`.
  // For instance, with 4 conjunctions:
  //
  //                          step1                 step2
  // not(a) and not(b) => p   ---->   not(a) => p   ---->   p
  // not(a) and     b  => p   _/                      /
  //     a  and not(b) => p   ---->       a  => p   _/
  //     a  and     b  => p   _/
  //
  // The combined clauses are stored in `clauses`, and replace the ones of
  // the previous step, which are deleted. At step i=0,1,..., each conjunction
  // has n-i remaining literals, and we combine the clauses at indices
  // (2*stride)k and (2*stride)k + stride, where stride = 2^i. This relies on
  // the conjunctions being sorted as described in kAtLeastOneCombination's
  // comment.
  int num_literals_per_conjunction = conjunctions[0].size();
  int stride = 1;
  while (true) {
    for (int i = 0; i < num_clauses; i += 2 * stride) {
      // The two clauses "... AND not(b) => propagated_lit" and "... AND b =>
      // propagated_lit" prove that "... => propagated_lit".
      tmp_proof_.clear();
      // Tautologies have no clause pointer.
      if (clauses[i] != kNullClausePtr) {
        tmp_proof_.push_back(clauses[i]);
      }
      if (clauses[i + stride] != kNullClausePtr) {
        tmp_proof_.push_back(clauses[i + stride]);
      }
      if (tmp_proof_.empty()) continue;
      GetConjunctionImpliesLiteralClause(
          absl::MakeConstSpan(conjunctions[i])
              .subspan(0, num_literals_per_conjunction - 1),
          propagated_lit, tmp_literals_);
      const ClausePtr new_clause = num_literals_per_conjunction == 1
                                       ? ClausePtr(propagated_lit)
                                       : ClausePtr(tmp_literals_);
      lrat_proof_handler_->AddInferredClause(new_clause, tmp_proof_);
      // Delete the clauses used to derive the new one.
      for (const int index : {i, i + stride}) {
        if (clauses[index] == kNullClausePtr) continue;
        lrat_proof_handler_->DeleteClause(clauses[index]);
        clauses[index] = kNullClausePtr;
      }
      if (num_literals_per_conjunction == 1) {
        return clause_manager_->InprocessingAddUnitClause(propagated_lit);
      }
      clauses[i] = new_clause;
    }
    num_literals_per_conjunction--;
    stride *= 2;
  }
}

bool LookForTrivialSatSolution(double deterministic_time_limit, Model* model,
                               SolverLogger* logger) {
  WallTimer wall_timer;
  wall_timer.Start();

  // Hack to not have empty logger.
  if (logger == nullptr) logger = model->GetOrCreate<SolverLogger>();

  // Reset the solver in case it was already used.
  auto* sat_solver = model->GetOrCreate<SatSolver>();
  if (!sat_solver->ResetToLevelZero()) return false;

  auto* time_limit = model->GetOrCreate<TimeLimit>();
  const int initial_num_fixed = sat_solver->LiteralTrail().Index();

  // Note that this code do not care about the non-Boolean part and just try to
  // assign the existing Booleans.
  SatParameters initial_params = *model->GetOrCreate<SatParameters>();
  SatParameters new_params = initial_params;
  new_params.set_log_search_progress(false);
  new_params.set_max_number_of_conflicts(1);
  new_params.set_max_deterministic_time(deterministic_time_limit);

  double elapsed_dtime = 0.0;

  // We need to keep a copy of the time limit to restore it later since we will
  // reset it by calling Model::SetParameters().
  TimeLimit original_time_limit;
  original_time_limit.MergeWithGlobalTimeLimit(model->GetOrCreate<TimeLimit>());

  const int num_times = 1000;
  bool limit_reached = false;
  auto* random = model->GetOrCreate<ModelRandomGenerator>();
  for (int i = 0; i < num_times; ++i) {
    if (time_limit->LimitReached() ||
        elapsed_dtime > deterministic_time_limit) {
      limit_reached = true;
      break;
    }

    // SetParameters() reset the deterministic time to zero inside time_limit.
    sat_solver->SetParameters(new_params);
    time_limit->MergeWithGlobalTimeLimit(&original_time_limit);
    sat_solver->ResetDecisionHeuristic();
    const SatSolver::Status result = sat_solver->SolveWithTimeLimit(time_limit);
    elapsed_dtime += time_limit->GetElapsedDeterministicTime();

    if (result == SatSolver::FEASIBLE) {
      SOLVER_LOG(logger, "Trivial exploration found feasible solution!");
      time_limit->AdvanceDeterministicTime(elapsed_dtime);
      return true;
    }

    if (!sat_solver->ResetToLevelZero()) {
      SOLVER_LOG(logger, "UNSAT during trivial exploration heuristic.");
      time_limit->AdvanceDeterministicTime(elapsed_dtime);
      return false;
    }

    // We randomize at the end so that the default params is executed
    // at least once.
    RandomizeDecisionHeuristic(*random, &new_params);
    new_params.set_random_seed(i);
    new_params.set_max_deterministic_time(deterministic_time_limit -
                                          elapsed_dtime);
  }

  // Restore the initial parameters.
  sat_solver->SetParameters(initial_params);
  sat_solver->ResetDecisionHeuristic();
  time_limit->MergeWithGlobalTimeLimit(&original_time_limit);
  time_limit->AdvanceDeterministicTime(elapsed_dtime);
  if (!sat_solver->ResetToLevelZero()) return false;

  if (logger->LoggingIsEnabled()) {
    const int num_fixed = sat_solver->LiteralTrail().Index();
    const int num_newly_fixed = num_fixed - initial_num_fixed;
    const int num_variables = sat_solver->NumVariables();
    SOLVER_LOG(logger, "[Random exploration]", " num_fixed: +",
               FormatCounter(num_newly_fixed), " (", FormatCounter(num_fixed),
               "/", FormatCounter(num_variables), ")",
               " dtime: ", elapsed_dtime, "/", deterministic_time_limit,
               " wtime: ", wall_timer.Get(),
               (limit_reached ? " (Aborted)" : ""));
  }
  return sat_solver->FinishPropagation();
}

FailedLiteralProbing::FailedLiteralProbing(Model* model)
    : sat_solver_(model->GetOrCreate<SatSolver>()),
      implication_graph_(model->GetOrCreate<BinaryImplicationGraph>()),
      time_limit_(model->GetOrCreate<TimeLimit>()),
      trail_(*model->GetOrCreate<Trail>()),
      assignment_(trail_.Assignment()),
      clause_manager_(model->GetOrCreate<ClauseManager>()),
      lrat_proof_handler_(model->Mutable<LratProofHandler>()),
      binary_propagator_id_(implication_graph_->PropagatorId()),
      clause_propagator_id_(clause_manager_->PropagatorId()) {}

// TODO(user): This might be broken if backtrack() propagates and go further
// back. Investigate and fix any issue.
bool FailedLiteralProbing::DoOneRound(ProbingOptions options) {
  WallTimer wall_timer;
  wall_timer.Start();

  options.log_info |= VLOG_IS_ON(1);

  num_variables_ = sat_solver_->NumVariables();
  to_fix_.clear();
  num_probed_ = 0;
  num_explicit_fix_ = 0;
  num_conflicts_ = 0;
  num_new_binary_ = 0;
  num_subsumed_ = 0;
  num_lrat_clauses_ = 0;
  num_lrat_proof_clauses_ = 0;

  // Reset the solver in case it was already used.
  if (!sat_solver_->ResetToLevelZero()) return false;

  // When called from Inprocessing, the implication graph should already be a
  // DAG, so these two calls should return right away. But we do need them to
  // get the topological order if this is used in isolation.
  if (!implication_graph_->DetectEquivalences()) return false;
  if (!sat_solver_->FinishPropagation()) return false;

  const int initial_num_fixed = sat_solver_->LiteralTrail().Index();
  const double initial_deterministic_time =
      time_limit_->GetElapsedDeterministicTime();
  const double limit = initial_deterministic_time + options.deterministic_limit;

  processed_.ClearAndResize(LiteralIndex(2 * num_variables_));

  queue_.clear();
  starts_.clear();
  if (!options.use_queue) {
    starts_.resize(2 * num_variables_, 0);
  }

  // Depending on the options, we do not use the same order.
  // With tree look, it is better to start with "leaf" first since we try
  // to reuse propagation as much as possible. This is also interesting to
  // do when extracting binary clauses since we will need to propagate
  // everyone anyway, and this should result in less clauses that can be
  // removed later by transitive reduction.
  //
  // However, without tree-look and without the need to extract all binary
  // clauses, it is better to just probe the root of the binary implication
  // graph. This is exactly what happen when we probe using the topological
  // order.
  probing_order_ = implication_graph_->ReverseTopologicalOrder();
  if (!options.use_tree_look && !options.extract_binary_clauses) {
    std::reverse(probing_order_.begin(), probing_order_.end());
  }
  order_index_ = 0;

  // We only use this for the queue version.
  position_in_order_.clear();
  if (options.use_queue) {
    position_in_order_.assign(2 * num_variables_, -1);
    for (int i = 0; i < probing_order_.size(); ++i) {
      position_in_order_[probing_order_[i]] = i;
    }
  }

  binary_clause_extracted_.assign(trail_.Index(), false);
  DeleteTemporaryLratImplicationsStartingFrom(0);
  trail_implication_clauses_.resize(trail_.Index());
  auto cleanup = absl::MakeCleanup([this]() {
    if (lrat_proof_handler_ != nullptr) {
      lrat_proof_handler_->DeleteTemporaryBinaryClauses();
    }
    DeleteTemporaryLratImplicationsStartingFrom(0);
  });

  while (!time_limit_->LimitReached() &&
         time_limit_->GetElapsedDeterministicTime() <= limit) {
    // We only enqueue literal at level zero if we don't use "tree look".
    if (!options.use_tree_look) {
      if (!sat_solver_->BacktrackAndPropagateReimplications(0)) return false;
      DeleteTemporaryLratImplicationsAfterBacktrack();
    }

    // Probing works by taking a series of decisions, and by analyzing what
    // they propagate. For efficiency, we only take a new decision d' if it
    // directly implies the last one d. By doing this we know that d' directly
    // or indirectly implies all the previous decisions, which then propagate
    // all the literals on the trail up to and excluding d'. The first step is
    // to find the next_decision d', which can be done in different ways
    // depending on the options.
    LiteralIndex next_decision = kNoLiteralIndex;
    if (sat_solver_->CurrentDecisionLevel() > 0 && options.use_queue) {
      if (!ComputeNextDecisionInOrder(next_decision)) return false;
    }
    if (sat_solver_->CurrentDecisionLevel() > 0 &&
        next_decision == kNoLiteralIndex) {
      if (!GetNextDecisionInNoParticularOrder(next_decision)) return false;
      if (next_decision == kNoLiteralIndex) continue;
    }
    if (sat_solver_->CurrentDecisionLevel() == 0) {
      if (!GetFirstDecision(next_decision)) return false;
      // The pass is finished.
      if (next_decision == kNoLiteralIndex) break;
    }

    // We now have a next decision, enqueue it and propagate until fix point.
    int first_new_trail_index;
    if (!EnqueueDecisionAndBackjumpOnConflict(next_decision, options.use_queue,
                                              first_new_trail_index)) {
      return false;
    }

    // Inspect the newly propagated literals. Depending on the options, try to
    // extract binary clauses via hyper binary resolution and/or mark the
    // literals on the trail so that they do not need to be probed later.
    const int new_level = sat_solver_->CurrentDecisionLevel();
    if (new_level == 0) continue;
    const Literal last_decision = trail_.Decisions()[new_level - 1].literal;
    binary_clauses_to_extract_.clear();
    for (int i = first_new_trail_index; i < trail_.Index(); ++i) {
      const Literal l = trail_[i];
      if (l == last_decision) continue;

      // This part of the trail is new, so we didn't extract any binary clause
      // about it yet.
      DCHECK(!binary_clause_extracted_[i]);

      if (options.subsume_with_binary_clause) {
        MaybeSubsumeWithBinaryClause(last_decision, l);
      }
      if (options.extract_binary_clauses) {
        // Note that we don't want to extract binary clauses that are already
        // present in some form in the binary implication graph. Thus we skip
        // anything that was propagated by it.
        //
        // Because the BinaryImplicationGraph has the highest priority of all
        // propagators, this should cover most of the cases.
        //
        // Note(user): This is not 100% true, since when we launch the clause
        // propagation for one literal we do finish it before calling again
        // the binary propagation.
        if (trail_.AssignmentType(l.Variable()) != binary_propagator_id_) {
          if (ShouldExtractImplication(l)) {
            binary_clauses_to_extract_.push_back(l);
          }
        }
      } else {
        // If we don't extract binary, we don't need to explore any of
        // these literals until more variables are fixed.
        processed_.Set(l.Index());
      }
    }
    if (!binary_clauses_to_extract_.empty()) {
      ExtractImplications(last_decision, binary_clauses_to_extract_);
    }

    if (options.subsume_with_binary_clause) {
      SubsumeWithBinaryClauseUsingBlockingLiteral(last_decision);
    }
  }

  if (!sat_solver_->ResetToLevelZero()) return false;
  if (!ProcessLiteralsToFix()) return false;
  DeleteTemporaryLratImplicationsAfterBacktrack();
  clause_manager_->CleanUpWatchers();

  // Display stats.
  const int num_fixed = sat_solver_->LiteralTrail().Index();
  const int num_newly_fixed = num_fixed - initial_num_fixed;
  const double time_diff =
      time_limit_->GetElapsedDeterministicTime() - initial_deterministic_time;
  const bool limit_reached = time_limit_->LimitReached() ||
                             time_limit_->GetElapsedDeterministicTime() > limit;
  LOG_IF(INFO, options.log_info)
      << "Probing. "
      << " num_probed: " << num_probed_ << "/" << probing_order_.size()
      << " num_fixed: +" << num_newly_fixed << " (" << num_fixed << "/"
      << num_variables_ << ")"
      << " explicit_fix:" << num_explicit_fix_
      << " num_conflicts:" << num_conflicts_
      << " new_binary_clauses: " << num_new_binary_
      << " num_lrat_clauses: " << num_lrat_clauses_
      << " num_lrat_proof_clauses: " << num_lrat_proof_clauses_
      << " subsumed: " << num_subsumed_ << " dtime: " << time_diff
      << " wtime: " << wall_timer.Get() << (limit_reached ? " (Aborted)" : "");
  return sat_solver_->FinishPropagation();
}

bool FailedLiteralProbing::SkipCandidate(Literal last_decision,
                                         Literal candidate) {
  if (processed_[candidate]) return true;
  if (implication_graph_->IsRedundant(candidate)) return true;
  if (assignment_.LiteralIsAssigned(candidate)) {
    // candidate => last_decision => all previous decisions, which then
    // propagate not(candidate). Hence candidate must be false.
    if (assignment_.LiteralIsFalse(candidate)) {
      AddFailedLiteralToFix(candidate);
    } else {
      // Here we have candidate => last_decision => candidate.
      // So we have an equivalence.
      //
      // If we extract all binary clauses, this shall be seen by
      // implication_graph_.DetectEquivalences(), but if not, we want to extract
      // the last_decision => candidate implication so it can be found.
      MaybeExtractImplication(last_decision, candidate);
    }
    return true;
  }
  return false;
}

// Sets `next_decision` to the unassigned literal which implies the last
// decision and which comes first in the probing order (which itself can be
// the topological order of the implication graph, or the reverse).
bool FailedLiteralProbing::ComputeNextDecisionInOrder(
    LiteralIndex& next_decision) {
  // TODO(user): Instead of minimizing index in topo order (which might be
  // nice for binary extraction), we could try to maximize reusability in
  // some way.
  const Literal last_decision =
      trail_.Decisions()[sat_solver_->CurrentDecisionLevel() - 1].literal;
  // If l => last_decision, then not(last_decision) => not(l). We can thus
  // find the candidates for the next decision by looking at all the
  // implications of not(last_decision).
  const absl::Span<const Literal> list =
      implication_graph_->Implications(last_decision.Negated());
  const int saved_queue_size = queue_.size();
  for (const Literal l : list) {
    const Literal candidate = l.Negated();
    if (position_in_order_[candidate] == -1) continue;
    if (SkipCandidate(last_decision, candidate)) continue;
    queue_.push_back({candidate.Index(), -position_in_order_[candidate]});
  }
  // Sort all the candidates.
  std::sort(queue_.begin() + saved_queue_size, queue_.end());

  // Set next_decision to the first unassigned candidate.
  while (!queue_.empty()) {
    const LiteralIndex index = queue_.back().literal_index;
    queue_.pop_back();
    if (index == kNoLiteralIndex) {
      // This is a backtrack marker, go back one level.
      CHECK_GT(sat_solver_->CurrentDecisionLevel(), 0);
      if (!sat_solver_->BacktrackAndPropagateReimplications(
              sat_solver_->CurrentDecisionLevel() - 1)) {
        return false;
      }
      DeleteTemporaryLratImplicationsAfterBacktrack();
      continue;
    }
    const Literal candidate(index);
    if (SkipCandidate(last_decision, candidate)) continue;
    next_decision = candidate.Index();
    break;
  }
  return true;
}

// Sets `next_decision` to the first unassigned literal we find which implies
// the last decision, in no particular order.
bool FailedLiteralProbing::GetNextDecisionInNoParticularOrder(
    LiteralIndex& next_decision) {
  const int level = sat_solver_->CurrentDecisionLevel();
  const Literal last_decision = trail_.Decisions()[level - 1].literal;
  const absl::Span<const Literal> list =
      implication_graph_->Implications(last_decision.Negated());

  // If l => last_decision, then not(last_decision) => not(l). We can thus
  // find the candidates for the next decision by looking at all the
  // implications of not(last_decision).
  int j = starts_[last_decision.NegatedIndex()];
  for (int i = 0; i < list.size(); ++i, ++j) {
    j %= list.size();
    const Literal candidate = Literal(list[j]).Negated();
    if (SkipCandidate(last_decision, candidate)) continue;
    next_decision = candidate.Index();
    break;
  }
  starts_[last_decision.NegatedIndex()] = j;
  if (next_decision == kNoLiteralIndex) {
    if (!sat_solver_->BacktrackAndPropagateReimplications(level - 1)) {
      return false;
    }
    DeleteTemporaryLratImplicationsAfterBacktrack();
  }
  return true;
}

// Sets `next_decision` to the first unassigned literal in probing_order (if
// there is no last decision we can use any literal as first decision).
bool FailedLiteralProbing::GetFirstDecision(LiteralIndex& next_decision) {
  // Fix any delayed fixed literal.
  if (!ProcessLiteralsToFix()) return false;

  // Probe an unexplored node.
  for (; order_index_ < probing_order_.size(); ++order_index_) {
    const Literal candidate(probing_order_[order_index_]);

    // Note that here this is a bit different than SkipCandidate().
    if (processed_[candidate]) continue;
    if (assignment_.LiteralIsAssigned(candidate)) continue;
    next_decision = candidate.Index();
    break;
  }
  return true;
}

bool FailedLiteralProbing::EnqueueDecisionAndBackjumpOnConflict(
    LiteralIndex next_decision, bool use_queue, int& first_new_trail_index) {
  ++num_probed_;
  processed_.Set(next_decision);
  CHECK_NE(next_decision, kNoLiteralIndex);
  queue_.push_back({kNoLiteralIndex, 0});  // Backtrack marker.
  const int level = sat_solver_->CurrentDecisionLevel();

  // The unit clause that fixes next_decision to false, if it causes a conflict.
  ClausePtr fixed_decision_clause = kNullClausePtr;
  auto conflict_callback = [&](ClausePtr conflict,
                               absl::Span<const Literal> conflict_clause) {
    DeleteTemporaryLratImplicationsAfterBacktrack();
    if (fixed_decision_clause != kNullClausePtr) return;
    fixed_decision_clause = ClausePtr(Literal(next_decision).Negated());
    if (fixed_decision_clause == conflict) return;
    tmp_proof_.clear();
    clause_manager_->AppendClausesFixing(conflict_clause, &tmp_proof_,
                                         next_decision);
    tmp_proof_.push_back(conflict);
    lrat_proof_handler_->AddInferredClause(fixed_decision_clause, tmp_proof_);
    num_lrat_clauses_++;
    num_lrat_proof_clauses_ += tmp_proof_.size();
  };
  first_new_trail_index = sat_solver_->EnqueueDecisionAndBackjumpOnConflict(
      Literal(next_decision),
      lrat_proof_handler_ != nullptr
          ? conflict_callback
          : std::optional<SatSolver::ConflictCallback>());

  if (first_new_trail_index == kUnsatTrailIndex) return false;
  binary_clause_extracted_.resize(first_new_trail_index);
  binary_clause_extracted_.resize(trail_.Index(), false);
  DeleteTemporaryLratImplicationsStartingFrom(first_new_trail_index);
  trail_implication_clauses_.resize(trail_.Index());

  // This is tricky, depending on the parameters, and for integer problem,
  // EnqueueDecisionAndBackjumpOnConflict() might create new Booleans.
  if (sat_solver_->NumVariables() > num_variables_) {
    num_variables_ = sat_solver_->NumVariables();
    processed_.Resize(LiteralIndex(2 * num_variables_));
    if (!use_queue) {
      starts_.resize(2 * num_variables_, 0);
    } else {
      position_in_order_.resize(2 * num_variables_, -1);
    }
  }

  const int new_level = sat_solver_->CurrentDecisionLevel();
  sat_solver_->AdvanceDeterministicTime(time_limit_);
  if (sat_solver_->ModelIsUnsat()) return false;
  if (new_level <= level) {
    ++num_conflicts_;

    // Sync the queue with the new level.
    if (use_queue) {
      if (new_level == 0) {
        queue_.clear();
      } else {
        int queue_level = level + 1;
        while (queue_level > new_level) {
          CHECK(!queue_.empty());
          if (queue_.back().literal_index == kNoLiteralIndex) --queue_level;
          queue_.pop_back();
        }
      }
    }

    // Fix `next_decision` to `false` if not already done.
    //
    // Even if we fixed something at level zero, next_decision might not be
    // fixed! But we can fix it. It can happen because when we propagate
    // with clauses, we might have `a => b` but not `not(b) => not(a)`. Like
    // `a => b` and clause `(not(a), not(b), c)`, propagating `a` will set
    // `c`, but propagating `not(c)` will not do anything.
    //
    // We "delay" the fixing if we are not at level zero so that we can
    // still reuse the current propagation work via tree look.
    //
    // TODO(user): Can we be smarter here? Maybe we can still fix the
    // literal without going back to level zero by simply enqueuing it with
    // no reason? it will be backtracked over, but we will still lazily fix
    // it later.
    if (sat_solver_->CurrentDecisionLevel() != 0 ||
        !assignment_.LiteralIsFalse(Literal(next_decision))) {
      to_fix_.push_back(Literal(next_decision).Negated());
    }
  }
  return true;
}

bool FailedLiteralProbing::ShouldExtractImplication(const Literal l) {
  const auto& info = trail_.Info(l.Variable());

  if (binary_clause_extracted_[info.trail_index]) return false;
  binary_clause_extracted_[info.trail_index] = true;

  // If the variable was true at level zero, this is not necessary.
  return info.level > 0;
}

void FailedLiteralProbing::ExtractImplication(const Literal last_decision,
                                              const Literal l, bool lrat_only) {
  const auto& info = trail_.Info(l.Variable());
  if (lrat_only && info.level != sat_solver_->CurrentDecisionLevel()) return;

  // TODO(user): Think about trying to extract clause that will not
  // get removed by transitive reduction later. If we can both extract
  // a => c and b => c , ideally we don't want to extract a => c first
  // if we already know that a => b.
  //
  // TODO(user): Similar to previous point, we could find the LCA
  // of all literals in the reason for this propagation. And use this
  // as a reason for later hyber binary resolution. Like we do when
  // this clause subsumes the reason.
  DCHECK(assignment_.LiteralIsTrue(l));
  CHECK_NE(l.Variable(), last_decision.Variable());

  // We should never probe a redundant literal.
  //
  // TODO(user): We should be able to enforce that l is non-redundant either if
  // we made sure the clause database is cleaned up before FailedLiteralProbing
  // is called. This should maybe simplify the ChangeReason() handling.
  DCHECK(!implication_graph_->IsRedundant(last_decision));

  if (lrat_proof_handler_ != nullptr) {
    if (trail_implication_clauses_[info.trail_index] == kNullClausePtr) {
      // Temporary binary clauses only must have a distinct pointer from
      // permanent ones, otherwise deleting them in
      // DeleteTemporaryLratImplicationsAfterBacktrack() could delete
      // "permanent" ones which are identical to temporary ones.
      const ClausePtr clause =
          lrat_only ? ClausePtr(SatClause::Create({last_decision.Negated(), l}))
                    : ClausePtr(last_decision.Negated(), l);
      tmp_proof_.clear();
      clause_manager_->AppendClausesFixing(
          {l}, &tmp_proof_, last_decision, [&](int /*level*/, int trail_index) {
            return trail_implication_clauses_[trail_index];
          });
      // Cache this LRAT clause so that it can be reused for later proofs. Do
      // this only if `l` is propagated by the last decision, so that this cache
      // entry remains valid when we backtrack later decisions.
      if (info.level == sat_solver_->CurrentDecisionLevel()) {
        trail_implication_clauses_[info.trail_index] = clause;
      }
      lrat_proof_handler_->AddInferredClause(clause, tmp_proof_);
      num_lrat_clauses_++;
      num_lrat_proof_clauses_ += tmp_proof_.size();
    } else {
      const ClausePtr clause = trail_implication_clauses_[info.trail_index];
      // If we have a temporary clause but need a permanent one, convert it. In
      // the opposite case there is nothing to do.
      if (!lrat_only && clause.IsSatClausePtr()) {
        const ClausePtr new_clause = ClausePtr(last_decision.Negated(), l);
        lrat_proof_handler_->AddInferredClause(new_clause, {clause});
        lrat_proof_handler_->DeleteClause(clause);
        if (info.level == sat_solver_->CurrentDecisionLevel()) {
          trail_implication_clauses_[info.trail_index] = new_clause;
        }
        num_lrat_clauses_++;
        num_lrat_proof_clauses_++;
      } else {
      }
    }
  }
  if (lrat_only) return;

  // Each time we extract a binary clause, we change the reason in the trail.
  // This is important as it will allow us to remove clauses that are now
  // subsumed by this binary, even if it was a reason.
  ++num_new_binary_;
  CHECK(implication_graph_->AddBinaryClauseAndChangeReason(
      l, last_decision.Negated()));
}

void FailedLiteralProbing::MaybeExtractImplication(const Literal last_decision,
                                                   const Literal l) {
  if (ShouldExtractImplication(l)) {
    ExtractImplication(last_decision, l);
  }
}

void FailedLiteralProbing::ExtractImplications(
    Literal last_decision, absl::Span<const Literal> literals) {
  // 1. Find all the literals which appear in the expansion of the reasons of
  //    all the `literals` and collect them in reverse trail order in
  //    `tmp_marked_literals_`.
  // 1.a Put the `literals` in a heap.
  tmp_mark_.ClearAndResize(BooleanVariable(trail_.NumVariables()));
  tmp_heap_.clear();
  const VariablesAssignment& assignment = trail_.Assignment();
  for (const Literal lit : literals) {
    CHECK(assignment.LiteralIsAssigned(lit));
    tmp_mark_.Set(lit.Variable());
    tmp_heap_.push_back(trail_.Info(lit.Variable()).trail_index);
  }
  absl::c_make_heap(tmp_heap_);

  // 1.b Expand the reasons of all the literals in the heap and add the reason
  //     literals back in the heap. Collect the literals in the order they are
  //     popped from the heap in `tmp_marked_literals_`.
  tmp_marked_literals_.clear();
  while (!tmp_heap_.empty()) {
    absl::c_pop_heap(tmp_heap_);
    const int trail_index = tmp_heap_.back();
    tmp_heap_.pop_back();
    const Literal marked_literal = trail_[trail_index];
    tmp_marked_literals_.push_back(marked_literal);

    DCHECK_GT(trail_.Info(marked_literal.Variable()).level, 0);
    DCHECK_NE(trail_.AssignmentType(marked_literal.Variable()),
              AssignmentType::kSearchDecision);

    for (const Literal literal : trail_.Reason(marked_literal.Variable())) {
      const BooleanVariable var = literal.Variable();
      const AssignmentInfo& info = trail_.Info(var);
      if (info.level > 0 && !tmp_mark_[var] &&
          trail_.AssignmentType(var) != AssignmentType::kSearchDecision) {
        tmp_mark_.Set(var);
        tmp_heap_.push_back(info.trail_index);
        absl::c_push_heap(tmp_heap_);
      }
    }
  }

  // 2. Add an LRAT implication "last_decision => l" for each literal l in
  //    `tmp_marked_literals_`, in increasing trail index order. Thanks to the
  //    cache in ExtractImplication(), the proof for each new implication has
  //    the same size as its reason. Also add a "real" implication in the binary
  //    implication graph if `l` is in `literals`.
  tmp_mark_.ClearAndResize(BooleanVariable(trail_.NumVariables()));
  for (const Literal lit : literals) {
    tmp_mark_.Set(lit.Variable());
  }
  for (int i = tmp_marked_literals_.size() - 1; i >= 0; --i) {
    const bool lrat_only = !tmp_mark_[tmp_marked_literals_[i].Variable()];
    ExtractImplication(last_decision, tmp_marked_literals_[i], lrat_only);
  }
}

// If we can extract a binary clause that subsume the reason clause, we do add
// the binary and remove the subsumed clause.
//
// TODO(user): We could be slightly more generic and subsume some clauses that
// do not contain last_decision.Negated().
void FailedLiteralProbing::MaybeSubsumeWithBinaryClause(
    const Literal last_decision, const Literal l) {
  const int trail_index = trail_.Info(l.Variable()).trail_index;
  if (trail_.AssignmentType(l.Variable()) != clause_propagator_id_) return;
  SatClause* clause = clause_manager_->ReasonClause(trail_index);

  bool subsumed = false;
  for (const Literal lit : trail_.Reason(l.Variable())) {
    if (lit == last_decision.Negated()) {
      subsumed = true;
      break;
    }
  }
  if (!subsumed) {
    // The clause is not subsumed but its lbd is 2 when last_decision is
    // propagated. This is a "glue" clause.
    clause_manager_->ChangeLbdIfBetter(clause, 2);
    return;
  }

  // Since we will remove the clause, we need to make sure we do have the
  // implication in our repository.
  MaybeExtractImplication(last_decision, l);

  int test = 0;
  for (const Literal lit :
       clause_manager_->ReasonClause(trail_index)->AsSpan()) {
    if (lit == l) ++test;
    if (lit == last_decision.Negated()) ++test;
  }
  CHECK_EQ(test, 2);

  // Because of MaybeExtractImplication(), this shouldn't be a reason anymore.
  CHECK(!clause_manager_->ClauseIsUsedAsReason(clause));
  ++num_subsumed_;
  clause_manager_->LazyDelete(clause,
                              DeletionSourceForStat::SUBSUMPTION_PROBING);
}

// Inspect the watcher list for last_decision, If we have a blocking
// literal at true (implied by last decision), then we have subsumptions.
//
// The intuition behind this is that if a binary clause (a,b) subsumes a
// clause, and we watch a.Negated() for this clause with a blocking
// literal b, then this watch entry will never change because we always
// propagate binary clauses first and the blocking literal will always be
// true. So after many propagations, we hope to have such configuration
// which is quite cheap to test here.
void FailedLiteralProbing::SubsumeWithBinaryClauseUsingBlockingLiteral(
    const Literal last_decision) {
  for (const auto& w :
       clause_manager_->WatcherListOnFalse(last_decision.Negated())) {
    if (!assignment_.LiteralIsTrue(w.blocking_literal)) continue;
    if (w.clause->IsRemoved()) continue;

    // This should be enough for proof correctness.
    //
    // TODO(user): Always extract the binary otherwise we might loose structural
    // property, or do not subsume size 3 clauses.
    DCHECK_NE(last_decision, w.blocking_literal);
    if (clause_manager_->ClauseIsUsedAsReason(w.clause)) {
      MaybeExtractImplication(last_decision, w.blocking_literal);

      // It should have been replaced by a binary clause now.
      CHECK(!clause_manager_->ClauseIsUsedAsReason(w.clause));
    }

    ++num_subsumed_;
    clause_manager_->LazyDelete(w.clause,
                                DeletionSourceForStat::SUBSUMPTION_PROBING);
  }
}

// Adds 'not(literal)' to `to_fix_`, assuming that 'literal' directly implies
// the current decision, which itself implies all the previous decisions, with
// some of them propagating 'not(literal)'.
void FailedLiteralProbing::AddFailedLiteralToFix(const Literal literal) {
  if (trail_.AssignmentLevel(literal.Negated()) == 0) return;
  to_fix_.push_back(literal.Negated());
  if (lrat_proof_handler_ == nullptr) return;

  tmp_proof_.clear();
  clause_manager_->AppendClausesFixing({literal.Negated()}, &tmp_proof_,
                                       literal);
  lrat_proof_handler_->AddInferredClause(ClausePtr(literal.Negated()),
                                         tmp_proof_);
  num_lrat_clauses_++;
  num_lrat_proof_clauses_ += tmp_proof_.size();
}

// Fixes all the literals in to_fix_, and finish propagation.
bool FailedLiteralProbing::ProcessLiteralsToFix() {
  for (int i = 0; i < to_fix_.size(); ++i) {
    const Literal literal = to_fix_[i];
    if (assignment_.LiteralIsTrue(literal)) continue;
    ++num_explicit_fix_;
    if (!clause_manager_->InprocessingAddUnitClause(literal)) {
      return false;
    }
  }
  to_fix_.clear();
  return sat_solver_->FinishPropagation();
}

void FailedLiteralProbing::DeleteTemporaryLratImplicationsAfterBacktrack() {
  DeleteTemporaryLratImplicationsStartingFrom(trail_.Index());
}

void FailedLiteralProbing::DeleteTemporaryLratImplicationsStartingFrom(
    int trail_index) {
  if (lrat_proof_handler_ != nullptr) {
    for (int i = trail_index; i < trail_implication_clauses_.size(); ++i) {
      const ClausePtr clause = trail_implication_clauses_[i];
      if (clause.IsSatClausePtr()) {
        lrat_proof_handler_->DeleteClause(clause);
      }
    }
  }
  trail_implication_clauses_.resize(trail_index);
}

}  // namespace sat
}  // namespace operations_research
