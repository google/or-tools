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

#ifndef OR_TOOLS_SAT_RINS_H_
#define OR_TOOLS_SAT_RINS_H_

#include <cstdint>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/optional.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/linear_programming_constraint.h"
#include "ortools/sat/model.h"
#include "ortools/sat/synchronization.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

// Links IntegerVariable with model variable and its lp constraint if any.
struct LPVariable {
  IntegerVariable positive_var = kNoIntegerVariable;
  LinearProgrammingConstraint* lp = nullptr;
  int model_var;

  bool operator==(const LPVariable other) const {
    return (positive_var == other.positive_var && lp == other.lp &&
            model_var == other.model_var);
  }
};

// This is used to "cache" in the model the set of relevant LPVariable.
struct LPVariables {
  std::vector<LPVariable> vars;
  int model_vars_size = 0;
};

// A RINS Neighborhood is actually just a generic neighborhood where the domain
// of some variable have been reduced (fixed or restricted in [lb, ub]).
//
// Important: It might be possible that the value of the variables here are
// outside the domains of these variables! This happens for RENS type of
// neighborhood in the presence of holes in the domains because the LP
// relaxation ignore those.
struct RINSNeighborhood {
  // A variable will appear only once and not in both vectors.
  std::vector<std::pair</*model_var*/ int, /*value*/ int64_t>> fixed_vars;
  std::vector<
      std::pair</*model_var*/ int, /*domain*/ std::pair<int64_t, int64_t>>>
      reduced_domain_vars;
};

// Helper method to create a RINS neighborhood by fixing variables with same
// values in relaxation solution and the current best solution in the
// response_manager. Prioritizes repositories in following order to get a
// relaxation solution.
//  1. incomplete_solutions
//  2. lp_solutions
//  3. relaxation_solutions
//
// If response_manager is not provided, this generates a RENS neighborhood by
// ignoring the solutions and using the relaxation values. The domain of the
// variables are reduced to integer values around relaxation values. If the
// relaxation value is integer, then we fix the domain of the variable to that
// value.
RINSNeighborhood GetRINSNeighborhood(
    const SharedResponseManager* response_manager,
    const SharedRelaxationSolutionRepository* relaxation_solutions,
    const SharedLPSolutionRepository* lp_solutions,
    SharedIncompleteSolutionManager* incomplete_solutions,
    absl::BitGenRef random);

// Adds the current LP solution to the pool.
void RecordLPRelaxationValues(Model* model);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_RINS_H_
