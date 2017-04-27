// Test file for hungarian.h

#include "ortools/algorithms/hungarian.h"

#include <unordered_map>
#include "ortools/base/integral_types.h"
#include "ortools/base/macros.h"
#include "ortools/base/map_util.h"
#include "ortools/base/random.h"
#include "gtest/gtest.h"

namespace operations_research {

// Generic check function that checks consistency of a linear assignment
// result as well as whether the result is the expected one.

void GenericCheck(const int expected_assignment_size,
                  const std::unordered_map<int, int>& direct_assignment,
                  const std::unordered_map<int, int>& reverse_assignment,
                  const int expected_agents[], const int expected_tasks[]) {
  EXPECT_EQ(expected_assignment_size, direct_assignment.size());
  EXPECT_EQ(expected_assignment_size, reverse_assignment.size());
  for (int i = 0; i < expected_assignment_size; ++i) {
    EXPECT_EQ(FindOrDie(direct_assignment, expected_agents[i]),
              expected_tasks[i]);
    EXPECT_EQ(FindOrDie(reverse_assignment, expected_tasks[i]),
              expected_agents[i]);
  }
  for (const auto& direct_iter : direct_assignment) {
    EXPECT_EQ(FindOrDie(reverse_assignment, direct_iter.second),
              direct_iter.first)
        << direct_iter.first << " -> " << direct_iter.second;
  }
}

void TestMinimization(const std::vector<std::vector<double> >& cost,
                      const int expected_assignment_size,
                      const int expected_agents[], const int expected_tasks[]) {
  std::unordered_map<int, int> direct_assignment;
  std::unordered_map<int, int> reverse_assignment;
  MinimizeLinearAssignment(cost, &direct_assignment, &reverse_assignment);
  SCOPED_TRACE("Minimization");
  GenericCheck(expected_assignment_size, direct_assignment, reverse_assignment,
               expected_agents, expected_tasks);
}

void TestMaximization(const std::vector<std::vector<double> >& cost,
                      const int expected_assignment_size,
                      const int expected_agents[], const int expected_tasks[]) {
  std::unordered_map<int, int> direct_assignment;
  std::unordered_map<int, int> reverse_assignment;
  MaximizeLinearAssignment(cost, &direct_assignment, &reverse_assignment);
  SCOPED_TRACE("Maximization");
  GenericCheck(expected_assignment_size, direct_assignment, reverse_assignment,
               expected_agents, expected_tasks);
}

// Test on an empty matrix

TEST(LinearAssignmentTest, NullMatrix) {
  std::vector<std::vector<double> > cost;
  const int* expected_agents = NULL;
  const int* expected_tasks = NULL;
  TestMinimization(cost, 0, expected_agents, expected_tasks);
  TestMaximization(cost, 0, expected_agents, expected_tasks);
}

#define MATRIX_TEST                                                  \
  {                                                                  \
    std::vector<std::vector<double> > cost(kMatrixHeight);                     \
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
