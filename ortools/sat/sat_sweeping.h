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

#ifndef ORTOOLS_SAT_SAT_SWEEPING_H_
#define ORTOOLS_SAT_SAT_SWEEPING_H_

#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/types/span.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/drat_checker.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/util.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

// This is a heuristic to find pairs of equivalent literals as described in [1].
// The idea is to pick a random variable and define a neighborhood of clauses
// and variables close to this variable. Next we define a local model containing
// only those variables and clauses. Since this model is just a smaller portion
// of the original model, we can expect it to have several feasible solutions.
// Each solution we find reduces the set of possible equivalent variables. For
// example, finding two solutions {l1=0, l2=0, ...} and {l1=0, l2=1, ...}
// implies that `l1` and `l2` are not equivalent. This is done systematically by
// keeping a partitioning of variables into potential clusters, and solving the
// local model each time with the right assumptions to either refine a partition
// or prove that a pair of literals are equivalent. This continue until we are
// sure to have found all the equivalences.
//
// [1] "Clausal Equivalence Sweeping", Armin Biere, Katalin Fazekas, Mathias
// Fleury, Nils Froleyks, 2025.
class EquivalenceSatSweeping {
 public:
  explicit EquivalenceSatSweeping(Model* model)
      : sat_solver_(model->GetOrCreate<SatSolver>()),
        implication_graph_(model->GetOrCreate<BinaryImplicationGraph>()),
        clause_manager_(model->GetOrCreate<ClauseManager>()),
        global_time_limit_(model->GetOrCreate<TimeLimit>()),
        random_(model->GetOrCreate<ModelRandomGenerator>()) {}

  // Do one round of equivalence SAT sweeping.
  // `run_inprocessing` is a function that is called on the model before solving
  // it for the first time. It is useful to call inprocessing without creating a
  // dependency cycle.
  bool DoOneRound(std::function<void(Model*)> run_inprocessing);

 private:
  std::vector<absl::Span<const Literal>> GetNeighborhood(BooleanVariable var);
  void LoadClausesInModel(absl::Span<const SatClause* const> clauses, Model* m);

  Literal Representative(Literal l) {
    auto it = lit_representative_.find(l);
    return it == lit_representative_.end() ? l : it->second;
  }
  BooleanVariable RepresentativeVar(BooleanVariable v) {
    return Representative(Literal(v, true)).Variable();
  }

  SatSolver* sat_solver_;
  BinaryImplicationGraph* implication_graph_;
  ClauseManager* clause_manager_;
  TimeLimit* global_time_limit_;
  ModelRandomGenerator* random_;

  int max_num_clauses_ = 52000;
  int max_num_boolean_variables_ = 2000;

  // We compute the occurrence graph once at the beginning of each round.
  util_intops::StrongVector<ClauseIndex, absl::Span<const Literal>> clauses_;
  MergeableOccurrenceList<BooleanVariable, ClauseIndex> var_to_clauses_;
  absl::flat_hash_map<Literal, Literal> lit_representative_;

  absl::flat_hash_map<BooleanVariable, BooleanVariable>
      big_model_to_small_model_;
  util_intops::StrongVector<BooleanVariable, BooleanVariable>
      small_model_to_big_model_;
};

// Do a full SAT sweeping on the model defined by the clauses.
// The `status` of the result is either FEASIBLE, INFEASIBLE or LIMIT_REACHED.
// If the result is LIMIT_REACHED, the returned clauses are valid, but they are
// not exhaustive. If the result is FEASIBLE, all possible binary clauses that
// define equivalences and all possible unary clauses of the model are
// guaranteed to be either in the `clauses` input or in the output. Many binary
// clauses that are not equivalences will be returned too, but not necessarily
// all of them. This call increases the deterministic time of the `time_limit`.
struct SatSweepingResult {
  // Literals that if are set to false make the problem unsat.
  std::vector<Literal> unary_clauses;
  // Pairs of literals that if are both set to false make the problem unsat.
  // We filter out the clauses that are already in the input.
  std::vector<std::pair<Literal, Literal>> binary_clauses;
  // TODO(user): also return small clauses of size > 2?
  SatSolver::Status status;
  std::vector<std::pair<Literal, Literal>> new_equivalences;
};
SatSweepingResult DoFullSatSweeping(
    const CompactVectorVector<int, Literal>& clauses, TimeLimit* time_limit,
    std::function<void(Model*)> configure_model_before_first_solve);

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_SAT_SWEEPING_H_
