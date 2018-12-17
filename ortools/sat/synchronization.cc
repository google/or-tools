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

#include "ortools/sat/synchronization.h"

#include "absl/container/flat_hash_set.h"

namespace operations_research {
namespace sat {

SharedBoundsManager::SharedBoundsManager(int num_workers, int num_variables)
    : num_workers_(num_workers),
      num_variables_(num_variables),
      changed_variables_per_workers_(num_workers),
      lower_bounds_(num_variables, kint64min),
      upper_bounds_(num_variables, kint64max) {
  for (int i = 0; i < num_workers_; ++i) {
    changed_variables_per_workers_[i].ClearAndResize(num_variables_);
  }
}

void SharedBoundsManager::ReportPotentialNewBounds(
    const std::vector<int>& variables,
    const std::vector<int64>& new_lower_bounds,
    const std::vector<int64>& new_upper_bounds, int worker_id,
    const std::string& worker_name) {
  CHECK_EQ(variables.size(), new_lower_bounds.size());
  CHECK_EQ(variables.size(), new_upper_bounds.size());
  {
    absl::MutexLock mutex_lock(&mutex_);
    int modified_domains = 0;
    int fixed_domains = 0;
    for (int i = 0; i < variables.size(); ++i) {
      const int var = variables[i];
      if (var >= num_variables_) continue;
      const int64 new_lb = new_lower_bounds[i];
      const int64 new_ub = new_upper_bounds[i];
      CHECK_GE(var, 0);
      bool changed = false;
      if (lower_bounds_[var] < new_lb) {
        changed = true;
        lower_bounds_[var] = new_lb;
      }
      if (upper_bounds_[var] > new_ub) {
        changed = true;
        upper_bounds_[var] = new_ub;
      }
      if (changed) {
        if (lower_bounds_[var] == upper_bounds_[var]) {
          fixed_domains++;
        } else {
          modified_domains++;
        }
        for (int j = 0; j < num_workers_; ++j) {
          if (worker_id == j) continue;
          changed_variables_per_workers_[j].Set(var);
        }
      }
    }
    if (fixed_domains > 0 || modified_domains > 0) {
      VLOG(1) << "Worker " << worker_name << ": fixed domains=" << fixed_domains
              << ", modified domains=" << modified_domains << " out of "
              << variables.size() << " events";
    }
  }
}

// When called, returns the set of bounds improvements since
// the last time this method was called by the same worker.
void SharedBoundsManager::GetChangedBounds(
    int worker_id, std::vector<int>* variables,
    std::vector<int64>* new_lower_bounds,
    std::vector<int64>* new_upper_bounds) {
  variables->clear();
  new_lower_bounds->clear();
  new_upper_bounds->clear();
  {
    absl::MutexLock mutex_lock(&mutex_);
    for (const int var :
         changed_variables_per_workers_[worker_id].PositionsSetAtLeastOnce()) {
      variables->push_back(var);
      new_lower_bounds->push_back(lower_bounds_[var]);
      new_upper_bounds->push_back(upper_bounds_[var]);
    }
    changed_variables_per_workers_[worker_id].ClearAll();
  }
}

}  // namespace sat
}  // namespace operations_research
