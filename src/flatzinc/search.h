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

#ifndef OR_TOOLS_FLATZINC_SEARCH_H_
#define OR_TOOLS_FLATZINC_SEARCH_H_

#include "constraint_solver/constraint_solver.h"
#include "flatzinc/model.h"

namespace operations_research {
struct FzSolverParameters {
  enum SearchType {
    DEFAULT,
    IBS,
    FIRST_UNBOUND,
    MIN_SIZE,
    RANDOM_MIN,
    RANDOM_MAX,
  };

  FzSolverParameters();

  bool all_solutions;
  bool free_search;
  bool last_conflict;
  bool ignore_annotations;
  bool ignore_unknown;
  bool use_log;
  bool verbose_impact;
  double restart_log_size;
  bool run_all_heuristics;
  int heuristic_period;
  int log_period;
  int luby_restart;
  int num_solutions;
  int random_seed;
  int threads;
  int worker_id;
  int64 time_limit_in_ms;
  SearchType search_type;
  bool store_all_solutions;
};

// This class is used to abstract the interface to parallelism from
// the search code. It offers two sets of API:
//    - Create specific search objects (Objective(), Limit(), Log()).
//    - Report solution (SatSolution(), OptimizeSolution(), FinalOutput(),
//                       EndSearch(), BestSolution(), Interrupted()).
class FzParallelSupportInterface {
 public:
  enum Type {
    UNDEF,
    SATISFY,
    MINIMIZE,
    MAXIMIZE,
  };

  FzParallelSupportInterface() : num_solutions_(0) {}
  virtual ~FzParallelSupportInterface() {}
  // Initialize the interface for a given worker id.
  // In sequential mode, the worker id is always -1.
  // In parallel mode, it ranges from 0 to num_workers - 1.
  virtual void Init(int worker_id, const std::string& init_string) = 0;
  // Callback on the start search event.
  virtual void StartSearch(int worker_id, Type type) = 0;
  // Worker 'worker_id' notifies a new solution in a satisfaction
  // problem. 'solution_string' is the solution to display if needed.
  virtual void SatSolution(int worker_id, const std::string& solution_string) = 0;
  // Worker 'worker_id' notifies a new solution in an optimization
  // problem. 'solution_string' is the solution to display if needed.
  virtual void OptimizeSolution(int worker_id, int64 value,
                                const std::string& solution_string) = 0;
  // Worker 'worker_id' sends its final output (mostly search
  // statistics) to display.
  virtual void FinalOutput(int worker_id, const std::string& final_output) = 0;
  // Checks if we should finish the search right away, for instance,
  // in a satisfaction problem if a solution has already be found.
  virtual bool ShouldFinish() const = 0;
  // Callback on the end search event.
  virtual void EndSearch(int worker_id, bool interrupted) = 0;
  // Returns the value of the best solution found during search.
  virtual int64 BestSolution() const = 0;
#if !defined(SWIG)
  // Creates a dedicated search monitor for the objective.
  virtual OptimizeVar* Objective(Solver* s, bool maximize, IntVar* var,
                                 int64 step, int worker_id) = 0;
  // Creates a dedicated search limit.
  virtual SearchLimit* Limit(Solver* s, int worker_id) = 0;
  // Creates a dedicated search log.
  virtual void Log(int worker_id, const std::string& message) = 0;
#endif
  // Returns if the search was interrupted, usually by a time or
  // solution limit.
  virtual bool Interrupted() const = 0;

  // Increments the number of solutions found.
  void IncrementSolutions() { num_solutions_++; }
  // Returns the number of solutions found.
  int NumSolutions() const { return num_solutions_; }

 private:
  int num_solutions_;
};

// Create an interface suitable for a sequential search.
FzParallelSupportInterface* MakeSequentialSupport(bool print_all,
                                                  int num_solutions);
// Creates an interface suitable for a multi-threaded search.
FzParallelSupportInterface* MakeMtSupport(bool print_all, int num_solutions,
                                          bool verbose);
}  // namespace operations_research

#endif  // OR_TOOLS_FLATZINC_SEARCH_H_
