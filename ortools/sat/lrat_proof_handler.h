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
#include <fstream>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/types/span.h"
#include "ortools/base/strong_int.h"
#include "ortools/sat/drat_checker.h"
#include "ortools/sat/drat_writer.h"
#include "ortools/sat/lrat.pb.h"
#include "ortools/sat/lrat_checker.h"
#include "ortools/sat/model.h"
#include "ortools/sat/recordio.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"

namespace operations_research {
namespace sat {

// Writes an LRAT proof to a file in "record io" format.
class LratWriter {
 public:
  explicit LratWriter(std::string_view filename);
  ~LratWriter();

  std::string_view filename() const { return filename_; }

  void AddImportedClause(ClauseId id, absl::Span<const Literal> clause);

  void AddInferredClause(ClauseId id, absl::Span<const Literal> clause,
                         absl::Span<const ClauseId> unit_ids,
                         absl::Span<const LratChecker::RatIds> rat = {});

  void DeleteClause(ClauseId id);

 private:
  std::string filename_;
  std::ofstream ofstream_;
  RecordWriter writer_;
};

// Merges separate LRAT proofs into a single LRAT file in ASCII format.
class LratMerger {
 public:
  explicit LratMerger(Model* model);
  ~LratMerger();

  // Merges the given LRAT proofs in a single one, and writes it to a file in
  // ASCII format. The first proof must be the presolve proof. Its imported
  // clauses must be the input problem clauses, and their ID must be the 1-based
  // clause index in the input CNF file. Returns true on success, false
  // otherwise.
  bool Merge(absl::Span<const std::string> proof_filenames);

 private:
  // Clause IDs used in the merged proof. Local clause IDs in individual proof
  // files are remapped to global clause IDs (except for the presolve proof: its
  // IDs are kept unchanged). This mapping is stored in `local_to_global_ids_`
  // (one map per proof file, except for the presolve proof).
  DEFINE_STRONG_INT_TYPE(GlobalId, int64_t);

  // Reads the proof of the presolved model and adds its clauses to
  // `shared_clause_ids_`. Also checks this proof if lrat_checker_ is not null.
  // Returns true on success, false otherwise.
  bool ReadPresolveProof(const std::string& filename);

  // Canonicalizes (i.e., sorts) and registers a clause so that it can be
  // imported from an individual proof file.
  // TODO(user): is the canonicalization really needed?
  void SortAndAddSharedClause(GlobalId id, std::vector<Literal>& literals);

  // Remaps the local clause IDs in the given inferred clause to global IDs, in
  // place. Returns true on success, false otherwise.
  bool RemapInferredClause(int worker_index, const std::string& filename,
                           LratInferredClause& inferred_clause);
  bool RemapClauseIds(int worker_index, const std::string& filename,
                      google::protobuf::RepeatedField<int64_t>* clause_ids);

  // Writes the given clause to the merged proof file, in LRAT ASCII file
  // format. Also checks it if lrat_checker_ is not null. Returns true on
  // success, false otherwise.
  bool WriteInferredClause(const LratInferredClause& inferred_clause);

  GlobalId NextGlobalId() { return GlobalId(next_global_id_++); }

  bool Error(std::string_view message) const;
  bool LratError() const;

  const int id_;
  SharedLratProofStatus* proof_status_;
  std::unique_ptr<LratChecker> lrat_checker_;
  bool debug_crash_on_error_;

  std::string merged_proof_filename_;
  std::ofstream merged_proof_file_;
  GlobalId next_global_id_;

  absl::flat_hash_map<std::vector<Literal>, GlobalId> shared_clause_ids_;
  std::vector<absl::flat_hash_map<ClauseId, GlobalId>> local_to_global_ids_;
  std::vector<LratProofStep> last_read_steps_;

  std::vector<Literal> tmp_clause_;
  std::vector<ClauseId> tmp_unit_ids_;
  std::vector<LratChecker::RatIds> tmp_rat_ids_;
};

// Handles the LRAT proof of a SAT problem by either checking it incrementally
// and/or by saving it to a file.
class LratProofHandler {
 public:
  static std::unique_ptr<LratProofHandler> MaybeCreate(Model* model);

  bool lrat_check_enabled() const { return lrat_checker_ != nullptr; }
  bool lrat_output_enabled() const { return lrat_writer_ != nullptr; }
  bool drat_check_enabled() const { return drat_checker_ != nullptr; }
  bool drat_output_enabled() const { return drat_writer_ != nullptr; }

  int64_t num_assumed_clauses() const { return num_assumed_clauses_; }

  // Adds a clause of the problem. See LratChecker for more details.
  bool AddProblemClause(ClauseId id, absl::Span<const Literal> clause);

  // No more problem clauses must be added after this call.
  void EndProblemClauses();

  // Adds a clause which is inferred from the problem clauses and/or the
  // previously inferred clauses. See LratChecker for more details.
  bool AddInferredClause(ClauseId id, absl::Span<const Literal> clause,
                         absl::Span<const ClauseId> unit_ids,
                         absl::Span<const LratChecker::RatIds> rat = {});

  // This assumes that the 'new_clause' to prove and all the ones needed for the
  // proof only touch a small number of variables (<= 6). It will then prove the
  // new clause by enumerating all possibilities and producing the relevant
  // intermediate LRAT RUP steps.
  //
  // The last two arguments must have the same size and are in one to one
  // correspondence. Note that we might not need all the given clauses in the
  // proof.
  //
  // Return the new clause id. Note that in some corner cases, this could be
  // one of the id passed in ids_for_proof. Return kNoClauseId if the proof
  // is wrong.
  ClauseId AddAndProveInferredClauseByEnumeration(
      absl::Span<const Literal> new_clause,
      absl::Span<const ClauseId> ids_for_proof,
      const CompactVectorVector<int, Literal>& clauses_for_proof);

  // Adds a clause which was inferred by another worker. Returns true if
  // successful (the operation can fail if LRAT checks are enabled, and the ID
  // is already used by another clause).
  bool AddImportedClause(ClauseId id, absl::Span<const Literal> clause);

  // Adds a clause which is assumed to be true, without proof. Returns true if
  // successful (the operation fails if DRAT checks are enabled, or if LRAT
  // checks are enabled and the ID is already used by another clause).
  bool AddAssumedClause(ClauseId id, absl::Span<const Literal> clause);

  // Prevents the given clause from being deleted, until UnpinClause() is called
  // with the same ID. At most one clause can be pinned at any time.
  void PinClause(ClauseId id, absl::Span<const Literal> clause);

  // Unpins the clause with the given ID, and deletes it if a call to
  // DeleteClause() for this clause was made since it was pinned.
  void UnpinClause(ClauseId id);

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
  DratChecker::Status Check();

  void Close(bool model_is_unsat);

 private:
  explicit LratProofHandler(Model* model);

  bool LratError() const;

  const int id_;
  ClauseIdGenerator* id_generator_;
  SharedLratProofStatus* proof_status_;
  std::unique_ptr<LratChecker> lrat_checker_;
  std::unique_ptr<LratWriter> lrat_writer_;
  std::unique_ptr<DratChecker> drat_checker_;
  std::unique_ptr<DratWriter> drat_writer_;
  double max_drat_time_in_seconds_ = std::numeric_limits<double>::infinity();
  bool debug_crash_on_error_ = false;

  bool all_problem_clauses_loaded_ = false;
  int64_t num_assumed_clauses_ = 0;

  ClauseId pinned_clause_id_ = kNoClauseId;
  std::vector<Literal> pinned_clause_;
  bool delete_pinned_clause_ = false;

  // Only used when checking DRAT, because the DRAT checker does not support
  // interleaving problem and inferred clauses.
  std::vector<std::vector<Literal>> clauses_inferred_during_problem_loading_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_LRAT_PROOF_HANDLER_H_
