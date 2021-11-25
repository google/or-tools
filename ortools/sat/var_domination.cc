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

#include "ortools/sat/var_domination.h"

#include <cstdint>
#include <limits>

#include "ortools/base/stl_util.h"
#include "ortools/base/strong_vector.h"

namespace operations_research {
namespace sat {

void VarDomination::Reset(int num_variables) {
  phase_ = 0;
  num_vars_with_negation_ = 2 * num_variables;
  partition_ = absl::make_unique<DynamicPartition>(num_vars_with_negation_);

  can_freely_decrease_.assign(num_vars_with_negation_, true);

  shared_buffer_.clear();
  initial_candidates_.assign(num_vars_with_negation_, IntegerVariableSpan());

  buffer_.clear();
  dominating_vars_.assign(num_vars_with_negation_, IntegerVariableSpan());

  ct_index_for_signature_ = 0;
  block_down_signatures_.assign(num_vars_with_negation_, 0);
}

void VarDomination::RefinePartition(std::vector<int>* vars) {
  if (vars->empty()) return;
  partition_->Refine(*vars);
  for (int& var : *vars) {
    const IntegerVariable wrapped(var);
    can_freely_decrease_[wrapped] = false;
    can_freely_decrease_[NegationOf(wrapped)] = false;
    var = NegationOf(wrapped).value();
  }
  partition_->Refine(*vars);
}

void VarDomination::CanOnlyDominateEachOther(absl::Span<const int> refs) {
  if (phase_ != 0) return;
  tmp_vars_.clear();
  for (const int ref : refs) {
    tmp_vars_.push_back(RefToIntegerVariable(ref).value());
  }
  RefinePartition(&tmp_vars_);
  tmp_vars_.clear();
}

void VarDomination::ActivityShouldNotChange(absl::Span<const int> refs,
                                            absl::Span<const int64_t> coeffs) {
  if (phase_ != 0) return;
  FillTempRanks(/*reverse_references=*/false, /*enforcements=*/{}, refs,
                coeffs);
  tmp_vars_.clear();
  for (int i = 0; i < tmp_ranks_.size(); ++i) {
    if (i > 0 && tmp_ranks_[i].rank != tmp_ranks_[i - 1].rank) {
      RefinePartition(&tmp_vars_);
      tmp_vars_.clear();
    }
    tmp_vars_.push_back(tmp_ranks_[i].var.value());
  }
  RefinePartition(&tmp_vars_);
  tmp_vars_.clear();
}

// This correspond to a lower bounded constraint.
void VarDomination::ProcessTempRanks() {
  if (phase_ == 0) {
    // We actually "split" tmp_ranks_ according to the current partition and
    // process each resulting list independently for a faster algo.
    ++ct_index_for_signature_;
    for (IntegerVariableWithRank& entry : tmp_ranks_) {
      can_freely_decrease_[entry.var] = false;
      block_down_signatures_[entry.var] |= uint64_t{1}
                                           << (ct_index_for_signature_ % 64);
      entry.part = partition_->PartOf(entry.var.value());
    }
    std::stable_sort(
        tmp_ranks_.begin(), tmp_ranks_.end(),
        [](const IntegerVariableWithRank& a, const IntegerVariableWithRank& b) {
          return a.part < b.part;
        });
    int start = 0;
    for (int i = 1; i < tmp_ranks_.size(); ++i) {
      if (tmp_ranks_[i].part != tmp_ranks_[start].part) {
        Initialize({&tmp_ranks_[start], static_cast<size_t>(i - start)});
        start = i;
      }
    }
    if (start < tmp_ranks_.size()) {
      Initialize({&tmp_ranks_[start], tmp_ranks_.size() - start});
    }
  } else if (phase_ == 1) {
    FilterUsingTempRanks();
  } else {
    // This is only used for debugging, and we shouldn't reach here in prod.
    CheckUsingTempRanks();
  }
}

void VarDomination::ActivityShouldNotDecrease(
    absl::Span<const int> enforcements, absl::Span<const int> refs,
    absl::Span<const int64_t> coeffs) {
  FillTempRanks(/*reverse_references=*/false, enforcements, refs, coeffs);
  ProcessTempRanks();
}

void VarDomination::ActivityShouldNotIncrease(
    absl::Span<const int> enforcements, absl::Span<const int> refs,
    absl::Span<const int64_t> coeffs) {
  FillTempRanks(/*reverse_references=*/true, enforcements, refs, coeffs);
  ProcessTempRanks();
}

void VarDomination::MakeRankEqualToStartOfPart(
    absl::Span<IntegerVariableWithRank> span) {
  const int size = span.size();
  int start = 0;
  int previous_value = 0;
  for (int i = 0; i < size; ++i) {
    const int64_t value = span[i].rank;
    if (value != previous_value) {
      previous_value = value;
      start = i;
    }
    span[i].rank = start;
  }
}

void VarDomination::Initialize(absl::Span<IntegerVariableWithRank> span) {
  // The rank can be wrong and need to be recomputed because of how we splitted
  // tmp_ranks_ into spans.
  MakeRankEqualToStartOfPart(span);

  const int future_start = shared_buffer_.size();
  int first_start = -1;

  // This is mainly to avoid corner case and hopefully, it should be big enough
  // to not matter too much.
  const int kSizeThreshold = 1000;
  const int size = span.size();
  for (int i = std::max(0, size - kSizeThreshold); i < size; ++i) {
    const IntegerVariableWithRank entry = span[i];
    const int num_candidates = size - entry.rank;
    if (num_candidates >= kSizeThreshold) continue;

    // Compute size to beat.
    int size_threshold = kSizeThreshold;

    // Take into account the current partition size.
    const int var_part = partition_->PartOf(entry.var.value());
    const int part_size = partition_->SizeOfPart(var_part);
    size_threshold = std::min(size_threshold, part_size);

    // Take into account our current best candidate if there is one.
    const int current_num_candidates = initial_candidates_[entry.var].size;
    if (current_num_candidates != 0) {
      size_threshold = std::min(size_threshold, current_num_candidates);
    }

    if (num_candidates < size_threshold) {
      if (first_start == -1) first_start = entry.rank;
      initial_candidates_[entry.var] = {
          future_start - first_start + static_cast<int>(entry.rank),
          num_candidates};
    }
  }

  // Only store what is necessary.
  if (first_start == -1) return;
  for (int i = first_start; i < size; ++i) {
    shared_buffer_.push_back(span[i].var);
  }
}

// TODO(user): Use more heuristics to not miss as much dominance relation when
// we crop initial lists.
void VarDomination::EndFirstPhase() {
  CHECK_EQ(phase_, 0);
  phase_ = 1;

  // Some initial lists ar too long and will be cropped to this size.
  // We will handle them slightly differently.
  //
  // TODO(user): Tune the initial size, 50 might be a bit large, since our
  // complexity is borned by this number times the number of entries in the
  // constraints. Still we should in most situation be a lot lower than that.
  const int kMaxInitialSize = 50;
  std::vector<IntegerVariable> cropped_lists;
  absl::StrongVector<IntegerVariable, bool> is_cropped(num_vars_with_negation_,
                                                       false);

  // Fill the initial domination candidates.
  for (IntegerVariable var(0); var < num_vars_with_negation_; ++var) {
    if (can_freely_decrease_[var]) continue;
    const int part = partition_->PartOf(var.value());
    const int part_size = partition_->SizeOfPart(part);

    const int start = buffer_.size();
    int new_size = 0;

    const uint64_t var_sig = block_down_signatures_[var];
    const uint64_t not_var_sig = block_down_signatures_[NegationOf(var)];
    const int stored_size = initial_candidates_[var].size;
    if (stored_size == 0 || part_size < stored_size) {
      // We start with the partition part.
      // Note that all constraint will be filtered again in the second pass.
      int num_tested = 0;
      for (const int value : partition_->ElementsInPart(part)) {
        const IntegerVariable c = IntegerVariable(value);

        // This is to limit the complexity to 1k * num_vars. We fill the list
        // with dummy node so that the heuristic below will fill it with
        // potential transpose candidates.
        if (++num_tested > 1000) {
          is_cropped[var] = true;
          cropped_lists.push_back(var);
          int extra = new_size;
          while (extra < kMaxInitialSize) {
            ++extra;
            buffer_.push_back(kNoIntegerVariable);
          }
          break;
        }
        if (PositiveVariable(c) == PositiveVariable(var)) continue;
        if (can_freely_decrease_[NegationOf(c)]) continue;
        if (var_sig & ~block_down_signatures_[c]) continue;  // !included.
        if (block_down_signatures_[NegationOf(c)] & ~not_var_sig) continue;
        ++new_size;
        buffer_.push_back(c);

        // We do not want too many candidates per variables.
        // TODO(user): randomize?
        if (new_size > kMaxInitialSize) {
          is_cropped[var] = true;
          cropped_lists.push_back(var);
          break;
        }
      }
    } else {
      // Copy the one that are in the same partition_ part.
      //
      // TODO(user): This can be too long maybe? even if we have list of at
      // most 1000 at this point, see InitializeUsingTempRanks().
      for (const IntegerVariable c : InitialDominatingCandidates(var)) {
        if (PositiveVariable(c) == PositiveVariable(var)) continue;
        if (can_freely_decrease_[NegationOf(c)]) continue;
        if (partition_->PartOf(c.value()) != part) continue;
        if (var_sig & ~block_down_signatures_[c]) continue;  // !included.
        if (block_down_signatures_[NegationOf(c)] & ~not_var_sig) continue;
        ++new_size;
        buffer_.push_back(c);

        // We do not want too many candidates per variables.
        // TODO(user): randomize?
        if (new_size > kMaxInitialSize) {
          is_cropped[var] = true;
          cropped_lists.push_back(var);
          break;
        }
      }
    }

    dominating_vars_[var] = {start, new_size};
  }

  // Heuristic: To try not to remove domination relations corresponding to short
  // lists during transposition (see EndSecondPhase()), we fill half of the
  // cropped list with the transpose of the short list relations. This helps
  // finding more relation in the presence of cropped lists.
  for (const IntegerVariable var : cropped_lists) {
    if (kMaxInitialSize / 2 < dominating_vars_[var].size) {
      dominating_vars_[var].size = kMaxInitialSize / 2;  // Restrict.
    }
  }
  for (IntegerVariable var(0); var < num_vars_with_negation_; ++var) {
    for (const IntegerVariable dom : DominatingVariables(var)) {
      if (!is_cropped[NegationOf(dom)]) continue;
      IntegerVariableSpan& s = dominating_vars_[NegationOf(dom)];
      if (s.size >= kMaxInitialSize) continue;
      buffer_[s.start + s.size++] = NegationOf(var);
    }
  }

  // Remove any duplicates.
  //
  // TODO(user): Maybe we should do that with all lists in case the
  // input function are called with duplicates too.
  for (const IntegerVariable var : cropped_lists) {
    if (!is_cropped[var]) continue;
    IntegerVariableSpan& s = dominating_vars_[var];
    std::sort(&buffer_[s.start], &buffer_[s.start + s.size]);
    const auto p = std::unique(&buffer_[s.start], &buffer_[s.start + s.size]);
    s.size = p - &buffer_[s.start];
  }

  // We no longer need the first phase memory.
  VLOG(1) << "Num initial list that where cropped: " << cropped_lists.size();
  VLOG(1) << "Shared buffer size: " << shared_buffer_.size();
  VLOG(1) << "Buffer size: " << buffer_.size();
  gtl::STLClearObject(&initial_candidates_);
  gtl::STLClearObject(&shared_buffer_);
}

void VarDomination::EndSecondPhase() {
  CHECK_EQ(phase_, 1);
  phase_ = 2;

  // Perform intersection with transpose.
  shared_buffer_.clear();
  initial_candidates_.assign(num_vars_with_negation_, IntegerVariableSpan());

  // Pass 1: count.
  for (IntegerVariable var(0); var < num_vars_with_negation_; ++var) {
    for (const IntegerVariable dom : DominatingVariables(var)) {
      ++initial_candidates_[NegationOf(dom)].size;
    }
  }

  // Pass 2: compute starts.
  int start = 0;
  for (IntegerVariable var(0); var < num_vars_with_negation_; ++var) {
    initial_candidates_[var].start = start;
    start += initial_candidates_[var].size;
    initial_candidates_[var].size = 0;
  }
  shared_buffer_.resize(start);

  // Pass 3: transpose.
  for (IntegerVariable var(0); var < num_vars_with_negation_; ++var) {
    for (const IntegerVariable dom : DominatingVariables(var)) {
      IntegerVariableSpan& span = initial_candidates_[NegationOf(dom)];
      shared_buffer_[span.start + span.size++] = NegationOf(var);
    }
  }

  // Pass 4: intersect.
  int num_removed = 0;
  tmp_var_to_rank_.resize(num_vars_with_negation_, -1);
  for (IntegerVariable var(0); var < num_vars_with_negation_; ++var) {
    for (const IntegerVariable dom : InitialDominatingCandidates(var)) {
      tmp_var_to_rank_[dom] = 1;
    }

    int new_size = 0;
    IntegerVariableSpan& span = dominating_vars_[var];
    for (const IntegerVariable dom : DominatingVariables(var)) {
      if (tmp_var_to_rank_[dom] != 1) {
        ++num_removed;
        continue;
      }
      buffer_[span.start + new_size++] = dom;
    }
    span.size = new_size;

    for (const IntegerVariable dom : InitialDominatingCandidates(var)) {
      tmp_var_to_rank_[dom] = -1;
    }
  }

  VLOG(1) << "Transpose removed " << num_removed;
  gtl::STLClearObject(&initial_candidates_);
  gtl::STLClearObject(&shared_buffer_);
}

void VarDomination::FillTempRanks(bool reverse_references,
                                  absl::Span<const int> enforcements,
                                  absl::Span<const int> refs,
                                  absl::Span<const int64_t> coeffs) {
  tmp_ranks_.clear();
  if (coeffs.empty()) {
    // Simple case: all coefficients are assumed to be the same.
    for (const int ref : refs) {
      const IntegerVariable var =
          RefToIntegerVariable(reverse_references ? NegatedRef(ref) : ref);
      tmp_ranks_.push_back({var, 0, 0});
    }
  } else {
    // Complex case: different coefficients.
    for (int i = 0; i < refs.size(); ++i) {
      if (coeffs[i] == 0) continue;
      const IntegerVariable var = RefToIntegerVariable(
          reverse_references ? NegatedRef(refs[i]) : refs[i]);
      if (coeffs[i] > 0) {
        tmp_ranks_.push_back({var, 0, coeffs[i]});
      } else {
        tmp_ranks_.push_back({NegationOf(var), 0, -coeffs[i]});
      }
    }
    std::sort(tmp_ranks_.begin(), tmp_ranks_.end());
    MakeRankEqualToStartOfPart({&tmp_ranks_[0], tmp_ranks_.size()});
  }

  // Add the enforcement last with a new rank. We add their negation since
  // we want the activity to not decrease, and we want to allow any
  // enforcement-- to dominate a variable in the constraint.
  const int enforcement_rank = tmp_ranks_.size();
  for (const int ref : enforcements) {
    tmp_ranks_.push_back(
        {RefToIntegerVariable(NegatedRef(ref)), 0, enforcement_rank});
  }
}

// We take the intersection of the current dominating candidate with the
// restriction imposed by the current content of tmp_ranks_.
void VarDomination::FilterUsingTempRanks() {
  // Expand ranks in temp vector.
  tmp_var_to_rank_.resize(num_vars_with_negation_, -1);
  for (const IntegerVariableWithRank entry : tmp_ranks_) {
    tmp_var_to_rank_[entry.var] = entry.rank;
  }

  // The activity of the variable in tmp_rank must not decrease.
  for (const IntegerVariableWithRank entry : tmp_ranks_) {
    // The only variables that can be paired with a var-- in the constriants are
    // the var++ in the constraints with the same rank or higher.
    //
    // Note that we only filter the var-- domination lists here, we do not
    // remove the var-- appearing in all the lists corresponding to wrong var++.
    // This is left to the tranpose operation in EndSecondPhase().
    {
      IntegerVariableSpan& span = dominating_vars_[entry.var];
      if (span.size == 0) continue;
      int new_size = 0;
      for (const IntegerVariable candidate : DominatingVariables(entry.var)) {
        if (tmp_var_to_rank_[candidate] < entry.rank) continue;
        buffer_[span.start + new_size++] = candidate;
      }
      span.size = new_size;
    }
  }

  // Reset temporary vector to all -1.
  for (const IntegerVariableWithRank entry : tmp_ranks_) {
    tmp_var_to_rank_[entry.var] = -1;
  }
}

// Slow: This is for debugging only.
void VarDomination::CheckUsingTempRanks() {
  tmp_var_to_rank_.resize(num_vars_with_negation_, -1);
  for (const IntegerVariableWithRank entry : tmp_ranks_) {
    tmp_var_to_rank_[entry.var] = entry.rank;
  }

  // The activity of the variable in tmp_rank must not decrease.
  for (IntegerVariable var(0); var < num_vars_with_negation_; ++var) {
    const int var_rank = tmp_var_to_rank_[var];
    const int negated_var_rank = tmp_var_to_rank_[NegationOf(var)];
    for (const IntegerVariable dom : DominatingVariables(var)) {
      CHECK(!can_freely_decrease_[NegationOf(dom)]);

      // Doing X--, Y++ is compatible if the rank[X] <= rank[Y]. But we also
      // need to check if doing Not(Y)-- is compatible with Not(X)++.
      CHECK_LE(var_rank, tmp_var_to_rank_[dom]);
      CHECK_LE(tmp_var_to_rank_[NegationOf(dom)], negated_var_rank);
    }
  }

  for (const IntegerVariableWithRank entry : tmp_ranks_) {
    tmp_var_to_rank_[entry.var] = -1;
  }
}

bool VarDomination::CanFreelyDecrease(int ref) const {
  return CanFreelyDecrease(RefToIntegerVariable(ref));
}

bool VarDomination::CanFreelyDecrease(IntegerVariable var) const {
  return can_freely_decrease_[var];
}

absl::Span<const IntegerVariable> VarDomination::InitialDominatingCandidates(
    IntegerVariable var) const {
  const IntegerVariableSpan span = initial_candidates_[var];
  if (span.size == 0) return absl::Span<const IntegerVariable>();
  return absl::Span<const IntegerVariable>(&shared_buffer_[span.start],
                                           span.size);
}

absl::Span<const IntegerVariable> VarDomination::DominatingVariables(
    int ref) const {
  return DominatingVariables(RefToIntegerVariable(ref));
}

absl::Span<const IntegerVariable> VarDomination::DominatingVariables(
    IntegerVariable var) const {
  const IntegerVariableSpan span = dominating_vars_[var];
  if (span.size == 0) return absl::Span<const IntegerVariable>();
  return absl::Span<const IntegerVariable>(&buffer_[span.start], span.size);
}

std::string VarDomination::DominationDebugString(IntegerVariable var) const {
  const int ref = IntegerVariableToRef(var);
  std::string result =
      absl::StrCat(PositiveRef(ref), RefIsPositive(ref) ? "--" : "++", " : ");
  for (const IntegerVariable dom : DominatingVariables(var)) {
    const int dom_ref = IntegerVariableToRef(dom);
    absl::StrAppend(&result, PositiveRef(dom_ref),
                    RefIsPositive(dom_ref) ? "++" : "--", " ");
  }
  return result;
}

// TODO(user): No need to set locking_ct_index_[var] if num_locks_[var] > 1
void DualBoundStrengthening::CannotDecrease(absl::Span<const int> refs,
                                            int ct_index) {
  for (const int ref : refs) {
    const IntegerVariable var = RefToIntegerVariable(ref);
    can_freely_decrease_until_[var] = kMaxIntegerValue;
    num_locks_[var]++;
    locking_ct_index_[var] = ct_index;
  }
}

void DualBoundStrengthening::CannotIncrease(absl::Span<const int> refs,
                                            int ct_index) {
  for (const int ref : refs) {
    const IntegerVariable var = RefToIntegerVariable(ref);
    can_freely_decrease_until_[NegationOf(var)] = kMaxIntegerValue;
    num_locks_[NegationOf(var)]++;
    locking_ct_index_[NegationOf(var)] = ct_index;
  }
}

void DualBoundStrengthening::CannotMove(absl::Span<const int> refs) {
  for (const int ref : refs) {
    const IntegerVariable var = RefToIntegerVariable(ref);
    can_freely_decrease_until_[var] = kMaxIntegerValue;
    can_freely_decrease_until_[NegationOf(var)] = kMaxIntegerValue;
    num_locks_[var]++;
    num_locks_[NegationOf(var)]++;
  }
}

template <typename LinearProto>
void DualBoundStrengthening::ProcessLinearConstraint(
    bool is_objective, const PresolveContext& context,
    const LinearProto& linear, int64_t min_activity, int64_t max_activity) {
  const int64_t lb_limit = linear.domain(linear.domain_size() - 2);
  const int64_t ub_limit = linear.domain(1);
  const int num_terms = linear.vars_size();
  for (int i = 0; i < num_terms; ++i) {
    int ref = linear.vars(i);
    int64_t coeff = linear.coeffs(i);
    if (coeff < 0) {
      ref = NegatedRef(ref);
      coeff = -coeff;
    }

    const int64_t min_term = coeff * context.MinOf(ref);
    const int64_t max_term = coeff * context.MaxOf(ref);
    const int64_t term_diff = max_term - min_term;
    const IntegerVariable var = RefToIntegerVariable(ref);

    // lb side.
    if (min_activity < lb_limit) {
      num_locks_[var]++;
      if (min_activity + term_diff < lb_limit) {
        can_freely_decrease_until_[var] = kMaxIntegerValue;
      } else {
        const IntegerValue slack(lb_limit - min_activity);
        const IntegerValue var_diff =
            CeilRatio(IntegerValue(slack), IntegerValue(coeff));
        can_freely_decrease_until_[var] =
            std::max(can_freely_decrease_until_[var],
                     IntegerValue(context.MinOf(ref)) + var_diff);
      }
    }

    if (is_objective) {
      // We never want to increase the objective value. Note that if the
      // objective is lower bounded, we checked that on the lb side above.
      num_locks_[NegationOf(var)]++;
      can_freely_decrease_until_[NegationOf(var)] = kMaxIntegerValue;
      continue;
    }

    // ub side.
    if (max_activity > ub_limit) {
      num_locks_[NegationOf(var)]++;
      if (max_activity - term_diff > ub_limit) {
        can_freely_decrease_until_[NegationOf(var)] = kMaxIntegerValue;
      } else {
        const IntegerValue slack(max_activity - ub_limit);
        const IntegerValue var_diff =
            CeilRatio(IntegerValue(slack), IntegerValue(coeff));
        can_freely_decrease_until_[NegationOf(var)] =
            std::max(can_freely_decrease_until_[NegationOf(var)],
                     -IntegerValue(context.MaxOf(ref)) + var_diff);
      }
    }
  }
}

bool DualBoundStrengthening::Strengthen(PresolveContext* context) {
  const CpModelProto& cp_model = *context->working_model;
  const int num_vars = cp_model.variables_size();
  for (int var = 0; var < num_vars; ++var) {
    if (context->IsFixed(var)) continue;

    // Fix to lb?
    const int64_t lb = context->MinOf(var);
    const int64_t ub_limit = std::max(lb, CanFreelyDecreaseUntil(var));
    if (ub_limit == lb) {
      context->UpdateRuleStats("dual: fix variable");
      CHECK(context->IntersectDomainWith(var, Domain(lb)));
      continue;
    }

    // Fix to ub?
    const int64_t ub = context->MaxOf(var);
    const int64_t lb_limit =
        std::min(ub, -CanFreelyDecreaseUntil(NegatedRef(var)));
    if (lb_limit == ub) {
      context->UpdateRuleStats("dual: fix variable");
      CHECK(context->IntersectDomainWith(var, Domain(ub)));
      continue;
    }

    // Here we can fix to any value in [ub_limit, lb_limit] that is compatible
    // with the current domain. We prefer zero or the lowest possible magnitude.
    if (lb_limit > ub_limit) {
      const Domain domain =
          context->DomainOf(var).IntersectionWith(Domain(ub_limit, lb_limit));
      if (!domain.IsEmpty()) {
        int64_t value = domain.Contains(0) ? 0 : domain.Min();
        if (value != 0) {
          for (const int64_t bound : domain.FlattenedIntervals()) {
            if (std::abs(bound) < std::abs(value)) value = bound;
          }
        }
        context->UpdateRuleStats("dual: fix variable with multiple choices");
        CHECK(context->IntersectDomainWith(var, Domain(value)));
        continue;
      }
    }

    // Here we can reduce the domain, but we must be careful when the domain
    // has holes.
    if (lb_limit > lb || ub_limit < ub) {
      const int64_t new_ub =
          ub_limit < ub
              ? context->DomainOf(var)
                    .IntersectionWith(
                        Domain(ub_limit, std::numeric_limits<int64_t>::max()))
                    .Min()
              : ub;
      const int64_t new_lb =
          lb_limit > lb
              ? context->DomainOf(var)
                    .IntersectionWith(
                        Domain(std::numeric_limits<int64_t>::min(), lb_limit))
                    .Max()
              : lb;
      context->UpdateRuleStats("dual: reduced domain");
      CHECK(context->IntersectDomainWith(var, Domain(new_lb, new_ub)));
    }
  }

  // If (a => b) is the only constraint blocking a literal a in the up
  // direction, then we can set a == b !
  //
  // TODO(user): We can deal with more general situation. For instance an at
  // most one that is the only blocking constraint can become an exactly one.
  std::vector<bool> processed(num_vars, false);
  for (int positive_ref = 0; positive_ref < num_vars; ++positive_ref) {
    if (processed[positive_ref]) continue;
    if (context->IsFixed(positive_ref)) continue;
    const IntegerVariable var = RefToIntegerVariable(positive_ref);
    int ct_index = -1;
    if (num_locks_[var] == 1 && locking_ct_index_[var] != -1) {
      ct_index = locking_ct_index_[var];
    } else if (num_locks_[NegationOf(var)] == 1 &&
               locking_ct_index_[NegationOf(var)] != -1) {
      ct_index = locking_ct_index_[NegationOf(var)];
    } else {
      continue;
    }
    const ConstraintProto& ct = context->working_model->constraints(ct_index);
    if (ct.constraint_case() == ConstraintProto::kAtMostOne) {
      context->UpdateRuleStats("TODO dual: tighten at most one");
      continue;
    }

    if (ct.constraint_case() != ConstraintProto::kBoolAnd) continue;
    if (ct.enforcement_literal().size() != 1) continue;

    // Recover a => b where a is having an unique up_lock (i.e this constraint).
    // Note that if many implications are encoded in the same bool_and, we have
    // to be careful that a is appearing in just one of them.
    int a = ct.enforcement_literal(0);
    int b = 1;
    if (PositiveRef(a) == positive_ref &&
        num_locks_[RefToIntegerVariable(NegatedRef(a))] == 1) {
      // Here, we can only add the equivalence if the literal is the only
      // on the lhs, otherwise there is actually more lock.
      if (ct.bool_and().literals().size() != 1) continue;
      b = ct.bool_and().literals(0);
    } else {
      bool found = false;
      b = NegatedRef(ct.enforcement_literal(0));
      for (const int lhs : ct.bool_and().literals()) {
        if (PositiveRef(lhs) == positive_ref &&
            num_locks_[RefToIntegerVariable(lhs)] == 1) {
          found = true;
          a = NegatedRef(lhs);
          break;
        }
      }
      CHECK(found);
    }
    CHECK_EQ(num_locks_[RefToIntegerVariable(NegatedRef(a))], 1);

    processed[PositiveRef(a)] = true;
    processed[PositiveRef(b)] = true;
    context->StoreBooleanEqualityRelation(a, b);
    context->UpdateRuleStats("dual: enforced equivalence");
  }

  return true;
}

namespace {

// TODO(user): Maybe we should avoid recomputing that here.
template <typename LinearExprProto>
void FillMinMaxActivity(const PresolveContext& context,
                        const LinearExprProto& proto, int64_t* min_activity,
                        int64_t* max_activity) {
  *min_activity = 0;
  *max_activity = 0;
  const int num_vars = proto.vars().size();
  for (int i = 0; i < num_vars; ++i) {
    const int64_t a = proto.coeffs(i) * context.MinOf(proto.vars(i));
    const int64_t b = proto.coeffs(i) * context.MaxOf(proto.vars(i));
    *min_activity += std::min(a, b);
    *max_activity += std::max(a, b);
  }
}

}  // namespace

void DetectDominanceRelations(
    const PresolveContext& context, VarDomination* var_domination,
    DualBoundStrengthening* dual_bound_strengthening) {
  const CpModelProto& cp_model = *context.working_model;
  const int num_vars = cp_model.variables().size();
  var_domination->Reset(num_vars);
  dual_bound_strengthening->Reset(num_vars);

  int64_t min_activity = std::numeric_limits<int64_t>::min();
  int64_t max_activity = std::numeric_limits<int64_t>::max();

  for (int var = 0; var < num_vars; ++var) {
    // Deal with the affine relations that are not part of the proto.
    // Those only need to be processed in the first pass.
    //
    // TODO(user): This is not ideal since if only the representative is still
    // used, we shouldn't restrict any dominance relation involving it.
    const AffineRelation::Relation r = context.GetAffineRelation(var);
    if (r.representative != var) {
      dual_bound_strengthening->CannotMove({var, r.representative});
      if (r.coeff == 1) {
        var_domination->CanOnlyDominateEachOther(
            {NegatedRef(var), r.representative});
      } else if (r.coeff == -1) {
        var_domination->CanOnlyDominateEachOther({var, r.representative});
      } else {
        var_domination->CanOnlyDominateEachOther({var});
        var_domination->CanOnlyDominateEachOther({r.representative});
      }
    }

    // Also ignore variables that have been substitued already or are unused.
    if (context.IsFixed(var) || context.VariableWasRemoved(var) ||
        context.VariableIsNotUsedAnymore(var)) {
      dual_bound_strengthening->CannotMove({var});
      var_domination->CanOnlyDominateEachOther({var});
    }
  }

  // TODO(user): Benchmark and experiment with 3 phases algo:
  // - Only ActivityShouldNotChange()/CanOnlyDominateEachOther().
  // - The other cases once.
  // - EndFirstPhase() and then the other cases a second time.
  std::vector<int> tmp;
  const int num_constraints = cp_model.constraints_size();
  for (int phase = 0; phase < 2; phase++) {
    for (int c = 0; c < num_constraints; ++c) {
      const ConstraintProto& ct = cp_model.constraints(c);
      if (phase == 0) {
        dual_bound_strengthening->CannotIncrease(ct.enforcement_literal(), c);
      }
      switch (ct.constraint_case()) {
        case ConstraintProto::kBoolOr:
          if (phase == 0) {
            dual_bound_strengthening->CannotDecrease(ct.bool_or().literals());
          }
          var_domination->ActivityShouldNotDecrease(ct.enforcement_literal(),
                                                    ct.bool_or().literals(),
                                                    /*coeffs=*/{});
          break;
        case ConstraintProto::kBoolAnd:
          if (phase == 0) {
            dual_bound_strengthening->CannotDecrease(ct.bool_and().literals(),
                                                     c);
          }

          // We process it like n clauses.
          //
          // TODO(user): the way we process that is a bit restrictive. By
          // working on the implication graph we could detect more dominance
          // relations. Since if a => b we say that a++ can only be paired with
          // b--, but it could actually be paired with any variables that when
          // dereased implies b = 0. This is a bit mitigated by the fact that
          // we regroup when we can such implications into big at most ones.
          tmp.clear();
          for (const int ref : ct.enforcement_literal()) {
            tmp.push_back(NegatedRef(ref));
          }
          for (const int ref : ct.bool_and().literals()) {
            tmp.push_back(ref);
            var_domination->ActivityShouldNotDecrease(/*enforcements=*/{}, tmp,
                                                      /*coeffs=*/{});
            tmp.pop_back();
          }
          break;
        case ConstraintProto::kAtMostOne:
          if (phase == 0) {
            dual_bound_strengthening->CannotIncrease(
                ct.at_most_one().literals(), c);
          }
          var_domination->ActivityShouldNotIncrease(ct.enforcement_literal(),
                                                    ct.at_most_one().literals(),
                                                    /*coeffs=*/{});
          break;
        case ConstraintProto::kExactlyOne:
          if (phase == 0) {
            dual_bound_strengthening->CannotMove(ct.exactly_one().literals());
          }
          var_domination->ActivityShouldNotChange(ct.exactly_one().literals(),
                                                  /*coeffs=*/{});
          break;
        case ConstraintProto::kLinear: {
          FillMinMaxActivity(context, ct.linear(), &min_activity,
                             &max_activity);
          if (phase == 0) {
            dual_bound_strengthening->ProcessLinearConstraint(
                false, context, ct.linear(), min_activity, max_activity);
          }
          const bool domain_is_simple = ct.linear().domain().size() == 2;
          const bool free_to_increase =
              domain_is_simple && ct.linear().domain(1) >= max_activity;
          const bool free_to_decrease =
              domain_is_simple && ct.linear().domain(0) <= min_activity;
          if (free_to_decrease && free_to_increase) break;
          if (free_to_increase) {
            var_domination->ActivityShouldNotDecrease(ct.enforcement_literal(),
                                                      ct.linear().vars(),
                                                      ct.linear().coeffs());
          } else if (free_to_decrease) {
            var_domination->ActivityShouldNotIncrease(ct.enforcement_literal(),
                                                      ct.linear().vars(),
                                                      ct.linear().coeffs());
          } else {
            // TODO(user): Handle enforcement better here.
            if (!ct.enforcement_literal().empty()) {
              var_domination->ActivityShouldNotIncrease(
                  /*enforcements=*/{}, ct.enforcement_literal(), /*coeffs=*/{});
            }
            var_domination->ActivityShouldNotChange(ct.linear().vars(),
                                                    ct.linear().coeffs());
          }
          break;
        }
        default:
          // We cannot infer anything if we don't know the constraint.
          // TODO(user): Handle enforcement better here.
          if (phase == 0) {
            dual_bound_strengthening->CannotMove(context.ConstraintToVars(c));
          }
          for (const int var : context.ConstraintToVars(c)) {
            var_domination->CanOnlyDominateEachOther({var});
          }
          break;
      }
    }

    // The objective is handled like a <= constraints, or an == constraint if
    // there is a non-trivial domain.
    if (cp_model.has_objective()) {
      // WARNING: The proto objective might not be up to date, so we need to
      // write it first.
      if (phase == 0) {
        context.WriteObjectiveToProto();
      }
      FillMinMaxActivity(context, cp_model.objective(), &min_activity,
                         &max_activity);
      if (phase == 0) {
        dual_bound_strengthening->ProcessLinearConstraint(
            true, context, cp_model.objective(), min_activity, max_activity);
      }
      const auto& domain = cp_model.objective().domain();
      if (domain.empty() || (domain.size() == 2 && domain[0] <= min_activity)) {
        var_domination->ActivityShouldNotIncrease(
            /*enforcements=*/{}, cp_model.objective().vars(),
            cp_model.objective().coeffs());
      } else {
        var_domination->ActivityShouldNotChange(cp_model.objective().vars(),
                                                cp_model.objective().coeffs());
      }
    }

    if (phase == 0) var_domination->EndFirstPhase();
    if (phase == 1) var_domination->EndSecondPhase();
  }

  // Some statistics.
  int64_t num_unconstrained_refs = 0;
  int64_t num_dominated_refs = 0;
  int64_t num_dominance_relations = 0;
  for (int var = 0; var < num_vars; ++var) {
    if (context.IsFixed(var)) continue;

    for (const int ref : {var, NegatedRef(var)}) {
      if (var_domination->CanFreelyDecrease(ref)) {
        num_unconstrained_refs++;
      } else if (!var_domination->DominatingVariables(ref).empty()) {
        num_dominated_refs++;
        num_dominance_relations +=
            var_domination->DominatingVariables(ref).size();
      }
    }
  }
  if (num_unconstrained_refs == 0 && num_dominated_refs == 0) return;
  VLOG(1) << "Dominance:"
          << " num_unconstrained_refs=" << num_unconstrained_refs
          << " num_dominated_refs=" << num_dominated_refs
          << " num_dominance_relations=" << num_dominance_relations;
}

bool ExploitDominanceRelations(const VarDomination& var_domination,
                               PresolveContext* context) {
  const CpModelProto& cp_model = *context->working_model;
  const int num_vars = cp_model.variables_size();

  // Abort early if there is nothing to do.
  bool work_to_do = false;
  for (int var = 0; var < num_vars; ++var) {
    if (context->IsFixed(var)) continue;
    if (!var_domination.DominatingVariables(var).empty() ||
        !var_domination.DominatingVariables(NegatedRef(var)).empty()) {
      work_to_do = true;
      break;
    }
  }
  if (!work_to_do) return true;

  absl::StrongVector<IntegerVariable, int64_t> var_lb_to_ub_diff(num_vars * 2,
                                                                 0);
  absl::StrongVector<IntegerVariable, bool> in_constraints(num_vars * 2, false);

  const int num_constraints = cp_model.constraints_size();
  for (int c = 0; c < num_constraints; ++c) {
    const ConstraintProto& ct = cp_model.constraints(c);

    if (ct.constraint_case() == ConstraintProto::kBoolAnd) {
      if (ct.enforcement_literal().size() != 1) continue;
      const int a = ct.enforcement_literal(0);
      if (context->IsFixed(a)) continue;
      for (const int b : ct.bool_and().literals()) {
        if (context->IsFixed(b)) continue;

        // If (a--, b--) is valid, we can always set a to false.
        for (const IntegerVariable ivar :
             var_domination.DominatingVariables(a)) {
          const int ref = VarDomination::IntegerVariableToRef(ivar);
          if (ref == NegatedRef(b)) {
            context->UpdateRuleStats("domination: in implication");
            if (!context->SetLiteralToFalse(a)) return false;
            break;
          }
        }
        if (context->IsFixed(a)) break;

        // If (b++, a++) is valid, then we can always set b to true.
        for (const IntegerVariable ivar :
             var_domination.DominatingVariables(NegatedRef(b))) {
          const int ref = VarDomination::IntegerVariableToRef(ivar);
          if (ref == a) {
            context->UpdateRuleStats("domination: in implication");
            if (!context->SetLiteralToTrue(b)) return false;
            break;
          }
        }
      }
      continue;
    }

    if (!ct.enforcement_literal().empty()) continue;

    // TODO(user): Also deal with exactly one.
    // TODO(user): More generally, combine with probing? if a dominated variable
    // implies one of its dominant to zero, then it can be set to zero. It seems
    // adding the implication below should have the same effect? but currently
    // it requires a lot of presolve rounds.
    if (ct.constraint_case() == ConstraintProto::kAtMostOne) {
      for (const int ref : ct.at_most_one().literals()) {
        in_constraints[VarDomination::RefToIntegerVariable(ref)] = true;
      }
      for (const int ref : ct.at_most_one().literals()) {
        if (context->IsFixed(ref)) continue;

        const auto dominating_ivars = var_domination.DominatingVariables(ref);
        if (dominating_ivars.empty()) continue;
        for (const IntegerVariable ivar : dominating_ivars) {
          if (!in_constraints[ivar]) continue;
          if (context->IsFixed(VarDomination::IntegerVariableToRef(ivar))) {
            continue;
          }

          // We can set the dominated variable to false.
          context->UpdateRuleStats("domination: in at most one");
          if (!context->SetLiteralToFalse(ref)) return false;
          break;
        }
      }
      for (const int ref : ct.at_most_one().literals()) {
        in_constraints[VarDomination::RefToIntegerVariable(ref)] = false;
      }
    }

    if (ct.constraint_case() != ConstraintProto::kLinear) continue;

    int num_dominated = 0;
    for (const int var : context->ConstraintToVars(c)) {
      if (!var_domination.DominatingVariables(var).empty()) ++num_dominated;
      if (!var_domination.DominatingVariables(NegatedRef(var)).empty()) {
        ++num_dominated;
      }
    }
    if (num_dominated == 0) continue;

    // Precompute.
    int64_t min_activity = 0;
    int64_t max_activity = 0;
    const int num_terms = ct.linear().vars_size();
    for (int i = 0; i < num_terms; ++i) {
      int ref = ct.linear().vars(i);
      int64_t coeff = ct.linear().coeffs(i);
      if (coeff < 0) {
        ref = NegatedRef(ref);
        coeff = -coeff;
      }
      const int64_t min_term = coeff * context->MinOf(ref);
      const int64_t max_term = coeff * context->MaxOf(ref);
      min_activity += min_term;
      max_activity += max_term;
      const IntegerVariable ivar = VarDomination::RefToIntegerVariable(ref);
      var_lb_to_ub_diff[ivar] = max_term - min_term;
      var_lb_to_ub_diff[NegationOf(ivar)] = min_term - max_term;
    }
    const int64_t rhs_lb = ct.linear().domain(0);
    const int64_t rhs_ub = ct.linear().domain(ct.linear().domain_size() - 1);
    if (max_activity < rhs_lb || min_activity > rhs_ub) {
      return context->NotifyThatModelIsUnsat("linear equation unsat.");
    }

    // Look for dominated var.
    for (int i = 0; i < num_terms; ++i) {
      const int ref = ct.linear().vars(i);
      const int64_t coeff = ct.linear().coeffs(i);
      const int64_t coeff_magnitude = std::abs(coeff);
      if (context->IsFixed(ref)) continue;

      for (const int current_ref : {ref, NegatedRef(ref)}) {
        const absl::Span<const IntegerVariable> dominated_by =
            var_domination.DominatingVariables(current_ref);
        if (dominated_by.empty()) continue;

        const bool ub_side = (coeff > 0) == (current_ref == ref);
        if (ub_side) {
          if (max_activity <= rhs_ub) continue;
        } else {
          if (min_activity >= rhs_lb) continue;
        }
        const int64_t slack =
            ub_side ? rhs_ub - min_activity : max_activity - rhs_lb;

        // Compute the delta in activity if all dominating var moves to their
        // other bound.
        int64_t delta = 0;
        for (const IntegerVariable ivar : dominated_by) {
          if (ub_side) {
            delta += std::max(int64_t{0}, var_lb_to_ub_diff[ivar]);
          } else {
            delta += std::max(int64_t{0}, -var_lb_to_ub_diff[ivar]);
          }
        }

        const int64_t lb = context->MinOf(current_ref);
        if (delta + coeff_magnitude > slack) {
          context->UpdateRuleStats("domination: fixed to lb.");
          if (!context->IntersectDomainWith(current_ref, Domain(lb))) {
            return false;
          }

          // We need to update the precomputed quantities.
          const IntegerVariable current_var =
              VarDomination::RefToIntegerVariable(current_ref);
          if (ub_side) {
            CHECK_GE(var_lb_to_ub_diff[current_var], 0);
            max_activity -= var_lb_to_ub_diff[current_var];
          } else {
            CHECK_LE(var_lb_to_ub_diff[current_var], 0);
            min_activity -= var_lb_to_ub_diff[current_var];
          }
          var_lb_to_ub_diff[current_var] = 0;
          var_lb_to_ub_diff[NegationOf(current_var)] = 0;

          continue;
        }

        const IntegerValue diff = FloorRatio(IntegerValue(slack - delta),
                                             IntegerValue(coeff_magnitude));
        int64_t new_ub = lb + diff.value();
        if (new_ub < context->MaxOf(current_ref)) {
          // Tricky: If there are holes, we can't just reduce the domain to
          // new_ub if it is not a valid value, so we need to compute the Min()
          // of the intersection.
          new_ub = context->DomainOf(current_ref)
                       .IntersectionWith(
                           Domain(new_ub, std::numeric_limits<int64_t>::max()))
                       .Min();
        }
        if (new_ub < context->MaxOf(current_ref)) {
          context->UpdateRuleStats("domination: reduced ub.");
          if (!context->IntersectDomainWith(current_ref, Domain(lb, new_ub))) {
            return false;
          }

          // We need to update the precomputed quantities.
          const IntegerVariable current_var =
              VarDomination::RefToIntegerVariable(current_ref);
          if (ub_side) {
            CHECK_GE(var_lb_to_ub_diff[current_var], 0);
            max_activity -= var_lb_to_ub_diff[current_var];
          } else {
            CHECK_LE(var_lb_to_ub_diff[current_var], 0);
            min_activity -= var_lb_to_ub_diff[current_var];
          }
          const int64_t new_diff = std::abs(coeff_magnitude * (new_ub - lb));
          if (ub_side) {
            var_lb_to_ub_diff[current_var] = new_diff;
            var_lb_to_ub_diff[NegationOf(current_var)] = -new_diff;
            max_activity += new_diff;
          } else {
            var_lb_to_ub_diff[current_var] = -new_diff;
            var_lb_to_ub_diff[NegationOf(current_var)] = +new_diff;
            min_activity -= new_diff;
          }
        }
      }
    }

    // Restore.
    for (const int ref : ct.linear().vars()) {
      const IntegerVariable ivar = VarDomination::RefToIntegerVariable(ref);
      var_lb_to_ub_diff[ivar] = 0;
      var_lb_to_ub_diff[NegationOf(ivar)] = 0;
    }
  }

  // For any dominance relation still left (i.e. between non-fixed vars), if
  // the variable are Boolean and X is dominated by Y, we can add
  // (X == 1) => (Y = 1). But, as soon as we do that, we break some symmetry
  // and cannot add any incompatible relations.
  //
  // EX: It is possible that X dominate Y and Y dominate X if they are both
  // appearing in exactly the same constraint with the same coefficient.
  //
  // TODO(user): generalize to non Booleans?
  // TODO(user): We always keep adding the same relations. Do that only once!
  int num_added = 0;
  absl::StrongVector<IntegerVariable, bool> increase_is_forbidden(2 * num_vars,
                                                                  false);
  for (int positive_ref = 0; positive_ref < num_vars; ++positive_ref) {
    if (context->IsFixed(positive_ref)) continue;
    if (!context->CanBeUsedAsLiteral(positive_ref)) continue;
    for (const int ref : {positive_ref, NegatedRef(positive_ref)}) {
      const IntegerVariable var = VarDomination::RefToIntegerVariable(ref);
      if (increase_is_forbidden[NegationOf(var)]) continue;
      for (const IntegerVariable dom :
           var_domination.DominatingVariables(ref)) {
        if (increase_is_forbidden[dom]) continue;
        const int dom_ref = VarDomination::IntegerVariableToRef(dom);
        if (context->IsFixed(dom_ref)) continue;
        if (!context->CanBeUsedAsLiteral(dom_ref)) continue;
        ++num_added;
        context->AddImplication(ref, dom_ref);

        // dom-- or var++ are now forbidden.
        increase_is_forbidden[var] = true;
        increase_is_forbidden[NegationOf(dom)] = true;
      }
    }
  }
  if (num_added > 0) {
    VLOG(1) << "Added " << num_added << " domination implications.";
    context->UpdateNewConstraintsVariableUsage();
    context->UpdateRuleStats("domination: added implications", num_added);
  }

  return true;
}

}  // namespace sat
}  // namespace operations_research
