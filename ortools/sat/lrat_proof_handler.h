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
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/sat/lrat.pb.h"
#include "ortools/sat/lrat_checker.h"
#include "ortools/sat/model.h"
#include "ortools/sat/recordio.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/synchronization.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

// Writes an LRAT proof to a file in "record io" format.
class LratWriter {
 public:
  explicit LratWriter(std::string_view filename);
  ~LratWriter();

  std::string_view filename() const { return filename_; }

  void AddImportedClause(ClausePtr clause, int64_t one_based_cnf_index);

  void AddInferredClause(ClausePtr clause, absl::Span<const ClausePtr> proof,
                         absl::Span<const LratChecker::RatClauses> rat_proof,
                         bool exported = false);

  void RewriteClause(ClausePtr clause, absl::Span<const Literal> literals,
                     absl::Span<const ClausePtr> proof,
                     absl::Span<const LratChecker::RatClauses> rat_proof,
                     bool exported = false);

  void ExportClause(ClausePtr clause);

  void DeleteClause(ClausePtr clause);

  void RemapBooleanVariables(absl::Span<const int32_t> new_to_old_var);

 private:
  void WriteDeletedClauses();

  std::string filename_;
  std::ofstream ofstream_;
  RecordWriter writer_;
  std::vector<ClausePtr> deleted_clauses_;
};

// Merges separate LRAT proofs into a single LRAT file in ASCII format.
class LratMerger {
 public:
  explicit LratMerger(Model* model);
  ~LratMerger();

  // Merges the given LRAT proofs in a single one, and writes it to a file in
  // ASCII format. The first proof must be the presolve proof. Its imported
  // clauses must be the input problem clauses. Returns true on success, false
  // otherwise.
  bool Merge(absl::Span<const std::string> proof_filenames);

 private:
  // Clause IDs used in the merged proof. Local clause IDs in individual proof
  // files are remapped to global clause IDs (except for the presolve proof: its
  // IDs are kept unchanged). This mapping is stored in `local_to_global_ids_`
  // (one map per proof file).
  DEFINE_STRONG_INT64_TYPE(GlobalId);

  // Reads the proof of the presolved model and adds its clauses to
  // `shared_global_id_`. Also checks this proof if lrat_checker_ is not null.
  // Returns true on success, false otherwise.
  bool ReadPresolveProof(const std::string& filename);

  // Canonicalizes (i.e., sorts) and registers a clause so that it can be
  // imported from an individual proof file.
  void SortAndAddSharedClause(GlobalId id, std::vector<Literal>& literals);

  // Remaps the given literal indices to global indices, and stores the result
  // in `literals`. Also updates `literal_indices` if it is not const.
  template <typename T>
  void RemapLiteralIndices(T& literal_indices, std::vector<Literal>* literals);

  // Remaps the local clause IDs in the given inferred clause to global IDs, in
  // place. Returns true on success, false otherwise. Literals indices must be
  // remapped before calling this function.
  bool RemapInferredClause(int proof_index, const std::string& filename,
                           LratInferredClause& inferred_clause,
                           GlobalId global_id);
  bool RemapClauseIds(int proof_index, const std::string& filename,
                      google::protobuf::RepeatedField<int64_t>* clause_ids);

  // Writes the given clause to the merged proof file, in LRAT ASCII file
  // format. Also checks it if lrat_checker_ is not null. Returns true on
  // success, false otherwise.
  bool WriteInferredClause(const LratInferredClause& inferred_clause);
  void WriteDeletedClauses(absl::Span<const GlobalId> global_ids);

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
  GlobalId last_written_global_id_;

  std::vector<absl::flat_hash_map<int64_t, GlobalId>> local_to_global_ids_;
  absl::flat_hash_map<std::vector<Literal>, GlobalId> shared_global_id_;
  absl::flat_hash_set<GlobalId> shared_global_ids_;
  std::vector<LratProofStep> last_read_steps_;

  absl::flat_hash_map<GlobalId, ClausePtr> global_id_to_clause_;

  // Remapping of local to global variable indices (possibly empty).
  std::vector<int> local_to_global_var_;
  // The maximum global variable index of the remapped variables.
  int max_global_var_ = 0;

  std::vector<Literal> tmp_literals_;
  std::vector<ClausePtr> tmp_proof_;
  std::vector<LratChecker::RatClauses> tmp_rat_clauses_;
  std::string tmp_clause_str_;
};

// Handles the LRAT proof of a SAT problem by either checking it incrementally
// and/or by saving it to a file.
class LratProofHandler {
 public:
  static std::unique_ptr<LratProofHandler> MaybeCreate(
      Model* model, bool enable_rat_proofs = false);
  static std::unique_ptr<LratProofHandler> MaybeCreate(
      const SatParameters& params, SharedLratProofStatus* proof_status,
      SharedStatistics* stats);

  SharedLratProofStatus* proof_status() const { return proof_status_; }

  bool lrat_check_enabled() const { return lrat_checker_ != nullptr; }
  bool lrat_output_enabled() const { return lrat_writer_ != nullptr; }

  int64_t num_assumed_clauses() const { return num_assumed_clauses_; }

  // Adds a clause of the problem. `one_based_cnf_index` is the clause's 1-based
  // index in the input CNF file, or a nonpositive value if not applicable. See
  // LratChecker for more details.
  bool AddProblemClause(ClausePtr clause, int64_t one_based_cnf_index = 0);

  // No more problem clauses must be added after this call.
  void EndProblemClauses();

  // Adds a clause which is inferred from the problem clauses and/or the
  // previously inferred clauses. See LratChecker for more details.
  bool AddInferredClause(ClausePtr clause, absl::Span<const ClausePtr> proof,
                         bool exported = false) {
    return AddInferredClause(clause, proof, {}, exported);
  }
  bool AddInferredClause(ClausePtr clause, absl::Span<const ClausePtr> proof,
                         absl::Span<const LratChecker::RatClauses> rat_proof,
                         bool exported = false);

  // Rewrites a clause. See LratChecker for more details.
  bool RewriteClause(ClausePtr clause, absl::Span<const Literal> literals,
                     absl::Span<const ClausePtr> proof);

  // This assumes that the 'new_clause' to prove and all the ones needed for the
  // proof only touch a small number of variables (<= 6). It will then prove the
  // new clause by enumerating all possibilities and producing the relevant
  // intermediate LRAT RUP steps.
  //
  // The last two arguments must have the same size and are in one to one
  // correspondence. Note that we might not need all the given clauses in the
  // proof.
  //
  // Returns false if the proof is wrong.
  bool AddAndProveInferredClauseByEnumeration(
      ClausePtr new_clause, absl::Span<const ClausePtr> clauses_for_proof);

  // Adds a clause which was inferred and exported by another worker. Always
  // returns true.
  bool AddImportedClause(ClausePtr clause);

  // Adds a clause which is assumed to be true, without proof. Always returns
  // true.
  bool AddAssumedClause(ClausePtr clause);

  // Exports a clause so that it can be imported by other workers. If you know
  // whether a clause must be exported when it is inferred, it is more efficient
  // to use the `exported` parameter of AddInferredClause(). `clause` must be a
  // previously added clause. This is not needed for unary and binary clauses,
  // which are always exported.
  bool ExportClause(ClausePtr clause);

  // Deletes a problem or inferred clause. If `delete_sat_clause` is true and
  // `clause` is a SatClause pointer, then this SatClause* is deleted.
  void DeleteClause(ClausePtr clause, bool delete_sat_clause = true);

  // Defines a mapping of variable indices for the next clauses added to the
  // merged proof. This mapping is only used in LratMerger. For efficiency, it
  // is currently not used during on-the-fly checks if `lrat_check_enabled()` is
  // true (hence no new clauses should be added to this handler after this
  // method has been called; but new proof handlers directly using the new
  // variables can be created). A variable index i in a next clause will be
  // remapped to `new_to_old_var[i]`.
  void RemapBooleanVariables(absl::Span<const int32_t> new_to_old_var);

  // Returns VALID if all the inferred clauses were successfully checked with
  // LRAT. Returns INVALID if at least one of them was not. Returns UNKNOWN if
  // LRAT checks are not enabled.
  SharedLratProofStatus::Status Valid() const;

  // Returns VALID if the unsatisfiability proof is valid and complete, i.e.
  // whether the empty clause has been successfully inferred. Returns INVALID if
  // it is not. Returns UNKNOWN if LRAT checks are not enabled.
  SharedLratProofStatus::Status Check();

  void Close(bool model_is_unsat);

  // Returns true if the given binary clause has been added to the LRAT proof,
  // and has not been deleted yet.
  bool HasBinaryClause(Literal a, Literal b) const;

  // Returns true if the given binary clause is in the BinaryImplicationGraph.
  // By hypothesis, it is also in the LRAT proof.
  bool HasImplicationGraphClause(Literal a, Literal b) const;

  // Reserved for the BinaryImplicationGraph implementation:
  // - marks a clause as being in the BinaryImplicationGraph. The clause must
  // be added to the LRAT proof first.
  bool AddImplicationGraphClause(Literal a, Literal b);
  // - marks a clause as no longer being in the BinaryImplicationGraph, and
  // deletes it from the LRAT proof.
  void DeleteImplicationGraphClause(ClausePtr clause);
  // - returns all the binary clauses which have been added to the LRAT proof
  // and which have not been deleted, together with a boolean indicating whether
  // they are in the BinaryImplicationGraph.
  const absl::flat_hash_map<ClausePtr, bool>& BinaryClauses();

  // Deletes the binary clauses which have been added with AddProblemClause() or
  // AddInferredClause() since the last call to this method, if they have not
  // been added to the BinaryImplicationGraph in between. Returns the number of
  // clauses deleted.
  int DeleteTemporaryBinaryClauses();

 private:
  LratProofHandler(const SatParameters& params,
                   SharedLratProofStatus* proof_status,
                   SharedStatistics* stats);

  void DeleteClauseInternal(ClausePtr clause);

  bool LratError(absl::string_view message) const;

  const int id_;
  SharedLratProofStatus* proof_status_;
  std::unique_ptr<LratChecker> lrat_checker_;
  std::unique_ptr<LratWriter> lrat_writer_;
  bool debug_crash_on_error_ = false;

  bool all_problem_clauses_loaded_ = false;
  int64_t num_assumed_clauses_ = 0;

  // The binary clauses which have been added to the LRAT proof and have not
  // been deleted yet, and whether they are also in the BinaryImplicationGraph.
  // All these clause pointers are of ClausePtr::kBinaryClause type.
  absl::flat_hash_map<ClausePtr, bool> binary_clauses_;

  // The clauses in `binary_clauses_` which might not be in the
  // BinaryImplicationGraph.
  std::vector<ClausePtr> temporary_binary_clauses_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_LRAT_PROOF_HANDLER_H_
