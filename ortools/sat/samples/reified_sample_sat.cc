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

// [START program]
#include <stdlib.h>

#include "absl/log/globals.h"
#include "absl/types/span.h"
#include "ortools/base/init_google.h"
#include "ortools/base/log_severity.h"
#include "ortools/sat/cp_model.h"

namespace operations_research {
namespace sat {

void ReifiedSampleSat() {
  CpModelBuilder cp_model;

  const BoolVar x = cp_model.NewBoolVar();
  const BoolVar y = cp_model.NewBoolVar();
  const BoolVar b = cp_model.NewBoolVar();

  // First version using a half-reified bool and.
  cp_model.AddBoolAnd({x, ~y}).OnlyEnforceIf(b);

  // Second version using implications.
  cp_model.AddImplication(b, x);
  cp_model.AddImplication(b, ~y);

  // Third version using bool or.
  cp_model.AddBoolOr({~b, x});
  cp_model.AddBoolOr({~b, ~y});
}

}  // namespace sat
}  // namespace operations_research

int main(int argc, char* argv[]) {
  InitGoogle(argv[0], &argc, &argv, true);
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);
  operations_research::sat::ReifiedSampleSat();
  return EXIT_SUCCESS;
}
// [END program]
