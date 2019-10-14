// Copyright 2010-2018 Google LLC
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

#ifndef OR_TOOLS_SAT_SYNCHRONIZATION_H_
#define OR_TOOLS_SAT_SYNCHRONIZATION_H_

#include <string>
#include <vector>

#include "absl/synchronization/mutex.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/util/bitset.h"
#include "ortools/util/random_engine.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

// Thread-safe. Keeps a set of n unique best solution found so far.
//
// TODO(user): Maybe add some criteria to only keep solution with an objective
// really close to the best solution.
class SharedSolutionRepository {
 public:
  explicit SharedSolutionRepository(int num_solutions_to_keep)
      : num_solutions_to_keep_(num_solutions_to_keep) {
    CHECK_GE(num_solutions_to_keep_, 1);
  }

  // The solution format used by this class.
  // We use the unscaled internal minimization objective.
  struct Solution {
    int64 internal_objective;
    std::vector<int64> variable_values;

    bool operator==(const Solution& other) const {
      return internal_objective == other.internal_objective &&
             variable_values == other.variable_values;
    }
    bool operator<(const Solution& other) const {
      if (internal_objective != other.internal_objective) {
        return internal_objective < other.internal_objective;
      }
      return variable_values < other.variable_values;
    }
  };

  // Returns the number of current solution in the pool. This will never
  // decrease.
  int NumSolutions() const;

  // Returns the solution #i where i must be smaller than NumSolutions().
  Solution GetSolution(int index) const;

  // Returns a random solution biased towards good solutions.
  Solution GetRandomBiasedSolution(random_engine_t* random) const;

  // Add a new solution. Note that it will not be added to the pool of solution
  // right away. One must call Synchronize for this to happen.
  //
  // Works in O(num_solutions_to_keep_).
  void Add(const Solution& solution);

  // Updates the current pool of solution with the one recently added. Note that
  // we use a stable ordering of solutions, so the final pool will be
  // independent on the order of the calls to AddSolution() provided that the
  // set of added solutions is the same.
  //
  // Works in O(num_solutions_to_keep_).
  void Synchronize();

 private:
  const int num_solutions_to_keep_;
  mutable absl::Mutex mutex_;

  // Our two solutions pools, the current one and the new one that will be
  // merged into the current one on each Synchronize() calls.
  mutable std::vector<double> weights_ GUARDED_BY(mutex_);
  std::vector<Solution> solutions_ GUARDED_BY(mutex_);
  std::vector<Solution> new_solutions_ GUARDED_BY(mutex_);
};

// Manages the global best response kept by the solver.
// All functions are thread-safe.
class SharedResponseManager {
 public:
  // If log_updates is true, then all updates to the global "state" will be
  // logged. This class is responsible for our solver log progress.
  SharedResponseManager(bool log_updates, bool enumerate_all_solutions,
                        const CpModelProto* proto, const WallTimer* wall_timer,
                        const SharedTimeLimit* shared_time_limit);

  // Returns the current solver response. That is the best known response at the
  // time of the call with the best feasible solution and objective bounds.
  //
  // Note that the solver statistics correspond to the last time a better
  // solution was found or SetStatsFromModel() was called.
  CpSolverResponse GetResponse();

  // Adds a callback that will be called on each new solution (for
  // statisfiablity problem) or each improving new solution (for an optimization
  // problem). Returns its id so it can be unregistered if needed.
  //
  // Note that currently the class is waiting for the callback to finish before
  // accepting any new updates. That could be changed if needed.
  int AddSolutionCallback(
      std::function<void(const CpSolverResponse&)> callback);
  void UnregisterCallback(int callback_id);

  // The "inner" objective is the CpModelProto objective without scaling/offset.
  // Note that these bound correspond to valid bound for the problem of finding
  // a strictly better objective than the current one. Thus the lower bound is
  // always a valid bound for the global problem, but the upper bound is NOT.
  IntegerValue GetInnerObjectiveLowerBound();
  IntegerValue GetInnerObjectiveUpperBound();

  // Returns the current best solution inner objective value or kInt64Max if
  // there is no solution.
  IntegerValue BestSolutionInnerObjectiveValue();

  double PrimalIntegral() const;

  // Updates the inner objective bounds.
  void UpdateInnerObjectiveBounds(const std::string& worker_info,
                                  IntegerValue lb, IntegerValue ub);

  // Reads the new solution from the response and update our state. For an
  // optimization problem, we only do something if the solution is strictly
  // improving.
  //
  // TODO(user): Only the follwing fields from response are accessed here, we
  // might want a tighter API:
  //  - solution_info
  //  - solution
  //  - solution_lower_bounds and solution_upper_bounds.
  void NewSolution(const CpSolverResponse& response, Model* model);

  // Changes the solution to reflect the fact that the "improving" problem is
  // infeasible. This means that if we have a solution, we have proven
  // optimality, otherwise the global problem is infeasible.
  //
  // Note that this shouldn't be called before the solution is actually
  // reported. We check for this case in NewSolution().
  void NotifyThatImprovingProblemIsInfeasible(const std::string& worker_info);

  // Sets the statistics in the response to the one of the solver inside the
  // given in-memory model. This does nothing if the model is nullptr.
  //
  // TODO(user): Also support merging statistics together.
  void SetStatsFromModel(Model* model);

  // Returns true if we found the optimal solution or the problem was proven
  // infeasible.
  bool ProblemIsSolved() const;

  // Returns the underlying solution repository where we keep a set of best
  // solutions.
  const SharedSolutionRepository& SolutionsRepository() const {
    return solutions_;
  }
  SharedSolutionRepository* MutableSolutionsRepository() { return &solutions_; }

 private:
  void FillObjectiveValuesInBestResponse()
      EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void SetStatsFromModelInternal(Model* model)
      EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Updates the primal integral using the old bounds on the objective. If the
  // old bounds are not finite, it uses the 'max_integral' value instead of gap.
  void UpdatePrimalIntegral(int64 max_integral)
      EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  const bool log_updates_;
  const bool enumerate_all_solutions_;
  const CpModelProto& model_proto_;
  const WallTimer& wall_timer_;
  const SharedTimeLimit& shared_time_limit_;

  mutable absl::Mutex mutex_;

  CpSolverResponse best_response_ GUARDED_BY(mutex_);
  SharedSolutionRepository solutions_ GUARDED_BY(mutex_);

  int num_solutions_ GUARDED_BY(mutex_) = 0;
  int64 inner_objective_lower_bound_ GUARDED_BY(mutex_) = kint64min;
  int64 inner_objective_upper_bound_ GUARDED_BY(mutex_) = kint64max;
  int64 best_solution_objective_value_ GUARDED_BY(mutex_) = kint64max;
  double primal_integral_ GUARDED_BY(mutex_) = 0.0;
  double last_primal_integral_time_stamp_ GUARDED_BY(mutex_) = 0.0;

  int next_callback_id_ GUARDED_BY(mutex_) = 0;
  std::vector<std::pair<int, std::function<void(const CpSolverResponse&)>>>
      callbacks_ GUARDED_BY(mutex_);
};

// This class manages a pool of lower and upper bounds on a set of variables in
// a parallel context.
class SharedBoundsManager {
 public:
  SharedBoundsManager(int num_workers, const CpModelProto& model_proto);

  // Reports a set of locally improved variable bounds to the shared bounds
  // manager. The manager will compare these bounds changes against its
  // global state, and incorporate the improving ones.
  void ReportPotentialNewBounds(const CpModelProto& model_proto, int worker_id,
                                const std::string& worker_name,
                                const std::vector<int>& variables,
                                const std::vector<int64>& new_lower_bounds,
                                const std::vector<int64>& new_upper_bounds);

  // When called, returns the set of bounds improvements since
  // the last time this method was called by the same worker.
  void GetChangedBounds(int worker_id, std::vector<int>* variables,
                        std::vector<int64>* new_lower_bounds,
                        std::vector<int64>* new_upper_bounds);

 private:
  const int num_workers_;
  const int num_variables_;

  absl::Mutex mutex_;

  std::vector<SparseBitset<int64>> changed_variables_per_workers_
      GUARDED_BY(mutex_);
  std::vector<int64> lower_bounds_ GUARDED_BY(mutex_);
  std::vector<int64> upper_bounds_ GUARDED_BY(mutex_);
};

// Stores information on the worker in the parallel context.
struct WorkerInfo {
  std::string worker_name;
  int worker_id = -1;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_SYNCHRONIZATION_H_
