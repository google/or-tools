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

#ifndef OR_TOOLS_SAT_RINS_H_
#define OR_TOOLS_SAT_RINS_H_

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "absl/random/bit_gen_ref.h"
#include "ortools/sat/model.h"
#include "ortools/sat/synchronization.h"

namespace operations_research {
namespace sat {

// A RINS Neighborhood is actually just a generic neighborhood where the domain
// of some variable have been reduced (fixed or restricted in [lb, ub]).
//
// Important: It might be possible that the value of the variables here are
// outside the domains of these variables! This happens for RENS type of
// neighborhood in the presence of holes in the domains because the LP
// relaxation ignore those.
struct ReducedDomainNeighborhood {
  // A variable will appear only once and not in both vectors.
  std::vector<std::pair</*model_var*/ int, /*value*/ int64_t>> fixed_vars;
  std::vector<
      std::pair</*model_var*/ int, /*domain*/ std::pair<int64_t, int64_t>>>
      reduced_domain_vars;
  std::string source_info;
};

// Helper method to create a RINS neighborhood by fixing variables with same
// values in relaxation solution and the current best solution in the
// response_manager. Prioritizes repositories in following order to get a
// neighborhood.
//  1. incomplete_solutions
//  2. lp_solutions
//
// If response_manager has no solution, this generates a RENS neighborhood by
// ignoring the solutions and using the relaxation values. The domain of the
// variables are reduced to integer values around relaxation values. If the
// relaxation value is integer, then we fix the domain of the variable to that
// value.
ReducedDomainNeighborhood GetRinsRensNeighborhood(
    const SharedResponseManager* response_manager,
    const SharedLPSolutionRepository* lp_solutions,
    SharedIncompleteSolutionManager* incomplete_solutions, double difficulty,
    absl::BitGenRef random);

// Adds the current LP solution to the pool.
void RecordLPRelaxationValues(Model* model);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_RINS_H_
