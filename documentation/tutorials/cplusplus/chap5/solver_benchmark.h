// Copyright 2011-2014 Google
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
//
//
//  Benchmark class to test the CP Solver.
//  We use the internal wall time from the CP Solver.
//  We take into account the creation of the model and the initialization.

#ifndef _OR_TOOLS_EXAMPLES_CPLUSPLUS_EXAMPLES_CHAP5_SOLVER_BENCHMARK_H
#define _OR_TOOLS_EXAMPLES_CPLUSPLUS_EXAMPLES_CHAP5_SOLVER_BENCHMARK_H

#include <vector>
#include <string>
#include <ostream>
#include <fstream>

#include "base/macros.h"
#include "base/commandlineflags.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "constraint_solver/constraint_solver.h"

namespace operations_research {

// Default Assignment operator and copy constructor are OK.
struct SolverBenchmarkStats {
  SolverBenchmarkStats():
    description_("None"),
    wall_time_(kint64max),
    branches_(kint64max),
    solutions_(0LL),
    failures_(kint64max),
    neighbors_(0LL),
    filtered_neighbors_(0LL),
    accepted_neighbors_(0LL),
    stamp_(kuint64max),
    fail_stamp_(kuint64max),
    solution_process_ok_(false) {
      // kNumPriorities gives the number of Demon priorities
      demon_runs_ = std::vector<int64>(Solver::kNumPriorities, 0LL);
    }

  SolverBenchmarkStats(Solver * s, std::string description):
    description_(description),
    wall_time_(s->wall_time()),
    branches_(s->branches()),
    solutions_(s->solutions()),
    failures_(s->failures()),
    neighbors_(s->neighbors()),
    filtered_neighbors_(s->filtered_neighbors()),
    accepted_neighbors_(s->accepted_neighbors()),
    stamp_(s->stamp()),
    fail_stamp_(s->fail_stamp()),
    solution_process_ok_(false) {
    // kNumPriorities gives the number of Demon priorities
      demon_runs_ = std::vector<int64>(Solver::kNumPriorities, 0LL);
      demon_runs_[Solver::DELAYED_PRIORITY] =
                            s->demon_runs(Solver::DELAYED_PRIORITY);
      demon_runs_[Solver::VAR_PRIORITY] =
                            s->demon_runs(Solver::VAR_PRIORITY);
      demon_runs_[Solver::NORMAL_PRIORITY] =
                            s->demon_runs(Solver::NORMAL_PRIORITY);
  }

  void Update(Solver * s, std::string description, bool solution_process_ok);
  void Reset();

  std::string description_;
  int64   wall_time_;
  int64   branches_;
  int64   solutions_;
  std::vector<int64> demon_runs_;
  int64   failures_;
  int64   neighbors_;
  int64   filtered_neighbors_;
  int64   accepted_neighbors_;
  uint64  stamp_;
  uint64  fail_stamp_;
  bool solution_process_ok_;
};

void SolverBenchmarkStats::Update(Solver* s,
                                  std::string description,
                                  bool solution_process_ok) {
  description_ = description;
  wall_time_   = s->wall_time();
  branches_    = s->branches();
  solutions_   = s->solutions();
  failures_    = s->failures();
  neighbors_   = s->neighbors();
  filtered_neighbors_ = s->filtered_neighbors();
  accepted_neighbors_ = s->accepted_neighbors();
  stamp_              = s->stamp();
  fail_stamp_         = s->fail_stamp();
  solution_process_ok_ = solution_process_ok;

  // kNumPriorities gives the number of Demon priorities
  demon_runs_[Solver::DELAYED_PRIORITY] =
                                 s->demon_runs(Solver::DELAYED_PRIORITY);
  demon_runs_[Solver::VAR_PRIORITY] = s->demon_runs(Solver::VAR_PRIORITY);
  demon_runs_[Solver::NORMAL_PRIORITY] = s->demon_runs(Solver::NORMAL_PRIORITY);
}


void SolverBenchmarkStats::Reset() {
  description_ = "None";
  wall_time_ = kint64max;
  branches_ = kint64max;
  solutions_ = 0LL;
  for (int i = 0; i < Solver::kNumPriorities; ++i) {
    demon_runs_[i] = 0LL;
  }
  failures_ = kint64max;
  neighbors_ = 0LL;
  filtered_neighbors_ = 0LL;
  accepted_neighbors_ = 0LL;
  stamp_ = kuint64max;
  fail_stamp_ = kuint64max;
  solution_process_ok_ = false;
}

std::ostream& operator<< (std::ostream& stream,
                          const SolverBenchmarkStats stats) {
  stream << "Algo description: " << stats.description_ << std::endl;
  stream << "Wall time: " << stats.wall_time_/1000.0 << std::endl;
  stream << "Branches: " << stats.branches_ << std::endl;
  stream << "Solutions: " << stats.solutions_ << std::endl;
  stream << "Demon runs:" << std::endl;
  stream << "  DELAYED_PRIORITY: " <<
                       stats.demon_runs_[Solver::DELAYED_PRIORITY] << std::endl;
  stream << "  VAR_PRIORITY: " <<
                       stats.demon_runs_[Solver::VAR_PRIORITY] << std::endl;
  stream << "  NORMAL_PRIORITY: " <<
                       stats.demon_runs_[Solver::NORMAL_PRIORITY] << std::endl;
  stream << "Failures: " << stats.failures_ << std::endl;
  stream << "Neighbors: " << stats.neighbors_ << std::endl;
  stream << "Filtered neighbors: " << stats.filtered_neighbors_ << std::endl;
  stream << "Accepted neighbors: " << stats.accepted_neighbors_ << std::endl;
  stream << "Stamp: " << stats.stamp_ << std::endl;
  stream << "Fail stamp: " << stats.fail_stamp_ << std::endl;

  return stream;
}


class SolverBenchmark {
  public:
    SolverBenchmark(): run_number_(0LL) {}
    virtual ~SolverBenchmark() {}
    void Reset();
    virtual bool Test() = 0;
    bool Run(Solver * s,
             DecisionBuilder * db,
             std::vector<SearchMonitor *>& monitors,
             std::string description,
             SolverBenchmarkStats & stats);
    void UpdateBestStats(const SolverBenchmarkStats & stats);
    void Report(std::ostream & out);
    void Report(std::string filename);
    void ReportFailedRuns(std::ostream & out);
    void ReportSuccessfulRuns(std::ostream & out);

  private:
    SolverBenchmarkStats best_wall_time_;
    SolverBenchmarkStats best_branches_;
    SolverBenchmarkStats best_failures_;
    SolverBenchmarkStats best_stamp_;
    SolverBenchmarkStats best_fail_stamp_;

    int64 run_number_;
    std::vector<std::string> successful_runs_;
    std::vector<std::string> unsuccessful_runs_;

    DISALLOW_COPY_AND_ASSIGN(SolverBenchmark);
};

void SolverBenchmark::Reset() {
  best_wall_time_.Reset();
  best_branches_.Reset();
  best_failures_.Reset();
  best_stamp_.Reset();
  best_fail_stamp_.Reset();

  run_number_ = 0LL;
  successful_runs_.clear();
  unsuccessful_runs_.clear();
}

bool SolverBenchmark::Run(Solver* s,
                          DecisionBuilder* db,
                          std::vector< SearchMonitor* >& monitors,
                          std::string description,
                          SolverBenchmarkStats & stats) {
  bool solution_process_ok = s->Solve(db, monitors);
  stats.Update(s, description, solution_process_ok);
  UpdateBestStats(stats);
  if (solution_process_ok) {
    successful_runs_.push_back(description);
  } else {
    unsuccessful_runs_.push_back(description);
  }
  ++run_number_;

  return solution_process_ok;
}

void SolverBenchmark::UpdateBestStats(const SolverBenchmarkStats & stats) {
  if (stats.solution_process_ok_) {
    if (stats.wall_time_ < best_wall_time_.wall_time_) {
      best_wall_time_ = stats;
    }
    if (stats.branches_ < best_branches_.branches_) {
      best_branches_ = stats;
    }
    if (stats.failures_ < best_failures_.failures_) {
      best_failures_ = stats;
    }
    if (stats.stamp_ < best_stamp_.stamp_) {
      best_stamp_ = stats;
    }
    if (stats.fail_stamp_ < best_fail_stamp_.fail_stamp_) {
      best_fail_stamp_ = stats;
    }
  }
}


void SolverBenchmark::Report(std::ostream& out) {
  out << "CP solver benchmark report: " << std::endl;
  out << "===========================" << std::endl;
  out << "Number of runs: " << run_number_
      << " (successes: " << successful_runs_.size()
      << ", fails: " << unsuccessful_runs_.size() << ")" << std::endl;
  out << "---------------------------" << std::endl;
  out << "Best wall time:" << std::endl;
  out << best_wall_time_ << std::endl;
  out << "----" << std::endl;
  out << "Best branches:" << std::endl;
  out << best_branches_ << std::endl;
  out << "----" << std::endl;
  out << "Best failures:" << std::endl;
  out << best_failures_ << std::endl;
  out << "----" << std::endl;
  out << "Best stamps:" << std::endl;
  out << best_stamp_ << std::endl;
  out << "----" << std::endl;
  out << "Best fail stamps:" << std::endl;
  out << best_fail_stamp_ << std::endl;
  out << "----" << std::endl;
}

void SolverBenchmark::Report(std::string filename) {
  std::ofstream f_stream(filename.c_str());
  Report(f_stream);
  f_stream.close();
}

void SolverBenchmark::ReportFailedRuns(std::ostream & out) {
  out << "Failed runs:" << std::endl;
  out << "============" << std::endl;
  for (int i = 0; i < unsuccessful_runs_.size(); ++i) {
    out << unsuccessful_runs_[i] << std::endl;
  }
}

void SolverBenchmark::ReportSuccessfulRuns(std::ostream & out) {
  out << "Successful runs:" << std::endl;
  out << "================" << std::endl;
  for (int i = 0; i < successful_runs_.size(); ++i) {
    out << successful_runs_[i] << std::endl;
  }
}

}  // namespace operations_research

#endif  // _OR_TOOLS_EXAMPLES_CPLUSPLUS_EXAMPLES_CHAP5_SOLVER_BENCHMARK_H
