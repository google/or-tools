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

#include "ortools/sat/presolve_util.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/random/distributions.h"
#include "absl/types/span.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/util.h"
#include "ortools/util/bitset.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

void DomainDeductions::AddDeduction(int literal_ref, int var, Domain domain) {
  CHECK_GE(var, 0);
  const Index index = IndexFromLiteral(literal_ref);
  if (index >= something_changed_.size()) {
    something_changed_.Resize(index + 1);
    enforcement_to_vars_.resize(index.value() + 1);
  }
  if (var >= tmp_num_occurrences_.size()) {
    tmp_num_occurrences_.resize(var + 1, 0);
  }
  const auto insert = deductions_.insert({{index, var}, domain});
  if (insert.second) {
    // New element.
    something_changed_.Set(index);
    enforcement_to_vars_[index].push_back(var);
  } else {
    // Existing element.
    const Domain& old_domain = insert.first->second;
    if (!old_domain.IsIncludedIn(domain)) {
      insert.first->second = domain.IntersectionWith(old_domain);
      something_changed_.Set(index);
    }
  }
}

Domain DomainDeductions::ImpliedDomain(int literal_ref, int var) const {
  CHECK_GE(var, 0);
  const Index index = IndexFromLiteral(literal_ref);
  const auto it = deductions_.find({index, var});
  if (it == deductions_.end()) return Domain::AllValues();
  return it->second;
}

std::vector<std::pair<int, Domain>> DomainDeductions::ProcessClause(
    absl::Span<const int> clause) {
  std::vector<std::pair<int, Domain>> result;

  // We only need to process this clause if something changed since last time.
  bool abort = true;
  for (const int ref : clause) {
    const Index index = IndexFromLiteral(ref);
    if (index >= something_changed_.size()) return result;
    if (something_changed_[index]) {
      abort = false;
    }
  }
  if (abort) return result;

  // Count for each variable, how many times it appears in the deductions lists.
  std::vector<int> to_process;
  std::vector<int> to_clean;
  for (const int ref : clause) {
    const Index index = IndexFromLiteral(ref);
    for (const int var : enforcement_to_vars_[index]) {
      if (tmp_num_occurrences_[var] == 0) {
        to_clean.push_back(var);
      }
      tmp_num_occurrences_[var]++;
      if (tmp_num_occurrences_[var] == clause.size()) {
        to_process.push_back(var);
      }
    }
  }

  // Clear the counts.
  for (const int var : to_clean) {
    tmp_num_occurrences_[var] = 0;
  }

  // Compute the domain unions.
  std::vector<Domain> domains(to_process.size());
  for (const int ref : clause) {
    const Index index = IndexFromLiteral(ref);
    for (int i = 0; i < to_process.size(); ++i) {
      domains[i] = domains[i].UnionWith(deductions_.at({index, to_process[i]}));
    }
  }

  for (int i = 0; i < to_process.size(); ++i) {
    result.push_back({to_process[i], std::move(domains[i])});
  }
  return result;
}

namespace {

// Helper method for variable substitution.
// Sorts and merges the terms in 'terms'.
// Returns false on overflow.
bool SortAndMergeTerms(std::vector<std::pair<int, int64_t>>* terms) {
  std::sort(terms->begin(), terms->end());

  int new_size = 0;
  int current_var = 0;
  int64_t current_coeff = 0;
  for (const auto& entry : *terms) {
    DCHECK(RefIsPositive(entry.first));
    if (entry.first == current_var) {
      current_coeff = CapAdd(current_coeff, entry.second);
      if (AtMinOrMaxInt64(current_coeff)) return false;
    } else {
      if (current_coeff != 0) {
        (*terms)[new_size++] = {current_var, current_coeff};
      }
      current_var = entry.first;
      current_coeff = entry.second;
    }
  }
  if (current_coeff != 0) {
    (*terms)[new_size++] = {current_var, current_coeff};
  }
  terms->resize(new_size);
  return true;
}

}  // namespace

bool AddLinearConstraintMultiple(int64_t factor, const ConstraintProto& to_add,
                                 ConstraintProto* to_modify) {
  if (factor == 0) return true;
  DCHECK_EQ(to_add.constraint_case(), ConstraintProto::kLinear);
  DCHECK_EQ(to_modify->constraint_case(), ConstraintProto::kLinear);

  // Copy to_modify terms.
  std::vector<std::pair<int, int64_t>> terms;
  LinearConstraintProto* out = to_modify->mutable_linear();
  const int initial_size = out->vars().size();
  for (int i = 0; i < initial_size; ++i) {
    const int var = out->vars(i);
    const int64_t coeff = out->coeffs(i);
    if (!RefIsPositive(var)) return false;
    terms.push_back({var, coeff});
  }

  // Add factor * to_add and check first kind of overflow.
  const int to_add_size = to_add.linear().vars().size();
  for (int i = 0; i < to_add_size; ++i) {
    const int var = to_add.linear().vars(i);
    const int64_t coeff = to_add.linear().coeffs(i);
    if (!RefIsPositive(var)) return false;
    terms.push_back({var, CapProd(coeff, factor)});
    if (AtMinOrMaxInt64(terms.back().second)) return false;
  }

  // Merge terms, return false if we get an overflow here.
  if (!SortAndMergeTerms(&terms)) return false;

  // Copy terms.
  out->clear_vars();
  out->clear_coeffs();
  for (const auto [var, coeff] : terms) {
    out->add_vars(var);
    out->add_coeffs(coeff);
  }

  // Write new rhs. We want to be exact during the multiplication. Note that in
  // practice this domain is fixed, so this will always be the case.
  bool exact = false;
  Domain offset = ReadDomainFromProto(to_add.linear());
  offset = offset.MultiplicationBy(factor, &exact);
  CHECK(exact);

  const Domain rhs = ReadDomainFromProto(*out);
  FillDomainInProto(rhs.AdditionWith(offset), out);
  return true;
}

bool SubstituteVariable(int var, int64_t var_coeff_in_definition,
                        const ConstraintProto& definition,
                        ConstraintProto* ct) {
  CHECK(RefIsPositive(var));

  // Get the coefficient of var in the constraint.
  // We assume positive reference here (it should always be the case now).
  // If we don't find var, we abort.
  int64_t var_coeff = 0;
  const int initial_ct_size = ct->linear().vars().size();
  for (int i = 0; i < initial_ct_size; ++i) {
    const int ref = ct->linear().vars(i);
    if (!RefIsPositive(ref)) return false;
    if (ref == var) {
      // If var appear multiple time, we add all its coefficients.
      var_coeff += ct->linear().coeffs(i);
    }
  }
  if (var_coeff == 0) return false;

  CHECK_EQ(std::abs(var_coeff_in_definition), 1);
  const int64_t factor = var_coeff_in_definition > 0 ? -var_coeff : var_coeff;
  return AddLinearConstraintMultiple(factor, definition, ct);
}

void ActivityBoundHelper::ClearAtMostOnes() {
  num_at_most_ones_ = 0;
  amo_indices_.clear();
}

void ActivityBoundHelper::AddAtMostOne(absl::Span<const int> amo) {
  int num_skipped = 0;
  const int complexity_limit = 50;
  for (const int literal : amo) {
    const Index i = IndexFromLiteral(literal);
    if (i >= amo_indices_.size()) amo_indices_.resize(i + 1);
    if (amo_indices_[i].size() >= complexity_limit) ++num_skipped;
  }
  if (num_skipped + 1 >= amo.size()) return;

  // Add it.
  const int unique_index = num_at_most_ones_++;
  for (const int literal : amo) {
    const Index i = IndexFromLiteral(literal);
    if (amo_indices_[i].size() < complexity_limit) {
      amo_indices_[i].push_back(unique_index);
    }
  }
}

// TODO(user): Add long ones first, or at least the ones of size 2 after.
void ActivityBoundHelper::AddAllAtMostOnes(const CpModelProto& proto) {
  for (const ConstraintProto& ct : proto.constraints()) {
    const auto type = ct.constraint_case();
    if (type == ConstraintProto::kAtMostOne) {
      AddAtMostOne(ct.at_most_one().literals());
    } else if (type == ConstraintProto::kExactlyOne) {
      AddAtMostOne(ct.exactly_one().literals());
    } else if (type == ConstraintProto::kBoolAnd) {
      if (ct.enforcement_literal().size() == 1) {
        const int a = ct.enforcement_literal(0);
        for (const int b : ct.bool_and().literals()) {
          // a => b same as amo(a, not(b)).
          AddAtMostOne({a, NegatedRef(b)});
        }
      }
    }
  }
}

int64_t ActivityBoundHelper::ComputeActivity(
    bool compute_min, absl::Span<const std::pair<int, int64_t>> terms,
    std::vector<std::array<int64_t, 2>>* conditional) {
  tmp_terms_for_compute_activity_.clear();
  tmp_terms_for_compute_activity_.reserve(terms.size());
  int64_t offset = 0;
  for (auto [lit, coeff] : terms) {
    if (compute_min) coeff = -coeff;  // Negate.
    if (coeff >= 0) {
      tmp_terms_for_compute_activity_.push_back({lit, coeff});
    } else {
      // l is the same as 1 - (1 - l)
      tmp_terms_for_compute_activity_.push_back({NegatedRef(lit), -coeff});
      offset += coeff;
    }
  }
  const int64_t internal_result =
      ComputeMaxActivityInternal(tmp_terms_for_compute_activity_, conditional);

  // Correct everything.
  if (conditional != nullptr) {
    const int num_terms = terms.size();
    for (int i = 0; i < num_terms; ++i) {
      if (tmp_terms_for_compute_activity_[i].first != terms[i].first) {
        // The true/false meaning is swapped
        std::swap((*conditional)[i][0], (*conditional)[i][1]);
      }
      (*conditional)[i][0] += offset;
      (*conditional)[i][1] += offset;
      if (compute_min) {
        (*conditional)[i][0] = -(*conditional)[i][0];
        (*conditional)[i][1] = -(*conditional)[i][1];
      }
    }
  }
  if (compute_min) return -(offset + internal_result);
  return offset + internal_result;
}

// Use trivial heuristic for now:
// - Sort by decreasing coeff.
// - If belong to a chosen part, use it.
// - If not, choose biggest part left. TODO(user): compute sum of coeff in part?
void ActivityBoundHelper::PartitionIntoAmo(
    absl::Span<const std::pair<int, int64_t>> terms) {
  auto amo_sums = amo_sums_.ClearedView(num_at_most_ones_);

  const int num_terms = terms.size();
  to_sort_.clear();
  to_sort_.reserve(num_terms);
  for (int i = 0; i < num_terms; ++i) {
    const Index index = IndexFromLiteral(terms[i].first);
    const int64_t coeff = terms[i].second;
    DCHECK_GE(coeff, 0);
    if (index < amo_indices_.size()) {
      for (const int a : amo_indices_[index]) {
        amo_sums[a] += coeff;
      }
    }
    to_sort_.push_back(
        TermWithIndex{.coeff = coeff, .index = index, .span_index = i});
  }
  std::sort(to_sort_.begin(), to_sort_.end(),
            [](const TermWithIndex& a, const TermWithIndex& b) {
              // We take into account the index to make the result
              // deterministic.
              return std::tie(a.coeff, a.index) > std::tie(b.coeff, b.index);
            });

  int num_parts = 0;
  partition_.resize(num_terms);
  for (const TermWithIndex& term : to_sort_) {
    const int original_i = term.span_index;
    const Index index = term.index;
    const int64_t coeff = term.coeff;
    int best = -1;
    int64_t best_sum = 0;
    bool done = false;
    if (index < amo_indices_.size()) {
      for (const int a : amo_indices_[index]) {
        // Tricky/Optim: we use negative amo_sums to indicate if this amo was
        // already used and its dense index (we use -1 - index).
        const int64_t sum_left = amo_sums[a];
        if (sum_left < 0) {
          partition_[original_i] = -sum_left - 1;
          done = true;
          break;
        }

        amo_sums[a] -= coeff;
        if (sum_left > best_sum) {
          best_sum = sum_left;
          best = a;
        }
      }
    }
    if (done) continue;

    // New element.
    if (best == -1) {
      partition_[original_i] = num_parts++;
    } else {
      amo_sums[best] = -num_parts - 1;  // "dense indexing"
      partition_[original_i] = num_parts;
      ++num_parts;
    }
  }
  for (const int p : partition_) CHECK_LT(p, num_parts);
  CHECK_LE(num_parts, num_terms);
}

// Similar algo as above for this simpler case.
std::vector<absl::Span<const int>>
ActivityBoundHelper::PartitionLiteralsIntoAmo(absl::Span<const int> literals) {
  auto amo_sums = amo_sums_.ClearedView(num_at_most_ones_);
  for (const int ref : literals) {
    const Index index = IndexFromLiteral(ref);
    if (index < amo_indices_.size()) {
      for (const int a : amo_indices_[index]) {
        amo_sums[a] += 1;
      }
    }
  }

  int num_parts = 0;
  const int num_literals = literals.size();
  partition_.resize(num_literals);
  for (int i = 0; i < literals.size(); ++i) {
    const Index index = IndexFromLiteral(literals[i]);
    int best = -1;
    int64_t best_sum = 0;
    bool done = false;
    if (index < amo_indices_.size()) {
      for (const int a : amo_indices_[index]) {
        const int64_t sum_left = amo_sums[a];

        // Tricky/Optim: we use negative amo_sums to indicate if this amo was
        // already used and its dense index (we use -1 - index).
        if (sum_left < 0) {
          partition_[i] = -sum_left - 1;
          done = true;
          break;
        }

        amo_sums[a] -= 1;
        if (sum_left > best_sum) {
          best_sum = sum_left;
          best = a;
        }
      }
    }
    if (done) continue;

    // New element.
    if (best != -1) {
      amo_sums[best] = -num_parts - 1;
    }
    partition_[i] = num_parts++;
  }

  // We have the partition, lets construct the spans now.
  part_to_literals_.ResetFromFlatMapping(partition_, literals);
  DCHECK_EQ(part_to_literals_.size(), num_parts);
  return part_to_literals_.AsVectorOfSpan();
}

bool ActivityBoundHelper::IsAmo(absl::Span<const int> literals) {
  auto amo_sums = amo_sums_.ClearedView(num_at_most_ones_);
  for (int i = 0; i < literals.size(); ++i) {
    bool has_max_size = false;
    const Index index = IndexFromLiteral(literals[i]);
    if (index >= amo_indices_.size()) return false;
    for (const int a : amo_indices_[index]) {
      if (amo_sums[a]++ == i) has_max_size = true;
    }
    if (!has_max_size) return false;
  }
  return true;
}

int64_t ActivityBoundHelper::ComputeMaxActivityInternal(
    absl::Span<const std::pair<int, int64_t>> terms,
    std::vector<std::array<int64_t, 2>>* conditional) {
  PartitionIntoAmo(terms);

  // Compute the max coefficient in each partition.
  const int num_terms = terms.size();
  max_by_partition_.assign(num_terms, 0);
  second_max_by_partition_.assign(num_terms, 0);
  for (int i = 0; i < num_terms; ++i) {
    const int p = partition_[i];
    const int64_t coeff = terms[i].second;
    if (coeff >= max_by_partition_[p]) {
      second_max_by_partition_[p] = max_by_partition_[p];
      max_by_partition_[p] = coeff;
    } else if (coeff > second_max_by_partition_[p]) {
      second_max_by_partition_[p] = coeff;
    }
  }

  // Once we have this, we can compute bound.
  int64_t max_activity = 0;
  for (int p = 0; p < partition_.size(); ++p) {
    max_activity += max_by_partition_[p];
  }
  if (conditional != nullptr) {
    conditional->resize(num_terms);
    for (int i = 0; i < num_terms; ++i) {
      const int64_t coeff = terms[i].second;
      const int p = partition_[i];
      const int64_t max_used = max_by_partition_[p];

      // We have two cases depending if coeff was the maximum in its part or
      // not.
      if (coeff == max_used) {
        // Use the second max.
        (*conditional)[i][0] =
            max_activity - max_used + second_max_by_partition_[p];
        (*conditional)[i][1] = max_activity;
      } else {
        // The max is still there, no change at 0 but change for 1.
        (*conditional)[i][0] = max_activity;
        (*conditional)[i][1] = max_activity - max_used + coeff;
      }
    }
  }
  return max_activity;
}

bool ActivityBoundHelper::AppearInTriggeredAmo(int literal) const {
  const Index index = IndexFromLiteral(literal);
  if (index >= amo_indices_.size()) return false;
  for (const int amo : amo_indices_[index]) {
    if (triggered_amo_.contains(amo)) return true;
  }
  return false;
}

bool ActivityBoundHelper::PresolveEnforcement(
    absl::Span<const int> refs, ConstraintProto* ct,
    absl::flat_hash_set<int>* literals_at_true) {
  if (ct->enforcement_literal().empty()) return true;

  literals_at_true->clear();
  triggered_amo_.clear();
  int new_size = 0;
  for (int i = 0; i < ct->enforcement_literal().size(); ++i) {
    const int ref = ct->enforcement_literal(i);
    if (literals_at_true->contains(ref)) continue;  // Duplicate.
    if (literals_at_true->contains(NegatedRef(ref))) return false;  // False.
    literals_at_true->insert(ref);

    // If a previous enforcement literal implies this one, we can skip it.
    //
    // Tricky: We need to do that before appending the amo containing ref in
    // case an amo contains both ref and not(ref).
    // TODO(user): Ideally these amo should not be added to this class.
    if (AppearInTriggeredAmo(NegatedRef(ref))) continue;

    const Index index = IndexFromLiteral(ref);
    if (index < amo_indices_.size()) {
      for (const int a : amo_indices_[index]) {
        // If some other literal is at one in this amo, literal must be false,
        // and so the constraint cannot be enforced.
        const auto [_, inserted] = triggered_amo_.insert(a);
        if (!inserted) return false;
      }
    }

    // Keep this enforcement.
    ct->set_enforcement_literal(new_size++, ref);
  }
  ct->mutable_enforcement_literal()->Truncate(new_size);

  for (const int ref : refs) {
    // Skip already fixed.
    if (literals_at_true->contains(ref)) continue;
    if (literals_at_true->contains(NegatedRef(ref))) continue;
    for (const int to_test : {ref, NegatedRef(ref)}) {
      const Index index = IndexFromLiteral(to_test);
      if (index < amo_indices_.size()) {
        for (const int a : amo_indices_[index]) {
          if (triggered_amo_.contains(a)) {
            // If some other literal is at one in this amo,
            // literal must be false.
            if (literals_at_true->contains(to_test)) return false;
            literals_at_true->insert(NegatedRef(to_test));
            break;
          }
        }
      }
    }
  }

  return true;
}

int ActivityBoundHelper::RemoveEnforcementThatMakesConstraintTrivial(
    absl::Span<const std::pair<int, int64_t>> boolean_terms,
    const Domain& other_terms, const Domain& rhs, ConstraintProto* ct) {
  if (boolean_terms.empty()) return 0;
  tmp_set_.clear();
  triggered_amo_.clear();
  tmp_boolean_terms_in_some_amo_.clear();
  tmp_boolean_terms_in_some_amo_.reserve(boolean_terms.size());
  int num_enforcement_to_check = 0;
  for (const int enf_lit : ct->enforcement_literal()) {
    const Index negated_index = IndexFromLiteral(NegatedRef(enf_lit));
    if (negated_index >= amo_indices_.size()) continue;
    if (amo_indices_[negated_index].empty()) continue;
    triggered_amo_.insert(amo_indices_[negated_index].begin(),
                          amo_indices_[negated_index].end());
    ++num_enforcement_to_check;
  }
  int64_t non_amo_min_activity = 0;
  int64_t non_amo_max_activity = 0;
  auto log_work = [&]() {
    VLOG(1) << "RemoveEnforcementThatMakesConstraintTrivial: "
               "aborting because too expensive: "
            << num_enforcement_to_check << " " << boolean_terms.size();
    return 0;
  };
  static const int kMaxWork = 1e9;
  int work = 0;
  for (int i = 0; i < boolean_terms.size(); ++i) {
    const int ref = boolean_terms[i].first;
    const int64_t coeff = boolean_terms[i].second;
    if (AppearInTriggeredAmo(ref) || AppearInTriggeredAmo(NegatedRef(ref))) {
      tmp_boolean_terms_in_some_amo_.push_back(i);
    } else {
      if (coeff > 0) {
        non_amo_max_activity += coeff;
      } else {
        non_amo_min_activity += coeff;
      }
    }
    work += NumAmoForVariable(ref);
    if (work > kMaxWork) return log_work();
  }

  for (const int enf_lit : ct->enforcement_literal()) {
    const Index negated_index = IndexFromLiteral(NegatedRef(enf_lit));
    if (negated_index >= amo_indices_.size()) continue;
    if (amo_indices_[negated_index].empty()) continue;

    triggered_amo_.clear();
    triggered_amo_.insert(amo_indices_[negated_index].begin(),
                          amo_indices_[negated_index].end());

    // Compute min_max activity when enf_lit is false.
    int64_t min_activity = non_amo_min_activity;
    int64_t max_activity = non_amo_max_activity;
    for (const int i : tmp_boolean_terms_in_some_amo_) {
      const int ref = boolean_terms[i].first;
      const int64_t coeff = boolean_terms[i].second;
      // This is not supposed to happen after PresolveEnforcement(), so we
      // just abort in this case.
      if (ref == enf_lit || ref == NegatedRef(enf_lit)) break;

      const bool is_true = AppearInTriggeredAmo(NegatedRef(ref));
      const bool is_false = AppearInTriggeredAmo(ref);
      work += NumAmoForVariable(ref);
      if (work > kMaxWork) return log_work();

      // Similarly, this is not supposed to happen after PresolveEnforcement().
      if (is_true && is_false) break;

      if (is_false) continue;
      if (is_true) {
        min_activity += coeff;
        max_activity += coeff;
        continue;
      }
      if (coeff > 0) {
        max_activity += coeff;
      } else {
        min_activity += coeff;
      }
    }

    if (Domain(min_activity, max_activity)
            .AdditionWith(other_terms)
            .IsIncludedIn(rhs)) {
      tmp_set_.insert(enf_lit);
    }
  }

  if (!tmp_set_.empty()) {
    int new_size = 0;
    for (const int ref : ct->enforcement_literal()) {
      if (tmp_set_.contains(ref)) continue;
      ct->set_enforcement_literal(new_size++, ref);
    }
    const int old_size = ct->enforcement_literal().size();
    ct->mutable_enforcement_literal()->Truncate(new_size);
    return old_size - new_size;
  }
  return 0;
}

void ClauseWithOneMissingHasher::RegisterClause(int c,
                                                absl::Span<const int> clause) {
  uint64_t hash = 0;
  for (const int ref : clause) {
    const Index index = IndexFromLiteral(ref);
    while (index >= literal_to_hash_.size()) {
      // We use random value for a literal hash.
      literal_to_hash_.push_back(absl::Uniform<uint64_t>(random_));
    }
    hash ^= literal_to_hash_[index];
  }

  if (c >= clause_to_hash_.size()) clause_to_hash_.resize(c + 1, 0);
  clause_to_hash_[c] = hash;
}

uint64_t ClauseWithOneMissingHasher::HashOfNegatedLiterals(
    absl::Span<const int> literals) {
  uint64_t hash = 0;
  for (const int ref : literals) {
    const Index index = IndexFromLiteral(NegatedRef(ref));
    while (index >= literal_to_hash_.size()) {
      // We use random value for a literal hash.
      literal_to_hash_.push_back(absl::Uniform<uint64_t>(random_));
    }
    hash ^= literal_to_hash_[index];
  }
  return hash;
}

bool FindSingleLinearDifference(const LinearConstraintProto& lin1,
                                const LinearConstraintProto& lin2, int* var1,
                                int64_t* coeff1, int* var2, int64_t* coeff2) {
  const int size = lin1.vars().size();
  CHECK_EQ(size, lin2.vars().size());
  *coeff1 = 0;
  *coeff2 = 0;
  int i = 0;
  int j = 0;
  while (i < size || j < size) {
    // Note that we can't have both undefined or the loop would have exited.
    const int v1 = i < size ? lin1.vars(i) : std::numeric_limits<int>::max();
    const int v2 = j < size ? lin2.vars(j) : std::numeric_limits<int>::max();

    // Same term, continue.
    if (v1 == v2 && lin1.coeffs(i) == lin2.coeffs(j)) {
      ++i;
      ++j;
      continue;
    }

    // We have a diff.
    // term i not in lin2.
    if (v1 < v2) {
      if (*coeff1 != 0) return false;  // Returns if second diff.
      *var1 = v1;
      *coeff1 = lin1.coeffs(i);
      ++i;
      continue;
    }

    // term j not in lin1.
    if (v1 > v2) {
      if (*coeff2 != 0) return false;  // Returns if second diff.
      *var2 = v2;
      *coeff2 = lin2.coeffs(j);
      ++j;
      continue;
    }

    // Coeff differ. Returns if we had a diff previously.
    if (*coeff1 != 0 || *coeff2 != 0) return false;
    *var1 = v1;
    *var2 = v2;
    *coeff1 = lin1.coeffs(i);
    *coeff2 = lin2.coeffs(j);
    ++i;
    ++j;
  }

  return *coeff1 != 0 && *coeff2 != 0;
}

}  // namespace sat
}  // namespace operations_research
