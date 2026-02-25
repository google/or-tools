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

#ifndef ORTOOLS_CONSTRAINT_SOLVER_DEFAULT_SEARCH_H_
#define ORTOOLS_CONSTRAINT_SOLVER_DEFAULT_SEARCH_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/util/cached_log.h"

namespace operations_research {

// This class follows the domains of variables and will report the log of the
// search space of all integer variables.
class DomainWatcher {
 public:
  DomainWatcher(const std::vector<IntVar*>& vars, int cache_size);
  DomainWatcher(const DomainWatcher&) = delete;
  DomainWatcher& operator=(const DomainWatcher&) = delete;

  double LogSearchSpaceSize();
  double Log2(int64_t size) const;

 private:
  std::vector<IntVar*> vars_;
  CachedLog cached_log_;
};

class FindVar : public DecisionVisitor {
 public:
  enum Operation { NONE, ASSIGN, SPLIT_LOW, SPLIT_HIGH };

  FindVar();
  ~FindVar() override;

  void VisitSetVariableValue(IntVar* var, int64_t value) override;
  void VisitSplitVariableDomain(IntVar* var, int64_t value,
                                bool start_with_lower_half) override;
  void VisitScheduleOrPostpone(IntervalVar* var, int64_t est) override;
  void VisitTryRankFirst(SequenceVar* sequence, int index);
  void VisitTryRankLast(SequenceVar* sequence, int index);
  void VisitUnknownDecision() override;

  IntVar* var() const;
  int64_t value() const;
  Operation operation() const;

  std::string DebugString() const override;

 private:
  IntVar* var_;
  int64_t value_;
  Operation operation_;
};

// This class initialize impacts by scanning each value of the domain
// of the variable.
class InitVarImpacts : public DecisionBuilder {
 public:
  InitVarImpacts();
  ~InitVarImpacts() override;

  void UpdateImpacts();
  void Init(IntVar* var, IntVarIterator* iterator, int var_index);
  Decision* Next(Solver* solver) override;
  void set_update_impact_callback(std::function<void(int, int64_t)> callback);

 private:
  class AssignCallFail : public Decision {
   public:
    explicit AssignCallFail(const std::function<void()>& update_impact_closure);
    AssignCallFail(const AssignCallFail&) = delete;
    AssignCallFail& operator=(const AssignCallFail&) = delete;
    ~AssignCallFail() override;
    void Apply(Solver* solver) override;
    void Refute(Solver* solver) override;

    IntVar* var_;
    int64_t value_;

   private:
    const std::function<void()>& update_impact_closure_;
  };

  IntVar* var_;
  std::function<void(int, int64_t)> update_impact_callback_;
  bool new_start_;
  IntVarIterator* iterator_;
  int var_index_;
  std::vector<int64_t> active_values_;
  int value_index_;
  std::function<void()> update_impact_closure_;
  AssignCallFail updater_;
};

class InitVarImpactsWithSplits : public DecisionBuilder {
 public:
  explicit InitVarImpactsWithSplits(int split_size);
  ~InitVarImpactsWithSplits() override;

  void UpdateImpacts();
  void Init(IntVar* var, IntVarIterator* iterator, int var_index);
  int64_t IntervalStart(int index) const;
  Decision* Next(Solver* solver) override;
  void set_update_impact_callback(std::function<void(int, int64_t)> callback);

 private:
  class AssignIntervalCallFail : public Decision {
   public:
    explicit AssignIntervalCallFail(
        const std::function<void()>& update_impact_closure);
    AssignIntervalCallFail(const AssignIntervalCallFail&) = delete;
    AssignIntervalCallFail& operator=(const AssignIntervalCallFail&) = delete;
    ~AssignIntervalCallFail() override;
    void Apply(Solver* solver) override;
    void Refute(Solver* solver) override;

    IntVar* var_;
    int64_t value_min_;
    int64_t value_max_;

   private:
    const std::function<void()>& update_impact_closure_;
  };

  IntVar* var_;
  std::function<void(int, int64_t)> update_impact_callback_;
  bool new_start_;
  IntVarIterator* iterator_;
  int var_index_;
  int64_t min_value_;
  int64_t max_value_;
  const int split_size_;
  int split_index_;
  std::function<void()> update_impact_closure_;
  AssignIntervalCallFail updater_;
};
// This class will record the impacts of all assignment of values to
// variables. Its main output is to find the optimal pair (variable/value)
// based on default phase parameters.
class ImpactRecorder : public SearchMonitor {
 public:
  static const int kLogCacheSize;
  static const double kPerfectImpact;
  static const double kFailureImpact;
  static const double kInitFailureImpact;
  static const int kUninitializedVarIndex;

  ImpactRecorder(Solver* solver, DomainWatcher* domain_watcher,
                 const std::vector<IntVar*>& vars,
                 DefaultPhaseParameters::DisplayLevel display_level);
  ImpactRecorder(const ImpactRecorder&) = delete;
  ImpactRecorder& operator=(const ImpactRecorder&) = delete;

  void ApplyDecision(Decision* d) override;
  void AfterDecision(Decision* d, bool apply) override;
  void BeginFail() override;
  void ResetAllImpacts();
  void UpdateImpact(int var_index, int64_t value, double impact);
  void InitImpact(int var_index, int64_t value);
  void FirstRun(int64_t splits);
  // This method scans the domain of one variable and returns the sum
  // of the impacts of all values in its domain, along with the value
  // with minimal impact.
  void ScanVarImpacts(int var_index, int64_t* best_impact_value,
                      double* var_impacts,
                      DefaultPhaseParameters::VariableSelection var_select,
                      DefaultPhaseParameters::ValueSelection value_select);
  std::string DebugString() const override;

 private:
  class FirstRunVariableContainers : public BaseObject {
   public:
    FirstRunVariableContainers(ImpactRecorder* impact_recorder, int64_t splits);
    std::function<void(int, int64_t)> update_impact_callback() const;
    void PushBackRemovedValue(int64_t value);
    bool HasRemovedValues() const;
    void ClearRemovedValues();
    size_t NumRemovedValues() const;
    const std::vector<int64_t>& removed_values() const;
    InitVarImpacts* without_split();
    InitVarImpactsWithSplits* with_splits();
    std::string DebugString() const override;

   private:
    const std::function<void(int, int64_t)> update_impact_callback_;
    std::vector<int64_t> removed_values_;
    InitVarImpacts without_splits_;
    InitVarImpactsWithSplits with_splits_;
  };

  DomainWatcher* const domain_watcher_;
  std::vector<IntVar*> vars_;
  const int size_;
  double current_log_space_;
  std::vector<std::vector<double>> impacts_;
  std::vector<int64_t> original_min_;
  std::unique_ptr<IntVarIterator*[]> domain_iterators_;
  int64_t init_count_;
  const DefaultPhaseParameters::DisplayLevel display_level_;
  int current_var_;
  int64_t current_value_;
  FindVar find_var_;
  absl::flat_hash_map<const IntVar*, int> var_map_;
  bool init_done_;
};

class RunHeuristicsAsDives : public Decision {
 public:
  RunHeuristicsAsDives(Solver* solver, const std::vector<IntVar*>& vars,
                       DefaultPhaseParameters::DisplayLevel level,
                       bool run_all_heuristics, int random_seed,
                       int heuristic_period, int heuristic_num_failures_limit);
  ~RunHeuristicsAsDives() override;

  void Apply(Solver* solver) override;
  void Refute(Solver* solver) override;
  bool ShouldRun();
  bool RunOneHeuristic(Solver* solver, int index);
  bool RunAllHeuristics(Solver* solver);
  int Rand32(int size);
  void Init(Solver* solver, const std::vector<IntVar*>& vars,
            int heuristic_num_failures_limit);
  int heuristic_runs() const;

 private:
  struct HeuristicWrapper {
    HeuristicWrapper(Solver* solver, const std::vector<IntVar*>& vars,
                     Solver::IntVarStrategy var_strategy,
                     Solver::IntValueStrategy value_strategy,
                     const std::string& heuristic_name, int heuristic_runs);
    DecisionBuilder* const phase;
    const std::string name;
    const int runs;
  };

  std::vector<HeuristicWrapper*> heuristics_;
  SearchMonitor* heuristic_limit_;
  DefaultPhaseParameters::DisplayLevel display_level_;
  bool run_all_heuristics_;
  std::mt19937 random_;
  const int heuristic_period_;
  int heuristic_branch_count_;
  int heuristic_runs_;
};

class DefaultIntegerSearch : public DecisionBuilder {
 public:
  static const double kSmallSearchSpaceLimit;

  DefaultIntegerSearch(Solver* solver, const std::vector<IntVar*>& vars,
                       const DefaultPhaseParameters& parameters);
  ~DefaultIntegerSearch() override;

  Decision* Next(Solver* solver) override;
  void ClearLastDecision();
  void AppendMonitors(Solver* solver,
                      std::vector<SearchMonitor*>* extras) override;
  void Accept(ModelVisitor* visitor) const override;
  std::string DebugString() const override;
  std::string StatString() const;

 private:
  void CheckInit(Solver* solver);
  Decision* ImpactNext(Solver* solver);

  std::vector<IntVar*> vars_;
  DefaultPhaseParameters parameters_;
  DomainWatcher domain_watcher_;
  ImpactRecorder impact_recorder_;
  RunHeuristicsAsDives heuristics_;
  FindVar find_var_;
  IntVar* last_int_var_;
  int64_t last_int_value_;
  FindVar::Operation last_operation_;
  int last_conflict_count_;
  bool init_done_;
};

std::string DefaultPhaseStatString(DecisionBuilder* db);

}  // namespace operations_research

#endif  // ORTOOLS_CONSTRAINT_SOLVER_DEFAULT_SEARCH_H_
