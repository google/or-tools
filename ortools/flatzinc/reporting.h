// Copyright 2010-2017 Google
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

#ifndef OR_TOOLS_FLATZINC_REPORTING_H_
#define OR_TOOLS_FLATZINC_REPORTING_H_

#include <string>
#include "ortools/base/integral_types.h"
#include "ortools/base/mutex.h"
#include "ortools/base/stringprintf.h"
#include "ortools/constraint_solver/constraint_solver.h"

namespace operations_research {
namespace fz {
// This class is used to abstract the interface to parallelism from
// the search code. It offers two sets of API:
//    - Create specific search objects (Objective(), Limit()).
//    - Report solution (SatSolution(), OptimizeSolution(), FinalOutput(),
//                       EndSearch(), BestSolution(), Interrupted(),
//                       Log(), Print()).
//
// There will be only one SearchReporting class shared among all the solver
// threads.
class SearchReportingInterface {
 public:
  enum Type {
    UNDEF,
    SATISFY,
    MINIMIZE,
    MAXIMIZE,
  };

  SearchReportingInterface(bool print_all, int max_num_solutions)
      : print_all_solutions_(print_all),
        num_solutions_(0),
        max_num_solutions_(max_num_solutions) {}

  virtual ~SearchReportingInterface() {}

  // ----- Events on the search -----

  // Initialize the interface for a given thread id.
  // In sequential mode, the thread id is always -1.
  // In parallel mode, it ranges from 0 to num_threads - 1.
  virtual void Init(int thread_id, const std::string& init_string) = 0;

  // Callback on the start search event.
  virtual void OnSearchStart(int thread_id, Type type) = 0;

  // Worker 'thread_id' notifies a new solution in a satisfaction
  // problem. 'solution_string' is the solution to display if needed.
  virtual void OnSatSolution(int thread_id, const std::string& solution_string) = 0;

  // Worker 'thread_id' notifies a new solution in an optimization
  // problem. 'solution_string' is the solution to display if needed.
  virtual void OnOptimizeSolution(int thread_id, int64 value,
                                  const std::string& solution_string) = 0;

  // Callback on the end search event.
  virtual void OnSearchEnd(int thread_id, bool interrupted) = 0;

  // ----- Log methods ------

  // Logs the message from the given thread.
  virtual void Log(int thread_id, const std::string& message) const = 0;
  // Prints message to std::cout and adds a std::endl at the end.
  // The minizinc specifications indicates that solutions and search status
  // must be printed to std::cout.
  virtual void Print(int thread_id, const std::string& output) const = 0;

  // ----- Getters -----

  // Checks if we should finish the search right away, for instance,
  // in a satisfaction problem if a solution has already be found.
  virtual bool ShouldFinish() const = 0;

  // Returns the value of the best solution found during search.
  virtual int64 BestSolution() const = 0;

  // Returns true if the search was interrupted, usually by a time or
  // solution limit.
  virtual bool Interrupted() const = 0;

  // Returns the number of solutions found.
  int NumSolutions() const { return num_solutions_; }

  // Returns the limit on the number of solutions to find.
  int MaxNumSolutions() const { return max_num_solutions_; }

  // Indicates if we should print all solutions.
  bool ShouldPrintAllSolutions() const { return print_all_solutions_; }

// ----- Dedicated methods to create MT/Sequential specific objects -----

#if !defined(SWIG)
  // Creates the objective used by the search.
  // Each solver thread will get a different one.
  virtual OptimizeVar* CreateObjective(Solver* s, bool maximize, IntVar* var,
                                       int64 step, int thread_id) const = 0;
  // Creates a dedicated search limit.
  // Each solver thread will get a different one.
  virtual SearchLimit* CreateLimit(Solver* s, int thread_id) const = 0;
#endif  // SWIG

 protected:
  // Increments the number of solutions found.
  void IncrementSolutions() { num_solutions_++; }

 private:
  const bool print_all_solutions_;
  int num_solutions_;
  const int max_num_solutions_;
};

class MonoThreadReporting : public SearchReportingInterface {
 public:
  MonoThreadReporting(bool print_all, int num_solutions);
  ~MonoThreadReporting() override;

  void Init(int thread_id, const std::string& init_string) override;
  void OnSearchStart(int thread_id, Type type) override;
  void OnSatSolution(int thread_id, const std::string& solution_string) override;
  void OnOptimizeSolution(int thread_id, int64 value,
                          const std::string& solution_string) override;
  void Log(int thread_id, const std::string& message) const override;
  void Print(int thread_id, const std::string& final_output) const override;
  bool ShouldFinish() const override;
  void OnSearchEnd(int thread_id, bool interrupted) override;
  int64 BestSolution() const override;
#if !defined(SWIG)
  OptimizeVar* CreateObjective(Solver* s, bool maximize, IntVar* var,
                               int64 step, int thread_id) const override;
  SearchLimit* CreateLimit(Solver* s, int thread_id) const override;
#endif  // SWIG
  bool Interrupted() const override;

 private:
  std::string last_solution_;
  int64 best_objective_;
  bool interrupted_;
};

class SilentMonoThreadReporting : public MonoThreadReporting {
 public:
  SilentMonoThreadReporting(bool print_all, int num_solutions);
  ~SilentMonoThreadReporting() override;

  void Init(int thread_id, const std::string& init_string) override;
  void Log(int thread_id, const std::string& message) const override;
  void Print(int thread_id, const std::string& final_output) const override;
};

#if !defined(SWIG)
class MultiThreadReporting : public SearchReportingInterface {
 public:
  MultiThreadReporting(bool print_all, int num_solutions, bool verbose);
  ~MultiThreadReporting() override;
  void Init(int thread_id, const std::string& init_string) override;
  void OnSearchStart(int thread_id, Type type) override;
  void OnSatSolution(int thread_id, const std::string& solution_string) override;
  void OnOptimizeSolution(int thread_id, int64 value,
                          const std::string& solution_string) override;
  void Log(int thread_id, const std::string& message) const override;
  void Print(int thread_id, const std::string& final_out) const override;
  bool ShouldFinish() const override;
  void OnSearchEnd(int thread_id, bool interrupted) override;
  int64 BestSolution() const override;
  OptimizeVar* CreateObjective(Solver* s, bool maximize, IntVar* var,
                               int64 step, int w) const override;
  SearchLimit* CreateLimit(Solver* s, int thread_id) const override;
  bool Interrupted() const override;

 private:
  void LogNoLock(int thread_id, const std::string& message);

  const bool verbose_;
  mutable Mutex mutex_;
  Type type_ GUARDED_BY(mutex_);
  std::string last_solution_ GUARDED_BY(mutex_);
  int last_thread_ GUARDED_BY(mutex_);
  int64 best_objective_ GUARDED_BY(mutex_);
  bool should_finish_ GUARDED_BY(mutex_);
  bool interrupted_ GUARDED_BY(mutex_);
};
#endif  // SWIG
}  // namespace fz
}  // namespace operations_research
#endif  // OR_TOOLS_FLATZINC_REPORTING_H_
