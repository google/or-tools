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

#include "ortools/sat/diophantine.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <numeric>
#include <string>
#include <vector>

#include "absl/numeric/int128.h"
#include "absl/random/random.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "gtest/gtest.h"
#include "ortools/base/log_severity.h"
#include "ortools/base/mathutil.h"

namespace operations_research::sat {

namespace {

TEST(ReduceModuloBasis, LookZeroRows) {
  const std::vector<std::vector<absl::int128>> basis = {{0, 1}, {0, 0, 1}};
  std::vector<absl::int128> v = {1, 2, 3};
  ReduceModuloBasis(basis, 0, v);
  EXPECT_EQ(v, std::vector<absl::int128>({1, 2, 3}));
}

TEST(ReduceModuloBasis, LookOneRows) {
  const std::vector<std::vector<absl::int128>> basis = {{0, 1}, {0, 0, 1}};
  std::vector<absl::int128> v = {1, 2, 3};
  ReduceModuloBasis(basis, 1, v);
  EXPECT_EQ(v, std::vector<absl::int128>({1, 0, 3}));
}

TEST(ReduceModuloBasis, LookTwoRows) {
  const std::vector<std::vector<absl::int128>> basis = {{0, 1}, {0, 0, 1}};
  std::vector<absl::int128> v = {1, 2, 3};
  ReduceModuloBasis(basis, 2, v);
  EXPECT_EQ(v, std::vector<absl::int128>({1, 0, 0}));
}

template <typename T, typename URBG>
T UniformNonZero(URBG&& random, T lo, T hi) {
  T result = absl::Uniform<T>(random, lo, hi - 1);
  return result < 0 ? result : result + 1;
}

class RandomTest : public ::testing::TestWithParam<int> {
 protected:
  std::string GetSeedEnvName() {
    return absl::StrFormat("TestCase%d", GetParam());
  }
};

TEST_P(RandomTest, ReduceModuloBasis) {
  absl::BitGen random;

  const int num_rows = absl::Uniform<int>(random, 1, 20);

  std::vector<std::vector<absl::int128>> basis;
  for (int i = 0; i < num_rows; ++i) {
    basis.emplace_back(i + 2);
    for (int j = 0; j < i + 1; ++j) {
      basis.back()[j] = absl::Uniform<int>(random, -100, 100);
    }
    basis.back()[i + 1] = UniformNonZero(random, -100, 100);
  }
  std::vector<absl::int128> v_reduced(num_rows + 1);
  v_reduced[0] = absl::Uniform<int>(random, -100, 100);
  for (int i = 1; i <= num_rows; ++i) {
    int pivot = std::abs(static_cast<int>(basis[i - 1][i]));
    v_reduced[i] = absl::Uniform<int>(random, -MathUtil::FloorOfRatio(pivot, 2),
                                      MathUtil::CeilOfRatio(pivot, 2));
  }
  std::vector<absl::int128> v = v_reduced;
  for (int i = 0; i < num_rows; ++i) {
    int m = absl::Uniform<int>(random, -100, 100);
    for (int j = 0; j < basis[i].size(); ++j) {
      v[j] += m * basis[i][j];
    }
  }
  ReduceModuloBasis(basis, num_rows, v);
  EXPECT_EQ(v, v_reduced);
}

TEST_P(RandomTest, GreedyFastDecreasingGcd) {
  absl::BitGen random;

  const int num_elements = absl::Uniform<int>(random, 1, 50);
  std::vector<int64_t> coeffs(num_elements);
  for (int i = 0; i < num_elements; ++i) {
    coeffs[i] = UniformNonZero(random, 1 + std::numeric_limits<int64_t>::min(),
                               std::numeric_limits<int64_t>::max());
  }
  const std::vector<int> order = GreedyFastDecreasingGcd(coeffs);
  if (order.empty()) {
    int64_t gcd = std::abs(coeffs[0]);
    int64_t min_elem = std::abs(coeffs[0]);
    for (int i = 1; i < num_elements; ++i) {
      min_elem = std::min(min_elem, std::abs(coeffs[i]));
      gcd = std::gcd(gcd, std::abs(coeffs[i]));
    }
    EXPECT_EQ(gcd, min_elem);
    return;
  }

  // order should be a permutation.
  EXPECT_EQ(order.size(), num_elements);
  std::vector<bool> seen(num_elements, false);
  for (const int i : order) {
    EXPECT_FALSE(seen[i]);
    seen[i] = true;
  }

  // GCD should be decreasing then static.
  int64_t gcd = std::abs(coeffs[order[0]]);
  bool constant = false;
  int non_constant_terms = 1;

  for (int i = 1; i < num_elements; ++i) {
    const int64_t new_gcd = std::gcd(gcd, std::abs(coeffs[order[i]]));
    if (new_gcd != gcd) {
      EXPECT_FALSE(constant);
      gcd = new_gcd;
      ++non_constant_terms;
    } else {
      constant = true;
    }
  }
  EXPECT_GE(non_constant_terms, 2);  // order should be empty.
  EXPECT_LE(non_constant_terms, 15);
}

TEST_P(RandomTest, SolveDiophantine) {
  absl::BitGen random;

  // Creates a constraint and a solution.
  const int num_elements = absl::Uniform<int>(random, 1, 50);
  std::vector<int64_t> coeffs(num_elements);
  for (int i = 0; i < num_elements; ++i) {
    coeffs[i] = UniformNonZero(random, -1000000, 1000000);
  }

  std::vector<absl::int128> particular_solution(num_elements);
  std::vector<int64_t> lbs(num_elements);
  std::vector<int64_t> ubs(num_elements);
  absl::int128 rhs = 0;
  for (int i = 0; i < num_elements; ++i) {
    int64_t min = absl::Uniform<int64_t>(random, -1000000, 1000000);
    int64_t max = absl::Uniform<int64_t>(random, -1000000, 1000000);
    if (min > max) std::swap(min, max);
    lbs[i] = min;
    ubs[i] = max;
    particular_solution[i] = absl::Uniform<int64_t>(random, min, max + 1);
    rhs += coeffs[i] * particular_solution[i];
  }
  DiophantineSolution solution =
      SolveDiophantine(coeffs, static_cast<int64_t>(rhs), lbs, ubs);
  if (solution.no_reformulation_needed) {
    int64_t gcd = std::abs(coeffs[0]);
    int64_t min_elem = std::abs(coeffs[0]);
    for (int i = 1; i < num_elements; ++i) {
      min_elem = std::min(min_elem, std::abs(coeffs[i]));
      gcd = std::gcd(gcd, std::abs(coeffs[i]));
    }
    EXPECT_EQ(gcd, min_elem);
    return;
  }
  EXPECT_TRUE(solution.has_solutions);

  // Checks that the particular solution is inside the described solution.
  for (int i = 0; i < solution.special_solution.size(); ++i) {
    particular_solution[solution.index_permutation[i]] -=
        solution.special_solution[i];
  }
  int replaced_variable_count =
      static_cast<int>(solution.special_solution.size());
  for (int i = num_elements - 1; i >= replaced_variable_count; --i) {
    const absl::int128 q = particular_solution[solution.index_permutation[i]];
    particular_solution[solution.index_permutation[i]] = 0;
    for (int j = 0; j < solution.kernel_basis[i - 1].size(); ++j) {
      particular_solution[solution.index_permutation[j]] -=
          q * solution.kernel_basis[i - 1][j];
    }
  }
  for (int i = replaced_variable_count - 2; i >= 0; --i) {
    const int dom_coeff = static_cast<int>(solution.kernel_basis[i][i + 1]);
    EXPECT_EQ(
        particular_solution[solution.index_permutation[i + 1]] % dom_coeff, 0);
    const absl::int128 q =
        particular_solution[solution.index_permutation[i + 1]] / dom_coeff;
    EXPECT_LE(solution.kernel_vars_lbs[i], q);
    EXPECT_LE(q, solution.kernel_vars_ubs[i]);
    for (int j = 0; j < solution.kernel_basis[i].size(); ++j) {
      particular_solution[solution.index_permutation[j]] -=
          q * solution.kernel_basis[i][j];
    }
  }
  for (const absl::int128 s : particular_solution) {
    EXPECT_EQ(s, 0);
  }
}

INSTANTIATE_TEST_SUITE_P(RandomTests, RandomTest,
                         ::testing::Range(0, DEBUG_MODE ? 1000 : 10000));

}  // namespace

}  // namespace operations_research::sat
