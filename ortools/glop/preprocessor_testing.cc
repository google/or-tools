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

#include "ortools/glop/preprocessor_testing.h"

#include <optional>
#include <utility>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/glop/preprocessor.h"
#include "ortools/lp_data/lp_types.h"

namespace operations_research::glop {

testing::Matcher<Preprocessor::Result> PreprocessorResultIs(
    testing::Matcher<bool> postsolve_is_needed,
    const std::optional<ProblemStatus> status) {
  if (!status.has_value()) {
    return testing::AllOf(
        testing::Field("postsolve_is_needed",
                       &Preprocessor::Result::postsolve_is_needed,
                       std::move(postsolve_is_needed)),
        testing::Field("solve_status", &Preprocessor::Result::solve_status,
                       testing::Eq(std::nullopt)));
  }

  return testing::AllOf(
      testing::Field("postsolve_is_needed",
                     &Preprocessor::Result::postsolve_is_needed,
                     std::move(postsolve_is_needed)),
      testing::Field(
          "solve_status", &Preprocessor::Result::solve_status,
          testing::Optional(testing::Property(
              "problem_status", &SolveStatus::problem_status, *status))));
}

}  // namespace operations_research::glop
