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

#include "ortools/sat/presolve_util.h"

#include <cstdint>

#include "ortools/base/map_util.h"
#include "ortools/sat/cp_model_utils.h"

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
      domains[i] = domains[i].UnionWith(
          gtl::FindOrDieNoPrint(deductions_, {index, to_process[i]}));
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
  for (const auto entry : *terms) {
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

}  // namespace sat
}  // namespace operations_research
