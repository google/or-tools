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

#include "ortools/routing/parsers/simple_graph.h"

#include <sstream>
#include <string>

#include "absl/hash/hash_testing.h"
#include "gtest/gtest.h"

namespace operations_research::routing {
namespace {
TEST(SimpleGraphTest, EdgeHashing) {
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly({
      Edge(0, 0),
      Edge(1, 2),
      Edge(2, 1),
  }));
}

TEST(SimpleGraphTest, EdgeEquality) {
  EXPECT_EQ(Edge(0, 0), Edge(0, 0));
  EXPECT_NE(Edge(0, 0), Edge(1, 2));
  EXPECT_EQ(Edge(2, 1), Edge(1, 2));  // Undirected edge.
}

TEST(SimpleGraphTest, ArcHashing) {
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly({
      Arc(0, 0),
      Arc(1, 2),
      Arc(2, 1),
  }));
}

TEST(SimpleGraphTest, ArcEquality) {
  EXPECT_EQ(Arc(0, 0), Arc(0, 0));
  EXPECT_NE(Arc(0, 0), Arc(1, 2));
  EXPECT_NE(Arc(2, 1), Arc(1, 2));  // Directed edge.
}

TEST(Coordinates2Test, Int) {
  EXPECT_EQ(Coordinates2<int>(0, 0), Coordinates2<int>(0, 0));
  EXPECT_NE(Coordinates2<int>(0, 0), Coordinates2<int>(1, 2));

  std::stringstream generated;
  generated << Coordinates2<int>(0, 1);
  EXPECT_EQ(generated.str(), "{x = 0, y = 1}");

  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(
      {Coordinates2<int>(), Coordinates2<int>(0, 0), Coordinates2<int>(1, 2)}));
}

TEST(Coordinates2Test, Double) {
  EXPECT_EQ(Coordinates2<double>(0, 0), Coordinates2<double>(0, 0));
  EXPECT_NE(Coordinates2<double>(0, 0), Coordinates2<double>(1, 2));

  std::stringstream generated;
  generated << Coordinates2<double>(0.0, 1.0);
  EXPECT_EQ(generated.str(), "{x = 0, y = 1}");

  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(
      {Coordinates2<double>(), Coordinates2<double>(0, 0),
       Coordinates2<double>(1, 2)}));
}

TEST(Coordinates3Test, Int) {
  EXPECT_EQ(Coordinates3<int>(0, 0, 0), Coordinates3<int>(0, 0, 0));
  EXPECT_NE(Coordinates3<int>(0, 0, 0), Coordinates3<int>(1, 2, 3));

  std::stringstream generated;
  generated << Coordinates3<int>(0, 1, 2);
  EXPECT_EQ(generated.str(), "{x = 0, y = 1, z = 2}");

  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(
      {Coordinates3<int>(), Coordinates3<int>(0, 0, 0),
       Coordinates3<int>(1, 2, 3)}));
}

TEST(Coordinates3Test, Double) {
  EXPECT_EQ(Coordinates3<double>(0, 0, 0), Coordinates3<double>(0, 0, 0));
  EXPECT_NE(Coordinates3<double>(0, 0, 0), Coordinates3<double>(1, 2, 3));

  std::stringstream generated;
  generated << Coordinates3<double>(0.0, 1.0, 2.0);
  EXPECT_EQ(generated.str(), "{x = 0, y = 1, z = 2}");

  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(
      {Coordinates3<double>(), Coordinates3<double>(0.0, 0.0, 0.0),
       Coordinates3<double>(1.0, 2.0, 3.0)}));
}
}  //  namespace
}  // namespace operations_research::routing
