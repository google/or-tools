// Copyright 2010-2021 Google LLC
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

// Test file for hungarian.h

#include "ortools/algorithms/hungarian.h"

#include <cstdint>

#include "absl/container/flat_hash_map.h"
#include "gtest/gtest.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/macros.h"
#include "ortools/base/map_util.h"

namespace operations_research {

// Generic check function that checks consistency of a linear assignment
// result as well as whether the result is the expected one.

void GenericCheck(const int expected_assignment_size,
                  const absl::flat_hash_map<int, int>& direct_assignment,
                  const absl::flat_hash_map<int, int>& reverse_assignment,
                  const int expected_agents[], const int expected_tasks[]) {
  EXPECT_EQ(expected_assignment_size, direct_assignment.size());
  EXPECT_EQ(expected_assignment_size, reverse_assignment.size());
  for (int i = 0; i < expected_assignment_size; ++i) {
    EXPECT_EQ(gtl::FindOrDie(direct_assignment, expected_agents[i]),
              expected_tasks[i]);
    EXPECT_EQ(gtl::FindOrDie(reverse_assignment, expected_tasks[i]),
              expected_agents[i]);
  }
  for (const auto& direct_iter : direct_assignment) {
    EXPECT_EQ(gtl::FindOrDie(reverse_assignment, direct_iter.second),
              direct_iter.first)
        << direct_iter.first << " -> " << direct_iter.second;
  }
}

void TestMinimization(const std::vector<std::vector<double>>& cost,
                      const int expected_assignment_size,
                      const int expected_agents[], const int expected_tasks[]) {
  absl::flat_hash_map<int, int> direct_assignment;
  absl::flat_hash_map<int, int> reverse_assignment;
  MinimizeLinearAssignment(cost, &direct_assignment, &reverse_assignment);
  SCOPED_TRACE("Minimization");
  GenericCheck(expected_assignment_size, direct_assignment, reverse_assignment,
               expected_agents, expected_tasks);
}

void TestMaximization(const std::vector<std::vector<double>>& cost,
                      const int expected_assignment_size,
                      const int expected_agents[], const int expected_tasks[]) {
  absl::flat_hash_map<int, int> direct_assignment;
  absl::flat_hash_map<int, int> reverse_assignment;
  MaximizeLinearAssignment(cost, &direct_assignment, &reverse_assignment);
  SCOPED_TRACE("Maximization");
  GenericCheck(expected_assignment_size, direct_assignment, reverse_assignment,
               expected_agents, expected_tasks);
}

// Test on an empty matrix

TEST(LinearAssignmentTest, NullMatrix) {
  std::vector<std::vector<double>> cost;
  const int* expected_agents = nullptr;
  const int* expected_tasks = nullptr;
  TestMinimization(cost, 0, expected_agents, expected_tasks);
  TestMaximization(cost, 0, expected_agents, expected_tasks);
}

// Testing with NaN value in the input.
TEST(LinearAssignmentTest, InvalidMatrix) {
  const std::vector<std::vector<double>> cost_nan = {{1, 2},
                                                     {-std::sqrt(-1), 3}};
  const int* expected_agents = nullptr;
  const int* expected_tasks = nullptr;
  TestMaximization(cost_nan, 0, expected_agents, expected_tasks);
  TestMinimization(cost_nan, 0, expected_agents, expected_tasks);
}

#define MATRIX_TEST                                                  \
  {                                                                  \
    std::vector<std::vector<double>> cost(kMatrixHeight);            \
    for (int row = 0; row < kMatrixHeight; ++row) {                  \
      cost[row].resize(kMatrixWidth);                                \
      for (int col = 0; col < kMatrixWidth; ++col) {                 \
        cost[row][col] = kCost[row][col];                            \
      }                                                              \
    }                                                                \
    EXPECT_EQ(arraysize(expected_agents_for_min),                    \
              arraysize(expected_tasks_for_min));                    \
    EXPECT_EQ(arraysize(expected_agents_for_max),                    \
              arraysize(expected_tasks_for_max));                    \
    const int assignment_size = arraysize(expected_agents_for_max);  \
    TestMinimization(cost, assignment_size, expected_agents_for_min, \
                     expected_tasks_for_min);                        \
    TestMaximization(cost, assignment_size, expected_agents_for_max, \
                     expected_tasks_for_max);                        \
  }

// Test on a 1x1 matrix

TEST(LinearAssignmentTest, SizeOneMatrix) {
  const int kMatrixHeight = 1;
  const int kMatrixWidth = 1;
  const double kCost[kMatrixHeight][kMatrixWidth] = {{4}};
  const int expected_agents_for_min[] = {0};
  const int expected_tasks_for_min[] = {0};
  const int expected_agents_for_max[] = {0};
  const int expected_tasks_for_max[] = {0};
  MATRIX_TEST;
}

// Test on a 4x4 matrix. Example taken at
// http://www.ee.oulu.fi/~mpa/matreng/eem1_2-1.htm
TEST(LinearAssignmentTest, Small4x4Matrix) {
  const int kMatrixHeight = 4;
  const int kMatrixWidth = 4;
  const double kCost[kMatrixHeight][kMatrixWidth] = {{90, 75, 75, 80},
                                                     {35, 85, 55, 65},
                                                     {125, 95, 90, 105},
                                                     {45, 110, 95, 115}};
  const int expected_agents_for_min[] = {0, 1, 2, 3};
  const int expected_tasks_for_min[] = {3, 2, 1, 0};
  const int expected_agents_for_max[] = {0, 1, 2, 3};
  const int expected_tasks_for_max[] = {2, 1, 0, 3};
  MATRIX_TEST;
}

// Test on a 3x4 matrix. Sub-problem of Small4x4Matrix
TEST(LinearAssignmentTest, Small3x4Matrix) {
  const int kMatrixHeight = 3;
  const int kMatrixWidth = 4;
  const double kCost[kMatrixHeight][kMatrixWidth] = {
      {90, 75, 75, 80}, {35, 85, 55, 65}, {125, 95, 90, 105}};
  const int expected_agents_for_min[] = {0, 1, 2};
  const int expected_tasks_for_min[] = {1, 0, 2};
  const int expected_agents_for_max[] = {0, 1, 2};
  const int expected_tasks_for_max[] = {3, 1, 0};
  MATRIX_TEST;
}

// Test on a 4x3 matrix. Sub-problem of Small4x4Matrix
TEST(LinearAssignmentTest, Small4x3Matrix) {
  const int kMatrixHeight = 4;
  const int kMatrixWidth = 3;
  const double kCost[kMatrixHeight][kMatrixWidth] = {
      {90, 75, 75}, {35, 85, 55}, {125, 95, 90}, {45, 110, 95}};
  const int expected_agents_for_min[] = {0, 1, 3};
  const int expected_tasks_for_min[] = {1, 2, 0};
  const int expected_agents_for_max[] = {0, 2, 3};
  const int expected_tasks_for_max[] = {2, 0, 1};
  MATRIX_TEST;
}

#undef MATRIX_TEST

}  // namespace operations_research
