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

#include "ortools/util/running_stat.h"

#include <algorithm>

#include "gtest/gtest.h"

namespace {

TEST(MovingAverageTest, InitialValues) {
  operations_research::RunningAverage running_average(10);
  EXPECT_EQ(0.0, running_average.GlobalAverage());
  EXPECT_EQ(0.0, running_average.WindowAverage());
  EXPECT_FALSE(running_average.IsWindowFull());
}

TEST(MovingAverageTest, WindowOfSizeOne) {
  operations_research::RunningAverage running_average(1);
  for (int i = 0; i < 1000; ++i) {
    running_average.Add(i);
    EXPECT_EQ(i, running_average.WindowAverage());
    EXPECT_EQ(static_cast<double>(i) / 2.0, running_average.GlobalAverage());
    EXPECT_TRUE(running_average.IsWindowFull());
  }
}

TEST(MovingAverageTest, WindowOfSizeTwo) {
  operations_research::RunningAverage running_average(2);
  for (int i = 0; i < 1000; ++i) {
    running_average.Add(i);
    if (i == 0) {
      EXPECT_FALSE(running_average.IsWindowFull());
    } else {
      EXPECT_EQ(static_cast<double>(2 * i - 1) / 2.0,
                running_average.WindowAverage());
      EXPECT_TRUE(running_average.IsWindowFull());
    }
  }
}

TEST(MovingAverageTest, ClearWindow) {
  operations_research::RunningAverage running_average(50);
  for (int i = 0; i < 1000; ++i) {
    running_average.Add(i);
  }
  EXPECT_TRUE(running_average.IsWindowFull());
  running_average.ClearWindow();
  EXPECT_FALSE(running_average.IsWindowFull());
  EXPECT_EQ(0.0, running_average.WindowAverage());
}

TEST(MovingAverageTest, Reset) {
  operations_research::RunningAverage running_average(50);
  for (int i = 0; i < 1000; ++i) {
    running_average.Add(i);
  }
  EXPECT_TRUE(running_average.IsWindowFull());
  running_average.Reset(1);
  EXPECT_EQ(0.0, running_average.GlobalAverage());
  EXPECT_EQ(0.0, running_average.WindowAverage());
  EXPECT_FALSE(running_average.IsWindowFull());
  for (int i = 0; i < 1000; ++i) {
    running_average.Add(i);
    EXPECT_EQ(i, running_average.WindowAverage());
    EXPECT_EQ(static_cast<double>(i) / 2.0, running_average.GlobalAverage());
    EXPECT_TRUE(running_average.IsWindowFull());
  }
}

TEST(RunningMaxTest, WindowOfSize1) {
  operations_research::RunningMax<> running_max(1);
  for (int i = 0; i < 1000; ++i) {
    const double value = i % 15;
    running_max.Add(value);
    EXPECT_EQ(value, running_max.GetCurrentMax());
  }
}

TEST(RunningMaxTest, SmallSequence) {
  operations_research::RunningMax<> running_max(2);
  running_max.Add(1);
  EXPECT_EQ(1, running_max.GetCurrentMax());
  running_max.Add(10);
  EXPECT_EQ(10, running_max.GetCurrentMax());
  running_max.Add(9);
  EXPECT_EQ(10, running_max.GetCurrentMax());
  running_max.Add(8);
  EXPECT_EQ(9, running_max.GetCurrentMax());
  running_max.Add(11);
  EXPECT_EQ(11, running_max.GetCurrentMax());
}

TEST(RunningMaxTest, IncreasingSequence) {
  operations_research::RunningMax<> running_max(10);
  for (int i = 0; i < 1000; ++i) {
    running_max.Add(i);
    EXPECT_EQ(i, running_max.GetCurrentMax());
  }
}

TEST(RunningMaxTest, DecreasingSequence) {
  operations_research::RunningMax<> running_max(10);
  for (int i = 1000; i >= 0; --i) {
    running_max.Add(i);
    EXPECT_EQ(std::min(1000, i + 9), running_max.GetCurrentMax());
  }
}

}  // namespace
