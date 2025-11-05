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

#ifndef ORTOOLS_SAT_LRAT_PROOF_HANDLER_H_
#define ORTOOLS_SAT_LRAT_PROOF_HANDLER_H_

#include <memory>

#include "absl/types/span.h"
#include "ortools/sat/lrat_checker.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"

namespace operations_research {
namespace sat {

// Handles the LRAT proof of a SAT problem by either checking it incrementally
// and/or by saving it to a file.
class LratProofHandler {
 public:
  // TODO(user): Add a constructor to save the proof to a file in addition
  // to or instead of using the LratChecker.
  explicit LratProofHandler(Model* model);

  // Adds a clause of the problem. See LratChecker for more details.
  bool AddProblemClause(ClauseId id, absl::Span<const Literal> clause);

  // Adds a clause which is inferred from the problem clauses and/or the
  // previously inferred clauses. See LratChecker for more details.
  bool AddInferredClause(ClauseId id, absl::Span<const Literal> clause,
                         absl::Span<const ClauseId> unit_ids,
                         absl::Span<const LratChecker::RatIds> rat = {});

  // Adds a clause which is assumed to be true, without proof.
  bool AddAssumedClause(ClauseId id, absl::Span<const Literal> clause);

  // Deletes problem or inferred clauses.
  void DeleteClauses(absl::Span<const ClauseId> clause_ids);

  // Returns true if the unsatisfiability proof is valid and complete, i.e.
  // whether the empty clause has been successfully inferred.
  bool Check() const;

 private:
  bool CheckResult(bool result) const;

  std::unique_ptr<LratChecker> lrat_checker_;
  bool debug_crash_on_error_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_LRAT_PROOF_HANDLER_H_
