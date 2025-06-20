// Copyright 2010-2025 Google LLC
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

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/base/thread_annotations.h"
#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/random.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/timer.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/util.h"
#include "ortools/util/bitset.h"
#include "ortools/util/logging.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {

// Thread-safe. Keeps a set of n unique best solution found so far.
//
// TODO(user): Maybe add some criteria to only keep solution with an objective
// really close to the best solution.
template <typename ValueType>
class SharedSolutionRepository {
 public:
  explicit SharedSolutionRepository(int num_solutions_to_keep,
                                    absl::string_view name = "",
                                    int source_id = -1)
      : name_(name),
        num_solutions_to_keep_(num_solutions_to_keep),
        source_id_(source_id) {}

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

    int source_id;  // Internal information.

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
  // Returns nullptr if i is out of range.
  std::shared_ptr<const Solution> GetSolution(int index) const;

  // Returns the rank of the best known solution. If there is no solution, this
  // will return std::numeric_limits<int64_t>::max().
  int64_t GetBestRank() const;

  std::vector<std::shared_ptr<const Solution>> GetBestNSolutions(int n) const;

  // Returns the variable value of variable 'var_index' from solution
  // 'solution_index' where solution_index must be smaller than NumSolutions()
  // and 'var_index' must be smaller than number of variables.
  ValueType GetVariableValueInSolution(int var_index, int solution_index) const;

  // Returns a random solution biased towards good solutions.
  std::shared_ptr<const Solution> GetRandomBiasedSolution(
      absl::BitGenRef random) const;

  // Add a new solution. Note that it will not be added to the pool of solution
  // right away. One must call Synchronize for this to happen. In order to be
  // deterministic, this will keep all solutions until Synchronize() is called,
  // so we need to be careful not to generate too many solutions at once.
  //
  // Returns a shared pointer to the solution that was stored in the repository.
  std::shared_ptr<const Solution> Add(Solution solution);

  // Updates the current pool of solution with the one recently added. Note that
  // we use a stable ordering of solutions, so the final pool will be
  // independent on the order of the calls to AddSolution() provided that the
  // set of added solutions is the same.
  //
  // Works in O(num_solutions_to_keep_).
  //
  // If f() is provided, it will be called on all new solutions.
  void Synchronize(std::function<void(const Solution& solution)> f = nullptr);

  std::vector<std::string> TableLineStats() const {
    absl::MutexLock mutex_lock(&mutex_);
    return {FormatName(name_), FormatCounter(num_added_),
            FormatCounter(num_queried_), FormatCounter(num_synchronization_)};
  }

  int64_t NumRecentlyNonImproving() const {
    absl::MutexLock mutex_lock(&mutex_);
    return num_non_improving_;
  }

  void ClearSolutionsAndIncreaseSourceId() {
    absl::MutexLock mutex_lock(&mutex_);
    new_solutions_.clear();
    solutions_.clear();
    ++source_id_;
  }

  int source_id() const {
    absl::MutexLock mutex_lock(&mutex_);
    return source_id_;
  }

  int num_queried() const {
    absl::MutexLock mutex_lock(&mutex_);
    return num_queried_;
  }

  int num_solutions_to_keep() const { return num_solutions_to_keep_; }

 protected:
  const std::string name_;
  const int num_solutions_to_keep_;

  mutable absl::Mutex mutex_;
  int source_id_ ABSL_GUARDED_BY(mutex_);
  int64_t num_added_ ABSL_GUARDED_BY(mutex_) = 0;
  mutable int64_t num_queried_ ABSL_GUARDED_BY(mutex_) = 0;
  int64_t num_synchronization_ ABSL_GUARDED_BY(mutex_) = 0;

  mutable int64_t num_queried_at_last_sync_ ABSL_GUARDED_BY(mutex_) = 0;
  mutable int64_t num_non_improving_ ABSL_GUARDED_BY(mutex_) = 0;

  // Our two solutions pools, the current one and the new one that will be
  // merged into the current one on each Synchronize() calls.
  mutable std::vector<int> tmp_indices_ ABSL_GUARDED_BY(mutex_);
  std::vector<std::shared_ptr<Solution>> solutions_ ABSL_GUARDED_BY(mutex_);
  std::vector<std::shared_ptr<Solution>> new_solutions_ ABSL_GUARDED_BY(mutex_);

  // For computing orthogonality.
  std::vector<int64_t> ABSL_GUARDED_BY(mutex_) distances_;
  std::vector<int64_t> ABSL_GUARDED_BY(mutex_) buffer_;
};

// Solutions coming from the LP.
class SharedLPSolutionRepository : public SharedSolutionRepository<double> {
 public:
  explicit SharedLPSolutionRepository(int num_solutions_to_keep)
      : SharedSolutionRepository<double>(num_solutions_to_keep,
                                         "lp solutions") {}

  void NewLPSolution(std::vector<double> lp_solution);
};

// This stores all the feasible solutions the solver know about.
// Moreover, for meta-heuristics, we keep them in different buckets.
class SharedSolutionPool {
 public:
  explicit SharedSolutionPool(const SatParameters& parameters_)
      : best_solutions_(parameters_.solution_pool_size(), "best_solutions"),
        alternative_path_(parameters_.alternative_pool_size(),
                          "alternative_path", /*source_id=*/0) {}

  const SharedSolutionRepository<int64_t>& BestSolutions() const {
    return best_solutions_;
  }

  // Note that the given random generator is likely local to the thread calling
  // this.
  std::shared_ptr<const SharedSolutionRepository<int64_t>::Solution>
  GetSolutionToImprove(absl::BitGenRef random) const {
    // If we seems to have trouble making progress, work on the alternative
    // path too.
    if (alternative_path_.num_solutions_to_keep() > 0 &&
        best_solutions_.NumRecentlyNonImproving() > 100 &&
        absl::Bernoulli(random, 0.5) && alternative_path_.NumSolutions() > 0) {
      // Tricky: We might clear the alternative_path_ between NumSolutions()
      // and this call.
      auto result = alternative_path_.GetRandomBiasedSolution(random);
      if (result != nullptr) return result;
    }

    if (best_solutions_.NumSolutions() > 0) {
      return best_solutions_.GetRandomBiasedSolution(random);
    }
    return nullptr;
  }

  std::shared_ptr<const SharedSolutionRepository<int64_t>::Solution> Add(
      SharedSolutionRepository<int64_t>::Solution solution);

  void Synchronize(absl::BitGenRef random);

  void AddTableStats(std::vector<std::vector<std::string>>* table) const {
    table->push_back(best_solutions_.TableLineStats());
    table->push_back(alternative_path_.TableLineStats());
  }

 private:
  // Currently we only have two "pools" of solutions.
  SharedSolutionRepository<int64_t> best_solutions_;
  SharedSolutionRepository<int64_t> alternative_path_;

  // We also keep a list of possible "path seeds" in n buckets defined according
  // to the objective value of the solution. These are updated on Synchronize().
  // Bucket i will only contain the last seen solution in the internal objective
  // range [ranks_[i], ranks_[i + 1]).
  //
  // ranks_[0] should always be min_rank_, and seeds_[0] should be one of the
  // best known solution. We usually never select seeds_[0] but keep it around
  // for later in case new best solutions are found.
  absl::Mutex mutex_;
  int64_t max_rank_ ABSL_GUARDED_BY(mutex_) =
      std::numeric_limits<int64_t>::min();
  int64_t min_rank_ ABSL_GUARDED_BY(mutex_) =
      std::numeric_limits<int64_t>::max();
  std::vector<int64_t> ranks_;
  std::vector<
      std::shared_ptr<const SharedSolutionRepository<int64_t>::Solution>>
      ABSL_GUARDED_BY(mutex_) seeds_;
};

// Set of best solution from the feasibility jump workers.
//
// We store (solution, num_violated_constraints), so we have a list of solutions
// that violate as little constraints as possible. This can be used to set the
// phase during SAT search.
//
// TODO(user): We could also use it after first solution to orient a SAT search
// towards better solutions. But then it is a bit trickier to rank solutions
// compared to the old ones.
class SharedLsSolutionRepository : public SharedSolutionRepository<int64_t> {
 public:
  SharedLsSolutionRepository()
      : SharedSolutionRepository<int64_t>(10, "fj solution hints") {}

  void AddSolution(std::vector<int64_t> solution, int num_violations) {
    SharedSolutionRepository<int64_t>::Solution sol;
    sol.rank = num_violations;
    sol.variable_values = std::move(solution);
    Add(sol);
  }
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
  // This adds a new solution to the stack.
  // Note that we keep the last 100 ones at most.
  void AddSolution(const std::vector<double>& lp_solution);

  bool HasSolution() const;

  // If there are no solution, this return an empty vector.
  std::vector<double> PopLast();

  std::vector<std::string> TableLineStats() const {
    absl::MutexLock mutex_lock(&mutex_);
    return {FormatName("pump"), FormatCounter(num_added_),
            FormatCounter(num_queried_)};
  }

 private:
  mutable absl::Mutex mutex_;
  std::deque<std::vector<double>> solutions_ ABSL_GUARDED_BY(mutex_);
  int64_t num_added_ ABSL_GUARDED_BY(mutex_) = 0;
  mutable int64_t num_queried_ ABSL_GUARDED_BY(mutex_) = 0;
};

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

  // Similar to the one above, but this one has access to the local model used
  // to create the response. It can thus extract statistics and fill them.
  void AddStatisticsPostprocessor(
      std::function<void(Model*, CpSolverResponse*)> postprocessor);

  // Calls all registered AddStatisticsPostprocessor() with the given model on
  // the given response.
  void FillSolveStatsInResponse(Model* model, CpSolverResponse* response);

  // Adds a callback that will be called on each new solution (for
  // satisfiability problem) or each improving new solution (for an optimization
  // problem). Returns its id so it can be unregistered if needed.
  //
  // Note that adding a callback is not free since the solution will be
  // post-solved before this is called.
  //
  // Note that currently the class is waiting for the callback to finish before
  // accepting any new updates. That could be changed if needed.
  int AddSolutionCallback(
      std::function<void(const CpSolverResponse&)> callback);
  void UnregisterCallback(int callback_id);

  // Adds an inline callback that will be called on each new solution (for
  // satisfiability problem) or each improving new solution (for an optimization
  // problem). Returns its id so it can be unregistered if needed.
  //
  // Note that adding a callback is not free since the solution will be
  // post-solved before this is called.
  //
  // Note that currently the class is waiting for the callback to finish before
  // accepting any new updates. That could be changed if needed.
  int AddLogCallback(
      std::function<std::string(const CpSolverResponse&)> callback);
  void UnregisterLogCallback(int callback_id);

  // Adds a callback that will be called on each new best objective bound
  // found. Returns its id so it can be unregistered if needed.
  //
  // Note that currently the class is waiting for the callback to finish before
  // accepting any new updates. That could be changed if needed.
  int AddBestBoundCallback(std::function<void(double)> callback);
  void UnregisterBestBoundCallback(int callback_id);

  // The "inner" objective is the CpModelProto objective without scaling/offset.
  // Note that these bound correspond to valid bound for the problem of finding
  // a strictly better objective than the current one. Thus the lower bound is
  // always a valid bound for the global problem, but the upper bound is NOT.
  //
  // This is always the last bounds in "always_synchronize" mode, otherwise it
  // correspond to the bounds at the last Synchronize() call.
  void Synchronize();
  IntegerValue GetInnerObjectiveLowerBound();
  IntegerValue GetInnerObjectiveUpperBound();
  IntegerValue GetBestSolutionObjective() {
    if (solution_pool_.BestSolutions().NumSolutions() > 0) {
      return solution_pool_.BestSolutions().GetBestRank();
    } else {
      return GetInnerObjectiveUpperBound();
    }
  }

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
  // improving. Returns a shared pointer to the solution that was potentially
  // stored in the repository.
  std::shared_ptr<const SharedSolutionRepository<int64_t>::Solution>
  NewSolution(absl::Span<const int64_t> solution_values,
              const std::string& solution_info, Model* model = nullptr,
              int source_id = -1);

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

  bool HasFeasibleSolution() const {
    return solution_pool_.BestSolutions().NumSolutions() > 0;
  }

  // Returns the underlying solution repository where we keep a set of best
  // solutions.
  const SharedSolutionPool& SolutionPool() const { return solution_pool_; }

  // Debug only. Set dump prefix for solutions written to file.
  void set_dump_prefix(absl::string_view dump_prefix) {
    dump_prefix_ = dump_prefix;
  }

  // Display improvement stats.
  void DisplayImprovementStatistics();

  // Wrapper around our SolverLogger, but protected by mutex.
  void LogMessage(absl::string_view prefix, absl::string_view message);
  void LogMessageWithThrottling(absl::string_view prefix,
                                absl::string_view message);
  bool LoggingIsEnabled() const;

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

  void RegisterSolutionFound(const std::string& improvement_info,
                             int solution_rank)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void RegisterObjectiveBoundImprovement(const std::string& improvement_info)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void UpdateBestStatus(const CpSolverStatus& status)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Generates a response for callbacks and GetResponse().
  CpSolverResponse GetResponseInternal(
      absl::Span<const int64_t> variable_values,
      absl::string_view solution_info) ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  const SatParameters& parameters_;
  const WallTimer& wall_timer_;
  ModelSharedTimeLimit* shared_time_limit_;
  ModelRandomGenerator* random_;
  CpObjectiveProto const* objective_or_null_ = nullptr;

  mutable absl::Mutex mutex_;

  // Gap limits.
  double absolute_gap_limit_ ABSL_GUARDED_BY(mutex_) = 0.0;
  double relative_gap_limit_ ABSL_GUARDED_BY(mutex_) = 0.0;

  CpSolverStatus best_status_ ABSL_GUARDED_BY(mutex_) = CpSolverStatus::UNKNOWN;
  CpSolverStatus synchronized_best_status_ ABSL_GUARDED_BY(mutex_) =
      CpSolverStatus::UNKNOWN;
  std::vector<int> unsat_cores_ ABSL_GUARDED_BY(mutex_);
  SharedSolutionPool solution_pool_;  // Thread-safe.

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

  int next_search_log_callback_id_ ABSL_GUARDED_BY(mutex_) = 0;
  std::vector<
      std::pair<int, std::function<std::string(const CpSolverResponse&)>>>
      search_log_callbacks_ ABSL_GUARDED_BY(mutex_);

  int next_best_bound_callback_id_ ABSL_GUARDED_BY(mutex_) = 0;
  std::vector<std::pair<int, std::function<void(double)>>> best_bound_callbacks_
      ABSL_GUARDED_BY(mutex_);

  std::vector<std::function<void(std::vector<int64_t>*)>>
      solution_postprocessors_ ABSL_GUARDED_BY(mutex_);
  std::vector<std::function<void(CpSolverResponse*)>> postprocessors_
      ABSL_GUARDED_BY(mutex_);
  std::vector<std::function<void(CpSolverResponse*)>> final_postprocessors_
      ABSL_GUARDED_BY(mutex_);
  std::vector<std::function<void(Model*, CpSolverResponse*)>>
      statistics_postprocessors_ ABSL_GUARDED_BY(mutex_);

  // Dump prefix.
  std::string dump_prefix_;

  // Used for statistics of the improvements found by workers.
  absl::btree_map<std::string, int> primal_improvements_count_
      ABSL_GUARDED_BY(mutex_);
  absl::btree_map<std::string, int> primal_improvements_min_rank_
      ABSL_GUARDED_BY(mutex_);
  absl::btree_map<std::string, int> primal_improvements_max_rank_
      ABSL_GUARDED_BY(mutex_);

  absl::btree_map<std::string, int> dual_improvements_count_
      ABSL_GUARDED_BY(mutex_);

  SolverLogger* logger_ ABSL_GUARDED_BY(mutex_);
  absl::flat_hash_map<std::string, int> throttling_ids_ ABSL_GUARDED_BY(mutex_);

  int bounds_logging_id_;
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
                                absl::Span<const int> variables,
                                absl::Span<const int64_t> new_lower_bounds,
                                absl::Span<const int64_t> new_upper_bounds);

  // If we solved a small independent component of the full problem, then we can
  // in most situation fix the solution on this subspace.
  //
  // Note that because there can be more than one optimal solution on an
  // independent subproblem, it is important to do that in a locked fashion, and
  // reject future incompatible fixing.
  //
  // Note that this do not work with symmetries. And for now we don't call it
  // when this is the case.
  void FixVariablesFromPartialSolution(absl::Span<const int64_t> solution,
                                       absl::Span<const int> variables_to_fix);

  // Returns a new id to be used in GetChangedBounds(). This is just an ever
  // increasing sequence starting from zero. Note that the class is not designed
  // to have too many of these.
  int RegisterNewId();

  // When called, returns the set of bounds improvements since
  // the last time this method was called with the same id.
  void GetChangedBounds(int id, std::vector<int>* variables,
                        std::vector<int64_t>* new_lower_bounds,
                        std::vector<int64_t>* new_upper_bounds);

  // This should not be called too often as it lock the class for
  // O(num_variables) time.
  void UpdateDomains(std::vector<Domain>* domains);

  // Publishes any new bounds so that GetChangedBounds() will reflect the latest
  // state.
  void Synchronize();

  void LogStatistics(SolverLogger* logger);
  int NumBoundsExported(absl::string_view worker_name);

  // If non-empty, we will check that all bounds update contains this solution.
  // Note that this might fail once we reach optimality and we might have wrong
  // bounds, but if it fail before that it can help find bugs.
  void LoadDebugSolution(absl::Span<const int64_t> solution) {
    debug_solution_.assign(solution.begin(), solution.end());
  }

  // Debug only. Set dump prefix for solutions written to file.
  void set_dump_prefix(absl::string_view dump_prefix) {
    dump_prefix_ = dump_prefix;
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
  int64_t total_num_improvements_ ABSL_GUARDED_BY(mutex_) = 0;

  // These are only updated on Synchronize().
  std::vector<int64_t> synchronized_lower_bounds_ ABSL_GUARDED_BY(mutex_);
  std::vector<int64_t> synchronized_upper_bounds_ ABSL_GUARDED_BY(mutex_);
  std::deque<SparseBitset<int>> id_to_changed_variables_
      ABSL_GUARDED_BY(mutex_);

  // We track the number of bounds exported by each solver, and the "extra"
  // bounds pushed due to symmetries.
  struct Counters {
    int64_t num_exported = 0;
    int64_t num_symmetric = 0;
  };
  absl::btree_map<std::string, Counters> bounds_exported_
      ABSL_GUARDED_BY(mutex_);

  // Symmetry info.
  bool has_symmetry_ = false;
  std::vector<int> var_to_representative_;  // Identity if not touched.
  std::vector<int> var_to_orbit_index_;
  CompactVectorVector<int, int> orbits_;

  std::vector<int64_t> debug_solution_;
  std::string dump_prefix_;
  int export_counter_ = 0;
};

// Emit a stream of clauses in batches without duplicates. Each batch has a
// fixed number of literals, containing the smallest clauses added.
// It has a finite size internal buffer that is a small multiple of the batch
// size.
//
// This uses a finite buffer, so some clauses may be dropped if we generate too
// many more than we export, but that is rarely a problem because if we drop a
// clause on a worker, one of the following will most likely happen:
//   1. Some other worker learns the clause and shares it later.
//   2. All other workers also learn and drop the clause.
//   3. No other worker learns the clause, so it was not that helpful anyway.
//
// Note that this uses literals as encoded in a cp_model.proto. Thus, the
// literals can be negative numbers.
//
// TODO(user): This class might not want to live in this file now it no
// longer needs to be thread-safe.
class UniqueClauseStream {
 public:
  static constexpr int kMinClauseSize = 3;
  static constexpr int kMaxClauseSize = 32;
  static constexpr int kMinLbd = 2;
  static constexpr int kMaxLbd = 5;
  // Export 4KiB of clauses per batch.
  static constexpr int kMaxLiteralsPerBatch = 4096 / sizeof(int);

  UniqueClauseStream();
  // Move only - this is an expensive class to copy.
  UniqueClauseStream(const UniqueClauseStream&) = delete;
  UniqueClauseStream(UniqueClauseStream&&) = default;

  // Adds the clause to a future batch and returns true if the clause is new,
  // otherwise returns false.
  bool Add(absl::Span<const int> clause, int lbd = 2);

  // Stop a clause being added to future batches.
  // Returns true if the clause is new.
  // This is approximate and can have false positives and negatives, it is still
  // guaranteed to prevent adding the same clause twice to the next batch.
  bool BlockClause(absl::Span<const int> clause);

  // Returns a set of clauses totalling up to kMaxLiteralsPerBatch and clears
  // the internal buffer.
  // Increases the LBD threshold if the batch is underfull, and decreases it if
  // too many clauses were dropped.
  CompactVectorVector<int> NextBatch();

  void ClearFingerprints() {
    old_fingerprints_.clear();
    fingerprints_.clear();
    fingerprints_.reserve(kMaxFingerprints);
  }

  // Returns the number of buffered literals in clauses of a given size.
  int NumLiteralsOfSize(int size) const;
  int NumBufferedLiterals() const;

  int lbd_threshold() const { return lbd_threshold_; }
  void set_lbd_threshold(int lbd_threshold) { lbd_threshold_ = lbd_threshold; }

  // Computes a hash that is independent of the order of literals in the clause.
  static size_t HashClause(absl::Span<const int> clause, size_t hash_seed = 0);

 private:
  // This needs to be >> the number of clauses we can plausibly learn in
  // a few seconds.
  constexpr static size_t kMaxFingerprints = 1024 * 1024 / sizeof(size_t);
  constexpr static int kNumSizes = kMaxClauseSize - kMinClauseSize + 1;

  std::vector<int>* MutableBufferForSize(int size) {
    return &clauses_by_size_[size - kMinClauseSize];
  }
  absl::Span<const int> BufferForSize(int size) const {
    return clauses_by_size_[size - kMinClauseSize];
  }
  absl::Span<const int> NextClause(int size) const;
  void PopClause(int size);
  // Computes the number of clauses of a given size.
  int NumClausesOfSize(int size) const;

  int lbd_threshold_ = kMinLbd;
  int64_t dropped_literals_since_last_batch_ = 0;

  absl::flat_hash_set<size_t> fingerprints_;
  absl::flat_hash_set<size_t> old_fingerprints_;
  std::array<std::vector<int>, kNumSizes> clauses_by_size_;
};

// This class holds clauses found and shared by workers.
// It is exact for binary clauses, but approximate for longer ones.
//
// It is thread-safe.
//
// Note that this uses literal as encoded in a cp_model.proto. Thus, the
// literals can be negative numbers.
class SharedClausesManager {
 public:
  explicit SharedClausesManager(bool always_synchronize);
  void AddBinaryClause(int id, int lit1, int lit2);

  // Returns new glue clauses.
  // The spans are guaranteed to remain valid until the next call to
  // SyncClauses().
  const CompactVectorVector<int>& GetUnseenClauses(int id);

  void AddBatch(int id, CompactVectorVector<int> batch);

  // Fills new_clauses with
  //   {{lit1 of clause1, lit2 of clause1},
  //    {lit1 of clause2, lit2 of clause2},
  //     ...}
  void GetUnseenBinaryClauses(int id,
                              std::vector<std::pair<int, int>>* new_clauses);

  // Ids are used to identify which worker is exporting/importing clauses.
  int RegisterNewId(absl::string_view worker_name, bool may_terminate_early);

  // Search statistics.
  void LogStatistics(SolverLogger* logger);

  // Unlocks waiting binary clauses for workers if always_synchronize is false.
  // Periodically starts a new sharing round, making glue clauses visible.
  void Synchronize();

 private:
  // Returns true if `reader_id` should read batches produced by `writer_id`.
  bool ShouldReadBatch(int reader_id, int writer_id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  static constexpr int kMinBatches = 64;
  mutable absl::Mutex mutex_;

  // Binary clauses:
  // Cache to avoid adding the same binary clause twice.
  absl::flat_hash_set<std::pair<int, int>> added_binary_clauses_set_
      ABSL_GUARDED_BY(mutex_);
  std::vector<std::pair<int, int>> added_binary_clauses_
      ABSL_GUARDED_BY(mutex_);
  std::vector<int> id_to_last_processed_binary_clause_ ABSL_GUARDED_BY(mutex_);
  int last_visible_binary_clause_ ABSL_GUARDED_BY(mutex_) = 0;

  // This is slightly subtle - we need to track the batches that might be
  // currently being processed by each worker to make sure we don't erase any
  // batch that a worker might currently be reading.
  std::vector<int> id_to_last_returned_batch_ ABSL_GUARDED_BY(mutex_);
  std::vector<int> id_to_last_finished_batch_ ABSL_GUARDED_BY(mutex_);

  std::deque<CompactVectorVector<int>> batches_ ABSL_GUARDED_BY(mutex_);
  // pending_batches_ contains clauses produced by individual workers that have
  // not yet been merged into batches_, which can be read by other workers. When
  // this is long enough they will be merged into a single batch and appended to
  // batches_.
  std::vector<CompactVectorVector<int>> pending_batches_
      ABSL_GUARDED_BY(mutex_);
  int num_full_workers_ ABSL_GUARDED_BY(mutex_) = 0;

  const bool always_synchronize_ = true;

  // Stats:
  std::vector<int64_t> id_to_num_exported_ ABSL_GUARDED_BY(mutex_);
  std::vector<int64_t> id_to_num_updated_ ABSL_GUARDED_BY(mutex_);
  std::vector<std::string> id_to_worker_name_ ABSL_GUARDED_BY(mutex_);
};

// A class that allows to exchange root level bounds on linear2.
//
// TODO(user): Add Synchronize() support and only publish new bounds when this
// is called.
class SharedLinear2Bounds {
 public:
  int RegisterNewId(std::string worker_name);
  void LogStatistics(SolverLogger* logger);

  // This should only contain canonicalized expression.
  // See the code for IsCanonicalized() for the definition.
  struct Key {
    int vars[2];
    IntegerValue coeffs[2];

    bool IsCanonicalized() {
      return coeffs[0] > 0 && coeffs[1] != 0 && vars[0] < vars[1] &&
             std::gcd(coeffs[0].value(), coeffs[1].value()) == 1;
    }

    bool operator==(const Key& o) const {
      return vars[0] == o.vars[0] && vars[1] == o.vars[1] &&
             coeffs[0] == o.coeffs[0] && coeffs[1] == o.coeffs[1];
    }

    template <typename H>
    friend H AbslHashValue(H h, const Key& k) {
      return H::combine(std::move(h), k.vars[0], k.vars[1], k.coeffs[0],
                        k.coeffs[1]);
    }
  };

  // Exports new bounds on the given expr (should be canonicalized).
  void Add(int id, Key expr, IntegerValue lb, IntegerValue ub);

  // This is called less often, and maybe not every-worker that exports want to
  // export, so we use a separate id space. Because we rely on hash map to
  // check if a bound is new, it is not such a big deal that a worker re-read
  // once the bounds it exported.
  int RegisterNewImportId(std::string name);

  // Returns the linear2 and their bounds.
  // We only return changes since the last call with the same id.
  std::vector<std::pair<Key, std::pair<IntegerValue, IntegerValue>>>
  NewlyUpdatedBounds(int import_id);

  // This is not filled by NewlyUpdatedBounds() because we want to track the
  // bounds that were not already known by the worker at the time of the import,
  // and we don't have this information here.
  void NotifyNumImported(int import_id, int num) {
    absl::MutexLock mutex_lock(&mutex_);
    import_id_to_num_imported_[import_id] += num;
  }

 private:
  void MaybeCompressNewlyUpdateKeys() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  absl::Mutex mutex_;

  // The best known bounds for each key.
  absl::flat_hash_map<Key, std::pair<IntegerValue, IntegerValue>> shared_bounds_
      ABSL_GUARDED_BY(mutex_);

  // Ever growing list of updated position in shared_bounds_.
  // Note that we do reduce it in MaybeCompressNewlyUpdateKeys(), but that
  // requires all registered workers to have at least imported some bounds.
  //
  // TODO(user): use indirect addressing so that newly_updated_keys_ can just
  // deal with indices, and it is a bit tighter memory wise? We also avoid
  // hash-lookups on NewlyUpdatedBounds(). But since this is only called at
  // level zero on new bounds, I don't think we care.
  std::vector<Key> newly_updated_keys_;

  // For import.
  std::vector<std::string> import_id_to_name_ ABSL_GUARDED_BY(mutex_);
  std::vector<int> import_id_to_index_ ABSL_GUARDED_BY(mutex_);
  std::vector<int> import_id_to_num_imported_ ABSL_GUARDED_BY(mutex_);

  // Just for reporting at the end of the solve.
  struct Stats {
    int64_t num_new = 0;
    int64_t num_update = 0;
    int64_t num_imported = 0;  // Copy of import_id_to_num_imported_.
    bool empty() const {
      return num_new == 0 && num_update == 0 && num_imported == 0;
    }
  };
  std::vector<Stats> id_to_stats_ ABSL_GUARDED_BY(mutex_);
  std::vector<std::string> id_to_worker_name_ ABSL_GUARDED_BY(mutex_);
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
std::shared_ptr<const typename SharedSolutionRepository<ValueType>::Solution>
SharedSolutionRepository<ValueType>::GetSolution(int i) const {
  absl::MutexLock mutex_lock(&mutex_);
  if (i >= solutions_.size()) return nullptr;
  ++num_queried_;
  return solutions_[i];
}

template <typename ValueType>
int64_t SharedSolutionRepository<ValueType>::GetBestRank() const {
  absl::MutexLock mutex_lock(&mutex_);
  if (solutions_.empty()) return std::numeric_limits<int64_t>::max();
  return solutions_[0]->rank;
}

template <typename ValueType>
std::vector<std::shared_ptr<
    const typename SharedSolutionRepository<ValueType>::Solution>>
SharedSolutionRepository<ValueType>::GetBestNSolutions(int n) const {
  absl::MutexLock mutex_lock(&mutex_);
  // Sorted by rank and unique.
  DCHECK(absl::c_is_sorted(solutions_,
                           [](const std::shared_ptr<const Solution>& a,
                              const std::shared_ptr<const Solution>& b) {
                             return a->rank < b->rank;
                           }));
  DCHECK(absl::c_adjacent_find(solutions_,
                               [](const std::shared_ptr<const Solution>& a,
                                  const std::shared_ptr<const Solution>& b) {
                                 return *a == *b;
                               }) == solutions_.end());
  std::vector<std::shared_ptr<const Solution>> result;
  const int num_solutions = std::min(static_cast<int>(solutions_.size()), n);
  result.reserve(num_solutions);
  for (int i = 0; i < num_solutions; ++i) {
    result.push_back(solutions_[i]);
  }
  return result;
}

template <typename ValueType>
ValueType SharedSolutionRepository<ValueType>::GetVariableValueInSolution(
    int var_index, int solution_index) const {
  absl::MutexLock mutex_lock(&mutex_);
  return solutions_[solution_index]->variable_values[var_index];
}

// TODO(user): Experiments on the best distribution.
template <typename ValueType>
std::shared_ptr<const typename SharedSolutionRepository<ValueType>::Solution>
SharedSolutionRepository<ValueType>::GetRandomBiasedSolution(
    absl::BitGenRef random) const {
  absl::MutexLock mutex_lock(&mutex_);
  if (solutions_.empty()) return nullptr;
  ++num_queried_;
  int index = 0;

  if (solutions_.size() > 1) {
    const int64_t best_rank = solutions_[0]->rank;

    // As long as we have solution with the best objective that haven't been
    // explored too much, we select one uniformly. Otherwise, we select a
    // solution from the pool uniformly.
    //
    // Note(user): Because of the increase of num_selected, this is dependent on
    // the order of call. It should be fine for "determinism" because we do
    // generate the task of a batch always in the same order.
    const int kExplorationThreshold = 100;

    // Select all the best solution with a low enough selection count.
    tmp_indices_.clear();
    for (int i = 0; i < solutions_.size(); ++i) {
      std::shared_ptr<const Solution> solution = solutions_[i];
      if (solution->rank == best_rank &&
          solution->num_selected <= kExplorationThreshold) {
        tmp_indices_.push_back(i);
      }
    }

    if (tmp_indices_.empty()) {
      index = absl::Uniform<int>(random, 0, solutions_.size());
    } else {
      index = tmp_indices_[absl::Uniform<int>(random, 0, tmp_indices_.size())];
    }
  }

  CHECK_GE(index, 0);
  CHECK_LT(index, solutions_.size());
  solutions_[index]->num_selected++;
  return solutions_[index];
}

template <typename ValueType>
std::shared_ptr<const typename SharedSolutionRepository<ValueType>::Solution>
SharedSolutionRepository<ValueType>::Add(Solution solution) {
  std::shared_ptr<Solution> solution_ptr =
      std::make_shared<Solution>(std::move(solution));
  if (num_solutions_to_keep_ <= 0) return std::move(solution_ptr);
  {
    absl::MutexLock mutex_lock(&mutex_);
    ++num_added_;
    solution_ptr->source_id = source_id_;
    new_solutions_.push_back(solution_ptr);
  }
  return solution_ptr;
}

template <typename ValueType>
void SharedSolutionRepository<ValueType>::Synchronize(
    std::function<void(const Solution& solution)> f) {
  absl::MutexLock mutex_lock(&mutex_);
  if (new_solutions_.empty()) {
    const int64_t diff = num_queried_ - num_queried_at_last_sync_;
    num_non_improving_ += diff;
    num_queried_at_last_sync_ = num_queried_;
    return;
  }

  if (f != nullptr) {
    gtl::STLStableSortAndRemoveDuplicates(
        &new_solutions_,
        [](const std::shared_ptr<Solution>& a,
           const std::shared_ptr<Solution>& b) { return *a < *b; });
    for (const auto& ptr : new_solutions_) {
      f(*ptr);
    }
  }

  const int64_t old_best_rank = solutions_.empty()
                                    ? std::numeric_limits<int64_t>::max()
                                    : solutions_[0]->rank;

  solutions_.insert(solutions_.end(), new_solutions_.begin(),
                    new_solutions_.end());
  new_solutions_.clear();

  // We use a stable sort to keep the num_selected count for the already
  // existing solutions (in case of duplicates).
  gtl::STLStableSortAndRemoveDuplicates(
      &solutions_, [](const std::shared_ptr<Solution>& a,
                      const std::shared_ptr<Solution>& b) { return *a < *b; });
  const int64_t new_best_rank = solutions_[0]->rank;

  // If we have more than num_solutions_to_keep_ solutions with the best rank,
  // select them via orthogonality.
  if (solutions_.size() > num_solutions_to_keep_ &&
      num_solutions_to_keep_ > 1) {
    int num_best = 1;
    while (num_best < solutions_.size() &&
           solutions_[num_best]->rank == new_best_rank) {
      ++num_best;
    }

    if (num_best > num_solutions_to_keep_ && num_solutions_to_keep_ < 10) {
      // We should only be here if a new solution (not in our current set) was
      // found. It could be one we saw before but forgot about. We put one
      // first.
      for (auto& solution : solutions_) {
        if (solution->num_selected == 0) {
          // TODO(user): randomize amongst new solution?
          std::swap(solutions_[0], solution);
          break;
        }
      }

      // We are going to be in O(n^2 * solution_size), so keep n <= 10.
      solutions_.resize(std::min(10, num_best));

      // Fill the pairwise distances.
      const int n = solutions_.size();
      distances_.resize(n * n);
      const int size = solutions_[0]->variable_values.size();
      for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
          int64_t dist = 0;
          for (int k = 0; k < size; ++k) {
            if (solutions_[i]->variable_values[k] !=
                solutions_[j]->variable_values[k]) {
              ++dist;
            }
          }
          distances_[i * n + j] = distances_[j * n + i] = dist;
        }
      }

      // In order to not get stuck on a subset that always maximize the sum of
      // orthogonality, we pick the first element (which should be a new one
      // thanks to the swap above), and we maximize the sum of orthogonality
      // with the rest.
      //
      // This way, as we find new solution, the set changes slowly.
      const std::vector<int> selected =
          FindMostDiverseSubset(num_solutions_to_keep_, n, distances_, buffer_,
                                /*always_pick_mask = */ 1);

      DCHECK(std::is_sorted(selected.begin(), selected.end()));
      int new_size = 0;
      for (const int s : selected) {
        solutions_[new_size++] = std::move(solutions_[s]);
      }
      solutions_.resize(new_size);

      if (VLOG_IS_ON(3)) {
        int min_count = std::numeric_limits<int>::max();
        int max_count = 0;
        for (const auto& s : solutions_) {
          CHECK(s != nullptr);
          min_count = std::min(s->num_selected, min_count);
          max_count = std::max(s->num_selected, max_count);
        }
        int64_t score = 0;
        for (const int i : selected) {
          for (const int j : selected) {
            if (i > j) score += distances_[i * n + j];
          }
        }
        LOG(INFO) << name_ << " rank=" << new_best_rank
                  << " num=" << num_solutions_to_keep_ << "/" << num_best
                  << " orthogonality=" << score << " count=[" << min_count
                  << ", " << max_count << "]";
      }
    }
  }

  if (solutions_.size() > num_solutions_to_keep_) {
    solutions_.resize(num_solutions_to_keep_);
  }
  CHECK(!solutions_.empty());
  if (!solutions_.empty()) {
    VLOG(4) << "Solution pool update:" << " num_solutions=" << solutions_.size()
            << " min_rank=" << solutions_[0]->rank
            << " max_rank=" << solutions_.back()->rank;
  }

  num_synchronization_++;
  if (new_best_rank < old_best_rank) {
    num_non_improving_ = 0;
  } else {
    const int64_t diff = num_queried_ - num_queried_at_last_sync_;
    num_non_improving_ += diff;
  }
  num_queried_at_last_sync_ = num_queried_;
}

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_SYNCHRONIZATION_H_
