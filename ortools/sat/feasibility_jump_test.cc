// Copyright 2010-2024 Google LLC
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

#include "ortools/sat/feasibility_jump.h"

#include <cstdint>
#include <utility>

#include "gtest/gtest.h"

namespace operations_research::sat {
namespace {

TEST(JumpTableTest, TestCachesCalls) {
  int num_calls = 0;
  JumpTable jumps;
  jumps.SetComputeFunction(
      [&](int) { return std::make_pair(++num_calls, -1.0); });
  jumps.RecomputeAll(1);

  EXPECT_EQ(jumps.GetJump(0), std::make_pair(int64_t{1}, -1.0));
  EXPECT_EQ(jumps.GetJump(0), std::make_pair(int64_t{1}, -1.0));
  EXPECT_EQ(num_calls, 1);
}

TEST(JumpTableTest, TestNeedsRecomputationOneVar) {
  int num_calls = 0;
  JumpTable jumps;
  jumps.SetComputeFunction(
      [&](int) { return std::make_pair(++num_calls, -1.0); });
  jumps.RecomputeAll(1);

  jumps.GetJump(0);
  jumps.Recompute(0);

  EXPECT_EQ(jumps.GetJump(0), std::make_pair(int64_t{2}, -1.0));
  EXPECT_EQ(num_calls, 2);
}

TEST(JumpTableTest, TestNeedsRecomputationMultiVar) {
  int num_calls = 0;
  JumpTable jumps;
  jumps.SetComputeFunction(
      [&](int v) { return std::make_pair(++num_calls, v); });
  jumps.RecomputeAll(2);

  jumps.GetJump(0);
  jumps.GetJump(1);
  jumps.Recompute(0);

  EXPECT_EQ(jumps.GetJump(0), std::make_pair(int64_t{3}, 0.0));
  EXPECT_EQ(jumps.GetJump(1), std::make_pair(int64_t{2}, 1.0));
  EXPECT_EQ(num_calls, 3);
}

TEST(JumpTableTest, TestVarsNeedingRecomputePossiblyGood) {
  int num_calls = 0;
  JumpTable jumps;
  jumps.SetComputeFunction(
      [&](int) { return std::make_pair(++num_calls, 1.0); });
  jumps.RecomputeAll(1);

  EXPECT_TRUE(jumps.NeedRecomputation(0));
  EXPECT_EQ(num_calls, 0);
}

TEST(JumpTableTest, TestSetJump) {
  int num_calls = 0;
  JumpTable jumps;
  jumps.SetComputeFunction(
      [&](int) { return std::make_pair(++num_calls, -1.0); });
  jumps.RecomputeAll(1);

  jumps.SetJump(0, 1, 1.0);

  EXPECT_FALSE(jumps.NeedRecomputation(0));
  EXPECT_GE(jumps.Score(0), 0);
  EXPECT_EQ(jumps.GetJump(0), std::make_pair(int64_t{1}, 1.0));
  EXPECT_EQ(num_calls, 0);
}

}  // namespace
}  // namespace operations_research::sat
