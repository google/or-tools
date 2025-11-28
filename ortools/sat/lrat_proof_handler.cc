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
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <ios>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/types/span.h"
#include "ortools/base/file.h"
#include "ortools/base/options.h"
#include "ortools/base/timer.h"
#include "ortools/sat/drat_checker.h"
#include "ortools/sat/drat_writer.h"
#include "ortools/sat/lrat.pb.h"
#include "ortools/sat/lrat_checker.h"
#include "ortools/sat/model.h"
#include "ortools/sat/recordio.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/synchronization.h"

#if defined(_MSC_VER)
ABSL_FLAG(std::string, cp_model_drat_output, ".\\drat.txt",
          "File name for the generated DRAT proof, if DRAT output is enabled.");
ABSL_FLAG(std::string, cp_model_lrat_output_prefix, ".\\lrat",
          "File name prefix for the generated LRAT proof files, if LRAT output "
          "is enabled. One file is created for each worker.");
#else
ABSL_FLAG(std::string, cp_model_drat_output, "/tmp/drat.txt",
          "File name for the generated DRAT proof, if DRAT output is enabled.");
ABSL_FLAG(std::string, cp_model_lrat_output_prefix, "/tmp/lrat",
          "File name prefix for the generated LRAT proof files, if LRAT output "
          "is enabled. One file is created for each worker.");
#endif

namespace operations_research {
namespace sat {

LratWriter::LratWriter(std::string_view filename)
    : filename_(filename),
      ofstream_(filename_, std::ios::binary),
      writer_(&ofstream_) {
  if (!ofstream_.good()) {
    LOG(FATAL) << "Failed to open LRAT output file: " << filename;
  }
}

LratWriter::~LratWriter() { writer_.Close(); }

void LratWriter::AddImportedClause(ClauseId id,
                                   absl::Span<const Literal> clause) {
  LratProofStep step;
  LratImportedClause* imported_clause = step.mutable_imported_clause();
  imported_clause->set_clause_id(id.value());
  for (const Literal literal : clause) {
    imported_clause->add_literals(literal.Index().value());
  }
  CHECK(writer_.WriteRecord(step));
}

void LratWriter::AddInferredClause(ClauseId id,
                                   absl::Span<const Literal> clause,
                                   absl::Span<const ClauseId> unit_ids,
                                   absl::Span<const LratChecker::RatIds> rat) {
  LratProofStep step;
  LratInferredClause* inferred_clause = step.mutable_inferred_clause();
  inferred_clause->set_clause_id(id.value());
  for (const Literal literal : clause) {
    inferred_clause->add_literals(literal.Index().value());
  }
  for (const ClauseId unit_id : unit_ids) {
    inferred_clause->add_unit_ids(unit_id.value());
  }
  for (const LratChecker::RatIds& rat_ids : rat) {
    LratInferredClause::RatInfo* rat_info = inferred_clause->add_rat_infos();
    rat_info->set_resolvant_id(rat_ids.resolvant_id.value());
    for (const ClauseId unit_id : rat_ids.unit_ids) {
      rat_info->add_unit_ids(unit_id.value());
    }
  }
  CHECK(writer_.WriteRecord(step));
}

void LratWriter::DeleteClause(ClauseId id) {
  LratProofStep step;
  step.mutable_deleted_clauses()->add_clause_ids(id.value());
  CHECK(writer_.WriteRecord(step));
}

namespace {
void IndicesToLiterals(absl::Span<const int> literal_indices,
                       std::vector<Literal>* literals) {
  literals->clear();
  literals->reserve(literal_indices.size());
  for (const int lit : literal_indices) {
    literals->push_back(Literal(LiteralIndex(lit)));
  }
}
}  //  namespace

LratMerger::LratMerger(Model* model)
    : id_(model->GetOrCreate<SharedLratProofStatus>()->NewSubSolverId()),
      proof_status_(model->GetOrCreate<SharedLratProofStatus>()) {
  const SatParameters& params = *model->GetOrCreate<SatParameters>();
  if (params.check_merged_lrat_proof()) {
    lrat_checker_ = std::make_unique<LratChecker>(model);
  }
  debug_crash_on_error_ = params.debug_crash_if_lrat_check_fails();
}

LratMerger::~LratMerger() {
  DratChecker::Status status = DratChecker::Status::UNKNOWN;
  if (lrat_checker_ != nullptr) {
    status = lrat_checker_->Check() ? DratChecker::Status::VALID
                                    : DratChecker::Status::INVALID;
    if (status == DratChecker::Status::INVALID && debug_crash_on_error_) {
      LOG(FATAL) << "LRAT error: " << lrat_checker_->error_message();
    }
    lrat_checker_->AddStats();
  }
  proof_status_->NewSubsolverProofStatus(status, lrat_checker_ != nullptr,
                                         /*drat_check_enabled=*/false,
                                         /*num_assumed_clauses=*/0, 0.0);
}

bool LratMerger::Merge(absl::Span<const std::string> proof_filenames) {
  if (proof_filenames.empty()) return true;
  merged_proof_filename_ =
      absl::StrCat(absl::GetFlag(FLAGS_cp_model_lrat_output_prefix), ".txt");
  merged_proof_file_.open(merged_proof_filename_);
  if (!merged_proof_file_.good()) {
    return Error(absl::StrCat("failed to open LRAT output file: ",
                              merged_proof_filename_));
  }
  if (!ReadPresolveProof(proof_filenames[0])) return false;

  const int num_workers = proof_filenames.size() - 1;
  std::vector<std::ifstream> inputs(num_workers);
  std::vector<std::unique_ptr<RecordReader>> readers(num_workers);
  last_read_steps_.resize(num_workers);
  local_to_global_ids_.resize(num_workers);
  for (int i = 0; i < num_workers; ++i) {
    const std::string& filename = proof_filenames[i + 1];
    inputs[i].open(filename, std::ios::binary);
    if (!inputs[i].good()) {
      return Error(absl::StrCat("failed to open LRAT input file: ", filename));
    }
    readers[i] = std::make_unique<RecordReader>(&inputs[i]);
    if (!readers[i]->ReadRecord(&last_read_steps_[i])) {
      last_read_steps_[i].Clear();
    }
  }

  std::vector<Literal> clause;
  while (true) {
    bool at_least_one_step_read = false;
    int worker_with_missing_import = -1;
    for (int i = 0; i < num_workers; ++i) {
      const std::string& filename = proof_filenames[i + 1];
      // An empty step means that the reader is at the end of the file.
      bool missing_import = false;
      while (last_read_steps_[i].step_case() != LratProofStep::STEP_NOT_SET &&
             !missing_import) {
        LratProofStep& step = last_read_steps_[i];
        switch (step.step_case()) {
          case LratProofStep::kImportedClause: {
            ClauseId local_id(step.imported_clause().clause_id());
            IndicesToLiterals(step.imported_clause().literals(), &clause);
            std::sort(clause.begin(), clause.end());
            auto it = shared_clause_ids_.find(clause);
            if (it != shared_clause_ids_.end()) {
              local_to_global_ids_[i][local_id] = it->second;
            } else {
              missing_import = true;
            }
            break;
          }
          case LratProofStep::kInferredClause:
            if (!RemapInferredClause(i, filename,
                                     *step.mutable_inferred_clause())) {
              return false;
            }
            clause.clear();
            IndicesToLiterals(step.inferred_clause().literals(), &clause);
            std::sort(clause.begin(), clause.end());
            shared_clause_ids_.insert(
                {clause, GlobalId(step.inferred_clause().clause_id())});
            if (!WriteInferredClause(step.inferred_clause())) return false;
            // We found the empty clause, we don't need anymore steps.
            if (step.inferred_clause().literals().empty()) return true;
            break;
          case LratProofStep::kDeletedClauses:
            // TODO(user): implement this case.
            break;
          default:
            return Error(absl::StrCat("unknown step type ", step.step_case(),
                                      " in ", filename));
        }
        if (missing_import) {
          worker_with_missing_import = i;
        } else {
          if (!readers[i]->ReadRecord(&last_read_steps_[i])) {
            last_read_steps_[i].Clear();
          }
          at_least_one_step_read = true;
        }
      }
    }
    if (!at_least_one_step_read) {
      if (worker_with_missing_import >= 0) {
        const LratImportedClause& missing_import =
            last_read_steps_[worker_with_missing_import].imported_clause();
        clause.clear();
        IndicesToLiterals(missing_import.literals(), &clause);
        return Error(
            absl::StrCat("imported clause not found in ",
                         proof_filenames[worker_with_missing_import + 1],
                         ": id=", missing_import.clause_id(),
                         ", literals=", absl::StrJoin(clause, ",")));
      } else {
        return true;
      }
    }
  }
}

bool LratMerger::ReadPresolveProof(const std::string& filename) {
  std::ifstream input(filename, std::ios::binary);
  if (!input.good()) {
    return Error(absl::StrCat("failed to open LRAT input file: ", filename));
  }
  RecordReader reader(&input);
  LratProofStep step;
  std::vector<Literal> clause;
  GlobalId max_global_id(0);
  while (reader.ReadRecord(&step)) {
    switch (step.step_case()) {
      case LratProofStep::kImportedClause: {
        GlobalId global_id(step.imported_clause().clause_id());
        max_global_id = std::max(max_global_id, global_id);
        IndicesToLiterals(step.imported_clause().literals(), &clause);
        SortAndAddSharedClause(global_id, clause);
        if (lrat_checker_ != nullptr &&
            !lrat_checker_->AddProblemClause(ClauseId(global_id.value()),
                                             clause)) {
          return LratError();
        }
        break;
      }
      case LratProofStep::kInferredClause: {
        GlobalId global_id(step.inferred_clause().clause_id());
        max_global_id = std::max(max_global_id, global_id);
        IndicesToLiterals(step.inferred_clause().literals(), &clause);
        SortAndAddSharedClause(global_id, clause);
        if (!WriteInferredClause(step.inferred_clause())) return false;
        break;
      }
      case LratProofStep::kDeletedClauses:
        // TODO(user): implement this.
        break;
      default:
        return Error(absl::StrCat("unknown proof step type ", step.step_case(),
                                  " in ", filename));
    }
  }
  next_global_id_ = ++max_global_id;
  return true;
}

void LratMerger::SortAndAddSharedClause(GlobalId id,
                                        std::vector<Literal>& literals) {
  std::sort(literals.begin(), literals.end());
  shared_clause_ids_.insert({literals, id});
}

bool LratMerger::RemapInferredClause(int worker_index,
                                     const std::string& filename,
                                     LratInferredClause& inferred_clause) {
  const GlobalId global_id = NextGlobalId();
  ClauseId local_id = ClauseId(inferred_clause.clause_id());
  local_to_global_ids_[worker_index][local_id] = global_id;

  inferred_clause.set_clause_id(global_id.value());
  if (!RemapClauseIds(worker_index, filename,
                      inferred_clause.mutable_unit_ids())) {
    return false;
  }
  for (LratInferredClause::RatInfo& rat_info :
       *inferred_clause.mutable_rat_infos()) {
    local_id = ClauseId(rat_info.resolvant_id());
    auto it = local_to_global_ids_[worker_index].find(local_id);
    if (it == local_to_global_ids_[worker_index].end()) {
      return Error(
          absl::StrCat("unknown clause ID ", local_id, " in ", filename));
    }
    rat_info.set_resolvant_id(it->second.value());
    if (!RemapClauseIds(worker_index, filename, rat_info.mutable_unit_ids())) {
      return false;
    }
  }
  return true;
}

bool LratMerger::RemapClauseIds(
    int worker_index, const std::string& filename,
    google::protobuf::RepeatedField<int64_t>* clause_ids) {
  for (int i = 0; i < clause_ids->size(); ++i) {
    const ClauseId local_id = ClauseId(clause_ids->Get(i));
    auto it = local_to_global_ids_[worker_index].find(local_id);
    if (it == local_to_global_ids_[worker_index].end()) {
      return Error(
          absl::StrCat("unknown clause ID ", local_id, " in ", filename));
    }
    clause_ids->Set(i, it->second.value());
  }
  return true;
}

bool LratMerger::WriteInferredClause(
    const LratInferredClause& inferred_clause) {
  if (lrat_checker_ != nullptr) {
    // TODO(user): can we optimize away this format conversion?
    IndicesToLiterals(inferred_clause.literals(), &tmp_clause_);
    tmp_unit_ids_.clear();
    tmp_unit_ids_.reserve(inferred_clause.unit_ids_size());
    for (const int64_t id : inferred_clause.unit_ids()) {
      tmp_unit_ids_.push_back(ClauseId(id));
    }
    tmp_rat_ids_.clear();
    tmp_rat_ids_.reserve(inferred_clause.rat_infos_size());
    for (const LratInferredClause::RatInfo& rat_info :
         inferred_clause.rat_infos()) {
      tmp_rat_ids_.push_back(
          {ClauseId(rat_info.resolvant_id()),
           std::vector<ClauseId>(rat_info.unit_ids().begin(),
                                 rat_info.unit_ids().end())});
    }
    if (!lrat_checker_->AddInferredClause(ClauseId(inferred_clause.clause_id()),
                                          tmp_clause_, tmp_unit_ids_,
                                          tmp_rat_ids_)) {
      return LratError();
    }
  }
  merged_proof_file_ << inferred_clause.clause_id();
  for (const int lit : inferred_clause.literals()) {
    merged_proof_file_ << " " << Literal(LiteralIndex(lit)).SignedValue();
  }
  merged_proof_file_ << " 0";
  for (const int unit_id : inferred_clause.unit_ids()) {
    merged_proof_file_ << " " << unit_id;
  }
  for (const LratInferredClause::RatInfo& rat_info :
       inferred_clause.rat_infos()) {
    merged_proof_file_ << " " << -rat_info.resolvant_id();
    for (const int unit_id : rat_info.unit_ids()) {
      merged_proof_file_ << " " << unit_id;
    }
  }
  merged_proof_file_ << " 0\n";
  return true;
}

bool LratMerger::Error(std::string_view message) const {
  if (debug_crash_on_error_) {
    LOG(FATAL) << "LRAT merge error: " << message;
  } else {
    LOG(ERROR) << "LRAT merge error: " << message;
  }
  return false;
}

bool LratMerger::LratError() const {
  if (debug_crash_on_error_) {
    LOG(FATAL) << "LRAT error: " << lrat_checker_->error_message();
  }
  return false;
}

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
  if (!params.check_lrat_proof() && !params.output_lrat_proof() &&
      !params.check_drat_proof() && !params.output_drat_proof()) {
    return nullptr;
  }
  return std::unique_ptr<LratProofHandler>(new LratProofHandler(model));
}

LratProofHandler::LratProofHandler(Model* model)
    : id_(model->GetOrCreate<SharedLratProofStatus>()->NewSubSolverId()),
      proof_status_(model->GetOrCreate<SharedLratProofStatus>()) {
  const SatParameters& params = *model->GetOrCreate<SatParameters>();
  if (params.check_lrat_proof()) {
    lrat_checker_ = std::make_unique<LratChecker>(model);
  }
  if (params.output_lrat_proof()) {
    lrat_writer_ = std::make_unique<LratWriter>(absl::StrCat(
        absl::GetFlag(FLAGS_cp_model_lrat_output_prefix), id_, ".bin"));
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
  if (lrat_checker_ != nullptr &&
      !lrat_checker_->AddProblemClause(id, clause)) {
    return LratError();
  }
  if (drat_checker_ != nullptr) {
    drat_checker_->AddProblemClause(SortClauseForDrat(clause));
  }
  if (lrat_writer_ != nullptr) {
    lrat_writer_->AddImportedClause(id, clause);
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
  if (lrat_checker_ != nullptr &&
      !lrat_checker_->AddInferredClause(id, clause, unit_ids, rat)) {
    return LratError();
  }
  if (drat_checker_ != nullptr) {
    if (all_problem_clauses_loaded_) {
      drat_checker_->AddInferredClause(SortClauseForDrat(clause));
    } else {
      clauses_inferred_during_problem_loading_.push_back(
          SortClauseForDrat(clause));
    }
  }
  if (lrat_writer_ != nullptr) {
    lrat_writer_->AddInferredClause(id, clause, unit_ids, rat);
  }
  if (drat_writer_ != nullptr) {
    drat_writer_->AddClause(clause);
  }
  return true;
}

bool LratProofHandler::AddImportedClause(ClauseId id,
                                         absl::Span<const Literal> clause) {
  VLOG(1) << "AddImportedClause: id=" << id
          << " literals=" << absl::StrJoin(clause, ",");
  if (lrat_checker_ != nullptr &&
      !lrat_checker_->AddProblemClause(id, clause)) {
    return LratError();
  }
  if (drat_checker_ != nullptr) {
    LOG(ERROR) << "Imported clauses are not supported by the DRAT checker.";
    return false;
  }
  if (lrat_writer_ != nullptr) {
    lrat_writer_->AddImportedClause(id, clause);
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
  if (lrat_checker_ != nullptr &&
      !lrat_checker_->AddProblemClause(id, clause)) {
    return LratError();
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
  if (lrat_checker_ != nullptr) {
    lrat_checker_->DeleteClauses({id});
  }
  if (drat_checker_ != nullptr) {
    drat_checker_->DeleteClause(clause);
  }
  if (lrat_writer_ != nullptr) {
    lrat_writer_->DeleteClause(id);
  }
  if (drat_writer_ != nullptr) {
    drat_writer_->DeleteClause(clause);
  }
}

DratChecker::Status LratProofHandler::Valid() const {
  if (lrat_checker_ != nullptr) {
    if (lrat_checker_->Valid()) {
      return DratChecker::Status::VALID;
    }
    return DratChecker::Status::INVALID;
  }
  return DratChecker::Status::UNKNOWN;
}

DratChecker::Status LratProofHandler::Check() {
  DratChecker::Status status = DratChecker::Status::UNKNOWN;
  if (lrat_checker_ != nullptr) {
    status = lrat_checker_->Check() ? DratChecker::Status::VALID
                                    : DratChecker::Status::INVALID;
    if (status == DratChecker::Status::INVALID && debug_crash_on_error_) {
      LOG(FATAL) << "LRAT error: " << lrat_checker_->error_message();
    }
  }
  if (status != DratChecker::Status::INVALID && drat_checker_ != nullptr) {
    drat_checker_->Check(max_drat_time_in_seconds_);
    if (status == DratChecker::Status::INVALID && debug_crash_on_error_) {
      LOG(FATAL) << "DRAT check failed";
    }
  }
  return status;
}

bool LratProofHandler::LratError() const {
  if (debug_crash_on_error_) {
    LOG(FATAL) << "LRAT error: " << lrat_checker_->error_message();
  }
  return false;
}

void LratProofHandler::Close(bool model_is_unsat) {
  WallTimer timer;
  timer.Start();
  const bool valid = model_is_unsat ? Check() : Valid();
  proof_status_->NewSubsolverProofStatus(
      valid ? DratChecker::Status::VALID : DratChecker::Status::INVALID,
      lrat_check_enabled(), drat_check_enabled(), num_assumed_clauses(),
      timer.Get());
  if (lrat_checker_ != nullptr) {
    lrat_checker_->AddStats();
  }
  if (lrat_writer_ != nullptr) {
    proof_status_->NewProofFile(lrat_writer_->filename());
  }
}

}  // namespace sat
}  // namespace operations_research
