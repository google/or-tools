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

// A simple example where we look at how SCIP handles incremental bound updates
// going between variable types. SCIP can sometimes disregard bounds you pass it
// when going from binary (or integer) to continuous, but this appears to only
// happen when using the incremental bound updating API.
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
      0.25, 1.25, 1.0, operations_research::GScipVarType::kBinary, "x");
  QCHECK_OK(x_or);
  SCIP_VAR* const x = *x_or;

  // SCIP keeps the original, fractional bounds as-is.
  QCHECK(gscip->VarType(x) == operations_research::GScipVarType::kBinary);
  QCHECK_EQ(gscip->Lb(x), 0.25);
  QCHECK_EQ(gscip->Ub(x), 1.25);

  // We recover the original, fractional bounds after changing variable types.
  QCHECK_OK(
      gscip->SetVarType(x, operations_research::GScipVarType::kContinuous));
  QCHECK(gscip->VarType(x) == operations_research::GScipVarType::kContinuous);
  QCHECK_EQ(gscip->Lb(x), 0.25);
  QCHECK_EQ(gscip->Ub(x), 1.25);

  // Without error, we change the vartype and bounds back to what they were
  // originally (but now through the incremental API, not construction).
  QCHECK_OK(gscip->SetVarType(x, operations_research::GScipVarType::kBinary));
  QCHECK_OK(gscip->SetLb(x, 0.25));
  QCHECK_OK(gscip->SetUb(x, 1.25));

  // The fractional bounds are lost, replaced by rounded values.
  QCHECK_OK(
      gscip->SetVarType(x, operations_research::GScipVarType::kContinuous));
  QCHECK(gscip->VarType(x) == operations_research::GScipVarType::kContinuous);
  QCHECK_EQ(gscip->Lb(x), 1.0);
  QCHECK_EQ(gscip->Ub(x), 1.0);
}
