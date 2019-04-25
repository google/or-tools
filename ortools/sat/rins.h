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

#ifndef OR_TOOLS_SAT_RINS_H_
#define OR_TOOLS_SAT_RINS_H_

#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/optional.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/linear_programming_constraint.h"
#include "ortools/sat/model.h"

namespace operations_research {
namespace sat {

// TODO(user): This is not an ideal place for the solution details. Move to a
// file with other such stats or create one if needed.
struct SolutionDetails {
  int64 solution_count = 0;
  gtl::ITIVector<IntegerVariable, IntegerValue> best_solution;

  // Loads the solution in best_solution using lower bounds from integer trail.
  void LoadFromTrail(const IntegerTrail& integer_trail);
};

// Collection of useful data for RINS for a given variable.
struct RINSVariable {
  IntegerVariable positive_var = kNoIntegerVariable;
  LinearProgrammingConstraint* lp = nullptr;
  int model_var;

  bool operator==(const RINSVariable other) const {
    return (positive_var == other.positive_var && lp == other.lp &&
            model_var == other.model_var);
  }
};

struct RINSVariables {
  std::vector<RINSVariable> vars;
};

struct RINSNeighborhood {
  std::vector<std::pair<RINSVariable, /*value*/ int64>> fixed_vars;
};

// Shared object to pass around generated RINS neighborhoods across workers.
class SharedRINSNeighborhoodManager {
 public:
  explicit SharedRINSNeighborhoodManager(const int64 num_model_vars)
      : num_model_vars_(num_model_vars) {}

  // Model will be used to get the CpModelMapping for translation. Returns
  // true for success.
  bool AddNeighborhood(const RINSNeighborhood& neighborhood);

  // Returns an unexplored RINS neighborhood if any, otherwise returns nullopt.
  // TODO(user): This also deletes the returned neighborhood. Consider having
  // a different method/logic instead for deletion.
  absl::optional<RINSNeighborhood> GetUnexploredNeighborhood();

  // This is the memory limit we use to avoid storing too many neighborhoods.
  // The sum of the fixed variables of the stored neighborhood will always
  // stays smaller than this.
  int64 max_fixed_vars() const { return 100 * num_model_vars_; }

 private:
  // Used while adding and removing neighborhoods.
  absl::Mutex mutex_;

  // TODO(user): Use better data structure (e.g. queue) to store this
  // collection.
  std::vector<RINSNeighborhood> neighborhoods_;

  // This is the sum of number of fixed variables across all the shared
  // neighborhoods. This is used for controlling the size of storage.
  int64 total_num_fixed_vars_ = 0;

  const int64 num_model_vars_;
};

// Helper method to create a RINS neighborhood by fixing variables with same
// values in lp and last solution.
void AddRINSNeighborhood(Model* model);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_RINS_H_
