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

#include "ortools/sat/var_domination.h"

#include <stddef.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/meta/type_traits.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "ortools/algorithms/dynamic_partition.h"
#include "ortools/base/hash.h"
#include "ortools/base/logging.h"
#include "ortools/base/mathutil.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/presolve_context.h"
#include "ortools/sat/presolve_util.h"
#include "ortools/sat/solution_crush.h"
#include "ortools/sat/util.h"
#include "ortools/util/affine_relation.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

void VarDomination::Reset(int num_variables) {
  phase_ = 0;
  num_vars_with_negation_ = 2 * num_variables;
  partition_ =
      std::make_unique<SimpleDynamicPartition>(num_vars_with_negation_);

  can_freely_decrease_.assign(num_vars_with_negation_, true);

  shared_buffer_.clear();
  has_initial_candidates_.assign(num_vars_with_negation_, false);
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
  // The rank can be wrong and need to be recomputed because of how we split
  // tmp_ranks_ into spans.
  MakeRankEqualToStartOfPart(span);

  // We made sure all the variable belongs to the same part.
  if (DEBUG_MODE) {
    if (span.empty()) return;
    const int part = partition_->PartOf(span[0].var.value());
    for (int i = 1; i < span.size(); ++i) {
      CHECK_EQ(part, partition_->PartOf(span[i].var.value()));
    }
  }

  const int future_start = shared_buffer_.size();
  int first_start = -1;
  const int size = span.size();
  for (int i = 0; i < size; ++i) {
    const IntegerVariableWithRank entry = span[i];
    const int num_candidates = size - entry.rank;

    // Ignore if we already have a shorter list.
    if (has_initial_candidates_[entry.var]) {
      if (num_candidates >= initial_candidates_[entry.var].size) continue;
    }

    if (first_start == -1) first_start = entry.rank;
    has_initial_candidates_[entry.var] = true;
    initial_candidates_[entry.var] = {
        future_start - first_start + static_cast<int>(entry.rank),
        num_candidates};
  }

  // Only store what is necessary.
  // Note that since we share, we will never store more than the problem size.
  if (first_start == -1) return;
  for (int i = first_start; i < size; ++i) {
    shared_buffer_.push_back(span[i].var);
  }
}

// TODO(user): Use more heuristics to not miss as much dominance relation when
// we crop initial lists.
bool VarDomination::EndFirstPhase() {
  CHECK_EQ(phase_, 0);
  phase_ = 1;

  // Some initial lists ar too long and will be cropped to this size.
  // We will handle them slightly differently.
  //
  // TODO(user): Tune the initial size, 50 might be a bit large, since our
  // complexity is borned by this number times the number of entries in the
  // constraints. Still we should in most situation be a lot lower than that.
  const int kMaxInitialSize = 50;
  absl::btree_set<IntegerVariable> cropped_vars;
  util_intops::StrongVector<IntegerVariable, bool> is_cropped(
      num_vars_with_negation_, false);

  // Fill the initial domination candidates.
  int non_cropped_size = 0;
  std::vector<IntegerVariable> partition_data;
  const std::vector<absl::Span<const IntegerVariable>> elements_by_part =
      partition_->GetParts(&partition_data);
  for (IntegerVariable var(0); var < num_vars_with_negation_; ++var) {
    if (can_freely_decrease_[var]) continue;

    const int part = partition_->PartOf(var.value());
    const int start = buffer_.size();
    const uint64_t var_sig = block_down_signatures_[var];
    const uint64_t not_var_sig = block_down_signatures_[NegationOf(var)];
    absl::Span<const IntegerVariable> to_scan =
        has_initial_candidates_[var] ? InitialDominatingCandidates(var)
                                     : elements_by_part[part];

    // Two modes, either we scan the full list, or a small subset of it.
    // Not that low variable indices should appear first, so it is better not
    // to randomize.
    int new_size = 0;
    if (to_scan.size() <= 1'000) {
      for (const IntegerVariable x : to_scan) {
        if (var_sig & ~block_down_signatures_[x]) continue;  // !included.
        if (block_down_signatures_[NegationOf(x)] & ~not_var_sig) continue;
        if (PositiveVariable(x) == PositiveVariable(var)) continue;
        if (can_freely_decrease_[NegationOf(x)]) continue;
        ++new_size;
        buffer_.push_back(x);
        if (new_size >= kMaxInitialSize) {
          is_cropped[var] = true;
          cropped_vars.insert(var);
        }
      }
    } else {
      is_cropped[var] = true;
      cropped_vars.insert(var);
      for (int i = 0; i < 200; ++i) {
        const IntegerVariable x = to_scan[i];
        if (var_sig & ~block_down_signatures_[x]) continue;  // !included.
        if (block_down_signatures_[NegationOf(x)] & ~not_var_sig) continue;
        if (PositiveVariable(x) == PositiveVariable(var)) continue;
        if (can_freely_decrease_[NegationOf(x)]) continue;
        ++new_size;
        buffer_.push_back(x);
        if (new_size >= kMaxInitialSize) break;
      }
    }

    if (!is_cropped[var]) non_cropped_size += new_size;
    dominating_vars_[var] = {start, new_size};
  }

  // Heuristic: To try not to remove domination relations corresponding to short
  // lists during transposition (see EndSecondPhase()), we fill the cropped list
  // with the transpose of the short list relations. This helps finding more
  // relation in the presence of cropped lists.
  //
  // Compute how many extra space we need for transposed values.
  // Note that it cannot be more than twice.
  int total_extra_space = 0;
  util_intops::StrongVector<IntegerVariable, int> extra_space(
      num_vars_with_negation_, 0);
  for (IntegerVariable var(0); var < num_vars_with_negation_; ++var) {
    for (const IntegerVariable dom : DominatingVariables(var)) {
      if (!is_cropped[NegationOf(dom)]) continue;
      ++total_extra_space;
      extra_space[NegationOf(dom)]++;
    }
  }

  // Copy into a new buffer.
  int copy_index = 0;
  other_buffer_.resize(buffer_.size() + total_extra_space);
  for (IntegerVariable var(0); var < num_vars_with_negation_; ++var) {
    IntegerVariableSpan& s = dominating_vars_[var];
    for (int i = 0; i < s.size; ++i) {
      other_buffer_[copy_index + i] = buffer_[s.start + i];
    }
    s.start = copy_index;
    copy_index += s.size + extra_space[var];
    extra_space[var] = s.size;  // Used below.
  }
  DCHECK_EQ(copy_index, other_buffer_.size());
  std::swap(buffer_, other_buffer_);
  gtl::STLClearObject(&other_buffer_);

  // Fill the free spaces with transposed values.
  // But do not use new values !
  for (IntegerVariable var(0); var < num_vars_with_negation_; ++var) {
    const int start = dominating_vars_[var].start;
    const int size = extra_space[var];
    for (int i = 0; i < size; ++i) {
      const IntegerVariable dom = buffer_[start + i];
      if (!is_cropped[NegationOf(dom)]) continue;
      IntegerVariableSpan& s = dominating_vars_[NegationOf(dom)];
      buffer_[s.start + s.size++] = NegationOf(var);
    }
  }

  // Remove any duplicates.
  //
  // TODO(user): Maybe we should do that with all lists in case the
  // input function are called with duplicates too.
  for (const IntegerVariable var : cropped_vars) {
    DCHECK(is_cropped[var]);
    IntegerVariableSpan& s = dominating_vars_[var];
    if (s.size == 0) continue;
    IntegerVariable* pt = &buffer_[s.start];
    std::sort(pt, pt + s.size);
    const auto end = std::unique(pt, pt + s.size);
    s.size = end - pt;
  }

  // We no longer need the first phase memory.
  VLOG(1) << "Num initial list that where cropped: "
          << FormatCounter(cropped_vars.size());
  VLOG(1) << "Shared buffer size: " << FormatCounter(shared_buffer_.size());
  VLOG(1) << "Non-cropped buffer size: " << FormatCounter(non_cropped_size);
  VLOG(1) << "Buffer size: " << FormatCounter(buffer_.size());
  gtl::STLClearObject(&initial_candidates_);
  gtl::STLClearObject(&shared_buffer_);

  // A second phase is only needed if there are some potential dominance
  // relations.
  return !buffer_.empty();
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

  VLOG(1) << "Transpose removed " << FormatCounter(num_removed);
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
    // The only variables that can be paired with a var-- in the constraints are
    // the var++ in the constraints with the same rank or higher.
    //
    // Note that we only filter the var-- domination lists here, we do not
    // remove the var-- appearing in all the lists corresponding to wrong var++.
    // This is left to the transpose operation in EndSecondPhase().
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
  // Optim: We cache pointers to avoid refetching them in the loop.
  IntegerValue* bounds = can_freely_decrease_until_.data();
  int* locks = num_locks_.data();
  int* locking_index = locking_ct_index_.data();
  for (const int ref : refs) {
    const int var = RefToIntegerVariable(ref).value();
    bounds[var] = kMaxIntegerValue;
    locks[var]++;
    locking_index[var] = ct_index;
  }
}

void DualBoundStrengthening::CannotIncrease(absl::Span<const int> refs,
                                            int ct_index) {
  // Optim: We cache pointers to avoid refetching them in the loop.
  IntegerValue* bounds = can_freely_decrease_until_.data();
  int* locks = num_locks_.data();
  int* locking_index = locking_ct_index_.data();
  for (const int ref : refs) {
    const int negated_var = NegationOf(RefToIntegerVariable(ref)).value();
    bounds[negated_var] = kMaxIntegerValue;
    locks[negated_var]++;
    locking_index[negated_var] = ct_index;
  }
}

void DualBoundStrengthening::CannotMove(absl::Span<const int> refs,
                                        int ct_index) {
  // Optim: We cache pointers to avoid refetching them in the loop.
  IntegerValue* bounds = can_freely_decrease_until_.data();
  int* locks = num_locks_.data();
  int* locking_index = locking_ct_index_.data();
  for (const int ref : refs) {
    const IntegerVariable var = RefToIntegerVariable(ref);
    const IntegerVariable minus_var = NegationOf(var);
    bounds[var.value()] = kMaxIntegerValue;
    bounds[minus_var.value()] = kMaxIntegerValue;
    locks[var.value()]++;
    locks[minus_var.value()]++;
    locking_index[var.value()] = ct_index;
    locking_index[minus_var.value()] = ct_index;
  }
}

template <typename LinearProto>
void DualBoundStrengthening::ProcessLinearConstraint(
    bool is_objective, const PresolveContext& context,
    const LinearProto& linear, int64_t min_activity, int64_t max_activity,
    int ct_index) {
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
      locking_ct_index_[var] = ct_index;
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
      locking_ct_index_[NegationOf(var)] = ct_index;
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

namespace {

// This is used to detect if two linear constraint are equivalent if the literal
// ref is mapped to another value. We fill a vector that will only be equal
// to another such vector if the two constraint differ only there.
void TransformLinearWithSpecialBoolean(const ConstraintProto& ct, int ref,
                                       std::vector<int64_t>* output) {
  DCHECK_EQ(ct.constraint_case(), ConstraintProto::kLinear);
  output->clear();

  // Deal with enforcement.
  // We only detect NegatedRef() here.
  if (!ct.enforcement_literal().empty()) {
    output->push_back(ct.enforcement_literal().size());
    for (const int literal : ct.enforcement_literal()) {
      if (literal == NegatedRef(ref)) {
        output->push_back(std::numeric_limits<int32_t>::max());  // Sentinel
      } else {
        output->push_back(literal);
      }
    }
  }

  // Deal with linear part.
  // We look for both literal and not(literal) here.
  int64_t offset = 0;
  output->push_back(ct.linear().vars().size());
  for (int i = 0; i < ct.linear().vars().size(); ++i) {
    const int v = ct.linear().vars(i);
    const int64_t c = ct.linear().coeffs(i);
    if (v == ref) {
      output->push_back(std::numeric_limits<int32_t>::max());  // Sentinel
      output->push_back(c);
    } else if (v == NegatedRef(ref)) {
      // c * v = -c * (1 - v) + c
      output->push_back(std::numeric_limits<int32_t>::max());  // Sentinel
      output->push_back(-c);
      offset += c;
    } else {
      output->push_back(v);
      output->push_back(c);
    }
  }

  // Domain.
  for (const int64_t value : ct.linear().domain()) {
    output->push_back(CapSub(value, offset));
  }
}

}  // namespace

bool DualBoundStrengthening::Strengthen(PresolveContext* context) {
  SolutionCrush& crush = context->solution_crush();
  num_deleted_constraints_ = 0;
  const CpModelProto& cp_model = *context->working_model;
  const int num_vars = cp_model.variables_size();
  int64_t num_fixed_vars = 0;
  for (int var = 0; var < num_vars; ++var) {
    if (context->IsFixed(var)) continue;

    // Fix to lb?
    const int64_t lb = context->MinOf(var);
    const int64_t ub_limit = std::max(lb, CanFreelyDecreaseUntil(var));
    if (ub_limit == lb) {
      ++num_fixed_vars;
      CHECK(context->IntersectDomainWith(var, Domain(lb)));
      continue;
    }

    // Fix to ub?
    const int64_t ub = context->MaxOf(var);
    const int64_t lb_limit =
        std::min(ub, -CanFreelyDecreaseUntil(NegatedRef(var)));
    if (lb_limit == ub) {
      ++num_fixed_vars;
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
        ++num_fixed_vars;
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
  if (num_fixed_vars > 0) {
    context->UpdateRuleStats("dual: fix variable", num_fixed_vars);
  }

  // For detecting near-duplicate constraint that can be made equivalent.
  // hash -> (ct_index, modified ref).
  absl::flat_hash_set<int> equiv_ct_index_set;
  absl::flat_hash_map<uint64_t, std::pair<int, int>> equiv_modified_constraints;
  std::vector<int64_t> temp_data;
  std::vector<int64_t> other_temp_data;
  std::string s;

  // If there is only one blocking constraint, we can simplify the problem in
  // a few situation.
  //
  // TODO(user): Cover all the cases.
  std::vector<bool> processed(num_vars, false);
  int64_t num_bool_in_near_duplicate_ct = 0;
  for (IntegerVariable var(0); var < num_locks_.size(); ++var) {
    const int ref = VarDomination::IntegerVariableToRef(var);
    const int positive_ref = PositiveRef(ref);
    if (processed[positive_ref]) continue;
    if (context->IsFixed(positive_ref)) continue;
    if (context->VariableIsNotUsedAnymore(positive_ref)) continue;
    if (context->VariableWasRemoved(positive_ref)) continue;

    if (num_locks_[var] != 1) continue;
    if (locking_ct_index_[var] == -1) {
      context->UpdateRuleStats(
          "TODO dual: only one unspecified blocking constraint?");
      continue;
    }

    const int ct_index = locking_ct_index_[var];
    const ConstraintProto& ct = context->working_model->constraints(ct_index);
    if (ct.constraint_case() == ConstraintProto::CONSTRAINT_NOT_SET) {
      // TODO(user): Fix variable right away rather than waiting for next call.
      continue;
    }
    if (ct.constraint_case() == ConstraintProto::kAtMostOne) {
      context->UpdateRuleStats("TODO dual: tighten at most one");
      continue;
    }

    if (ct.constraint_case() != ConstraintProto::kBoolAnd) {
      // If we have an enforcement literal then we can always add the
      // implication "not enforced" => var at its lower bound.
      // If we also have enforced => fixed var, then var is in affine relation
      // with the enforced literal and we can remove one variable.
      //
      // TODO(user): We can also deal with more than one enforcement.
      if (ct.enforcement_literal().size() == 1 &&
          PositiveRef(ct.enforcement_literal(0)) != positive_ref) {
        const int enf = ct.enforcement_literal(0);
        const int64_t bound = RefIsPositive(ref) ? context->MinOf(positive_ref)
                                                 : context->MaxOf(positive_ref);
        const Domain implied =
            context->DomainOf(positive_ref)
                .IntersectionWith(
                    context->deductions.ImpliedDomain(enf, positive_ref));
        if (implied.IsEmpty()) {
          context->UpdateRuleStats("dual: fix variable");
          if (!context->SetLiteralToFalse(enf)) return false;
          if (!context->IntersectDomainWith(positive_ref, Domain(bound))) {
            return false;
          }
          continue;
        }
        if (implied.IsFixed()) {
          // Corner case.
          if (implied.FixedValue() == bound) {
            context->UpdateRuleStats("dual: fix variable");
            if (!context->IntersectDomainWith(positive_ref, implied)) {
              return false;
            }
            continue;
          }

          // Note(user): If we have enforced => var fixed, we could actually
          // just have removed var from the constraint it was implied by
          // another constraint. If not, because of the new affine relation we
          // could remove it right away.
          processed[PositiveRef(enf)] = true;
          processed[positive_ref] = true;
          context->UpdateRuleStats("dual: affine relation");
          // The new affine relation added below can break the hint if hint(enf)
          // is 0. In this case the only constraint blocking `ref` from
          // decreasing [`ct` = enf => (var = implied)] does not apply. We can
          // thus set the hint of `positive_ref` to `bound` to preserve the hint
          // feasibility.
          crush.SetVarToValueIf(positive_ref, bound, NegatedRef(enf));
          if (RefIsPositive(enf)) {
            // positive_ref = enf * implied + (1 - enf) * bound.
            if (!context->StoreAffineRelation(
                    positive_ref, enf, implied.FixedValue() - bound, bound)) {
              return false;
            }
          } else {
            // enf_var = PositiveRef(enf).
            // positive_ref = (1 - enf_var) * implied + enf_var * bound.
            if (!context->StoreAffineRelation(positive_ref, PositiveRef(enf),
                                              bound - implied.FixedValue(),
                                              implied.FixedValue())) {
              return false;
            }
          }
          continue;
        }

        if (context->CanBeUsedAsLiteral(positive_ref)) {
          // If we have a literal, we always add the implication.
          // This seems like a good thing to do.
          processed[PositiveRef(enf)] = true;
          processed[positive_ref] = true;
          context->UpdateRuleStats("dual: add implication");
          // The new implication can break the hint if hint(enf) is 0 and
          // hint(ref) is 1. In this case the only locking constraint `ct` does
          // not apply and thus does not prevent decreasing the hint of ref in
          // order to preserve the hint feasibility.
          crush.SetLiteralToValueIf(ref, false, NegatedRef(enf));
          context->AddImplication(NegatedRef(enf), NegatedRef(ref));
          context->UpdateNewConstraintsVariableUsage();
          continue;
        }

        // We can add an implication not_enforced => var to its bound ?
        context->UpdateRuleStats("TODO dual: add implied bound");
      }

      // We can make enf equivalent to the constraint instead of just =>. This
      // seems useful since internally we always use fully reified encoding.
      if (ct.constraint_case() == ConstraintProto::kLinear &&
          ct.linear().vars().size() == 1 &&
          ct.enforcement_literal().size() == 1 &&
          ct.enforcement_literal(0) == NegatedRef(ref)) {
        const int var = ct.linear().vars(0);
        const Domain var_domain = context->DomainOf(var);
        const Domain rhs = ReadDomainFromProto(ct.linear())
                               .InverseMultiplicationBy(ct.linear().coeffs(0))
                               .IntersectionWith(var_domain);
        if (rhs.IsEmpty()) {
          context->UpdateRuleStats("linear1: infeasible");
          if (!context->SetLiteralToFalse(ct.enforcement_literal(0))) {
            return false;
          }
          processed[PositiveRef(ref)] = true;
          processed[PositiveRef(var)] = true;
          context->working_model->mutable_constraints(ct_index)->Clear();
          context->UpdateConstraintVariableUsage(ct_index);
          continue;
        }
        if (rhs == var_domain) {
          context->UpdateRuleStats("linear1: always true");
          processed[PositiveRef(ref)] = true;
          processed[PositiveRef(var)] = true;
          context->working_model->mutable_constraints(ct_index)->Clear();
          context->UpdateConstraintVariableUsage(ct_index);
          continue;
        }

        const Domain complement = rhs.Complement().IntersectionWith(var_domain);
        if (rhs.IsFixed() || complement.IsFixed()) {
          context->UpdateRuleStats("dual: make encoding equiv");
          const int64_t value =
              rhs.IsFixed() ? rhs.FixedValue() : complement.FixedValue();
          int encoding_lit;
          if (context->HasVarValueEncoding(var, value, &encoding_lit)) {
            // If it is different, we have an equivalence now, and we can
            // remove the constraint.
            if (rhs.IsFixed()) {
              if (encoding_lit == NegatedRef(ref)) continue;
              // Extending `ct` = "not(ref) => encoding_lit" to an equality can
              // break the hint only if hint(ref) = hint(encoding_lit) = 1. But
              // in this case `ct` is actually not blocking ref from decreasing.
              // We can thus set its hint to 0 to preserve the hint feasibility.
              crush.SetLiteralToValueIf(ref, false, encoding_lit);
              if (!context->StoreBooleanEqualityRelation(encoding_lit,
                                                         NegatedRef(ref))) {
                return false;
              }
            } else {
              if (encoding_lit == ref) continue;
              // Extending `ct` = "not(ref) => not(encoding_lit)" to an equality
              // can break the hint only if hint(encoding_lit) = 0 and hint(ref)
              // = 1. But in this case `ct` is actually not blocking ref from
              // decreasing. We can thus set its hint to 0 to preserve the hint
              // feasibility.
              crush.SetLiteralToValueIf(ref, false, NegatedRef(encoding_lit));
              if (!context->StoreBooleanEqualityRelation(encoding_lit, ref)) {
                return false;
              }
            }
            context->working_model->mutable_constraints(ct_index)->Clear();
            context->UpdateConstraintVariableUsage(ct_index);
            processed[PositiveRef(ref)] = true;
            processed[PositiveRef(var)] = true;
            // `encoding_lit` was maybe a new variable added during this loop,
            // so make sure we cannot go out-of-bound.
            if (PositiveRef(encoding_lit) < processed.size()) {
              processed[PositiveRef(encoding_lit)] = true;
            }
            continue;
          }

          processed[PositiveRef(ref)] = true;
          processed[PositiveRef(var)] = true;
          // The `new_ct` constraint `ref` => (`var` in `complement`) below can
          // break the hint if hint(var) is not in `complement`. In this case,
          // set the hint of `ref` to false. This should be safe since the only
          // constraint blocking `ref` from decreasing is `ct` = not(ref) =>
          // (`var` in `rhs`) -- which does not apply when `ref` is true.
          crush.SetLiteralToValueIfLinearConstraintViolated(
              ref, false, {{var, 1}}, complement);
          ConstraintProto* new_ct = context->working_model->add_constraints();
          new_ct->add_enforcement_literal(ref);
          new_ct->mutable_linear()->add_vars(var);
          new_ct->mutable_linear()->add_coeffs(1);
          FillDomainInProto(complement, new_ct->mutable_linear());
          context->UpdateNewConstraintsVariableUsage();

          if (rhs.IsFixed()) {
            context->StoreLiteralImpliesVarEqValue(NegatedRef(ref), var, value);
            context->StoreLiteralImpliesVarNEqValue(ref, var, value);
          } else if (complement.IsFixed()) {
            context->StoreLiteralImpliesVarNEqValue(NegatedRef(ref), var,
                                                    value);
            context->StoreLiteralImpliesVarEqValue(ref, var, value);
          }
          continue;
        }
      }

      // If we have two Booleans, each with a single blocking constraint that is
      // the same if we "swap" the Booleans reference, then we can make the
      // Booleans equivalent. This is because they will be forced to their bad
      // value only if it is needed by the other part of their respective
      // blocking constraints. This is related to the
      // FindAlmostIdenticalLinearConstraints() except that here we can deal
      // with non-equality or enforced constraints.
      //
      // TODO(user): Generalize to non-Boolean. Also for Boolean, we might
      // miss some possible reduction if replacing X by 1 - X make a constraint
      // near-duplicate of another.
      //
      // TODO(user): We can generalize to non-linear constraint.
      //
      // Optimization: Skip if this constraint was already used as a single
      // blocking constraint of another variable. If this is the case, it cannot
      // be equivalent to another constraint with "var" substituted, since
      // otherwise var would have two blocking constraint. We can safely skip
      // it. this make sure this is in O(num_entries) and not more.
      if (equiv_ct_index_set.insert(ct_index).second &&
          ct.constraint_case() == ConstraintProto::kLinear &&
          context->CanBeUsedAsLiteral(ref)) {
        TransformLinearWithSpecialBoolean(ct, ref, &temp_data);
        const uint64_t hash =
            fasthash64(temp_data.data(), temp_data.size() * sizeof(int64_t),
                       uint64_t{0xa5b85c5e198ed849});
        const auto [it, inserted] =
            equiv_modified_constraints.insert({hash, {ct_index, ref}});
        if (!inserted) {
          // Already present!
          const auto [other_c_with_same_hash, other_ref] = it->second;
          CHECK_NE(other_c_with_same_hash, ct_index);
          const auto& other_ct =
              context->working_model->constraints(other_c_with_same_hash);
          TransformLinearWithSpecialBoolean(other_ct, other_ref,
                                            &other_temp_data);
          if (temp_data == other_temp_data) {
            // Corner case: We just discovered l => ct and not(l) => ct.
            // So ct must just always be true. And since that was the only
            // blocking constraint for l, we can just set l to an arbitrary
            // value.
            if (ref == NegatedRef(other_ref)) {
              context->UpdateRuleStats(
                  "dual: detected l => ct and not(l) => ct with unused l!");
              if (!context->IntersectDomainWith(ref, Domain(0))) {
                return false;
              }
              continue;
            }

            // We have a true equality. The two ref can be made equivalent.
            if (!processed[PositiveRef(other_ref)]) {
              ++num_bool_in_near_duplicate_ct;
              processed[PositiveRef(ref)] = true;
              processed[PositiveRef(other_ref)] = true;
              // If the two hints are different, and since both refs have an
              // equivalent blocking constraint, then the constraint is actually
              // not blocking the ref at 1 from decreasing. Hence we can set its
              // hint to false to preserve the hint feasibility despite the new
              // Boolean equality constraint.
              crush.UpdateLiteralsToFalseIfDifferent(ref, other_ref);
              if (!context->StoreBooleanEqualityRelation(ref, other_ref)) {
                return false;
              }

              // We can delete one of the constraint since they are duplicate
              // now.
              ++num_deleted_constraints_;
              context->working_model->mutable_constraints(ct_index)->Clear();
              context->UpdateConstraintVariableUsage(ct_index);
              continue;
            }
          }
        }
      }

      // Other potential cases?
      if (!ct.enforcement_literal().empty()) {
        if (ct.constraint_case() == ConstraintProto::kLinear &&
            ct.linear().vars().size() == 1 &&
            ct.enforcement_literal().size() == 1 &&
            ct.enforcement_literal(0) == NegatedRef(ref)) {
          context->UpdateRuleStats("TODO dual: make linear1 equiv");
        } else {
          context->UpdateRuleStats(
              "TODO dual: only one blocking enforced constraint?");
        }
      } else {
        context->UpdateRuleStats("TODO dual: only one blocking constraint?");
      }
      continue;
    }
    if (ct.enforcement_literal().size() != 1) continue;

    // If (a => b) is the only constraint blocking a literal a in the up
    // direction, then we can set a == b !
    //
    // Recover a => b where a is having an unique up_lock (i.e this constraint).
    // Note that if many implications are encoded in the same bool_and, we have
    // to be careful that a is appearing in just one of them.
    //
    // TODO(user): Make sure implication graph is transitively reduced to not
    // miss such reduction. More generally, this might only use the graph rather
    // than the encoding into bool_and / at_most_one ? Basically if a =>
    // all_direct_deduction, we can transform it into a <=> all_direct_deduction
    // if that is interesting. This could always be done on a max-2sat problem
    // in one of the two direction. Also think about max-2sat specific presolve.
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
    // If hint(a) is false we can always set it to hint(b) since this can only
    // increase its value. If hint(a) is true then hint(b) must be true as well
    // if the hint is feasible, due to the a => b constraint. Setting hint(a) to
    // hint(b) is thus always safe. The opposite is true as well.
    crush.MakeLiteralsEqual(a, b);
    if (!context->StoreBooleanEqualityRelation(a, b)) return false;
    context->UpdateRuleStats("dual: enforced equivalence");
  }

  if (num_bool_in_near_duplicate_ct) {
    context->UpdateRuleStats(
        "dual: equivalent Boolean in near-duplicate constraints",
        num_bool_in_near_duplicate_ct);
  }

  VLOG(2) << "Num deleted constraints: " << num_deleted_constraints_;
  return true;
}

void ScanModelForDominanceDetection(PresolveContext& context,
                                    VarDomination* var_domination) {
  if (context.ModelIsUnsat()) return;

  const CpModelProto& cp_model = *context.working_model;
  const int num_vars = cp_model.variables().size();
  var_domination->Reset(num_vars);

  for (int var = 0; var < num_vars; ++var) {
    // Ignore variables that have been substituted already or are unused.
    if (context.IsFixed(var) || context.VariableWasRemoved(var) ||
        context.VariableIsNotUsedAnymore(var)) {
      var_domination->CanOnlyDominateEachOther({var});
      continue;
    }

    // Deal with the affine relations that are not part of the proto.
    // Those only need to be processed in the first pass.
    const AffineRelation::Relation r = context.GetAffineRelation(var);
    if (r.representative != var) {
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
  }

  // First scan: update the partition.
  const int num_constraints = cp_model.constraints_size();
  std::vector<bool> c_is_free_to_increase(num_constraints);
  std::vector<bool> c_is_free_to_decrease(num_constraints);
  for (int c = 0; c < num_constraints; ++c) {
    const ConstraintProto& ct = cp_model.constraints(c);
    switch (ct.constraint_case()) {
      case ConstraintProto::kBoolOr:
        break;
      case ConstraintProto::kBoolAnd:
        break;
      case ConstraintProto::kAtMostOne:
        break;
      case ConstraintProto::kExactlyOne:
        var_domination->ActivityShouldNotChange(ct.exactly_one().literals(),
                                                /*coeffs=*/{});
        break;
      case ConstraintProto::kLinear: {
        // TODO(user): Maybe we should avoid recomputing that here.
        const auto [min_activity, max_activity] =
            context.ComputeMinMaxActivity(ct.linear());
        const bool domain_is_simple = ct.linear().domain().size() == 2;
        const bool free_to_increase =
            domain_is_simple && ct.linear().domain(1) >= max_activity;
        const bool free_to_decrease =
            domain_is_simple && ct.linear().domain(0) <= min_activity;
        c_is_free_to_increase[c] = free_to_increase;
        c_is_free_to_decrease[c] = free_to_decrease;
        if (free_to_decrease && free_to_increase) break;
        if (!free_to_increase && !free_to_decrease) {
          var_domination->ActivityShouldNotChange(ct.linear().vars(),
                                                  ct.linear().coeffs());
        }
        break;
      }
      default:
        // We cannot infer anything if we don't know the constraint.
        // TODO(user): Handle enforcement better here.
        for (const int var : context.ConstraintToVars(c)) {
          var_domination->CanOnlyDominateEachOther({var});
        }
        break;
    }
  }

  // The objective is handled like a <= constraints, or an == constraint if
  // there is a non-trivial domain.
  if (cp_model.has_objective()) {
    // Important: We need to write the objective first to make sure it is up to
    // date.
    context.WriteObjectiveToProto();
    const auto [min_activity, max_activity] =
        context.ComputeMinMaxActivity(cp_model.objective());
    const auto& domain = cp_model.objective().domain();
    if (domain.empty() || (domain.size() == 2 && domain[0] <= min_activity)) {
      // do nothing for now.
    } else {
      var_domination->ActivityShouldNotChange(cp_model.objective().vars(),
                                              cp_model.objective().coeffs());
    }
  }

  // Now do two more scan.
  // - the phase_ = 0 initialize candidate list, then EndFirstPhase()
  // - the phase_ = 1 filter them, then EndSecondPhase();
  std::vector<int> tmp;
  for (int phase = 0; phase < 2; phase++) {
    for (int c = 0; c < num_constraints; ++c) {
      const ConstraintProto& ct = cp_model.constraints(c);
      switch (ct.constraint_case()) {
        case ConstraintProto::kBoolOr:
          var_domination->ActivityShouldNotDecrease(ct.enforcement_literal(),
                                                    ct.bool_or().literals(),
                                                    /*coeffs=*/{});
          break;
        case ConstraintProto::kBoolAnd:
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
          var_domination->ActivityShouldNotIncrease(ct.enforcement_literal(),
                                                    ct.at_most_one().literals(),
                                                    /*coeffs=*/{});
          break;
        case ConstraintProto::kLinear: {
          const bool free_to_increase = c_is_free_to_increase[c];
          const bool free_to_decrease = c_is_free_to_decrease[c];
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
            if (!ct.enforcement_literal().empty()) {
              var_domination->ActivityShouldNotIncrease(
                  /*enforcements=*/{}, ct.enforcement_literal(), /*coeffs=*/{});
            }
          }
          break;
        }
        default:
          break;
      }
    }

    // The objective is handled like a <= constraints, or an == constraint if
    // there is a non-trivial domain.
    if (cp_model.has_objective()) {
      const auto [min_activity, max_activity] =
          context.ComputeMinMaxActivity(cp_model.objective());
      const auto& domain = cp_model.objective().domain();
      if (domain.empty() || (domain.size() == 2 && domain[0] <= min_activity)) {
        var_domination->ActivityShouldNotIncrease(
            /*enforcements=*/{}, cp_model.objective().vars(),
            cp_model.objective().coeffs());
      }
    }

    if (phase == 0) {
      // Early abort if no possible relations can be found.
      //
      // TODO(user): We might be able to detect that nothing can be done earlier
      // during the constraint scanning.
      if (!var_domination->EndFirstPhase()) return;
    } else {
      CHECK_EQ(phase, 1);
      var_domination->EndSecondPhase();
    }
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

void ScanModelForDualBoundStrengthening(
    const PresolveContext& context,
    DualBoundStrengthening* dual_bound_strengthening) {
  if (context.ModelIsUnsat()) return;
  const CpModelProto& cp_model = *context.working_model;
  const int num_vars = cp_model.variables().size();
  dual_bound_strengthening->Reset(num_vars);

  for (int var = 0; var < num_vars; ++var) {
    // Ignore variables that have been substituted already or are unused.
    if (context.IsFixed(var) || context.VariableWasRemoved(var) ||
        context.VariableIsNotUsedAnymore(var)) {
      dual_bound_strengthening->CannotMove({var});
      continue;
    }

    // Deal with the affine relations that are not part of the proto.
    // Those only need to be processed in the first pass.
    const AffineRelation::Relation r = context.GetAffineRelation(var);
    if (r.representative != var) {
      dual_bound_strengthening->CannotMove({var, r.representative});
    }
  }

  const int num_constraints = cp_model.constraints_size();
  for (int c = 0; c < num_constraints; ++c) {
    const ConstraintProto& ct = cp_model.constraints(c);
    dual_bound_strengthening->CannotIncrease(ct.enforcement_literal(), c);
    switch (ct.constraint_case()) {
      case ConstraintProto::kBoolOr:
        dual_bound_strengthening->CannotDecrease(ct.bool_or().literals(), c);
        break;
      case ConstraintProto::kBoolAnd:
        dual_bound_strengthening->CannotDecrease(ct.bool_and().literals(), c);
        break;
      case ConstraintProto::kAtMostOne:
        dual_bound_strengthening->CannotIncrease(ct.at_most_one().literals(),
                                                 c);
        break;
      case ConstraintProto::kExactlyOne:
        dual_bound_strengthening->CannotMove(ct.exactly_one().literals(), c);
        break;
      case ConstraintProto::kLinear: {
        // TODO(user): Maybe we should avoid recomputing that here.
        const auto [min_activity, max_activity] =
            context.ComputeMinMaxActivity(ct.linear());
        dual_bound_strengthening->ProcessLinearConstraint(
            false, context, ct.linear(), min_activity, max_activity, c);
        break;
      }
      default:
        // We cannot infer anything if we don't know the constraint.
        // TODO(user): Handle enforcement better here.
        dual_bound_strengthening->CannotMove(context.ConstraintToVars(c), c);
        break;
    }
  }

  // The objective is handled like a <= constraints, or an == constraint if
  // there is a non-trivial domain.
  if (cp_model.has_objective()) {
    // WARNING: The proto objective might not be up to date, so we need to
    // write it first.
    context.WriteObjectiveToProto();
    const auto [min_activity, max_activity] =
        context.ComputeMinMaxActivity(cp_model.objective());
    dual_bound_strengthening->ProcessLinearConstraint(
        true, context, cp_model.objective(), min_activity, max_activity);
  }
}

namespace {
// Decrements the solution hint of `ref` by the minimum amount necessary to be
// in `domain`, and increments the solution hint of one or more
// `dominating_variables` by the same total amount (or less if it is not
// possible to exactly match this amount).
//
// `domain` must be an interval with the same lower bound as `ref`'s current
// domain D in `context`, and whose upper bound must be in D.
void MaybeUpdateRefHintFromDominance(
    PresolveContext& context, int ref, const Domain& domain,
    absl::Span<const IntegerVariable> dominating_variables) {
  DCHECK_EQ(domain.NumIntervals(), 1);
  DCHECK_EQ(domain.Min(), context.DomainOf(ref).Min());
  DCHECK(context.DomainOf(ref).Contains(domain.Max()));
  std::vector<std::pair<int, Domain>> dominating_refs;
  dominating_refs.reserve(dominating_variables.size());
  for (const IntegerVariable var : dominating_variables) {
    const int dominating_ref = VarDomination::IntegerVariableToRef(var);
    dominating_refs.push_back(
        {dominating_ref, context.DomainOf(dominating_ref)});
  }
  context.solution_crush().UpdateRefsWithDominance(
      ref, domain.Min(), domain.Max(), dominating_refs);
}

bool ProcessAtMostOne(
    absl::Span<const int> literals, const std::string& message,
    const VarDomination& var_domination,
    util_intops::StrongVector<IntegerVariable, bool>* in_constraints,
    PresolveContext* context) {
  for (const int ref : literals) {
    (*in_constraints)[VarDomination::RefToIntegerVariable(ref)] = true;
  }
  for (const int ref : literals) {
    if (context->IsFixed(ref)) continue;

    const auto dominating_ivars = var_domination.DominatingVariables(ref);
    for (const IntegerVariable ivar : dominating_ivars) {
      const int iref = VarDomination::IntegerVariableToRef(ivar);
      if (!(*in_constraints)[ivar]) continue;
      if (context->IsFixed(iref)) {
        continue;
      }

      // We can set the dominated variable to false. If the hint value of `ref`
      // is 1 then the hint value of `iref` should be 0 due to the "at most one"
      // constraint. Hence the hint feasibility can always be preserved (if the
      // hint value of `ref` is 0 the hint does not need to be updated).
      context->UpdateRuleStats(message);
      context->solution_crush().UpdateLiteralsWithDominance(ref, iref);
      if (!context->SetLiteralToFalse(ref)) return false;
      break;
    }
  }
  for (const int ref : literals) {
    (*in_constraints)[VarDomination::RefToIntegerVariable(ref)] = false;
  }
  return true;
}

}  // namespace

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

  const int64_t saved_num_operations = context->num_presolve_operations;

  // Strenghtening via domination. When a variable is dominated by a bunch of
  // other, either we can do (var--, dom++) or if we can't (i.e all dominated
  // variable at their upper bound) then maybe all constraint are satisfied if
  // var is high enough and we can also decrease it.
  util_intops::StrongVector<IntegerVariable, int> can_freely_decrease_count(
      num_vars * 2, 0);
  util_intops::StrongVector<IntegerVariable, int64_t> can_freely_decrease_until(
      num_vars * 2, std::numeric_limits<int64_t>::min());

  // Temporary data that we fill/clear for each linear constraint.
  util_intops::StrongVector<IntegerVariable, int64_t> var_lb_to_ub_diff(
      num_vars * 2, 0);

  // Temporary data used for boolean constraints.
  util_intops::StrongVector<IntegerVariable, bool> in_constraints(num_vars * 2,
                                                                  false);

  SolutionCrush& crush = context->solution_crush();
  absl::flat_hash_set<std::pair<int, int>> implications;
  const int num_constraints = cp_model.constraints_size();
  for (int c = 0; c < num_constraints; ++c) {
    const ConstraintProto& ct = cp_model.constraints(c);

    if (ct.constraint_case() == ConstraintProto::kBoolAnd) {
      if (ct.enforcement_literal().size() != 1) continue;
      const int a = ct.enforcement_literal(0);
      if (context->IsFixed(a)) continue;
      for (const int b : ct.bool_and().literals()) {
        if (context->IsFixed(b)) continue;
        implications.insert({a, b});
        implications.insert({NegatedRef(b), NegatedRef(a)});

        // If (a--, b--) is valid, we can always set a to false. If the hint
        // value of `a` is 1 then the hint value of `b` should be 1 due to the
        // a => b constraint. Hence the hint feasibility can always be preserved
        // (if the hint value of `a` is 0 the hint does not need to be updated).
        for (const IntegerVariable ivar :
             var_domination.DominatingVariables(a)) {
          const int ref = VarDomination::IntegerVariableToRef(ivar);
          if (ref == NegatedRef(b)) {
            context->UpdateRuleStats("domination: in implication");
            crush.UpdateLiteralsWithDominance(a, ref);
            if (!context->SetLiteralToFalse(a)) return false;
            break;
          }
        }
        if (context->IsFixed(a)) break;

        // If (b++, a++) is valid, then we can always set b to true. If the hint
        // value of `b` is 0 then the hint value of `a` should be 0 due to the
        // a => b constraint. Hence the hint feasibility can always be preserved
        // (if the hint value of `b` is 1 the hint does not need to be updated).
        for (const IntegerVariable ivar :
             var_domination.DominatingVariables(NegatedRef(b))) {
          const int ref = VarDomination::IntegerVariableToRef(ivar);
          if (ref == a) {
            context->UpdateRuleStats("domination: in implication");
            crush.UpdateLiteralsWithDominance(NegatedRef(b), ref);
            if (!context->SetLiteralToTrue(b)) return false;
            break;
          }
        }
      }
      continue;
    }

    if (!ct.enforcement_literal().empty()) continue;

    // TODO(user): More generally, combine with probing? if a dominated
    // variable implies one of its dominant to zero, then it can be set to
    // zero. It seems adding the implication below should have the same
    // effect? but currently it requires a lot of presolve rounds.
    const auto add_implications = [&implications](absl::Span<const int> lits) {
      if (lits.size() > 3) return;
      for (int i = 0; i < lits.size(); ++i) {
        for (int j = 0; j < lits.size(); ++j) {
          if (i == j) continue;
          implications.insert({lits[i], NegatedRef(lits[j])});
        }
      }
    };
    if (ct.constraint_case() == ConstraintProto::kAtMostOne) {
      add_implications(ct.at_most_one().literals());
      if (!ProcessAtMostOne(ct.at_most_one().literals(),
                            "domination: in at most one", var_domination,
                            &in_constraints, context)) {
        return false;
      }
    } else if (ct.constraint_case() == ConstraintProto::kExactlyOne) {
      add_implications(ct.exactly_one().literals());
      if (!ProcessAtMostOne(ct.exactly_one().literals(),
                            "domination: in exactly one", var_domination,
                            &in_constraints, context)) {
        return false;
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

    // Returns the change magnitude in min-activity (resp. max-activity) if all
    // the given variables are fixed to their upper bound.
    const auto get_delta = [context, &var_lb_to_ub_diff](
                               bool use_min_side,
                               absl::Span<const IntegerVariable> vars) {
      int64_t delta = 0;
      for (const IntegerVariable var : vars) {
        // Tricky: For now we skip complex domain as we are not sure they
        // can be moved correctly.
        if (context->DomainOf(VarDomination::IntegerVariableToRef(var))
                .NumIntervals() != 1) {
          continue;
        }
        if (use_min_side) {
          delta += std::max(int64_t{0}, var_lb_to_ub_diff[var]);
        } else {
          delta += std::max(int64_t{0}, -var_lb_to_ub_diff[var]);
        }
      }
      return delta;
    };

    // Look for dominated var.
    for (int i = 0; i < num_terms; ++i) {
      const int ref = ct.linear().vars(i);
      const int64_t coeff = ct.linear().coeffs(i);
      const int64_t coeff_magnitude = std::abs(coeff);
      if (context->IsFixed(ref)) continue;

      // For strenghtening using domination, just consider >= constraint.
      const bool only_lb = max_activity <= rhs_ub;
      const bool only_ub = min_activity >= rhs_lb;
      if (only_lb || only_ub) {
        // Always transform to coeff_magnitude * current_ref + ... >=
        const int current_ref = (coeff > 0) == only_lb ? ref : NegatedRef(ref);
        const int64_t shifted_rhs =
            only_lb ? rhs_lb - min_activity : max_activity - rhs_ub;
        const IntegerVariable current_ivar =
            VarDomination::RefToIntegerVariable(current_ref);
        can_freely_decrease_count[NegationOf(current_ivar)]++;

        const int64_t delta = get_delta(
            only_lb, var_domination.DominatingVariables(current_ivar));
        if (delta > 0) {
          // When all dominated var are at their upper bound, we miss 'slack'
          // to make the constraint trivially satisfiable.
          const int64_t slack = shifted_rhs - delta;
          const int64_t current_lb = context->MinOf(current_ref);

          // Any increase such that coeff * delta >= slack make the constraint
          // trivial.
          //
          // Note(user): It look like even if any of the upper bound of the
          // dominating var decrease, this should still be valid. Here we only
          // decrease such a bound due to a dominance relation, so the slack
          // when all dominating variable are at their bound should not really
          // decrease.
          const int64_t min_delta =
              slack <= 0 ? 0 : MathUtil::CeilOfRatio(slack, coeff_magnitude);
          can_freely_decrease_until[current_ivar] = std::max(
              can_freely_decrease_until[current_ivar], current_lb + min_delta);
          can_freely_decrease_count[current_ivar]++;
        }
      }

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

        // Compute the delta in min-activity if all dominating var moves to
        // their other bound.
        const int64_t delta = get_delta(ub_side, dominated_by);
        const int64_t lb = context->MinOf(current_ref);
        if (delta + coeff_magnitude > slack) {
          context->UpdateRuleStats("domination: fixed to lb.");
          const Domain reduced_domain = Domain(lb);
          MaybeUpdateRefHintFromDominance(*context, current_ref, reduced_domain,
                                          dominated_by);
          if (!context->IntersectDomainWith(current_ref, reduced_domain)) {
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
          // new_ub if it is not a valid value, so we need to compute the
          // Min() of the intersection.
          new_ub = context->DomainOf(current_ref).ValueAtOrAfter(new_ub);
        }
        if (new_ub < context->MaxOf(current_ref)) {
          context->UpdateRuleStats("domination: reduced ub.");
          const Domain reduced_domain = Domain(lb, new_ub);
          MaybeUpdateRefHintFromDominance(*context, current_ref, reduced_domain,
                                          dominated_by);
          if (!context->IntersectDomainWith(current_ref, reduced_domain)) {
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
  // (X = 1) => (Y = 1). But, as soon as we do that, we break some symmetry
  // and cannot add any incompatible relations.
  //
  // EX: It is possible that X dominate Y and Y dominate X if they are both
  // appearing in exactly the same constraint with the same coefficient.
  //
  // TODO(user): if both variable are in a bool_or, this will allow us to
  // remove the dominated variable. Maybe we should exploit that to decide
  // which implication we add. Or just remove such variable and not add the
  // implications?
  //
  // TODO(user): generalize to non Booleans?
  int num_added = 0;
  util_intops::StrongVector<IntegerVariable, bool> increase_is_forbidden(
      2 * num_vars, false);
  for (int positive_ref = 0; positive_ref < num_vars; ++positive_ref) {
    if (context->IsFixed(positive_ref)) continue;
    if (context->VariableIsNotUsedAnymore(positive_ref)) continue;
    if (context->VariableWasRemoved(positive_ref)) continue;

    // Increase the count for variable in the objective to account for the
    // kObjectiveConstraint in their VarToConstraints() list.
    if (!context->ObjectiveDomainIsConstraining()) {
      const int64_t obj_coeff = context->ObjectiveCoeff(positive_ref);
      if (obj_coeff > 0) {
        can_freely_decrease_count[VarDomination::RefToIntegerVariable(
            positive_ref)]++;
      } else if (obj_coeff < 0) {
        can_freely_decrease_count[NegationOf(
            VarDomination::RefToIntegerVariable(positive_ref))]++;
      }
    }

    for (const int ref : {positive_ref, NegatedRef(positive_ref)}) {
      const IntegerVariable var = VarDomination::RefToIntegerVariable(ref);
      if (increase_is_forbidden[NegationOf(var)]) continue;
      if (can_freely_decrease_count[var] ==
          context->VarToConstraints(positive_ref).size()) {
        // We need to account for domain with hole, hence the ValueAtOrAfter().
        int64_t lb = can_freely_decrease_until[var];
        lb = context->DomainOf(ref).ValueAtOrAfter(lb);
        if (lb < context->MaxOf(ref)) {
          // We have a candidate, however, we need to make sure the dominating
          // variable upper bound didn't change.
          //
          // TODO(user): It look like testing this is not really necessary.
          // The reduction done by this class seem to be order independent.
          bool ok = true;
          const absl::Span<const IntegerVariable> dominating_vars =
              var_domination.DominatingVariables(var);
          for (const IntegerVariable dom : dominating_vars) {
            // Note that we assumed that a fixed point was reached before this
            // is called, so modified_domains should have been empty as we
            // entered this function. If not, the code is still correct, but we
            // might miss some reduction, they will still likely be done later
            // though.
            if (increase_is_forbidden[dom] ||
                context->modified_domains[PositiveRef(
                    VarDomination::IntegerVariableToRef(dom))]) {
              ok = false;
              break;
            }
          }
          if (increase_is_forbidden[NegationOf(var)]) {
            ok = false;
          }
          if (ok) {
            // TODO(user): Is this needed?
            increase_is_forbidden[var] = true;
            context->UpdateRuleStats(
                "domination: dual strenghtening using dominance");
            const Domain reduced_domain = Domain(context->MinOf(ref), lb);
            MaybeUpdateRefHintFromDominance(*context, ref, reduced_domain,
                                            dominating_vars);
            if (!context->IntersectDomainWith(ref, reduced_domain)) {
              return false;
            }

            // The rest of the loop only care about Booleans.
            // And if this was boolean, we would have fixed it.
            // If it became Boolean, we wait for the next call.
            // TODO(user): maybe the last point can be improved.
            continue;
          }
        }
      }

      if (!context->CanBeUsedAsLiteral(positive_ref)) continue;
      for (const IntegerVariable dom :
           var_domination.DominatingVariables(ref)) {
        if (increase_is_forbidden[dom]) continue;
        const int dom_ref = VarDomination::IntegerVariableToRef(dom);
        if (context->IsFixed(dom_ref)) continue;
        if (context->VariableIsNotUsedAnymore(dom_ref)) continue;
        if (context->VariableWasRemoved(dom_ref)) continue;
        if (!context->CanBeUsedAsLiteral(dom_ref)) continue;
        if (implications.contains({ref, dom_ref})) continue;

        ++num_added;
        context->AddImplication(ref, dom_ref);
        // The newly added implication can break the hint only if the hint value
        // of `ref` is 1 and the hint value of `dom_ref` is 0. In this case the
        // call below fixes it by negating both values. Otherwise it does
        // nothing and thus preserves its feasibility.
        crush.UpdateLiteralsWithDominance(ref, dom_ref);
        context->UpdateNewConstraintsVariableUsage();
        implications.insert({ref, dom_ref});
        implications.insert({NegatedRef(dom_ref), NegatedRef(ref)});

        // dom-- or var++ are now forbidden.
        increase_is_forbidden[var] = true;
        increase_is_forbidden[NegationOf(dom)] = true;
      }
    }
  }

  if (num_added > 0) {
    VLOG(1) << "Added " << num_added << " domination implications.";
    context->UpdateRuleStats("domination: added implications", num_added);
  }

  // TODO(user): We should probably be able to do something with this.
  if (saved_num_operations == context->num_presolve_operations) {
    context->UpdateRuleStats("TODO domination: unexploited dominations");
  }

  return true;
}

}  // namespace sat
}  // namespace operations_research
