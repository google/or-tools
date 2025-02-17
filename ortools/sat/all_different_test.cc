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

#include "ortools/sat/all_different.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <numeric>
#include <string>
#include <vector>

#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/logging.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/integer_search.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {
namespace {

class AllDifferentTest : public ::testing::TestWithParam<std::string> {
 public:
  std::function<void(Model*)> AllDifferent(
      absl::Span<const IntegerVariable> vars) {
    return [=, vars = std::vector<IntegerVariable>(vars.begin(), vars.end())](
               Model* model) {
      if (GetParam() == "binary") {
        model->Add(AllDifferentBinary(vars));
      } else if (GetParam() == "ac") {
        model->Add(AllDifferentBinary(vars));
        model->Add(AllDifferentAC(vars));
      } else if (GetParam() == "bounds") {
        model->Add(AllDifferentOnBounds(vars));
      } else {
        LOG(FATAL) << "Unknown implementation " << GetParam();
      }
    };
  }
};

INSTANTIATE_TEST_SUITE_P(All, AllDifferentTest,
                         ::testing::Values("binary", "ac", "bounds"));

TEST_P(AllDifferentTest, BasicBehavior) {
  Model model;
  std::vector<IntegerVariable> vars;
  vars.push_back(model.Add(NewIntegerVariable(1, 3)));
  vars.push_back(model.Add(NewIntegerVariable(0, 2)));
  vars.push_back(model.Add(NewIntegerVariable(1, 3)));
  vars.push_back(model.Add(NewIntegerVariable(0, 2)));
  model.Add(AllDifferent(vars));
  EXPECT_EQ(SatSolver::FEASIBLE, SolveIntegerProblemWithLazyEncoding(&model));

  std::vector<bool> value_seen(5, false);
  for (const IntegerVariable var : vars) {
    const int64_t value = model.Get(Value(var));
    EXPECT_FALSE(value_seen[value]);
    value_seen[value] = true;
  }
}

TEST_P(AllDifferentTest, PerfectMatching) {
  Model model;
  std::vector<IntegerVariable> vars;
  for (int i = 0; i < 4; ++i) {
    vars.push_back(model.Add(NewIntegerVariable(0, 10)));
  }
  IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();
  integer_trail->UpdateInitialDomain(vars[0], Domain::FromValues({3, 9}));
  integer_trail->UpdateInitialDomain(vars[1], Domain::FromValues({3, 8}));
  integer_trail->UpdateInitialDomain(vars[2], Domain::FromValues({1, 8}));
  integer_trail->UpdateInitialDomain(vars[3], Domain(1));
  model.Add(AllDifferent(vars));
  EXPECT_EQ(SatSolver::FEASIBLE, SolveIntegerProblemWithLazyEncoding(&model));
  EXPECT_EQ(1, model.Get(Value(vars[3])));
  EXPECT_EQ(8, model.Get(Value(vars[2])));
  EXPECT_EQ(3, model.Get(Value(vars[1])));
  EXPECT_EQ(9, model.Get(Value(vars[0])));
}

TEST_P(AllDifferentTest, EnumerateAllPermutations) {
  const int n = 6;
  Model model;
  std::vector<IntegerVariable> vars;
  for (int i = 0; i < n; ++i) {
    vars.push_back(model.Add(NewIntegerVariable(0, n - 1)));
  }
  model.Add(AllDifferent(vars));

  std::vector<std::vector<int>> solutions;
  while (true) {
    const auto status = SolveIntegerProblemWithLazyEncoding(&model);
    if (status != SatSolver::Status::FEASIBLE) break;
    solutions.emplace_back(n);
    for (int i = 0; i < n; ++i) solutions.back()[i] = model.Get(Value(vars[i]));
    model.Add(ExcludeCurrentSolutionAndBacktrack());
  }

  // Test that we do have all the permutations (but in a random order).
  std::sort(solutions.begin(), solutions.end());
  std::vector<int> expected(n);
  std::iota(expected.begin(), expected.end(), 0);
  for (int i = 0; i < solutions.size(); ++i) {
    EXPECT_EQ(expected, solutions[i]);
    if (i + 1 < solutions.size()) {
      EXPECT_TRUE(std::next_permutation(expected.begin(), expected.end()));
    } else {
      // We enumerated all the permutations.
      EXPECT_FALSE(std::next_permutation(expected.begin(), expected.end()));
    }
  }
}

int Factorial(int n) { return n ? n * Factorial(n - 1) : 1; }

TEST_P(AllDifferentTest, EnumerateAllInjections) {
  const int n = 5;
  const int m = n + 2;
  Model model;
  std::vector<IntegerVariable> vars;
  for (int i = 0; i < n; ++i) {
    vars.push_back(model.Add(NewIntegerVariable(0, m - 1)));
  }
  model.Add(AllDifferent(vars));

  std::vector<int> solution(n);
  int num_solutions = 0;
  while (true) {
    const auto status = SolveIntegerProblemWithLazyEncoding(&model);
    if (status != SatSolver::Status::FEASIBLE) break;
    for (int i = 0; i < n; i++) solution[i] = model.Get(Value(vars[i]));
    std::sort(solution.begin(), solution.end());
    for (int i = 1; i < n; i++) {
      EXPECT_LT(solution[i - 1], solution[i]);
    }
    num_solutions++;
    model.Add(ExcludeCurrentSolutionAndBacktrack());
  }
  // Count the number of solutions, it should be m!/(m-n)!.
  EXPECT_EQ(num_solutions, Factorial(m) / Factorial(m - n));
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
