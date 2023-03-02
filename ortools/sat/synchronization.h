// Copyright 2010-2022 Google LLC
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

#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/random.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/timer.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/util.h"
#include "ortools/util/bitset.h"
#include "ortools/util/logging.h"

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
      : num_solutions_to_keep_(num_solutions_to_keep) {}

  // The solution format used by this class.
  struct Solution {
    // Solution with lower "rank" will be preferred
    //
    // TODO(user): Some LNS code assume that for the SharedSolutionRepository
    // this rank is actually the unscaled internal minimization objective.
    // Remove this assumptions by simply recomputing this value since it is not
    // too costly to do so.
    int64_t rank = 0;

    std::vector<ValueType> variable_values;

    std::string info;

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
  Solution GetRandomBiasedSolution(absl::BitGenRef random) const;

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
  int64_t num_synchronization_ ABSL_GUARDED_BY(mutex_) = 0;

  // Our two solutions pools, the current one and the new one that will be
  // merged into the current one on each Synchronize() calls.
  mutable std::vector<int> tmp_indices_ ABSL_GUARDED_BY(mutex_);
  std::vector<Solution> solutions_ ABSL_GUARDED_BY(mutex_);
  std::vector<Solution> new_solutions_ ABSL_GUARDED_BY(mutex_);
};

// This is currently only used to store feasible solution from our 'relaxation'
// LNS generators which in turn are used to generate some RINS neighborhood.
class SharedRelaxationSolutionRepository
    : public SharedSolutionRepository<int64_t> {
 public:
  explicit SharedRelaxationSolutionRepository(int num_solutions_to_keep)
      : SharedSolutionRepository<int64_t>(num_solutions_to_keep) {}

  void NewRelaxationSolution(absl::Span<const int64_t> solution_values,
                             IntegerValue inner_objective_value);
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

// Used by FillSolveStatsInResponse() to extract statistic to put in a
// CpSolverResponse. The callbacks registered here are supposed to only modify
// the statistic fields, nothing else.
struct CpSolverResponseStatisticCallbacks {
  std::vector<std::function<void(CpSolverResponse*)>> callbacks;
};

// Get the solve statistics from the associated model classes and fills the
// response with them.
void FillSolveStatsInResponse(Model* model, CpSolverResponse* response);

// Manages the global best response kept by the solver. This class is
// responsible for logging the progress of the solutions and bounds as they are
// found.
//
// All functions are thread-safe except if specified otherwise.
class SharedResponseManager {
 public:
  explicit SharedResponseManager(Model* model);

  // Loads the initial objective bounds and keep a reference to the objective to
  // properly display the scaled bounds. This is optional if the model has no
  // objective.
  //
  // This function is not thread safe.
  void InitializeObjective(const CpModelProto& cp_model);

  // Reports OPTIMAL and stop the search if any gap limit are specified and
  // crossed. By default, we only stop when we have the true optimal, which is
  // well defined since we are solving our pure integer problem exactly.
  void SetGapLimitsFromParameters(const SatParameters& parameters);

  // Returns the current solver response. That is the best known response at the
  // time of the call with the best feasible solution and objective bounds.
  //
  // We will do more postprocessing by calling all the
  // AddFinalSolutionPostprocessor() postprocesors. Note that the response given
  // to the AddSolutionCallback() will not call them.
  CpSolverResponse GetResponse();

  // These will be called in REVERSE order on any feasible solution returned
  // to the user.
  void AddSolutionPostprocessor(
      std::function<void(std::vector<int64_t>*)> postprocessor);

  // These "postprocessing" steps will be applied in REVERSE order of
  // registration to all solution passed to the callbacks.
  void AddResponsePostprocessor(
      std::function<void(CpSolverResponse*)> postprocessor);

  // These "postprocessing" steps will only be applied after the others to the
  // solution returned by GetResponse().
  void AddFinalResponsePostprocessor(
      std::function<void(CpSolverResponse*)> postprocessor);

  // Adds a callback that will be called on each new solution (for
  // statisfiablity problem) or each improving new solution (for an optimization
  // problem). Returns its id so it can be unregistered if needed.
  //
  // Note that adding a callback is not free since the solution will be
  // postsolved before this is called.
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
  // Note: The integral will start counting on the first UpdateGapIntegral()
  // call, since before the difference is assumed to be zero.
  //
  // Important: To report a proper deterministic integral, we only update it
  // on UpdateGapIntegral() which should be called in the main subsolver
  // synchronization loop.
  //
  // Note(user): In the litterature, people use the relative gap to the optimal
  // solution (or the best known one), but this is ill defined in many case
  // (like if the optimal cost is zero), so I prefer this version.
  double GapIntegral() const;
  void UpdateGapIntegral();

  // Sets this to true to have the "real" but non-deterministic primal integral.
  // If this is true, then there is no need to manually call
  // UpdateGapIntegral() but it is not an issue to do so.
  void SetUpdateGapIntegralOnEachChange(bool set);

  // Sets this to false, it you want new solutions to wait for the Synchronize()
  // call.
  // The default 'true' indicates that all solutions passed through
  // NewSolution() are always propagated to the best response and to the
  // solution manager.
  void SetSynchronizationMode(bool always_synchronize);

  // Updates the inner objective bounds.
  void UpdateInnerObjectiveBounds(const std::string& update_info,
                                  IntegerValue lb, IntegerValue ub);

  // Reads the new solution from the response and update our state. For an
  // optimization problem, we only do something if the solution is strictly
  // improving.
  void NewSolution(absl::Span<const int64_t> solution_values,
                   const std::string& solution_info, Model* model = nullptr);

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

  // Returns true if we found the optimal solution or the problem was proven
  // infeasible. Note that if the gap limit is reached, we will also report
  // OPTIMAL and consider the problem solved.
  bool ProblemIsSolved() const;

  // Returns the underlying solution repository where we keep a set of best
  // solutions.
  const SharedSolutionRepository<int64_t>& SolutionsRepository() const {
    return solutions_;
  }
  SharedSolutionRepository<int64_t>* MutableSolutionsRepository() {
    return &solutions_;
  }

  // Debug only. Set dump prefix for solutions written to file.
  void set_dump_prefix(const std::string& dump_prefix) {
    dump_prefix_ = dump_prefix;
  }

  // Display improvement stats.
  void DisplayImprovementStatistics();

  void LogMessage(const std::string& prefix, const std::string& message);
  void LogPeriodicMessage(const std::string& prefix, const std::string& message,
                          double frequency_seconds,
                          absl::Time* last_logging_time);
  bool LoggingIsEnabled() const { return logger_->LoggingIsEnabled(); }

  void AppendResponseToBeMerged(const CpSolverResponse& response);

  std::atomic<bool>* first_solution_solvers_should_stop() {
    return &first_solution_solvers_should_stop_;
  }

  // We just store a loaded DebugSolution here. Note that this is supposed to be
  // stored once and then never change, so we do not need a mutex.
  void LoadDebugSolution(absl::Span<const int64_t> solution) {
    debug_solution_.assign(solution.begin(), solution.end());
  }
  const std::vector<int64_t>& DebugSolution() const { return debug_solution_; }

 private:
  void TestGapLimitsIfNeeded() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void FillObjectiveValuesInResponse(CpSolverResponse* response) const
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void UpdateGapIntegralInternal() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void RegisterSolutionFound(const std::string& improvement_info)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void RegisterObjectiveBoundImprovement(const std::string& improvement_info)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void UpdateBestStatus(const CpSolverStatus& status)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Generates a response for callbacks and GetResponse().
  CpSolverResponse GetResponseInternal(
      absl::Span<const int64_t> variable_values,
      const std::string& solution_info) ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  const SatParameters& parameters_;
  const WallTimer& wall_timer_;
  ModelSharedTimeLimit* shared_time_limit_;
  CpObjectiveProto const* objective_or_null_ = nullptr;

  mutable absl::Mutex mutex_;

  // Gap limits.
  double absolute_gap_limit_ ABSL_GUARDED_BY(mutex_) = 0.0;
  double relative_gap_limit_ ABSL_GUARDED_BY(mutex_) = 0.0;

  CpSolverStatus best_status_ ABSL_GUARDED_BY(mutex_) = CpSolverStatus::UNKNOWN;
  CpSolverStatus synchronized_best_status_ ABSL_GUARDED_BY(mutex_) =
      CpSolverStatus::UNKNOWN;
  std::vector<int> unsat_cores_ ABSL_GUARDED_BY(mutex_);
  SharedSolutionRepository<int64_t> solutions_ ABSL_GUARDED_BY(mutex_);

  int num_solutions_ ABSL_GUARDED_BY(mutex_) = 0;
  int64_t inner_objective_lower_bound_ ABSL_GUARDED_BY(mutex_) =
      std::numeric_limits<int64_t>::min();
  int64_t inner_objective_upper_bound_ ABSL_GUARDED_BY(mutex_) =
      std::numeric_limits<int64_t>::max();
  int64_t best_solution_objective_value_ ABSL_GUARDED_BY(mutex_) =
      std::numeric_limits<int64_t>::max();

  bool always_synchronize_ ABSL_GUARDED_BY(mutex_) = true;
  IntegerValue synchronized_inner_objective_lower_bound_ ABSL_GUARDED_BY(
      mutex_) = IntegerValue(std::numeric_limits<int64_t>::min());
  IntegerValue synchronized_inner_objective_upper_bound_ ABSL_GUARDED_BY(
      mutex_) = IntegerValue(std::numeric_limits<int64_t>::max());

  bool update_integral_on_each_change_ ABSL_GUARDED_BY(mutex_) = false;
  double gap_integral_ ABSL_GUARDED_BY(mutex_) = 0.0;
  double last_absolute_gap_ ABSL_GUARDED_BY(mutex_) = 0.0;
  double last_gap_integral_time_stamp_ ABSL_GUARDED_BY(mutex_) = 0.0;

  int next_callback_id_ ABSL_GUARDED_BY(mutex_) = 0;
  std::vector<std::pair<int, std::function<void(const CpSolverResponse&)>>>
      callbacks_ ABSL_GUARDED_BY(mutex_);

  std::vector<std::function<void(std::vector<int64_t>*)>>
      solution_postprocessors_ ABSL_GUARDED_BY(mutex_);
  std::vector<std::function<void(CpSolverResponse*)>> postprocessors_
      ABSL_GUARDED_BY(mutex_);
  std::vector<std::function<void(CpSolverResponse*)>> final_postprocessors_
      ABSL_GUARDED_BY(mutex_);

  // Dump prefix.
  std::string dump_prefix_;

  // Used for statistics of the improvements found by workers.
  absl::btree_map<std::string, int> primal_improvements_count_
      ABSL_GUARDED_BY(mutex_);
  absl::btree_map<std::string, int> dual_improvements_count_
      ABSL_GUARDED_BY(mutex_);

  SolverLogger* logger_;
  std::vector<CpSolverResponse> subsolver_responses_ ABSL_GUARDED_BY(mutex_);

  std::atomic<bool> first_solution_solvers_should_stop_ = false;

  std::vector<int64_t> debug_solution_;
};

// This class manages a pool of lower and upper bounds on a set of variables in
// a parallel context.
class SharedBoundsManager {
 public:
  explicit SharedBoundsManager(const CpModelProto& model_proto);

  // Reports a set of locally improved variable bounds to the shared bounds
  // manager. The manager will compare these bounds changes against its
  // global state, and incorporate the improving ones.
  void ReportPotentialNewBounds(const std::string& worker_name,
                                const std::vector<int>& variables,
                                const std::vector<int64_t>& new_lower_bounds,
                                const std::vector<int64_t>& new_upper_bounds);

  // If we solved a small independent component of the full problem, then we can
  // in most situation fix the solution on this subspace.
  //
  // Note that because there can be more than one optimal solution on an
  // independent subproblem, it is important to do that in a locked fashion, and
  // reject future incompatible fixing.
  void FixVariablesFromPartialSolution(
      const std::vector<int64_t>& solution,
      const std::vector<int>& variables_to_fix);

  // Returns a new id to be used in GetChangedBounds(). This is just an ever
  // increasing sequence starting from zero. Note that the class is not designed
  // to have too many of these.
  int RegisterNewId();

  // When called, returns the set of bounds improvements since
  // the last time this method was called with the same id.
  void GetChangedBounds(int id, std::vector<int>* variables,
                        std::vector<int64_t>* new_lower_bounds,
                        std::vector<int64_t>* new_upper_bounds);

  // Publishes any new bounds so that GetChangedBounds() will reflect the latest
  // state.
  void Synchronize();

  void LogStatistics(SolverLogger* logger);
  int NumBoundsExported(const std::string& worker_name);

  // If non-empty, we will check that all bounds update contains this solution.
  // Note that this might fail once we reach optimality and we might have wrong
  // bounds, but if it fail before that it can help find bugs.
  void LoadDebugSolution(absl::Span<const int64_t> solution) {
    debug_solution_.assign(solution.begin(), solution.end());
  }

 private:
  const int num_variables_;
  const CpModelProto& model_proto_;

  absl::Mutex mutex_;

  // These are always up to date.
  std::vector<int64_t> lower_bounds_ ABSL_GUARDED_BY(mutex_);
  std::vector<int64_t> upper_bounds_ ABSL_GUARDED_BY(mutex_);
  SparseBitset<int> changed_variables_since_last_synchronize_
      ABSL_GUARDED_BY(mutex_);

  // These are only updated on Synchronize().
  std::vector<int64_t> synchronized_lower_bounds_ ABSL_GUARDED_BY(mutex_);
  std::vector<int64_t> synchronized_upper_bounds_ ABSL_GUARDED_BY(mutex_);
  std::deque<SparseBitset<int>> id_to_changed_variables_
      ABSL_GUARDED_BY(mutex_);
  absl::btree_map<std::string, int> bounds_exported_ ABSL_GUARDED_BY(mutex_);

  std::vector<int64_t> debug_solution_;
};

// This class holds all the binary clauses that were found and shared by the
// workers.
//
// It is thread-safe.
//
// Note that this uses literal as encoded in a cp_model.proto. Thus, the
// literals can be negative numbers.
class SharedClausesManager {
 public:
  explicit SharedClausesManager(bool always_synchronize);
  void AddBinaryClause(int id, int lit1, int lit2);

  // Fills new_clauses with
  //   {{lit1 of clause1, lit2 of clause1},
  //    {lit1 of clause2, lit2 of clause2},
  //     ...}
  void GetUnseenBinaryClauses(int id,
                              std::vector<std::pair<int, int>>* new_clauses);

  // Ids are used to identify which worker is exporting/importing clauses.
  int RegisterNewId();
  void SetWorkerNameForId(int id, const std::string& worker_name);

  // Search statistics.
  void LogStatistics(SolverLogger* logger);

  // Unlocks waiting binary clauses for workers if always_synchronize is false.
  void Synchronize();

 private:
  absl::Mutex mutex_;
  // Cache to avoid adding the same clause twice.
  absl::flat_hash_set<std::pair<int, int>> added_binary_clauses_set_
      ABSL_GUARDED_BY(mutex_);
  std::vector<std::pair<int, int>> added_binary_clauses_
      ABSL_GUARDED_BY(mutex_);
  std::vector<int> id_to_last_processed_binary_clause_ ABSL_GUARDED_BY(mutex_);
  std::vector<int64_t> id_to_clauses_exported_;
  int last_visible_clause_ ABSL_GUARDED_BY(mutex_) = 0;
  const bool always_synchronize_ = true;

  // Used for reporting statistics.
  absl::flat_hash_map<int, std::string> id_to_worker_name_;
};

// Simple class to add statistics by name and print them at the end.
class SharedStatistics {
 public:
  SharedStatistics() = default;

  // Adds a bunch of stats, adding count for the same key together.
  void AddStats(absl::Span<const std::pair<std::string, int64_t>> stats);

  // Logs all the added stats.
  void Log(SolverLogger* logger);

 private:
  absl::Mutex mutex_;
  absl::flat_hash_map<std::string, int64_t> stats_ ABSL_GUARDED_BY(mutex_);
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
    absl::BitGenRef random) const {
  absl::MutexLock mutex_lock(&mutex_);
  const int64_t best_rank = solutions_[0].rank;

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
    index = absl::Uniform<int>(random, 0, solutions_.size());
  } else {
    index = tmp_indices_[absl::Uniform<int>(random, 0, tmp_indices_.size())];
  }
  solutions_[index].num_selected++;
  return solutions_[index];
}

template <typename ValueType>
void SharedSolutionRepository<ValueType>::Add(const Solution& solution) {
  if (num_solutions_to_keep_ <= 0) return;
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
  if (new_solutions_.empty()) return;

  solutions_.insert(solutions_.end(), new_solutions_.begin(),
                    new_solutions_.end());
  new_solutions_.clear();

  // We use a stable sort to keep the num_selected count for the already
  // existing solutions.
  //
  // TODO(user): Introduce a notion of orthogonality to diversify the pool?
  gtl::STLStableSortAndRemoveDuplicates(&solutions_);
  if (solutions_.size() > num_solutions_to_keep_) {
    solutions_.resize(num_solutions_to_keep_);
  }

  if (!solutions_.empty()) {
    VLOG(2) << "Solution pool update:"
            << " num_solutions=" << solutions_.size()
            << " min_rank=" << solutions_[0].rank
            << " max_rank=" << solutions_.back().rank;
  }

  num_synchronization_++;
}

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_SYNCHRONIZATION_H_
