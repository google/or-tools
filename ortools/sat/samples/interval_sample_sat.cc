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

void IntervalSampleSat() {
  CpModelBuilder cp_model;
  const int kHorizon = 100;
  const Domain horizon(0, kHorizon);

  // An interval can be created from three affine expressions.
  const IntVar x = cp_model.NewIntVar(horizon).WithName("x");
  const IntVar y = cp_model.NewIntVar({2, 4}).WithName("y");
  const IntVar z = cp_model.NewIntVar(horizon).WithName("z");

  const IntervalVar interval_var =
      cp_model.NewIntervalVar(x, y, LinearExpr(z).AddConstant(2))
          .WithName("interval");
  LOG(INFO) << "start = " << interval_var.StartExpr()
            << ", size = " << interval_var.SizeExpr()
            << ", end = " << interval_var.EndExpr()
            << ", interval_var = " << interval_var;

  // If the size is fixed, a simpler version uses the start expression and the
  // size.
  const IntervalVar fixed_size_interval_var =
      cp_model.NewFixedSizeIntervalVar(x, 10).WithName(
          "fixed_size_interval_var");
  LOG(INFO) << "start = " << fixed_size_interval_var.StartExpr()
            << ", size = " << fixed_size_interval_var.SizeExpr()
            << ", end = " << fixed_size_interval_var.EndExpr()
            << ", fixed_size_interval_var = " << fixed_size_interval_var;

  // A fixed interval can be created using the same API.
  const IntervalVar fixed_interval =
      cp_model.NewFixedSizeIntervalVar(5, 10).WithName("fixed_interval");
  LOG(INFO) << "start = " << fixed_interval.StartExpr()
            << ", size = " << fixed_interval.SizeExpr()
            << ", end = " << fixed_interval.EndExpr()
            << ", fixed_interval = " << fixed_interval;
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::IntervalSampleSat();

  return EXIT_SUCCESS;
}
