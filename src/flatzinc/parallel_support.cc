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

#include <iostream>  // NOLINT
#include <string>
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/mutex.h"
#include "base/stringprintf.h"
#include "base/synchronization.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"
#include "flatzinc/model.h"
#include "flatzinc/search.h"

DECLARE_bool(fz_logging);

namespace operations_research {
namespace {
class MtOptimizeVar : public OptimizeVar {
 public:
  MtOptimizeVar(Solver* s, bool maximize, IntVar* v, int64 step,
                FzParallelSupportInterface* support, int worker_id)
      : OptimizeVar(s, maximize, v, step),
        support_(support),
        worker_id_(worker_id) {}

  virtual ~MtOptimizeVar() {}

  virtual void RefuteDecision(Decision* d) {
    const int64 polled_best = support_->BestSolution();
    if ((maximize_ && polled_best > best_) ||
        (!maximize_ && polled_best < best_)) {
      support_->Log(
          worker_id_,
          StringPrintf("Polling improved objective %" GG_LL_FORMAT "d",
                       polled_best));
      best_ = polled_best;
    }
    OptimizeVar::RefuteDecision(d);
  }

 private:
  FzParallelSupportInterface* const support_;
  const int worker_id_;
};

class MtCustomLimit : public SearchLimit {
 public:
  MtCustomLimit(Solver* s, FzParallelSupportInterface* support, int worker_id)
      : SearchLimit(s), support_(support), worker_id_(worker_id) {}

  virtual ~MtCustomLimit() {}

  virtual void Init() {}

  virtual bool Check() {
    const bool result = support_->ShouldFinish();
    if (result) {
      support_->Log(worker_id_, "terminating");
    }
    return result;
  }

  virtual void Copy(const SearchLimit* limit) {}

  virtual SearchLimit* MakeClone() const { return nullptr; }

 private:
  FzParallelSupportInterface* const support_;
  const int worker_id_;
};

class MtSupportInterface : public FzParallelSupportInterface {
 public:
  MtSupportInterface(bool print_all, int num_solutions, bool verbose)
      : print_all_(print_all),
        num_solutions_(num_solutions),
        verbose_(verbose),
        type_(UNDEF),
        last_worker_(-1),
        best_solution_(0),
        should_finish_(false),
        interrupted_(false) {}

  virtual ~MtSupportInterface() {}

  virtual void Init(int worker_id, const std::string& init_string) {
    MutexLock lock(&mutex_);
    if (worker_id == 0) {
      std::cout << init_string << std::endl;
    }
    LogNoLock(worker_id, "starting");
  }

  virtual void StartSearch(int worker_id, Type type) {
    MutexLock lock(&mutex_);
    if (type_ == UNDEF) {
      type_ = type;
      if (type == MAXIMIZE) {
        best_solution_ = kint64min;
      } else if (type_ == MINIMIZE) {
        best_solution_ = kint64max;
      }
    }
  }

  virtual void SatSolution(int worker_id, const std::string& solution_string) {
    MutexLock lock(&mutex_);
    if (NumSolutions() < num_solutions_ || print_all_) {
      LogNoLock(worker_id, "solution found");
      std::cout << solution_string << std::endl;
      should_finish_ = true;
    }
    IncrementSolutions();
  }

  virtual void OptimizeSolution(int worker_id, int64 value,
                                const std::string& solution_string) {
    MutexLock lock(&mutex_);
    if (!should_finish_) {
      switch (type_) {
        case MINIMIZE: {
          if (value < best_solution_) {
            best_solution_ = value;
            IncrementSolutions();
            LogNoLock(
                worker_id,
                StringPrintf("solution found with value %" GG_LL_FORMAT "d",
                             value));
            if (print_all_ || num_solutions_ > 1) {
              std::cout << solution_string << std::endl;
            } else {
              last_solution_ = solution_string + "\n";
              last_worker_ = worker_id;
            }
          }
          break;
        }
        case MAXIMIZE: {
          if (value > best_solution_) {
            best_solution_ = value;
            IncrementSolutions();
            LogNoLock(
                worker_id,
                StringPrintf("solution found with value %" GG_LL_FORMAT "d",
                             value));
            if (print_all_ || num_solutions_ > 1) {
              std::cout << solution_string << std::endl;
            } else {
              last_solution_ = solution_string + "\n";
              last_worker_ = worker_id;
            }
          }
          break;
        }
        default:
          LOG(ERROR) << "Should not be here";
      }
    }
  }

  virtual void FinalOutput(int worker_id, const std::string& final_output) {
    MutexLock lock(&mutex_);
    std::cout << final_output << std::endl;
  }

  virtual bool ShouldFinish() const { return should_finish_; }

  virtual void EndSearch(int worker_id, bool interrupted) {
    MutexLock lock(&mutex_);
    LogNoLock(worker_id, "exiting");
    if (!last_solution_.empty()) {
      LogNoLock(last_worker_,
                StringPrintf("solution found with value %" GG_LL_FORMAT "d",
                             best_solution_));
      std::cout << last_solution_;
      last_solution_.clear();
    }
    should_finish_ = true;
    if (interrupted) {
      interrupted_ = true;
    }
  }

  virtual int64 BestSolution() const { return best_solution_; }

  virtual OptimizeVar* Objective(Solver* s, bool maximize, IntVar* var,
                                 int64 step, int w) {
    return s->RevAlloc(new MtOptimizeVar(s, maximize, var, step, this, w));
  }

  virtual SearchLimit* Limit(Solver* s, int worker_id) {
    return s->RevAlloc(new MtCustomLimit(s, this, worker_id));
  }

  virtual void Log(int worker_id, const std::string& message) {
    if (verbose_) {
      MutexLock lock(&mutex_);
      std::cout << "%%  worker " << worker_id << ": " << message << std::endl;
    }
  }

  virtual bool Interrupted() const { return interrupted_; }

  void LogNoLock(int worker_id, const std::string& message) {
    if (verbose_) {
      std::cout << "%%  worker " << worker_id << ": " << message << std::endl;
    }
  }

 private:
  const bool print_all_;
  const int num_solutions_;
  const bool verbose_;
  Mutex mutex_;
  Type type_;
  std::string last_solution_;
  int last_worker_;
  int64 best_solution_;
  bool should_finish_;
  bool interrupted_;
};
}  // namespace

FzParallelSupportInterface* MakeMtSupport(bool print_all, int num_solutions,
                                          bool verbose) {
  return new MtSupportInterface(print_all, num_solutions, verbose);
}
}  // namespace operations_research
