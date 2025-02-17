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

#include "ortools/graph/rooted_tree.h"

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/log/check.h"
#include "absl/random/random.h"
#include "absl/status/status.h"
#include "benchmark/benchmark.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/graph/graph.h"

namespace operations_research {
namespace {

using ::testing::AnyOf;
using ::testing::DoubleEq;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::HasSubstr;
using ::testing::IsFalse;
using ::testing::Pair;
using ::testing::SizeIs;
using ::testing::status::StatusIs;

////////////////////////////////////////////////////////////////////////////////
// RootedTree Tests
////////////////////////////////////////////////////////////////////////////////

template <typename T>
class RootedTreeTest : public testing::Test {
 public:
  static constexpr T kNullParent = RootedTree<T>::kNullParent;
};

TYPED_TEST_SUITE_P(RootedTreeTest);

TYPED_TEST_P(RootedTreeTest, CreateFailsRootOutOfBoundsInvalidArgument) {
  using Node = TypeParam;
  const Node root = 5;
  std::vector<Node> parents = {0, this->kNullParent};
  EXPECT_THAT(RootedTree<Node>::Create(root, parents),
              StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("root")));
}

TYPED_TEST_P(RootedTreeTest, CreateFailsRootHasParentInvalidArgument) {
  using Node = TypeParam;
  const Node root = 0;
  std::vector<Node> parents = {1, 0};
  EXPECT_THAT(RootedTree<Node>::Create(root, parents),
              StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("root")));
}

TYPED_TEST_P(RootedTreeTest, CreateFailsExtraRootInvalidArgument) {
  using Node = TypeParam;
  const Node root = 1;
  std::vector<Node> parents = {this->kNullParent, this->kNullParent};
  EXPECT_THAT(
      RootedTree<Node>::Create(root, parents),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("parent")));
}

TYPED_TEST_P(RootedTreeTest, CreateFailsBadParentInvalidArgument) {
  using Node = TypeParam;
  const Node root = 1;
  std::vector<Node> parents = {3, this->kNullParent};
  EXPECT_THAT(
      RootedTree<Node>::Create(root, parents),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("parent")));
}

TYPED_TEST_P(RootedTreeTest, CreateFailsIsolatedCycleInvalidArgument) {
  using Node = TypeParam;
  const Node root = 3;
  std::vector<Node> parents = {1, 2, 0, this->kNullParent, 3};
  EXPECT_THAT(RootedTree<Node>::Create(root, parents),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("cycle"), HasSubstr("0, 1, 2"))));
  std::vector<Node> cycle;
  EXPECT_THAT(RootedTree<Node>::Create(root, parents, &cycle),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("cycle"), HasSubstr("0, 1, 2"))));
  EXPECT_THAT(cycle, ElementsAre(0, 1, 2, 0));
}

TYPED_TEST_P(RootedTreeTest, CreateFailsPathLeadsToCycleInvalidArgument) {
  using Node = TypeParam;
  const Node root = 3;
  std::vector<Node> parents = {1, 2, 0, this->kNullParent, 0};
  EXPECT_THAT(RootedTree<Node>::Create(root, parents),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("cycle"), HasSubstr("0, 1, 2"))));
  std::vector<Node> cycle;
  EXPECT_THAT(RootedTree<Node>::Create(root, parents, &cycle),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("cycle"), HasSubstr("0, 1, 2"))));
  EXPECT_THAT(cycle, ElementsAre(0, 1, 2, 0));
}

TYPED_TEST_P(RootedTreeTest, CreatePathFailsLongCycleErrorIsTruncated) {
  using Node = TypeParam;
  const Node root = 50;
  std::vector<Node> parents(51);
  parents[root] = this->kNullParent;
  for (Node i = 0; i < Node{50}; ++i) {
    parents[i] = (i + 1) % Node{50};
  }
  EXPECT_THAT(RootedTree<Node>::Create(root, parents),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("cycle"),
                             HasSubstr("0, 1, 2, 3, 4, 5, 6, 7, ..., 0"))));
  std::vector<Node> cycle;
  EXPECT_THAT(RootedTree<Node>::Create(root, parents, &cycle),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("cycle"),
                             HasSubstr("0, 1, 2, 3, 4, 5, 6, 7, ..., 0"))));
  std::vector<Node> expected_cycle(50);
  absl::c_iota(expected_cycle, 0);
  expected_cycle.push_back(0);
  EXPECT_THAT(cycle, ElementsAreArray(expected_cycle));
}

TYPED_TEST_P(RootedTreeTest, PathToRoot) {
  using Node = TypeParam;
  //     1
  //  /  |
  // 0   3
  //     |
  //     2
  const Node root = 1;
  std::vector<Node> parents = {1, this->kNullParent, 3, 1};
  ASSERT_OK_AND_ASSIGN(const auto tree,
                       RootedTree<Node>::Create(root, parents));
  EXPECT_THAT(tree.PathToRoot(0), ElementsAre(0, 1));
  EXPECT_THAT(tree.PathToRoot(1), ElementsAre(1));
  EXPECT_THAT(tree.PathToRoot(2), ElementsAre(2, 3, 1));
  EXPECT_THAT(tree.PathToRoot(3), ElementsAre(3, 1));
}

TYPED_TEST_P(RootedTreeTest, DistanceToRoot) {
  using Node = TypeParam;
  //     1
  //  /  |
  // 0   3
  //     |
  //     2
  const int root = 1;
  std::vector<Node> parents = {1, this->kNullParent, 3, 1};
  std::vector<double> arc_lengths = {1, 0, 10, 100};
  ASSERT_OK_AND_ASSIGN(const auto tree,
                       RootedTree<Node>::Create(root, parents));
  EXPECT_DOUBLE_EQ(tree.DistanceToRoot(0, arc_lengths), 1);
  EXPECT_DOUBLE_EQ(tree.DistanceToRoot(1, arc_lengths), 0);
  EXPECT_DOUBLE_EQ(tree.DistanceToRoot(2, arc_lengths), 110);
  EXPECT_DOUBLE_EQ(tree.DistanceToRoot(3, arc_lengths), 100);
}

TYPED_TEST_P(RootedTreeTest, DistanceAndPathToRoot) {
  using Node = TypeParam;
  //     1
  //  /  |
  // 0   3
  //     |
  //     2
  const Node root = 1;
  std::vector<Node> parents = {1, this->kNullParent, 3, 1};
  std::vector<double> arc_lengths = {1, 0, 10, 100};
  ASSERT_OK_AND_ASSIGN(const auto tree,
                       RootedTree<Node>::Create(root, parents));
  EXPECT_THAT(tree.DistanceAndPathToRoot(0, arc_lengths),
              Pair(DoubleEq(1.0), ElementsAre(0, 1)));
  EXPECT_THAT(tree.DistanceAndPathToRoot(1, arc_lengths),
              Pair(DoubleEq(0.0), ElementsAre(1)));
  EXPECT_THAT(tree.DistanceAndPathToRoot(2, arc_lengths),
              Pair(DoubleEq(110.0), ElementsAre(2, 3, 1)));
  EXPECT_THAT(tree.DistanceAndPathToRoot(3, arc_lengths),
              Pair(DoubleEq(100.0), ElementsAre(3, 1)));
}

TYPED_TEST_P(RootedTreeTest, TopologicalSort) {
  using Node = TypeParam;
  //     1
  //  /  |
  // 0   3
  //     |
  //     2
  const Node root = 1;
  std::vector<Node> parents = {1, this->kNullParent, 3, 1};
  ASSERT_OK_AND_ASSIGN(const auto tree,
                       RootedTree<Node>::Create(root, parents));

  EXPECT_THAT(tree.TopologicalSort(),
              AnyOf(ElementsAre(1, 0, 3, 2), ElementsAre(1, 3, 2, 0),
                    ElementsAre(1, 3, 0, 2)));
}

TYPED_TEST_P(RootedTreeTest, AllDistancesToRoot) {
  using Node = TypeParam;
  //     1
  //  /  |
  // 0   3
  //     |
  //     2
  const Node root = 1;
  std::vector<Node> parents = {1, this->kNullParent, 3, 1};
  const std::vector<double> arc_lengths = {1, 0, 10, 100};
  ASSERT_OK_AND_ASSIGN(const auto tree,
                       RootedTree<Node>::Create(root, parents));
  EXPECT_THAT(tree.template AllDistancesToRoot<double>(arc_lengths),
              ElementsAre(1.0, 0.0, 110.0, 100.0));
}

TYPED_TEST_P(RootedTreeTest, AllDepths) {
  using Node = TypeParam;
  //     1
  //  /  |
  // 0   3
  //     |
  //     2
  const Node root = 1;
  std::vector<Node> parents = {1, this->kNullParent, 3, 1};
  ASSERT_OK_AND_ASSIGN(const auto tree,
                       RootedTree<Node>::Create(root, parents));
  EXPECT_THAT(tree.AllDepths(), ElementsAre(1, 0, 2, 1));
}

TYPED_TEST_P(RootedTreeTest, LCAByDepth) {
  using Node = TypeParam;
  //        4
  //      /
  //     1
  //  /  |
  // 0   3
  //     |
  //     2
  const int root = 4;
  std::vector<Node> parents = {1, 4, 3, 1, this->kNullParent};
  ASSERT_OK_AND_ASSIGN(const auto tree,
                       RootedTree<Node>::Create(root, parents));
  const std::vector<Node> depths = {2, 1, 3, 2, 0};
  ASSERT_THAT(tree.AllDepths(), ElementsAreArray(depths));
  EXPECT_EQ(tree.LowestCommonAncestorByDepth(0, 0, depths), 0);
  EXPECT_EQ(tree.LowestCommonAncestorByDepth(0, 1, depths), 1);
  EXPECT_EQ(tree.LowestCommonAncestorByDepth(0, 2, depths), 1);
  EXPECT_EQ(tree.LowestCommonAncestorByDepth(0, 3, depths), 1);
  EXPECT_EQ(tree.LowestCommonAncestorByDepth(0, 4, depths), 4);

  EXPECT_EQ(tree.LowestCommonAncestorByDepth(1, 0, depths), 1);
  EXPECT_EQ(tree.LowestCommonAncestorByDepth(1, 1, depths), 1);
  EXPECT_EQ(tree.LowestCommonAncestorByDepth(1, 2, depths), 1);
  EXPECT_EQ(tree.LowestCommonAncestorByDepth(1, 3, depths), 1);
  EXPECT_EQ(tree.LowestCommonAncestorByDepth(1, 4, depths), 4);

  EXPECT_EQ(tree.LowestCommonAncestorByDepth(2, 0, depths), 1);
  EXPECT_EQ(tree.LowestCommonAncestorByDepth(2, 1, depths), 1);
  EXPECT_EQ(tree.LowestCommonAncestorByDepth(2, 2, depths), 2);
  EXPECT_EQ(tree.LowestCommonAncestorByDepth(2, 3, depths), 3);
  EXPECT_EQ(tree.LowestCommonAncestorByDepth(2, 4, depths), 4);

  EXPECT_EQ(tree.LowestCommonAncestorByDepth(3, 0, depths), 1);
  EXPECT_EQ(tree.LowestCommonAncestorByDepth(3, 1, depths), 1);
  EXPECT_EQ(tree.LowestCommonAncestorByDepth(3, 2, depths), 3);
  EXPECT_EQ(tree.LowestCommonAncestorByDepth(3, 3, depths), 3);
  EXPECT_EQ(tree.LowestCommonAncestorByDepth(3, 4, depths), 4);

  EXPECT_EQ(tree.LowestCommonAncestorByDepth(4, 0, depths), 4);
  EXPECT_EQ(tree.LowestCommonAncestorByDepth(4, 1, depths), 4);
  EXPECT_EQ(tree.LowestCommonAncestorByDepth(4, 2, depths), 4);
  EXPECT_EQ(tree.LowestCommonAncestorByDepth(4, 3, depths), 4);
  EXPECT_EQ(tree.LowestCommonAncestorByDepth(4, 4, depths), 4);
}

TYPED_TEST_P(RootedTreeTest, LCAByBySearch) {
  using Node = TypeParam;
  //        4
  //      /
  //     1
  //  /  |
  // 0   3
  //     |
  //     2
  const Node root = 4;
  std::vector<Node> parents = {1, 4, 3, 1, this->kNullParent};
  ASSERT_OK_AND_ASSIGN(const auto tree,
                       RootedTree<Node>::Create(root, parents));
  std::vector<bool> ws(5, false);
  EXPECT_EQ(tree.LowestCommonAncestorBySearch(0, 0, ws), 0);
  ASSERT_THAT(ws, AllOf(SizeIs(5), Each(IsFalse())));
  EXPECT_EQ(tree.LowestCommonAncestorBySearch(0, 1, ws), 1);
  ASSERT_THAT(ws, AllOf(SizeIs(5), Each(IsFalse())));
  EXPECT_EQ(tree.LowestCommonAncestorBySearch(0, 2, ws), 1);
  ASSERT_THAT(ws, AllOf(SizeIs(5), Each(IsFalse())));
  EXPECT_EQ(tree.LowestCommonAncestorBySearch(0, 3, ws), 1);
  ASSERT_THAT(ws, AllOf(SizeIs(5), Each(IsFalse())));
  EXPECT_EQ(tree.LowestCommonAncestorBySearch(0, 4, ws), 4);
  ASSERT_THAT(ws, AllOf(SizeIs(5), Each(IsFalse())));

  EXPECT_EQ(tree.LowestCommonAncestorBySearch(1, 0, ws), 1);
  ASSERT_THAT(ws, AllOf(SizeIs(5), Each(IsFalse())));
  EXPECT_EQ(tree.LowestCommonAncestorBySearch(1, 1, ws), 1);
  ASSERT_THAT(ws, AllOf(SizeIs(5), Each(IsFalse())));
  EXPECT_EQ(tree.LowestCommonAncestorBySearch(1, 2, ws), 1);
  ASSERT_THAT(ws, AllOf(SizeIs(5), Each(IsFalse())));
  EXPECT_EQ(tree.LowestCommonAncestorBySearch(1, 3, ws), 1);
  ASSERT_THAT(ws, AllOf(SizeIs(5), Each(IsFalse())));
  EXPECT_EQ(tree.LowestCommonAncestorBySearch(1, 4, ws), 4);
  ASSERT_THAT(ws, AllOf(SizeIs(5), Each(IsFalse())));

  EXPECT_EQ(tree.LowestCommonAncestorBySearch(2, 0, ws), 1);
  ASSERT_THAT(ws, AllOf(SizeIs(5), Each(IsFalse())));
  EXPECT_EQ(tree.LowestCommonAncestorBySearch(2, 1, ws), 1);
  ASSERT_THAT(ws, AllOf(SizeIs(5), Each(IsFalse())));
  EXPECT_EQ(tree.LowestCommonAncestorBySearch(2, 2, ws), 2);
  ASSERT_THAT(ws, AllOf(SizeIs(5), Each(IsFalse())));
  EXPECT_EQ(tree.LowestCommonAncestorBySearch(2, 3, ws), 3);
  ASSERT_THAT(ws, AllOf(SizeIs(5), Each(IsFalse())));
  EXPECT_EQ(tree.LowestCommonAncestorBySearch(2, 4, ws), 4);
  ASSERT_THAT(ws, AllOf(SizeIs(5), Each(IsFalse())));

  EXPECT_EQ(tree.LowestCommonAncestorBySearch(3, 0, ws), 1);
  ASSERT_THAT(ws, AllOf(SizeIs(5), Each(IsFalse())));
  EXPECT_EQ(tree.LowestCommonAncestorBySearch(3, 1, ws), 1);
  ASSERT_THAT(ws, AllOf(SizeIs(5), Each(IsFalse())));
  EXPECT_EQ(tree.LowestCommonAncestorBySearch(3, 2, ws), 3);
  ASSERT_THAT(ws, AllOf(SizeIs(5), Each(IsFalse())));
  EXPECT_EQ(tree.LowestCommonAncestorBySearch(3, 3, ws), 3);
  ASSERT_THAT(ws, AllOf(SizeIs(5), Each(IsFalse())));
  EXPECT_EQ(tree.LowestCommonAncestorBySearch(3, 4, ws), 4);
  ASSERT_THAT(ws, AllOf(SizeIs(5), Each(IsFalse())));

  EXPECT_EQ(tree.LowestCommonAncestorBySearch(4, 0, ws), 4);
  ASSERT_THAT(ws, AllOf(SizeIs(5), Each(IsFalse())));
  EXPECT_EQ(tree.LowestCommonAncestorBySearch(4, 1, ws), 4);
  ASSERT_THAT(ws, AllOf(SizeIs(5), Each(IsFalse())));
  EXPECT_EQ(tree.LowestCommonAncestorBySearch(4, 2, ws), 4);
  ASSERT_THAT(ws, AllOf(SizeIs(5), Each(IsFalse())));
  EXPECT_EQ(tree.LowestCommonAncestorBySearch(4, 3, ws), 4);
  ASSERT_THAT(ws, AllOf(SizeIs(5), Each(IsFalse())));
  EXPECT_EQ(tree.LowestCommonAncestorBySearch(4, 4, ws), 4);
  ASSERT_THAT(ws, AllOf(SizeIs(5), Each(IsFalse())));
}

TYPED_TEST_P(RootedTreeTest, Path) {
  using Node = TypeParam;
  //        4
  //      /
  //     1
  //  /  |
  // 0   3
  //     |
  //     2
  const Node root = 4;
  std::vector<Node> parents = {1, 4, 3, 1, this->kNullParent};
  ASSERT_OK_AND_ASSIGN(const auto tree,
                       RootedTree<Node>::Create(root, parents));
  std::vector<Node> depths = {2, 1, 3, 2, 0};
  ASSERT_THAT(tree.AllDepths(), ElementsAreArray(depths));
  auto path = [&tree, &depths](Node start, Node end) {
    const Node lca = tree.LowestCommonAncestorByDepth(start, end, depths);
    return tree.Path(start, end, lca);
  };
  EXPECT_THAT(path(0, 0), ElementsAre(0));
  EXPECT_THAT(path(0, 1), ElementsAre(0, 1));
  EXPECT_THAT(path(0, 2), ElementsAre(0, 1, 3, 2));
  EXPECT_THAT(path(0, 3), ElementsAre(0, 1, 3));
  EXPECT_THAT(path(0, 4), ElementsAre(0, 1, 4));

  EXPECT_THAT(path(1, 0), ElementsAre(1, 0));
  EXPECT_THAT(path(1, 1), ElementsAre(1));
  EXPECT_THAT(path(1, 2), ElementsAre(1, 3, 2));
  EXPECT_THAT(path(1, 3), ElementsAre(1, 3));
  EXPECT_THAT(path(1, 4), ElementsAre(1, 4));

  EXPECT_THAT(path(2, 0), ElementsAre(2, 3, 1, 0));
  EXPECT_THAT(path(2, 1), ElementsAre(2, 3, 1));
  EXPECT_THAT(path(2, 2), ElementsAre(2));
  EXPECT_THAT(path(2, 3), ElementsAre(2, 3));
  EXPECT_THAT(path(2, 4), ElementsAre(2, 3, 1, 4));

  EXPECT_THAT(path(3, 0), ElementsAre(3, 1, 0));
  EXPECT_THAT(path(3, 1), ElementsAre(3, 1));
  EXPECT_THAT(path(3, 2), ElementsAre(3, 2));
  EXPECT_THAT(path(3, 3), ElementsAre(3));
  EXPECT_THAT(path(3, 4), ElementsAre(3, 1, 4));

  EXPECT_THAT(path(4, 0), ElementsAre(4, 1, 0));
  EXPECT_THAT(path(4, 1), ElementsAre(4, 1));
  EXPECT_THAT(path(4, 2), ElementsAre(4, 1, 3, 2));
  EXPECT_THAT(path(4, 3), ElementsAre(4, 1, 3));
  EXPECT_THAT(path(4, 4), ElementsAre(4));
}

TYPED_TEST_P(RootedTreeTest, Distance) {
  using Node = TypeParam;
  //        4
  //      /
  //     1
  //  /  |
  // 0   3
  //     |
  //     2
  const int root = 4;
  std::vector<Node> parents = {1, 4, 3, 1, this->kNullParent};
  std::vector<double> arc_lengths = {1.0, 10.0, 100.0, 1000.0, 0.0};
  ASSERT_OK_AND_ASSIGN(const auto tree,
                       RootedTree<Node>::Create(root, parents));
  std::vector<Node> depths = {2, 1, 3, 2, 0};
  ASSERT_THAT(tree.AllDepths(), ElementsAreArray(depths));
  auto dist = [&tree, &depths, &arc_lengths](Node start, Node end) {
    const Node lca = tree.LowestCommonAncestorByDepth(start, end, depths);
    return tree.Distance(start, end, lca, arc_lengths);
  };
  EXPECT_DOUBLE_EQ(dist(0, 0), 0.0);
  EXPECT_DOUBLE_EQ(dist(0, 1), 1.0);
  EXPECT_DOUBLE_EQ(dist(0, 2), 1101.0);
  EXPECT_DOUBLE_EQ(dist(0, 3), 1001.0);
  EXPECT_DOUBLE_EQ(dist(0, 4), 11.0);

  EXPECT_DOUBLE_EQ(dist(1, 0), 1.0);
  EXPECT_DOUBLE_EQ(dist(1, 1), 0.0);
  EXPECT_DOUBLE_EQ(dist(1, 2), 1100.0);
  EXPECT_DOUBLE_EQ(dist(1, 3), 1000.0);
  EXPECT_DOUBLE_EQ(dist(1, 4), 10.0);

  EXPECT_DOUBLE_EQ(dist(2, 0), 1101.0);
  EXPECT_DOUBLE_EQ(dist(2, 1), 1100.0);
  EXPECT_DOUBLE_EQ(dist(2, 2), 0.0);
  EXPECT_DOUBLE_EQ(dist(2, 3), 100.0);
  EXPECT_DOUBLE_EQ(dist(2, 4), 1110.0);

  EXPECT_DOUBLE_EQ(dist(3, 0), 1001.0);
  EXPECT_DOUBLE_EQ(dist(3, 1), 1000.0);
  EXPECT_DOUBLE_EQ(dist(3, 2), 100.0);
  EXPECT_DOUBLE_EQ(dist(3, 3), 0.0);
  EXPECT_DOUBLE_EQ(dist(3, 4), 1010.0);

  EXPECT_DOUBLE_EQ(dist(4, 0), 11.0);
  EXPECT_DOUBLE_EQ(dist(4, 1), 10.0);
  EXPECT_DOUBLE_EQ(dist(4, 2), 1110.0);
  EXPECT_DOUBLE_EQ(dist(4, 3), 1010.0);
  EXPECT_DOUBLE_EQ(dist(4, 4), 0.0);
}

TYPED_TEST_P(RootedTreeTest, EvertChangeRoot) {
  using Node = TypeParam;
  // Starting graph, with root 2:
  //  0 -> 1 -> 2
  //  |    |    |
  //  3    4    5
  //
  // Evert: change the root to 0
  //
  //  0 <- 1 <- 2
  //  |    |    |
  //  3    4    5
  const Node root = 2;
  const std::vector<Node> parents = {1, 2, this->kNullParent, 0, 1, 2};
  ASSERT_OK_AND_ASSIGN(auto tree, RootedTree<Node>::Create(root, parents));
  tree.Evert(0);
  EXPECT_EQ(tree.root(), 0);
  EXPECT_THAT(tree.parents(), ElementsAre(this->kNullParent, 0, 1, 0, 1, 2));
}

TYPED_TEST_P(RootedTreeTest, EvertSameRoot) {
  using Node = TypeParam;
  const Node root = 1;
  const std::vector<Node> parents = {1, this->kNullParent, 1};
  ASSERT_OK_AND_ASSIGN(auto tree, RootedTree<Node>::Create(root, parents));
  tree.Evert(1);
  EXPECT_EQ(tree.root(), 1);
  EXPECT_THAT(tree.parents(), ElementsAre(1, this->kNullParent, 1));
}

TYPED_TEST_P(RootedTreeTest, RootedTreeFromGraphSuccessNoExtraOutputs) {
  using Node = TypeParam;
  //        4
  //      /
  //     1
  //  /  |
  // 0   3
  //     |
  //     2
  util::ListGraph<Node, int> graph;
  graph.AddNode(4);
  for (auto [n1, n2] :
       std::vector<std::pair<Node, Node>>{{0, 1}, {1, 4}, {1, 3}, {3, 2}}) {
    graph.AddArc(n1, n2);
    graph.AddArc(n2, n1);
  }
  const Node root = 4;
  std::vector<Node>* topo = nullptr;
  std::vector<Node>* depth = nullptr;
  ASSERT_OK_AND_ASSIGN(const RootedTree<Node> tree,
                       RootedTreeFromGraph(root, graph, topo, depth));
  EXPECT_EQ(tree.root(), 4);
  EXPECT_THAT(tree.parents(), ElementsAre(1, 4, 3, 1, this->kNullParent));
  EXPECT_EQ(topo, nullptr);
  EXPECT_EQ(depth, nullptr);
}

TYPED_TEST_P(RootedTreeTest, RootedTreeFromGraphSuccessAllExtraOutputs) {
  using Node = TypeParam;
  //        4
  //      /
  //     1
  //  /  |
  // 0   3
  //     |
  //     2
  util::ListGraph<Node, int> graph;
  graph.AddNode(4);
  for (auto [n1, n2] :
       std::vector<std::pair<Node, Node>>{{0, 1}, {1, 4}, {1, 3}, {3, 2}}) {
    graph.AddArc(n1, n2);
    graph.AddArc(n2, n1);
  }
  const Node root = 4;
  std::vector<Node> topo;
  std::vector<Node> depth;
  ASSERT_OK_AND_ASSIGN(const RootedTree<Node> tree,
                       RootedTreeFromGraph(root, graph, &topo, &depth));
  EXPECT_EQ(tree.root(), 4);
  EXPECT_THAT(tree.parents(), ElementsAre(1, 4, 3, 1, this->kNullParent));
  EXPECT_THAT(topo,
              AnyOf(ElementsAre(4, 1, 0, 3, 2), ElementsAre(4, 1, 3, 0, 2),
                    ElementsAre(4, 1, 3, 2, 0)));
  EXPECT_THAT(depth, ElementsAre(2, 1, 3, 2, 0));
}

TYPED_TEST_P(RootedTreeTest, RootedTreeFromGraphBadRootInvalidArgument) {
  using Node = TypeParam;
  util::ListGraph<Node, int> graph;
  graph.AddNode(2);
  graph.AddArc(0, 1);
  graph.AddArc(1, 0);
  const Node root = 4;
  EXPECT_THAT(
      RootedTreeFromGraph(root, graph, nullptr, nullptr),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("invalid root")));
}

TYPED_TEST_P(RootedTreeTest, RootedTreeFromGraphSelfCycleInvalidArgument) {
  using Node = TypeParam;
  util::ListGraph<Node, int> graph;
  graph.AddNode(2);
  graph.AddArc(0, 1);
  graph.AddArc(1, 0);
  graph.AddArc(1, 1);
  const Node root = 0;
  EXPECT_THAT(RootedTreeFromGraph(root, graph, nullptr, nullptr),
              StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("cycle")));
}

TYPED_TEST_P(RootedTreeTest, RootedTreeFromGraphHasCycleInvalidArgument) {
  using Node = TypeParam;
  util::ListGraph<Node, int> graph;
  graph.AddNode(3);
  graph.AddArc(0, 1);
  graph.AddArc(1, 0);
  graph.AddArc(1, 2);
  graph.AddArc(2, 1);
  graph.AddArc(2, 0);
  graph.AddArc(0, 2);
  const Node root = 0;
  EXPECT_THAT(RootedTreeFromGraph(root, graph, nullptr, nullptr),
              StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("cycle")));
}

TYPED_TEST_P(RootedTreeTest, RootedTreeFromGraphNotConnectedInvalidArgument) {
  using Node = TypeParam;
  util::ListGraph<Node, int> graph;
  graph.AddNode(4);
  graph.AddArc(0, 1);
  graph.AddArc(1, 0);
  graph.AddArc(2, 3);
  graph.AddArc(3, 2);
  const Node root = 0;
  EXPECT_THAT(
      RootedTreeFromGraph(root, graph, nullptr, nullptr),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("not connected")));
}

REGISTER_TYPED_TEST_SUITE_P(
    RootedTreeTest, CreateFailsRootOutOfBoundsInvalidArgument,
    CreateFailsRootHasParentInvalidArgument,
    CreateFailsExtraRootInvalidArgument, CreateFailsBadParentInvalidArgument,
    CreateFailsIsolatedCycleInvalidArgument,
    CreateFailsPathLeadsToCycleInvalidArgument,
    CreatePathFailsLongCycleErrorIsTruncated, PathToRoot, DistanceToRoot,
    DistanceAndPathToRoot, TopologicalSort, AllDistancesToRoot, AllDepths,
    LCAByDepth, LCAByBySearch, Path, Distance, EvertChangeRoot, EvertSameRoot,
    RootedTreeFromGraphSuccessNoExtraOutputs,
    RootedTreeFromGraphSuccessAllExtraOutputs,
    RootedTreeFromGraphBadRootInvalidArgument,
    RootedTreeFromGraphSelfCycleInvalidArgument,
    RootedTreeFromGraphHasCycleInvalidArgument,
    RootedTreeFromGraphNotConnectedInvalidArgument);

using NodeTypes =
    ::testing::Types<int16_t, int32_t, int64_t, uint16_t, uint32_t, uint64_t>;
INSTANTIATE_TYPED_TEST_SUITE_P(AllRootedTreeTests, RootedTreeTest, NodeTypes);

////////////////////////////////////////////////////////////////////////////////
// Benchmarks
////////////////////////////////////////////////////////////////////////////////

std::vector<int> RandomTreeRootedZero(int num_nodes) {
  absl::BitGen bit_gen;
  std::vector<int> nodes(num_nodes);
  absl::c_iota(nodes, 0);
  std::shuffle(nodes.begin() + 1, nodes.end(), bit_gen);
  std::vector<int> result(num_nodes);
  result[0] = -1;
  for (int i = 1; i < num_nodes; ++i) {
    int target = absl::Uniform<int>(bit_gen, 0, i);
    result[i] = target;
  }
  return result;
}

void BM_RootedTreeShortestPath(benchmark::State& state) {
  const int num_nodes = state.range(0);
  std::vector<int> random_tree_data = RandomTreeRootedZero(num_nodes);
  for (auto s : state) {
    const RootedTree<int> tree =
        RootedTree<int>::Create(0, random_tree_data).value();
    std::vector<int> path = tree.PathToRoot(num_nodes - 1);
    CHECK_GE(path.size(), 2);
  }
}

BENCHMARK(BM_RootedTreeShortestPath)->Arg(100)->Arg(10'000)->Arg(1'000'000);

}  // namespace
}  // namespace operations_research
