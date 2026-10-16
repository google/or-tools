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

#include "ortools/math_opt/validators/solve_stats_validator.h"

#include "absl/status/status.h"
#include "google/protobuf/duration.pb.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/result.pb.h"

namespace operations_research {
namespace math_opt {
namespace {

using ::testing::AllOf;
using ::testing::HasSubstr;
using ::testing::status::StatusIs;

////////////////////////////////////////////////////////////////////////////////
// ValidateSolveStats
////////////////////////////////////////////////////////////////////////////////

TEST(SolveStatsValidatorTest, InvalidSolveTime) {
  SolveStatsProto solve_stats;
  solve_stats.mutable_solve_time()->set_seconds(1);
  solve_stats.mutable_solve_time()->set_nanos(-1);
  EXPECT_THAT(ValidateSolveStats(solve_stats),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("invalid solve_time"),
                             HasSubstr("sign mismatch"))));
}

TEST(SolveStatsValidatorTest, NegativeSolveTime) {
  SolveStatsProto solve_stats;
  solve_stats.mutable_solve_time()->set_seconds(-1);
  EXPECT_THAT(ValidateSolveStats(solve_stats),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("solve_time must be non-negative")));
}

TEST(SolveStatsValidatorTest, NegativeSimplexIterations) {
  SolveStatsProto solve_stats;
  solve_stats.set_simplex_iterations(-1);
  EXPECT_THAT(ValidateSolveStats(solve_stats),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("simplex_iterations must be non-negative")));
}

TEST(SolveStatsValidatorTest, NegativeBarrierIterations) {
  SolveStatsProto solve_stats;
  solve_stats.set_barrier_iterations(-1);
  EXPECT_THAT(ValidateSolveStats(solve_stats),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("barrier_iterations must be non-negative")));
}

TEST(SolveStatsValidatorTest, NegativeNodeCount) {
  SolveStatsProto solve_stats;
  solve_stats.set_node_count(-1);
  EXPECT_THAT(ValidateSolveStats(solve_stats),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("node_count must be non-negative")));
}

}  // namespace
}  // namespace math_opt
}  // namespace operations_research
