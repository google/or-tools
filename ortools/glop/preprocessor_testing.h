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

// Testing tool for glop::Preprocessor implementations.
#ifndef ORTOOLS_GLOP_PREPROCESSOR_TESTING_H_
#define ORTOOLS_GLOP_PREPROCESSOR_TESTING_H_

#include <optional>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/glop/preprocessor.h"
#include "ortools/lp_data/lp_types.h"

namespace operations_research::glop {

// Returns a matcher for a Preprocessor::Result.
testing::Matcher<Preprocessor::Result> PreprocessorResultIs(
    testing::Matcher<bool> postsolve_is_needed,
    std::optional<ProblemStatus> status);

}  // namespace operations_research::glop

#endif  // ORTOOLS_GLOP_PREPROCESSOR_TESTING_H_
