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
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/types/span.h"
#include "ortools/sat/sat_base.h"
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
       {"LratChecker/num_deleted_clauses", num_deleted_clauses_}});
}

void LratChecker::EnableRatProofs() {
  CHECK_EQ(num_problem_clauses_, 0);
  CHECK_EQ(num_inferred_clauses_, 0);
  rat_enabled_ = true;
}

void LratChecker::DisableRatProofs() {
  rat_enabled_ = false;
  occurrences_.clear();
}

bool LratChecker::AddProblemClause(ClausePtr clause) {
  ++num_problem_clauses_;
  return AddClauseInternal(kProblemClause, clause, clause.GetLiterals(),
                           /*rup_clauses=*/{}, /*rat_clauses=*/{});
}

bool LratChecker::AddInferredClause(ClausePtr clause,
                                    absl::Span<const ClausePtr> rup_clauses,
                                    absl::Span<const RatClauses> rat_clauses) {
  ++num_inferred_clauses_;
  return AddClauseInternal(kInferredClause, clause, clause.GetLiterals(),
                           rup_clauses, rat_clauses);
}

bool LratChecker::RewriteClause(ClausePtr clause,
                                absl::Span<const Literal> literals,
                                absl::Span<const ClausePtr> rup_clauses,
                                absl::Span<const RatClauses> rat_clauses) {
  ++num_inferred_clauses_;
  return AddClauseInternal(kRewrittenClause, clause, literals, rup_clauses,
                           rat_clauses);
}

void LratChecker::DeleteClauses(absl::Span<const ClausePtr> clauses) {
  ++num_deleted_clauses_;
  if (!valid_ || complete_) return;
  if constexpr (DEBUG_MODE) {
    for (const ClausePtr clause : clauses) {
      CHECK(debug_clause_by_ptr_.contains(clause))
          << clause << " " << clause.GetLiterals();
      debug_clause_by_ptr_.erase(clause);
    }
  }
  if (!rat_enabled_) return;
  for (const ClausePtr clause : clauses) {
    tmp_marked_literals_.ClearAndResize(LiteralIndex(2 * num_variables_));
    for (const Literal literal : clause.GetLiterals()) {
      if (tmp_marked_literals_[literal]) continue;
      occurrences_[literal.Index()]--;
      DCHECK_GE(occurrences_[literal.Index()], 0);
      tmp_marked_literals_.Set(literal);
    }
  }
}

LratChecker::UnitPropagationStatus LratChecker::Propagate(
    ClausePtr clause, SparseBitset<LiteralIndex>& false_literals_set) {
  LiteralIndex unique_unassigned_literal = kNoLiteralIndex;
  absl::Span<const Literal> literals = clause.GetLiterals();
  for (const Literal literal : literals) {
    if (!false_literals_set[literal]) {
      if (unique_unassigned_literal != kNoLiteralIndex) return kError;
      unique_unassigned_literal = literal.Index();
    }
  }
  num_processed_rup_literals_ += literals.size();
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

namespace {
bool EqualSatClausePtrs(ClausePtr ptr, ClausePtr other_ptr) {
  return ptr == other_ptr && ptr.IsSatClausePtr();
}
}  // namespace

bool LratChecker::AddClauseInternal(ClauseType type, ClausePtr ptr,
                                    absl::Span<const Literal> literals,
                                    absl::Span<const ClausePtr> rup_clauses,
                                    absl::Span<const RatClauses> rat_clauses) {
  if (!valid_) return false;
  if (complete_) return true;

  std::vector<Literal>& clause = tmp_clause_;
  clause.clear();
  int num_variables = num_variables_;
  for (const Literal literal : literals) {
    num_variables = std::max(num_variables, literal.Variable().value() + 1);
  }
  tmp_false_literals_set_.ClearAndResize(LiteralIndex(2 * num_variables));
  for (const Literal literal : literals) {
    if (tmp_false_literals_set_[literal]) continue;
    if (tmp_false_literals_set_[literal.Negated()]) {
      if (type == kProblemClause) ++num_inferred_clauses_always_true_;
      return true;
    }
    tmp_false_literals_set_.Set(literal);
    clause.push_back(literal);
  }

  if (num_variables >= num_variables_) {
    num_variables_ = num_variables;
    if (rat_enabled_) {
      occurrences_.resize(2 * num_variables_, 0);
    } else if (clause.size() == 1 && rup_clauses.empty() &&
               rat_clauses.empty() && type != kRewrittenClause) {
      // Early return for unit clauses made of a new variable. The following
      // code would validate this proof with the RAT property, but would also
      // require `rat_enabled_`, which is unnecessary here.
      if constexpr (DEBUG_MODE) {
        debug_clause_by_ptr_[ptr] = clause;
      }
      return true;
    }
  }

  if (type != kProblemClause) {
    UnitPropagationStatus last_propagation_status = kUnit;
    for (int i = 0; i < rup_clauses.size(); ++i) {
      const ClausePtr rup_clause = rup_clauses[i];
      // Using an already proved clause to prove it again is valid but error
      // prone with SatClause (we might accidentally use the new version to
      // prove it again, instead of proving the new version from the old one).
      // Hence we only allow this with the explicit RewriteClause() method.
      DCHECK(type == kRewrittenClause || !EqualSatClausePtrs(rup_clause, ptr));
      if constexpr (DEBUG_MODE) {
        if (!DebugCheckProofClauseId(ptr, rup_clause)) return false;
      }
      ++num_processed_rup_clauses_;
      last_propagation_status = Propagate(rup_clause, tmp_false_literals_set_);
      if (last_propagation_status == kError) {
        return Error(
            ptr,
            absl::StrCat("rup_clause ", rup_clause, " is not unit. literals=[",
                         absl::StrJoin(rup_clause.GetLiterals(), ","), "]"));
      } else if (last_propagation_status == kWarning) {
        last_propagation_status = kUnit;
        ++num_unneeded_rup_literals_;
      } else if (last_propagation_status == kConflict) {
        num_unneeded_rup_clauses_ += rup_clauses.size() - i - 1;
        break;
      }
    }
    if (last_propagation_status == kUnit) {
      if (!rat_enabled_) return Error(ptr, "RAT proof support is disabled");
      // Check if `clause` has the RAT property.
      if (clause.empty()) return Error(ptr, "missing pivot for RAT proof");
      const Literal pivot = clause.front();
      if (rat_clauses.size() != occurrences_[pivot.Negated()]) {
        return Error(ptr, "wrong number of resolvant clauses in RAT proof");
      }
      absl::flat_hash_set<ClausePtr>& resolvants = tmp_clauses_;
      resolvants.clear();
      // Check that the unit propagation proof of each rat_clauses is correct.
      for (const RatClauses& rat_clauses : rat_clauses) {
        const ClausePtr resolvant = rat_clauses.resolvant;
        DCHECK(type == kRewrittenClause || !EqualSatClausePtrs(resolvant, ptr));
        if constexpr (DEBUG_MODE) {
          if (!DebugCheckProofClauseId(ptr, resolvant)) return false;
        }
        // resolvants must not contain duplicate resolvant clause pointers.
        if (!resolvants.insert(resolvant).second) {
          return Error(ptr, absl::StrCat("duplicate resolvant ", resolvant));
        }
        // The resolvant clause must contain pivot.Negated().
        absl::Span<const Literal> resolvant_literals = resolvant.GetLiterals();
        if (!absl::c_linear_search(resolvant_literals, pivot.Negated())) {
          return Error(ptr, absl::StrCat("missing negated pivot in resolvant ",
                                         resolvant));
        }
        // Its unit propagation proof must be correct, unless `clause` and
        // `resolvant` have two complementary literals (other than the pivot --
        // this is still valid if we use `tmp_false_literals_set_` instead of
        // `clause`).
        tmp_rat_false_literals_set_.CopyFrom(tmp_false_literals_set_);
        bool has_two_complementary_literals = false;
        for (const Literal literal : resolvant_literals) {
          if (literal == pivot.Negated()) continue;
          if (tmp_false_literals_set_[literal.Negated()]) {
            has_two_complementary_literals = true;
            break;
          }
          tmp_rat_false_literals_set_.Set(literal);
        }
        if (has_two_complementary_literals) continue;
        last_propagation_status = kUnit;
        for (int j = 0; j < rat_clauses.rup_clauses.size(); ++j) {
          const ClausePtr rup_clause = rat_clauses.rup_clauses[j];
          DCHECK(type == kRewrittenClause ||
                 !EqualSatClausePtrs(rup_clause, ptr));
          if constexpr (DEBUG_MODE) {
            if (!DebugCheckProofClauseId(ptr, rup_clause)) return false;
          }
          ++num_processed_rat_clauses_;
          last_propagation_status =
              Propagate(rup_clause, tmp_rat_false_literals_set_);
          if (last_propagation_status == kError) {
            return Error(ptr, absl::StrCat("rat_clauses.rup_clause ",
                                           rup_clause, " is not unit"));
          } else if (last_propagation_status == kWarning) {
            last_propagation_status = kUnit;
            ++num_unneeded_rat_literals_;
          } else if (last_propagation_status == kConflict) {
            num_unneeded_rat_clauses_ += rat_clauses.rup_clauses.size() - j - 1;
            break;
          }
        }
        if (last_propagation_status != kConflict) {
          return Error(
              ptr, absl::StrCat("last rup_clause for rat_clauses.resolvant ",
                                resolvant, " is not a conflict"));
        }
      }
    }
  }

  if (rat_enabled_) {
    for (const Literal literal : clause) {
      occurrences_[literal.Index()]++;
    }
    if (type == kRewrittenClause) {
      // A rewrite is like removing and adding the same clause. To get correct
      // occurrence values we need to decrement the occurrences for the removed
      // literals (incrementing them for the added literals was done above).
      tmp_false_literals_set_.ClearAndResize(LiteralIndex(2 * num_variables));
      for (const Literal literal : ptr.GetLiterals()) {
        if (tmp_false_literals_set_[literal]) continue;
        tmp_false_literals_set_.Set(literal);
        occurrences_[literal.Index()]--;
      }
    }
  }
  if constexpr (DEBUG_MODE) {
    debug_clause_by_ptr_[ptr] = clause;
  }
  if (clause.empty()) {
    complete_ = true;
  }
  return true;
}

bool LratChecker::DebugCheckProofClauseId(ClausePtr clause,
                                          ClausePtr proof_clause) {
  if (proof_clause == kNullClausePtr) {
    return Error(clause, "null proof clause pointer");
  }
  auto it = debug_clause_by_ptr_.find(proof_clause);
  if (it == debug_clause_by_ptr_.end()) {
    return Error(clause,
                 absl::StrCat("proof clause not found: ", proof_clause, " ",
                              absl::StrJoin(proof_clause.GetLiterals(), ",")));
  }
  absl::btree_set<Literal> expected_literals;
  for (const Literal literal : it->second) {
    expected_literals.insert(literal);
  }
  absl::btree_set<Literal> actual_literals;
  for (const Literal literal : proof_clause.GetLiterals()) {
    actual_literals.insert(literal);
  }
  if (actual_literals != expected_literals) {
    return Error(
        clause,
        absl::StrCat("proof clause ", proof_clause, ": unexpected literals ",
                     absl::StrJoin(actual_literals, ","), " (expected ",
                     absl::StrJoin(expected_literals, ","), ")"));
  }
  return true;
}

bool LratChecker::Error(ClausePtr clause, std::string_view error) {
  if (valid_) {
    error_message_ = absl::StrCat("In clause ", clause, ": ", error);
    valid_ = false;
  }
  return false;
}

}  // namespace sat
}  // namespace operations_research
