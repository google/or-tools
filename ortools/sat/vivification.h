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

#ifndef ORTOOLS_SAT_VIVIFICATION_H_
#define ORTOOLS_SAT_VIVIFICATION_H_

#include <cstdint>

#include "absl/base/attributes.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/lrat_proof_handler.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/util/logging.h"
#include "ortools/util/time_limit.h"

namespace operations_research::sat {

// Helper class responsible for "vivifying" clauses.
//
// See for instance "Clause Vivification by Unit Propagation in CDCL SAT
// Solvers" Chu-Min Li, Fan Xiao, Mao Luo, Felip Manyà, Zhipeng Lü, Yu Li.
//
// This is basically trying to minimize clauses using propagation by taking as
// decisions the negation of some literals of that clause.
class Vivifier {
 public:
  explicit Vivifier(Model* model)
      : assignment_(model->GetOrCreate<Trail>()->Assignment()),
        parameters_(*model->GetOrCreate<SatParameters>()),
        time_limit_(model->GetOrCreate<TimeLimit>()),
        logger_(model->GetOrCreate<SolverLogger>()),
        sat_solver_(model->GetOrCreate<SatSolver>()),
        trail_(model->GetOrCreate<Trail>()),
        binary_clauses_(model->GetOrCreate<BinaryImplicationGraph>()),
        clause_manager_(model->GetOrCreate<ClauseManager>()),
        clause_id_generator_(model->GetOrCreate<ClauseIdGenerator>()),
        lrat_proof_handler_(model->Mutable<LratProofHandler>()) {}

  // Minimize a batch of clauses using propagation.
  // Returns false on UNSAT.
  ABSL_MUST_USE_RESULT bool MinimizeByPropagation(
      bool log_info, double dtime_budget,
      bool minimize_new_clauses_only = false);

  // Values from the last MinimizeByPropagation() call.
  int64_t last_num_vivified() const { return last_num_vivified_; }
  int64_t last_num_literals_removed() const {
    return last_num_literals_removed_;
  }

  // Sum of everything ever done by this class.
  struct Counters {
    int64_t num_clauses_vivified = 0;
    int64_t num_decisions = 0;
    int64_t num_true = 0;
    int64_t num_subsumed = 0;
    int64_t num_removed_literals = 0;
    int64_t num_reused = 0;
    int64_t num_conflicts = 0;
  };
  Counters counters() const { return counters_; }

 private:
  // Use propagation to try to minimize the given clause. This is really similar
  // to MinimizeCoreWithPropagation(). Note that because this does a small tree
  // search, it will impact the variable/clause activities and may add new
  // conflicts.
  ABSL_MUST_USE_RESULT bool TryToMinimizeClause(SatClause* clause);

  // Returns true if variable is fixed in the current assignment due to
  // non-removable clauses, plus at most one removable clause with size <=
  // max_size.
  bool SubsumptionIsInteresting(BooleanVariable variable, int max_size);
  void KeepAllClausesUsedToInfer(BooleanVariable variable);

  const VariablesAssignment& assignment_;
  const SatParameters& parameters_;

  TimeLimit* time_limit_;
  SolverLogger* logger_;
  SatSolver* sat_solver_;
  Trail* trail_;
  BinaryImplicationGraph* binary_clauses_;
  ClauseManager* clause_manager_;
  ClauseIdGenerator* clause_id_generator_;
  LratProofHandler* lrat_proof_handler_ = nullptr;

  Counters counters_;

  int64_t last_num_vivified_ = 0;
  int64_t last_num_literals_removed_ = 0;
};

}  // namespace operations_research::sat
#endif  // ORTOOLS_SAT_VIVIFICATION_H_
