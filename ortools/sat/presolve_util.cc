// Copyright 2010-2022 Google LLC
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
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/meta/type_traits.h"
#include "absl/strings/str_join.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/util/bitset.h"
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
// Helper method for variable substitution. Returns the coefficient of 'var' in
// 'proto' and copies other terms in 'terms'.
template <typename ProtoWithVarsAndCoeffs>
int64_t GetVarCoeffAndCopyOtherTerms(
    const int var, const ProtoWithVarsAndCoeffs& proto,
    std::vector<std::pair<int, int64_t>>* terms) {
  int64_t var_coeff = 0;
  const int size = proto.vars().size();
  for (int i = 0; i < size; ++i) {
    int ref = proto.vars(i);
    int64_t coeff = proto.coeffs(i);
    if (!RefIsPositive(ref)) {
      ref = NegatedRef(ref);
      coeff = -coeff;
    }

    if (ref == var) {
      // If var appear multiple time, we add its coefficient.
      var_coeff += coeff;
      continue;
    } else {
      terms->push_back({ref, coeff});
    }
  }
  return var_coeff;
}

// Helper method for variable substituion. Sorts and merges the terms in 'terms'
// and adds them to 'proto'.
template <typename ProtoWithVarsAndCoeffs>
void SortAndMergeTerms(std::vector<std::pair<int, int64_t>>* terms,
                       ProtoWithVarsAndCoeffs* proto) {
  proto->clear_vars();
  proto->clear_coeffs();
  std::sort(terms->begin(), terms->end());
  int current_var = 0;
  int64_t current_coeff = 0;
  for (const auto& entry : *terms) {
    CHECK(RefIsPositive(entry.first));
    if (entry.first == current_var) {
      current_coeff += entry.second;
    } else {
      if (current_coeff != 0) {
        proto->add_vars(current_var);
        proto->add_coeffs(current_coeff);
      }
      current_var = entry.first;
      current_coeff = entry.second;
    }
  }
  if (current_coeff != 0) {
    proto->add_vars(current_var);
    proto->add_coeffs(current_coeff);
  }
}

// Adds all the terms from the var definition constraint with given var
// coefficient.
void AddTermsFromVarDefinition(const int var, const int64_t var_coeff,
                               const ConstraintProto& definition,
                               std::vector<std::pair<int, int64_t>>* terms) {
  const int definition_size = definition.linear().vars().size();
  for (int i = 0; i < definition_size; ++i) {
    int ref = definition.linear().vars(i);
    int64_t coeff = definition.linear().coeffs(i);
    if (!RefIsPositive(ref)) {
      ref = NegatedRef(ref);
      coeff = -coeff;
    }

    if (ref == var) {
      continue;
    } else {
      terms->push_back({ref, -coeff * var_coeff});
    }
  }
}
}  // namespace

bool SubstituteVariable(int var, int64_t var_coeff_in_definition,
                        const ConstraintProto& definition,
                        ConstraintProto* ct) {
  CHECK(RefIsPositive(var));
  CHECK_EQ(std::abs(var_coeff_in_definition), 1);

  // Copy all the terms (except the one referring to var).
  std::vector<std::pair<int, int64_t>> terms;
  int64_t var_coeff = GetVarCoeffAndCopyOtherTerms(var, ct->linear(), &terms);
  if (var_coeff == 0) return false;

  if (var_coeff_in_definition < 0) var_coeff *= -1;

  AddTermsFromVarDefinition(var, var_coeff, definition, &terms);

  // The substitution is correct only if we don't loose information here.
  // But for a constant definition rhs that is always the case.
  bool exact = false;
  Domain offset = ReadDomainFromProto(definition.linear());
  offset = offset.MultiplicationBy(-var_coeff, &exact);
  CHECK(exact);

  const Domain rhs = ReadDomainFromProto(ct->linear());
  FillDomainInProto(rhs.AdditionWith(offset), ct->mutable_linear());

  SortAndMergeTerms(&terms, ct->mutable_linear());
  return true;
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
  tmp_terms_.clear();
  tmp_terms_.reserve(terms.size());
  int64_t offset = 0;
  for (auto [lit, coeff] : terms) {
    if (compute_min) coeff = -coeff;  // Negate.
    if (coeff >= 0) {
      tmp_terms_.push_back({lit, coeff});
    } else {
      // l is the same as 1 - (1 - l)
      tmp_terms_.push_back({NegatedRef(lit), -coeff});
      offset += coeff;
    }
  }
  const int64_t internal_result =
      ComputeMaxActivityInternal(tmp_terms_, conditional);

  // Correct everything.
  if (conditional != nullptr) {
    const int num_terms = terms.size();
    for (int i = 0; i < num_terms; ++i) {
      if (tmp_terms_[i].first != terms[i].first) {
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
  amo_sums_.clear();

  const int num_terms = terms.size();
  to_sort_.clear();
  to_sort_.reserve(num_terms);
  for (int i = 0; i < num_terms; ++i) {
    const Index index = IndexFromLiteral(terms[i].first);
    const int64_t coeff = terms[i].second;
    if (index < amo_indices_.size()) {
      for (const int a : amo_indices_[index]) {
        amo_sums_[a] += coeff;
      }
    }
    to_sort_.push_back({terms[i].second, i});
  }
  std::sort(to_sort_.begin(), to_sort_.end(), std::greater<>());

  int num_parts = 0;
  partition_.resize(num_terms);
  used_amo_to_dense_index_.clear();
  for (int i = 0; i < num_terms; ++i) {
    const int original_i = to_sort_[i].second;
    const Index index = IndexFromLiteral(terms[original_i].first);
    const int64_t coeff = terms[original_i].second;
    int best = -1;
    int64_t best_sum = 0;
    bool done = false;
    if (index < amo_indices_.size()) {
      for (const int a : amo_indices_[index]) {
        const auto it = used_amo_to_dense_index_.find(a);
        if (it != used_amo_to_dense_index_.end()) {
          partition_[original_i] = it->second;
          done = true;
          break;
        }

        const int64_t sum_left = amo_sums_[a];
        amo_sums_[a] -= coeff;
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
      used_amo_to_dense_index_[best] = num_parts;
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
  amo_sums_.clear();
  for (const int ref : literals) {
    const Index index = IndexFromLiteral(ref);
    if (index < amo_indices_.size()) {
      for (const int a : amo_indices_[index]) {
        amo_sums_[a] += 1;
      }
    }
  }

  int num_parts = 0;
  const int num_literals = literals.size();
  partition_.resize(num_literals);
  used_amo_to_dense_index_.clear();
  for (int i = 0; i < literals.size(); ++i) {
    const Index index = IndexFromLiteral(literals[i]);
    int best = -1;
    int64_t best_sum = 0;
    bool done = false;
    if (index < amo_indices_.size()) {
      for (const int a : amo_indices_[index]) {
        const auto it = used_amo_to_dense_index_.find(a);
        if (it != used_amo_to_dense_index_.end()) {
          partition_[i] = it->second;
          done = true;
          break;
        }

        const int64_t sum_left = amo_sums_[a];
        amo_sums_[a] -= 1;
        if (sum_left > best_sum) {
          best_sum = sum_left;
          best = a;
        }
      }
    }
    if (done) continue;

    // New element.
    if (best != -1) {
      used_amo_to_dense_index_[best] = num_parts;
    }
    partition_[i] = num_parts++;
  }

  // We have the partition, lets construct the spans now.
  part_starts_.assign(num_parts, 0);
  part_sizes_.assign(num_parts, 0);
  part_ends_.assign(num_parts, 0);
  for (int i = 0; i < num_literals; ++i) {
    DCHECK_GE(partition_[i], 0);
    DCHECK_LT(partition_[i], num_parts);
    part_sizes_[partition_[i]]++;
  }
  for (int p = 1; p < num_parts; ++p) {
    part_starts_[p] = part_ends_[p] = part_sizes_[p - 1] + part_starts_[p - 1];
  }
  reordered_literals_.resize(num_literals);
  for (int i = 0; i < num_literals; ++i) {
    const int p = partition_[i];
    reordered_literals_[part_ends_[p]++] = literals[i];
  }
  std::vector<absl::Span<const int>> result;
  for (int p = 0; p < num_parts; ++p) {
    result.push_back(
        absl::MakeSpan(&reordered_literals_[part_starts_[p]], part_sizes_[p]));
  }
  return result;
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

    const Index index = IndexFromLiteral(ref);
    if (index < amo_indices_.size()) {
      for (const int a : amo_indices_[index]) {
        // If some other literal is at one in this amo, literal must be false,
        // and so the constraint cannot be enforced.
        const auto [_, inserted] = triggered_amo_.insert(a);
        if (!inserted) return false;
      }
    }

    const Index negated_index = IndexFromLiteral(NegatedRef(ref));
    if (negated_index < amo_indices_.size()) {
      // If a previous enforcement literal implies this one, we can skip it.
      bool skip = false;
      for (const int a : amo_indices_[negated_index]) {
        if (triggered_amo_.contains(a)) {
          skip = true;
          break;
        }
      }
      if (skip) continue;
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

}  // namespace sat
}  // namespace operations_research
