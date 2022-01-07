// Copyright 2010-2021 Google LLC
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

void CopyModelSat() {
  // [START model]
  CpModelBuilder cp_model;
  // [END model]

  // [START variables]
  const Domain domain(0, 2);
  const IntVar x = cp_model.NewIntVar(domain).WithName("x");
  const IntVar y = cp_model.NewIntVar(domain).WithName("y");
  const IntVar z = cp_model.NewIntVar(domain).WithName("z");
  // [END variables]

  // [START constraints]
  cp_model.AddNotEqual(x, y);
  // [END constraints]

  // [START objective]
  cp_model.Maximize(x + 2 * y + 3 * z);
  // [END objective]

  const CpSolverResponse initial_response = Solve(cp_model.Build());
  LOG(INFO) << "Optimal value of the original model: "
            << initial_response.objective_value();

  CpModelBuilder copy;
  copy.CopyFrom(cp_model.Proto());

  // Add new constraint: copy_of_x + copy_of_y == 1.
  IntVar copy_of_x = copy.GetIntVarFromProtoIndex(x.index());
  IntVar copy_of_y = copy.GetIntVarFromProtoIndex(y.index());

  copy.AddLessOrEqual(copy_of_x + copy_of_y, 1);

  const CpSolverResponse modified_response = Solve(copy.Build());
  LOG(INFO) << "Optimal value of the modified model: "
            << modified_response.objective_value();
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::CopyModelSat();

  return EXIT_SUCCESS;
}
// [END program]
