// Copyright 2010-2014 Google
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


#include <algorithm>
#include <cmath>
#include <cstddef>
#include <unordered_map>
#include <string>
#include <utility>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/stringprintf.h"
#include "ortools/base/file.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/hash.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/constraint_solver/demon_profiler.pb.h"
#include "ortools/base/status.h"

namespace operations_research {
namespace {
struct Container {
  Container(const Constraint* ct_, int64 value_) : ct(ct_), value(value_) {}
  bool operator<(const Container& c) const { return value > c.value; }

  const Constraint* ct;
  int64 value;
};
}  // namespace

// DemonProfiler manages the profiling of demons and allows access to gathered
// data. Add this class as a parameter to Solver and access its information
// after the end of a search.
class DemonProfiler : public PropagationMonitor {
 public:
  explicit DemonProfiler(Solver* const solver)
      : PropagationMonitor(solver),
        active_constraint_(nullptr),
        active_demon_(nullptr),
        start_time_ns_(base::GetCurrentTimeNanos()) {}

  ~DemonProfiler() override {
    STLDeleteContainerPairSecondPointers(constraint_map_.begin(),
                                         constraint_map_.end());
  }

  // In microseconds.
  // TODO(user): rename and return nanoseconds.
  int64 CurrentTime() const {
    return (base::GetCurrentTimeNanos() - start_time_ns_) / 1000;
  }

  void BeginConstraintInitialPropagation(
      Constraint* const constraint) override {
    if (solver()->state() == Solver::IN_SEARCH) {
      return;
    }

    CHECK(active_constraint_ == nullptr);
    CHECK(active_demon_ == nullptr);
    CHECK(constraint != nullptr);
    ConstraintRuns* const ct_run = new ConstraintRuns;
    ct_run->set_constraint_id(constraint->DebugString());
    ct_run->add_initial_propagation_start_time(CurrentTime());
    active_constraint_ = constraint;
    constraint_map_[constraint] = ct_run;
  }

  void EndConstraintInitialPropagation(Constraint* const constraint) override {
    CHECK(active_constraint_ != nullptr);
    CHECK(active_demon_ == nullptr);
    CHECK(constraint != nullptr);
    CHECK_EQ(constraint, active_constraint_);
    ConstraintRuns* const ct_run = constraint_map_[constraint];
    if (ct_run != nullptr) {
      ct_run->add_initial_propagation_end_time(CurrentTime());
      ct_run->set_failures(0);
    }
    active_constraint_ = nullptr;
  }

  void BeginNestedConstraintInitialPropagation(
      Constraint* const constraint, Constraint* const delayed) override {
    if (solver()->state() == Solver::IN_SEARCH) {
      return;
    }

    CHECK(active_constraint_ == nullptr);
    CHECK(active_demon_ == nullptr);
    CHECK(constraint != nullptr);
    CHECK(delayed != nullptr);
    ConstraintRuns* const ct_run = constraint_map_[constraint];
    ct_run->add_initial_propagation_start_time(CurrentTime());
    active_constraint_ = constraint;
  }

  void EndNestedConstraintInitialPropagation(
      Constraint* const constraint, Constraint* const delayed) override {
    CHECK(active_constraint_ != nullptr);
    CHECK(active_demon_ == nullptr);
    CHECK(constraint != nullptr);
    CHECK(delayed != nullptr);
    CHECK_EQ(constraint, active_constraint_);
    ConstraintRuns* const ct_run = constraint_map_[constraint];
    if (ct_run != nullptr) {
      ct_run->add_initial_propagation_end_time(CurrentTime());
      ct_run->set_failures(0);
    }
    active_constraint_ = nullptr;
  }

  void RegisterDemon(Demon* const demon) override {
    if (solver()->state() == Solver::IN_SEARCH) {
      return;
    }

    if (demon_map_.find(demon) == demon_map_.end()) {
      CHECK(active_constraint_ != nullptr);
      CHECK(active_demon_ == nullptr);
      CHECK(demon != nullptr);
      ConstraintRuns* const ct_run = constraint_map_[active_constraint_];
      DemonRuns* const demon_run = ct_run->add_demons();
      demon_run->set_demon_id(demon->DebugString());
      demon_run->set_failures(0);
      demon_map_[demon] = demon_run;
      demons_per_constraint_[active_constraint_].push_back(demon_run);
    }
  }

  void BeginDemonRun(Demon* const demon) override {
    if (demon->priority() == Solver::VAR_PRIORITY) {
      return;
    }
    CHECK(active_demon_ == nullptr);
    CHECK(demon != nullptr);
    active_demon_ = demon;
    DemonRuns* const demon_run = demon_map_[active_demon_];
    if (demon_run != nullptr) {
      demon_run->add_start_time(CurrentTime());
    }
  }

  void EndDemonRun(Demon* const demon) override {
    if (demon->priority() == Solver::VAR_PRIORITY) {
      return;
    }
    CHECK_EQ(active_demon_, demon);
    CHECK(demon != nullptr);
    DemonRuns* const demon_run = demon_map_[active_demon_];
    if (demon_run != nullptr) {
      demon_run->add_end_time(CurrentTime());
    }
    active_demon_ = nullptr;
  }

  void StartProcessingIntegerVariable(IntVar* const var) override {}
  void EndProcessingIntegerVariable(IntVar* const var) override {}
  void PushContext(const std::string& context) override {}
  void PopContext() override {}

  void BeginFail() override {
    if (active_demon_ != nullptr) {
      DemonRuns* const demon_run = demon_map_[active_demon_];
      if (demon_run != nullptr) {
        demon_run->add_end_time(CurrentTime());
        demon_run->set_failures(demon_run->failures() + 1);
      }
      active_demon_ = nullptr;
      // active_constraint_ can be non null in case of initial propagation.
      active_constraint_ = nullptr;
    } else if (active_constraint_ != nullptr) {
      ConstraintRuns* const ct_run = constraint_map_[active_constraint_];
      if (ct_run != nullptr) {
        ct_run->add_initial_propagation_end_time(CurrentTime());
        ct_run->set_failures(1);
      }
      active_constraint_ = nullptr;
    }
  }

  // Restarts a search and clears all previously collected information.
  void RestartSearch() override {
    STLDeleteContainerPairSecondPointers(constraint_map_.begin(),
                                         constraint_map_.end());
    constraint_map_.clear();
    demon_map_.clear();
    demons_per_constraint_.clear();
  }

  // IntExpr modifiers.
  void SetMin(IntExpr* const expr, int64 new_min) override {}
  void SetMax(IntExpr* const expr, int64 new_max) override {}
  void SetRange(IntExpr* const expr, int64 new_min, int64 new_max) override {}
  // IntVar modifiers.
  void SetMin(IntVar* const var, int64 new_min) override {}
  void SetMax(IntVar* const var, int64 new_max) override {}
  void SetRange(IntVar* const var, int64 new_min, int64 new_max) override {}
  void RemoveValue(IntVar* const var, int64 value) override {}
  void SetValue(IntVar* const var, int64 value) override {}
  void RemoveInterval(IntVar* const var, int64 imin, int64 imax) override {}
  void SetValues(IntVar* const var, const std::vector<int64>& values) override {
  }
  void RemoveValues(IntVar* const var,
                    const std::vector<int64>& values) override {}
  // IntervalVar modifiers.
  void SetStartMin(IntervalVar* const var, int64 new_min) override {}
  void SetStartMax(IntervalVar* const var, int64 new_max) override {}
  void SetStartRange(IntervalVar* const var, int64 new_min,
                     int64 new_max) override {}
  void SetEndMin(IntervalVar* const var, int64 new_min) override {}
  void SetEndMax(IntervalVar* const var, int64 new_max) override {}
  void SetEndRange(IntervalVar* const var, int64 new_min,
                   int64 new_max) override {}
  void SetDurationMin(IntervalVar* const var, int64 new_min) override {}
  void SetDurationMax(IntervalVar* const var, int64 new_max) override {}
  void SetDurationRange(IntervalVar* const var, int64 new_min,
                        int64 new_max) override {}
  void SetPerformed(IntervalVar* const var, bool value) override {}
  void RankFirst(SequenceVar* const var, int index) override {}
  void RankNotFirst(SequenceVar* const var, int index) override {}
  void RankLast(SequenceVar* const var, int index) override {}
  void RankNotLast(SequenceVar* const var, int index) override {}
  void RankSequence(SequenceVar* const var, const std::vector<int>& rank_first,
                    const std::vector<int>& rank_last,
                    const std::vector<int>& unperformed) override {}

  // Useful for unit tests.
  void AddFakeRun(Demon* const demon, int64 start_time, int64 end_time,
                  bool is_fail) {
    CHECK(demon != nullptr);
    DemonRuns* const demon_run = demon_map_[demon];
    CHECK(demon_run != nullptr);
    demon_run->add_start_time(start_time);
    demon_run->add_end_time(end_time);
    if (is_fail) {
      demon_run->set_failures(demon_run->failures() + 1);
    }
  }

  // Exports collected data as human-readable text.
  void PrintOverview(Solver* const solver, const std::string& filename) {
    const char* const kConstraintFormat =
        "  - Constraint: %s\n                failures=%" GG_LL_FORMAT
        "d, initial propagation runtime=%" GG_LL_FORMAT
        "d us, demons=%d, demon invocations=%" GG_LL_FORMAT
        "d, total demon runtime=%" GG_LL_FORMAT "d us\n";
    const char* const kDemonFormat =
        "  --- Demon: %s\n             invocations=%" GG_LL_FORMAT
        "d, failures=%" GG_LL_FORMAT "d, total runtime=%" GG_LL_FORMAT
        "d us, [average=%.2lf, median=%.2lf, stddev=%.2lf]\n";
    File* file;
    const std::string model =
        StringPrintf("Model %s:\n", solver->model_name().c_str());
    if (file::Open(filename, "w", &file, file::Defaults()).ok()) {
      file::WriteString(file, model, file::Defaults()).IgnoreError();
      std::vector<Container> to_sort;
      for (std::unordered_map<const Constraint*, ConstraintRuns*>::const_iterator it =
               constraint_map_.begin();
           it != constraint_map_.end(); ++it) {
        const Constraint* const ct = it->first;
        int64 fails = 0;
        int64 demon_invocations = 0;
        int64 initial_propagation_runtime = 0;
        int64 total_demon_runtime = 0;
        int demon_count = 0;
        ExportInformation(ct, &fails, &initial_propagation_runtime,
                          &demon_invocations, &total_demon_runtime,
                          &demon_count);
        to_sort.push_back(
            Container(ct, total_demon_runtime + initial_propagation_runtime));
      }
      std::sort(to_sort.begin(), to_sort.end());

      for (int i = 0; i < to_sort.size(); ++i) {
        const Constraint* const ct = to_sort[i].ct;
        int64 fails = 0;
        int64 demon_invocations = 0;
        int64 initial_propagation_runtime = 0;
        int64 total_demon_runtime = 0;
        int demon_count = 0;
        ExportInformation(ct, &fails, &initial_propagation_runtime,
                          &demon_invocations, &total_demon_runtime,
                          &demon_count);
        const std::string constraint_message =
            StringPrintf(kConstraintFormat, ct->DebugString().c_str(), fails,
                         initial_propagation_runtime, demon_count,
                         demon_invocations, total_demon_runtime);
        file::WriteString(file, constraint_message, file::Defaults())
            .IgnoreError();
        const std::vector<DemonRuns*>& demons = demons_per_constraint_[ct];
        const int demon_size = demons.size();
        for (int demon_index = 0; demon_index < demon_size; ++demon_index) {
          DemonRuns* const demon_runs = demons[demon_index];
          int64 invocations = 0;
          int64 fails = 0;
          int64 runtime = 0;
          double mean_runtime = 0;
          double median_runtime = 0;
          double standard_deviation = 0.0;
          ExportInformation(demon_runs, &invocations, &fails, &runtime,
                            &mean_runtime, &median_runtime,
                            &standard_deviation);
          const std::string runs = StringPrintf(
              kDemonFormat, demon_runs->demon_id().c_str(), invocations, fails,
              runtime, mean_runtime, median_runtime, standard_deviation);
          file::WriteString(file, runs, file::Defaults()).IgnoreError();
        }
      }
    }
    file->Close(file::Defaults()).IgnoreError();
  }

  // Export Information
  void ExportInformation(const Constraint* const constraint, int64* const fails,
                         int64* const initial_propagation_runtime,
                         int64* const demon_invocations,
                         int64* const total_demon_runtime, int* demons) {
    CHECK(constraint != nullptr);
    ConstraintRuns* const ct_run = constraint_map_[constraint];
    CHECK(ct_run != nullptr);
    *demon_invocations = 0;
    *fails = ct_run->failures();
    *initial_propagation_runtime = 0;
    for (int i = 0; i < ct_run->initial_propagation_start_time_size(); ++i) {
      *initial_propagation_runtime += ct_run->initial_propagation_end_time(i) -
                                      ct_run->initial_propagation_start_time(i);
    }
    *total_demon_runtime = 0;

    // Gather information.
    *demons = ct_run->demons_size();
    CHECK_EQ(*demons, demons_per_constraint_[constraint].size());
    for (int demon_index = 0; demon_index < *demons; ++demon_index) {
      const DemonRuns& demon_runs = ct_run->demons(demon_index);
      *fails += demon_runs.failures();
      CHECK_EQ(demon_runs.start_time_size(), demon_runs.end_time_size());
      const int runs = demon_runs.start_time_size();
      *demon_invocations += runs;
      for (int run_index = 0; run_index < runs; ++run_index) {
        const int64 demon_time =
            demon_runs.end_time(run_index) - demon_runs.start_time(run_index);
        *total_demon_runtime += demon_time;
      }
    }
  }

  void ExportInformation(const DemonRuns* const demon_runs,
                         int64* const demon_invocations, int64* const fails,
                         int64* const total_demon_runtime,
                         double* const mean_demon_runtime,
                         double* const median_demon_runtime,
                         double* const stddev_demon_runtime) {
    CHECK(demon_runs != nullptr);
    CHECK_EQ(demon_runs->start_time_size(), demon_runs->end_time_size());

    const int runs = demon_runs->start_time_size();
    *demon_invocations = runs;
    *fails = demon_runs->failures();
    *total_demon_runtime = 0;
    *mean_demon_runtime = 0.0;
    *median_demon_runtime = 0.0;
    *stddev_demon_runtime = 0.0;
    std::vector<double> runtimes;
    for (int run_index = 0; run_index < runs; ++run_index) {
      const int64 demon_time =
          demon_runs->end_time(run_index) - demon_runs->start_time(run_index);
      *total_demon_runtime += demon_time;
      runtimes.push_back(demon_time);
    }
    // Compute mean.
    if (!runtimes.empty()) {
      *mean_demon_runtime = (1.0L * *total_demon_runtime) / runtimes.size();

      // Compute median.
      std::sort(runtimes.begin(), runtimes.end());
      const int pivot = runtimes.size() / 2;

      if (runtimes.size() == 1) {
        *median_demon_runtime = runtimes[0];
      } else {
        *median_demon_runtime =
            runtimes.size() % 2 == 1
                ? runtimes[pivot]
                : (runtimes[pivot - 1] + runtimes[pivot]) / 2.0;
      }

      // Compute standard deviation.
      double total_deviation = 0.0f;

      for (int i = 0; i < runtimes.size(); ++i) {
        total_deviation += pow(runtimes[i] - *mean_demon_runtime, 2);
      }

      *stddev_demon_runtime = sqrt(total_deviation / runtimes.size());
    }
  }

  // The demon_profiler is added by default on the main propagation
  // monitor.  It just needs to be added to the search monitors at the
  // start of the search.
  void Install() override { SearchMonitor::Install(); }

  std::string DebugString() const override { return "DemonProfiler"; }

 private:
  Constraint* active_constraint_;
  Demon* active_demon_;
  const int64 start_time_ns_;
  std::unordered_map<const Constraint*, ConstraintRuns*> constraint_map_;
  std::unordered_map<const Demon*, DemonRuns*> demon_map_;
  std::unordered_map<const Constraint*, std::vector<DemonRuns*> > demons_per_constraint_;
};

void Solver::ExportProfilingOverview(const std::string& filename) {
  if (demon_profiler_ != nullptr) {
    demon_profiler_->PrintOverview(this, filename);
  }
}

// ----- Exported Functions -----

void InstallDemonProfiler(DemonProfiler* const monitor) { monitor->Install(); }

DemonProfiler* BuildDemonProfiler(Solver* const solver) {
  if (solver->IsProfilingEnabled()) {
    return new DemonProfiler(solver);
  } else {
    return nullptr;
  }
}

void DeleteDemonProfiler(DemonProfiler* const monitor) { delete monitor; }

Demon* Solver::RegisterDemon(Demon* const demon) {
  CHECK(demon != nullptr);
  if (InstrumentsDemons()) {
    propagation_monitor_->RegisterDemon(demon);
  }
  return demon;
}

// ----- Exported Methods for Unit Tests -----

void RegisterDemon(Solver* const solver, Demon* const demon,
                   DemonProfiler* const monitor) {
  monitor->RegisterDemon(demon);
}

void DemonProfilerAddFakeRun(DemonProfiler* const monitor, Demon* const demon,
                             int64 start_time, int64 end_time, bool is_fail) {
  monitor->AddFakeRun(demon, start_time, end_time, is_fail);
}

void DemonProfilerExportInformation(DemonProfiler* const monitor,
                                    const Constraint* const constraint,
                                    int64* const fails,
                                    int64* const initial_propagation_runtime,
                                    int64* const demon_invocations,
                                    int64* const total_demon_runtime,
                                    int* const demon_count) {
  monitor->ExportInformation(constraint, fails, initial_propagation_runtime,
                             demon_invocations, total_demon_runtime,
                             demon_count);
}

void DemonProfilerBeginInitialPropagation(DemonProfiler* const monitor,
                                          Constraint* const constraint) {
  monitor->BeginConstraintInitialPropagation(constraint);
}

void DemonProfilerEndInitialPropagation(DemonProfiler* const monitor,
                                        Constraint* const constraint) {
  monitor->EndConstraintInitialPropagation(constraint);
}

}  // namespace operations_research
