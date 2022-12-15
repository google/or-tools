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

#ifndef OR_TOOLS_SAT_PSEUDO_COSTS_H_
#define OR_TOOLS_SAT_PSEUDO_COSTS_H_

#include <vector>

#include "ortools/base/logging.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/util.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

// Pseudo cost of a variable is measured as average observed change in the
// objective bounds per unit change in the variable bounds.
class PseudoCosts {
 public:
  // Helper struct to get information relevant for pseudo costs from branching
  // decisions.
  struct VariableBoundChange {
    IntegerVariable var = kNoIntegerVariable;
    IntegerValue lower_bound_change = IntegerValue(0);
  };
  explicit PseudoCosts(Model* model);

  // Updates the pseudo costs for the given decision.
  void UpdateCost(const std::vector<VariableBoundChange>& bound_changes,
                  IntegerValue obj_bound_improvement);

  // Returns the variable with best reliable pseudo cost that is not fixed.
  IntegerVariable GetBestDecisionVar();

  // Returns the pseudo cost of given variable. Currently used for testing only.
  double GetCost(IntegerVariable var) const {
    CHECK_LT(var, pseudo_costs_.size());
    return pseudo_costs_[var].CurrentAverage();
  }

  // Returns the number of recordings of given variable. Currently used for
  // testing only.
  int GetRecordings(IntegerVariable var) const {
    CHECK_LT(var, pseudo_costs_.size());
    return pseudo_costs_[var].NumRecords();
  }

  // Returns extracted information to update pseudo costs from the given
  // branching decision.
  std::vector<VariableBoundChange> GetBoundChanges(Literal decision);

 private:
  // Updates the cost of a given variable.
  void UpdateCostForVar(IntegerVariable var, double new_cost);

  // Reference of integer trail to access the current bounds of variables.
  const SatParameters& parameters_;
  IntegerTrail* integer_trail_;
  IntegerEncoder* encoder_;

  absl::StrongVector<IntegerVariable, IncrementalAverage> pseudo_costs_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_PSEUDO_COSTS_H_
