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

#include "ortools/sat/cp_model.h"

namespace operations_research {
namespace sat {

void OptionalIntervalSampleSat() {
  CpModelBuilder cp_model;
  const int kHorizon = 100;

  const Domain horizon(0, kHorizon);
  const IntVar start_var = cp_model.NewIntVar(horizon).WithName("start");
  const IntVar duration_var = cp_model.NewConstant(10);
  const IntVar end_var = cp_model.NewIntVar(horizon).WithName("end");
  const BoolVar presence_var = cp_model.NewBoolVar().WithName("presence");

  const IntervalVar interval_var =
      cp_model
          .NewOptionalIntervalVar(start_var, duration_var, end_var,
                                  presence_var)
          .WithName("interval");

  LOG(INFO) << "start_var = " << start_var
            << ", duration_var = " << duration_var << ", end_var = " << end_var
            << ", presence_var = " << presence_var
            << ", interval_var = " << interval_var;
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::OptionalIntervalSampleSat();

  return EXIT_SUCCESS;
}
