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

#ifndef OR_TOOLS_FLATZINC_CP_MODEL_FZ_SOLVER_H_
#define OR_TOOLS_FLATZINC_CP_MODEL_FZ_SOLVER_H_

#include <string>

#include "ortools/flatzinc/model.h"

namespace operations_research {
namespace fz {

struct FlatzincSatParameters {
  bool display_all_solutions = false;
  bool use_free_search = false;
  bool log_search_progress = false;
  bool display_statistics = false;
  int random_seed = 0;
  int number_of_threads = 0;
  double max_time_in_seconds = 0.0;
};

}  // namespace fz

namespace sat {

void SolveFzWithCpModelProto(const fz::Model& model,
                             const fz::FlatzincSatParameters& p,
                             const std::string& sat_params,
                             SolverLogger* logger,
                             SolverLogger* solution_logger);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_FLATZINC_CP_MODEL_FZ_SOLVER_H_
