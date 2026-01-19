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
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/types/span.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/util.h"
#include "ortools/util/bitset.h"

namespace operations_research {
namespace sat {

void LratChecker::AddStats() const {
  if (!VLOG_IS_ON(1)) return;
  stats_->AddStats(
      {{"LratChecker/num_problem_clauses", num_problem_clauses_},
       {"LratChecker/num_inferred_clauses", num_inferred_clauses_},
       {"LratChecker/num_inferred_clauses_always_true",
        num_inferred_clauses_always_true_},
       {"LratChecker/num_processed_rup_literals", num_processed_rup_literals_},
       {"LratChecker/num_processed_rup_clauses", num_processed_rup_clauses_},
       {"LratChecker/num_unneeded_rup_literals", num_unneeded_rup_literals_},
       {"LratChecker/num_unneeded_rup_clauses", num_unneeded_rup_clauses_},
       {"LratChecker/num_processed_rat_literals", num_processed_rat_literals_},
       {"LratChecker/num_processed_rat_clauses", num_processed_rat_clauses_},
       {"LratChecker/num_unneeded_rat_literals", num_unneeded_rat_literals_},
       {"LratChecker/num_unneeded_rat_clauses", num_unneeded_rat_clauses_},
       {"LratChecker/num_deleted_clauses", num_deleted_clauses_},
       {"LratChecker/num_deleted_clauses_not_found",
        num_deleted_clauses_not_found_}});
}

bool LratChecker::AddProblemClause(ClauseId id,
                                   absl::Span<const Literal> clause) {
  ++num_problem_clauses_;
  return AddClauseInternal(id, clause, /*is_problem_clause=*/true,
                           /*unit_ids=*/{}, /*rat=*/{});
}

bool LratChecker::AddInferredClause(ClauseId id,
                                    absl::Span<const Literal> clause,
                                    absl::Span<const ClauseId> unit_ids,
                                    absl::Span<const RatIds> rat) {
  ++num_inferred_clauses_;
  return AddClauseInternal(id, clause, /*is_problem_clause=*/false, unit_ids,
                           rat);
}

void LratChecker::DeleteClauses(absl::Span<const ClauseId> clause_ids) {
  ++num_deleted_clauses_;
  for (const ClauseId clause_id : clause_ids) {
    const auto it = clauses_.find(clause_id);
    if (it == clauses_.end()) {
      ++num_deleted_clauses_not_found_;
      continue;
    }
    if (occurrences_needed_) {
      for (const Literal literal : it->second) {
        occurrences_[literal.Index()]--;
      }
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
    SparseBitset<LiteralIndex>& false_literals_set) {
  LiteralIndex unique_unassigned_literal = kNoLiteralIndex;
  for (const Literal literal : clause) {
    if (!false_literals_set[literal]) {
      if (unique_unassigned_literal != kNoLiteralIndex) return kError;
      unique_unassigned_literal = literal.Index();
    }
  }
  if (unique_unassigned_literal == kNoLiteralIndex) return kConflict;
  const Literal unassigned_literal = Literal(unique_unassigned_literal);
  DCHECK(!false_literals_set[unassigned_literal]);
  if (false_literals_set[unassigned_literal.Negated()]) {
    // `clause` propagates `unassigned_literal` which was already propagated by
    // a previous clause.
    return kWarning;
  }
  false_literals_set.Set(unassigned_literal.Negated());
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

  if (!clause.empty()) {
    BooleanVariable last_variable = clause[0].Variable();
    for (const Literal literal : clause) {
      last_variable = std::max(last_variable, literal.Variable());
    }
    if (last_variable >= num_variables_) {
      num_variables_ = last_variable.value() + 1;
      if (occurrences_needed_) {
        occurrences_.resize(2 * num_variables_, 0);
      } else if (clause.size() == 1 && unit_ids.empty() && rat.empty()) {
        // Early return for unit clauses made of a new variable. The following
        // code would validate this proof with the RAT property, but would also
        // set `occurrences_needed_` to true, which is unnecessary.
        clauses_[id] = FixedCapacityVector<Literal>(clause);
        return true;
      }
    }
  }

  // `clause` with duplicate literals removed.
  FixedCapacityVector<Literal> cleaned_clause;
  cleaned_clause.ClearAndReserve(clause.size());
  tmp_false_literals_set_.ClearAndResize(LiteralIndex(2 * num_variables_));
  for (const Literal literal : clause) {
    if (tmp_false_literals_set_[literal]) continue;
    if (tmp_false_literals_set_[literal.Negated()]) {
      if (!is_problem_clause) ++num_inferred_clauses_always_true_;
      return true;
    }
    tmp_false_literals_set_.Set(literal);
    cleaned_clause.push_back(literal);
  }

  if (!is_problem_clause) {
    UnitPropagationStatus last_propagation_status = kUnit;
    for (int i = 0; i < unit_ids.size(); ++i) {
      const ClauseId unit_id = unit_ids[i];
      auto it = clauses_.find(unit_id);
      if (it == clauses_.end()) {
        return Error(id, absl::StrCat("unit_id ", unit_id, " not found"));
      }
      ++num_processed_rup_clauses_;
      num_processed_rup_literals_ += it->second.size();
      last_propagation_status = Propagate(it->second, tmp_false_literals_set_);
      if (last_propagation_status == kError) {
        return Error(
            id, absl::StrCat("unit_id ", unit_id, " is not unit. literals=[",
                             absl::StrJoin(it->second, ","), "]"));
      } else if (last_propagation_status == kWarning) {
        last_propagation_status = kUnit;
        ++num_unneeded_rup_literals_;
      } else if (last_propagation_status == kConflict) {
        num_unneeded_rup_clauses_ += unit_ids.size() - i - 1;
        break;
      }
    }
    if (last_propagation_status == kUnit) {
      // Check if `clause` has the RAT property.
      if (clause.empty()) return Error(id, "missing pivot for RAT proof");
      const Literal pivot = clause.front();
      if (!occurrences_needed_) {
        occurrences_needed_ = true;
        InitializeOccurrences();
      }
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
        tmp_rat_false_literals_set_.CopyFrom(tmp_false_literals_set_);
        bool has_two_complementary_literals = false;
        for (const Literal literal : resolvant) {
          if (literal == pivot.Negated()) continue;
          if (tmp_false_literals_set_[literal.Negated()]) {
            has_two_complementary_literals = true;
            break;
          }
          tmp_rat_false_literals_set_.Set(literal);
        }
        if (has_two_complementary_literals) continue;
        last_propagation_status = kUnit;
        for (int j = 0; j < rat_ids.unit_ids.size(); ++j) {
          const ClauseId unit_id = rat_ids.unit_ids[j];
          const auto it = clauses_.find(unit_id);
          if (it == clauses_.end()) {
            return Error(id,
                         absl::StrCat("rat.unit_id ", unit_id, " not found"));
          }
          ++num_processed_rat_clauses_;
          num_processed_rat_literals_ += it->second.size();
          last_propagation_status =
              Propagate(it->second, tmp_rat_false_literals_set_);
          if (last_propagation_status == kError) {
            return Error(id,
                         absl::StrCat("rat.unit_id ", unit_id, " is not unit"));
          } else if (last_propagation_status == kWarning) {
            last_propagation_status = kUnit;
            ++num_unneeded_rat_literals_;
          } else if (last_propagation_status == kConflict) {
            num_unneeded_rat_clauses_ += rat_ids.unit_ids.size() - j - 1;
            break;
          }
        }
        if (last_propagation_status != kConflict) {
          return Error(id, absl::StrCat("last unit_id for rat.resolvant_id ",
                                        resolvant_id, " is not a conflict"));
        }
      }
    }
  }

  if (occurrences_needed_) {
    for (const Literal literal : cleaned_clause) {
      occurrences_[literal.Index()]++;
    }
  }
  clauses_[id] = std::move(cleaned_clause);
  if (clause.empty()) {
    complete_ = true;
  }
  return true;
}

void LratChecker::InitializeOccurrences() {
  occurrences_.assign(2 * num_variables_, 0);
  for (const auto& [id, clause] : clauses_) {
    for (const Literal literal : clause) {
      occurrences_[literal.Index()]++;
    }
  }
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
