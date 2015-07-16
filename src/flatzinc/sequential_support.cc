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
#include "base/stringprintf.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"
#include "flatzinc/model.h"
#include "flatzinc/search.h"

DECLARE_bool(fz_logging);

namespace operations_research {
namespace {
class SequentialSupportInterface : public FzParallelSupportInterface {
 public:
  SequentialSupportInterface(bool print_all, int num_solutions)
      : print_all_(print_all),
        num_solutions_(num_solutions),
        type_(UNDEF),
        best_solution_(0),
        interrupted_(false) {}

  virtual ~SequentialSupportInterface() {}

  virtual void Init(int worker_id, const std::string& init_string) {
    std::cout << init_string << std::endl;
  }

  virtual void StartSearch(int worker_id, Type type) {
    type_ = type;
    if (type == MAXIMIZE) {
      best_solution_ = kint64min;
    } else if (type_ == MINIMIZE) {
      best_solution_ = kint64max;
    }
  }

  virtual void SatSolution(int worker_id, const std::string& solution_string) {
    if (NumSolutions() < num_solutions_ || print_all_) {
      std::cout << solution_string << std::endl;
    }
    IncrementSolutions();
  }

  virtual void OptimizeSolution(int worker_id, int64 value,
                                const std::string& solution_string) {
    best_solution_ = value;
    if (print_all_ || num_solutions_ > 1) {
      std::cout << solution_string << std::endl;
    } else {
      last_solution_ = solution_string + "\n";
    }
    IncrementSolutions();
  }

  virtual void FinalOutput(int worker_id, const std::string& final_output) {
    std::cout << final_output << std::endl;
  }

  virtual bool ShouldFinish() const { return false; }

  virtual void EndSearch(int worker_id, bool interrupted) {
    if (!last_solution_.empty()) {
      std::cout << last_solution_;
    }
    interrupted_ = interrupted;
  }

  virtual int64 BestSolution() const { return best_solution_; }

  virtual OptimizeVar* Objective(Solver* s, bool maximize, IntVar* var,
                                 int64 step, int worker_id) {
    return s->MakeOptimize(maximize, var, step);
  }

  virtual SearchLimit* Limit(Solver* s, int worker_id) { return nullptr; }

  virtual void Log(int worker_id, const std::string& message) {
    std::cout << "%%  worker " << worker_id << ": " << message << std::endl;
  }

  virtual bool Interrupted() const { return interrupted_; }

 private:
  const bool print_all_;
  const int num_solutions_;
  Type type_;
  std::string last_solution_;
  int64 best_solution_;
  bool interrupted_;
};
}  // namespace

FzParallelSupportInterface* MakeSequentialSupport(bool print_all,
                                                  int num_solutions) {
  return new SequentialSupportInterface(print_all, num_solutions);
}
}  // namespace operations_research
