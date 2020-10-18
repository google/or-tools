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

#include <deque>
#include <string>
#include <vector>

#include "absl/random/random.h"
#include "absl/synchronization/mutex.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/stl_util.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/bitset.h"
#include "ortools/util/random_engine.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

// Thread-safe. Keeps a set of n unique best solution found so far.
//
// TODO(user): Maybe add some criteria to only keep solution with an objective
// really close to the best solution.
template <typename ValueType>
class SharedSolutionRepository {
 public:
  explicit SharedSolutionRepository(int num_solutions_to_keep)
      : num_solutions_to_keep_(num_solutions_to_keep) {
    CHECK_GE(num_solutions_to_keep_, 1);
  }

  // The solution format used by this class.
  struct Solution {
    // Solution with lower "rank" will be preferred
    //
    // TODO(user): Some LNS code assume that for the SharedSolutionRepository
    // this rank is actually the unscaled internal minimization objective.
    // Remove this assumptions by simply recomputing this value since it is not
    // too costly to do so.
    int64 rank;

    std::vector<ValueType> variable_values;

    // Number of time this was returned by GetRandomBiasedSolution(). We use
    // this information during the selection process.
    //
    // Should be private: only SharedSolutionRepository should modify this.
    mutable int num_selected = 0;

    bool operator==(const Solution& other) const {
      return rank == other.rank && variable_values == other.variable_values;
    }
    bool operator<(const Solution& other) const {
      if (rank != other.rank) {
        return rank < other.rank;
      }
      return variable_values < other.variable_values;
    }
  };

  // Returns the number of current solution in the pool. This will never
  // decrease.
  int NumSolutions() const;

  // Returns the solution #i where i must be smaller than NumSolutions().
  Solution GetSolution(int index) const;

  // Returns the variable value of variable 'var_index' from solution
  // 'solution_index' where solution_index must be smaller than NumSolutions()
  // and 'var_index' must be smaller than number of variables.
  ValueType GetVariableValueInSolution(int var_index, int solution_index) const;

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

 protected:
  // Helper method for adding the solutions once the mutex is acquired.
  void AddInternal(const Solution& solution)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  const int num_solutions_to_keep_;
  mutable absl::Mutex mutex_;
  int64 num_synchronization_ ABSL_GUARDED_BY(mutex_) = 0;

  // Our two solutions pools, the current one and the new one that will be
  // merged into the current one on each Synchronize() calls.
  mutable std::vector<int> tmp_indices_ ABSL_GUARDED_BY(mutex_);
  std::vector<Solution> solutions_ ABSL_GUARDED_BY(mutex_);
  std::vector<Solution> new_solutions_ ABSL_GUARDED_BY(mutex_);
};

// This is currently only used to store feasible solution from our 'relaxation'
// LNS generators which in turn are used to generate some RINS neighborhood.
class SharedRelaxationSolutionRepository
    : public SharedSolutionRepository<int64> {
 public:
  explicit SharedRelaxationSolutionRepository(int num_solutions_to_keep)
      : SharedSolutionRepository<int64>(num_solutions_to_keep) {}

  void NewRelaxationSolution(const CpSolverResponse& response);
};

class SharedLPSolutionRepository : public SharedSolutionRepository<double> {
 public:
  explicit SharedLPSolutionRepository(int num_solutions_to_keep)
      : SharedSolutionRepository<double>(num_solutions_to_keep) {}

  void NewLPSolution(std::vector<double> lp_solution);
};

// Set of partly filled solutions. They are meant to be finished by some lns
// worker.
//
// The solutions are stored as a vector of doubles. The value at index i
// represents the solution value of model variable indexed i. Note that some
// values can be infinity which should be interpreted as 'unknown' solution
// value for that variable. These solutions can not necessarily be completed to
// complete feasible solutions.
class SharedIncompleteSolutionManager {
 public:
  bool HasNewSolution() const;
  std::vector<double> GetNewSolution();

  void AddNewSolution(const std::vector<double>& lp_solution);

 private:
  // New solutions are added and removed from the back.
  std::vector<std::vector<double>> solutions_;
  mutable absl::Mutex mutex_;
};

// Manages the global best response kept by the solver.
// All functions are thread-safe.
class SharedResponseManager {
 public:
  // If log_updates is true, then all updates to the global "state" will be
  // logged. This class is responsible for our solver log progress.
  SharedResponseManager(bool log_updates, bool enumerate_all_solutions,
                        const CpModelProto* proto, const WallTimer* wall_timer,
                        SharedTimeLimit* shared_time_limit);

  // Reports OPTIMAL and stop the search if any gap limit are specified and
  // crossed. By default, we only stop when we have the true optimal, which is
  // well defined since we are solving our pure integer problem exactly.
  void SetGapLimitsFromParameters(const SatParameters& parameters);

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

  // These functions return the same as the non-synchronized() version but
  // only the values at the last time Synchronize() was called.
  void Synchronize();
  IntegerValue SynchronizedInnerObjectiveLowerBound();
  IntegerValue SynchronizedInnerObjectiveUpperBound();

  // Returns the current best solution inner objective value or kInt64Max if
  // there is no solution.
  IntegerValue BestSolutionInnerObjectiveValue();

  // Returns the integral of the log of the absolute gap over deterministic
  // time. This is mainly used to compare how fast the gap closes on a
  // particular instance. Or to evaluate how efficient our LNS code is improving
  // solution.
  //
  // Important: To report a proper deterministic integral, we only update it
  // on UpdatePrimalIntegral() which should be called in the main subsolver
  // synchronization loop.
  //
  // Note(user): In the litterature, people use the relative gap to the optimal
  // solution (or the best known one), but this is ill defined in many case
  // (like if the optimal cost is zero), so I prefer this version.
  double PrimalIntegral() const;
  void UpdatePrimalIntegral();

  // Updates the inner objective bounds.
  void UpdateInnerObjectiveBounds(const std::string& worker_info,
                                  IntegerValue lb, IntegerValue ub);

  // Reads the new solution from the response and update our state. For an
  // optimization problem, we only do something if the solution is strictly
  // improving.
  //
  // TODO(user): Only the following fields from response are accessed here, we
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

  // Adds to the shared response a subset of assumptions that are enough to
  // make the problem infeasible.
  void AddUnsatCore(const std::vector<int>& core);

  // Sets the statistics in the response to the one of the solver inside the
  // given in-memory model. This does nothing if the model is nullptr.
  //
  // TODO(user): Also support merging statistics together.
  void SetStatsFromModel(Model* model);

  // Returns true if we found the optimal solution or the problem was proven
  // infeasible. Note that if the gap limit is reached, we will also report
  // OPTIMAL and consider the problem solved.
  bool ProblemIsSolved() const;

  // Returns the underlying solution repository where we keep a set of best
  // solutions.
  const SharedSolutionRepository<int64>& SolutionsRepository() const {
    return solutions_;
  }
  SharedSolutionRepository<int64>* MutableSolutionsRepository() {
    return &solutions_;
  }

  // This should be called after the model is loaded. It will read the file
  // specified by --cp_model_load_debug_solution and properly fill the
  // model->Get<DebugSolution>() vector.
  //
  // TODO(user): Note that for now, only the IntegerVariable value are loaded,
  // not the value of the pure Booleans variables.
  void LoadDebugSolution(Model*);

  // Debug only. Set dump prefix for solutions written to file.
  void set_dump_prefix(const std::string& dump_prefix) {
    dump_prefix_ = dump_prefix;
  }

 private:
  void TestGapLimitsIfNeeded() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void FillObjectiveValuesInBestResponse()
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void SetStatsFromModelInternal(Model* model)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  const bool log_updates_;
  const bool enumerate_all_solutions_;
  const CpModelProto& model_proto_;
  const WallTimer& wall_timer_;
  SharedTimeLimit* shared_time_limit_;

  mutable absl::Mutex mutex_;

  // Gap limits.
  double absolute_gap_limit_ ABSL_GUARDED_BY(mutex_) = 0.0;
  double relative_gap_limit_ ABSL_GUARDED_BY(mutex_) = 0.0;

  CpSolverResponse best_response_ ABSL_GUARDED_BY(mutex_);
  SharedSolutionRepository<int64> solutions_ ABSL_GUARDED_BY(mutex_);

  int num_solutions_ ABSL_GUARDED_BY(mutex_) = 0;
  int64 inner_objective_lower_bound_ ABSL_GUARDED_BY(mutex_) = kint64min;
  int64 inner_objective_upper_bound_ ABSL_GUARDED_BY(mutex_) = kint64max;
  int64 best_solution_objective_value_ ABSL_GUARDED_BY(mutex_) = kint64max;

  IntegerValue synchronized_inner_objective_lower_bound_
      ABSL_GUARDED_BY(mutex_) = IntegerValue(kint64min);
  IntegerValue synchronized_inner_objective_upper_bound_
      ABSL_GUARDED_BY(mutex_) = IntegerValue(kint64max);

  double primal_integral_ ABSL_GUARDED_BY(mutex_) = 0.0;
  double last_primal_integral_time_stamp_ ABSL_GUARDED_BY(mutex_) = 0.0;

  int next_callback_id_ ABSL_GUARDED_BY(mutex_) = 0;
  std::vector<std::pair<int, std::function<void(const CpSolverResponse&)>>>
      callbacks_ ABSL_GUARDED_BY(mutex_);

  // Dump prefix.
  std::string dump_prefix_;
};

// This class manages a pool of lower and upper bounds on a set of variables in
// a parallel context.
class SharedBoundsManager {
 public:
  explicit SharedBoundsManager(const CpModelProto& model_proto);

  // Reports a set of locally improved variable bounds to the shared bounds
  // manager. The manager will compare these bounds changes against its
  // global state, and incorporate the improving ones.
  void ReportPotentialNewBounds(const CpModelProto& model_proto,
                                const std::string& worker_name,
                                const std::vector<int>& variables,
                                const std::vector<int64>& new_lower_bounds,
                                const std::vector<int64>& new_upper_bounds);

  // Returns a new id to be used in GetChangedBounds(). This is just an ever
  // increasing sequence starting from zero. Note that the class is not designed
  // to have too many of these.
  int RegisterNewId();

  // When called, returns the set of bounds improvements since
  // the last time this method was called with the same id.
  void GetChangedBounds(int id, std::vector<int>* variables,
                        std::vector<int64>* new_lower_bounds,
                        std::vector<int64>* new_upper_bounds);

  // Publishes any new bounds so that GetChangedBounds() will reflect the latest
  // state.
  void Synchronize();

 private:
  const int num_variables_;
  const CpModelProto& model_proto_;

  absl::Mutex mutex_;

  // These are always up to date.
  std::vector<int64> lower_bounds_ ABSL_GUARDED_BY(mutex_);
  std::vector<int64> upper_bounds_ ABSL_GUARDED_BY(mutex_);
  SparseBitset<int64> changed_variables_since_last_synchronize_
      ABSL_GUARDED_BY(mutex_);

  // These are only updated on Synchronize().
  std::vector<int64> synchronized_lower_bounds_ ABSL_GUARDED_BY(mutex_);
  std::vector<int64> synchronized_upper_bounds_ ABSL_GUARDED_BY(mutex_);
  std::deque<SparseBitset<int64>> id_to_changed_variables_
      ABSL_GUARDED_BY(mutex_);
};

template <typename ValueType>
int SharedSolutionRepository<ValueType>::NumSolutions() const {
  absl::MutexLock mutex_lock(&mutex_);
  return solutions_.size();
}

template <typename ValueType>
typename SharedSolutionRepository<ValueType>::Solution
SharedSolutionRepository<ValueType>::GetSolution(int i) const {
  absl::MutexLock mutex_lock(&mutex_);
  return solutions_[i];
}

template <typename ValueType>
ValueType SharedSolutionRepository<ValueType>::GetVariableValueInSolution(
    int var_index, int solution_index) const {
  absl::MutexLock mutex_lock(&mutex_);
  return solutions_[solution_index].variable_values[var_index];
}

// TODO(user): Experiments on the best distribution.
template <typename ValueType>
typename SharedSolutionRepository<ValueType>::Solution
SharedSolutionRepository<ValueType>::GetRandomBiasedSolution(
    random_engine_t* random) const {
  absl::MutexLock mutex_lock(&mutex_);
  const int64 best_rank = solutions_[0].rank;

  // As long as we have solution with the best objective that haven't been
  // explored too much, we select one uniformly. Otherwise, we select a solution
  // from the pool uniformly.
  //
  // Note(user): Because of the increase of num_selected, this is dependent on
  // the order of call. It should be fine for "determinism" because we do
  // generate the task of a batch always in the same order.
  const int kExplorationThreshold = 100;

  // Select all the best solution with a low enough selection count.
  tmp_indices_.clear();
  for (int i = 0; i < solutions_.size(); ++i) {
    const auto& solution = solutions_[i];
    if (solution.rank == best_rank &&
        solution.num_selected <= kExplorationThreshold) {
      tmp_indices_.push_back(i);
    }
  }

  int index = 0;
  if (tmp_indices_.empty()) {
    index = absl::Uniform<int>(*random, 0, solutions_.size());
  } else {
    index = tmp_indices_[absl::Uniform<int>(*random, 0, tmp_indices_.size())];
  }
  solutions_[index].num_selected++;
  return solutions_[index];
}

template <typename ValueType>
void SharedSolutionRepository<ValueType>::Add(const Solution& solution) {
  absl::MutexLock mutex_lock(&mutex_);
  AddInternal(solution);
}

template <typename ValueType>
void SharedSolutionRepository<ValueType>::AddInternal(
    const Solution& solution) {
  int worse_solution_index = 0;
  for (int i = 0; i < new_solutions_.size(); ++i) {
    // Do not add identical solution.
    if (new_solutions_[i] == solution) return;
    if (new_solutions_[worse_solution_index] < new_solutions_[i]) {
      worse_solution_index = i;
    }
  }
  if (new_solutions_.size() < num_solutions_to_keep_) {
    new_solutions_.push_back(solution);
  } else if (solution < new_solutions_[worse_solution_index]) {
    new_solutions_[worse_solution_index] = solution;
  }
}

template <typename ValueType>
void SharedSolutionRepository<ValueType>::Synchronize() {
  absl::MutexLock mutex_lock(&mutex_);
  solutions_.insert(solutions_.end(), new_solutions_.begin(),
                    new_solutions_.end());
  new_solutions_.clear();

  // We use a stable sort to keep the num_selected count for the already
  // existing solutions.
  //
  // TODO(user): Intoduce a notion of orthogonality to diversify the pool?
  gtl::STLStableSortAndRemoveDuplicates(&solutions_);
  if (solutions_.size() > num_solutions_to_keep_) {
    solutions_.resize(num_solutions_to_keep_);
  }
  num_synchronization_++;
}

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_SYNCHRONIZATION_H_
