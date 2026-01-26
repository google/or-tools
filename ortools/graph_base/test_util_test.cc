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

#include "ortools/graph_base/test_util.h"

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/graph_base/io.h"

namespace util {
namespace {

TEST(Create2DGridGraphTest, Small) {
  EXPECT_EQ(GraphToString(*Create2DGridGraph<ListGraph<>>(4, 3),
                          PRINT_GRAPH_ADJACENCY_LISTS_SORTED),
            R"(0: 1 4
1: 0 2 5
2: 1 3 6
3: 2 7
4: 0 5 8
5: 1 4 6 9
6: 2 5 7 10
7: 3 6 11
8: 4 9
9: 5 8 10
10: 6 9 11
11: 7 10)");
}

}  // namespace
}  // namespace util
