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

// A simple example where an integer variable with fractional bounds whose only
// feasible value is 0 or 1 is silently converted to a binary variable. This
// leads to a debug crash if you try to widen the bounds.
#include <memory>
#include <utility>

#include "absl/log/check.h"
#include "ortools/base/init_google.h"
#include "ortools/math_opt/solvers/gscip/gscip.h"

int main(int argc, char* argv[]) {
  InitGoogle("", &argc, &argv, true);

  auto gscip_or = operations_research::GScip::Create("");
  std::unique_ptr<operations_research::GScip> gscip = *std::move(gscip_or);

  const auto x_or = gscip->AddVariable(
      0.5, 1.5, 0.0, operations_research::GScipVarType::kInteger, "x");
  CHECK_OK(x_or);
  SCIP_VAR* const x = *x_or;

  QCHECK(gscip->VarType(x) == operations_research::GScipVarType::kBinary);
  QCHECK_EQ(gscip->Lb(x), 0.5);
  QCHECK_EQ(gscip->Ub(x), 1.5);

  // Setting the bounds to [0, 2] will CHECK-fail in debug model.
  gscip->SetUb(x, 2.0).IgnoreError();

  // Similarly, updating the lower bound to -1 will CHECK-fail in debug model.
  gscip->SetLb(x, -1.0).IgnoreError();
}
