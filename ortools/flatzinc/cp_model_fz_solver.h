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

#ifndef ORTOOLS_FLATZINC_CP_MODEL_FZ_SOLVER_H_
#define ORTOOLS_FLATZINC_CP_MODEL_FZ_SOLVER_H_

#include <string>

#include "ortools/flatzinc/model.h"
#include "ortools/util/logging.h"

namespace operations_research {
namespace fz {

struct FlatzincSatParameters {
  bool search_all_solutions = false;
  bool display_all_solutions = false;
  bool use_free_search = false;
  bool log_search_progress = false;
  bool display_statistics = false;
  int random_seed = 0;
  int number_of_threads = 0;
  double max_time_in_seconds = 0.0;
  bool ortools_mode = false;
};

}  // namespace fz

namespace sat {

// Scan the model to replace all int2float to int_eq, and all floating point
// variables used in these int2float constraint to be integral.
//
// Scan the model to find a floating point objective (defined by a single
// floating point variable and a single float_lin_eq constraint defining it),
// and replace them by a single objective with integer variables and floating
// point weights.
void ProcessFloatingPointOVariablesAndObjective(fz::Model* fz_model);

// Solves the given flatzinc model using the CP-SAT solver.
void SolveFzWithCpModelProto(const fz::Model& model,
                             const fz::FlatzincSatParameters& p,
                             const std::string& sat_params,
                             SolverLogger* logger,
                             SolverLogger* solution_logger);

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_FLATZINC_CP_MODEL_FZ_SOLVER_H_
