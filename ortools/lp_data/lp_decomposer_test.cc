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

#include "ortools/lp_data/lp_decomposer.h"

#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_parser.h"
#include "ortools/lp_data/lp_types.h"

namespace operations_research {
namespace glop {
namespace {
void CheckDecomposition(absl::string_view original_problem,
                        absl::Span<const std::string> expected_problems,
                        const DenseRow& original_result,
                        absl::Span<const DenseRow> expected_results) {
  CHECK_EQ(expected_problems.size(), expected_results.size());
  LinearProgram linear_program;
  CHECK(ParseLp(original_problem, &linear_program));

  LPDecomposer decomposer;
  decomposer.Decompose(&linear_program);
  EXPECT_EQ(expected_problems.size(), decomposer.GetNumberOfProblems());

  // Check the generated problems.
  for (int i = 0; i < expected_problems.size(); ++i) {
    LinearProgram lp;
    decomposer.ExtractLocalProblem(i, &lp);
    EXPECT_EQ(expected_problems[i], lp.Dump());
  }

  // Check the generated problems again to make sure that calling
  // ExtractLocalProblem() twice gives the same result.
  LinearProgram lp;
  for (int i = 0; i < expected_problems.size(); ++i) {
    lp.Clear();
    decomposer.ExtractLocalProblem(i, &lp);
    EXPECT_EQ(expected_problems[i], lp.Dump());
  }

  EXPECT_EQ(original_result, decomposer.AggregateAssignments(expected_results));

  // Check that ExtractLocalAssignment() correctly returns the subproblem
  // assignment.
  for (int i = 0; i < expected_results.size(); ++i) {
    EXPECT_EQ(decomposer.ExtractLocalAssignment(i, original_result),
              expected_results[i]);
  }
}

TEST(LPDecomposer, NoVariables) {
  const std::string kLinearProgram = "max:";
  const DenseRow kResults = {};
  const std::vector<std::string> kExpectedProblems = {};
  const std::vector<DenseRow> kExpectedResults = {};
  CheckDecomposition(kLinearProgram, kExpectedProblems, kResults,
                     kExpectedResults);
}

TEST(LPDecomposer, NoConstraints) {
  const std::string kLinearProgram =
      "max: x + 2 y + 3 z;"
      "int: x, y, z";
  const DenseRow kResults = {1, 2, 3};
  const std::vector<std::string> kExpectedProblems = {// First sub problem
                                                      "max: + x;\n"
                                                      "-inf <= x <= inf;\n"
                                                      "int x;\n",
                                                      // Second sub problem
                                                      "max: + 2 y;\n"
                                                      "-inf <= y <= inf;\n"
                                                      "int y;\n",
                                                      // Third sub problem
                                                      "max: + 3 z;\n"
                                                      "-inf <= z <= inf;\n"
                                                      "int z;\n"};
  const std::vector<DenseRow> kExpectedResults = {{1}, {2}, {3}};
  CheckDecomposition(kLinearProgram, kExpectedProblems, kResults,
                     kExpectedResults);
}

TEST(LPDecomposer, OneCluster) {
  const std::string kLinearProgram =
      "max: x + 2 y + 3 z - 4 t + u;"
      "0 <= 5 x + 6 z <= 1;"
      "0 <= 7 y + 8 t <= 1;"
      "2 <= x <= 7;"
      "0 <= x + y + u <= 1;"
      "int: x, y, z, t, u";
  const DenseRow kResults = {1, 2, 3, 4, 5};
  const std::vector<std::string> kExpectedProblems = {
      // First sub problem
      "max: + x + 2 y + 3 z - 4 t + u;\n"
      "r0: 0 <= + 5 x + 6 z <= 1;\n"
      "r2: 0 <= + x + y + u <= 1;\n"
      "r1: 0 <= + 7 y + 8 t <= 1;\n"
      "2 <= x <= 7;\n"
      "-inf <= y <= inf;\n"
      "-inf <= z <= inf;\n"
      "-inf <= t <= inf;\n"
      "-inf <= u <= inf;\n"
      "int x y z t u;\n"};
  const std::vector<DenseRow> kExpectedResults = {{1, 2, 3, 4, 5}};
  CheckDecomposition(kLinearProgram, kExpectedProblems, kResults,
                     kExpectedResults);
}

TEST(LPDecomposer, TwoClusters) {
  const std::string kLinearProgram =
      "min: a1 + b1 + c1 + d1 + a2 + b2 + c2 + d2;"
      "0 <= a1 + b1 <= 1;"
      "0 <= a2 + b2 <= 1;"
      "0 <= c1 + d1 <= 1;"
      "0 <= c2 + d2 <= 1;"
      "0 <= a1 + d1 <= 1;"
      "0 <= a2 + d2 <= 1;"
      "bin: a1, b1, c1, d1, a2, b2, c2, d2";
  const DenseRow kResults = {1, 2, 3, 4, 5, 6, 7, 8};
  const std::vector<std::string> kExpectedProblems = {
      // First sub problem
      "min: + a1 + b1 + c1 + d1;\n"
      "r0: 0 <= + a1 + b1 <= 1;\n"
      "r4: 0 <= + a1 + d1 <= 1;\n"
      "r2: 0 <= + c1 + d1 <= 1;\n"
      "0 <= a1 <= 1;\n"
      "0 <= b1 <= 1;\n"
      "0 <= c1 <= 1;\n"
      "0 <= d1 <= 1;\n"
      "int a1 b1 c1 d1;\n",
      // Second sub problem
      "min: + a2 + b2 + c2 + d2;\n"
      "r1: 0 <= + a2 + b2 <= 1;\n"
      "r5: 0 <= + a2 + d2 <= 1;\n"
      "r3: 0 <= + c2 + d2 <= 1;\n"
      "0 <= a2 <= 1;\n"
      "0 <= b2 <= 1;\n"
      "0 <= c2 <= 1;\n"
      "0 <= d2 <= 1;\n"
      "int a2 b2 c2 d2;\n"};
  const std::vector<DenseRow> kExpectedResults = {{1, 2, 3, 4}, {5, 6, 7, 8}};
  CheckDecomposition(kLinearProgram, kExpectedProblems, kResults,
                     kExpectedResults);
}

TEST(LPDecomposer, ThreeClusters) {
  const std::string kLinearProgram =
      "max: x + 2 y + 3 z - 4 t + u;"
      "0 <= 5 x + 6 z <= 1;"
      "0 <= 7 y + 8 t <= 1;"
      "2 <= x <= 7;"
      "int: x, y, z, t, u";
  const DenseRow kResults = {1, 2, 3, 4, 5};
  const std::vector<std::string> kExpectedProblems = {
      // First sub problem
      "max: + x + 3 z;\n"
      "r0: 0 <= + 5 x + 6 z <= 1;\n"
      "2 <= x <= 7;\n"
      "-inf <= z <= inf;\n"
      "int x z;\n",
      // Second sub problem
      "max: + 2 y - 4 t;\n"
      "r1: 0 <= + 7 y + 8 t <= 1;\n"
      "-inf <= y <= inf;\n"
      "-inf <= t <= inf;\n"
      "int y t;\n",
      // Third sub problem
      "max: + u;\n"
      "-inf <= u <= inf;\n"
      "int u;\n"};
  const std::vector<DenseRow> kExpectedResults = {{1, 3}, {2, 4}, {5}};
  CheckDecomposition(kLinearProgram, kExpectedProblems, kResults,
                     kExpectedResults);
}

}  // anonymous namespace
}  // namespace glop
}  // namespace operations_research
