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

namespace operations_research {
namespace sat {

// Manages the global best response kept by the solver.
// All functions are thread-safe.
class SharedResponseManager {
 public:
  // If log_updates is true, then all updates to the global "state" will be
  // logged. This class is responsible for our solver log progress.
  explicit SharedResponseManager(bool log_updates_, const CpModelProto* proto,
                                 const WallTimer* wall_timer);

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

 private:
  void FillObjectiveValuesInBestResponse();
  void SetStatsFromModelInternal(Model* model);

  const bool log_updates_;
  const CpModelProto& model_proto_;
  const WallTimer& wall_timer_;

  absl::Mutex mutex_;

  CpSolverResponse best_response_;

  int num_solutions_ = 0;
  int64 inner_objective_lower_bound_ = kint64min;
  int64 inner_objective_upper_bound_ = kint64max;
  int64 best_solution_objective_value_ = kint64max;

  int next_callback_id_ = 0;
  std::vector<std::pair<int, std::function<void(const CpSolverResponse&)>>>
      callbacks_;
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
  std::vector<SparseBitset<int64>> changed_variables_per_workers_;
  std::vector<int64> lower_bounds_;
  std::vector<int64> upper_bounds_;
  absl::Mutex mutex_;
};

// Registers a callback to import new variables bounds stored in the
// shared_bounds_manager. These bounds are imported at level 0 of the search
// in the linear scan minimize function.
void RegisterVariableBoundsLevelZeroImport(
    const CpModelProto& model_proto, SharedBoundsManager* shared_bounds_manager,
    Model* model);

// Registers a callback that will export variables bounds fixed at level 0 of
// the search. This should not be registered to a LNS search.
void RegisterVariableBoundsLevelZeroExport(
    const CpModelProto& model_proto, SharedBoundsManager* shared_bounds_manager,
    Model* model);

// Registers a callback to import new objective bounds.
//
// Currently, standard search works fine with it.
// LNS search and Core based search do not support it
void RegisterObjectiveBoundsImport(
    SharedResponseManager* shared_response_manager, Model* model);

// Registers a callback that will report improving objective best bound.
void RegisterObjectiveBestBoundExport(
    IntegerVariable objective_var,
    SharedResponseManager* shared_response_manager, Model* model);

// Stores information on the worker in the parallel context.
struct WorkerInfo {
  std::string worker_name;
  int worker_id = -1;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_SYNCHRONIZATION_H_
