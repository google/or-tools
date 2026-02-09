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

#ifndef ORTOOLS_SAT_GATE_CONGRUENCE_CLOSURE_H_
#define ORTOOLS_SAT_GATE_CONGRUENCE_CLOSURE_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/hash/hash.h"
#include "absl/types/span.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/gate_utils.h"
#include "ortools/sat/lrat_proof_handler.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"
#include "ortools/util/bitset.h"
#include "ortools/util/logging.h"
#include "ortools/util/strong_integers.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

// If we have a = f(literals) and b = f(same_literals), then a and b are
// equivalent.
//
// This class first detects such relationships, then reaches the "equivalence"
// fixed point (i.e. closure) by detecting equivalences, then updating the RHS
// of the relations which might lead to more equivalences and so on.
//
// This mostly follows the paper "Clausal Congruence closure", Armin Biere,
// Katalin Fazekas, Mathias Fleury, Nils Froleyks, 2024.
//
// TODO(user): For now we only deal with f() being an and_gate with an arbitrary
// number of inputs, or equivalently target = product/and(literals). The next
// most important one is xor().
//
// TODO(user): What is the relation with symmetry ? It feel like all the
// equivalences found here should be in the same symmetry orbit ?
DEFINE_STRONG_INDEX_TYPE(GateId);
class GateCongruenceClosure {
 public:
  explicit GateCongruenceClosure(Model* model)
      : assignment_(model->GetOrCreate<Trail>()->Assignment()),
        sat_solver_(model->GetOrCreate<SatSolver>()),
        trail_(model->GetOrCreate<Trail>()),
        implication_graph_(model->GetOrCreate<BinaryImplicationGraph>()),
        clause_manager_(model->GetOrCreate<ClauseManager>()),
        lrat_proof_handler_(model->Mutable<LratProofHandler>()),
        shared_stats_(model->GetOrCreate<SharedStatistics>()),
        logger_(model->GetOrCreate<SolverLogger>()),
        time_limit_(model->GetOrCreate<TimeLimit>()) {}

  ~GateCongruenceClosure();

  bool DoOneRound(bool log_info);

  // This is meant to be called as soon as possible, before any inprocessing is
  // run to try to keep the structural information from the model.
  void EarlyGateDetection();

 private:
  DEFINE_STRONG_INDEX_TYPE(TruthTableId);

  struct GateHash {
    explicit GateHash(util_intops::StrongVector<GateId, int>* t,
                      CompactVectorVector<GateId, Literal>* g)
        : gates_type(t), gates_inputs(g) {}
    std::size_t operator()(GateId gate_id) const {
      return absl::HashOf((*gates_type)[gate_id], (*gates_inputs)[gate_id]);
    }
    const util_intops::StrongVector<GateId, int>* gates_type;
    const CompactVectorVector<GateId, Literal>* gates_inputs;
  };

  struct GateEq {
    explicit GateEq(util_intops::StrongVector<GateId, int>* t,
                    CompactVectorVector<GateId, Literal>* g)
        : gates_type(t), gates_inputs(g) {}
    bool operator()(GateId gate_a, GateId gate_b) const {
      if (gate_a == gate_b) return true;

      // We use absl::span<> comparison.
      return ((*gates_type)[gate_a] == (*gates_type)[gate_b]) &&
             ((*gates_inputs)[gate_a] == (*gates_inputs)[gate_b]);
    }
    const util_intops::StrongVector<GateId, int>* gates_type;
    const CompactVectorVector<GateId, Literal>* gates_inputs;
  };

  // Display one line with all the gate info.
  std::string GateDebugString(GateId id) const;

  // As we presolve the model, some clause can be shrinked and we can loose
  // some structural information. This seeds the small truth-table detection
  // with information we already had.
  void ProcessPreviousTruthTables(PresolveTimer& timer);

  // Initialize data-structures and call
  // ExtractAndGatesAndFillShortTruthTables() then ExtractShortGates().
  void StructureExtraction(PresolveTimer& timer);

  // Recovers "target_literal = and(literals)" from the model.
  //
  // The current algo is pretty basic: For all clauses, look for literals that
  // imply all the others to false. We only look at direct implications to be
  // fast.
  //
  // This is because such an and_gate is encoded as:
  // - for all i, target_literal => literal_i  (direct binary implication)
  // - all literal at true => target_literal, this is a clause:
  //   (not(literal[i]) for all i, target_literal).
  void ExtractAndGatesAndFillShortTruthTables(PresolveTimer& timer);

  // From possible assignment of small set of variables (truth_tables), extract
  // functions of the form one_var = f(other_vars).
  void ExtractShortGates(PresolveTimer& timer);

  // Detects gates encoded in the given truth table, and add them to the set
  // of gates. Returns the number of gate detected.
  int ProcessTruthTable(
      absl::Span<const BooleanVariable> inputs, SmallBitset truth_table,
      absl::Span<const TruthTableId> ids_for_proof,
      absl::Span<const std::pair<Literal, Literal>> binary_used);

  // Add a small clause to the corresponding truth table.
  template <int arity>
  void AddToTruthTable(SatClause* clause,
                       absl::flat_hash_map<std::array<BooleanVariable, arity>,
                                           TruthTableId>& ids);

  // Make sure the small gate at given id is canonicalized.
  // Returns its number of inputs.
  int CanonicalizeShortGate(GateId id);

  const VariablesAssignment& assignment_;
  SatSolver* sat_solver_;
  Trail* trail_;
  BinaryImplicationGraph* implication_graph_;
  ClauseManager* clause_manager_;
  LratProofHandler* lrat_proof_handler_;
  SharedStatistics* shared_stats_;
  SolverLogger* logger_;
  TimeLimit* time_limit_;

  SparseBitset<LiteralIndex> marked_;
  SparseBitset<LiteralIndex> seen_;
  SparseBitset<LiteralIndex> next_seen_;

  // A Boolean gates correspond to target = f(inputs).
  //
  // Note that the inputs are canonicalized. For and_gates, they are sorted,
  // since the gate function does not depend on the order. The type of an
  // and_gates is kAndGateType.
  //
  // Otherwise, we support generic 2 and 3 inputs gates where the type is the
  // truth table. i.e. target = type[sum value_of_inputs[i] * 2^i]. For such
  // gate, the target and inputs will always be canonicalized to positive and
  // sorted literal. We just update the truth table accordingly.
  //
  // If lrat_proof_handler_ != nullptr, we also store all the SatClause* needed
  // to infer such gate from the clause database. The case of kAndGateType is
  // special, because we don't have SatClause for the binary clauses used to
  // infer it. We will thus only store the base clause used, if we have a =
  // and(x,y,...) we only store the clause "x and y and ... => a".
  static constexpr int kAndGateType = -1;
  util_intops::StrongVector<GateId, Literal> gates_target_;
  util_intops::StrongVector<GateId, int> gates_type_;
  CompactVectorVector<GateId, Literal> gates_inputs_;
  CompactVectorVector<GateId, const SatClause*> gates_clauses_;

  // Truth tables on 2 variables are handled differently, and we don't use
  // a TruthTableId indirection.
  //
  // TODO(user): it feels like we could benefit from just storing this all
  // the time in the binary_implication graph. This allow to never add duplicate
  // and detect easy case of fixing/equivalences right away. To investigate.
  absl::flat_hash_map<std::array<BooleanVariable, 2>, SmallBitset> ids2_;

  // Map (Xi) (sorted) to a bitmask corresponding to the allowed values.
  // We loop over all short clauses to fill this. We actually store an "id"
  // pointing in the vectors below.
  //
  // TruthTableIds are assigned in insertion order. We copy the map key in
  // truth_tables_inputs_, this is a bit wasted but simplify the code.
  absl::flat_hash_map<std::array<BooleanVariable, 3>, TruthTableId> ids3_;
  absl::flat_hash_map<std::array<BooleanVariable, 4>, TruthTableId> ids4_;
  absl::flat_hash_map<std::array<BooleanVariable, 5>, TruthTableId> ids5_;
  CompactVectorVector<TruthTableId, BooleanVariable> truth_tables_inputs_;
  util_intops::StrongVector<TruthTableId, SmallBitset> truth_tables_bitset_;
  CompactVectorVector<TruthTableId, SatClause*> truth_tables_clauses_;

  // Only used for logs.
  util_intops::StrongVector<TruthTableId, SmallBitset> old_truth_tables_bitset_;

  // Temporary vector used to construct truth_tables_clauses_.
  std::vector<TruthTableId> tmp_ids_;
  std::vector<SatClause*> tmp_clauses_;

  // Temporary SatClause* for binary, so we don't need to specialize too much
  // code for them.
  std::vector<std::unique_ptr<SatClause>> tmp_binary_clauses_;

  // For stats.
  double total_dtime_ = 0.0;
  double total_wtime_ = 0.0;
  int64_t total_num_units_ = 0;
  int64_t total_gates_ = 0;
  int64_t total_equivalences_ = 0;
};

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_GATE_CONGRUENCE_CLOSURE_H_
