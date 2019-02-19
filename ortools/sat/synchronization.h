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

// This class manages a pool of lower and upper bounds on a set of variables in
// a parallel context.
class SharedBoundsManager {
 public:
  SharedBoundsManager(int num_workers, int num_variables);

  // Reports a set of locally improved variable bounds to the shared bounds
  // manager. The manager will compare these bounds changes against its
  // global state, and incorporate the improving ones.
  void ReportPotentialNewBounds(int worker_id,
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

// Registers a callback to import new objective bounds. It will use callbacks
// stored in the ObjectiveSynchronizationHelper to query external objective
// bounds.
//
// Currently, standard search works fine with it.
// LNS search and Core based search do not support it
void RegisterObjectiveBoundsImport(Model* model);

// Registers a callback that will report improving objective best bound.
//
// TODO(user): A solver can also improve the objective upper bound without
// finding a solution and we should maybe share this as well.
void RegisterObjectiveBestBoundExport(const CpModelProto& model_proto,
                                      bool log_progress,
                                      IntegerVariable objective_var,
                                      WallTimer* wall_timer, Model* model);

// Stores information on the worker in the parallel context.
struct WorkerInfo {
  std::string worker_name;
  int worker_id = -1;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_SYNCHRONIZATION_H_
