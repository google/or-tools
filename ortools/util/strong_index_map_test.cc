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

#include "ortools/util/strong_index_map.h"

#include <optional>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/strong_int.h"

namespace operations_research {
namespace {

using ::testing::ElementsAre;
using ::testing::Optional;
using ::testing::Pair;

DEFINE_STRONG_INT_TYPE(Node, int);

// StrongIndex<> is just a trivial wrapper around the thoroughly-tested
// util::graph::Index<>, so we only test the plumbing.
TEST(StrongIndexTest, Api) {
  StrongIndexMap<Node, std::string> index;
  index.TryEmplace("yo");
  EXPECT_EQ(index.size(), Node(1));
  index.clear();
  EXPECT_EQ(index.size(), Node(0));
  EXPECT_EQ(index.Lookup("foo"), std::nullopt);
  EXPECT_EQ(index.LookupOrAdd("foo"), Node(0));
  EXPECT_THAT(index.Lookup("foo"), Optional(Node(0)));
  EXPECT_EQ(index.LookupOrAdd("bar"), Node(1));
  EXPECT_EQ(index[Node(0)], "foo");
  EXPECT_EQ(index[Node(1)], "bar");
  EXPECT_THAT(index.TryEmplace(absl::string_view("baz")), Pair(Node(2), true));
  EXPECT_THAT(index.TryEmplace("bar"), Pair(Node(1), false));
  EXPECT_THAT(index, ElementsAre("foo", "bar", "baz"));
  EXPECT_THAT(index.span(), ElementsAre("foo", "bar", "baz"));
  EXPECT_EQ(std::move(index).Extract()[Node(1)], "bar");
}

}  // namespace
}  // namespace operations_research
