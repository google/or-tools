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

#include "ortools/graph/dag_connectivity.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/util/bitset.h"

using std::pair;
using std::vector;

namespace operations_research {
namespace {

TEST(DagConnectivityTest, EmptyGraph) {
  vector<pair<int, int>> arc_list;
  bool error = false;
  vector<int> cycle;
  vector<Bitset64<int64_t>> conn =
      ComputeDagConnectivity(arc_list, &error, &cycle);
  ASSERT_FALSE(error);
  EXPECT_TRUE(cycle.empty());
  EXPECT_TRUE(conn.empty());
}

void CheckMatrix(vector<vector<bool>> expected,
                 absl::Span<const Bitset64<int64_t>> actual) {
  int size = expected.size();
  ASSERT_EQ(size, actual.size());
  for (int i = 0; i < size; i++) {
    SCOPED_TRACE(absl::StrCat("row", i));
    ASSERT_EQ(size, expected[i].size());
    ASSERT_EQ(size, actual[i].size());
  }
  for (int i = 0; i < size; i++) {
    for (int j = 0; j < size; j++) {
      SCOPED_TRACE(absl::StrCat("entry:", i, ",", j));
      EXPECT_EQ(expected[i][j], actual[i][j]);
    }
  }
}

TEST(DagConnectivityTest, SimpleGraph) {
  vector<pair<int, int>> arc_list({{1, 0}});
  bool error = false;
  vector<int> cycle;
  vector<Bitset64<int64_t>> conn =
      ComputeDagConnectivity(arc_list, &error, &cycle);
  ASSERT_FALSE(error);
  EXPECT_TRUE(cycle.empty());
  CheckMatrix({{false, false}, {true, false}}, conn);
}

TEST(DagConnectivityTest, SimpleSparseGraph) {
  vector<pair<int, int>> arc_list({{3, 1}});
  bool error = false;
  vector<int> cycle;
  vector<Bitset64<int64_t>> conn =
      ComputeDagConnectivity(arc_list, &error, &cycle);
  ASSERT_FALSE(error);
  EXPECT_TRUE(cycle.empty());
  vector<vector<bool>> expected(4, vector<bool>(4));
  expected[3][1] = true;
  CheckMatrix(expected, conn);
}

TEST(DagConnectivityTest, SelfCycle) {
  vector<pair<int, int>> arc_list({{0, 1}, {1, 1}});
  bool error = false;
  vector<int> cycle;
  vector<Bitset64<int64_t>> conn =
      ComputeDagConnectivity(arc_list, &error, &cycle);
  ASSERT_TRUE(error);
  EXPECT_THAT(cycle, testing::ElementsAre(1));
}

TEST(DagConnectivityTest, LongCycle) {
  vector<pair<int, int>> arc_list({{2, 3}, {3, 5}, {5, 2}});
  bool error = false;
  vector<int> cycle;
  vector<Bitset64<int64_t>> conn =
      ComputeDagConnectivity(arc_list, &error, &cycle);
  ASSERT_TRUE(error);
  EXPECT_THAT(cycle, testing::UnorderedElementsAre(2, 3, 5));
}

TEST(DagConnectivityTest, BasicTransitive) {
  vector<pair<int, int>> arc_list({{0, 1}, {1, 2}});
  bool error = false;
  vector<int> cycle;
  vector<Bitset64<int64_t>> conn =
      ComputeDagConnectivity(arc_list, &error, &cycle);
  ASSERT_FALSE(error);
  EXPECT_TRUE(cycle.empty());
  vector<vector<bool>> expected(
      {{false, true, true}, {false, false, true}, {false, false, false}});
  CheckMatrix(expected, conn);
}

TEST(DagConnectivityTest, SparseTransitive) {
  vector<pair<int, int>> arc_list({{2, 5}, {5, 7}});
  bool error = false;
  vector<int> cycle;
  vector<Bitset64<int64_t>> conn =
      ComputeDagConnectivity(arc_list, &error, &cycle);
  ASSERT_FALSE(error);
  EXPECT_TRUE(cycle.empty());
  vector<vector<bool>> expected(8, vector<bool>(8));
  expected[2][5] = true;
  expected[2][7] = true;
  expected[5][7] = true;
  CheckMatrix(expected, conn);
}

TEST(DagConnectivityTest, RealGraph) {
  vector<pair<int, int>> arc_list;
  arc_list.push_back({8, 0});
  arc_list.push_back({1, 2});
  arc_list.push_back({1, 3});
  arc_list.push_back({3, 2});
  arc_list.push_back({4, 3});
  arc_list.push_back({4, 5});
  arc_list.push_back({5, 2});
  arc_list.push_back({7, 5});
  arc_list.push_back({5, 6});
  bool error = false;
  vector<int> cycle;
  vector<Bitset64<int64_t>> conn =
      ComputeDagConnectivity(arc_list, &error, &cycle);
  ASSERT_FALSE(error);
  EXPECT_TRUE(cycle.empty());
  vector<vector<bool>> expected(9, vector<bool>(9));
  expected[1][2] = true;
  expected[1][3] = true;
  expected[3][2] = true;
  expected[4][3] = true;
  expected[4][2] = true;
  expected[4][5] = true;
  expected[4][6] = true;
  expected[5][2] = true;
  expected[5][6] = true;
  expected[7][5] = true;
  expected[7][2] = true;
  expected[7][6] = true;
  expected[8][0] = true;
  CheckMatrix(expected, conn);
}

TEST(ComputeDagConnectivityOrDie, SimpleGraph) {
  vector<pair<int, int>> arc_list({{0, 1}, {1, 2}});
  vector<Bitset64<int64_t>> conn = ComputeDagConnectivityOrDie(arc_list);
  vector<vector<bool>> expected(
      {{false, true, true}, {false, false, true}, {false, false, false}});
  CheckMatrix(expected, conn);
}

TEST(ComputeDagConnectivityOrDieDeathTest, SimpleCycleCausesDeath) {
  vector<pair<int, int>> arc_list({{2, 3}, {3, 5}, {5, 2}});
  EXPECT_DEATH(
      { ComputeDagConnectivityOrDie(arc_list); },
      "Graph should have been acyclic but has cycle:");
}

}  // namespace
}  // namespace operations_research
