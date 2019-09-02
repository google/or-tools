// Copyright 2010-2018 Google LLC
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

#include "ortools/base/map_util.h"

namespace operations_research {
namespace sat {

void DomainDeductions::AddDeduction(int literal_ref, int var, Domain domain) {
  CHECK_GE(var, 0);
  const Index index = IndexFromLiteral(literal_ref);
  if (index >= something_changed_.size()) {
    something_changed_.Resize(index + 1);
    enforcement_to_vars_.resize(index.value() + 1);
  }
  if (var >= tmp_num_occurences_.size()) {
    tmp_num_occurences_.resize(var + 1, 0);
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
      if (tmp_num_occurences_[var] == 0) {
        to_clean.push_back(var);
      }
      tmp_num_occurences_[var]++;
      if (tmp_num_occurences_[var] == clause.size()) {
        to_process.push_back(var);
      }
    }
  }

  // Clear the counts.
  for (const int var : to_clean) {
    tmp_num_occurences_[var] = 0;
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

}  // namespace sat
}  // namespace operations_research
