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

#ifndef ORTOOLS_SAT_LRAT_CHECKER_H_
#define ORTOOLS_SAT_LRAT_CHECKER_H_

#include <stddef.h>

#include <cstdint>
#include <string>
#include <string_view>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_join.h"
#include "absl/types/span.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"

namespace operations_research {
namespace sat {

// An incremental checker for LRAT proofs (https://arxiv.org/abs/1612.02353).
class LratChecker {
 public:
  explicit LratChecker(Model* model)
      : stats_(model->GetOrCreate<SharedStatistics>()) {}
  ~LratChecker();

  // The clause IDs used in a proof that a clause has a Resolution Asymmetric
  // Tautology (RAT) property. See AddInferredClause() for more details.
  struct RatIds {
    ClauseId resolvant_id;
    absl::Span<const ClauseId> unit_ids;
  };

  // Adds a clause of the problem. Returns true if successful, i.e., if the
  // given ID is not already used by another clause (it is OK to reuse the ID of
  // a deleted clause). Does nothing if a previous step failed or if the proof
  // is already complete, or if the clause contains a literal and its negation
  // (since it is always true, it should not be needed to infer anything).
  //
  // Problem clauses can be added after inferred clauses which do not reference
  // them. This can be used to add learned clauses proved by another worker, or
  // "axioms" that we admit without proof.
  bool AddProblemClause(ClauseId id, absl::Span<const Literal> clause);

  // Adds a clause which is inferred from the problem clauses and/or the
  // previously inferred clauses (that have not been deleted; they are called
  // the 'current' clauses). Does nothing if a previous step failed or if the
  // proof is already complete, or if the clause contains a literal and its
  // negation (since it is always true, it should not be needed to infer
  // anything). Otherwise, returns true if the given inference proof is valid,
  // i.e., if the following conditions hold (checked sequentially):
  //
  // 1) The clause ID is not already used by another clause (it is OK to reuse
  //    the ID of a deleted clause).
  // 2) The `unit_ids` clauses exist and are or become unit and eventually empty
  //    if all the literals of `clause` are assumed to be false (verification
  //    stops at the first empty clause). This list must be in unit propagation
  //    order: if a clause c becomes unit (or empty) because clauses c_1, ...,
  //    c_k are unit, then c must appear after c_1, ..., c_k in the list. Let
  //    RUP be all the literals which are found to be false by unit propagation.
  // 3) If `rat` is empty, the last `unit_ids` clause must become empty after
  //    unit propagation.
  // 4) Otherwise `clause` must not be empty, and `rat_clauses` must contain
  //    all the current clauses which contain the negation of the first
  //    literal of `clause` (called the pivot 'p') -- and no other clauses.
  //    Moreover, for each r in `rat`:
  //    * Either `clause` and the `r.resolvant_id` clause have two pairs of
  //      complementary literals.
  //    * Or all the `r.unit_ids` clauses must become unit and eventually empty
  //      if all the literals of `clause` and of the `r.resolvant_id` clause
  //      (minus ~p), as well as those in RUP (from condition 2), are assumed to
  //      be false (this list must be in unit propagation order, as explained
  //      above; verification stops at the first empty clause).
  bool AddInferredClause(ClauseId id, absl::Span<const Literal> clause,
                         absl::Span<const ClauseId> unit_ids,
                         absl::Span<const RatIds> rat = {});

  // Deletes problem or inferred clauses. It is OK to delete a clause which has
  // already been deleted or has never been added.
  void DeleteClauses(absl::Span<const ClauseId> clause_ids);

  // Returns true if the unsatisfiability proof is valid and complete, i.e.
  // whether the empty clause has been successfully inferred.
  bool Check() {
    if (valid_ && !complete_) {
      error_message_ = "empty clause not inferred";
    }
    return complete_;
  }

  // Returns the reason of the first failed operation, or an empty string if all
  // operations were successful.
  std::string_view error_message() const { return error_message_; }

 private:
  bool AddClauseInternal(ClauseId id, absl::Span<const Literal> clause,
                         bool is_problem_clause,
                         absl::Span<const ClauseId> unit_ids,
                         absl::Span<const RatIds> rat);

  bool Error(ClauseId id, std::string_view error);

  // The problem and inferred clauses which have not been deleted. The clause
  // literals are sorted and without duplicates.
  // TODO(user): investigate more cache friendly data structures (could be
  // more efficient but their correctness could be harder to trust).
  absl::flat_hash_map<ClauseId, FixedCapacityVector<Literal>> clauses_;

  // The number of clauses in `clauses_` which contain each literal.
  absl::flat_hash_map<Literal, int> occurrences_;

  // Whether all the operations made so far were valid.
  bool valid_ = true;
  std::string error_message_;

  // Statistics.
  int64_t num_problem_clauses_ = 0;
  int64_t num_inferred_clauses_ = 0;
  int64_t num_processed_rup_literals_ = 0;
  int64_t num_processed_rup_clauses_ = 0;
  int64_t num_unneeded_rup_literals_ = 0;
  int64_t num_unneeded_rup_clauses_ = 0;
  int64_t num_processed_rat_literals_ = 0;
  int64_t num_processed_rat_clauses_ = 0;
  int64_t num_unneeded_rat_literals_ = 0;
  int64_t num_unneeded_rat_clauses_ = 0;
  int64_t num_deleted_clauses_not_found_ = 0;

  // Whether the proof is complete, i.e., whether the empty clause has been
  // successfully inferred.
  bool complete_ = false;

  // Temporary sets used to check unit propagation proofs.
  absl::flat_hash_set<Literal> tmp_false_literals_set_;
  absl::flat_hash_set<Literal> tmp_rat_false_literals_set_;

  // Temporary set used to check the RAT property of an inferred clause.
  absl::flat_hash_set<ClauseId> tmp_clause_ids_;

  SharedStatistics* stats_;
};

template <typename Sink, typename... T>
void AbslStringify(Sink& sink, LratChecker::RatIds arg) {
  absl::Format(&sink, "resolvant_id=%d unit_ids=[%s]", arg.resolvant_id.value(),
               absl::StrJoin(arg.unit_ids, " "));
}

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_LRAT_CHECKER_H_
