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

#include "ortools/sat/lrat_checker.h"

#include <algorithm>
#include <string_view>
#include <utility>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/util.h"

namespace operations_research {
namespace sat {

bool LratChecker::AddProblemClause(ClauseId id,
                                   absl::Span<const Literal> clause) {
  return AddClauseInternal(id, clause, /*is_problem_clause=*/true,
                           /*unit_ids=*/{}, /*rat=*/{});
}

bool LratChecker::AddInferredClause(ClauseId id,
                                    absl::Span<const Literal> clause,
                                    absl::Span<const ClauseId> unit_ids,
                                    absl::Span<const RatIds> rat) {
  return AddClauseInternal(id, clause, /*is_problem_clause=*/false, unit_ids,
                           rat);
}

void LratChecker::DeleteClauses(absl::Span<const ClauseId> clause_ids) {
  for (const ClauseId clause_id : clause_ids) {
    const auto it = clauses_.find(clause_id);
    if (it == clauses_.end()) continue;
    for (const Literal literal : it->second) {
      occurrences_[literal]--;
    }
    clauses_.erase(it);
  }
}

namespace {
enum UnitPropagationStatus {
  kUnit = 1,
  kConflict = 2,
  kWarning = 3,
  kError = 4,
};

UnitPropagationStatus Propagate(
    absl::Span<const Literal> clause,
    absl::flat_hash_set<Literal>* false_literals_set) {
  LiteralIndex unique_unassigned_literal = kNoLiteralIndex;
  for (const Literal literal : clause) {
    if (!false_literals_set->contains(literal)) {
      if (unique_unassigned_literal != kNoLiteralIndex) return kError;
      unique_unassigned_literal = literal.Index();
    }
  }
  if (unique_unassigned_literal == kNoLiteralIndex) return kConflict;
  const Literal unassigned_literal = Literal(unique_unassigned_literal);
  DCHECK(!false_literals_set->contains(unassigned_literal));
  if (false_literals_set->contains(unassigned_literal.Negated())) {
    // `clause` propagates `unassigned_literal` which was already propagated by
    // a previous clause.
    return kWarning;
  }
  false_literals_set->insert(unassigned_literal.Negated());
  return kUnit;
}
}  // namespace

bool LratChecker::AddClauseInternal(ClauseId id,
                                    absl::Span<const Literal> clause,
                                    bool is_problem_clause,
                                    absl::Span<const ClauseId> unit_ids,
                                    absl::Span<const RatIds> rat) {
  if (!valid_) return false;
  if (complete_) return true;
  if (clauses_.contains(id))
    return Error(id, absl::StrCat("clause ID ", id, " already used"));

  FixedCapacityVector<Literal> sorted_clause(clause);
  std::sort(sorted_clause.begin(), sorted_clause.end());
  sorted_clause.resize(std::unique(sorted_clause.begin(), sorted_clause.end()) -
                       sorted_clause.begin());
  for (int i = 1; i < sorted_clause.size(); ++i) {
    if (sorted_clause[i] == sorted_clause[i - 1].Negated()) {
      return true;
    }
  }

  if (!is_problem_clause) {
    tmp_false_literals_set_.clear();
    tmp_false_literals_set_.insert(sorted_clause.begin(), sorted_clause.end());
    UnitPropagationStatus last_propagation_status = kUnit;
    for (const ClauseId unit_id : unit_ids) {
      auto it = clauses_.find(unit_id);
      if (it == clauses_.end()) {
        return Error(id, absl::StrCat("unit_id ", unit_id, " not found"));
      }
      last_propagation_status = Propagate(it->second, &tmp_false_literals_set_);
      if (last_propagation_status == kError) {
        return Error(id, absl::StrCat("unit_id ", unit_id, " is not unit"));
      } else if (last_propagation_status == kWarning) {
        ++num_warnings_;
      }
    }
    if (last_propagation_status == kUnit) {
      // Check if `clause` has the RAT property.
      if (clause.empty()) return Error(id, "missing pivot for RAT proof");
      const Literal pivot = clause.front();
      if (rat.size() != occurrences_[pivot.Negated()]) {
        return Error(id, "wrong number of resolvant IDs in RAT proof");
      }
      absl::flat_hash_set<ClauseId>& resolvant_ids_set = tmp_clause_ids_;
      resolvant_ids_set.clear();
      // Check that the unit propagation proof of each rat_id is correct.
      for (const RatIds& rat_ids : rat) {
        const ClauseId resolvant_id = rat_ids.resolvant_id;
        // rat_ids must not contain duplicate resolvant clause IDs.
        if (!resolvant_ids_set.insert(resolvant_id).second) {
          return Error(id,
                       absl::StrCat("duplicate resolvant_id ", resolvant_id));
        }
        // The resolvant clause must exist and must contain pivot.Negated().
        const auto it = clauses_.find(resolvant_id);
        if (it == clauses_.end()) {
          return Error(
              id, absl::StrCat("resolvant_id ", resolvant_id, " not found"));
        }
        const absl::Span<const Literal> resolvant = it->second;
        if (!absl::c_binary_search(resolvant, pivot.Negated())) {
          return Error(id,
                       absl::StrCat("missing negated pivot in resolvant_id ",
                                    resolvant_id));
        }
        // Its unit propagation proof must be correct, unless `clause` and
        // `resolvant` have two complementary literals (other than the pivot --
        // this is still valid if we use `tmp_false_literals_set_` instead of
        // `clause`).
        tmp_rat_false_literals_set_ = tmp_false_literals_set_;
        bool has_two_complementary_literals = false;
        for (const Literal literal : resolvant) {
          if (literal == pivot.Negated()) continue;
          if (tmp_false_literals_set_.contains(literal.Negated())) {
            has_two_complementary_literals = true;
            break;
          }
          tmp_rat_false_literals_set_.insert(literal);
        }
        if (has_two_complementary_literals) continue;
        last_propagation_status = kUnit;
        for (const ClauseId unit_id : rat_ids.unit_ids) {
          const auto it = clauses_.find(unit_id);
          if (it == clauses_.end()) {
            return Error(id,
                         absl::StrCat("rat.unit_id ", unit_id, " not found"));
          }
          last_propagation_status =
              Propagate(it->second, &tmp_rat_false_literals_set_);
          if (last_propagation_status == kError) {
            return Error(id,
                         absl::StrCat("rat.unit_id ", unit_id, " is not unit"));
          }
        }
        if (last_propagation_status != kConflict) {
          return Error(id, absl::StrCat("last unit_id for rat.resolvant_id ",
                                        resolvant_id, " is not a conflict"));
        }
      }
    }
  }

  for (const Literal literal : sorted_clause) {
    occurrences_[literal]++;
  }
  clauses_[id] = std::move(sorted_clause);
  if (clause.empty()) {
    complete_ = true;
  }
  return true;
}

bool LratChecker::Error(ClauseId id, std::string_view error) {
  if (valid_) {
    error_message_ = absl::StrCat("In clause ", id, ": ", error);
    valid_ = false;
  }
  return false;
}

}  // namespace sat
}  // namespace operations_research
