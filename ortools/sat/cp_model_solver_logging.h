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

#ifndef ORTOOLS_SAT_CP_MODEL_SOLVER_LOGGING_H_
#define ORTOOLS_SAT_CP_MODEL_SOLVER_LOGGING_H_

#include <string>

#include "absl/container/btree_map.h"
#include "ortools/base/timer.h"
#include "ortools/sat/model.h"
#include "ortools/sat/synchronization.h"
#include "ortools/util/logging.h"

namespace operations_research {
namespace sat {

// This class implements the standard logging of the solver progress to the
// SolverLogger object in the model, typically enabled by setting
// SatParameters.log_search_progress to true.
class SolverProgressLogger {
 public:
  explicit SolverProgressLogger(Model* model)
      : wall_timer_(*model->GetOrCreate<WallTimer>()),
        logger_(model->GetOrCreate<SolverLogger>()) {
    bounds_logging_id_ = logger_->GetNewThrottledId();
  }

  void UpdateProgress(const SolverStatusChangeInfo& info);

  void SetIsOptimization(bool is_optimization) {
    is_optimization_ = is_optimization;
  }

  void DisplayImprovementStatistics(SolverLogger* logger) const;

 private:
  void RegisterSolutionFound(const std::string& improvement_info,
                             int solution_number);
  void RegisterObjectiveBoundImprovement(const std::string& improvement_info);

  const WallTimer& wall_timer_;
  SolverLogger* logger_;
  bool is_optimization_ = false;
  int bounds_logging_id_;

  int num_solutions_ = 0;

  absl::btree_map<std::string, int> primal_improvements_count_;
  absl::btree_map<std::string, int> primal_improvements_min_rank_;
  absl::btree_map<std::string, int> primal_improvements_max_rank_;
  absl::btree_map<std::string, int> dual_improvements_count_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_CP_MODEL_SOLVER_LOGGING_H_
