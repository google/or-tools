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

// This file contains all the top-level logic responsible for driving the search
// of a satisfiability integer problem. What decision we take next, which new
// Literal associated to an IntegerLiteral we create and when we restart.
//
// For an optimization problem, our algorithm solves a sequence of decision
// problem using this file as an entry point. Note that some heuristics here
// still use the objective if there is one in order to orient the search towards
// good feasible solution though.

#ifndef ORTOOLS_SAT_CONTINUOUS_PROBER_H_
#define ORTOOLS_SAT_CONTINUOUS_PROBER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/strings/string_view.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/model.h"
#include "ortools/sat/probing.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_inprocessing.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/synchronization.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

// This class will loop continuously on model variables and try to probe/shave
// its bounds.
class ContinuousProber {
 public:
  // The model_proto is just used to construct the lists of variable to probe.
  ContinuousProber(const CpModelProto& model_proto, Model* model);
  ~ContinuousProber();

  // Starts or continues probing variables and their bounds.
  // It returns:
  //   - SatSolver::INFEASIBLE if the problem is proven infeasible.
  //   - SatSolver::FEASIBLE when a feasible solution is found
  //   - SatSolver::LIMIT_REACHED if the limit stored in the model is reached
  // Calling Probe() after it has returned FEASIBLE or LIMIT_REACHED will resume
  // probing from its previous state.
  SatSolver::Status Probe();

 private:
  static const int kTestLimitPeriod = 20;
  static const int kLogPeriod = 5000;
  static const int kSyncPeriod = 50;

  struct MethodStats {
    explicit MethodStats(absl::string_view name = "") : name(name) {}
    std::string name;
    int64_t num_calls = 0;
    int64_t num_new_bounds = 0;
    int64_t num_new_literals_fixed = 0;
    int64_t num_new_binary = 0;
  };

  SatSolver::Status ShaveLiteral(Literal literal,
                                 bool literal_is_an_integer_bound);
  bool ReportStatus(SatSolver::Status status);
  void LogStatistics();
  SatSolver::Status PeriodicSyncAndCheck();
  bool use_shaving() const;

  MethodStats GetStats(Prober* prober) const;
  static void AddStats(MethodStats& total_stats, const MethodStats& start_stats,
                       const MethodStats& end_stats);

  // Variables to probe.
  std::vector<BooleanVariable> bool_vars_;
  std::vector<IntegerVariable> int_vars_;

  // Model object.
  Model* model_;
  SatSolver* sat_solver_;
  TimeLimit* time_limit_;
  BinaryImplicationGraph* binary_implication_graph_;
  ClauseManager* clause_manager_;
  Trail* trail_;
  IntegerTrail* integer_trail_;
  IntegerEncoder* encoder_;
  Inprocessing* inprocessing_;
  const SatParameters parameters_;
  LevelZeroCallbackHelper* level_zero_callbacks_;
  Prober* prober_;
  SharedResponseManager* shared_response_manager_;
  SharedBoundsManager* shared_bounds_manager_;
  SharedStatistics* shared_stats_;
  absl::BitGenRef random_;

  // Statistics.
  struct Counters {
    MethodStats at_least_one_stats = MethodStats("at_least_one");
    MethodStats at_most_one_stats = MethodStats("at_most_one");
    MethodStats one_variable_stats = MethodStats("one_variable");
    MethodStats pairs_stats = MethodStats("pairs");
    MethodStats shaving = MethodStats("shaving");
  };
  Counters counters_;

  // Period counters;
  int num_logs_remaining_ = 0;
  int num_syncs_remaining_ = 0;
  int num_test_limit_remaining_ = 0;

  // Current state of the probe.
  double active_limit_;
  // TODO(user): use 2 vector<bool>.
  absl::flat_hash_set<BooleanVariable> probed_bool_vars_;
  absl::flat_hash_set<LiteralIndex> shaved_literals_;
  int iteration_ = 1;
  int current_int_var_ = 0;
  int current_bool_var_ = 0;
  int current_bv1_ = 0;
  int current_bv2_ = 0;
  int random_pair_of_bool_vars_probed_ = 0;
  std::vector<std::vector<Literal>> tmp_dnf_;
  std::vector<Literal> tmp_literals_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_CONTINUOUS_PROBER_H_
