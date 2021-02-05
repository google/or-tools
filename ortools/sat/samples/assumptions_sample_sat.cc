// Copyright 2021 Xiang Chen
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

// [START program]
#include "ortools/sat/cp_model.h"
#include "ortools/sat/model.h"

namespace operations_research {
namespace sat {

void AssumptionsSampleSat() {
  // [START model]
  CpModelBuilder cp_model;
  // [END model]

  // [START variables]
  const Domain domain(0, 10);
  const IntVar x = cp_model.NewIntVar(domain).WithName("x");
  const IntVar y = cp_model.NewIntVar(domain).WithName("y");
  const IntVar z = cp_model.NewIntVar(domain).WithName("z");
  const BoolVar a = cp_model.NewBoolVar().WithName("a");
  const BoolVar b = cp_model.NewBoolVar().WithName("b");
  const BoolVar c = cp_model.NewBoolVar().WithName("c");
  // [END variables]

  // [START constraints]
  cp_model.AddGreaterThan(x, y).OnlyEnforceIf(a);
  cp_model.AddGreaterThan(y, z).OnlyEnforceIf(b);
  cp_model.AddGreaterThan(z, x).OnlyEnforceIf(c);
  // [END constraints]

  // Add assumptions
  cp_model.AddAssumptions({a, b, c});

  // Solving part.
  // [START solve]
  const CpSolverResponse response = Solve(cp_model.Build());
  LOG(INFO) << CpSolverResponseStats(response);
  for (const int index : response.sufficient_assumptions_for_infeasibility()) {
    LOG(INFO) << index;
  }
  // [END solve]
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::AssumptionsSampleSat();

  return EXIT_SUCCESS;
}
// [END program]
