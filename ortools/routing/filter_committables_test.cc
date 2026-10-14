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

#include "ortools/routing/filter_committables.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/routing/filter_test_utils.h"

namespace operations_research::routing {

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::UnorderedElementsAre;

namespace {

TEST(CommittableValueTest, InitialState) {
  CommittableValue<int> value(42);
  EXPECT_EQ(value.Get(), 42);
  EXPECT_EQ(value.GetCommitted(), 42);
}

TEST(CommittableValueTest, Revert) {
  CommittableValue<int> value(42);
  value.Set(23);
  EXPECT_EQ(value.Get(), 23);
  EXPECT_EQ(value.GetCommitted(), 42);
  value.Revert();
  EXPECT_EQ(value.Get(), 42);
  EXPECT_EQ(value.GetCommitted(), 42);
}

TEST(CommittableValueTest, Commit) {
  CommittableValue<int> value(42);
  value.Set(23);
  EXPECT_EQ(value.Get(), 23);
  EXPECT_EQ(value.GetCommitted(), 42);
  value.Commit();
  EXPECT_EQ(value.Get(), 23);
  EXPECT_EQ(value.GetCommitted(), 23);
  value.Revert();
  EXPECT_EQ(value.Get(), 23);
  EXPECT_EQ(value.GetCommitted(), 23);
}

TEST(CommittableValueTest, SetAndCommit) {
  CommittableValue<int> value(42);
  value.SetAndCommit(23);
  EXPECT_EQ(value.Get(), 23);
  EXPECT_EQ(value.GetCommitted(), 23);
}

TEST(CommittableArrayTest, EmptyArray) {
  CommittableArray<int> vector(0, 42);
  EXPECT_EQ(vector.Size(), 0);
  vector.SetAllAndCommit(0);
  EXPECT_EQ(vector.ChangedIndices().size(), 0);
}

TEST(CommittableArrayTest, InitialState) {
  CommittableArray<int> values(3, 42);
  EXPECT_EQ(values.Size(), 3);
  EXPECT_EQ(values.ChangedIndices().size(), 0);
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(values.Get(i), 42);
    EXPECT_EQ(values.GetCommitted(i), 42);
    EXPECT_FALSE(values.HasChanged(i));
  }
}

TEST(CommittableArrayTest, SetGetGetCommitted) {
  CommittableArray<int> values(5, 42);
  EXPECT_EQ(values.Size(), 5);
  values.Set(0, 23);
  values.Set(3, 0);
  values.Set(3, 25);
  EXPECT_EQ(values.ChangedIndices().size(), 2);
  EXPECT_THAT(values.ChangedIndices(), ElementsAre(0, 3));
  EXPECT_EQ(values.Get(0), 23);
  EXPECT_EQ(values.Get(1), 42);
  EXPECT_EQ(values.Get(2), 42);
  EXPECT_EQ(values.Get(3), 25);
  EXPECT_EQ(values.Get(4), 42);
  EXPECT_EQ(values.GetCommitted(0), 42);
  EXPECT_EQ(values.GetCommitted(3), 42);
  EXPECT_TRUE(values.HasChanged(0));
  EXPECT_FALSE(values.HasChanged(1));
  EXPECT_FALSE(values.HasChanged(2));
  EXPECT_TRUE(values.HasChanged(3));
  EXPECT_FALSE(values.HasChanged(4));
}

TEST(CommittableArrayTest, Revert) {
  CommittableArray<int> values(3, 42);
  values.Set(0, 23);
  values.Set(1, 24);
  values.Set(2, 25);
  EXPECT_EQ(values.ChangedIndices().size(), 3);
  values.Revert();
  EXPECT_EQ(values.ChangedIndices().size(), 0);
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(values.Get(i), 42);
    EXPECT_FALSE(values.HasChanged(i));
  }
}

TEST(CommittableArrayTest, Commit) {
  CommittableArray<double> values(3, 42.0);
  values.Set(0, 23.0);
  values.Set(1, 24.0);
  values.Set(2, 25.0);
  EXPECT_EQ(values.ChangedIndices().size(), 3);
  values.Commit();
  EXPECT_EQ(values.ChangedIndices().size(), 0);
  EXPECT_EQ(values.Get(0), 23.0);
  EXPECT_EQ(values.Get(1), 24.0);
  EXPECT_EQ(values.Get(2), 25.0);
  for (int i = 0; i < 3; ++i) {
    EXPECT_FALSE(values.HasChanged(i));
  }
}

TEST(CommittableArrayTest, GetMutable) {
  CommittableArray<double> values(3, 42.0);
  double& v1 = values.GetMutable(1);
  EXPECT_TRUE(values.HasChanged(1));
  v1 = 24.0;
  EXPECT_EQ(values.Get(1), 24.0);
}

TEST(CommittableArrayTest, SetAllAndCommit) {
  struct Point {
    double x;
    double y;
  };
  CommittableArray<Point> points(7, {.x = 5.0, .y = 9.0});
  EXPECT_EQ(points.Size(), 7);
  EXPECT_EQ(points.ChangedIndices().size(), 0);
  for (size_t p = 0; p < 7; ++p) {
    EXPECT_EQ(points.Get(p).x, 5.0);
    EXPECT_EQ(points.Get(p).y, 9.0);
    EXPECT_EQ(points.GetCommitted(p).x, 5.0);
    EXPECT_EQ(points.GetCommitted(p).y, 9.0);
    EXPECT_FALSE(points.HasChanged(p));
  }
  points.Set(3, {.x = 42.0, .y = 0.42});
  points.SetAllAndCommit({.x = 1.0, .y = 2.0});
  EXPECT_EQ(points.ChangedIndices().size(), 0);
  for (int it = 0; it < 2; ++it) {
    for (size_t p = 0; p < 7; ++p) {
      EXPECT_EQ(points.Get(p).x, 1.0);
      EXPECT_EQ(points.Get(p).y, 2.0);
      EXPECT_EQ(points.GetCommitted(p).x, 1.0);
      EXPECT_EQ(points.GetCommitted(p).y, 2.0);
      EXPECT_FALSE(points.HasChanged(p));
    }
    points.Revert();
  }
}

TEST(DefragmentRangesTest, DefragmentSomeRanges) {
  std::vector<int> input = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
  CommittableArray<IndexRange> ranges(4, {.begin = 0, .end = 0});
  ranges.Set(0, {.begin = 7, .end = 12});
  ranges.Set(1, {.begin = 4, .end = 7});
  ranges.Set(2, {.begin = 4, .end = 4});  // Empty.
  ranges.Set(3, {.begin = 0, .end = 4});
  ranges.Commit();
  std::vector<int> temp;
  DefragmentRanges(input, ranges, temp);
  // Ranges changed, the new ranges are contiguous, in order of range index.
  EXPECT_THAT(input, ElementsAre(8, 9, 10, 11, 12,  // range 0
                                 5, 6, 7,           // range 1
                                 1, 2, 3, 4));      // range 3
  EXPECT_THAT(ranges.ChangedIndices(), ElementsAre(0, 1, 2, 3));
  EXPECT_EQ(ranges.Get(0), (IndexRange{.begin = 0, .end = 5}));
  EXPECT_EQ(ranges.Get(1), (IndexRange{.begin = 5, .end = 8}));
  EXPECT_EQ(ranges.Get(2), (IndexRange{.begin = 8, .end = 8}));
  EXPECT_EQ(ranges.Get(3), (IndexRange{.begin = 8, .end = 12}));
}

TEST(DimensionValuesTest, Empty) {
  const int num_paths = 3;
  const int num_nodes = 15;
  DimensionValues values(num_paths, num_nodes);
  for (int p = 0; p < num_paths; ++p) {
    EXPECT_TRUE(values.CommittedNodes(p).empty());
    EXPECT_TRUE(values.Nodes(p).empty());
    EXPECT_EQ(values.NumNodes(p), 0);
    // Value sequences.
    EXPECT_TRUE(values.CommittedTravels(p).empty());
    EXPECT_TRUE(values.Travels(p).empty());
    EXPECT_TRUE(values.Transits(p).empty());
    EXPECT_TRUE(values.TravelSums(p).empty());
    EXPECT_TRUE(values.Cumuls(p).empty());
    // Mutable value sequences.
    EXPECT_TRUE(values.MutableTravels(p).empty());
    EXPECT_TRUE(values.MutableTransits(p).empty());
    EXPECT_TRUE(values.MutableTravelSums(p).empty());
    EXPECT_TRUE(values.MutableCumuls(p).empty());
    // Span.
    EXPECT_EQ(values.Span(p), DimensionValues::Interval::AllIntegers());
    // Paths.
    EXPECT_FALSE(values.PathHasChanged(p));
  }
  // Paths.
  EXPECT_EQ(0, values.ChangedPaths().size());
}

TEST(DimensionValuesTest, MakePathFromNewNodes) {
  const int num_paths = 3;
  const int num_nodes = 15;
  DimensionValues values(num_paths, num_nodes);
  // Add some nodes.
  const std::vector<int> nodes = {3, 9, 1};
  for (const int node : nodes) values.PushNode(node);
  // The new nodes are not in any paths.
  for (int p = 0; p < num_paths; ++p) {
    EXPECT_EQ(0, values.CommittedNodes(p).size());
    EXPECT_EQ(0, values.Nodes(p).size());
  }
  // Make a path from newly pushed nodes, now they should appear.
  values.MakePathFromNewNodes(1);
  EXPECT_TRUE(values.Nodes(0).empty());
  EXPECT_THAT(values.Nodes(1), ElementsAre(3, 9, 1));
  EXPECT_TRUE(values.Nodes(2).empty());
  // New nodes do not appear in committed paths.
  EXPECT_TRUE(values.CommittedNodes(0).empty());
  EXPECT_TRUE(values.CommittedNodes(1).empty());
  EXPECT_TRUE(values.CommittedNodes(2).empty());
}

TEST(DimensionValuesTest, MakePathCommitRevert) {
  const int num_path = 3;
  const int num_nodes = 9;
  DimensionValues values(num_path, num_nodes);
  const std::vector<int> nodes0 = {4, 0, 6, 1};
  const std::vector<int> nodes1 = {3, 5, 2};
  for (const int node : nodes0) values.PushNode(node);
  values.MakePathFromNewNodes(0);
  for (const int node : nodes1) values.PushNode(node);
  values.MakePathFromNewNodes(1);
  EXPECT_TRUE(values.CommittedNodes(0).empty());
  EXPECT_TRUE(values.CommittedNodes(1).empty());
  EXPECT_THAT(values.Nodes(0), ElementsAreArray(nodes0));
  EXPECT_THAT(values.Nodes(1), ElementsAreArray(nodes1));
  EXPECT_THAT(values.ChangedPaths(), UnorderedElementsAre(0, 1));
  values.Commit();
  EXPECT_THAT(values.CommittedNodes(0), ElementsAreArray(nodes0));
  EXPECT_THAT(values.CommittedNodes(1), ElementsAreArray(nodes1));
  EXPECT_THAT(values.Nodes(0), ElementsAreArray(nodes0));
  EXPECT_THAT(values.Nodes(1), ElementsAreArray(nodes1));
  // DimensionValues may compact its memory on Commit, this behavior is only
  // triggered after committing 16*num_nodes by default.
  // We iterate many times to test that the compaction is correct.
  for (int it = 0; it < 50; ++it) {
    // Exchange paths, check current and committed nodes.
    for (const int node : nodes1) values.PushNode(node);
    values.MakePathFromNewNodes(0);
    for (const int node : nodes0) values.PushNode(node);
    values.MakePathFromNewNodes(1);
    EXPECT_THAT(values.CommittedNodes(0), ElementsAreArray(nodes0));
    EXPECT_THAT(values.CommittedNodes(1), ElementsAreArray(nodes1));
    EXPECT_THAT(values.Nodes(0), ElementsAreArray(nodes1));
    EXPECT_THAT(values.Nodes(1), ElementsAreArray(nodes0));
    EXPECT_THAT(values.ChangedPaths(), UnorderedElementsAre(0, 1));
    // Revert, check that nodes came back to the initial state.
    values.Revert();
    EXPECT_THAT(values.CommittedNodes(0), ElementsAreArray(nodes0));
    EXPECT_THAT(values.CommittedNodes(1), ElementsAreArray(nodes1));
    EXPECT_THAT(values.Nodes(0), ElementsAreArray(nodes0));
    EXPECT_THAT(values.Nodes(1), ElementsAreArray(nodes1));
    EXPECT_TRUE(values.ChangedPaths().empty());
    // Exchange paths again, check nodes.
    for (const int node : nodes1) values.PushNode(node);
    values.MakePathFromNewNodes(0);
    for (const int node : nodes0) values.PushNode(node);
    values.MakePathFromNewNodes(1);
    EXPECT_THAT(values.CommittedNodes(0), ElementsAreArray(nodes0));
    EXPECT_THAT(values.CommittedNodes(1), ElementsAreArray(nodes1));
    EXPECT_THAT(values.Nodes(0), ElementsAreArray(nodes1));
    EXPECT_THAT(values.Nodes(1), ElementsAreArray(nodes0));
    EXPECT_THAT(values.ChangedPaths(), UnorderedElementsAre(0, 1));
    // Commit, check that paths have been exchanged.
    values.Commit();
    EXPECT_THAT(values.CommittedNodes(0), ElementsAreArray(nodes1));
    EXPECT_THAT(values.CommittedNodes(1), ElementsAreArray(nodes0));
    EXPECT_THAT(values.Nodes(0), ElementsAreArray(nodes1));
    EXPECT_THAT(values.Nodes(1), ElementsAreArray(nodes0));
    EXPECT_TRUE(values.ChangedPaths().empty());
    // Exchange paths, check nodes.
    for (const int node : nodes1) values.PushNode(node);
    values.MakePathFromNewNodes(1);
    for (const int node : nodes0) values.PushNode(node);
    values.MakePathFromNewNodes(0);
    EXPECT_THAT(values.CommittedNodes(0), ElementsAreArray(nodes1));
    EXPECT_THAT(values.CommittedNodes(1), ElementsAreArray(nodes0));
    EXPECT_THAT(values.Nodes(0), ElementsAreArray(nodes0));
    EXPECT_THAT(values.Nodes(1), ElementsAreArray(nodes1));
    EXPECT_THAT(values.ChangedPaths(), UnorderedElementsAre(0, 1));
    // Commit, check nodes.
    values.Commit();
    EXPECT_THAT(values.CommittedNodes(0), ElementsAreArray(nodes0));
    EXPECT_THAT(values.CommittedNodes(1), ElementsAreArray(nodes1));
    EXPECT_THAT(values.Nodes(0), ElementsAreArray(nodes0));
    EXPECT_THAT(values.Nodes(1), ElementsAreArray(nodes1));
    EXPECT_TRUE(values.ChangedPaths().empty());
  }
}

TEST(DimensionValuesTest, MutableSpan) {
  const int num_path = 3;
  const int num_nodes = 9;
  DimensionValues values(num_path, num_nodes);
  using Interval = DimensionValues::Interval;
  // Set some values.
  values.MakePathFromNewNodes(0);
  values.MakePathFromNewNodes(1);
  values.MakePathFromNewNodes(2);
  values.MutableSpan(0) = {.min = 10, .max = 100};
  values.MutableSpan(1) = {.min = 20, .max = 200};
  values.MutableSpan(2) = {.min = 30, .max = 300};
  EXPECT_EQ(values.Span(0), Interval(10, 100));
  EXPECT_EQ(values.Span(1), Interval(20, 200));
  EXPECT_EQ(values.Span(2), Interval(30, 300));
  // Commit values, they should stay the same.
  values.Commit();
  EXPECT_EQ(values.Span(0), Interval(10, 100));
  EXPECT_EQ(values.Span(1), Interval(20, 200));
  EXPECT_EQ(values.Span(2), Interval(30, 300));
  // Change span values, check getters.
  values.MakePathFromNewNodes(0);
  values.MakePathFromNewNodes(1);
  values.MakePathFromNewNodes(2);
  values.MutableSpan(0) = {.min = 1000, .max = 10000};
  values.MutableSpan(1) = {.min = 2000, .max = 20000};
  values.MutableSpan(2) = {.min = 3000, .max = 30000};
  EXPECT_EQ(values.Span(0), Interval(1000, 10000));
  EXPECT_EQ(values.Span(1), Interval(2000, 20000));
  EXPECT_EQ(values.Span(2), Interval(3000, 30000));
  // Revert, check that span got back to its committed values.
  values.Revert();
  EXPECT_EQ(values.Span(0), Interval(10, 100));
  EXPECT_EQ(values.Span(1), Interval(20, 200));
  EXPECT_EQ(values.Span(2), Interval(30, 300));
}

TEST(DimensionValuesTest, MutableVehicleBreak) {
  const int num_path = 3;
  const int num_nodes = 9;
  DimensionValues values(num_path, num_nodes);
  using VehicleBreak = DimensionValues::VehicleBreak;
  std::vector<VehicleBreak> breaks0 = {
      {.start = {.min = 10, .max = 20},
       .end = {.min = 30, .max = 40},
       .duration = {.min = 10, .max = 10},
       .is_performed = {.min = false, .max = true}},
      {.start = {.min = 50, .max = 60},
       .end = {.min = 70, .max = 80},
       .duration = {.min = 10, .max = 10},
       .is_performed = {.min = true, .max = true}}};
  std::vector<VehicleBreak> breaks1 = {};
  std::vector<VehicleBreak> breaks2 = {
      {.start = {.min = 1000, .max = 2000},
       .end = {.min = 3000, .max = 4000},
       .duration = {.min = 1000, .max = 1000},
       .is_performed = {.min = true, .max = true}}};
  // Set some values.
  values.MakePathFromNewNodes(0);
  values.MakePathFromNewNodes(1);
  values.MakePathFromNewNodes(2);
  values.MutableVehicleBreaks(0) = breaks0;
  values.MutableVehicleBreaks(1) = breaks1;
  values.MutableVehicleBreaks(2) = breaks2;
  EXPECT_THAT(values.VehicleBreaks(0), ElementsAreArray(breaks0));
  EXPECT_THAT(values.VehicleBreaks(1), ElementsAreArray(breaks1));
  EXPECT_THAT(values.VehicleBreaks(2), ElementsAreArray(breaks2));
  // Commit values, they should stay the same.
  values.Commit();
  EXPECT_THAT(values.VehicleBreaks(0), ElementsAreArray(breaks0));
  EXPECT_THAT(values.VehicleBreaks(1), ElementsAreArray(breaks1));
  EXPECT_THAT(values.VehicleBreaks(2), ElementsAreArray(breaks2));
  // Change span values, check getters.
  values.MakePathFromNewNodes(0);
  values.MakePathFromNewNodes(1);
  values.MakePathFromNewNodes(2);
  values.MutableVehicleBreaks(0) = breaks1;
  values.MutableVehicleBreaks(1) = breaks2;
  values.MutableVehicleBreaks(2) = breaks0;
  EXPECT_THAT(values.VehicleBreaks(0), ElementsAreArray(breaks1));
  EXPECT_THAT(values.VehicleBreaks(1), ElementsAreArray(breaks2));
  EXPECT_THAT(values.VehicleBreaks(2), ElementsAreArray(breaks0));
  // Revert, check that span got back to its committed values.
  values.Revert();
  EXPECT_THAT(values.VehicleBreaks(0), ElementsAreArray(breaks0));
  EXPECT_THAT(values.VehicleBreaks(1), ElementsAreArray(breaks1));
  EXPECT_THAT(values.VehicleBreaks(2), ElementsAreArray(breaks2));
}

// Set a committed state, add some changes, check that CommittedTravels() points
// to the committed state and not the current state.
TEST(DimensionValuesTest, CommittedTravels) {
  DimensionValues values(1, 5);
  values.PushNode(1);
  values.PushNode(2);
  values.PushNode(3);
  values.PushNode(4);
  values.MakePathFromNewNodes(0);
  {
    absl::Span<int64_t> travels = values.MutableTravels(0);
    travels[0] = 10;
    travels[1] = 20;
    travels[2] = 30;
    values.Commit();
  }
  // Change the state.
  values.PushNode(1);
  values.PushNode(3);
  values.PushNode(2);
  values.PushNode(4);
  values.MakePathFromNewNodes(0);
  {
    absl::Span<int64_t> travels = values.MutableTravels(0);
    travels[0] = 1;
    travels[1] = 2;
    travels[2] = 3;
  }
  EXPECT_THAT(values.CommittedTravels(0), ElementsAre(10, 20, 30));
  EXPECT_THAT(values.Travels(0), ElementsAre(1, 2, 3));
}

#define TEST_DIMENSION_VALUES_MAKE_INTERVAL(value) \
  DimensionValues::Interval { value, value }

#define TEST_DIMENSION_VALUES_COMMIT_REVERT_FIELD(field, path, make)         \
  TEST(DimensionValuesTest, CommitRevert##field) {                           \
    const int num_path = 3;                                                  \
    const int num_nodes = 9;                                                 \
    DimensionValues values(num_path, num_nodes);                             \
    const std::vector<int> nodes0 = {0, 1, 2};                               \
    const std::vector<int> nodes1 = {3, 4, 5, 6};                            \
    const std::vector<int> nodes2 = {7, 8};                                  \
    for (const int node : nodes0) values.PushNode(node);                     \
    values.MakePathFromNewNodes(0);                                          \
    for (const int node : nodes1) values.PushNode(node);                     \
    values.MakePathFromNewNodes(1);                                          \
    for (const int node : nodes2) values.PushNode(node);                     \
    values.MakePathFromNewNodes(2);                                          \
    {                                                                        \
      auto mutable_view = values.Mutable##field(path);                       \
      mutable_view[0] = make(10);                                            \
      mutable_view[1] = make(20);                                            \
      mutable_view[2] = make(30);                                            \
      auto const_view = values.field(path);                                  \
      EXPECT_THAT(const_view, ElementsAre(make(10), make(20), make(30)));    \
      values.Commit();                                                       \
      EXPECT_THAT(const_view, ElementsAre(make(10), make(20), make(30)));    \
    }                                                                        \
    for (const int node : nodes##path) values.PushNode(node);                \
    values.MakePathFromNewNodes(path);                                       \
    {                                                                        \
      auto mutable_view = values.Mutable##field(path);                       \
      mutable_view[0] = make(100);                                           \
      mutable_view[1] = make(200);                                           \
      mutable_view[2] = make(300);                                           \
      auto const_view = values.field(path);                                  \
      EXPECT_THAT(const_view, ElementsAre(make(100), make(200), make(300))); \
    }                                                                        \
    values.Revert();                                                         \
    {                                                                        \
      auto const_view = values.field(path);                                  \
      EXPECT_THAT(const_view, ElementsAre(make(10), make(20), make(30)));    \
    }                                                                        \
  }

TEST_DIMENSION_VALUES_COMMIT_REVERT_FIELD(Cumuls, 0,
                                          TEST_DIMENSION_VALUES_MAKE_INTERVAL)
TEST_DIMENSION_VALUES_COMMIT_REVERT_FIELD(Transits, 1,
                                          TEST_DIMENSION_VALUES_MAKE_INTERVAL)
TEST_DIMENSION_VALUES_COMMIT_REVERT_FIELD(Travels, 1, int64_t)
TEST_DIMENSION_VALUES_COMMIT_REVERT_FIELD(TravelSums, 0, int64_t)

#undef TEST_DIMENSION_VALUES_COMMIT_REVERT_FIELD
#undef TEST_DIMENSION_VALUES_MAKE_INTERVAL

TEST(PrePostVisitValuesTest, Empty) {
  const int num_paths = 3;
  const int num_nodes = 15;
  PrePostVisitValues values(num_paths, num_nodes);
  for (int p = 0; p < num_paths; ++p) {
    // Value sequences.
    EXPECT_TRUE(values.CommittedPreVisits(p).empty());
    EXPECT_TRUE(values.PreVisits(p).empty());
    EXPECT_TRUE(values.MutablePreVisits(p).empty());
    EXPECT_TRUE(values.CommittedPostVisits(p).empty());
    EXPECT_TRUE(values.PostVisits(p).empty());
    EXPECT_TRUE(values.MutablePostVisits(p).empty());
    // Paths.
    EXPECT_FALSE(values.PathHasChanged(p));
    EXPECT_EQ(0, values.NumNodes(p));
  }
  // Paths.
  EXPECT_EQ(0, values.ChangedPaths().size());
}

TEST(PrePostVisitValuesTest, ChangePrePostVisitsOfPath) {
  const int num_paths = 3;
  const int num_nodes = 15;
  PrePostVisitValues values(num_paths, num_nodes);
  // Change path 1, it has 3 nodes.
  values.ChangePathSize(1, 3);
  EXPECT_EQ(values.NumNodes(0), 0);
  EXPECT_EQ(values.NumNodes(1), 3);
  EXPECT_EQ(values.NumNodes(2), 0);
  EXPECT_FALSE(values.PathHasChanged(0));
  EXPECT_TRUE(values.PathHasChanged(1));
  EXPECT_FALSE(values.PathHasChanged(2));
}

// Start from some pre/post visits, check that commit
TEST(PrePostVisitValuesTest, MakePathCommitRevert) {
  const int num_path = 3;
  const int num_nodes = 9;
  PrePostVisitValues values(num_path, num_nodes);
  // Set some initial values, commit.
  const std::vector<int64_t> pre_visits0 = {3, 9, 7};
  const std::vector<int64_t> post_visits0 = {30, 90, 70};
  values.ChangePathSize(1, 3);
  absl::c_copy(pre_visits0, values.MutablePreVisits(1).begin());
  absl::c_copy(post_visits0, values.MutablePostVisits(1).begin());
  EXPECT_THAT(values.PreVisits(1), ElementsAreArray(pre_visits0));
  EXPECT_THAT(values.PostVisits(1), ElementsAreArray(post_visits0));
  values.Commit();
  // Change the values, check that committed/current values are correct.
  // Iterate many time to exercise vector compaction.
  const std::vector<int64_t> pre_visits1 = {1, 2, 3, 4};
  const std::vector<int64_t> post_visits1 = {10, 20, 30, 40};
  const std::vector<int64_t> pre_visits2 = {5, 6, 7, 8, 9};
  const std::vector<int64_t> post_visits2 = {500, 600, 700, 800, 900};
  for (int it = 0; it < 50; ++it) {
    // Try values 1.
    values.ChangePathSize(1, pre_visits1.size());
    EXPECT_THAT(values.CommittedPreVisits(1), ElementsAreArray(pre_visits0));
    EXPECT_THAT(values.CommittedPostVisits(1), ElementsAreArray(post_visits0));
    absl::c_copy(pre_visits1, values.MutablePreVisits(1).begin());
    absl::c_copy(post_visits1, values.MutablePostVisits(1).begin());
    EXPECT_THAT(values.PreVisits(1), ElementsAreArray(pre_visits1));
    EXPECT_THAT(values.PostVisits(1), ElementsAreArray(post_visits1));
    EXPECT_THAT(values.CommittedPreVisits(1), ElementsAreArray(pre_visits0));
    EXPECT_THAT(values.CommittedPostVisits(1), ElementsAreArray(post_visits0));
    // Revert.
    values.Revert();
    EXPECT_THAT(values.PreVisits(1), ElementsAreArray(pre_visits0));
    EXPECT_THAT(values.PostVisits(1), ElementsAreArray(post_visits0));
    // Go to values 2, commit.
    values.ChangePathSize(1, pre_visits2.size());
    absl::c_copy(pre_visits2, values.MutablePreVisits(1).begin());
    absl::c_copy(post_visits2, values.MutablePostVisits(1).begin());
    EXPECT_THAT(values.PreVisits(1), ElementsAreArray(pre_visits2));
    EXPECT_THAT(values.PostVisits(1), ElementsAreArray(post_visits2));
    EXPECT_THAT(values.CommittedPreVisits(1), ElementsAreArray(pre_visits0));
    EXPECT_THAT(values.CommittedPostVisits(1), ElementsAreArray(post_visits0));
    values.Commit();
    EXPECT_THAT(values.PreVisits(1), ElementsAreArray(pre_visits2));
    EXPECT_THAT(values.PostVisits(1), ElementsAreArray(post_visits2));
    EXPECT_THAT(values.CommittedPreVisits(1), ElementsAreArray(pre_visits2));
    EXPECT_THAT(values.CommittedPostVisits(1), ElementsAreArray(post_visits2));
    // Go to values 0, commit.
    values.ChangePathSize(1, pre_visits0.size());
    absl::c_copy(pre_visits0, values.MutablePreVisits(1).begin());
    absl::c_copy(post_visits0, values.MutablePostVisits(1).begin());
    EXPECT_THAT(values.PreVisits(1), ElementsAreArray(pre_visits0));
    EXPECT_THAT(values.PostVisits(1), ElementsAreArray(post_visits0));
    EXPECT_THAT(values.CommittedPreVisits(1), ElementsAreArray(pre_visits2));
    EXPECT_THAT(values.CommittedPostVisits(1), ElementsAreArray(post_visits2));
    values.Commit();
    EXPECT_THAT(values.PreVisits(1), ElementsAreArray(pre_visits0));
    EXPECT_THAT(values.PostVisits(1), ElementsAreArray(post_visits0));
    EXPECT_THAT(values.CommittedPreVisits(1), ElementsAreArray(pre_visits0));
    EXPECT_THAT(values.CommittedPostVisits(1), ElementsAreArray(post_visits0));
  }
}

TEST(PrePostVisitValuesTest, Reset) {
  const int num_paths = 3;
  const int num_nodes = 15;
  PrePostVisitValues values(num_paths, num_nodes);
  // Set some initial values, commit.
  const std::vector<int64_t> pre_visits0 = {3, 9, 7};
  const std::vector<int64_t> post_visits0 = {30, 90, 70};
  values.ChangePathSize(1, 3);
  absl::c_copy(pre_visits0, values.MutablePreVisits(1).begin());
  absl::c_copy(post_visits0, values.MutablePostVisits(1).begin());
  EXPECT_THAT(values.PreVisits(1), ElementsAreArray(pre_visits0));
  EXPECT_THAT(values.PostVisits(1), ElementsAreArray(post_visits0));
  values.Commit();
  // Reset, check that all values are 0.
  values.Reset();
  for (int p = 0; p < num_paths; ++p) {
    EXPECT_TRUE(values.CommittedPreVisits(p).empty());
    EXPECT_TRUE(values.PreVisits(p).empty());
    EXPECT_TRUE(values.MutablePreVisits(p).empty());
    EXPECT_TRUE(values.CommittedPostVisits(p).empty());
    EXPECT_TRUE(values.PostVisits(p).empty());
    EXPECT_TRUE(values.MutablePostVisits(p).empty());
    EXPECT_FALSE(values.PathHasChanged(p));
    EXPECT_EQ(0, values.NumNodes(p));
  }
}

TEST(DimensionValuesIntervalTest, ConstOperations) {
  using Interval = DimensionValues::Interval;
  EXPECT_TRUE(Interval(1, 2) == Interval(1, 2));
  EXPECT_FALSE(Interval(1, 2) != Interval(1, 2));

  EXPECT_FALSE(Interval(1, 2) == Interval(0, 2));
  EXPECT_TRUE(Interval(1, 2) != Interval(0, 2));

  EXPECT_FALSE(Interval(1, 2) == Interval(1, 3));
  EXPECT_TRUE(Interval(1, 2) != Interval(1, 3));

  EXPECT_FALSE(Interval(1, 2).IsEmpty());
  EXPECT_TRUE(Interval(2, 1).IsEmpty());
}

TEST(DimensionValuesIntervalTest, IncreaseDecreaseIntersect) {
  using Interval = DimensionValues::Interval;
  Interval interval(-10, 10);
  // IncreaseMin(): feasible, infeasible.
  EXPECT_TRUE(interval.IncreaseMin(10));
  EXPECT_EQ(interval, Interval(10, 10));
  EXPECT_FALSE(interval.IncreaseMin(11));
  // DecreaseMax(): feasible, infeasible.
  interval = {.min = -10, .max = 10};
  EXPECT_TRUE(interval.DecreaseMax(-10));
  EXPECT_EQ(interval, Interval(-10, -10));
  EXPECT_FALSE(interval.DecreaseMax(-11));
  // Intersect with larger interval.
  interval = {.min = -10, .max = 10};
  EXPECT_TRUE(interval.IntersectWith(Interval(-100, 100)));
  EXPECT_EQ(interval, Interval(-10, 10));
  // Intersect with smaller interval.
  interval = {.min = -10, .max = 10};
  EXPECT_TRUE(interval.IntersectWith(Interval(-1, 1)));
  EXPECT_EQ(interval, Interval(-1, 1));
  // Intersect with interval that has larger min.
  interval = {.min = -10, .max = 10};
  EXPECT_TRUE(interval.IntersectWith(Interval(-1, 21)));
  EXPECT_EQ(interval, Interval(-1, 10));
  // Intersect with interval that has smaller max.
  interval = {.min = -10, .max = 10};
  EXPECT_TRUE(interval.IntersectWith(Interval(-21, 1)));
  EXPECT_EQ(interval, Interval(-10, 1));
  // Intersect with disjoint interval, from below.
  interval = {.min = -10, .max = 10};
  EXPECT_FALSE(interval.IntersectWith(Interval(-21, -11)));
  // Intersect with disjoint interval, from above.
  interval = {.min = -10, .max = 10};
  EXPECT_FALSE(interval.IntersectWith(Interval(-21, -11)));
}

TEST(DimensionValuesIntervalTest, AddSubtract) {
  using Interval = DimensionValues::Interval;
  Interval i1(-10, 2);
  Interval i2(-20, 15);
  // operator+.
  EXPECT_EQ(i1 + i2, Interval(-30, 17));
  // Add.
  i1.Add(i2);
  EXPECT_EQ(i1, Interval(-30, 17));
  // Subtract.
  i1 = {.min = -10, .max = 2};
  EXPECT_EQ(i1 - i2, Interval(-25, 22));
  i1.Subtract(i2);
  EXPECT_EQ(i1, Interval(-25, 22));
  // Add must be capped.
  i1 = {.min = -10, .max = 10};
  i2 = Interval::AllIntegers();
  EXPECT_EQ(i1 + i2, Interval::AllIntegers());
  i1.Add(i2);
  EXPECT_EQ(i1, Interval::AllIntegers());
  // Add must be capped.
  i1 = Interval::AllIntegers();
  i2 = {.min = -10, .max = 10};
  EXPECT_EQ(i1 + i2, Interval::AllIntegers());
  i1.Add(i2);
  EXPECT_EQ(i1, Interval::AllIntegers());
  // Subtract must be capped.
  i1 = {.min = -10, .max = 10};
  i2 = Interval::AllIntegers();
  EXPECT_EQ(i1 - i2, Interval::AllIntegers());
  i1.Subtract(i2);
  EXPECT_EQ(i1, Interval::AllIntegers());
  // Subtract must be capped.
  i1 = Interval::AllIntegers();
  i2 = {.min = -10, .max = 10};
  EXPECT_EQ(i1 - i2, Interval::AllIntegers());
  i1.Subtract(i2);
  EXPECT_EQ(i1, Interval::AllIntegers());
}

// Tests a situation where the transit constraint must reduce time windows.
TEST(PropagateTransitAndSpan, FeasibleTransits) {
  using Interval = DimensionValues::Interval;
  const DimensionValuesPathScenario scenario = {
      .nodes = {1, 0, 3},
      .cumuls = std::vector<Interval>(3, {.min = 0, .max = 10}),
      .transits = std::vector<Interval>(2, {1, 2}),
  };
  const DimensionValuesPathScenario expected_scenario = {
      .nodes = {1, 0, 3},
      .cumuls = {{.min = 0, .max = 8},
                 {.min = 1, .max = 9},
                 {.min = 2, .max = 10}},
  };
  DimensionValues dimension(1, 4);
  FillDimensionValuesWithPathScenario(scenario, 0, dimension);
  EXPECT_TRUE(PropagateTransitAndSpan(0, dimension));
  ExpectDimensionValuesScenario(expected_scenario, 0, dimension);
}

// Total travel is 8, but global time window is 7 long.
TEST(PropagateTransitAndSpan, InfeasibleTransits) {
  using Interval = DimensionValues::Interval;
  const DimensionValuesPathScenario scenario = {
      .nodes = {1, 0, 3},
      .cumuls = std::vector<Interval>(3, {.min = 0, .max = 7}),
      .travels = {4, 4},
  };
  DimensionValues dimension(1, 4);
  FillDimensionValuesWithPathScenario(scenario, 0, dimension);
  EXPECT_FALSE(PropagateTransitAndSpan(0, dimension));
}

// Time windows force end - start to be larger than total travel.
// This should be propagated to the span.
TEST(PropagateTransitAndSpan, TimeWindowsToSpan) {
  using Interval = DimensionValues::Interval;
  const DimensionValuesPathScenario scenario = {
      .nodes = {1, 0, 3},
      .cumuls = {{.min = 0, .max = 1},
                 {.min = 4, .max = 7},
                 {.min = 0, .max = 7}},
      .travels = {2, 2},
  };
  const DimensionValuesPathScenario expected_scenario = {
      .nodes = {1, 0, 3},
      .cumuls = {{.min = 0, .max = 1},
                 {.min = 4, .max = 5},
                 {.min = 6, .max = 7}},
      .span = Interval{.min = 5, .max = 7},
  };
  for (int s = 0; s < 3; ++s) {
    DimensionValues dimension(1, 4);
    FillDimensionValuesWithPathScenario(scenario, 0, dimension);
    if (s == 0) {
      EXPECT_TRUE(PropagateTransitAndSpan(0, dimension));
      ExpectDimensionValuesScenario(expected_scenario, 0, dimension);
    } else if (s == 1) {
      dimension.MutableSpan(0) = {.min = 8, .max = 10};
      EXPECT_FALSE(PropagateTransitAndSpan(0, dimension));
    } else {
      dimension.MutableSpan(0) = {.min = 2, .max = 4};
      EXPECT_FALSE(PropagateTransitAndSpan(0, dimension));
    }
  }
}

TEST(PropagateTransitAndSpan, SpanToTimeWindows) {
  using Interval = DimensionValues::Interval;
  const DimensionValuesPathScenario scenario = {
      .nodes = {1, 0, 3, 2, 4},
      .cumuls = {{.min = 0, .max = 10},
                 {.min = 0, .max = 3},
                 {.min = 0, .max = 10},
                 {.min = 7, .max = 10},
                 {.min = 0, .max = 10}},
      .travels = {2, 2, 2, 2},
      .span = Interval{.min = 5, .max = 8},
  };
  const DimensionValuesPathScenario expected_scenario = {
      .nodes = {1, 0, 3, 2, 4},
      .cumuls = {{.min = 1, .max = 1},
                 {.min = 3, .max = 3},
                 {.min = 5, .max = 5},
                 {.min = 7, .max = 7},
                 {.min = 9, .max = 9}},
      .travels = {2, 2, 2, 2},
      .span = Interval{.min = 8, .max = 8},
  };
  DimensionValues dimension(1, 5);
  FillDimensionValuesWithPathScenario(scenario, 0, dimension);
  EXPECT_TRUE(PropagateTransitAndSpan(0, dimension));
  ExpectDimensionValuesScenario(expected_scenario, 0, dimension);
}

}  // namespace

}  // namespace operations_research::routing
