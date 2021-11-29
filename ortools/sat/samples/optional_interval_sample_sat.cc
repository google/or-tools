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

#include "ortools/sat/cp_model.h"

namespace operations_research {
namespace sat {

void OptionalIntervalSampleSat() {
  CpModelBuilder cp_model;
  const int kHorizon = 100;
  const Domain horizon(0, kHorizon);

  // An optional interval can be created from three affine expressions and a
  // BoolVar.
  const IntVar x = cp_model.NewIntVar(horizon).WithName("x");
  const IntVar y = cp_model.NewIntVar({2, 4}).WithName("y");
  const IntVar z = cp_model.NewIntVar(horizon).WithName("z");
  const BoolVar presence_var = cp_model.NewBoolVar().WithName("presence");

  const IntervalVar interval_var =
      cp_model
          .NewOptionalIntervalVar(x, y, LinearExpr(z).AddConstant(2),
                                  presence_var)
          .WithName("interval");
  LOG(INFO) << "start = " << interval_var.StartExpr()
            << ", size = " << interval_var.SizeExpr()
            << ", end = " << interval_var.EndExpr()
            << ", presence = " << interval_var.PresenceBoolVar()
            << ", interval_var = " << interval_var;

  // If the size is fixed, a simpler version uses the start expression and the
  // size.
  const IntervalVar fixed_size_interval_var =
      cp_model.NewOptionalFixedSizeIntervalVar(x, 10, presence_var)
          .WithName("fixed_size_interval_var");
  LOG(INFO) << "start = " << fixed_size_interval_var.StartExpr()
            << ", size = " << fixed_size_interval_var.SizeExpr()
            << ", end = " << fixed_size_interval_var.EndExpr()
            << ", presence = " << fixed_size_interval_var.PresenceBoolVar()
            << ", interval_var = " << fixed_size_interval_var;
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::OptionalIntervalSampleSat();

  return EXIT_SUCCESS;
}
