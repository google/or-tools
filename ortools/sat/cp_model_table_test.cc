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

#include "ortools/sat/cp_model_table.h"

#include <cstdint>
#include <vector>

#include "absl/container/inlined_vector.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/sat_parameters.pb.h"

namespace operations_research {
namespace sat {
namespace {

TEST(CompressTuplesTest, OneAny) {
  const std::vector<int64_t> domain_sizes = {2, 2, 2, 4};
  std::vector<std::vector<int64_t>> tuples = {
      {0, 0, 0, 0},
      {1, 1, 0, 2},
      {0, 0, 1, 3},
      {0, 1, 1, 3},
  };
  CompressTuples(domain_sizes, &tuples);
  const std::vector<std::vector<int64_t>>& expected_tuples = {
      {0, 0, 0, 0},
      {0, kTableAnyValue, 1, 3},  // Result is sorted.
      {1, 1, 0, 2},
  };
  EXPECT_EQ(tuples, expected_tuples);
}

TEST(CompressTuplesTest, NotPerfect) {
  const std::vector<int64_t> domain_sizes = {3, 3};
  std::vector<std::vector<int64_t>> tuples = {
      {0, 0}, {0, 1}, {0, 2}, {1, 2}, {2, 2},
  };
  CompressTuples(domain_sizes, &tuples);

  // Here we could return instead:
  // {0, kint64min}
  // {kint64min, 2}
  const std::vector<std::vector<int64_t>>& expected_tuples = {
      {0, 0},
      {0, 1},
      {kTableAnyValue, 2},
  };
  EXPECT_EQ(tuples, expected_tuples);
}

TEST(FullyCompressTuplesTest, BasicTest) {
  const std::vector<int64_t> domain_sizes = {4, 4};
  std::vector<std::vector<int64_t>> tuples = {
      {0, 1}, {0, 2}, {0, 3}, {1, 1}, {1, 2},
  };
  const auto result = FullyCompressTuples(domain_sizes, &tuples);
  const std::vector<std::vector<absl::InlinedVector<int64_t, 2>>>& expected = {
      {{1}, {1, 2}},
      {{0}, {1, 2, 3}},
  };
  EXPECT_EQ(result, expected);
}

TEST(CompressTuplesTest, BasicTest2) {
  const std::vector<int64_t> domain_sizes = {4, 4, 4, 4};
  std::vector<std::vector<int64_t>> tuples = {
      {0, 0, 0, 0},
      {1, 1, 0, 2},
      {0, 0, 1, 3},
      {0, 1, 1, 3},
  };
  const auto result = FullyCompressTuples(domain_sizes, &tuples);
  const std::vector<std::vector<absl::InlinedVector<int64_t, 2>>>& expected = {
      {{0}, {0}, {0}, {0}}, {{1}, {1}, {0}, {2}}, {{0}, {0, 1}, {1}, {3}}};
  EXPECT_EQ(result, expected);
}

TEST(CompressTuplesTest, BasicTest3) {
  const std::vector<int64_t> domain_sizes = {4, 4, 4, 4};
  std::vector<std::vector<int64_t>> tuples = {
      {0, 0, 0, 0}, {0, 1, 0, 0}, {1, 0, 0, 0}, {1, 1, 0, 0},
      {0, 0, 2, 0}, {0, 1, 2, 0}, {1, 0, 2, 0}, {1, 1, 2, 0},
  };
  const auto result = FullyCompressTuples(domain_sizes, &tuples);
  const std::vector<std::vector<absl::InlinedVector<int64_t, 2>>>& expected = {
      {{0, 1}, {0, 1}, {0, 2}, {0}}};
  EXPECT_EQ(result, expected);
}

TEST(FullyCompressTuplesTest, BasicTestWithAnyValue) {
  const std::vector<int64_t> domain_sizes = {4, 3};
  std::vector<std::vector<int64_t>> tuples = {
      {0, 1}, {0, 2}, {0, 3}, {1, 1}, {1, 2},
  };
  const auto result = FullyCompressTuples(domain_sizes, &tuples);
  const std::vector<std::vector<absl::InlinedVector<int64_t, 2>>>& expected = {
      {{0}, {}},
      {{1}, {1, 2}},
  };
  EXPECT_EQ(result, expected);
}

TEST(FullyCompressTuplesTest, ConvertAnyValueRepresentation) {
  const std::vector<int64_t> domain_sizes = {4, 3};
  std::vector<std::vector<int64_t>> tuples = {{0, kTableAnyValue},
                                              {kTableAnyValue, 2}};
  const auto result = FullyCompressTuples(domain_sizes, &tuples);
  const std::vector<std::vector<absl::InlinedVector<int64_t, 2>>>& expected = {
      {{0}, {}},
      {{}, {2}},
  };
  EXPECT_EQ(result, expected);
}

TEST(FullyCompressTuplesTest, ConvertAnyValueRepresentation2) {
  const std::vector<int64_t> domain_sizes = {4, 3, 2, 3};
  std::vector<std::vector<int64_t>> tuples = {
      {0, kTableAnyValue, 3, kTableAnyValue}};
  const auto result = FullyCompressTuples(domain_sizes, &tuples);
  const std::vector<std::vector<absl::InlinedVector<int64_t, 2>>>& expected = {
      {{0}, {}, {3}, {}},
  };
  EXPECT_EQ(result, expected);
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
