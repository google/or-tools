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
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_join.h"
#include "absl/types/span.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/synchronization.h"
#include "ortools/util/bitset.h"

namespace operations_research {
namespace sat {

// An incremental checker for LRAT proofs (https://arxiv.org/abs/1612.02353).
class LratChecker {
 public:
  explicit LratChecker(Model* model)
      : LratChecker(model->GetOrCreate<SharedStatistics>()) {}
  explicit LratChecker(SharedStatistics* stats) : stats_(stats) {}

  // The clauses used in a proof that a clause has a Resolution Asymmetric
  // Tautology (RAT) property. See AddInferredClause() for more details.
  struct RatClauses {
    ClausePtr resolvant;
    absl::Span<const ClausePtr> rup_clauses;
  };

  // Enables the support of inferred clauses with RAT proofs (disabled by
  // default). This must be called before adding any problem or inferred clause.
  // Adds a memory and time overhead to the verification of all proofs, even if
  // they do not use RAT.
  void EnableRatProofs();

  // Adds a clause of the problem. Does nothing if a previous step failed or if
  // the proof is already complete, or if the clause contains a literal and its
  // negation (since it is always true, it should not be needed to infer
  // anything). Always returns true.
  //
  // Problem clauses can be added after inferred clauses which do not reference
  // them. This can be used to add learned clauses proved by another worker, or
  // "axioms" that we admit without proof.
  //
  // If a clause with the same pointer has already been added, this redefines
  // it. This can happen, for instance, if a unit or binary clause is added
  // several times (since the pointer is computed from the clause literals).
  bool AddProblemClause(ClausePtr clause);

  // Adds a clause which is inferred from the problem clauses and/or the
  // previously inferred clauses (that have not been deleted; they are called
  // the 'current' clauses). Does nothing if a previous step failed or if the
  // proof is already complete, or if the clause contains a literal and its
  // negation (since it is always true, it should not be needed to infer
  // anything). Otherwise, returns true if the given inference proof is valid,
  // i.e., if the following conditions hold (checked sequentially):
  //
  // 1) The `rup_clauses` are or become unit and eventually empty if all the
  //    literals of `clause` are assumed to be false (verification stops at the
  //    first empty clause). This list must be in unit propagation order: if a
  //    clause c becomes unit (or empty) because clauses c_1, ..., c_k are unit,
  //    then c must appear after c_1, ..., c_k in the list. Let RUP be all the
  //    literals which are found to be false by unit propagation. WARNING: there
  //    is no check that the `rup_clauses` are existing problem clauses or
  //    already inferred clauses!
  // 2) If `rat_clauses` is empty, the last `rup_clauses` must become empty
  //    after unit propagation.
  // 3) Otherwise `clause` must not be empty, and `rat_clauses` must contain
  //    all the current clauses which contain the negation of the first
  //    literal of `clause` (called the pivot 'p') -- and no other clauses.
  //    Moreover, for each r in `rat_clauses`:
  //    * Either `clause` and `r.resolvant` have two pairs of complementary
  //      literals.
  //    * Or all the `r.rup_clauses` must become unit and eventually empty
  //      if all the literals of `clause` and of the `r.resolvant_id` clause
  //      (minus ~p), as well as those in RUP (from condition 2), are assumed to
  //      be false (this list must be in unit propagation order, as explained
  //      above; verification stops at the first empty clause).
  //    WARNING: there is no check that the `r.resolvant` and `r.rup_clauses`
  //    are existing problem clauses or already inferred clauses!
  //
  // If a clause with the same pointer has already been added, this redefines
  // it. This can happen, for instance, if a unit or binary clause is inferred
  // several times (since the pointer is computed from the clause literals). To
  // redefine a SatClause clause, use RewriteClause() instead.
  bool AddInferredClause(ClausePtr clause,
                         absl::Span<const ClausePtr> rup_clauses,
                         absl::Span<const RatClauses> rat_clauses = {});

  // Rewrites a problem or inferred clause. Same as AddInferredClause() but with
  // clause literals taken from `literals` instead of from `clause`.
  bool RewriteClause(ClausePtr clause, absl::Span<const Literal> literals,
                     absl::Span<const ClausePtr> rup_clauses,
                     absl::Span<const RatClauses> rat_clauses = {});

  // Deletes problem or inferred clauses. It is OK to delete a clause which has
  // already been deleted or has never been added.
  void DeleteClauses(absl::Span<const ClausePtr> clauses);

  // Returns true if all the operations made so far were valid.
  bool Valid() const { return valid_; }

  // Returns true if the unsatisfiability proof is valid and complete, i.e.
  // whether the empty clause has been successfully inferred.
  bool Check() {
    if (valid_ && !complete_) {
      error_message_ = "empty clause not inferred";
    }
    return complete_;
  }

  void AddStats() const;

  // Returns the reason of the first failed operation, or an empty string if all
  // operations were successful.
  std::string_view error_message() const { return error_message_; }

 private:
  // Set this to true to check that clauses used in proofs have already been
  // added as problem or inferred clauses before, and have not been modified or
  // deleted since. This can be used to debug invalid LRAT proofs.
  static constexpr bool kDebugCheckProofClauses = false;

  enum UnitPropagationStatus {
    kUnit = 1,
    kConflict = 2,
    kWarning = 3,
    kError = 4,
  };
  UnitPropagationStatus Propagate(
      ClausePtr clause, SparseBitset<LiteralIndex>& false_literals_set);

  enum ClauseType { kProblemClause, kInferredClause, kRewrittenClause };
  bool AddClauseInternal(ClauseType type, ClausePtr ptr,
                         absl::Span<const Literal> literals,
                         absl::Span<const ClausePtr> rup_clauses,
                         absl::Span<const RatClauses> rat_clauses);

  // Checks that a clause used in a proof has already been added as a problem
  // or inferred clause before, and has not been modified or deleted since.
  // This is only used if kDebugCheckProofClauses is true.
  bool DebugCheckProofClauseId(ClausePtr clause, ClausePtr proof_clause);

  bool Error(ClausePtr clause, std::string_view error);

  bool rat_enabled_ = false;
  int num_variables_ = 0;

  // The number of clauses which contain each literal.
  // This is only used if rat_enabled_ is true.
  util_intops::StrongVector<LiteralIndex, int> occurrences_;

  // Whether all the operations made so far were valid.
  bool valid_ = true;
  std::string error_message_;

  // Whether the proof is complete, i.e., whether the empty clause has been
  // successfully inferred.
  bool complete_ = false;

  // Statistics.
  int64_t num_problem_clauses_ = 0;
  int64_t num_inferred_clauses_ = 0;
  int64_t num_inferred_clauses_always_true_ = 0;
  int64_t num_processed_rup_literals_ = 0;
  int64_t num_processed_rup_clauses_ = 0;
  int64_t num_unneeded_rup_literals_ = 0;
  int64_t num_unneeded_rup_clauses_ = 0;
  int64_t num_processed_rat_literals_ = 0;
  int64_t num_processed_rat_clauses_ = 0;
  int64_t num_unneeded_rat_literals_ = 0;
  int64_t num_unneeded_rat_clauses_ = 0;
  int64_t num_deleted_clauses_ = 0;

  std::vector<Literal> tmp_clause_;
  // Temporary set used to get the unique literals of a clause.
  SparseBitset<LiteralIndex> tmp_marked_literals_;
  // Temporary sets used to check unit propagation proofs.
  SparseBitset<LiteralIndex> tmp_false_literals_set_;
  SparseBitset<LiteralIndex> tmp_rat_false_literals_set_;

  // Temporary set used to check the RAT property of an inferred clause.
  absl::flat_hash_set<ClausePtr> tmp_clauses_;

  // Only used if kDebugCheckProofClauses is true.
  absl::flat_hash_map<ClausePtr, std::vector<Literal>> debug_clause_by_ptr_;

  SharedStatistics* stats_;
};

template <typename Sink, typename... T>
void AbslStringify(Sink& sink, LratChecker::RatClauses arg) {
  absl::Format(&sink, "resolvant=%v rup_clauses=[%s]", arg.resolvant,
               absl::StrJoin(arg.rup_clauses, " "));
}

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_LRAT_CHECKER_H_
