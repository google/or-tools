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
#include <functional>
#include <optional>
#include <utility>
#include <vector>

#include "absl/cleanup/cleanup.h"
#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/inlined_vector.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/log/vlog_is_on.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
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
      clause_id_generator_(model->GetOrCreate<ClauseIdGenerator>()),
      lrat_proof_handler_(model->Mutable<LratProofHandler>()),
      drat_enabled_(lrat_proof_handler_ != nullptr &&
                    (lrat_proof_handler_->drat_check_enabled() ||
                     lrat_proof_handler_->drat_output_enabled())),
      logger_(model->GetOrCreate<SolverLogger>()) {}

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
  tmp_binary_clause_ids_.clear();
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
    for (int i = saved_index + 1; i < trail_.Index(); ++i) {
      const Literal l = trail_[i];

      // We mark on the first run (b.IsPositive()) and check on the second.
      if (decision.IsPositive()) {
        propagated_.Set(l.Index());
      } else {
        if (propagated_[l]) {
          to_fix_at_true_.push_back(l);
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
      }
      // TODO(user): it might be possible to generate less temporary LRAT
      // clauses by adding them in a third iteration instead of in the first
      // one, once we know if there are any literal to fix.
      // Otherwise, since we always add binary, the reason should be retrievable
      // from the binary implication graph alone. So we might just need a
      // MarkDescendant() + parent inspection for this.
      if (lrat_proof_handler_ != nullptr) {
        tmp_clause_ids_.clear();
        sat_solver_->AppendClausesFixing(
            {l}, &tmp_clause_ids_, decision.Index(), &tmp_binary_clause_ids_);
        const ClauseId clause_id = clause_id_generator_->GetNextId();
        lrat_proof_handler_->AddInferredClause(
            clause_id, {decision.Negated(), l}, tmp_clause_ids_);
        tmp_binary_clause_ids_[std::minmax(decision.Negated(), l)] = clause_id;
      }
    }

    // Fix variables and add new binary clauses.
    if (!sat_solver_->ResetToLevelZero()) return false;
    for (const Literal l : to_fix_at_true_) {
      ClauseId clause_id = kNoClauseId;
      if (lrat_proof_handler_ != nullptr) {
        clause_id = clause_id_generator_->GetNextId();
        absl::InlinedVector<ClauseId, 2> clause_ids;
        clause_ids.push_back(
            tmp_binary_clause_ids_.at(std::minmax(decision.Negated(), l)));
        if (l != decision.Negated()) {
          clause_ids.push_back(
              tmp_binary_clause_ids_.at(std::minmax(decision, l)));
        }
        lrat_proof_handler_->AddInferredClause(clause_id, {l}, clause_ids);
      }
      if (!clause_manager_->InprocessingAddUnitClause(clause_id, l)) {
        return false;
      }
    }
    if (!sat_solver_->FinishPropagation()) return false;
    for (const Literal l : new_literals_implied_by_decision_) {
      // Some variables can be fixed by the above loop.
      if (trail_.Assignment().LiteralIsAssigned(decision)) break;
      if (trail_.Assignment().LiteralIsAssigned(l)) continue;
      ClauseId clause_id = kNoClauseId;
      if (lrat_proof_handler_ != nullptr) {
        clause_id =
            tmp_binary_clause_ids_.at(std::minmax(decision.Negated(), l));
      }
      num_new_binary_++;
      // Tricky: by default AddBinaryClause() can delete the LRAT `clause_id`
      // and create a new ID for a similar clause between the representatives.
      // But `clause_id`, registered in tmp_binary_clause_ids_, can be needed in
      // the next iteration for the proof of a new fixed literal. Hence we must
      // not delete it here. Instead, it is deleted at the end of this method,
      // with the other non-longer needed clauses.
      // TODO(user): can we maintain a one to one correspondence of clauses
      // in LRAT and in the binary implication graph to avoid this?
      if (!implication_graph_->AddBinaryClause(
              clause_id, decision.Negated(), l,
              /*delete_non_representative_id=*/false)) {
        return false;
      }
    }
    if (!sat_solver_->FinishPropagation()) return false;
  }
  if (lrat_proof_handler_ != nullptr) {
    // Delete the temporary LRAT clauses that have not been added to the binary
    // implication graph.
    for (const auto& [binary_clause, clause_id] : tmp_binary_clause_ids_) {
      if (implication_graph_->GetClauseId(binary_clause.first,
                                          binary_clause.second) != clause_id) {
        lrat_proof_handler_->DeleteClause(
            clause_id, {binary_clause.first, binary_clause.second});
      }
    }
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
  }

  return true;
}

bool Prober::ProbeDnf(absl::string_view name,
                      absl::Span<const std::vector<Literal>> dnf,
                      DnfType dnf_type, const SatClause* dnf_clause) {
  if (dnf.size() <= 1) return true;

  // dnf_clause can be deleted as a side effect of probing, but is needed for
  // LRAT in FixProbedDnfLiterals(). We thus copy its literals first, and
  // prevent the corresponding LRAT clause from being deleted.
  ClauseId dnf_clause_id = kNoClauseId;
  std::vector<Literal> dnf_clause_literals;
  if (dnf_clause != nullptr && lrat_proof_handler_ != nullptr) {
    dnf_clause_id = clause_manager_->GetClauseId(dnf_clause);
    dnf_clause_literals.assign(dnf_clause->AsSpan().begin(),
                               dnf_clause->AsSpan().end());
    lrat_proof_handler_->PinClause(dnf_clause_id, dnf_clause_literals);
  }
  absl::Cleanup cleanup = [this, dnf_clause_id] {
    if (dnf_clause_id != kNoClauseId) {
      lrat_proof_handler_->UnpinClause(dnf_clause_id);
    }
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
                              dnf_clause_id, dnf_clause_literals)) {
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
    ClauseId dnf_clause_id, absl::Span<const Literal> dnf_clause_literals) {
  if (propagated_literals.empty()) return true;

  // For each propagated literal (in propagated_literals order), and for each
  // conjunction, the ID of a temporary LRAT clause "conjunction => propagated
  // literal" (or kNoClauseId if "conjunction" contains "propagated_literal", if
  // the clause has not been created yet, or has been deleted).
  CompactVectorVector<int, ClauseId>& propagation_clause_ids =
      tmp_dnf_clause_ids_;
  propagation_clause_ids.clear();
  propagation_clause_ids.reserve(propagated_literals.size() * dnf.size());
  for (int i = 0; i < propagated_literals.size(); ++i) {
    propagation_clause_ids.Add({});
    for (int j = 0; j < dnf.size(); ++j) {
      propagation_clause_ids.AppendToLastVector(kNoClauseId);
    }
  }
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
    // and the ID of a temporary LRAT clause proving that the previous literals
    // of `conjunction` imply this.
    LiteralIndex first_false_literal = kNoLiteralIndex;
    ClauseId first_false_literal_clause_id = kNoClauseId;
    tmp_literals_.clear();
    for (const Literal lit : conjunction) {
      tmp_literals_.push_back(lit.Negated());
      if (assignment_.LiteralIsAssigned(lit)) {
        if (assignment_.LiteralIsTrue(lit)) continue;
        first_false_literal = lit.Index();
        first_false_literal_clause_id = clause_id_generator_->GetNextId();
        tmp_clause_ids_.clear();
        sat_solver_->AppendClausesFixing({lit.Negated()}, &tmp_clause_ids_);
        lrat_proof_handler_->AddInferredClause(first_false_literal_clause_id,
                                               tmp_literals_, tmp_clause_ids_);
        break;
      }

      // If enqueuing `lit` causes a conflict, the previous literals of
      // `conjunction` imply not(lit). Use the learned conflict to prove that.
      auto conflict_callback = [&](ClauseId conflict_id,
                                   absl::Span<const Literal> conflict_clause) {
        if (first_false_literal != kNoLiteralIndex) return;
        first_false_literal = lit.Index();
        first_false_literal_clause_id = clause_id_generator_->GetNextId();
        tmp_clause_ids_.clear();
        sat_solver_->AppendClausesFixing(conflict_clause, &tmp_clause_ids_);
        tmp_clause_ids_.push_back(conflict_id);
        lrat_proof_handler_->AddInferredClause(first_false_literal_clause_id,
                                               tmp_literals_, tmp_clause_ids_);
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
      absl::Span<ClauseId> propagation_ids = propagation_clause_ids[i++];
      // Create the clause "conjunction => propagated_lit".
      if (!GetConjunctionImpliesLiteralClause(conjunction, propagated_lit,
                                              tmp_literals_)) {
        // The clause is a tautology.
        continue;
      }
      // Compute its proof.
      tmp_clause_ids_.clear();
      if (first_false_literal_clause_id != kNoClauseId) {
        // If some literals of `conjunction` imply that another one is false,
        // the corresponding LRAT clause is sufficient to prove that
        // `conjunction` is false and thus that "conjunction => propagated_lit".
        tmp_clause_ids_.push_back(first_false_literal_clause_id);
      } else {
        // TODO(user): processing the propagated literals in trail order
        // and reusing the previous proofs to compute new ones
        // could reduce the algorithmic complexity here.
        sat_solver_->AppendClausesFixing({propagated_lit}, &tmp_clause_ids_);
      }
      // Add the inferred clause to the LratProofHandler.
      const ClauseId clause_id = clause_id_generator_->GetNextId();
      lrat_proof_handler_->AddInferredClause(clause_id, tmp_literals_,
                                             tmp_clause_ids_);
      propagation_ids[conjunction_index] = clause_id;
    }
    if (first_false_literal_clause_id != kNoClauseId) {
      if (drat_enabled_) {
        // DRAT needs the clause literals to delete a clause.
        tmp_literals_.clear();
        for (const Literal lit : conjunction) {
          tmp_literals_.push_back(lit.Negated());
          if (lit.Index() == first_false_literal) break;
        }
        lrat_proof_handler_->DeleteClause(first_false_literal_clause_id,
                                          tmp_literals_);
      } else {
        lrat_proof_handler_->DeleteClause(first_false_literal_clause_id, {});
      }
    }
  }

  if (!sat_solver_->ResetToLevelZero()) return false;

  // Fix literals implied by the dnf.
  int i = 0;
  for (const LiteralIndex literal_index : propagated_literals) {
    const Literal propagated_lit(literal_index);
    absl::Span<ClauseId> propagation_ids = propagation_clause_ids[i++];
    if (assignment_.LiteralIsTrue(propagated_lit)) continue;

    ++num_new_literals_fixed_;
    switch (dnf_type) {
      case DnfType::kAtLeastOne:
        // `propagation_ids` contains the clauses "not(l_i) OR propagated_lit"
        // for each literal l_i of the dnf. Together with the unit clauses for
        // the already assigned literals of the original clause, and the clause
        // itself, they prove that propagated_lit is true.
        CHECK_NE(dnf_clause_id, kNoClauseId);
        tmp_clause_ids_.clear();
        for (const ClauseId clause_id : propagation_ids) {
          if (clause_id == kNoClauseId) continue;
          tmp_clause_ids_.push_back(clause_id);
        }
        for (const Literal lit : dnf_clause_literals) {
          if (assignment_.LiteralIsAssigned(lit)) {
            tmp_clause_ids_.push_back(trail_.GetUnitClauseId(lit.Variable()));
          }
        }
        tmp_clause_ids_.push_back(dnf_clause_id);
        if (!clause_manager_->InprocessingFixLiteral(propagated_lit,
                                                     tmp_clause_ids_)) {
          return false;
        }
        break;
      case DnfType::kAtLeastOneOrZero:
        // `propagation_ids` contains the clauses "not(l_i) OR propagated_lit"
        // (for each single literal conjunction), and "l1 OR ... OR ln OR
        // propagated_lit", in this order. These are sufficient to prove that
        // propagated_lit is true.
        tmp_clause_ids_.clear();
        for (const ClauseId clause_id : propagation_ids) {
          if (clause_id == kNoClauseId) continue;
          tmp_clause_ids_.push_back(clause_id);
        }
        if (!clause_manager_->InprocessingFixLiteral(propagated_lit,
                                                     tmp_clause_ids_)) {
          return false;
        }
        break;
      case DnfType::kAtLeastOneCombination:
        if (!FixLiteralImpliedByAnAtLeastOneCombinationDnf(dnf, propagation_ids,
                                                           propagated_lit)) {
          return false;
        }
        break;
    }
  }

  // Delete the temporary LRAT clauses.
  i = 0;
  for (const LiteralIndex literal_index : propagated_literals) {
    const Literal propagated_lit(literal_index);
    const absl::Span<ClauseId> propagation_ids = propagation_clause_ids[i++];
    for (int j = 0; j < dnf.size(); ++j) {
      const ClauseId clause_id = propagation_ids[j];
      if (clause_id == kNoClauseId) continue;
      if (drat_enabled_) {
        // DRAT needs the clause literals to delete a clause.
        GetConjunctionImpliesLiteralClause(dnf[j], propagated_lit,
                                           tmp_literals_);
        lrat_proof_handler_->DeleteClause(clause_id, tmp_literals_);
      } else {
        lrat_proof_handler_->DeleteClause(clause_id, {});
      }
    }
  }
  return true;
}

bool Prober::FixLiteralImpliedByAnAtLeastOneCombinationDnf(
    absl::Span<const std::vector<Literal>> conjunctions,
    absl::Span<ClauseId> clause_ids, Literal propagated_lit) {
  const int num_clauses = clause_ids.size();
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
  // The combined clauses are stored in `clause_ids`, and replace the ones of
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
      tmp_clause_ids_.clear();
      // Tautologies have no clause ID.
      if (clause_ids[i] != kNoClauseId) {
        tmp_clause_ids_.push_back(clause_ids[i]);
      }
      if (clause_ids[i + stride] != kNoClauseId) {
        tmp_clause_ids_.push_back(clause_ids[i + stride]);
      }
      if (tmp_clause_ids_.empty()) continue;
      const ClauseId new_clause_id = clause_id_generator_->GetNextId();
      GetConjunctionImpliesLiteralClause(
          absl::MakeConstSpan(conjunctions[i])
              .subspan(0, num_literals_per_conjunction - 1),
          propagated_lit, tmp_literals_);
      lrat_proof_handler_->AddInferredClause(new_clause_id, tmp_literals_,
                                             tmp_clause_ids_);
      // Delete the clauses used to derive the new one.
      for (const int index : {i, i + stride}) {
        if (clause_ids[index] == kNoClauseId) continue;
        if (drat_enabled_) {
          // DRAT needs the clause literals to delete a clause.
          GetConjunctionImpliesLiteralClause(
              absl::MakeConstSpan(conjunctions[index])
                  .subspan(0, num_literals_per_conjunction),
              propagated_lit, tmp_literals_);
          lrat_proof_handler_->DeleteClause(clause_ids[index], tmp_literals_);
        } else {
          lrat_proof_handler_->DeleteClause(clause_ids[index], {});
        }
        clause_ids[index] = kNoClauseId;
      }
      if (num_literals_per_conjunction == 1) {
        return clause_manager_->InprocessingAddUnitClause(new_clause_id,
                                                          propagated_lit);
      }
      clause_ids[i] = new_clause_id;
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
      clause_id_generator_(model->GetOrCreate<ClauseIdGenerator>()),
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
  to_fix_unit_id_.clear();
  num_probed_ = 0;
  num_explicit_fix_ = 0;
  num_conflicts_ = 0;
  num_new_binary_ = 0;
  num_subsumed_ = 0;

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

  while (!time_limit_->LimitReached() &&
         time_limit_->GetElapsedDeterministicTime() <= limit) {
    // We only enqueue literal at level zero if we don't use "tree look".
    if (!options.use_tree_look) {
      if (!sat_solver_->BacktrackAndPropagateReimplications(0)) return false;
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
    const Literal last_decision =
        sat_solver_->Decisions()[new_level - 1].literal;
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
          MaybeExtractImplication(last_decision, l);
        }
      } else {
        // If we don't extract binary, we don't need to explore any of
        // these literals until more variables are fixed.
        processed_.Set(l.Index());
      }
    }

    if (options.subsume_with_binary_clause) {
      SubsumeWithBinaryClauseUsingBlockingLiteral(last_decision);
    }
  }

  if (!sat_solver_->ResetToLevelZero()) return false;
  if (!ProcessLiteralsToFix()) return false;
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
      sat_solver_->Decisions()[sat_solver_->CurrentDecisionLevel() - 1].literal;
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
              sat_solver_->CurrentDecisionLevel() - 1))
        return false;
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
  const Literal last_decision = sat_solver_->Decisions()[level - 1].literal;
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

  // The unit clause ID that fixes next_decision to false, if it causes a
  // conflict.
  ClauseId fixed_decision_unit_id = kNoClauseId;
  auto conflict_callback = [&](ClauseId conflict_id,
                               absl::Span<const Literal> conflict_clause) {
    if (fixed_decision_unit_id != kNoClauseId) return;
    tmp_clause_ids_.clear();
    sat_solver_->AppendClausesFixing(conflict_clause, &tmp_clause_ids_,
                                     next_decision);
    tmp_clause_ids_.push_back(conflict_id);
    fixed_decision_unit_id = clause_id_generator_->GetNextId();
    lrat_proof_handler_->AddInferredClause(fixed_decision_unit_id,
                                           {Literal(next_decision).Negated()},
                                           tmp_clause_ids_);
  };
  first_new_trail_index = sat_solver_->EnqueueDecisionAndBackjumpOnConflict(
      Literal(next_decision),
      lrat_proof_handler_ != nullptr
          ? conflict_callback
          : std::optional<SatSolver::ConflictCallback>());

  if (first_new_trail_index == kUnsatTrailIndex) return false;
  binary_clause_extracted_.resize(first_new_trail_index);
  binary_clause_extracted_.resize(trail_.Index(), false);

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
        assignment_.LiteralIsFalse(Literal(next_decision))) {
      to_fix_.push_back(Literal(next_decision).Negated());
      if (lrat_proof_handler_ != nullptr) {
        to_fix_unit_id_.push_back(fixed_decision_unit_id);
      }
    }
  }
  return true;
}

void FailedLiteralProbing::MaybeExtractImplication(const Literal last_decision,
                                                   const Literal l) {
  const auto& info = trail_.Info(l.Variable());

  if (binary_clause_extracted_[info.trail_index]) return;
  binary_clause_extracted_[info.trail_index] = true;

  // If the variable was true at level zero, this is not necessary.
  if (info.level == 0) return;

  // TODO(user): Think about trying to extract clause that will not
  // get removed by transitive reduction later. If we can both extract
  // a => c and b => c , ideally we don't want to extract a => c first
  // if we already know that a => b.
  //
  // TODO(user): Similar to previous point, we could find the LCA
  // of all literals in the reason for this propagation. And use this
  // as a reason for later hyber binary resolution. Like we do when
  // this clause subsumes the reason.
  ++num_new_binary_;
  DCHECK(assignment_.LiteralIsTrue(l));
  CHECK_NE(l.Variable(), last_decision.Variable());

  // We should never probe a redundant literal.
  //
  // TODO(user): We should be able to enforce that l is non-redundant either if
  // we made sure the clause database is cleaned up before FailedLiteralProbing
  // is called. This should maybe simplify the ChangeReason() handling.
  DCHECK(!implication_graph_->IsRedundant(last_decision));

  ClauseId clause_id = kNoClauseId;
  if (lrat_proof_handler_ != nullptr) {
    clause_id = clause_id_generator_->GetNextId();
    tmp_clause_ids_.clear();
    sat_solver_->AppendClausesFixing({l}, &tmp_clause_ids_, last_decision);
    lrat_proof_handler_->AddInferredClause(
        clause_id, {last_decision.Negated(), l}, tmp_clause_ids_);
  }

  // Each time we extract a binary clause, we change the reason in the trail.
  // This is important as it will allow us to remove clauses that are now
  // subsumed by this binary, even if it was a reason.
  CHECK(implication_graph_->AddBinaryClauseAndChangeReason(
      clause_id, l, last_decision.Negated()));
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
  if (!subsumed) return;

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
  // TODO(user): skip if literal.Negated() is already in to_fix? This does
  // not seem to happen often enough to be a problem.
  if (trail_.AssignmentLevel(literal.Negated()) == 0) return;
  to_fix_.push_back(literal.Negated());
  if (lrat_proof_handler_ == nullptr) return;

  tmp_clause_ids_.clear();
  sat_solver_->AppendClausesFixing({literal.Negated()}, &tmp_clause_ids_,
                                   literal);
  const ClauseId unit_id = clause_id_generator_->GetNextId();
  lrat_proof_handler_->AddInferredClause(unit_id, {literal.Negated()},
                                         tmp_clause_ids_);
  to_fix_unit_id_.push_back({unit_id});
}

// Fixes all the literals in to_fix_, and finish propagation.
bool FailedLiteralProbing::ProcessLiteralsToFix() {
  for (int i = 0; i < to_fix_.size(); ++i) {
    const Literal literal = to_fix_[i];
    if (assignment_.LiteralIsTrue(literal)) continue;
    ++num_explicit_fix_;
    const ClauseId clause_id =
        lrat_proof_handler_ != nullptr ? to_fix_unit_id_[i] : kNoClauseId;
    if (!clause_manager_->InprocessingAddUnitClause(clause_id, literal)) {
      return false;
    }
  }
  to_fix_.clear();
  to_fix_unit_id_.clear();
  return sat_solver_->FinishPropagation();
}

}  // namespace sat
}  // namespace operations_research
