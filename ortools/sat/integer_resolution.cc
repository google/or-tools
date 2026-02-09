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

#include "ortools/sat/integer_resolution.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/log/vlog_is_on.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/synchronization.h"

namespace operations_research::sat {

IntegerConflictResolution::IntegerConflictResolution(Model* model)
    : trail_(model->GetOrCreate<Trail>()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()),
      integer_encoder_(model->GetOrCreate<IntegerEncoder>()),
      sat_solver_(model->GetOrCreate<SatSolver>()),
      shared_stats_(model->GetOrCreate<SharedStatistics>()),
      clauses_propagator_(model->GetOrCreate<ClauseManager>()),
      implications_(model->GetOrCreate<BinaryImplicationGraph>()),
      params_(*model->GetOrCreate<SatParameters>()) {
  trail_->SetConflictResolutionFunction(
      [this](std::vector<Literal>* conflict,
             std::vector<Literal>* reason_used_to_infer_the_conflict) {
        ComputeFirstUIPConflict(conflict, reason_used_to_infer_the_conflict);
      });
  integer_trail_->UseNewConflictResolution();
}

IntegerConflictResolution::~IntegerConflictResolution() {
  if (!VLOG_IS_ON(1)) return;
  std::vector<std::pair<std::string, int64_t>> stats;
  stats.push_back(
      {"IntegerConflictResolution/num_expansions", num_expansions_});
  stats.push_back({"IntegerConflictResolution/num_conflicts_at_wrong_level",
                   num_conflicts_at_wrong_level_});
  stats.push_back({"IntegerConflictResolution/num_conflict_literals",
                   num_conflict_literals_});
  stats.push_back({"IntegerConflictResolution/num_associated",
                   num_associated_integer_for_literals_in_conflict_});

  stats.push_back({"IntegerConflictResolution/num_asso_lit_uses",
                   num_associated_literal_use_});
  stats.push_back({"IntegerConflictResolution/num_asso_lit_fails",
                   num_associated_literal_fail_});
  stats.push_back({"IntegerConflictResolution/num_possibly_non_optimal_reason",
                   num_possibly_non_optimal_reason_});
  stats.push_back(
      {"IntegerConflictResolution/num_slack_usage", num_slack_usage_});
  stats.push_back(
      {"IntegerConflictResolution/num_slack_relax", num_slack_relax_});
  stats.push_back(
      {"IntegerConflictResolution/num_holes_relax", num_holes_relax_});
  stats.push_back(
      {"IntegerConflictResolution/num_new_1uip_bools", num_created_1uip_bool_});
  stats.push_back({"IntegerConflictResolution/num_binary_minimizations",
                   num_binary_minimization_});

  if (comparison_old_sum_of_literals_ > 0) {
    stats.push_back({"Comparison/num_win", comparison_num_win_});
    stats.push_back({"Comparison/num_same", comparison_num_same_});
    stats.push_back({"Comparison/num_loose", comparison_num_loose_});
    stats.push_back(
        {"Comparison/old_sum_of_literals", comparison_old_sum_of_literals_});
  }

  shared_stats_->AddStats(stats);
}

absl::Span<const IntegerLiteral>
IntegerConflictResolution::IndexToIntegerLiterals(GlobalTrailIndex index) {
  if (index.IsInteger()) {
    tmp_integer_literals_.clear();
    tmp_integer_literals_.push_back(
        integer_trail_->IntegerLiteralAtIndex(index.integer_index));
    return tmp_integer_literals_;
  } else if (index.bool_index < trail_->Index()) {
    const Literal literal = (*trail_)[index.bool_index];
    return integer_encoder_->GetIntegerLiterals(literal);
  }
  return {};
}

IntegerValue IntegerConflictResolution::RelaxBoundIfHoles(IntegerVariable var,
                                                          IntegerValue value) {
  if (!integer_encoder_->VariableDomainHasHoles(var)) return value;
  const auto [cano_lit, negated_lit] = integer_encoder_->Canonicalize(
      IntegerLiteral::GreaterOrEqual(var, value));
  const IntegerValue relaxed = negated_lit.Negated().bound;
  if (relaxed != value) {
    CHECK_LE(relaxed, value);
    value = relaxed;
    ++num_holes_relax_;
  }
  return value;
}

void IntegerConflictResolution::AddToQueue(GlobalTrailIndex source_index,
                                           const IntegerReason& reason) {
  ++num_expansions_;

  // If we have a linear reason with slack, check to see if we can relax the
  // reason and have more slack, because we don't need to explain the stronger
  // possible push that was done.
  //
  // TODO(user): Skip for the first AddToQueue() that correspond to a conflict.
  // Or handle properly, for now, we never have !vars.empty() for conflicts.
  IntegerValue slack = reason.slack;
  if (!reason.vars.empty()) {
    const IntegerLiteral propagated_i_lit = reason.propagated_i_lit;
    const IntegerVariable var = propagated_i_lit.var;

    IntegerValue needed_bound = kMaxIntegerValue;
    if (source_index.IsInteger()) {
      CHECK_LE(reason.index_at_propagation, source_index.integer_index);
      CHECK_EQ(var,
               integer_trail_->IntegerLiteralAtIndex(source_index.integer_index)
                   .var);
      IntegerVariableData& data = int_data_[var];
      needed_bound = data.bound;
    } else {
      // Currently the only other case where we have a linear reason is for
      // associated literals, in which case, we just need to explain the
      // associated bound, which might be lower than what is currently
      // explained.
      for (const IntegerLiteral i_lit : IndexToIntegerLiterals(source_index)) {
        if (i_lit.var == var) {
          // In some corner case we see the same variable more than once!
          needed_bound = std::min(i_lit.bound, needed_bound);
        }
      }

      // Special case for initial conflict.
      // TODO(user): We can relax more in this case.
      if (source_index.bool_index == trail_->Index()) {
        needed_bound = propagated_i_lit.bound;
      }
      CHECK_NE(needed_bound, kMaxIntegerValue);
    }

    // If we have holes, and var >= needed_bound falls into one, we can relax
    // it as much as possible.
    //
    // Note that this is needed for the check needed_bound <= propagated_bound.
    needed_bound = RelaxBoundIfHoles(var, needed_bound);
    CHECK_LE(needed_bound, propagated_i_lit.bound);

    // TODO(user): It might be better to pass to the Explain() function the
    // thing we need to be explaining, and let it handle the modification of
    // the slack. So we can also relax non-linear reason.
    if (needed_bound < propagated_i_lit.bound) {
      IntegerValue coeff = 0;
      for (int i = 0; i < reason.vars.size(); ++i) {
        if (reason.vars[i] == NegationOf(propagated_i_lit.var)) {
          coeff = reason.coeffs[i];
          break;
        }
      }
      CHECK_GT(coeff, 0);  // Should always be positive.

      // Bump the slack !
      ++num_slack_relax_;
      slack = CapAddI(slack,
                      CapProdI(coeff, propagated_i_lit.bound - needed_bound));
    }
  }

  // Reset.
  // As we explain var >= bound, we might need var >= lower_bound.
  for (const IntegerLiteral i_lit : IndexToIntegerLiterals(source_index)) {
    IntegerVariableData& data = int_data_[i_lit.var];
    if (i_lit.bound >= data.bound) {
      data.bound = kMinIntegerValue;
    }
  }

  for (const auto span_of_literals :
       {reason.boolean_literals_at_true, reason.boolean_literals_at_false}) {
    for (const Literal literal : span_of_literals) {
      const auto& info = trail_->Info(literal.Variable());
      if (info.level == 0) continue;
      if (tmp_bool_index_seen_[info.trail_index]) continue;

      tmp_bool_index_seen_.Set(info.trail_index);

      const GlobalTrailIndex index{info.level, info.trail_index};
      tmp_queue_.push_back(index);
    }
  }
  for (const IntegerLiteral i_lit : reason.integer_literals) {
    ProcessIntegerLiteral(source_index, i_lit);
  }

  // Deal with linear reason.
  // TODO(user): The support for that could be improved.
  // In particular, we can sort in order to process slack in a good heuristic
  // order.
  if (reason.vars.empty()) return;

  const int size = reason.vars.size();
  const IntegerVariable to_ignore =
      PositiveVariable(reason.propagated_i_lit.var);
  for (int i = 0; i < size; ++i) {
    const IntegerVariable var = reason.vars[i];
    if (PositiveVariable(var) == to_ignore) continue;

    IntegerVariableData& data = int_data_[var];
    if (!data.in_queue) {
      data.int_index_in_queue = integer_trail_->GetFirstIndexBefore(
          var, source_index, data.int_index_in_queue);
      if (data.int_index_in_queue < 0) continue;  // root level.

      data.in_queue = true;
      tmp_queue_.push_back(
          integer_trail_->GlobalIndexAt(data.int_index_in_queue));
    }

    CHECK_LT(integer_trail_->GlobalIndexAt(data.int_index_in_queue),
             source_index);

    // In all case, we need the bound at the time.
    // in some rare case, we have reason.index_at_propagation <
    // data.int_index_in_queue So we might use a stronger integer literal than
    // necessary. Investigate further.
    if (data.int_index_in_queue > reason.index_at_propagation) {
      ++num_possibly_non_optimal_reason_;
    }

    IntegerValue required_bound =
        integer_trail_->IntegerLiteralAtIndex(data.int_index_in_queue).bound;

    CHECK_GE(required_bound, data.bound);
    if (slack > 0 && required_bound > data.bound) {
      CHECK_GT(reason.coeffs[i], 0);
      IntegerValue delta = FloorRatio(slack, reason.coeffs[i]);
      delta = std::min(delta, CapSubI(required_bound, data.bound));
      if (delta > 0) {
        ++num_slack_usage_;
        required_bound -= delta;
        slack -= reason.coeffs[i] * delta;
      }
    }

    data.bound = required_bound;
  }
}

void IntegerConflictResolution::ProcessIntegerLiteral(
    GlobalTrailIndex source_index, IntegerLiteral i_lit) {
  CHECK(!i_lit.IsAlwaysFalse());
  if (i_lit.IsAlwaysTrue()) return;

  DCHECK_GE(i_lit.var, 0);
  DCHECK_LT(i_lit.var, int_data_.size());
  if (i_lit.bound <= tmp_var_to_settled_lb_[i_lit.var]) return;
  if (i_lit.bound <= integer_trail_->LevelZeroLowerBound(i_lit.var)) return;
  DCHECK_LE(i_lit.bound, integer_trail_->LowerBound(i_lit.var));

  IntegerVariableData& data = int_data_[i_lit.var];

  if (!data.in_queue) {
    // Initialize if we never saw it before.
    data.int_index_in_queue = integer_trail_->GetFirstIndexBefore(
        i_lit.var, source_index, data.int_index_in_queue);

    if (data.int_index_in_queue < 0) return;  // root level.
    data.in_queue = true;
    tmp_queue_.push_back(
        integer_trail_->GlobalIndexAt(data.int_index_in_queue));
  }

  data.bound = std::max(data.bound, i_lit.bound);
  CHECK_LE(data.bound,
           integer_trail_->IntegerLiteralAtIndex(data.int_index_in_queue).bound)
      << " " << i_lit.bound;
}

void IntegerConflictResolution::MarkAllAssociatedLiterals(
    absl::Span<const Literal> literals) {
  for (const Literal l : literals) {
    for (const IntegerLiteral i_lit : integer_encoder_->GetIntegerLiterals(l)) {
      // The std::max() is for the corner case of more than one
      // integer literal on the same variable.
      //
      // TODO(user): we should probably make sure this never happen
      // instead.
      tmp_var_to_settled_lb_[i_lit.var] =
          std::max(tmp_var_to_settled_lb_[i_lit.var], i_lit.bound);
      ++num_associated_integer_for_literals_in_conflict_;
    }
  }
}

void IntegerConflictResolution::ComputeFirstUIPConflict(
    std::vector<Literal>* conflict,
    std::vector<Literal>* reason_used_to_infer_the_conflict) {
  const int old_conflict_size = conflict->size();
  if (old_conflict_size > 0) {
    comparison_old_sum_of_literals_ += old_conflict_size;
  }

  conflict->clear();
  reason_used_to_infer_the_conflict->clear();

  // WARNING: This is not valid after further GetIntegerReason() calls.
  const IntegerReason& starting_conflict = integer_trail_->IntegerConflict();
  if (starting_conflict.empty()) return;

  // Clear data.
  // TODO(user): Sparse clear.
  const int num_i_vars = integer_trail_->NumIntegerVariables().value();
  int_data_.clear();
  int_data_.resize(num_i_vars);
  // Note the +1 in case we create a new 1-UIP boolean.
  tmp_bool_index_seen_.ClearAndResize(trail_->Index() + 1);
  tmp_var_to_settled_lb_.assign(num_i_vars, kMinIntegerValue);

  tmp_queue_.clear();
  AddToQueue(GlobalTrailIndex{trail_->CurrentDecisionLevel(), trail_->Index()},
             starting_conflict);
  std::make_heap(tmp_queue_.begin(), tmp_queue_.end());

  // We will expand Booleans as long as we don't have first UIP.
  // Then we will expand all integer_literal until we have only Boolean left.
  int64_t work_done = 0;
  bool uip_found = false;
  while (!tmp_queue_.empty()) {
    ++work_done;

    // TODO(user): By looking at the reason, we can "correct" the level and
    // trail index, if it changes, we could update the queue and continue. This
    // is however harder to do now that we use bounds for the reason not
    // indices.
    GlobalTrailIndex top_index = tmp_queue_.front();
    std::pop_heap(tmp_queue_.begin(), tmp_queue_.end());
    tmp_queue_.pop_back();

    const bool is_only_one_left_at_top_level =
        tmp_queue_.empty() || tmp_queue_.front().level < top_index.level;

    if (top_index.IsInteger()) {
      const IntegerLiteral i_lit =
          integer_trail_->IntegerLiteralAtIndex(top_index.integer_index);
      IntegerVariableData& data = int_data_[i_lit.var];
      const IntegerValue bound_to_explain = data.bound;
      CHECK(data.in_queue);
      CHECK_EQ(data.int_index_in_queue, top_index.integer_index);
      CHECK_LE(data.bound, i_lit.bound);

      // Skip until next time we need this variable.
      if (data.bound <= tmp_var_to_settled_lb_[i_lit.var] ||
          data.bound <= integer_trail_->LevelZeroLowerBound(i_lit.var) ||
          data.int_index_in_queue < num_i_vars) {
        data.in_queue = false;
        data.bound = kMinIntegerValue;
        continue;
      }

      const int previous_index =
          integer_trail_->PreviousTrailIndex(top_index.integer_index);
      if (data.bound < i_lit.bound) {
        if (previous_index >= 0) {
          const IntegerLiteral previous_i_lit =
              integer_trail_->IntegerLiteralAtIndex(previous_index);
          if (data.bound <= previous_i_lit.bound) {
            // The previous integer entry can explain our data.bound,
            // re-enqueue until next time.
            data.int_index_in_queue = previous_index;
            tmp_queue_.push_back(
                integer_trail_->GlobalIndexAt(data.int_index_in_queue));
            CHECK_LE(
                data.bound,
                integer_trail_->IntegerLiteralAtIndex(data.int_index_in_queue)
                    .bound);
            std::push_heap(tmp_queue_.begin(), tmp_queue_.end());
            continue;
          }
        } else {
          // Remove.
          // This variable shouldn't be needed anymore.
          data.int_index_in_queue = previous_index;
          data.in_queue = false;
          data.bound = kMinIntegerValue;
          continue;
        }
      }

      // We are going to expand the reason at top_index, clear the data for
      // future reasons.
      data.int_index_in_queue = previous_index;
      data.in_queue = false;

      // Optional. Try to see if we have a good enough associated
      // integer_literal. This can be disabled, but it should lead to better
      // reason hopefully.
      if (is_only_one_left_at_top_level || uip_found) {
        // We don't want trivial literal here.
        //
        // TODO(user): Deal with literal falling in holes? the situation is
        // not clear.
        const IntegerLiteral needed_lit =
            IntegerLiteral::GreaterOrEqual(i_lit.var, bound_to_explain);
        DCHECK(!integer_trail_->IsTrueAtLevelZero(needed_lit));
        DCHECK(!integer_trail_->IsTrueAtLevelZero(needed_lit.Negated()));

        // We only need to explain var >= bound_to_explain.
        // We have the explanation for i_lit.
        IntegerValue associated_bound;
        const LiteralIndex lit_index =
            integer_encoder_->SearchForLiteralAtOrAfter(
                IntegerLiteral::GreaterOrEqual(i_lit.var, bound_to_explain),
                &associated_bound);

        if (lit_index == kNoLiteralIndex) {
          IntegerValue test_bound;
          const LiteralIndex test_index =
              integer_encoder_->SearchForLiteralAtOrBefore(
                  IntegerLiteral::GreaterOrEqual(i_lit.var, bound_to_explain),
                  &test_bound);
          if (test_index != kNoLiteralIndex && test_bound == bound_to_explain) {
            LOG(FATAL) << top_index.level << " BUG " << i_lit.var
                       << " >= " << bound_to_explain
                       << " Not AtOrAfter, but at or before return"
                       << Literal(test_index) << " var >=" << test_bound
                       << " | " << integer_trail_->VarDebugString(i_lit.var);
          }
        }

        if (lit_index != kNoLiteralIndex && associated_bound <= i_lit.bound) {
          CHECK_GE(associated_bound, bound_to_explain);
          const Literal lit(lit_index);

          IntegerValue test_bound;
          const LiteralIndex test_index =
              integer_encoder_->SearchForLiteralAtOrBefore(i_lit, &test_bound);
          if (test_index != kNoLiteralIndex) {
            CHECK_LE(associated_bound, test_bound);
          }

          // Lets do more sanity_check before just using this literal.
          // Instead. Since we output it right away. we should be good.
          const auto& info = trail_->Info(lit.Variable());
          if (trail_->Assignment().LiteralIsTrue(lit) &&
              info.level == top_index.level) {
            // Note that we don't always have new_top >= top_index, this is fine
            // we can still use this Boolean in the final output.
            if (tmp_bool_index_seen_[info.trail_index]) {
              data.bound = kMinIntegerValue;
              ++num_associated_literal_use_;
              continue;
            }
            const GlobalTrailIndex new_top{info.level, info.trail_index};
            tmp_bool_index_seen_.Set(info.trail_index);

            data.bound = kMinIntegerValue;
            top_index = new_top;
            ++num_associated_literal_use_;
          } else {
            ++num_associated_literal_fail_;
          }
        } else if (params_.create_1uip_boolean_during_icr() &&
                   top_index.level > sat_solver_->AssumptionLevel() &&
                   is_only_one_left_at_top_level && !uip_found) {
          // Lets create a new associated literal and use it as the UIP.
          // Note that we should always create a new fresh literal here.
          //
          // TODO(user): Note that we disabled this with assumptions otherwise
          // we might have a core with new literal !
          const int num_bools = trail_->NumVariables();
          const Literal new_lit =
              integer_encoder_->GetOrCreateAssociatedLiteral(
                  IntegerLiteral::GreaterOrEqual(i_lit.var, bound_to_explain));
          CHECK_EQ(new_lit.Variable().value(), num_bools);

          // TODO(user): This can happen is some rare corner case, we just skip.
          if (!trail_->Assignment().LiteralIsFalse(new_lit)) {
            // The literal can be true if we have other encoding literal at true
            // that implies it. However, if we only have an integer literal that
            // implies it, the "integer_encoder_" do not have access to
            // integer_trail_ (it should probably be split) and it cannot set it
            // to true.
            if (!trail_->Assignment().LiteralIsAssigned(new_lit)) {
              // Using a decision should work as we will backtrack right away.
              trail_->EnqueueSearchDecision(new_lit);
            }

            // It should be true.
            CHECK(trail_->Assignment().LiteralIsTrue(new_lit));

            const auto& info = trail_->Info(new_lit.Variable());
            CHECK_GE(info.level, top_index.level);
            CHECK_EQ((*trail_)[info.trail_index], new_lit);
            const GlobalTrailIndex new_top{info.level, info.trail_index};

            tmp_bool_index_seen_.Set(info.trail_index);
            data.bound = kMinIntegerValue;

            top_index = new_top;
            ++num_created_1uip_bool_;
          }
        }
      }
    }

    if (top_index.IsBoolean()) {
      const Literal literal = (*trail_)[top_index.bool_index];

      // Do we have a single GlobalTrailIndex at the top assignment level ?
      if (top_index.level <= sat_solver_->AssumptionLevel()) {
        // This will just output all Booleans from the assumption level.
        uip_found = true;
      }
      if (!uip_found) {
        if (is_only_one_left_at_top_level) {
          if (top_index.level < trail_->CurrentDecisionLevel()) {
            ++num_conflicts_at_wrong_level_;
          }

          // Only one Boolean at max-level, we have the first UIP.
          uip_found = true;
        }
      }

      if (uip_found) {
        if (params_.binary_minimization_algorithm() !=
            SatParameters::NO_BINARY_MINIMIZATION) {
          if (conflict->empty()) {
            // This one will always stay in the conflict, even after
            // minimization. So we can use it to minimize the conflict and avoid
            // some further expansion.
            MarkAllAssociatedLiterals(
                implications_->GetAllImpliedLiterals(literal));
          } else {
            // This assumes no-one else call
            // GetAllImpliedLiterals()/GetNewlyImpliedLiterals() while we run
            // this algorithm, and that the info stays valid as we create new
            // literal.
            if (implications_->LiteralIsImplied(literal)) {
              ++num_binary_minimization_;
              continue;
            }

            // We are about to add this literal to the conflict, mark all the
            // literal implied using binary implication only as no need to be
            // expanded further. Note that we don't need to expand already
            // expanded literals in the binary implication graph.
            MarkAllAssociatedLiterals(
                implications_->GetNewlyImpliedLiterals(literal));
          }
        } else {
          // This literal is staying in the final conflict. If it has
          // associated integer_literal, then these integer literals will be
          // true for all the subsequent resolution. We can exploit that.
          MarkAllAssociatedLiterals({literal});
        }

        // Note that we will fill conflict in reverse order of GlobalTrailIndex.
        // So the first-UIP will be first, this is required by the sat solver.
        conflict->push_back(literal.Negated());
        continue;
      }

      // We will expand this Boolean.
      CHECK_NE(trail_->Info(literal.Variable()).type,
               AssignmentType::kSearchDecision)
          << DebugGlobalIndex(top_index)
          << " before: " << DebugGlobalIndex(tmp_queue_.front());
      reason_used_to_infer_the_conflict->push_back(literal);
    } else {
      // Skip stale integer entry.
      const IntegerLiteral i_lit =
          integer_trail_->IntegerLiteralAtIndex(top_index.integer_index);
      if (tmp_var_to_settled_lb_[i_lit.var] >= i_lit.bound) continue;
    }

    std::optional<IntegerValue> needed_bound;
    if (top_index.IsInteger()) {
      const IntegerVariable var =
          integer_trail_->IntegerLiteralAtIndex(top_index.integer_index).var;
      needed_bound = RelaxBoundIfHoles(var, int_data_[var].bound);
    }

    // Expand.
    //
    // TODO(user): There is probably a faster way to recover the heap propety
    // than doing it one by one.
    const int old_size = tmp_queue_.size();
    AddToQueue(top_index,
               integer_trail_->GetIntegerReason(top_index, needed_bound));
    for (int i = old_size + 1; i <= tmp_queue_.size(); ++i) {
      std::push_heap(tmp_queue_.begin(), tmp_queue_.begin() + i);
    }
  }

  num_conflict_literals_ += conflict->size();

  if (old_conflict_size > 0) {
    if (conflict->size() < old_conflict_size) {
      ++comparison_num_win_;
    } else if (conflict->size() > old_conflict_size) {
      ++comparison_num_loose_;
    } else {
      ++comparison_num_same_;
    }
  }
}

std::string IntegerConflictResolution::DebugGlobalIndex(
    GlobalTrailIndex index) {
  return absl::StrCat(
      index.level, "|", index.bool_index, "|",
      index.IsInteger() ? absl::StrCat(index.integer_index) : "", " ",
      (index.IsBoolean()
           ? (*trail_)[index.bool_index].DebugString()
           : integer_trail_->IntegerLiteralAtIndex(index.integer_index)
                 .DebugString()));
}

std::string IntegerConflictResolution::DebugGlobalIndex(
    absl::Span<const GlobalTrailIndex> indices) {
  std::string out = "[";
  bool first = true;
  for (const GlobalTrailIndex index : indices) {
    if (!first) absl::StrAppend(&out, ", ");
    first = false;
    absl::StrAppend(&out, DebugGlobalIndex(index));
  }
  return absl::StrCat(out, "]");
}

}  // namespace operations_research::sat
