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

#include "ortools/math_opt/solvers/gscip/gscip_testing.h"

#include <algorithm>
#include <cstdlib>
#include <ostream>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "ortools/math_opt/solvers/gscip/gscip_parameters.h"

namespace operations_research {

using ::testing::MakeMatcher;
using ::testing::Matcher;
using ::testing::MatcherInterface;
using ::testing::MatchResultListener;

GScipParameters TestGScipParameters() {
  GScipParameters result;
  GScipSetOutputEnabled(&result, false);
  return result;
}

std::string SolutionToString(const GScipSolution& solution) {
  std::vector<std::string> terms;
  for (const auto& var_value_pair : solution) {
    terms.push_back(absl::StrCat(SCIPvarGetName(var_value_pair.first), "=",
                                 var_value_pair.second));
  }
  std::sort(terms.begin(), terms.end());
  return absl::StrCat("{", absl::StrJoin(terms, ","), "}");
}

bool SolutionsAlmostEqual(const GScipSolution& left, const GScipSolution& right,
                          const double tolerance) {
  if (left.size() != right.size()) return false;
  for (const auto& kv_pair : left) {
    if (!right.contains(kv_pair.first)) return false;
    if (std::abs(kv_pair.second - right.at(kv_pair.first)) > tolerance) {
      return false;
    }
  }
  return true;
}

namespace {

class GScipSolutionNearMatcher : public MatcherInterface<GScipSolution> {
 public:
  GScipSolutionNearMatcher(const GScipSolution& rhs, double tolerance)
      : rhs_(rhs), tolerance_(tolerance) {}

  bool MatchAndExplain(GScipSolution lhs,
                       MatchResultListener* listener) const override {
    if (!SolutionsAlmostEqual(lhs, rhs_, tolerance_)) {
      *listener << "Expected: " << SolutionToString(lhs)
                << " != actual: " << SolutionToString(rhs_);
      return false;
    }
    return true;
  }

  void DescribeTo(std::ostream* os) const override {
    *os << "solution is term-wise within " << tolerance_ << " of "
        << SolutionToString(rhs_);
  }

  void DescribeNegationTo(std::ostream* os) const override {
    *os << "solution differs by at least " << tolerance_ << " from "
        << SolutionToString(rhs_);
  }

 private:
  const operations_research::GScipSolution rhs_;
  const double tolerance_;
};

void AssertOptimalWithASolution(const GScipResult& actual_result,
                                const double expected_objective_value,
                                const double tolerance) {
  ASSERT_EQ(actual_result.gscip_output.status(), GScipOutput::OPTIMAL);
  EXPECT_NEAR(actual_result.gscip_output.stats().best_bound(),
              expected_objective_value, tolerance);
  EXPECT_NEAR(actual_result.gscip_output.stats().best_objective(),
              expected_objective_value, tolerance);
  ASSERT_GE(actual_result.solutions.size(), 1);
  ASSERT_GE(actual_result.objective_values.size(), 1);
  EXPECT_NEAR(actual_result.objective_values[0], expected_objective_value,
              tolerance);
  EXPECT_THAT(actual_result.primal_ray, ::testing::IsEmpty());
}

}  // namespace

testing::Matcher<GScipSolution> GScipSolutionEquals(const GScipSolution& rhs) {
  return MakeMatcher(new GScipSolutionNearMatcher(rhs, 0.0));
}
testing::Matcher<GScipSolution> GScipSolutionAlmostEquals(
    const GScipSolution& rhs, double tolerance) {
  return MakeMatcher(new GScipSolutionNearMatcher(rhs, tolerance));
}

void AssertOptimalWithBestSolution(const GScipResult& actual_result,
                                   const double expected_objective_value,
                                   const GScipSolution& expected_solution,
                                   const double tolerance) {
  ASSERT_NO_FATAL_FAILURE(AssertOptimalWithASolution(
      actual_result, expected_objective_value, tolerance));
  EXPECT_THAT(actual_result.solutions[0],
              GScipSolutionAlmostEquals(expected_solution, tolerance));
}

void AssertOptimalWithPartialBestSolution(
    const GScipResult& actual_result, const double expected_objective_value,
    const GScipSolution& expected_solution, const double tolerance) {
  ASSERT_NO_FATAL_FAILURE(AssertOptimalWithASolution(
      actual_result, expected_objective_value, tolerance));
  // TODO(user): this should have some kind of matcher, it will give a better
  // error message.
  for (const auto& expected_var_val : expected_solution) {
    ASSERT_TRUE(actual_result.solutions[0].contains(expected_var_val.first));
    EXPECT_NEAR(actual_result.solutions[0].at(expected_var_val.first),
                expected_var_val.second, tolerance);
  }
}

}  // namespace operations_research
