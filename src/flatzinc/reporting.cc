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

#include "flatzinc/reporting.h"

#include <iostream>  // NOLINT

#include <string>
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"
#include "flatzinc/logging.h"

namespace operations_research {
namespace fz {
MonoThreadReporting::MonoThreadReporting(bool print_all, int max_num_solutions)
    : SearchReportingInterface(print_all, max_num_solutions),
      best_objective_(0),
      interrupted_(false) {}

MonoThreadReporting::~MonoThreadReporting() {}

void MonoThreadReporting::Init(int thread_id, const std::string& init_string) {
  FZLOG << init_string << FZENDL;
}

void MonoThreadReporting::OnSearchStart(int thread_id, Type type) {
  if (type == MAXIMIZE) {
    best_objective_ = kint64min;
  } else if (type == MINIMIZE) {
    best_objective_ = kint64max;
  }
}

void MonoThreadReporting::OnSatSolution(int thread_id,
                                        const std::string& solution_string) {
  if (NumSolutions() < MaxNumSolutions() || ShouldPrintAllSolutions()) {
    Print(thread_id, solution_string);
  }
  IncrementSolutions();
}

void MonoThreadReporting::OnOptimizeSolution(int thread_id, int64 value,
                                             const std::string& solution_string) {
  best_objective_ = value;
  if (ShouldPrintAllSolutions() || MaxNumSolutions() > 1) {
    Print(thread_id, solution_string);
  } else {
    last_solution_ = solution_string;
  }
  IncrementSolutions();
}

void MonoThreadReporting::Log(int thread_id, const std::string& final_output) const {
  FZLOG << final_output << FZENDL;
}

void MonoThreadReporting::Print(int thread_id,
                                const std::string& final_output) const {
  std::cout << final_output << std::endl;
}

bool MonoThreadReporting::ShouldFinish() const { return false; }

void MonoThreadReporting::OnSearchEnd(int thread_id, bool interrupted) {
  if (!last_solution_.empty()) {
    Print(thread_id, last_solution_);
  }
  interrupted_ = interrupted;
}

int64 MonoThreadReporting::BestSolution() const { return best_objective_; }

OptimizeVar* MonoThreadReporting::CreateObjective(Solver* s, bool maximize,
                                                  IntVar* var, int64 step,
                                                  int thread_id) const {
  return s->MakeOptimize(maximize, var, step);
}

SearchLimit* MonoThreadReporting::CreateLimit(Solver* s, int thread_id) const {
  return nullptr;
}

bool MonoThreadReporting::Interrupted() const { return interrupted_; }

// Silent subclass.
SilentMonoThreadReporting::SilentMonoThreadReporting(
    bool print_all, int max_num_solutions)
    : MonoThreadReporting(print_all, max_num_solutions) {}

SilentMonoThreadReporting::~SilentMonoThreadReporting() {}

void SilentMonoThreadReporting::Init(int thread_id,
                                     const std::string& init_string) {}

void SilentMonoThreadReporting::Log(int thread_id,
                                    const std::string& final_output) const {}

void SilentMonoThreadReporting::Print(int thread_id,
                                      const std::string& final_output) const {}

// ----- Helper classes for MultiThreadReporting -----

class MtOptimizeVar : public OptimizeVar {
 public:
  MtOptimizeVar(Solver* s, bool maximize, IntVar* v, int64 step,
                const MultiThreadReporting* report, int thread_id, bool verbose)
      : OptimizeVar(s, maximize, v, step),
        report_(report),
        thread_id_(thread_id),
        verbose_(verbose) {}

  ~MtOptimizeVar() override {}

  void RefuteDecision(Decision* d) override {
    const int64 polled_best = report_->BestSolution();
    if ((maximize_ && polled_best > best_) ||
        (!maximize_ && polled_best < best_)) {
      if (verbose_) {
        report_->Log(
            thread_id_,
            StringPrintf("Polling improved objective %" GG_LL_FORMAT "d",
                         polled_best));
      }
      best_ = polled_best;
    }
    OptimizeVar::RefuteDecision(d);
  }

 private:
  const MultiThreadReporting* const report_;
  const int thread_id_;
  const bool verbose_;
};

class MtCustomLimit : public SearchLimit {
 public:
  MtCustomLimit(Solver* s, const MultiThreadReporting* report, int thread_id,
                const bool verbose)
      : SearchLimit(s),
        report_(report),
        thread_id_(thread_id),
        verbose_(verbose) {}

  ~MtCustomLimit() override {}

  void Init() override {}

  bool Check() override {
    const bool result = report_->ShouldFinish();
    if (result && verbose_) {
      report_->Log(thread_id_, "terminating");
    }
    return result;
  }

  void Copy(const SearchLimit* limit) override {}

  // Not used in this context.
  SearchLimit* MakeClone() const override { return nullptr; }

 private:
  const MultiThreadReporting* const report_;
  const int thread_id_;
  const bool verbose_;
};

MultiThreadReporting::MultiThreadReporting(bool print_all, int num_solutions,
                                           bool verbose)
    : SearchReportingInterface(print_all, num_solutions),
      verbose_(verbose),
      type_(UNDEF),
      last_thread_(-1),
      best_objective_(0),
      should_finish_(false),
      interrupted_(false) {}

MultiThreadReporting::~MultiThreadReporting() {}

void MultiThreadReporting::Init(int thread_id, const std::string& init_string) {
  MutexLock lock(&mutex_);
  if (thread_id == 0) {
    FZLOG << init_string << FZENDL;
  }
  if (verbose_) {
    LogNoLock(thread_id, "starting");
  }
}

void MultiThreadReporting::OnSearchStart(int thread_id, Type type) {
  MutexLock lock(&mutex_);
  if (type_ == UNDEF) {
    type_ = type;
    if (type_ == MAXIMIZE) {
      best_objective_ = kint64min;
    } else if (type_ == MINIMIZE) {
      best_objective_ = kint64max;
    }
  }
}

void MultiThreadReporting::OnSatSolution(int thread_id,
                                         const std::string& solution_string) {
  MutexLock lock(&mutex_);
  if (NumSolutions() < MaxNumSolutions() || ShouldPrintAllSolutions()) {
    if (verbose_) {
      LogNoLock(thread_id, "solution found");
    }
    Print(thread_id, solution_string);
    should_finish_ = true;
  }
  IncrementSolutions();
}

void MultiThreadReporting::OnOptimizeSolution(int thread_id, int64 value,
                                              const std::string& solution_string) {
  MutexLock lock(&mutex_);
  if (!should_finish_) {
    switch (type_) {
      case MINIMIZE: {
        if (value < best_objective_) {
          best_objective_ = value;
          IncrementSolutions();
          if (verbose_) {
            LogNoLock(
                thread_id,
                StringPrintf("solution found with value %" GG_LL_FORMAT "d",
                             value));
          }
          if (ShouldPrintAllSolutions() || MaxNumSolutions() > 1) {
            Print(thread_id, solution_string);
          } else {
            last_solution_ = solution_string;
            last_thread_ = thread_id;
          }
        }
        break;
      }
      case MAXIMIZE: {
        if (value > best_objective_) {
          best_objective_ = value;
          IncrementSolutions();
          if (verbose_) {
            LogNoLock(
                thread_id,
                StringPrintf("solution found with value %" GG_LL_FORMAT "d",
                             value));
          }
          if (ShouldPrintAllSolutions() || MaxNumSolutions() > 1) {
            Print(thread_id, solution_string);
          } else {
            last_solution_ = solution_string;
            last_thread_ = thread_id;
          }
        }
        break;
      }
      default:
        LOG(ERROR) << "Should not be here";
    }
  }
}

void MultiThreadReporting::Log(int thread_id, const std::string& message) const {
  MutexLock lock(&mutex_);
  FZLOG << message << FZENDL;
}

void MultiThreadReporting::Print(int thread_id, const std::string& message) const {
  std::cout << message << std::endl;
}
bool MultiThreadReporting::ShouldFinish() const {
  MutexLock lock(&mutex_);
  return should_finish_;
}

void MultiThreadReporting::OnSearchEnd(int thread_id, bool interrupted) {
  MutexLock lock(&mutex_);
  if (verbose_) {
    LogNoLock(thread_id, "exiting");
  }
  if (!last_solution_.empty()) {
    if (verbose_) {
      LogNoLock(last_thread_,
                StringPrintf("solution found with value %" GG_LL_FORMAT "d",
                             best_objective_));
    }
    Print(thread_id, last_solution_);
    last_solution_.clear();
  }
  should_finish_ = true;
  if (interrupted) {
    interrupted_ = true;
  }
}

int64 MultiThreadReporting::BestSolution() const {
  MutexLock lock(&mutex_);
  return best_objective_;
}

OptimizeVar* MultiThreadReporting::CreateObjective(Solver* s, bool maximize,
                                                   IntVar* var, int64 step,
                                                   int thread_id) const {
  return s->RevAlloc(
      new MtOptimizeVar(s, maximize, var, step, this, thread_id, verbose_));
}

SearchLimit* MultiThreadReporting::CreateLimit(Solver* s, int thread_id) const {
  return s->RevAlloc(new MtCustomLimit(s, this, thread_id, verbose_));
}

bool MultiThreadReporting::Interrupted() const {
  MutexLock lock(&mutex_);
  return interrupted_;
}

void MultiThreadReporting::LogNoLock(int thread_id, const std::string& message) {
  FZLOG << "%%  thread " << thread_id << ": " << message << FZENDL;
}

}  // namespace fz
}  // namespace operations_research
