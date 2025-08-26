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

// A simple example where SCIP's promotion of [0, 1] integer variables to binary
// variables can lead to unexpected behaviors: either incorrect bounds, or an
// internal CHECK-fail in debug mode. The same as `binary_variable_bounds.cc`,
// except the variable is passed as a [0, 1] integer variable and immediately
// promoted to a binary variable, thus inheriting all the odd behaviors.
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
      0.0, 1.0, 0.0, operations_research::GScipVarType::kInteger, "x");
  QCHECK_OK(x_or);
  SCIP_VAR* const x = *x_or;

  QCHECK(gscip->VarType(x) == operations_research::GScipVarType::kBinary);

  // First we set the bounds to [0, 0]. This works fine, no crash.
  QCHECK_OK(gscip->SetLb(x, 0.0));
  QCHECK_OK(gscip->SetUb(x, 0.0));
  QCHECK_EQ(gscip->Lb(x), 0.0);
  QCHECK_EQ(gscip->Ub(x), 0.0);
  QCHECK(gscip->VarType(x) == operations_research::GScipVarType::kBinary);

  // Next we set the bounds to [1, 1]. Also fine without a crash.
  QCHECK_OK(gscip->SetLb(x, 1.0));
  QCHECK_OK(gscip->SetUb(x, 1.0));
  QCHECK_EQ(gscip->Lb(x), 1.0);
  QCHECK_EQ(gscip->Ub(x), 1.0);
  QCHECK(gscip->VarType(x) == operations_research::GScipVarType::kBinary);

  // We change bounds back to [0, 1]. Also fine without a crash.
  QCHECK_OK(gscip->SetLb(x, 0.0));
  QCHECK_OK(gscip->SetUb(x, 1.0));
  QCHECK_EQ(gscip->Lb(x), 0.0);
  QCHECK_EQ(gscip->Ub(x), 1.0);
  QCHECK(gscip->VarType(x) == operations_research::GScipVarType::kBinary);

  // Next we set the bounds to [0.25, 0.75]. This will not crash, but sets the
  // bounds to unexpected (but mathematically equivalent) values.
  QCHECK_OK(gscip->SetLb(x, 0.25));
  QCHECK_OK(gscip->SetUb(x, 0.75));
  QCHECK_EQ(gscip->Lb(x), 1.0);
  QCHECK_EQ(gscip->Ub(x), 0.0);
  QCHECK(gscip->VarType(x) == operations_research::GScipVarType::kBinary);

  // Setting the bounds to [0, 2] will CHECK-fail in debug model.
  gscip->SetUb(x, 2.0).IgnoreError();
}
