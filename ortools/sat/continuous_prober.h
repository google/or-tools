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

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/random/bit_gen_ref.h"
#include "absl/strings/string_view.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/model.h"
#include "ortools/sat/probing.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_inprocessing.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/synchronization.h"
#include "ortools/util/running_stat.h"
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
  static constexpr int kNumPairsOfBooleanVarsToProbe = 5000;

  struct MethodStats {
    explicit MethodStats(absl::string_view name = "") : name(name) {}
    virtual ~MethodStats() = default;

    virtual void AddToStats(
        std::vector<std::pair<std::string, int64_t>>& stats) const;
    virtual std::string StatsStr() const;

    std::string name;
    int64_t num_calls = 0;
    int64_t num_new_bounds = 0;
    int64_t num_new_literals_fixed = 0;
    int64_t num_new_binary = 0;
  };

  struct ProbingStats : public MethodStats {
    explicit ProbingStats(absl::string_view state_name = "")
        : MethodStats(state_name) {}
    ~ProbingStats() override = default;

    void AddToStats(
        std::vector<std::pair<std::string, int64_t>>& stats) const override;
    std::string StatsStr() const override;

    int iteration = 1;
    int current_var = 0;
  };

  struct ShavingStats : public ProbingStats {
    static constexpr int kUpdateFrequency = 30;
    static constexpr int kRateWindowSizeMax = 200;
    static constexpr double kLimitMultiplierMax = 20.0;
    static constexpr double kLimitIncrease = 1.5;

    explicit ShavingStats(double dtime, absl::string_view state_name = "")
        : ProbingStats(state_name), base_dtime(dtime) {}
    ~ShavingStats() override = default;

    void AddToStats(
        std::vector<std::pair<std::string, int64_t>>& stats) const override;
    std::string StatsStr() const override;
    void AdaptMultiplier(bool success, int num_vars);

    const double base_dtime;
    double limit_multiplier = 1.0;
    RunningAverage success_tracker;
    int limit_update_frequency = 0;
    int ineffective_streak = 0;
    int limit_reset_count = 0;
  };

  // Probes variable bounds. Returns a status if the main Probe() function
  // should abort and return this status, or nullopt if probing should continue.
  std::optional<SatSolver::Status> ProbeIntegerVariables(double deadline);
  // Shaves variable bounds. Returns a status if the main Probe() function
  // should abort and return this status, or nullopt if shaving should continue.
  std::optional<SatSolver::Status> ShaveIntegerVariables(double deadline);
  // Probes Boolean variables from the model.
  std::optional<SatSolver::Status> ProbeBooleanVariables(double deadline);
  // Shaves Boolean variables from the model.
  std::optional<SatSolver::Status> ShaveBooleanVariables(double deadline);
  // Probes clauses of the SAT model.
  std::optional<SatSolver::Status> ProbeAtLeastOnes(double deadline);
  // Probes at_most_ones of the SAT model.
  std::optional<SatSolver::Status> ProbeAtMostOnes(double deadline);
  // Probes combinations of Booleans variables.
  std::optional<SatSolver::Status> ProbePairsOfBoolVars(double deadline);

  // Returns true if bool_var is not assigned and not redundant.
  bool ShouldProbe(BooleanVariable bool_var) const;
  // Returns true if var is the representative of its orbit.
  bool IsOrbitRepresentative(int var) const;
  void CompactAndShuffleBooleanVariables(
      std::vector<BooleanVariable>& bool_vars);
  void CompactAndShuffleIntegerVariables(
      std::vector<IntegerVariable>& int_vars);
  SatSolver::Status ShaveLiteral(Literal literal,
                                 bool literal_is_an_integer_bound);
  bool ReportStatus(SatSolver::Status status);
  void LogStatistics();
  SatSolver::Status PeriodicSyncAndCheck();

  MethodStats GetStats(Prober* prober) const;
  static void AddStats(MethodStats& total_stats, const MethodStats& start_stats,
                       const MethodStats& end_stats);

  // Variables containers. We reuse the bool_vars_to_probe_ for the pair of
  // Booleans probing method.
  std::vector<BooleanVariable> bool_vars_to_probe_;
  std::vector<BooleanVariable> bool_vars_to_shave_;
  std::vector<IntegerVariable> int_vars_to_probe_;
  std::vector<IntegerVariable> int_vars_to_shave_;

  // Model object.
  Model* model_;
  CpModelMapping* cp_model_mapping_;
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
  const double base_dtime_;
  const bool use_shaving_ = false;

  // Var representatives, for the symmetry of the model proto.
  std::vector<int> var_to_representative_;

  // Period counters;
  int num_logs_remaining_ = 0;
  int num_syncs_remaining_ = 0;
  int num_test_limit_remaining_ = 0;

  // Current statistics and states of the continuous probing and shaving
  // process.
  MethodStats at_least_one_stats_;
  MethodStats at_most_one_stats_;
  MethodStats pairs_stats_;
  ProbingStats bool_probing_;
  ProbingStats int_probing_;
  ShavingStats bool_shaving_;
  ShavingStats int_shaving_;
  std::vector<std::vector<Literal>> tmp_dnf_;
  std::vector<Literal> tmp_literals_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_CONTINUOUS_PROBER_H_
