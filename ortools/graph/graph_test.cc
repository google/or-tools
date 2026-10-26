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

#include "ortools/graph/graph.h"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <random>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/log/check.h"
#include "absl/random/random.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/strong_int.h"
#include "ortools/base/strong_vector.h"

namespace util {

using testing::_;
using testing::AllOf;
using testing::ElementsAre;
using testing::Field;
using testing::Pair;
using testing::Pointee;
using testing::UnorderedElementsAre;
using util::internal::SVector;

DEFINE_STRONG_INT_TYPE(StrongNodeId, int32_t);
DEFINE_STRONG_INT_TYPE(StrongArcId, int32_t);

// Iterators.
template <typename T>
concept IsInputIterator =
    std::input_iterator<T> &&
    // This is supposed to be part of the input_iterator concept but is not in
    // the current libc++ implementation.
    std::same_as<typename std::iterator_traits<T>::iterator_category,
                 std::input_iterator_tag>;

template <typename T>
concept IsForwardIterator =
    std::forward_iterator<T> &&
    std::same_as<typename std::iterator_traits<T>::iterator_category,
                 std::forward_iterator_tag>;

template <typename T>
concept IsRandomAccessIterator =
    std::random_access_iterator<T> &&
    std::same_as<typename std::iterator_traits<T>::iterator_category,
                 std::random_access_iterator_tag>;

class ConceptsTest : public ::testing::Test {
 protected:
  template <typename GraphT>
  struct IteratorTypes {
    static constexpr auto kNode0 = typename GraphT::NodeIndex(0);

    using OutgoingArcs =
        decltype(std::declval<GraphT>().OutgoingArcs(kNode0).begin());

    template <typename T = GraphT>
    using IncomingArcs =
        decltype(std::declval<T>().IncomingArcs(kNode0).begin());

    using OutgoingHeads = decltype(std::declval<GraphT>()[kNode0].begin());

    template <typename T = GraphT>
    using OppositeIncomingArcs =
        decltype(std::declval<T>().OppositeIncomingArcs(kNode0).begin());

    template <typename T = GraphT>
    using OutgoingOrOppositeIncomingArcs =
        decltype(std::declval<T>()
                     .OutgoingOrOppositeIncomingArcs(kNode0)
                     .begin());
  };
};

TEST_F(ConceptsTest, ListGraph) {
  using UntypedIterators = IteratorTypes<ListGraph<>>;

  static_assert(IsForwardIterator<UntypedIterators::OutgoingArcs>);
  static_assert(IsForwardIterator<UntypedIterators::OutgoingHeads>);

  using TypedIterators = IteratorTypes<ListGraph<StrongNodeId, StrongArcId>>;

  static_assert(IsForwardIterator<TypedIterators::OutgoingArcs>);
  static_assert(IsForwardIterator<TypedIterators::OutgoingHeads>);
}

TEST_F(ConceptsTest, StaticGraph) {
  using UntypedIterators = IteratorTypes<StaticGraph<>>;

  static_assert(IsRandomAccessIterator<UntypedIterators::OutgoingArcs>);
  static_assert(IsRandomAccessIterator<UntypedIterators::OutgoingHeads>);

  using TypedIterators = IteratorTypes<StaticGraph<StrongNodeId, StrongArcId>>;

  static_assert(IsRandomAccessIterator<TypedIterators::OutgoingArcs>);
  static_assert(IsRandomAccessIterator<TypedIterators::OutgoingHeads>);
}

TEST_F(ConceptsTest, ReverseArcListGraph) {
  using UntypedIterators = IteratorTypes<ReverseArcListGraph<>>;

  static_assert(IsForwardIterator<UntypedIterators::OutgoingArcs>);
  static_assert(IsForwardIterator<UntypedIterators::OutgoingHeads>);
  static_assert(IsForwardIterator<UntypedIterators::IncomingArcs<>>);
  static_assert(IsForwardIterator<UntypedIterators::OppositeIncomingArcs<>>);
  // TODO(user): Make this a forward iterator.
  static_assert(
      IsInputIterator<UntypedIterators::OutgoingOrOppositeIncomingArcs<>>);

  using TypedIterators =
      IteratorTypes<ReverseArcListGraph<StrongNodeId, StrongArcId>>;

  static_assert(IsForwardIterator<TypedIterators::OutgoingArcs>);
  static_assert(IsForwardIterator<TypedIterators::OutgoingHeads>);
  static_assert(IsForwardIterator<TypedIterators::IncomingArcs<>>);
  static_assert(IsForwardIterator<TypedIterators::OppositeIncomingArcs<>>);
  // TODO(user): Make this a forward iterator.
  static_assert(
      IsInputIterator<TypedIterators::OutgoingOrOppositeIncomingArcs<>>);
}

TEST_F(ConceptsTest, ReverseArcStaticGraph) {
  using UntypedIterators = IteratorTypes<ReverseArcStaticGraph<>>;

  static_assert(IsRandomAccessIterator<UntypedIterators::OutgoingArcs>);
  static_assert(IsRandomAccessIterator<UntypedIterators::OutgoingHeads>);
  // TODO(user): Make this a random access iterator.
  static_assert(IsForwardIterator<UntypedIterators::IncomingArcs<>>);
  static_assert(
      IsRandomAccessIterator<UntypedIterators::OppositeIncomingArcs<>>);
  // TODO(user): Make this a random access iterator.
  static_assert(
      IsInputIterator<UntypedIterators::OutgoingOrOppositeIncomingArcs<>>);

  using TypedIterators =
      IteratorTypes<ReverseArcStaticGraph<StrongNodeId, StrongArcId>>;

  static_assert(IsRandomAccessIterator<TypedIterators::OutgoingArcs>);
  static_assert(IsRandomAccessIterator<TypedIterators::OutgoingHeads>);
  // TODO(user): Make this a random access iterator.
  static_assert(IsForwardIterator<TypedIterators::IncomingArcs<>>);
  static_assert(IsRandomAccessIterator<TypedIterators::OppositeIncomingArcs<>>);
  // TODO(user): Make this a random access iterator.
  static_assert(
      IsInputIterator<TypedIterators::OutgoingOrOppositeIncomingArcs<>>);
}

// GraphTraits.
static_assert(
    std::is_same_v<typename GraphTraits<ListGraph<int32_t, int16_t>>::NodeIndex,
                   int32_t>);
static_assert(
    std::is_same_v<
        typename GraphTraits<ReverseArcListGraph<int16_t, int32_t>>::NodeIndex,
        int16_t>);
static_assert(std::is_same_v<
              typename GraphTraits<StaticGraph<uint32_t, int16_t>>::NodeIndex,
              uint32_t>);
static_assert(
    std::is_same_v<
        typename GraphTraits<StaticGraph<StrongNodeId, StrongArcId>>::NodeIndex,
        StrongNodeId>);
static_assert(
    std::is_same_v<
        typename GraphTraits<std::vector<std::vector<int>>>::NodeIndex, int>);

// Check that the OutgoingArcs() returns exactly the same arcs as the verifier.
// This also test Head(), Tail(), and OutDegree().
template <typename GraphType>
void CheckOutgoingArcIterator(
    const GraphType& graph,
    absl::Span<const std::vector<typename GraphType::NodeIndex>> verifier) {
  using NodeIndex = typename GraphType::NodeIndex;
  using ArcIndex = typename GraphType::ArcIndex;
  std::vector<int> node_seen(verifier.size(), 0);
  for (int i = 0; i < verifier.size(); ++i) {
    for (int j = 0; j < verifier[i].size(); ++j) {
      // We have to use int because there can be multiple arcs.
      node_seen[static_cast<size_t>(verifier[i][j])]++;
    }
    int outgoing_arc_number = 0;
    for (const ArcIndex arc : graph.OutgoingArcs(NodeIndex(i))) {
      const int head = static_cast<int>(graph.Head(arc));
      const int tail = static_cast<int>(graph.Tail(arc));
      EXPECT_GE(head, 0);
      EXPECT_LT(head, verifier.size());
      EXPECT_GT(node_seen[head], 0);
      node_seen[head]--;
      EXPECT_EQ(i, tail);
      EXPECT_EQ(arc,
                *(graph.OutgoingArcsStartingFrom(NodeIndex(i), arc).begin()));
      ++outgoing_arc_number;
    }
    // If this is true, then node_seen must have been cleaned.
    EXPECT_EQ(verifier[i].size(), outgoing_arc_number);
    EXPECT_EQ(ArcIndex(verifier[i].size()), graph.OutDegree(NodeIndex(i)));
  }
}

// Check that the operator[] returns exactly the same nodes as the verifier.
template <typename GraphType>
void CheckOutgoingHeadIterator(
    const GraphType& graph,
    absl::Span<const std::vector<typename GraphType::NodeIndex>> verifier) {
  using NodeIndex = typename GraphType::NodeIndex;
  using ArcIndex = typename GraphType::ArcIndex;
  std::vector<int> node_seen(verifier.size(), 0);
  for (int i = 0; i < verifier.size(); ++i) {
    for (int j = 0; j < verifier[i].size(); ++j) {
      // We have to use int because there can be multiple arcs.
      node_seen[static_cast<size_t>(verifier[i][j])]++;
    }
    int outgoing_head_number = 0;
    for (const NodeIndex node : graph[NodeIndex(i)]) {
      const int node_id = static_cast<int>(node);
      EXPECT_GE(node_id, 0);
      EXPECT_LT(node_id, verifier.size());
      EXPECT_GT(node_seen[node_id], 0);
      node_seen[node_id]--;
      ++outgoing_head_number;
    }
    // If this is true, then node_seen must have been cleaned.
    EXPECT_EQ(verifier[i].size(), outgoing_head_number);
    EXPECT_EQ(ArcIndex(verifier[i].size()), graph.OutDegree(NodeIndex(i)));
  }
}

// Check that the heads of OutgoingArcs() + the tails of IncomingArcs() are the
// same as the heads of OutgoingOrOppositeIncomingArcs(). Also perform
// various checks on the arcs.
template <typename GraphType>
void CheckReverseArcIterator(const GraphType& graph) {
  using NodeIndex = typename GraphType::NodeIndex;
  using ArcIndex = typename GraphType::ArcIndex;
  ArcIndex total_arc_number(0);
  std::vector<int> node_seen(static_cast<size_t>(graph.num_nodes()), 0);
  for (const NodeIndex node : graph.AllNodes()) {
    ArcIndex num_incident_arcs(0);
    for (const ArcIndex arc : graph.OutgoingOrOppositeIncomingArcs(node)) {
      EXPECT_EQ(node, graph.Tail(arc));
      EXPECT_EQ(arc,
                *(graph.OutgoingOrOppositeIncomingArcsStartingFrom(node, arc)
                      .begin()));
      node_seen[static_cast<size_t>(graph.Head(arc))]++;
      ++num_incident_arcs;
    }
    total_arc_number += num_incident_arcs;
    ArcIndex num_outgoing_arcs(0);
    for (const ArcIndex arc : graph.OutgoingArcs(node)) {
      EXPECT_GE(arc, ArcIndex(0));
      EXPECT_EQ(node, graph.Tail(arc));
      EXPECT_EQ(arc, *(graph.OutgoingArcsStartingFrom(node, arc).begin()));
      const size_t head = static_cast<size_t>(graph.Head(arc));
      EXPECT_GE(node_seen[head], 0);
      node_seen[head]--;
      ++num_outgoing_arcs;
    }
    EXPECT_EQ(num_outgoing_arcs, graph.OutDegree(node));
    ArcIndex num_incoming_arcs(0);
    for (const ArcIndex arc : graph.IncomingArcs(node)) {
      EXPECT_GE(arc, ArcIndex(0));
      EXPECT_EQ(node, graph.Head(arc));
      EXPECT_EQ(arc, *(graph.IncomingArcsStartingFrom(node, arc).begin()));
      const size_t tail = static_cast<size_t>(graph.Tail(arc));
      node_seen[tail]--;
      EXPECT_GE(node_seen[tail], 0);
      ++num_incoming_arcs;
    }
    EXPECT_EQ(num_incoming_arcs, graph.InDegree(node));
    // If this is true, then node_seen must have been cleaned.
    EXPECT_EQ(num_incident_arcs, num_outgoing_arcs + num_incoming_arcs);
  }
  EXPECT_EQ(2 * graph.num_arcs(), total_arc_number);
}

// Check that the arcs returned by OppositeIncomingArcs() are exactly the
// reverse of the arcs returned by IncomingArcs().
template <typename GraphType>
void CheckOppositeIncomingArcs(const GraphType& graph) {
  using NodeIndex = typename GraphType::NodeIndex;
  using ArcIndex = typename GraphType::ArcIndex;
  std::vector<ArcIndex> arcs;
  std::vector<ArcIndex> opposite_arcs;
  for (const NodeIndex node : graph.AllNodes()) {
    arcs.clear();
    opposite_arcs.clear();
    for (const ArcIndex arc : graph.IncomingArcs(node)) {
      arcs.push_back(arc);
    }
    for (const ArcIndex arc : graph.OppositeIncomingArcs(node)) {
      opposite_arcs.push_back(arc);
    }
    ASSERT_EQ(arcs.size(), opposite_arcs.size());
    for (int a = 0; a < arcs.size(); ++a) {
      ASSERT_EQ(opposite_arcs[a], graph.OppositeArc(arcs[a]));
    }
  }
}

template <typename GraphType>
void CheckReverseArc(const GraphType& graph) {}

template <typename NodeIndexType, typename ArcIndexType>
void CheckReverseArc(
    const ReverseArcListGraph<NodeIndexType, ArcIndexType>& graph) {
  CheckReverseArcIterator(graph);
  CheckOppositeIncomingArcs(graph);
}

template <typename NodeIndexType, typename ArcIndexType>
void CheckReverseArc(
    const ReverseArcStaticGraph<NodeIndexType, ArcIndexType>& graph) {
  CheckReverseArcIterator(graph);
  CheckOppositeIncomingArcs(graph);
}

// Check that arc annotation can be permuted properly. This is achieved
// by "annotating" the original arc index with the head and tail information
// and checking that after permutation the annotation of a given arc index
// matches is actual head and tail in the graph.
template <typename GraphType>
void CheckArcIndexPermutation(
    const GraphType& graph,
    const std::vector<typename GraphType::ArcIndex>& permutation,
    const std::vector<typename GraphType::NodeIndex>& heads,
    const std::vector<typename GraphType::NodeIndex>& tails) {
  using NodeIndex = typename GraphType::NodeIndex;
  using ArcIndex = typename GraphType::ArcIndex;
  std::vector<NodeIndex> annotation_h(heads);
  std::vector<NodeIndex> annotation_t(tails);
  Permute(permutation, &annotation_h);
  Permute(permutation, &annotation_t);
  for (ArcIndex arc : graph.AllForwardArcs()) {
    CHECK_EQ(annotation_h[static_cast<size_t>(arc)], graph.Head(arc));
    CHECK_EQ(annotation_t[static_cast<size_t>(arc)], graph.Tail(arc));
  }
}

template <typename GraphType>
void ConstructAndCheckGraph(
    const typename GraphType::NodeIndex num_nodes,
    const typename GraphType::ArcIndex num_arcs,
    const std::vector<typename GraphType::NodeIndex>& heads,
    const std::vector<typename GraphType::NodeIndex>& tails, bool reserve,
    bool test_permutation) {
  using NodeIndex = typename GraphType::NodeIndex;
  using ArcIndex = typename GraphType::ArcIndex;
  auto builder = reserve ? typename GraphType::Builder(num_nodes, num_arcs)
                         : typename GraphType::Builder();
  std::vector<std::vector<NodeIndex>> verifier(static_cast<size_t>(num_nodes));

  for (ArcIndex i(0); i < num_arcs; ++i) {
    NodeIndex head = heads[static_cast<size_t>(i)];
    NodeIndex tail = tails[static_cast<size_t>(i)];
    EXPECT_EQ(i, builder.AddArc(tail, head));
    verifier[static_cast<size_t>(tail)].push_back(head);
  }
  std::vector<typename GraphType::ArcIndex> permutation;
  const auto graph =
      std::move(builder).Build(test_permutation ? &permutation : nullptr);

  EXPECT_EQ(num_nodes, graph->num_nodes());
  EXPECT_EQ(num_nodes, size(*graph));
  EXPECT_EQ(num_arcs, graph->num_arcs());
  CheckOutgoingArcIterator(*graph, verifier);
  CheckOutgoingHeadIterator(*graph, verifier);
  if (test_permutation) {
    CheckArcIndexPermutation(*graph, permutation, heads, tails);
  }
  CheckReverseArc(*graph);
}

// Return the size of the memory block allocated by malloc when asking for x
// bytes.
template <typename IndexType>
inline IndexType UpperBoundOfMallocBlockSizeOf(IndexType x) {
  // NOTE(user): as of 2012-09, the rule seems to be: round x up to the
  // next multiple of 16.
  // WARNING: This may change, and may already be wrong for small values.
  return IndexType((16 * (static_cast<int64_t>(x) + 15)) / 16);
}

TEST(SvectorTest, SVectorGrow) {
  SVector<int, int> svector;
  svector.grow(1, 2);
  EXPECT_EQ(svector.size(), 1);
  EXPECT_EQ(svector[-1], 1);
  EXPECT_EQ(svector[0], 2);

  svector.grow(3, 4);
  EXPECT_EQ(svector.size(), 2);
  EXPECT_EQ(svector[-2], 3);
  EXPECT_EQ(svector[-1], 1);
  EXPECT_EQ(svector[0], 2);
  EXPECT_EQ(svector[1], 4);
}

TEST(SvectorTest, SVectorCapacity) {
  SVector<int, int> svector;
  static constexpr int kMinCapacity = svector.kMinCapacity;
  EXPECT_EQ(svector.size(), 0);
  EXPECT_EQ(svector.capacity(), 0);

  svector.grow(1, 2);
  EXPECT_EQ(svector.size(), 1);
  EXPECT_EQ(svector.capacity(), kMinCapacity);

  while (svector.size() < kMinCapacity) {
    svector.grow(1, 2);
  }
  EXPECT_EQ(svector.size(), kMinCapacity);
  EXPECT_EQ(svector.capacity(), kMinCapacity);

  svector.grow(1, 2);
  EXPECT_EQ(svector.size(), kMinCapacity + 1);
  EXPECT_GT(svector.capacity(), kMinCapacity);
}

TEST(SvectorTest, SVectorResize) {
  SVector<int, int> svector;
  svector.resize(10);
  EXPECT_EQ(svector.size(), 10);
  EXPECT_GE(svector.capacity(), 10);
  svector.resize(5);
  EXPECT_EQ(svector.size(), 5);
  EXPECT_GE(svector.capacity(), 10);
}

TEST(SvectorTest, SVectorWithNegativeIndex) {
  SVector<int, int> svector;
  svector.grow(1, 2);
  EXPECT_EQ(svector[-1], 1);
  EXPECT_EQ(svector[0], 2);
}

DEFINE_STRONG_INT_TYPE(MyInt, int64_t);
TEST(SvectorTest, SVectorWithStrongIndex) {
  SVector<MyInt, int> svector;
  svector.grow(1, 2);
  EXPECT_EQ(svector[MyInt(-1)], 1);
  EXPECT_EQ(svector[MyInt(0)], 2);
}

TEST(SvectorTest, SVectorDataOnEmpty) {
  SVector<int, int> svector;
  EXPECT_EQ(svector.data(), nullptr);
  const auto& const_svector = svector;
  EXPECT_EQ(const_svector.data(), nullptr);
}

TEST(SvectorTest, SVectorGrowthPreservesAllElements) {
  SVector<int, int> svector;
  for (int i = 0; i < 20; ++i) {
    svector.grow(~i, i);
  }
  EXPECT_EQ(svector.size(), 20);
  for (int i = 0; i < 20; ++i) {
    EXPECT_EQ(svector[~i], ~i);
    EXPECT_EQ(svector[i], i);
  }
}

TEST(SvectorTest, SVectorCopyAndMove) {
  SVector<int, int> svector;
  svector.grow(10, 20);
  svector.grow(30, 40);

  // Copy construction
  SVector<int, int> copy = svector;
  EXPECT_EQ(copy.size(), 2);
  EXPECT_EQ(copy[-2], 30);
  EXPECT_EQ(copy[-1], 10);
  EXPECT_EQ(copy[0], 20);
  EXPECT_EQ(copy[1], 40);

  // Modifying copy should not modify original
  copy[0] = 99;
  EXPECT_EQ(copy[0], 99);
  EXPECT_EQ(svector[0], 20);

  // Move construction
  SVector<int, int> moved = std::move(copy);
  EXPECT_EQ(moved.size(), 2);
  EXPECT_EQ(moved[0], 99);
}

TEST(SvectorTest, ValueInitializationOnResize) {
  SVector<int, int> v;
  v.resize(40);
  for (int i = 0; i < 40; ++i) {
    EXPECT_EQ(v[~i], 0);
    EXPECT_EQ(v[i], 0);
  }
  for (int i = 0; i < 40; ++i) {
    v[~i] = 42;
    v[i] = 42;
  }
  v.resize(20);
  v.resize(40);
  for (int i = 0; i < 20; ++i) {
    EXPECT_EQ(v[~i], 42);
    EXPECT_EQ(v[i], 42);
  }
  for (int i = 20; i < 40; ++i) {
    EXPECT_EQ(v[~i], 0);
    EXPECT_EQ(v[i], 0);
  }
}

TEST(SvectorTest, SelfAssignment) {
  SVector<int, int> v;
  v.grow(10, 20);
  v.grow(30, 40);

  // Self copy assignment
  auto* const v_ptr = &v;
  v = *v_ptr;
  EXPECT_EQ(v.size(), 2);
  EXPECT_EQ(v[-2], 30);
  EXPECT_EQ(v[-1], 10);
  EXPECT_EQ(v[0], 20);
  EXPECT_EQ(v[1], 40);

  // Self move assignment
  v = std::move(*v_ptr);
  EXPECT_EQ(v.size(), 2);
  EXPECT_EQ(v[-2], 30);
  EXPECT_EQ(v[-1], 10);
  EXPECT_EQ(v[0], 20);
  EXPECT_EQ(v[1], 40);
}

DEFINE_STRONG_INT_TYPE(StrongInt16, int16_t);

template <typename IndexT>
void TestGrowUntilCapacityAndSizeBoundaries() {
  SVector<IndexT, char> s;
  constexpr IndexT kMaxCapacity = std::numeric_limits<IndexT>::max();
  while (s.size() < kMaxCapacity) {
    s.grow('a', 'b');
  }
  EXPECT_EQ(s.size(), kMaxCapacity);
  EXPECT_EQ(s.capacity(), kMaxCapacity);
  EXPECT_EQ('a', s[-kMaxCapacity]);
  EXPECT_EQ('b', s[kMaxCapacity - IndexT(1)]);
  // Verify that the [] operators aren't flawed by changing the extremal values
  // and verifying that only those appear changed.
  s[-kMaxCapacity] = 'A';
  s[kMaxCapacity - IndexT(1)] = 'B';
  EXPECT_EQ('A', s[-kMaxCapacity]);
  EXPECT_EQ('B', s[kMaxCapacity - IndexT(1)]);
  for (IndexT i = -kMaxCapacity + IndexT(1); i < IndexT(0); ++i) {
    EXPECT_EQ('a', s[i]);
  }
  for (IndexT i = IndexT(0); i < kMaxCapacity - IndexT(1); ++i) {
    EXPECT_EQ('b', s[i]);
  }
}

TEST(SvectorTest, GrowUntilCapacityAndSizeBoundaries) {
  TestGrowUntilCapacityAndSizeBoundaries<int16_t>();
}

TEST(SvectorTest, GrowUntilCapacityAndSizeBoundariesWithStrongIndex) {
  TestGrowUntilCapacityAndSizeBoundaries<StrongInt16>();
}

template <typename IndexType>
class SVectorTest : public ::testing::Test {};

typedef ::testing::Types<std::pair<int, int>, std::pair<int, StrongArcId>,
                         std::pair<StrongArcId, int>,
                         std::pair<StrongArcId, StrongArcId>>
    TestSVectorIndexTypes;

TYPED_TEST_SUITE(SVectorTest, TestSVectorIndexTypes);

TYPED_TEST(SVectorTest, DynamicGrowth) {
  using IndexT = typename TypeParam::first_type;
  using ValueT = typename TypeParam::second_type;
  internal::SVector<IndexT, ValueT> v;
  EXPECT_EQ(IndexT(0), v.size());
  EXPECT_EQ(IndexT(0), v.capacity());
  for (ValueT i(0); i < ValueT(100); i++) {
    v.grow(-i, i);
  }
  EXPECT_EQ(IndexT(100), v.size());
  EXPECT_GE(v.capacity(), IndexT(100));
  for (IndexT i(0); i < IndexT(100); ++i) {
    EXPECT_EQ(ValueT(static_cast<int>(-i)), v[~i]);
    EXPECT_EQ(ValueT(static_cast<int>(i)), v[i]);
  }
}

TYPED_TEST(SVectorTest, Reserve) {
  using IndexT = typename TypeParam::first_type;
  using ValueT = typename TypeParam::second_type;
  internal::SVector<IndexT, ValueT> v;
  v.reserve(IndexT(100));
  EXPECT_EQ(IndexT(0), v.size());
  EXPECT_GE(v.capacity(), IndexT(100));
  for (ValueT i(0); i < ValueT(100); i++) {
    v.grow(-i, i);
  }
  EXPECT_EQ(IndexT(100), v.size());
  EXPECT_GE(v.capacity(), IndexT(100));
  for (IndexT i(0); i < IndexT(10); i++) {
    EXPECT_EQ(ValueT(static_cast<int>(-i)), v[~i]);
    EXPECT_EQ(ValueT(static_cast<int>(i)), v[i]);
  }
}

TYPED_TEST(SVectorTest, Resize) {
  using IndexT = typename TypeParam::first_type;
  using ValueT = typename TypeParam::second_type;
  internal::SVector<IndexT, ValueT> v;
  v.resize(IndexT(100));
  EXPECT_EQ(IndexT(100), v.size());
  EXPECT_GE(v.capacity(), IndexT(100));
  for (IndexT i(0); i < IndexT(100); ++i) {
    EXPECT_EQ(ValueT(0), v[-i - IndexT(1)]);
    EXPECT_EQ(ValueT(0), v[i]);
  }
}

TYPED_TEST(SVectorTest, ResizeToZero) {
  using IndexT = typename TypeParam::first_type;
  using ValueT = typename TypeParam::second_type;
  internal::SVector<IndexT, ValueT> v;
  v.resize(IndexT(1));
  v.resize(IndexT(0));
  EXPECT_EQ(IndexT(0), v.size());
}

TYPED_TEST(SVectorTest, CopyAndMove) {
  using IndexT = typename TypeParam::first_type;
  using ValueT = typename TypeParam::second_type;
  using VectorT = internal::SVector<IndexT, ValueT>;
  VectorT v;
  v.resize(IndexT(2));
  v[IndexT(-2)] = ValueT(10);
  v[IndexT(-1)] = ValueT(20);
  v[IndexT(0)] = ValueT(30);
  v[IndexT(1)] = ValueT(40);

  // Copy construction
  VectorT copy(v);
  EXPECT_EQ(IndexT(2), copy.size());
  EXPECT_EQ(ValueT(10), copy[IndexT(-2)]);
  EXPECT_EQ(ValueT(20), copy[IndexT(-1)]);
  EXPECT_EQ(ValueT(30), copy[IndexT(0)]);
  EXPECT_EQ(ValueT(40), copy[IndexT(1)]);

  // Copy assignment
  VectorT copy2;
  copy2 = v;
  EXPECT_EQ(IndexT(2), copy2.size());
  EXPECT_EQ(ValueT(10), copy2[IndexT(-2)]);
  EXPECT_EQ(ValueT(20), copy2[IndexT(-1)]);
  EXPECT_EQ(ValueT(30), copy2[IndexT(0)]);
  EXPECT_EQ(ValueT(40), copy2[IndexT(1)]);

  // Move construction
  VectorT moved(std::move(copy));
  EXPECT_EQ(IndexT(2), moved.size());
  EXPECT_EQ(ValueT(10), moved[IndexT(-2)]);
  EXPECT_EQ(ValueT(20), moved[IndexT(-1)]);
  EXPECT_EQ(ValueT(30), moved[IndexT(0)]);
  EXPECT_EQ(ValueT(40), moved[IndexT(1)]);

  // Move assignment
  VectorT moved2;
  moved2 = std::move(copy2);
  EXPECT_EQ(IndexT(2), moved2.size());
  EXPECT_EQ(ValueT(10), moved2[IndexT(-2)]);
  EXPECT_EQ(ValueT(20), moved2[IndexT(-1)]);
  EXPECT_EQ(ValueT(30), moved2[IndexT(0)]);
  EXPECT_EQ(ValueT(40), moved2[IndexT(1)]);
}

TEST(SVectorStringTest, DynamicSize) {
  internal::SVector<int, std::string> s;
  s.resize(10);
  for (int i = 0; i < 10; ++i) {
    s[i] = "Right";
    s[~i] = "Left";
  }
  ASSERT_LT(s.capacity(), 50);
  for (int i = 0; i < 50; ++i) s.grow("NewLeft", "NewRight");
  s.resize(10);
  for (int i = 0; i < 50; ++i) s.grow("NewNewLeft", "NewNewRight");
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ("Left", s[~i]);
    EXPECT_EQ("Right", s[i]);
  }
  for (int i = 10; i < 10 + 50; ++i) {
    EXPECT_EQ("NewNewLeft", s[~i]);
    EXPECT_EQ("NewNewRight", s[i]);
  }
}

class MoveOnlyObject {
 public:
  MoveOnlyObject() : id_(std::make_unique<int>(sequence_++)) {
    ++object_count_;
    Validate();
  }
  ~MoveOnlyObject() {
    Validate();
    --object_count_;
    CHECK_GE(object_count_, 0);
  }
  MoveOnlyObject(const MoveOnlyObject&) = delete;
  MoveOnlyObject(MoveOnlyObject&& other) : id_(std::move(other.id_)) {
    ++object_count_;
    other.id_ = std::make_unique<int>(sequence_++);
    Validate();
    other.Validate();
  }
  MoveOnlyObject& operator=(const MoveOnlyObject&) = delete;
  MoveOnlyObject& operator=(MoveOnlyObject&& other) {
    using std::swap;
    swap(id_, other.id_);
    Validate();
    other.Validate();
    return *this;
  }

  static int GetObjectCount() { return object_count_; }

 private:
  void Validate() {
    EXPECT_TRUE(id_ != nullptr);
    if (id_ != nullptr) {
      EXPECT_GT(*id_, 0);
      EXPECT_LT(*id_, sequence_);
    }
  }

  static int sequence_;
  static int object_count_;
  std::unique_ptr<int> id_;
};

int MoveOnlyObject::sequence_ = 1;
int MoveOnlyObject::object_count_ = 0;

TEST(SVectorMoveOnlyTest, MoveWithMoveOnlyObject) {
  EXPECT_EQ(0, MoveOnlyObject::GetObjectCount());
  internal::SVector<int, MoveOnlyObject> a;
  a.resize(10);
  EXPECT_EQ(10, a.size());
  EXPECT_EQ(20, MoveOnlyObject::GetObjectCount());

  internal::SVector<int, MoveOnlyObject> b = std::move(a);
  EXPECT_EQ(10, b.size());
  EXPECT_EQ(20, MoveOnlyObject::GetObjectCount());
  EXPECT_EQ(0, a.size());  // NOLINT
}

TEST(SVectorMoveOnlyTest, ShrinkWithMoveOnlyObject) {
  EXPECT_EQ(0, MoveOnlyObject::GetObjectCount());
  {
    internal::SVector<int, MoveOnlyObject> a;
    a.resize(10);
    EXPECT_EQ(20, MoveOnlyObject::GetObjectCount());
    a.resize(5);
    EXPECT_EQ(10, MoveOnlyObject::GetObjectCount());
  }
  EXPECT_EQ(0, MoveOnlyObject::GetObjectCount());
}

TEST(SVectorMoveOnlyTest, ResizeWithMoveOnlyObject) {
  EXPECT_EQ(0, MoveOnlyObject::GetObjectCount());
  {
    internal::SVector<int, MoveOnlyObject> a;
    a.resize(10);
    EXPECT_EQ(a.size() * 2, MoveOnlyObject::GetObjectCount());

    MoveOnlyObject* const original_data = a.data();
    while (original_data == a.data()) {
      a.resize(a.size() + 1);
      EXPECT_EQ(a.size() * 2, MoveOnlyObject::GetObjectCount());
    }
  }
  EXPECT_EQ(0, MoveOnlyObject::GetObjectCount());
}

TEST(SVectorMoveOnlyTest, ReserveWithMoveOnlyObject) {
  EXPECT_EQ(0, MoveOnlyObject::GetObjectCount());
  {
    internal::SVector<int, MoveOnlyObject> a;
    a.resize(10);
    EXPECT_EQ(a.size() * 2, MoveOnlyObject::GetObjectCount());

    MoveOnlyObject* const original_data = a.data();
    while (original_data == a.data()) {
      a.reserve(a.size() * 2);
      EXPECT_EQ(a.size() * 2, MoveOnlyObject::GetObjectCount());
    }
  }
  EXPECT_EQ(0, MoveOnlyObject::GetObjectCount());
}

TEST(SVectorMoveOnlyTest, GrowWithMoveOnlyObject) {
  EXPECT_EQ(0, MoveOnlyObject::GetObjectCount());
  {
    internal::SVector<int, MoveOnlyObject> a;
    a.grow(MoveOnlyObject(), MoveOnlyObject());
    EXPECT_EQ(1, a.size());
    EXPECT_EQ(2, MoveOnlyObject::GetObjectCount());
    for (int i = 0; i < 50; ++i) {
      a.grow(MoveOnlyObject(), MoveOnlyObject());
    }
    EXPECT_EQ(51, a.size());
    EXPECT_EQ(102, MoveOnlyObject::GetObjectCount());
  }
  EXPECT_EQ(0, MoveOnlyObject::GetObjectCount());
}

struct TrackedObject {
  static int num_constructions;
  static int num_destructions;
  static int num_moves;
  static int num_copies;
  static void ResetCounters() {
    num_constructions = 0;
    num_destructions = 0;
    num_moves = 0;
    num_copies = 0;
  }
  static std::string Counters() {
    return absl::StrCat("constructions: ", num_constructions,
                        ", destructions: ", num_destructions,
                        ", moves: ", num_moves, ", copies: ", num_copies);
  }

  TrackedObject() { ++num_constructions; }
  ~TrackedObject() { ++num_destructions; }
  TrackedObject(TrackedObject&&) { ++num_moves; }
  TrackedObject(const TrackedObject&) { ++num_copies; }
  TrackedObject& operator=(const TrackedObject&) {
    ++num_copies;
    return *this;
  }
  TrackedObject& operator=(TrackedObject&&) {
    ++num_moves;
    return *this;
  }
};

int TrackedObject::num_constructions = 0;
int TrackedObject::num_destructions = 0;
int TrackedObject::num_moves = 0;
int TrackedObject::num_copies = 0;

TEST(SVectorTrackingTest, CopyConstructor) {
  TrackedObject::ResetCounters();
  ASSERT_EQ(TrackedObject::Counters(),
            "constructions: 0, destructions: 0, moves: 0, copies: 0");
  auto v = std::make_unique<internal::SVector<int, TrackedObject>>();
  ASSERT_EQ(TrackedObject::Counters(),
            "constructions: 0, destructions: 0, moves: 0, copies: 0");
  v->resize(5);
  ASSERT_EQ(TrackedObject::Counters(),
            "constructions: 10, destructions: 0, moves: 0, copies: 0");
  auto v_copy(*v);
  ASSERT_EQ(TrackedObject::Counters(),
            "constructions: 10, destructions: 0, moves: 0, copies: 10");
  v.reset(nullptr);
  ASSERT_EQ(TrackedObject::Counters(),
            "constructions: 10, destructions: 10, moves: 0, copies: 10");
  ASSERT_EQ(v_copy.size(), 5);
}

TEST(SVectorTrackingTest, AssignmentOperator) {
  TrackedObject::ResetCounters();
  ASSERT_EQ(TrackedObject::Counters(),
            "constructions: 0, destructions: 0, moves: 0, copies: 0");
  auto v = std::make_unique<internal::SVector<int, TrackedObject>>();
  ASSERT_EQ(TrackedObject::Counters(),
            "constructions: 0, destructions: 0, moves: 0, copies: 0");
  v->resize(5);
  ASSERT_EQ(TrackedObject::Counters(),
            "constructions: 10, destructions: 0, moves: 0, copies: 0");
  internal::SVector<int, TrackedObject> other;
  ASSERT_EQ(TrackedObject::Counters(),
            "constructions: 10, destructions: 0, moves: 0, copies: 0");
  other = *v;
  ASSERT_EQ(TrackedObject::Counters(),
            "constructions: 10, destructions: 0, moves: 0, copies: 10");
  v.reset(nullptr);
  ASSERT_EQ(TrackedObject::Counters(),
            "constructions: 10, destructions: 10, moves: 0, copies: 10");
  ASSERT_EQ(other.size(), 5);
}

TEST(SVectorTrackingTest, AssignmentOverwritesExistingElements) {
  TrackedObject::ResetCounters();
  internal::SVector<int, TrackedObject> a;
  a.resize(3);
  EXPECT_EQ(TrackedObject::Counters(),
            "constructions: 6, destructions: 0, moves: 0, copies: 0");
  {
    internal::SVector<int, TrackedObject> b;
    b.resize(5);
    EXPECT_EQ(TrackedObject::Counters(),
              "constructions: 16, destructions: 0, moves: 0, copies: 0");
    a = b;
    EXPECT_EQ(TrackedObject::Counters(),
              "constructions: 16, destructions: 6, moves: 0, copies: 10");
  }
  EXPECT_EQ(TrackedObject::Counters(),
            "constructions: 16, destructions: 16, moves: 0, copies: 10");
  EXPECT_EQ(a.size(), 5);
}

TEST(SVectorTrackingTest, CopyConstructorIntegralType) {
  auto v = internal::SVector<int, int32_t>();
  v.resize(3);
  v[-3] = 1;
  v[-2] = 2;
  v[-1] = 3;
  v[0] = 1;
  v[1] = 2;
  v[2] = 3;

  auto other = internal::SVector<int, int32_t>(v);

  ASSERT_EQ(v.size(), other.size());
  for (int i = -v.size(); i < v.size(); i++) {
    ASSERT_EQ(v[i], other[i]);
  }
}

TEST(SVectorTrackingTest, AssignmentOperatorIntegralType) {
  internal::SVector<int, int32_t> other;
  auto v = internal::SVector<int, int32_t>();
  v.resize(3);
  v[-3] = 1;
  v[-2] = 2;
  v[-1] = 3;
  v[0] = 1;
  v[1] = 2;
  v[2] = 3;

  other = v;

  ASSERT_EQ(v.size(), other.size());
  for (int i = -v.size(); i < v.size(); i++) {
    ASSERT_EQ(v[i], other[i]);
  }
}

TEST(SVectorTrackingTest, MoveConstructor) {
  TrackedObject::ResetCounters();
  ASSERT_EQ(TrackedObject::Counters(),
            "constructions: 0, destructions: 0, moves: 0, copies: 0");
  internal::SVector<int, TrackedObject> a;
  ASSERT_EQ(TrackedObject::Counters(),
            "constructions: 0, destructions: 0, moves: 0, copies: 0");
  a.resize(5);
  ASSERT_EQ(TrackedObject::Counters(),
            "constructions: 10, destructions: 0, moves: 0, copies: 0");
  internal::SVector<int, TrackedObject> b = std::move(a);
  ASSERT_EQ(TrackedObject::Counters(),
            "constructions: 10, destructions: 0, moves: 0, copies: 0");
  ASSERT_EQ(b.size(), 5);
}

TEST(SVectorTrackingTest, MoveAssignmentOperator) {
  TrackedObject::ResetCounters();
  ASSERT_EQ(TrackedObject::Counters(),
            "constructions: 0, destructions: 0, moves: 0, copies: 0");
  internal::SVector<int, TrackedObject> a;
  a.resize(3);
  ASSERT_EQ(TrackedObject::Counters(),
            "constructions: 6, destructions: 0, moves: 0, copies: 0");
  {
    internal::SVector<int, TrackedObject> b;
    b.resize(5);
    ASSERT_EQ(TrackedObject::Counters(),
              "constructions: 16, destructions: 0, moves: 0, copies: 0");
    a = std::move(b);
    ASSERT_EQ(TrackedObject::Counters(),
              "constructions: 16, destructions: 6, moves: 0, copies: 0");
  }
  ASSERT_EQ(a.size(), 5);
}

TEST(SVectorTrackingTest, ReserveMovesAndDestroysOldElements) {
  TrackedObject::ResetCounters();
  internal::SVector<int, TrackedObject> v;
  v.resize(5);
  EXPECT_EQ(TrackedObject::Counters(),
            "constructions: 10, destructions: 0, moves: 0, copies: 0");
  const size_t cap = v.capacity();
  v.reserve(cap + 10);
  EXPECT_EQ(TrackedObject::Counters(),
            "constructions: 10, destructions: 10, moves: 10, copies: 0");
  EXPECT_EQ(v.size(), 5);
}

TEST(SVectorTrackingTest, ClearDestroysAllElements) {
  TrackedObject::ResetCounters();
  internal::SVector<int, TrackedObject> v;
  v.resize(5);
  EXPECT_EQ(TrackedObject::Counters(),
            "constructions: 10, destructions: 0, moves: 0, copies: 0");
  v.resize(0);
  EXPECT_EQ(TrackedObject::Counters(),
            "constructions: 10, destructions: 10, moves: 0, copies: 0");
  EXPECT_EQ(v.size(), 0);
}

template <typename GraphType>
class GenericGraphInterfaceTest : public ::testing::Test {};

typedef ::testing::Types<
    ListGraph<int16_t, int16_t>, ListGraph<int16_t, int32_t>,
    ListGraph<int32_t, int32_t>, ListGraph<uint32_t, uint32_t>,
    ListGraph<StrongNodeId, StrongArcId>, ReverseArcListGraph<int16_t, int32_t>,
    ReverseArcListGraph<int16_t, int32_t>,
    ReverseArcListGraph<int32_t, int32_t>,
    ReverseArcListGraph<uint32_t, int32_t>,
    ReverseArcListGraph<StrongNodeId, StrongArcId>,
    ReverseArcStaticGraph<int16_t, int32_t>,
    ReverseArcStaticGraph<int16_t, int32_t>,
    ReverseArcStaticGraph<int32_t, int32_t>,
    ReverseArcStaticGraph<uint32_t, int32_t>,
    ReverseArcStaticGraph<StrongNodeId, StrongArcId>,
    StaticGraph<int16_t, int32_t>, StaticGraph<int16_t, int32_t>,
    StaticGraph<int32_t, int32_t>, StaticGraph<uint32_t, uint32_t>,
    StaticGraph<StrongNodeId, StrongArcId>>
    GraphType;

TYPED_TEST_SUITE(GenericGraphInterfaceTest, GraphType);

TYPED_TEST(GenericGraphInterfaceTest, EmptyGraph) {
  using NodeIndex = typename TypeParam::NodeIndex;
  using ArcIndex = typename TypeParam::ArcIndex;
  const auto graph = typename TypeParam::Builder().Build(nullptr);
  EXPECT_EQ(NodeIndex(0), graph->num_nodes());
  EXPECT_EQ(NodeIndex(0), size(*graph));
  EXPECT_EQ(ArcIndex(0), graph->num_arcs());
}

TYPED_TEST(GenericGraphInterfaceTest, EmptyGraphAlternateSyntax) {
  using NodeIndex = typename TypeParam::NodeIndex;
  using ArcIndex = typename TypeParam::ArcIndex;
  const auto graph =
      typename TypeParam::Builder(NodeIndex(0), ArcIndex(0)).Build(nullptr);
  EXPECT_EQ(NodeIndex(0), graph->num_nodes());
  EXPECT_EQ(NodeIndex(0), size(*graph));
  EXPECT_EQ(ArcIndex(0), graph->num_arcs());
}

TYPED_TEST(GenericGraphInterfaceTest, GraphWithNodesButNoArc) {
  using NodeIndex = typename TypeParam::NodeIndex;
  using ArcIndex = typename TypeParam::ArcIndex;
  const NodeIndex kNodes(1000);
  const auto graph =
      typename TypeParam::Builder(kNodes, ArcIndex(0)).Build(nullptr);
  EXPECT_EQ(kNodes, graph->num_nodes());
  EXPECT_EQ(kNodes, size(*graph));
  EXPECT_EQ(ArcIndex(0), graph->num_arcs());
  int count = 0;
  for (const NodeIndex node : graph->AllNodes()) {
    for ([[maybe_unused]] const ArcIndex arc : graph->OutgoingArcs(node)) {
      ++count;
    }
  }
  EXPECT_EQ(0, count);
  for ([[maybe_unused]] const ArcIndex arc : graph->AllForwardArcs()) {
    ++count;
  }
  EXPECT_EQ(0, count);
}

TYPED_TEST(GenericGraphInterfaceTest, BuildWithRandomArc) {
  using NodeIndex = typename TypeParam::NodeIndex;
  using ArcIndex = typename TypeParam::ArcIndex;
  const int kNodes = 1000;
  const int kArcs = 5 * kNodes;
  std::vector<NodeIndex> heads(kArcs);
  std::vector<NodeIndex> tails(kArcs);

  std::mt19937 randomizer(42);
  for (int i = 0; i < kArcs; ++i) {
    heads[i] = NodeIndex(absl::Uniform(randomizer, 0, kNodes));
    tails[i] = NodeIndex(absl::Uniform(randomizer, 0, kNodes));
  }
  for (int i = 0; i < 4; ++i) {
    const bool reserve = i % 2;
    const bool test_permutation = i < 2;
    ConstructAndCheckGraph<TypeParam>(NodeIndex(kNodes), ArcIndex(kArcs), heads,
                                      tails, reserve, test_permutation);
  }
}

// This exercise the arc index permutation a bit differently, it also
// test for node with 0 outgoing arcs.
TYPED_TEST(GenericGraphInterfaceTest, BuildWithOrderedArc) {
  using NodeIndex = typename TypeParam::NodeIndex;
  using ArcIndex = typename TypeParam::ArcIndex;
  const int kNodes = 10000;
  const int kDegree = 2;
  const int kArcs = kDegree * kNodes;
  std::vector<NodeIndex> heads(kArcs);
  std::vector<NodeIndex> tails(kArcs);

  std::mt19937 randomizer(42);
  int index = 0;
  for (int i = 0; i < kNodes; ++i) {
    for (int j = 0; j < kDegree; ++j) {
      heads[index] = NodeIndex(absl::Uniform(randomizer, 0, kNodes));
      tails[index] = NodeIndex(i);
      index++;
    }
  }
  for (int i = 0; i < 4; ++i) {
    const bool reserve = i % 2;
    const bool test_permutation = i < 2;
    ConstructAndCheckGraph<TypeParam>(NodeIndex(kNodes), ArcIndex(kArcs), heads,
                                      tails, reserve, test_permutation);
  }
}

TYPED_TEST(GenericGraphInterfaceTest, PastTheEndIterators) {
  using NodeIndex = typename TypeParam::NodeIndex;
  typename TypeParam::Builder builder;
  builder.AddArc(NodeIndex(0), NodeIndex(1));
  builder.AddArc(NodeIndex(0), NodeIndex(2));
  builder.AddArc(NodeIndex(0), NodeIndex(3));
  builder.AddArc(NodeIndex(3), NodeIndex(4));
  builder.AddArc(NodeIndex(1), NodeIndex(4));
  const auto graph = std::move(builder).Build(nullptr);
  for (NodeIndex i(0); i < NodeIndex(4); ++i) {
    EXPECT_EQ(graph->OutgoingArcsStartingFrom(i, TypeParam::kNilArc).end(),
              graph->OutgoingArcs(i).end());
    if constexpr (TypeParam::kHasNegativeReverseArcs) {
      EXPECT_EQ(graph->IncomingArcsStartingFrom(i, TypeParam::kNilArc).end(),
                graph->IncomingArcs(i).end());
      EXPECT_EQ(
          graph->OppositeIncomingArcsStartingFrom(i, TypeParam::kNilArc).end(),
          graph->OppositeIncomingArcs(i).end());
      EXPECT_EQ(graph
                    ->OutgoingOrOppositeIncomingArcsStartingFrom(
                        i, TypeParam::kNilArc)
                    .end(),
                typename TypeParam::OutgoingOrOppositeIncomingArcIterator(
                    *graph, i, TypeParam::kNilArc));
    }
  }
}

TEST(StaticGraphTest, FromArcs) {
  const std::vector<std::pair<StrongNodeId, StrongNodeId>> arcs = {
      {StrongNodeId(1), StrongNodeId(2)}, {StrongNodeId(1), StrongNodeId(0)}};
  const auto graph = GraphFromArcs<StaticGraph<StrongNodeId, StrongArcId>>(
      StrongNodeId(3), arcs);
  EXPECT_EQ(StrongNodeId(3), graph.num_nodes());
  EXPECT_EQ(StrongNodeId(3), size(graph));
  EXPECT_EQ(StrongArcId(2), graph.num_arcs());
  std::vector<std::pair<StrongNodeId, StrongNodeId>> read_arcs;
  for (const auto arc : graph.AllForwardArcs()) {
    read_arcs.push_back({graph.Tail(arc), graph.Head(arc)});
  }
  EXPECT_THAT(read_arcs,
              UnorderedElementsAre(Pair(StrongNodeId(1), StrongNodeId(2)),
                                   Pair(StrongNodeId(1), StrongNodeId(0))));
}

TEST(CompleteGraphTest, EmptyGraph) {
  CompleteGraph<> graph(0);
  EXPECT_EQ(0, graph.num_nodes());
  EXPECT_EQ(0, size(graph));
  EXPECT_EQ(0, graph.num_arcs());
  for (const auto arc : graph.AllForwardArcs()) {
    EXPECT_TRUE(false);
    EXPECT_TRUE(graph.IsArcValid(arc));
  }
}

TEST(CompleteGraphTest, OneNodeGraph) {
  CompleteGraph<> graph(1);
  EXPECT_EQ(1, graph.num_nodes());
  EXPECT_EQ(1, size(graph));
  EXPECT_EQ(1, graph.num_arcs());
  EXPECT_EQ(graph.Head(0), 0);
  EXPECT_EQ(graph.Tail(0), 0);
}

TEST(CompleteGraphTest, NonEmptyGraph) {
  static const int kNumNodes = 5;
  CompleteGraph<> graph(kNumNodes);
  EXPECT_EQ(kNumNodes, graph.num_nodes());
  EXPECT_EQ(kNumNodes, size(graph));
  EXPECT_EQ(kNumNodes * kNumNodes, graph.num_arcs());
  int count = 0;
  for (const auto arc : graph.AllForwardArcs()) {
    ++count;
    EXPECT_TRUE(graph.IsArcValid(arc));
  }
  EXPECT_EQ(kNumNodes * kNumNodes, count);
  for (const auto node : graph.AllNodes()) {
    EXPECT_EQ(kNumNodes, graph.OutDegree(node));
    EXPECT_TRUE(graph.IsNodeValid(node));
    count = 0;
    for (const auto arc : graph.OutgoingArcs(node)) {
      EXPECT_EQ(node, graph.Tail(arc));
      ++count;
      EXPECT_EQ(*(graph.OutgoingArcsStartingFrom(node, arc).begin()), arc);
    }
    EXPECT_EQ(kNumNodes, count);
    count = 0;
    for (const auto head : graph[node]) {
      ++count;
      EXPECT_TRUE(graph.IsNodeValid(head));
    }
    EXPECT_EQ(kNumNodes, count);
  }
}

TEST(CompleteGraphTest, Typed) {
  CompleteGraph<StrongNodeId, StrongArcId> graph(StrongNodeId(4));
  EXPECT_EQ(graph.num_nodes(), StrongNodeId(4));
  EXPECT_EQ(size(graph), StrongNodeId(4));
  EXPECT_EQ(graph.num_arcs(), StrongArcId(16));
  for (const auto node : graph.AllNodes()) {
    EXPECT_EQ(graph.OutDegree(node), StrongArcId(4));
    EXPECT_EQ(std::size(graph.OutgoingArcs(node)), 4);
    EXPECT_THAT(graph[node], ElementsAre(StrongNodeId(0), StrongNodeId(1),
                                         StrongNodeId(2), StrongNodeId(3)));
  }
}

TEST(CompleteBipartiteGraphTest, EmptyGraph) {
  CompleteBipartiteGraph<> graph(0, 0);
  EXPECT_EQ(0, graph.num_nodes());
  EXPECT_EQ(0, size(graph));
  EXPECT_EQ(0, graph.num_arcs());
  EXPECT_TRUE(graph.AllForwardArcs().empty());
}

TEST(CompleteBipartiteGraphTest, OneRightNodeGraph) {
  CompleteBipartiteGraph<> graph(3, 1);
  EXPECT_EQ(4, graph.num_nodes());
  EXPECT_EQ(4, size(graph));
  EXPECT_EQ(3, graph.num_arcs());
  EXPECT_EQ(graph.Head(0), 3);
  EXPECT_EQ(graph.Head(1), 3);
  EXPECT_EQ(graph.Head(2), 3);
  EXPECT_EQ(graph.Tail(0), 0);
  EXPECT_EQ(graph.Tail(1), 1);
  EXPECT_EQ(graph.Tail(2), 2);
}

TEST(CompleteBipartiteGraphTest, NonEmptyGraph) {
  static const int kNumRightNodes = 5;
  static const int kNumLeftNodes = 3;
  CompleteBipartiteGraph<> graph(kNumLeftNodes, kNumRightNodes);
  EXPECT_EQ(kNumLeftNodes + kNumRightNodes, graph.num_nodes());
  EXPECT_EQ(graph.num_nodes(), size(graph));
  EXPECT_EQ(kNumLeftNodes * kNumRightNodes, graph.num_arcs());
  int count = 0;
  for (const auto arc : graph.AllForwardArcs()) {
    ++count;
    EXPECT_TRUE(graph.IsArcValid(arc));
  }
  EXPECT_EQ(kNumLeftNodes * kNumRightNodes, count);
  for (const auto node : graph.AllNodes()) {
    EXPECT_EQ(node < kNumLeftNodes ? kNumRightNodes : 0, graph.OutDegree(node));
    EXPECT_TRUE(graph.IsNodeValid(node));
    count = 0;
    for (const auto arc : graph.OutgoingArcs(node)) {
      EXPECT_EQ(node, graph.Tail(arc));
      EXPECT_EQ(kNumLeftNodes + count, graph.Head(arc));
      ++count;
      EXPECT_EQ(*(graph.OutgoingArcsStartingFrom(node, arc).begin()), arc);
    }
    EXPECT_EQ(node < kNumLeftNodes ? kNumRightNodes : 0, count);
    count = 0;
    for (const auto head : graph[node]) {
      EXPECT_EQ(head, kNumLeftNodes + count);
      EXPECT_TRUE(graph.IsNodeValid(head));
      ++count;
    }
    EXPECT_EQ(node < kNumLeftNodes ? kNumRightNodes : 0, count);
  }
  for (const auto arc : graph.AllForwardArcs()) {
    EXPECT_EQ(graph.GetArc(graph.Tail(arc), graph.Head(arc)), arc);
  }
}

TEST(CompleteBipartiteGraphTest, Overflow) {
  using NodeIndex = uint32_t;
  using ArcIndex = uint64_t;
  using Graph = CompleteBipartiteGraph<NodeIndex, ArcIndex>;
  constexpr NodeIndex kNumNodes = std::numeric_limits<NodeIndex>::max() / 2;
  Graph graph(kNumNodes, kNumNodes);
  EXPECT_EQ(2 * kNumNodes, graph.num_nodes());
  EXPECT_EQ(graph.num_nodes(), size(graph));
  EXPECT_EQ(uint64_t{kNumNodes} * uint64_t{kNumNodes}, graph.num_arcs());
  constexpr uint64_t kLeft = kNumNodes - 3;
  constexpr uint64_t kRight = kNumNodes + kNumNodes - 2;

  EXPECT_EQ(graph.GetArc(kLeft, kRight),
            kLeft * kNumNodes + (kRight - kNumNodes));
}

TEST(CompleteBipartiteGraphTest, Typed) {
  CompleteBipartiteGraph<StrongNodeId, StrongArcId> graph(StrongNodeId(2),
                                                          StrongNodeId(3));
  EXPECT_EQ(graph.num_nodes(), StrongNodeId(5));
  EXPECT_EQ(size(graph), StrongNodeId(5));
  EXPECT_EQ(graph.num_arcs(), StrongArcId(6));
  for (StrongNodeId node(0); node < StrongNodeId(2); ++node) {
    EXPECT_EQ(graph.OutDegree(node), StrongArcId(3));
    EXPECT_EQ(std::size(graph.OutgoingArcs(node)), 3);
    EXPECT_THAT(graph[node],
                ElementsAre(StrongNodeId(2), StrongNodeId(3), StrongNodeId(4)));
  }
  for (StrongNodeId node(2); node < StrongNodeId(5); ++node) {
    EXPECT_EQ(graph.OutDegree(node), StrongArcId(0));
    EXPECT_EQ(std::size(graph.OutgoingArcs(node)), 0);
    EXPECT_EQ(std::size(graph[node]), 0);
  }
}

TEST(SVector, NoHeapCheckerFalsePositive) {
  static const internal::SVector<int, int32_t>* const kVector = []() {
    auto* vector = new internal::SVector<int, int32_t>();
    vector->resize(5000);
    return vector;
  }();
  EXPECT_EQ(kVector->size(), 5000);
}

TEST(Permute, IntArray) {
  int array[] = {4, 5, 6};
  std::vector<int> permutation = {0, 2, 1};
  util::Permute(permutation, &array);
  EXPECT_THAT(array, ElementsAre(4, 6, 5));
}

TEST(Permute, BoolVector) {
  std::vector<bool> array = {true, false, true};
  std::vector<int> permutation = {0, 2, 1};
  util::Permute(permutation, &array);
  EXPECT_THAT(array, ElementsAre(true, true, false));
}

TEST(Permute, StrongVector) {
  util_intops::StrongVector<StrongArcId, int> array = {4, 5, 6};
  std::vector<StrongArcId> permutation = {StrongArcId(0), StrongArcId(2),
                                          StrongArcId(1)};
  util::Permute(permutation, &array);
  EXPECT_THAT(array, ElementsAre(4, 6, 5));
}

class TestGraphBuilder
    : public internal::GraphBuilderBase<TestGraphBuilder, ListGraph<int, int>,
                                        int, int> {
 public:
  static constexpr bool kBuildPermutesArcs = true;

  explicit TestGraphBuilder(std::vector<int> permutation)
      : permutation_(std::move(permutation)) {}

  std::unique_ptr<ListGraph<int, int>> Build(std::vector<int>* permutation) && {
    *permutation = std::move(permutation_);
    return nullptr;
  }

 private:
  std::vector<int> permutation_;
};

TEST(BuildAndPermuteTest, Works) {
  TestGraphBuilder builder({0, 2, 1});
  int array[] = {4, 5, 6};
  std::vector<bool> bool_vector = {true, false, true};
  util_intops::StrongVector<StrongArcId, int> strong_vector = {4, 5, 6};
  std::unique_ptr<int> unique_ptr_array[] = {std::make_unique<int>(4),
                                             std::make_unique<int>(5),
                                             std::make_unique<int>(6)};
  auto graph = std::move(builder).BuildAndPermute(
      array, bool_vector, strong_vector, unique_ptr_array);
  EXPECT_THAT(array, ElementsAre(4, 6, 5));
  EXPECT_THAT(bool_vector, ElementsAre(true, true, false));
  EXPECT_THAT(strong_vector, ElementsAre(4, 6, 5));
  EXPECT_THAT(unique_ptr_array,
              ElementsAre(Pointee(4), Pointee(6), Pointee(5)));
}

}  // namespace util
