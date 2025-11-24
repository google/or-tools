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

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_join.h"
#include "absl/types/span.h"
#include "ortools/base/file.h"
#include "ortools/base/options.h"
#include "ortools/sat/drat_checker.h"
#include "ortools/sat/drat_writer.h"
#include "ortools/sat/lrat_checker.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"

#if defined(_MSC_VER)
ABSL_FLAG(std::string, cp_model_drat_output, ".\\drat.txt",
          "File name for the generated DRAT proof, if DRAT output is enabled.");
#else
ABSL_FLAG(std::string, cp_model_drat_output, "/tmp/drat.txt",
          "File name for the generated DRAT proof, if DRAT output is enabled.");
#endif

namespace operations_research {
namespace sat {

namespace {
std::vector<Literal> SortClauseForDrat(absl::Span<const Literal> clause) {
  // The sorting is such that new variables appear first. This is important for
  // BVA since DRAT-trim only check the RAT property with respect to the first
  // variable of the clause.
  std::vector<Literal> sorted_clause(clause.begin(), clause.end());
  std::sort(sorted_clause.begin(), sorted_clause.end(),
            [](Literal a, Literal b) {
              return std::abs(a.SignedValue()) > std::abs(b.SignedValue());
            });
  return sorted_clause;
}
}  // namespace

std::unique_ptr<LratProofHandler> LratProofHandler::MaybeCreate(Model* model) {
  const SatParameters& params = *model->GetOrCreate<SatParameters>();
  if (!params.check_lrat_proof() && !params.check_drat_proof() &&
      !params.output_drat_proof()) {
    return nullptr;
  }
  return std::unique_ptr<LratProofHandler>(new LratProofHandler(model));
}

LratProofHandler::LratProofHandler(Model* model) {
  const SatParameters& params = *model->GetOrCreate<SatParameters>();
  if (params.check_lrat_proof()) {
    lrat_checker_ = std::make_unique<LratChecker>(model);
  }
  if (params.check_drat_proof()) {
    drat_checker_ = std::make_unique<DratChecker>();
  }
  if (params.output_drat_proof()) {
    File* drat_output = nullptr;
    CHECK_OK(file::Open(absl::GetFlag(FLAGS_cp_model_drat_output), "w",
                        &drat_output, file::Defaults()));
    drat_writer_ = std::make_unique<DratWriter>(
        /*in_binary_drat_format=*/false, drat_output);
  }
  max_drat_time_in_seconds_ = params.max_drat_time_in_seconds();
  debug_crash_on_error_ = params.debug_crash_if_lrat_check_fails();
}

bool LratProofHandler::AddProblemClause(ClauseId id,
                                        absl::Span<const Literal> clause) {
  VLOG(1) << "AddProblemClause: id=" << id
          << " literals=" << absl::StrJoin(clause, ",");
  if (all_problem_clauses_loaded_ && debug_crash_on_error_) {
    LOG(FATAL) << "LRAT error: problem clauses must not be added after "
                  "EndProblemClauses()";
  }
  if (lrat_checker_ != nullptr) {
    return CheckResult(lrat_checker_->AddProblemClause(id, clause));
  }
  if (drat_checker_ != nullptr) {
    drat_checker_->AddProblemClause(SortClauseForDrat(clause));
  }
  return true;
}

void LratProofHandler::EndProblemClauses() {
  all_problem_clauses_loaded_ = true;
  if (drat_checker_ != nullptr) {
    for (const auto& clause : clauses_inferred_during_problem_loading_) {
      drat_checker_->AddInferredClause(clause);
    }
    clauses_inferred_during_problem_loading_.clear();
  }
}

bool LratProofHandler::AddInferredClause(
    ClauseId id, absl::Span<const Literal> clause,
    absl::Span<const ClauseId> unit_ids,
    absl::Span<const LratChecker::RatIds> rat) {
  VLOG(1) << "AddInferredClause: id=" << id
          << " literals=" << absl::StrJoin(clause, ",")
          << " unit_ids=" << absl::StrJoin(unit_ids, ",") << " rat={"
          << absl::StrJoin(rat, " ") << "}";
  if (lrat_checker_ != nullptr) {
    return CheckResult(
        lrat_checker_->AddInferredClause(id, clause, unit_ids, rat));
  }
  if (drat_checker_ != nullptr) {
    if (all_problem_clauses_loaded_) {
      drat_checker_->AddInferredClause(SortClauseForDrat(clause));
    } else {
      clauses_inferred_during_problem_loading_.push_back(
          SortClauseForDrat(clause));
    }
  }
  if (drat_writer_ != nullptr) {
    drat_writer_->AddClause(clause);
  }
  return true;
}

bool LratProofHandler::AddSharedClause(ClauseId id,
                                       absl::Span<const Literal> clause) {
  VLOG(1) << "AddSharedClause: id=" << id
          << " literals=" << absl::StrJoin(clause, ",");
  if (lrat_checker_ != nullptr) {
    return CheckResult(lrat_checker_->AddProblemClause(id, clause));
  }
  if (drat_checker_ != nullptr) {
    LOG(ERROR) << "Shared clauses are not supported by the DRAT checker.";
    return false;
  }
  return true;
}

bool LratProofHandler::AddAssumedClause(ClauseId id,
                                        absl::Span<const Literal> clause) {
  VLOG(1) << "AddAssumedClause: id=" << id
          << " literals=" << absl::StrJoin(clause, ",");
  if (debug_crash_on_error_) {
    LOG(FATAL) << "LRAT error: assumed clauses are not supposed to happen";
  }
  ++num_assumed_clauses_;
  if (lrat_checker_ != nullptr) {
    return CheckResult(lrat_checker_->AddProblemClause(id, clause));
  }
  if (drat_checker_ != nullptr) {
    // The DRAT checker requires all problem clauses first, followed by inferred
    // clauses only.
    LOG(ERROR) << "Assumed clauses are not supported by the DRAT checker.";
    return false;
  }
  return true;
}

void LratProofHandler::PinClause(ClauseId id,
                                 absl::Span<const Literal> clause) {
  DCHECK_NE(id, kNoClauseId);
  DCHECK_EQ(pinned_clause_id_, kNoClauseId);
  pinned_clause_id_ = id;
  if (drat_checker_ != nullptr || drat_writer_ != nullptr) {
    pinned_clause_.assign(clause.begin(), clause.end());
  }
  delete_pinned_clause_ = false;
}

void LratProofHandler::UnpinClause(ClauseId id) {
  DCHECK_NE(id, kNoClauseId);
  DCHECK_EQ(pinned_clause_id_, id);
  pinned_clause_id_ = kNoClauseId;
  if (delete_pinned_clause_) {
    DeleteClause(id, pinned_clause_);
  }
}

void LratProofHandler::DeleteClause(ClauseId id,
                                    absl::Span<const Literal> clause) {
  if (pinned_clause_id_ == id) {
    delete_pinned_clause_ = true;
    return;
  }
  VLOG(1) << "DeleteClause: id=" << id
          << " literals=" << absl::StrJoin(clause, ",");
  if (drat_checker_ != nullptr) {
    drat_checker_->DeleteClause(clause);
  }
  if (drat_writer_ != nullptr) {
    drat_writer_->DeleteClause(clause);
  }
  if (lrat_checker_ != nullptr) {
    lrat_checker_->DeleteClauses({id});
  }
}

DratChecker::Status LratProofHandler::Valid() const {
  if (lrat_checker_ != nullptr) {
    return CheckResult(lrat_checker_->Valid()) ? DratChecker::Status::VALID
                                               : DratChecker::Status::INVALID;
  }
  return DratChecker::Status::UNKNOWN;
}

DratChecker::Status LratProofHandler::Check() {
  DratChecker::Status status = DratChecker::Status::UNKNOWN;
  if (lrat_checker_ != nullptr) {
    status = CheckResult(lrat_checker_->Check()) ? DratChecker::Status::VALID
                                                 : DratChecker::Status::INVALID;
  }
  if (status != DratChecker::Status::INVALID && drat_checker_ != nullptr) {
    drat_checker_->Check(max_drat_time_in_seconds_);
    if (status == DratChecker::Status::INVALID && debug_crash_on_error_) {
      LOG(FATAL) << "DRAT check failed";
    }
  }
  return status;
}

bool LratProofHandler::CheckResult(bool result) const {
  if (debug_crash_on_error_ && !result && lrat_checker_ != nullptr) {
    LOG(FATAL) << "LRAT error: " << lrat_checker_->error_message();
  }
  return result;
}

void LratProofHandler::AddStats() const {
  if (lrat_checker_ != nullptr) {
    lrat_checker_->AddStats();
  }
}

}  // namespace sat
}  // namespace operations_research
