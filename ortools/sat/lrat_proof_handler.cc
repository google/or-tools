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
#include <bitset>
#include <cstdint>
#include <fstream>
#include <ios>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/sat/lrat.pb.h"
#include "ortools/sat/lrat_checker.h"
#include "ortools/sat/model.h"
#include "ortools/sat/recordio.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/synchronization.h"

#if defined(_MSC_VER)
ABSL_FLAG(std::string, cp_model_lrat_output_prefix, ".\\lrat",
          "File name prefix for the generated LRAT proof files, if LRAT output "
          "is enabled. One file is created for each worker.");
#else
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

LratWriter::~LratWriter() {
  WriteDeletedClauses();
  writer_.Close();
}

void LratWriter::AddImportedClause(ClausePtr clause,
                                   int64_t one_based_cnf_index) {
  WriteDeletedClauses();
  LratProofStep step;
  LratImportedClause* imported_clause = step.mutable_imported_clause();
  imported_clause->set_clause_id(clause.SerializePtr());
  for (const Literal literal : clause.GetLiterals()) {
    imported_clause->add_literals(literal.Index().value());
  }
  if (one_based_cnf_index > 0) {
    imported_clause->set_one_based_cnf_index(one_based_cnf_index);
  }
  CHECK(writer_.WriteRecord(step));
}

void LratWriter::AddInferredClause(
    ClausePtr clause, absl::Span<const ClausePtr> proof,
    absl::Span<const LratChecker::RatClauses> rat_proof, bool exported) {
  WriteDeletedClauses();
  LratProofStep step;
  LratInferredClause* inferred_clause = step.mutable_inferred_clause();
  inferred_clause->set_clause_id(clause.SerializePtr());
  for (const Literal literal : clause.GetLiterals()) {
    inferred_clause->add_literals(literal.Index().value());
  }
  for (const ClausePtr clause : proof) {
    inferred_clause->add_rup_clause_ids(clause.SerializePtr());
  }
  for (const LratChecker::RatClauses& rat_clauses : rat_proof) {
    LratInferredClause::RatInfo* rat_info = inferred_clause->add_rat_infos();
    rat_info->set_resolvant_id(rat_clauses.resolvant.SerializePtr());
    for (const ClausePtr clause : rat_clauses.rup_clauses) {
      rat_info->add_rup_clause_ids(clause.SerializePtr());
    }
  }
  if (exported) inferred_clause->set_exported(true);
  CHECK(writer_.WriteRecord(step));
}

void LratWriter::RewriteClause(
    ClausePtr clause, absl::Span<const Literal> literals,
    absl::Span<const ClausePtr> proof,
    absl::Span<const LratChecker::RatClauses> rat_proof, bool exported) {
  WriteDeletedClauses();
  LratProofStep step;
  LratInferredClause* inferred_clause = step.mutable_inferred_clause();
  inferred_clause->set_clause_id(clause.SerializePtr());
  for (const Literal literal : literals) {
    inferred_clause->add_literals(literal.Index().value());
  }
  for (const ClausePtr clause : proof) {
    inferred_clause->add_rup_clause_ids(clause.SerializePtr());
  }
  for (const LratChecker::RatClauses& rat_clauses : rat_proof) {
    LratInferredClause::RatInfo* rat_info = inferred_clause->add_rat_infos();
    rat_info->set_resolvant_id(rat_clauses.resolvant.SerializePtr());
    for (const ClausePtr clause : rat_clauses.rup_clauses) {
      rat_info->add_rup_clause_ids(clause.SerializePtr());
    }
  }
  if (exported) inferred_clause->set_exported(true);
  CHECK(writer_.WriteRecord(step));
}

void LratWriter::ExportClause(ClausePtr clause) {
  WriteDeletedClauses();
  LratProofStep step;
  LratExportedClause* exported_clause = step.mutable_exported_clause();
  exported_clause->set_clause_id(clause.SerializePtr());
  for (const Literal literal : clause.GetLiterals()) {
    exported_clause->add_literals(literal.Index().value());
  }
  CHECK(writer_.WriteRecord(step));
}

void LratWriter::DeleteClause(ClausePtr clause) {
  deleted_clauses_.push_back(clause);
}

void LratWriter::WriteDeletedClauses() {
  if (deleted_clauses_.empty()) return;
  LratProofStep step;
  for (const ClausePtr clause : deleted_clauses_) {
    step.mutable_deleted_clauses()->add_clause_ids(clause.SerializePtr());
  }
  CHECK(writer_.WriteRecord(step));
  deleted_clauses_.clear();
}

namespace {
ClausePtr NewClausePtr(absl::Span<const Literal> literals) {
  switch (literals.size()) {
    case 0:
      return ClausePtr::EmptyClausePtr();
    case 1:
      return ClausePtr(literals[0]);
    case 2:
      return ClausePtr(literals[0], literals[1]);
    default:
      return ClausePtr(literals);
  }
}

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
  SharedLratProofStatus::Status status = SharedLratProofStatus::Status::UNKNOWN;
  if (lrat_checker_ != nullptr) {
    status = lrat_checker_->Check() ? SharedLratProofStatus::Status::VALID
                                    : SharedLratProofStatus::Status::INVALID;
    if (status == SharedLratProofStatus::Status::INVALID &&
        debug_crash_on_error_) {
      LOG(FATAL) << "LRAT error: " << lrat_checker_->error_message();
    }
    lrat_checker_->AddStats();
  }
  for (const auto& [global_id, clause] : global_id_to_clause_) {
    if (clause.IsSatClausePtr()) {
      delete clause.GetSatClause();
    }
  }
  proof_status_->NewSubsolverProofStatus(status, lrat_checker_ != nullptr,
                                         /*num_assumed_clauses=*/0);
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

  local_to_global_ids_.resize(proof_filenames.size());
  if (!ReadPresolveProof(proof_filenames[0])) return false;

  const int num_workers = proof_filenames.size() - 1;
  std::vector<std::ifstream> inputs(num_workers);
  std::vector<std::unique_ptr<RecordReader>> readers(num_workers);
  last_read_steps_.resize(num_workers);
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
      const int proof_index = i + 1;
      const std::string& filename = proof_filenames[proof_index];
      // An empty step means that the reader is at the end of the file.
      bool missing_import = false;
      while (last_read_steps_[i].step_case() != LratProofStep::STEP_NOT_SET &&
             !missing_import) {
        LratProofStep& step = last_read_steps_[i];
        switch (step.step_case()) {
          case LratProofStep::kImportedClause: {
            const int64_t local_id = step.imported_clause().clause_id();
            IndicesToLiterals(step.imported_clause().literals(), &clause);
            std::sort(clause.begin(), clause.end());
            auto it = shared_global_id_.find(clause);
            if (it != shared_global_id_.end()) {
              local_to_global_ids_[proof_index][local_id] = it->second;
            } else {
              missing_import = true;
            }
            break;
          }
          case LratProofStep::kInferredClause: {
            const int64_t local_id = step.inferred_clause().clause_id();
            const auto it = local_to_global_ids_[proof_index].find(local_id);
            const GlobalId old_global_id =
                it == local_to_global_ids_[proof_index].end() ? GlobalId(-1)
                                                              : it->second;
            if (!RemapInferredClause(proof_index, filename,
                                     *step.mutable_inferred_clause(),
                                     NextGlobalId())) {
              return false;
            }
            if (!WriteInferredClause(step.inferred_clause())) return false;
            if (old_global_id != GlobalId(-1)) {
              // Case of a clause rewritten without changing its local ID.
              // We can delete the old one via its old global ID.
              if (shared_global_ids_.contains(old_global_id)) {
                // TODO(user): implement this case. We should delete the
                // clause from `shared_global_ids_`, but only after we are sure
                // that no other worker will ever import it.
              } else {
                WriteDeletedClauses({old_global_id});
              }
            }
            // We found the empty clause, we don't need anymore steps.
            if (step.inferred_clause().literals().empty()) return true;
            if (step.inferred_clause().exported() ||
                step.inferred_clause().literals_size() <= 2) {
              IndicesToLiterals(step.inferred_clause().literals(), &clause);
              SortAndAddSharedClause(
                  GlobalId(step.inferred_clause().clause_id()), clause);
            }
            break;
          }
          case LratProofStep::kExportedClause: {
            const int64_t local_id = step.exported_clause().clause_id();
            auto it = local_to_global_ids_[proof_index].find(local_id);
            if (it == local_to_global_ids_[proof_index].end()) {
              return Error(absl::StrCat("unknown exported clause ID ", local_id,
                                        " in ", filename));
            }
            const GlobalId global_id = it->second;
            IndicesToLiterals(step.exported_clause().literals(), &clause);
            SortAndAddSharedClause(global_id, clause);
            break;
          }
          case LratProofStep::kDeletedClauses: {
            std::vector<GlobalId> global_ids_to_delete;
            for (const int64_t local_id : step.deleted_clauses().clause_ids()) {
              auto it = local_to_global_ids_[proof_index].find(local_id);
              if (it != local_to_global_ids_[proof_index].end()) {
                const GlobalId global_id = it->second;
                if (shared_global_ids_.contains(global_id)) {
                  // TODO(user): implement this case. We should delete the
                  // clause from `shared_global_ids_`, but only after we are
                  // sure that no other worker will ever import it.
                } else {
                  local_to_global_ids_[proof_index].erase(it);
                  global_ids_to_delete.push_back(global_id);
                }
              }
            }
            WriteDeletedClauses(global_ids_to_delete);
            break;
          }
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
  std::vector<Literal> literals;
  absl::flat_hash_map<GlobalId, std::vector<Literal>> shared_clauses;
  last_written_global_id_ = GlobalId(proof_status_->MaxOneBasedCnfIndex());
  next_global_id_ = last_written_global_id_ + GlobalId(1);
  while (reader.ReadRecord(&step)) {
    switch (step.step_case()) {
      case LratProofStep::kImportedClause: {
        const int64_t local_id = step.imported_clause().clause_id();
        const GlobalId global_id =
            GlobalId(step.imported_clause().one_based_cnf_index());
        local_to_global_ids_[0][local_id] = global_id;
        IndicesToLiterals(step.imported_clause().literals(), &literals);
        std::sort(literals.begin(), literals.end());
        shared_clauses[global_id] = literals;
        if (lrat_checker_ != nullptr) {
          const ClausePtr clause = NewClausePtr(literals);
          DCHECK(!global_id_to_clause_.contains(global_id));
          global_id_to_clause_[global_id] = clause;
          if (!lrat_checker_->AddProblemClause(clause)) {
            return LratError();
          }
        }
        break;
      }
      case LratProofStep::kInferredClause: {
        const int64_t local_id = step.inferred_clause().clause_id();
        GlobalId global_id = NextGlobalId();
        if (!RemapInferredClause(/*proof_index=*/0, filename,
                                 *step.mutable_inferred_clause(), global_id)) {
          return false;
        }
        local_to_global_ids_[0][local_id] = global_id;
        IndicesToLiterals(step.inferred_clause().literals(), &literals);
        std::sort(literals.begin(), literals.end());
        shared_clauses[global_id] = literals;
        if (!WriteInferredClause(step.inferred_clause())) return false;
        break;
      }
      case LratProofStep::kExportedClause: {
        // Nothing to do, since we export all clauses in the presolve proof.
        break;
      }
      case LratProofStep::kDeletedClauses: {
        std::vector<GlobalId> global_ids_to_delete;
        for (const int64_t id : step.deleted_clauses().clause_ids()) {
          auto it = local_to_global_ids_[0].find(id);
          if (it != local_to_global_ids_[0].end()) {
            const GlobalId global_id = it->second;
            shared_clauses.erase(global_id);
            global_ids_to_delete.push_back(global_id);
          }
        }
        WriteDeletedClauses(global_ids_to_delete);
        break;
      }
      default:
        return Error(absl::StrCat("unknown proof step type ", step.step_case(),
                                  " in ", filename));
    }
  }
  for (const auto& [global_id, clause] : shared_clauses) {
    shared_global_id_.insert({clause, global_id});
    shared_global_ids_.insert(global_id);
  }
  local_to_global_ids_[0].clear();
  return true;
}

void LratMerger::SortAndAddSharedClause(GlobalId id,
                                        std::vector<Literal>& literals) {
  std::sort(literals.begin(), literals.end());
  shared_global_id_.insert({literals, id});
  shared_global_ids_.insert(id);
}

bool LratMerger::RemapInferredClause(int proof_index,
                                     const std::string& filename,
                                     LratInferredClause& inferred_clause,
                                     GlobalId global_id) {
  if (!RemapClauseIds(proof_index, filename,
                      inferred_clause.mutable_rup_clause_ids())) {
    return false;
  }
  for (LratInferredClause::RatInfo& rat_info :
       *inferred_clause.mutable_rat_infos()) {
    const int64_t local_id = rat_info.resolvant_id();
    auto it = local_to_global_ids_[proof_index].find(local_id);
    if (it == local_to_global_ids_[proof_index].end()) {
      return Error(
          absl::StrCat("unknown clause ID ", local_id, " in ", filename));
    }
    rat_info.set_resolvant_id(it->second.value());
    if (!RemapClauseIds(proof_index, filename,
                        rat_info.mutable_rup_clause_ids())) {
      return false;
    }
  }

  // It is important to update local_to_global_ids_ at the end, so that the
  // above code works when a clause is rewritten without changing its ID (its
  // proof generally uses this ID too).
  const int64_t local_id = inferred_clause.clause_id();
  inferred_clause.set_clause_id(global_id.value());
  local_to_global_ids_[proof_index][local_id] = global_id;
  if (lrat_checker_ != nullptr) {
    IndicesToLiterals(inferred_clause.literals(), &tmp_literals_);
    DCHECK(!global_id_to_clause_.contains(global_id));
    global_id_to_clause_[global_id] = NewClausePtr(tmp_literals_);
  }
  return true;
}

bool LratMerger::RemapClauseIds(
    int proof_index, const std::string& filename,
    google::protobuf::RepeatedField<int64_t>* clause_ids) {
  for (int i = 0; i < clause_ids->size(); ++i) {
    const int64_t local_id = clause_ids->Get(i);
    auto it = local_to_global_ids_[proof_index].find(local_id);
    if (it == local_to_global_ids_[proof_index].end()) {
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
    const ClausePtr clause =
        global_id_to_clause_[GlobalId(inferred_clause.clause_id())];
    tmp_proof_.clear();
    tmp_proof_.reserve(inferred_clause.rup_clause_ids_size());
    for (const int64_t id : inferred_clause.rup_clause_ids()) {
      tmp_proof_.push_back(global_id_to_clause_[GlobalId(id)]);
    }
    tmp_rat_clauses_.clear();
    tmp_rat_clauses_.reserve(inferred_clause.rat_infos_size());
    for (const LratInferredClause::RatInfo& rat_info :
         inferred_clause.rat_infos()) {
      std::vector<ClausePtr> proof;
      proof.reserve(rat_info.rup_clause_ids_size());
      for (const int64_t id : rat_info.rup_clause_ids()) {
        proof.push_back(global_id_to_clause_[GlobalId(id)]);
      }
      tmp_rat_clauses_.push_back(
          {global_id_to_clause_[GlobalId(rat_info.resolvant_id())], proof});
    }
    if (!lrat_checker_->AddInferredClause(clause, tmp_proof_,
                                          tmp_rat_clauses_)) {
      return LratError();
    }
  }
  std::string& clause_str = tmp_clause_str_;
  clause_str.clear();
  absl::StrAppend(&clause_str, inferred_clause.clause_id());
  for (const int lit : inferred_clause.literals()) {
    absl::StrAppend(&clause_str, " ", Literal(LiteralIndex(lit)).SignedValue());
  }
  absl::StrAppend(&clause_str, " 0");
  for (const int rup_clause_id : inferred_clause.rup_clause_ids()) {
    absl::StrAppend(&clause_str, " ", rup_clause_id);
  }
  for (const LratInferredClause::RatInfo& rat_info :
       inferred_clause.rat_infos()) {
    absl::StrAppend(&clause_str, " ", -rat_info.resolvant_id());
    for (const int rup_clause_id : rat_info.rup_clause_ids()) {
      absl::StrAppend(&clause_str, " ", rup_clause_id);
    }
  }
  absl::StrAppend(&clause_str, " 0\n");
  merged_proof_file_ << clause_str;
  last_written_global_id_ = GlobalId(inferred_clause.clause_id());
  return true;
}

void LratMerger::WriteDeletedClauses(absl::Span<const GlobalId> global_ids) {
  if (global_ids.empty()) return;
  if (lrat_checker_ != nullptr) {
    std::vector<ClausePtr> clauses;
    clauses.reserve(global_ids.size());
    for (const GlobalId id : global_ids) {
      const auto it = global_id_to_clause_.find(id);
      clauses.push_back(it->second);
      global_id_to_clause_.erase(it);
    }
    lrat_checker_->DeleteClauses(clauses);
    for (const ClausePtr clause : clauses) {
      if (clause.IsSatClausePtr()) {
        delete clause.GetSatClause();
      }
    }
  }
  merged_proof_file_ << last_written_global_id_ << " d";
  for (const GlobalId id : global_ids) {
    merged_proof_file_ << " " << id.value();
  }
  merged_proof_file_ << " 0\n";
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

std::unique_ptr<LratProofHandler> LratProofHandler::MaybeCreate(Model* model) {
  return MaybeCreate(*model->GetOrCreate<SatParameters>(),
                     model->GetOrCreate<SharedLratProofStatus>(),
                     model->GetOrCreate<SharedStatistics>());
}

std::unique_ptr<LratProofHandler> LratProofHandler::MaybeCreate(
    const SatParameters& params, SharedLratProofStatus* proof_status,
    SharedStatistics* stats) {
  if (!params.check_lrat_proof() && !params.output_lrat_proof()) {
    return nullptr;
  }
  return std::unique_ptr<LratProofHandler>(
      new LratProofHandler(params, proof_status, stats));
}

LratProofHandler::LratProofHandler(
    const SatParameters& params,
    SharedLratProofStatus* shared_lrat_proof_status, SharedStatistics* stats)
    : id_(shared_lrat_proof_status->NewSubSolverId()),
      proof_status_(shared_lrat_proof_status) {
  if (params.check_lrat_proof()) {
    lrat_checker_ = std::make_unique<LratChecker>(stats);
  }
  if (params.output_lrat_proof()) {
    lrat_writer_ = std::make_unique<LratWriter>(absl::StrCat(
        absl::GetFlag(FLAGS_cp_model_lrat_output_prefix), id_, ".bin"));
  }
  debug_crash_on_error_ = params.debug_crash_if_lrat_check_fails();
}

bool LratProofHandler::AddProblemClause(ClausePtr clause,
                                        int64_t one_based_cnf_index) {
  VLOG(2) << "AddProblemClause: ptr=" << clause
          << " literals=" << absl::StrJoin(clause.GetLiterals(), ",");
  if (all_problem_clauses_loaded_ && debug_crash_on_error_) {
    LOG(FATAL) << "LRAT error: problem clauses must not be added after "
                  "EndProblemClauses()";
  }
  if (lrat_checker_ != nullptr && !lrat_checker_->AddProblemClause(clause)) {
    return LratError("In AddProblemClause.");
  }
  if (lrat_writer_ != nullptr) {
    lrat_writer_->AddImportedClause(clause, one_based_cnf_index);
  }
  return true;
}

void LratProofHandler::EndProblemClauses() {
  all_problem_clauses_loaded_ = true;
}

bool LratProofHandler::AddInferredClause(
    ClausePtr clause, absl::Span<const ClausePtr> proof,
    absl::Span<const LratChecker::RatClauses> rat_proof, bool exported) {
  VLOG(2) << "AddInferredClause: ptr=" << clause
          << " literals=" << absl::StrJoin(clause.GetLiterals(), ",")
          << " proof=" << absl::StrJoin(proof, ",") << " rat_proof={"
          << absl::StrJoin(rat_proof, " ") << "}";
  if (lrat_checker_ != nullptr &&
      !lrat_checker_->AddInferredClause(clause, proof, rat_proof)) {
    return LratError(
        absl::StrCat("AddInferredClause: ptr=", clause,
                     "\nliterals=", absl::StrJoin(clause.GetLiterals(), ","),
                     "\nproof=", absl::StrJoin(proof, ","), "\nrat_proof={",
                     absl::StrJoin(rat_proof, " "), "}"));
  }
  if (lrat_writer_ != nullptr) {
    lrat_writer_->AddInferredClause(clause, proof, rat_proof, exported);
  }
  return true;
}

bool LratProofHandler::RewriteClause(ClausePtr clause,
                                     absl::Span<const Literal> literals,
                                     absl::Span<const ClausePtr> proof) {
  VLOG(2) << "RewriteClause: ptr=" << clause
          << " literals=" << absl::StrJoin(literals, ",")
          << " unit_ids=" << absl::StrJoin(proof, ",");
  if (lrat_checker_ != nullptr &&
      !lrat_checker_->RewriteClause(clause, literals, proof)) {
    return LratError(absl::StrCat("RewriteClause: ptr=", clause,
                                  "\nliterals=", absl::StrJoin(literals, ","),
                                  "\nproof=", absl::StrJoin(proof, ",")));
  }
  if (lrat_writer_ != nullptr) {
    lrat_writer_->RewriteClause(clause, literals, proof, /*rat_proof=*/{});
  }
  return true;
}

bool LratProofHandler::AddImportedClause(ClausePtr clause) {
  VLOG(2) << "AddImportedClause: ptr=" << clause
          << " literals=" << absl::StrJoin(clause.GetLiterals(), ",");
  if (lrat_checker_ != nullptr && !lrat_checker_->AddProblemClause(clause)) {
    return LratError("In AddImportedClause");
  }
  if (lrat_writer_ != nullptr) {
    lrat_writer_->AddImportedClause(clause, /*one_based_cnf_index=*/0);
  }
  return true;
}

bool LratProofHandler::AddAssumedClause(ClausePtr clause) {
  VLOG(2) << "AddAssumedClause: ptr=" << clause
          << " literals=" << absl::StrJoin(clause.GetLiterals(), ",");
  if (debug_crash_on_error_) {
    LOG(FATAL) << "LRAT error: assumed clauses are not supposed to happen";
  }
  ++num_assumed_clauses_;
  if (lrat_checker_ != nullptr && !lrat_checker_->AddProblemClause(clause)) {
    return LratError("In AddAssumedClause");
  }
  return true;
}

bool LratProofHandler::ExportClause(ClausePtr clause) {
  VLOG(2) << "ExportClause: ptr=" << clause
          << " literals=" << absl::StrJoin(clause.GetLiterals(), ",");
  if (lrat_writer_ != nullptr) {
    lrat_writer_->ExportClause(clause);
  }
  return true;
}

void LratProofHandler::DeleteClause(ClausePtr clause, bool delete_sat_clause) {
  VLOG(2) << "DeleteClause: ptr=" << clause
          << " literals=" << absl::StrJoin(clause.GetLiterals(), ",");
  if (lrat_checker_ != nullptr) {
    lrat_checker_->DeleteClauses({clause});
  }
  if (lrat_writer_ != nullptr) {
    lrat_writer_->DeleteClause(clause);
  }
  if (delete_sat_clause && clause.IsSatClausePtr()) {
    delete clause.GetSatClause();
  }
}

SharedLratProofStatus::Status LratProofHandler::Valid() const {
  if (lrat_checker_ != nullptr) {
    if (lrat_checker_->Valid()) {
      return SharedLratProofStatus::Status::VALID;
    }
    return SharedLratProofStatus::Status::INVALID;
  }
  return SharedLratProofStatus::Status::UNKNOWN;
}

SharedLratProofStatus::Status LratProofHandler::Check() {
  if (lrat_checker_ != nullptr) {
    if (lrat_checker_->Check()) {
      return SharedLratProofStatus::Status::VALID;
    }
    if (debug_crash_on_error_) {
      LOG(FATAL) << "LRAT error: " << lrat_checker_->error_message();
    }
    return SharedLratProofStatus::Status::INVALID;
  }
  return SharedLratProofStatus::Status::UNKNOWN;
}

bool LratProofHandler::LratError(absl::string_view message) const {
  if (debug_crash_on_error_) {
    LOG(FATAL) << "LRAT error: " << message
               << "\nChecker_error:" << lrat_checker_->error_message();
  }
  return false;
}

void LratProofHandler::Close(bool model_is_unsat) {
  const bool valid = model_is_unsat ? Check() : Valid();
  proof_status_->NewSubsolverProofStatus(
      valid ? SharedLratProofStatus::Status::VALID
            : SharedLratProofStatus::Status::INVALID,
      lrat_check_enabled(), num_assumed_clauses());
  if (lrat_checker_ != nullptr) {
    lrat_checker_->AddStats();
  }
  if (lrat_writer_ != nullptr) {
    proof_status_->NewProofFile(lrat_writer_->filename());
  }
}

bool LratProofHandler::AddAndProveInferredClauseByEnumeration(
    ClausePtr new_clause, absl::Span<const ClausePtr> clauses_for_proof) {
  CHECK(!clauses_for_proof.empty());

  // helper function to report some info on proof failure.
  const auto error = [&, this](absl::string_view message) {
    if (debug_crash_on_error_) {
      LOG(INFO) << "Proving " << new_clause.GetLiterals();
      for (int i = 0; i < clauses_for_proof.size(); ++i) {
        LOG(INFO) << "input= " << clauses_for_proof[i].GetLiterals();
      }
      LOG(FATAL) << message;
    } else {
      VLOG(2) << "Proving " << new_clause.GetLiterals();
      for (int i = 0; i < clauses_for_proof.size(); ++i) {
        VLOG(2) << "input = " << clauses_for_proof[i].GetLiterals();
      }
      VLOG(2) << message;
    }
    return false;
  };

  // First we count the number of variables appearing and have a separate dense
  // index for them. The first new_clause.size() dense index are exactly the
  // literal of the new_clause.
  absl::flat_hash_map<BooleanVariable, int> to_dense_index;
  absl::Span<const Literal> new_clause_literals = new_clause.GetLiterals();
  for (const Literal lit : new_clause_literals) {
    const auto [it, inserted] =
        to_dense_index.insert({lit.Variable(), to_dense_index.size()});
    if (!inserted) {
      return error("Duplicate variable in new clause");
    }
  }

  // Then any new BooleanVariable appearing get the next dense index.
  std::vector<Literal> relevant_literals;
  for (int i = 0; i < clauses_for_proof.size(); ++i) {
    for (const Literal lit : clauses_for_proof[i].GetLiterals()) {
      const auto [it, inserted] =
          to_dense_index.insert({lit.Variable(), to_dense_index.size()});
      if (inserted) {
        relevant_literals.push_back(lit);
      }
    }
  }

  // Too many variables.
  //
  // TODO(user): The limit can be increased a bit if needed.
  if (to_dense_index.size() > 8) {
    return error(absl::StrCat("Too many variables: ", to_dense_index.size()));
  }

  // For the proof we will need all clauses of the form
  //    {new_clause, l0, ..., lk} for all k in [0, n) and
  //    li = relevant_literals[i] OR relevant_literals[i].Negated().
  //
  // That give us 2^(n + 1) intermediate clauses.
  // Their pointers will be stored in (1 << k) + binary_encoding_of_the_li.
  const int n = to_dense_index.size() - new_clause_literals.size();
  CHECK_EQ(n, relevant_literals.size());
  const int num_intermediates = 1 << (n + 1);
  std::vector<ClausePtr> intermediate_clauses(num_intermediates,
                                              kNullClausePtr);

  VLOG(2) << "Starting proof n= " << n << " " << num_intermediates;

  // Any initial clause can be used to prove all the intermediates that contains
  // it. Note that this code supports duplicate literals in the clauses.
  for (int i = 0; i < clauses_for_proof.size(); ++i) {
    bool skip = false;
    int base_index = 0;
    int mask = 0;
    int k = 0;
    absl::Span<const Literal> clause_for_proof =
        clauses_for_proof[i].GetLiterals();
    for (const Literal lit : clause_for_proof) {
      const int dense_index = to_dense_index[lit.Variable()];
      if (dense_index < new_clause_literals.size()) {
        // Check that the literal is the same as in the new_clause, if
        // not, this clause will not be needed for the proof.
        if (lit != new_clause_literals[dense_index]) {
          skip = true;
          break;
        }
      } else {
        k = std::max(k, dense_index);
        mask |= 1 << dense_index;
        if (lit ==
            relevant_literals[dense_index - new_clause_literals.size()]) {
          base_index |= 1 << dense_index;
        }
      }
    }
    if (skip) continue;
    if (k == 0) {
      // The clause is the same as the one we try to prove! or smaller.
      if (clause_for_proof.size() == new_clause_literals.size() &&
          clauses_for_proof[i] == new_clause) {
        return true;
      } else {
        // TODO(user): Likely we could have simplified what we are trying to
        // prove. Like I saw this happen when we prove an equivalence but we
        // can actually prove that the variables are fixed.
        if (!AddInferredClause(new_clause, {clauses_for_proof[i]})) {
          return error("failed trivial inclusion proof");
        }
        return true;
      }
    }

    mask >>= new_clause_literals.size();
    base_index >>= new_clause_literals.size();
    k = k + 1 - new_clause_literals.size();

    VLOG(2) << k << " " << std::bitset<8>(mask) << " "
            << std::bitset<8>(base_index);

    // TODO(user): we could be faster here if it become needed.
    for (int m = 0; m < (1 << n); ++m) {
      if ((m & mask) != base_index) continue;  // not included.
      const int index = m | base_index;
      for (int j = k; j <= n; ++j) {
        if (index >> j == 0) {
          VLOG(2) << "Included in " << j << " "
                  << std::bitset<8>((1 << j) | index);
          intermediate_clauses[(1 << j) | index] = clauses_for_proof[i];
        }
      }
    }
  }

  // We can prove the others by decreasing k.
  std::vector<Literal> tmp_clause;
  tmp_clause.assign(new_clause_literals.begin(), new_clause_literals.end());
  std::vector<bool> id_need_deletion(num_intermediates, false);
  for (int k = n; --k >= 0;) {
    for (int m = 0; m < (1 << k); ++m) {
      const int index = (1 << k) | m;
      if (intermediate_clauses[index] != kNullClausePtr) {
        continue;  // Already proved.
      }

      // Generate the tmp_clause.
      tmp_clause.resize(new_clause_literals.size());
      for (int i = 0; i < k; ++i) {
        tmp_clause.push_back(relevant_literals[i]);
        if (((index >> i) & 1) == 0) {
          tmp_clause.back() = tmp_clause.back().Negated();
        }
      }

      // Prove it from the two clauses at k + 1.
      const int higher1 = index ^ ((0b11) << k);
      const int higher2 = index ^ ((0b10) << k);
      const ClausePtr clause1 = intermediate_clauses[higher1];
      const ClausePtr clause2 = intermediate_clauses[higher2];
      if (clause1 == kNullClausePtr || clause2 == kNullClausePtr) {
        return error(
            absl::StrCat("missing higher level clauses in the resolution.",
                         " index: ", std::bitset<8>(index).to_string(),
                         " higher1: ", std::bitset<8>(higher1).to_string(),
                         " higher2: ", std::bitset<8>(higher2).to_string()));
      }

      intermediate_clauses[index] = k == 0 ? new_clause : ClausePtr(tmp_clause);
      if (k != 0) {
        VLOG(2) << "temporary !! " << intermediate_clauses[index] << " "
                << tmp_clause;
        id_need_deletion[index] = true;  // temporary.
      }
      if (!AddInferredClause(intermediate_clauses[index], {clause1, clause2})) {
        return error("Failed resolution step");
      }

      if (k == 0) {
        DCHECK_EQ(new_clause_literals, tmp_clause);
        VLOG(2) << "Proven " << new_clause << "!";
      }

      // Lets delete the intermediate_clauses if they were temporary.
      if (id_need_deletion[higher1]) {
        VLOG(2) << "deleting: " << clause1 << " " << clause1.GetLiterals();
        DeleteClause(clause1);
      }
      if (id_need_deletion[higher2]) {
        VLOG(2) << "deleting: " << clause2 << " " << clause2.GetLiterals();
        DeleteClause(clause2);
      }
    }
  }

  return true;
}

}  // namespace sat
}  // namespace operations_research
