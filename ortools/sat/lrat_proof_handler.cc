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

#include "ortools/sat/lrat_proof_handler.h"

#include <memory>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_join.h"
#include "absl/types/span.h"
#include "ortools/sat/lrat_checker.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"

namespace operations_research {
namespace sat {

LratProofHandler::LratProofHandler(Model* model)
    : lrat_checker_(std::make_unique<LratChecker>(model)),
      debug_crash_on_error_(model->GetOrCreate<SatParameters>()
                                ->debug_crash_if_lrat_check_fails()) {}

bool LratProofHandler::AddProblemClause(ClauseId id,
                                        absl::Span<const Literal> clause) {
  VLOG(1) << "AddProblemClause: id=" << id
          << " literals=" << absl::StrJoin(clause, ",");
  return CheckResult(lrat_checker_->AddProblemClause(id, clause));
}

bool LratProofHandler::AddInferredClause(
    ClauseId id, absl::Span<const Literal> clause,
    absl::Span<const ClauseId> unit_ids,
    absl::Span<const LratChecker::RatIds> rat) {
  VLOG(1) << "AddInferredClause: id=" << id
          << " literals=" << absl::StrJoin(clause, ",")
          << " unit_ids=" << absl::StrJoin(unit_ids, ",") << " rat={"
          << absl::StrJoin(rat, " ") << "}";
  return CheckResult(
      lrat_checker_->AddInferredClause(id, clause, unit_ids, rat));
}

bool LratProofHandler::AddAssumedClause(ClauseId id,
                                        absl::Span<const Literal> clause) {
  VLOG(1) << "AddAssumedClause: id=" << id
          << " literals=" << absl::StrJoin(clause, ",");
  if (debug_crash_on_error_) {
    LOG(FATAL) << "LRAT error: assumed clauses are not supposed to happen";
  }
  return CheckResult(lrat_checker_->AddProblemClause(id, clause));
}

void LratProofHandler::DeleteClauses(absl::Span<const ClauseId> clause_ids) {
  VLOG(1) << "DeleteClauses: clause_ids=" << absl::StrJoin(clause_ids, " ");
  lrat_checker_->DeleteClauses(clause_ids);
}

bool LratProofHandler::Check() const {
  return CheckResult(lrat_checker_->Check());
}

bool LratProofHandler::CheckResult(bool result) const {
  if (debug_crash_on_error_ && !result) {
    LOG(FATAL) << "LRAT error: " << lrat_checker_->error_message();
  }
  return result;
}

}  // namespace sat
}  // namespace operations_research
