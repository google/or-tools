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
#include "benchmark/benchmark.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/strong_int.h"
#include "ortools/base/strong_vector.h"

namespace util {

using testing::ElementsAre;
using testing::Pair;
using testing::UnorderedElementsAre;

DEFINE_STRONG_INT_TYPE(StrongNodeId, int32_t);
DEFINE_STRONG_INT_TYPE(StrongArcId, int32_t);

// Iterators.
#if __cplusplus >= 202002L
static_assert(std::forward_iterator<ListGraph<>::OutgoingArcIterator>);
static_assert(std::forward_iterator<ListGraph<>::OutgoingHeadIterator>);
static_assert(
    std::forward_iterator<ReverseArcListGraph<>::OutgoingArcIterator>);
static_assert(
    std::forward_iterator<ReverseArcListGraph<>::IncomingArcIterator>);
static_assert(std::input_iterator<
              ReverseArcListGraph<>::OutgoingOrOppositeIncomingArcIterator>);
static_assert(
    std::forward_iterator<ReverseArcListGraph<>::OutgoingHeadIterator>);
#endif  // __cplusplus >= 202002L

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
  std::unique_ptr<GraphType> graph;
  if (reserve) {
    graph.reset(new GraphType(num_nodes, num_arcs));
  } else {
    graph.reset(new GraphType());
  }
  std::vector<std::vector<NodeIndex>> verifier(static_cast<size_t>(num_nodes));

  for (ArcIndex i(0); i < num_arcs; ++i) {
    NodeIndex head = heads[static_cast<size_t>(i)];
    NodeIndex tail = tails[static_cast<size_t>(i)];
    EXPECT_EQ(i, graph->AddArc(tail, head));
    verifier[static_cast<size_t>(tail)].push_back(head);
  }
  std::vector<typename GraphType::ArcIndex> permutation;
  if (test_permutation) {
    graph->Build(&permutation);
  } else {
    graph->Build();
  }

  EXPECT_EQ(num_nodes, graph->num_nodes());
  EXPECT_EQ(num_nodes, graph->size());
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
  // Note(user): as of 2012-09, the rule seems to be: round x up to the
  // next multiple of 16.
  // WARNING: This may change, and may already be wrong for small values.
  return IndexType((16 * (static_cast<int64_t>(x) + 15)) / 16);
}

template <typename IndexType>
class SVectorTest : public ::testing::Test {};

typedef ::testing::Types<std::pair<int, int>, std::pair<int, StrongArcId>,
                         std::pair<StrongArcId, int>,
                         std::pair<StrongArcId, StrongArcId>>
    TestSVectorIndexTypes;

TYPED_TEST_SUITE(SVectorTest, TestSVectorIndexTypes);

TYPED_TEST(SVectorTest, CopyMoveIterate) {
  using IndexT = typename TypeParam::first_type;
  using ValueT = typename TypeParam::second_type;
  using VectorT = internal::SVector<IndexT, ValueT>;
  VectorT v;
  v.resize(IndexT(2));
  v[IndexT(0)] = ValueT(1);
  v[IndexT(1)] = ValueT(2);

  {
    EXPECT_THAT(VectorT(v), ElementsAre(ValueT(1), ValueT(2)));
    VectorT v2 = v;
    EXPECT_THAT(v2, ElementsAre(ValueT(1), ValueT(2)));
    EXPECT_THAT(v, ElementsAre(ValueT(1), ValueT(2)));
  }

  {
    VectorT v2 = std::move(v);
    EXPECT_THAT(v2, ElementsAre(ValueT(1), ValueT(2)));
    EXPECT_THAT(VectorT(std::move(v2)), ElementsAre(ValueT(1), ValueT(2)));
  }
}

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
  EXPECT_LE(v.capacity(), UpperBoundOfMallocBlockSizeOf(IndexT(100)));
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
  EXPECT_LE(v.capacity(), UpperBoundOfMallocBlockSizeOf(IndexT(100)));
  for (ValueT i(0); i < ValueT(100); i++) {
    v.grow(-i, i);
  }
  EXPECT_EQ(IndexT(100), v.size());
  EXPECT_GE(v.capacity(), IndexT(100));
  EXPECT_LE(v.capacity(), UpperBoundOfMallocBlockSizeOf(IndexT(100)));
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
  EXPECT_LE(v.capacity(), UpperBoundOfMallocBlockSizeOf(IndexT(100)));
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

TYPED_TEST(SVectorTest, Swap) {
  using IndexT = typename TypeParam::first_type;
  using ValueT = typename TypeParam::second_type;
  internal::SVector<IndexT, ValueT> s;
  internal::SVector<IndexT, ValueT> t;
  s.resize(IndexT(1));
  s[IndexT(0)] = ValueT('s');
  s[IndexT(-1)] = ValueT('s');
  t.resize(IndexT(2));
  for (IndexT i(-2); i <= IndexT(1); ++i) {
    t[i] = ValueT('t');
  }
  s.swap(t);
  EXPECT_EQ(IndexT(1), t.size());
  EXPECT_EQ(ValueT('s'), t[IndexT(-1)]);
  EXPECT_EQ(ValueT('s'), t[IndexT(0)]);
  EXPECT_EQ(IndexT(2), s.size());
  EXPECT_EQ(ValueT('t'), s[IndexT(-2)]);
  EXPECT_EQ(ValueT('t'), s[IndexT(-1)]);
  EXPECT_EQ(ValueT('t'), s[IndexT(0)]);
  EXPECT_EQ(ValueT('t'), s[IndexT(1)]);
}

TYPED_TEST(SVectorTest, SwapAndDestroy) {
  using IndexT = typename TypeParam::first_type;
  using ValueT = typename TypeParam::second_type;
  internal::SVector<IndexT, ValueT> s;
  {
    internal::SVector<IndexT, ValueT> t;
    t.resize(IndexT(2));
    t[IndexT(-2)] = ValueT(42);
    t.swap(s);
  }
  EXPECT_EQ(IndexT(2), s.size());
  EXPECT_EQ(ValueT(42), s[IndexT(-2)]);
  EXPECT_EQ(ValueT(0), s[IndexT(1)]);
}

// Use a more complex type to better check the invocations of
// constructors/destructors.
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
    EXPECT_EQ("Left", s[-i - 1]);
    EXPECT_EQ("Right", s[i]);
  }
  for (int i = 10; i < 10 + 50; ++i) {
    EXPECT_EQ("NewNewLeft", s[-i - 1]);
    EXPECT_EQ("NewNewRight", s[i]);
  }
}

// An object that supports moves but not copies.  It also has non-trivial
// default constructor, and a non-trivial destructor, and makes various internal
// consistency checks that help flush out bugs (double destruction, failure to
// destruct, etc.).
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
    // Every MoveOnlyObject, even after it has been moved from, has a valid
    // non-null id_.
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
  // Suppress the bugprone-use-after-move clang-tidy warning on `a`
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

TEST(SVectorMoveOnlyTest, GrowMoveOnlyObject) {
  EXPECT_EQ(0, MoveOnlyObject::GetObjectCount());
  {
    internal::SVector<int, MoveOnlyObject> a;
    a.resize(10);
    EXPECT_EQ(a.size() * 2, MoveOnlyObject::GetObjectCount());

    // Grow to the point where the vector reallocates.
    MoveOnlyObject* const original_data = a.data();
    while (original_data == a.data()) {
      a.resize(a.size() + 1);
      EXPECT_EQ(a.size() * 2, MoveOnlyObject::GetObjectCount());
    }
  }
  EXPECT_EQ(0, MoveOnlyObject::GetObjectCount());
}

TEST(SVectorMoveOnlyTest, ReserveMoveOnlyObject) {
  EXPECT_EQ(0, MoveOnlyObject::GetObjectCount());
  {
    internal::SVector<int, MoveOnlyObject> a;
    a.resize(10);
    EXPECT_EQ(a.size() * 2, MoveOnlyObject::GetObjectCount());

    // Reserve to the point where the vector reallocates.
    MoveOnlyObject* const original_data = a.data();
    while (original_data == a.data()) {
      a.reserve(a.size() * 2);
      EXPECT_EQ(a.size() * 2, MoveOnlyObject::GetObjectCount());
    }
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
  // We don't expect any moves of the individual elements, because the
  // containers just swap their memory buffers.
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
    // Ditto: the containers swap themselves. But we do trigger the destruction
    // of the underlying elements of the destination vector immediately.
    ASSERT_EQ(TrackedObject::Counters(),
              "constructions: 16, destructions: 6, moves: 0, copies: 0");
  }
  ASSERT_EQ(a.size(), 5);
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
  TypeParam graph;
  graph.Build();
  EXPECT_EQ(NodeIndex(0), graph.num_nodes());
  EXPECT_EQ(NodeIndex(0), graph.size());
  EXPECT_EQ(ArcIndex(0), graph.num_arcs());
}

TYPED_TEST(GenericGraphInterfaceTest, EmptyGraphAlternateSyntax) {
  using NodeIndex = typename TypeParam::NodeIndex;
  using ArcIndex = typename TypeParam::ArcIndex;
  TypeParam graph(NodeIndex(0), ArcIndex(0));
  graph.Build();
  EXPECT_EQ(NodeIndex(0), graph.num_nodes());
  EXPECT_EQ(NodeIndex(0), graph.size());
  EXPECT_EQ(ArcIndex(0), graph.num_arcs());
}

TYPED_TEST(GenericGraphInterfaceTest, GraphWithNodesButNoArc) {
  using NodeIndex = typename TypeParam::NodeIndex;
  using ArcIndex = typename TypeParam::ArcIndex;
  const NodeIndex kNodes(1000);
  TypeParam graph(kNodes, ArcIndex(0));
  graph.Build();
  EXPECT_EQ(kNodes, graph.num_nodes());
  EXPECT_EQ(kNodes, graph.size());
  EXPECT_EQ(ArcIndex(0), graph.num_arcs());
  int count = 0;
  for (const NodeIndex node : graph.AllNodes()) {
    for ([[maybe_unused]] const ArcIndex arc : graph.OutgoingArcs(node)) {
      ++count;
    }
  }
  EXPECT_EQ(0, count);
  for ([[maybe_unused]] const ArcIndex arc : graph.AllForwardArcs()) {
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
  TypeParam graph;
  graph.AddArc(NodeIndex(0), NodeIndex(1));
  graph.AddArc(NodeIndex(0), NodeIndex(2));
  graph.AddArc(NodeIndex(0), NodeIndex(3));
  graph.AddArc(NodeIndex(3), NodeIndex(4));
  graph.AddArc(NodeIndex(1), NodeIndex(4));
  graph.Build();
  for (NodeIndex i(0); i < NodeIndex(4); ++i) {
    EXPECT_EQ(graph.OutgoingArcsStartingFrom(i, TypeParam::kNilArc).end(),
              graph.OutgoingArcs(i).end());
    if constexpr (TypeParam::kHasNegativeReverseArcs) {
      EXPECT_EQ(graph.IncomingArcsStartingFrom(i, TypeParam::kNilArc).end(),
                graph.IncomingArcs(i).end());
      EXPECT_EQ(
          graph.OppositeIncomingArcsStartingFrom(i, TypeParam::kNilArc).end(),
          graph.OppositeIncomingArcs(i).end());
      EXPECT_EQ(
          graph
              .OutgoingOrOppositeIncomingArcsStartingFrom(i, TypeParam::kNilArc)
              .end(),
          typename TypeParam::OutgoingOrOppositeIncomingArcIterator(
              graph, i, TypeParam::kNilArc));
    }
  }
}

TEST(StaticGraphTest, HeadAndTailBeforeAndAfterBuild) {
  for (const bool poll_in_the_middle_of_construction : {false, true}) {
    for (const bool build : {false, true}) {
      SCOPED_TRACE(absl::StrCat(
          "Polling in the middle of construction: ",
          poll_in_the_middle_of_construction,
          ", Calling Build() at the end of the construction: ", build));
      StaticGraph<> graph;
      graph.AddArc(1, 3);
      graph.AddArc(2, 1);
      graph.AddArc(4, 6);
      if (poll_in_the_middle_of_construction) {
        ASSERT_EQ(1, graph.Tail(0));
        ASSERT_EQ(3, graph.Head(0));
        ASSERT_EQ(2, graph.Tail(1));
        ASSERT_EQ(1, graph.Head(1));
        ASSERT_EQ(4, graph.Tail(2));
        ASSERT_EQ(6, graph.Head(2));
        ASSERT_EQ(3, graph.num_arcs());
      }
      graph.AddArc(2, 1);
      graph.AddArc(0, 0);
      graph.AddArc(7, 7);
      if (build) {
        graph.Build();
        std::vector<std::string> arcs;
        for (int i = 0; i < graph.num_arcs(); ++i) {
          arcs.push_back(absl::StrCat(graph.Tail(i), "->", graph.Head(i)));
        }
        EXPECT_THAT(arcs, UnorderedElementsAre("1->3", "2->1", "4->6", "2->1",
                                               "0->0", "7->7"));
      } else {
        ASSERT_EQ(1, graph.Tail(0));
        ASSERT_EQ(3, graph.Head(0));
        ASSERT_EQ(2, graph.Tail(1));
        ASSERT_EQ(1, graph.Head(1));
        ASSERT_EQ(4, graph.Tail(2));
        ASSERT_EQ(6, graph.Head(2));
        ASSERT_EQ(2, graph.Tail(3));
        ASSERT_EQ(1, graph.Head(3));
        ASSERT_EQ(0, graph.Tail(4));
        ASSERT_EQ(0, graph.Head(4));
        ASSERT_EQ(7, graph.Tail(5));
        ASSERT_EQ(7, graph.Head(5));
        ASSERT_EQ(6, graph.num_arcs());
      }
    }
  }
}

TEST(StaticGraphTest, FromArcs) {
  const std::vector<std::pair<int, int>> arcs = {{1, 2}, {1, 0}};
  StaticGraph<> graph = StaticGraph<>::FromArcs(3, arcs);
  EXPECT_EQ(3, graph.num_nodes());
  EXPECT_EQ(3, graph.size());
  EXPECT_EQ(2, graph.num_arcs());
  std::vector<std::pair<int, int>> read_arcs;
  for (const auto arc : graph.AllForwardArcs()) {
    read_arcs.push_back({graph.Tail(arc), graph.Head(arc)});
  }
  EXPECT_THAT(read_arcs, UnorderedElementsAre(Pair(1, 2), Pair(1, 0)));
}

TEST(CompleteGraphTest, EmptyGraph) {
  CompleteGraph<> graph(0);
  EXPECT_EQ(0, graph.num_nodes());
  EXPECT_EQ(0, graph.size());
  EXPECT_EQ(0, graph.num_arcs());
  for (const auto arc : graph.AllForwardArcs()) {
    EXPECT_TRUE(false);
    EXPECT_TRUE(graph.IsArcValid(arc));
  }
}

TEST(CompleteGraphTest, OneNodeGraph) {
  CompleteGraph<> graph(1);
  EXPECT_EQ(1, graph.num_nodes());
  EXPECT_EQ(1, graph.size());
  EXPECT_EQ(1, graph.num_arcs());
  EXPECT_EQ(graph.Head(0), 0);
  EXPECT_EQ(graph.Tail(0), 0);
}

TEST(CompleteGraphTest, NonEmptyGraph) {
  static const int kNumNodes = 5;
  CompleteGraph<> graph(kNumNodes);
  EXPECT_EQ(kNumNodes, graph.num_nodes());
  EXPECT_EQ(kNumNodes, graph.size());
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

TEST(CompleteBipartiteGraphTest, EmptyGraph) {
  CompleteBipartiteGraph<> graph(0, 0);
  EXPECT_EQ(0, graph.num_nodes());
  EXPECT_EQ(0, graph.size());
  EXPECT_EQ(0, graph.num_arcs());
  EXPECT_TRUE(graph.AllForwardArcs().empty());
}

TEST(CompleteBipartiteGraphTest, OneRightNodeGraph) {
  CompleteBipartiteGraph<> graph(3, 1);
  EXPECT_EQ(4, graph.num_nodes());
  EXPECT_EQ(4, graph.size());
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
  EXPECT_EQ(graph.num_nodes(), graph.size());
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
  EXPECT_EQ(graph.num_nodes(), graph.size());
  EXPECT_EQ(kNumNodes * kNumNodes, graph.num_arcs());
  constexpr uint64_t kLeft = kNumNodes - 3;
  constexpr uint64_t kRight = kNumNodes + kNumNodes - 2;

  EXPECT_EQ(graph.GetArc(kLeft, kRight),
            kLeft * kNumNodes + (kRight - kNumNodes));
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

template <typename GraphType, bool reserve>
static void BM_RandomArcs(benchmark::State& state) {
  const int kRandomSeed = 0;
  const int kNodes = 10 * 1000 * 1000;
  const int kArcs = 5 * kNodes;
  int items_processed = 0;
  for (auto _ : state) {
    GraphType graph;
    if (reserve) {
      graph.Reserve(kNodes, kArcs);
    }
    std::mt19937 randomizer(kRandomSeed);
    for (int i = 0; i < kArcs; ++i) {
      graph.AddArc(absl::Uniform<int32_t>(randomizer, 0, kNodes),
                   absl::Uniform<int32_t>(randomizer, 0, kNodes));
    }
    graph.Build();
    items_processed += kArcs;
  }
  state.SetItemsProcessed(items_processed);
}

template <typename GraphType, bool reserve>
static void BM_OrderedArcs(benchmark::State& state) {
  const int kRandomSeed = 0;
  const int kNodes = 10 * 1000 * 1000;
  const int kDegree = 5;
  const int kArcs = kDegree * kNodes;
  int items_processed = 0;
  for (auto _ : state) {
    GraphType graph;
    if (reserve) {
      graph.Reserve(kNodes, kArcs);
    }
    std::mt19937 randomizer(kRandomSeed);
    for (int i = 0; i < kNodes; ++i) {
      for (int j = 0; j < kDegree; ++j) {
        graph.AddArc(i, absl::Uniform<int32_t>(randomizer, 0, kNodes));
      }
    }
    graph.Build();
    items_processed += kArcs;
  }
  state.SetItemsProcessed(items_processed);
}

// This is just here to get some timing on the AddArc() function to see how the
// GraphType building time is split between the AddArc() calls and the actual
// Build() call. It is not usefull for all type of graph.
template <typename GraphType, bool reserve>
static void BM_RandomArcsBeforeBuild(benchmark::State& state) {
  const int kRandomSeed = 0;
  const int kNodes = 10 * 1000 * 1000;
  const int kArcs = 5 * kNodes;
  int items_processed = 0;
  for (auto _ : state) {
    GraphType graph;
    if (reserve) {
      graph.Reserve(kNodes, kArcs);
    }
    std::mt19937 randomizer(kRandomSeed);
    for (int i = 0; i < kArcs; ++i) {
      graph.AddArc(absl::Uniform<int32_t>(randomizer, 0, kNodes),
                   absl::Uniform<int32_t>(randomizer, 0, kNodes));
    }
    items_processed += kArcs;
  }
  state.SetItemsProcessed(items_processed);
}

// A basic vector<vector<>> graph implementation that many people uses. It is
// quite slower and use more memory than a static graph, except maybe during
// construction.
class VectorVectorGraph {
 public:
  VectorVectorGraph() = default;
  void Reserve(int32_t num_nodes, int32_t num_arcs) {
    // We could only reserve the space, but AddArc() need to be smarter then.
    graph_.resize(num_nodes);
  }
  void Build() {}
  void AddArc(int32_t tail, int32_t head) { graph_[tail].push_back(head); }

 private:
  std::vector<std::vector<int32_t>> graph_;
};

#define RESERVE true
#define NO_RESERVE false
BENCHMARK_TEMPLATE2(BM_RandomArcsBeforeBuild, StaticGraph<>, RESERVE);
BENCHMARK_TEMPLATE2(BM_RandomArcsBeforeBuild, StaticGraph<>, NO_RESERVE);
BENCHMARK_TEMPLATE2(BM_RandomArcsBeforeBuild, ReverseArcStaticGraph<>, RESERVE);
BENCHMARK_TEMPLATE2(BM_RandomArcsBeforeBuild, ReverseArcStaticGraph<>,
                    NO_RESERVE);
BENCHMARK_TEMPLATE2(BM_OrderedArcs, ListGraph<>, RESERVE);
BENCHMARK_TEMPLATE2(BM_OrderedArcs, ListGraph<>, NO_RESERVE);
BENCHMARK_TEMPLATE2(BM_OrderedArcs, StaticGraph<>, RESERVE);
BENCHMARK_TEMPLATE2(BM_OrderedArcs, StaticGraph<>, NO_RESERVE);
BENCHMARK_TEMPLATE2(BM_OrderedArcs, VectorVectorGraph, RESERVE);
BENCHMARK_TEMPLATE2(BM_OrderedArcs, ReverseArcListGraph<>, RESERVE);
BENCHMARK_TEMPLATE2(BM_OrderedArcs, ReverseArcListGraph<>, NO_RESERVE);
BENCHMARK_TEMPLATE2(BM_OrderedArcs, ReverseArcStaticGraph<>, RESERVE);
BENCHMARK_TEMPLATE2(BM_OrderedArcs, ReverseArcStaticGraph<>, NO_RESERVE);
BENCHMARK_TEMPLATE2(BM_RandomArcs, ListGraph<>, RESERVE);
BENCHMARK_TEMPLATE2(BM_RandomArcs, ListGraph<>, NO_RESERVE);
BENCHMARK_TEMPLATE2(BM_RandomArcs, StaticGraph<>, RESERVE);
BENCHMARK_TEMPLATE2(BM_RandomArcs, StaticGraph<>, NO_RESERVE);
BENCHMARK_TEMPLATE2(BM_RandomArcs, VectorVectorGraph, RESERVE);
BENCHMARK_TEMPLATE2(BM_RandomArcs, ReverseArcListGraph<>, RESERVE);
BENCHMARK_TEMPLATE2(BM_RandomArcs, ReverseArcListGraph<>, NO_RESERVE);
BENCHMARK_TEMPLATE2(BM_RandomArcs, ReverseArcStaticGraph<>, RESERVE);
BENCHMARK_TEMPLATE2(BM_RandomArcs, ReverseArcStaticGraph<>, NO_RESERVE);
#undef RESERVE
#undef NO_RESERVE

template <typename GraphType>
void BuildGraphForIterationsBenchmarks(GraphType* graph) {
  const int kRandomSeed = 0;
  const int kNodes = 10 * 1000 * 1000;
  const int kArcs = 5 * kNodes;
  graph->Reserve(kNodes, kArcs);
  std::mt19937 randomizer(kRandomSeed);
  for (int i = 0; i < kArcs; ++i) {
    graph->AddArc(absl::Uniform<int32_t>(randomizer, 0, kNodes),
                  absl::Uniform<int32_t>(randomizer, 0, kNodes));
  }
  graph->Build();
}

template <typename GraphType>
static void BM_OutgoingIterations(benchmark::State& state) {
  GraphType graph;
  BuildGraphForIterationsBenchmarks(&graph);
  int64_t num_arcs = 0;
  int64_t some_work = 0;
  for (auto _ : state) {
    for (int node = 0; node < graph.num_nodes(); ++node) {
      for (const int arc : graph.OutgoingArcs(node)) {
        some_work += graph.Head(arc);
        ++num_arcs;
      }
    }
  }
  CHECK_GT(some_work, 0);
  state.SetItemsProcessed(num_arcs);
}

BENCHMARK_TEMPLATE(BM_OutgoingIterations, ListGraph<>);
BENCHMARK_TEMPLATE(BM_OutgoingIterations, StaticGraph<>);
BENCHMARK_TEMPLATE(BM_OutgoingIterations, ReverseArcListGraph<>);
BENCHMARK_TEMPLATE(BM_OutgoingIterations, ReverseArcStaticGraph<>);

template <typename GraphType>
static void BM_IncomingIterations(benchmark::State& state) {
  GraphType graph;
  BuildGraphForIterationsBenchmarks(&graph);
  int64_t num_arcs = 0;
  int64_t some_work = 0;
  for (auto _ : state) {
    for (int node = 0; node < graph.num_nodes(); ++node) {
      for (const int arc : graph.IncomingArcs(node)) {
        some_work += graph.Tail(arc);
        ++num_arcs;
      }
    }
  }
  CHECK_GT(some_work, 0);
  state.SetItemsProcessed(num_arcs);
}

BENCHMARK_TEMPLATE(BM_IncomingIterations, ReverseArcListGraph<>);
BENCHMARK_TEMPLATE(BM_IncomingIterations, ReverseArcStaticGraph<>);

template <typename GraphType>
static void BM_OppositeIncomingIterations(benchmark::State& state) {
  GraphType graph;
  BuildGraphForIterationsBenchmarks(&graph);
  int64_t num_arcs = 0;
  int64_t some_work = 0;
  for (auto _ : state) {
    for (int node = 0; node < graph.num_nodes(); ++node) {
      for (const int arc : graph.OppositeIncomingArcs(node)) {
        some_work += graph.Head(arc);
        ++num_arcs;
      }
    }
  }
  CHECK_GT(some_work, 0);
  state.SetItemsProcessed(num_arcs);
}

BENCHMARK_TEMPLATE(BM_OppositeIncomingIterations, ReverseArcListGraph<>);
BENCHMARK_TEMPLATE(BM_OppositeIncomingIterations, ReverseArcStaticGraph<>);

template <typename GraphType>
static void BM_OutgoingOrOppositeIncomingIterations(benchmark::State& state) {
  GraphType graph;
  BuildGraphForIterationsBenchmarks(&graph);
  int64_t num_arcs = 0;
  for (auto _ : state) {
    for (int node = 0; node < graph.num_nodes(); ++node) {
      for (const int arc : graph.OutgoingOrOppositeIncomingArcs(node)) {
        auto head = graph.Head(arc);
        benchmark::DoNotOptimize(head);
      }
    }
  }
  state.SetItemsProcessed(state.iterations() * num_arcs);
}

BENCHMARK_TEMPLATE(BM_OutgoingOrOppositeIncomingIterations,
                   ReverseArcListGraph<>);
BENCHMARK_TEMPLATE(BM_OutgoingOrOppositeIncomingIterations,
                   ReverseArcStaticGraph<>);

// It's bit sad, but having two loops to iterate over opposite incoming and
// outgoing arcs is much faster than using `OutgoingOrOppositeIncomingArcs`.
template <typename GraphType>
static void BM_OutgoingOrOppositeIncomingIterationsTwoLoops(
    benchmark::State& state) {
  GraphType graph;
  BuildGraphForIterationsBenchmarks(&graph);
  for (auto _ : state) {
    for (int node = 0; node < graph.num_nodes(); ++node) {
      const auto work = [&graph](int arc) {
        auto head = graph.Head(arc);
        benchmark::DoNotOptimize(head);
      };
      for (const int arc : graph.OppositeIncomingArcs(node)) {
        work(arc);
      }
      for (const int arc : graph.OutgoingArcs(node)) {
        work(arc);
      }
    }
  }
  state.SetItemsProcessed(state.iterations() * graph.num_arcs());
}
BENCHMARK_TEMPLATE(BM_OutgoingOrOppositeIncomingIterationsTwoLoops,
                   ReverseArcListGraph<>);
BENCHMARK_TEMPLATE(BM_OutgoingOrOppositeIncomingIterationsTwoLoops,
                   ReverseArcStaticGraph<>);

template <typename GraphType>
static void BM_IntegralTypeCopy(benchmark::State& state) {
  GraphType graph;
  BuildGraphForIterationsBenchmarks(&graph);

  for (auto s : state) {
    GraphType copied;
    benchmark::DoNotOptimize(copied = GraphType(graph));
  }
}

BENCHMARK_TEMPLATE(BM_IntegralTypeCopy, ListGraph<>);
BENCHMARK_TEMPLATE(BM_IntegralTypeCopy, StaticGraph<>);
BENCHMARK_TEMPLATE(BM_IntegralTypeCopy, ReverseArcListGraph<>);
BENCHMARK_TEMPLATE(BM_IntegralTypeCopy, ReverseArcStaticGraph<>);

template <typename GraphType>
void TailHeadBenchmark(benchmark::State& state, const GraphType& graph) {
  // Prevent constant folding. Weird construct due to b/284459966.
  auto* graph_ptr = &graph;
  benchmark::DoNotOptimize(graph_ptr);
  for (auto s : state) {
    const auto num_arcs = graph.num_arcs();
    for (int arc = 0; arc < num_arcs; ++arc) {
      auto head = graph.Head(arc);
      auto tail = graph.Tail(arc);
      benchmark::DoNotOptimize(head);
      benchmark::DoNotOptimize(tail);
    }
  }
}

template <typename IndexT>
static void BM_CompleteGraphTailHead(benchmark::State& state) {
  constexpr int kNumNodes = 100;
  CompleteGraph<IndexT, IndexT> graph(kNumNodes);
  TailHeadBenchmark(state, graph);
}
BENCHMARK_TEMPLATE(BM_CompleteGraphTailHead, int32_t);
BENCHMARK_TEMPLATE(BM_CompleteGraphTailHead, int16_t);

template <typename IndexT>
static void BM_CompleteBipartiteGraphTailHead(benchmark::State& state) {
  constexpr int kNumLeft = 100;
  CompleteBipartiteGraph<IndexT, IndexT> graph(kNumLeft, kNumLeft);
  TailHeadBenchmark(state, graph);
}
BENCHMARK_TEMPLATE(BM_CompleteBipartiteGraphTailHead, int32_t);
BENCHMARK_TEMPLATE(BM_CompleteBipartiteGraphTailHead, int16_t);

template <typename ArrayT, typename IndexT>
void BM_Permute(benchmark::State& state) {
  const int size = state.range(0);
  ArrayT array(size);

  std::vector<IndexT> permutation(size);
  absl::c_iota(permutation, IndexT(0));

  for (const auto s : state) {
    util::Permute(permutation, &array);
    benchmark::DoNotOptimize(array);
    benchmark::DoNotOptimize(permutation);
  }
}
BENCHMARK(BM_Permute<util_intops::StrongVector<StrongArcId, int>, StrongArcId>)
    ->Arg(128);
BENCHMARK(BM_Permute<std::vector<int>, int>)->Arg(128);
BENCHMARK(BM_Permute<std::vector<bool>, int>)->Arg(128);

}  // namespace util
