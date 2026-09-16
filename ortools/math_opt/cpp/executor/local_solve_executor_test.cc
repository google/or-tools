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

#include "ortools/math_opt/cpp/executor/local_solve_executor.h"

#include <memory>

#include "gtest/gtest.h"
#include "ortools/math_opt/cpp/executor/solve_executor_tests.h"

namespace operations_research::math_opt {
namespace {

INSTANTIATE_TEST_SUITE_P(
    LocalSolveExecutorTests, SolveExecutorTest,
    testing::Values(SolveExecutorTestParameters{
        .name = "local_solve",
        .executor_provider =
            []() { return std::make_unique<LocalSolveExecutor>(); },
        .features = {.interruption = true,
                     .solve_callback = true,
                     .actual_incrementalism = true}}));

}  // namespace

}  // namespace operations_research::math_opt
