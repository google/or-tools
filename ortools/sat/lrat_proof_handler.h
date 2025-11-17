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

#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

#include "absl/types/span.h"
#include "ortools/base/file.h"
#include "ortools/sat/drat_checker.h"
#include "ortools/sat/drat_writer.h"
#include "ortools/sat/lrat_checker.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"

namespace operations_research {
namespace sat {

// Handles the LRAT proof of a SAT problem by either checking it incrementally
// and/or by saving it to a file.
class LratProofHandler {
 public:
  explicit LratProofHandler(Model* model, bool check_lrat, bool check_drat,
                            File* drat_output, bool in_binary_drat_format);

  bool lrat_check_enabled() const { return lrat_checker_ != nullptr; }
  bool drat_check_enabled() const { return drat_checker_ != nullptr; }
  bool drat_output_enabled() const { return drat_writer_ != nullptr; }

  // Adds a clause of the problem. See LratChecker for more details.
  bool AddProblemClause(ClauseId id, absl::Span<const Literal> clause);

  // No more problem clauses must be added after this call.
  void EndProblemClauses();

  // Adds a clause which is inferred from the problem clauses and/or the
  // previously inferred clauses. See LratChecker for more details.
  bool AddInferredClause(ClauseId id, absl::Span<const Literal> clause,
                         absl::Span<const ClauseId> unit_ids,
                         absl::Span<const LratChecker::RatIds> rat = {});

  // Adds a clause which was inferred by another worker. Returns true if
  // successful (the operation can fail if LRAT checks are enabled, and the ID
  // is already used by another clause).
  bool AddSharedClause(ClauseId id, absl::Span<const Literal> clause);

  // Adds a clause which is assumed to be true, without proof. Returns true if
  // successful (the operation fails if DRAT checks are enabled, or if LRAT
  // checks are enabled and the ID is already used by another clause).
  bool AddAssumedClause(ClauseId id, absl::Span<const Literal> clause);

  // Deletes a problem or inferred clause. The clause literals are only needed
  // when checking DRAT.
  void DeleteClause(ClauseId id, absl::Span<const Literal> clause);

  // Returns VALID if all the inferred clauses were successfully checked with
  // LRAT. Returns INVALID if at least one of them was not. Returns UNKNOWN if
  // LRAT checks are not enabled.
  DratChecker::Status Valid() const;

  // Returns VALID if the unsatisfiability proof is valid and complete, i.e.
  // whether the empty clause has been successfully inferred. Returns INVALID if
  // it is not. Returns UNKNOWN if the check timed out (this can only occur
  // with DRAT checks), or if neither LRAT nor DRAT checks were enabled.
  DratChecker::Status Check(double max_drat_check_time_in_seconds =
                                std::numeric_limits<double>::infinity());

  void AddStats() const;

  int64_t num_assumed_clauses() const { return num_assumed_clauses_; }

 private:
  bool CheckResult(bool result) const;

  std::unique_ptr<LratChecker> lrat_checker_;
  std::unique_ptr<DratChecker> drat_checker_;
  std::unique_ptr<DratWriter> drat_writer_;

  bool all_problem_clauses_loaded_ = false;
  int64_t num_assumed_clauses_ = 0;
  bool debug_crash_on_error_;

  // Only used when checking DRAT, because the DRAT checker does not support
  // interleaving problem and inferred clauses.
  std::vector<std::vector<Literal>> clauses_inferred_during_problem_loading_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_LRAT_PROOF_HANDLER_H_
