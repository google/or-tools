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

#include "ortools/sat/diffn_util.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/log/check.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "absl/strings/str_join.h"
#include "absl/types/span.h"
#include "benchmark/benchmark.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/logging.h"
#include "ortools/graph/connected_components.h"
#include "ortools/sat/2d_orthogonal_packing_testing.h"
#include "ortools/sat/integer.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {
namespace {

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

TEST(GetOverlappingRectangleComponentsTest, NoComponents) {
  EXPECT_TRUE(GetOverlappingRectangleComponents({}, {}).empty());
  IntegerValue zero(0);
  IntegerValue two(2);
  IntegerValue four(4);
  EXPECT_TRUE(GetOverlappingRectangleComponents(
                  {{zero, two, zero, two}, {two, four, two, four}}, {})
                  .empty());
  std::vector<int> first = {0};
  EXPECT_TRUE(GetOverlappingRectangleComponents(
                  {{zero, two, zero, two}, {two, four, two, four}},
                  absl::MakeSpan(first))
                  .empty());
  std::vector<int> both = {0, 1};
  EXPECT_TRUE(GetOverlappingRectangleComponents(
                  {{zero, two, zero, two}, {two, four, two, four}},
                  absl::MakeSpan(both))
                  .empty());
  EXPECT_TRUE(GetOverlappingRectangleComponents(
                  {{zero, two, zero, two}, {two, four, zero, two}},
                  absl::MakeSpan(both))
                  .empty());
  EXPECT_TRUE(GetOverlappingRectangleComponents(
                  {{zero, two, zero, two}, {zero, two, two, four}},
                  absl::MakeSpan(both))
                  .empty());
}

TEST(GetOverlappingRectangleComponentsTest, ComponentAndActive) {
  EXPECT_TRUE(GetOverlappingRectangleComponents({}, {}).empty());
  IntegerValue zero(0);
  IntegerValue one(1);
  IntegerValue two(2);
  IntegerValue three(3);
  IntegerValue four(4);

  std::vector<int> all = {0, 1, 2};
  const auto& components = GetOverlappingRectangleComponents(
      {{zero, two, zero, two}, {zero, two, one, three}, {zero, two, two, four}},
      absl::MakeSpan(all));
  ASSERT_EQ(1, components.size());
  EXPECT_EQ(3, components[0].size());

  std::vector<int> only_two = {0, 2};
  EXPECT_TRUE(GetOverlappingRectangleComponents({{zero, two, zero, two},
                                                 {zero, two, one, three},
                                                 {zero, two, two, four}},
                                                absl::MakeSpan(only_two))
                  .empty());
}

TEST(AnalyzeIntervalsTest, Random) {
  // Generate a random set of intervals until the first conflict. We are in n^5!
  absl::BitGen random;
  const int64_t size = 20;
  std::vector<Rectangle> rectangles;
  std::vector<IntegerValue> energies;
  std::vector<int> boxes;
  for (int i = 0; i < 40; ++i) {
    Rectangle box;
    box.x_min = IntegerValue(absl::Uniform(random, 0, size));
    box.x_max =
        IntegerValue(absl::Uniform(random, box.x_min.value() + 1, size + 1));
    box.y_min = IntegerValue(absl::Uniform(random, 0, size));
    box.y_max =
        IntegerValue(absl::Uniform(random, box.y_min.value() + 1, size + 1));
    rectangles.push_back(box);
    boxes.push_back(i);
    energies.push_back(IntegerValue(absl::Uniform(
                           random, 1, (box.x_max - box.x_min + 1).value())) *
                       IntegerValue(absl::Uniform(
                           random, 1, (box.y_max - box.y_min + 1).value())));

    LOG(INFO) << i << " " << box << " energy:" << energies.back();
    Rectangle conflict;
    if (!BoxesAreInEnergyConflict(rectangles, energies, boxes, &conflict)) {
      continue;
    }

    LOG(INFO) << "Conflict! " << conflict;

    // Make sure whatever filter we do, we do not remove the conflict.
    absl::Span<int> s = absl::MakeSpan(boxes);
    IntegerValue threshold_x = kMaxIntegerValue;
    IntegerValue threshold_y = kMaxIntegerValue;
    for (int i = 0; i < 4; ++i) {
      if (!AnalyzeIntervals(/*transpose=*/i % 2 == 1, s, rectangles, energies,
                            &threshold_x, &threshold_y)) {
        LOG(INFO) << "Detected by analyse.";
        return;
      }
      s = FilterBoxesAndRandomize(rectangles, s, threshold_x, threshold_y,
                                  random);
      LOG(INFO) << "Filtered size: " << s.size() << " x<=" << threshold_x
                << " y<=" << threshold_y;
      ASSERT_TRUE(BoxesAreInEnergyConflict(rectangles, energies, s));
    }

    break;
  }
}

TEST(FilterBoxesThatAreTooLargeTest, Empty) {
  std::vector<Rectangle> r;
  std::vector<IntegerValue> energies;
  std::vector<int> boxes;
  EXPECT_TRUE(
      FilterBoxesThatAreTooLarge(r, energies, absl::MakeSpan(boxes)).empty());
}

TEST(FilterBoxesThatAreTooLargeTest, BasicTest) {
  int num_boxes(3);
  std::vector<Rectangle> r(num_boxes);
  std::vector<IntegerValue> energies(num_boxes, IntegerValue(25));
  std::vector<int> boxes{0, 1, 2};

  r[0] = {IntegerValue(0), IntegerValue(5), IntegerValue(0), IntegerValue(5)};
  r[1] = {IntegerValue(0), IntegerValue(10), IntegerValue(0), IntegerValue(10)};
  r[2] = {IntegerValue(0), IntegerValue(6), IntegerValue(0), IntegerValue(6)};

  EXPECT_THAT(FilterBoxesThatAreTooLarge(r, energies, absl::MakeSpan(boxes)),
              ElementsAre(0, 2));
}

TEST(ConstructOverlappingSetsTest, BasicTest) {
  std::vector<std::vector<int>> result{{3}};  // To be sure we clear.

  //    --------------------0
  //    --------1   --------2
  //        ------------3
  //          ------4
  std::vector<IndexedInterval> intervals{{0, IntegerValue(0), IntegerValue(10)},
                                         {1, IntegerValue(0), IntegerValue(4)},
                                         {2, IntegerValue(6), IntegerValue(10)},
                                         {3, IntegerValue(2), IntegerValue(8)},
                                         {4, IntegerValue(3), IntegerValue(6)}};

  // Note that the order is deterministic, but not sorted.
  ConstructOverlappingSets(/*already_sorted=*/false, &intervals, &result);
  EXPECT_THAT(result, ElementsAre(UnorderedElementsAre(0, 1, 3, 4),
                                  UnorderedElementsAre(3, 0, 2)));
}

TEST(ConstructOverlappingSetsTest, OneSet) {
  std::vector<std::vector<int>> result{{3}};  // To be sure we clear.

  std::vector<IndexedInterval> intervals{
      {0, IntegerValue(0), IntegerValue(10)},
      {1, IntegerValue(1), IntegerValue(10)},
      {2, IntegerValue(2), IntegerValue(10)},
      {3, IntegerValue(3), IntegerValue(10)},
      {4, IntegerValue(4), IntegerValue(10)}};

  ConstructOverlappingSets(/*already_sorted=*/false, &intervals, &result);
  EXPECT_THAT(result, ElementsAre(ElementsAre(0, 1, 2, 3, 4)));
}

TEST(GetOverlappingIntervalComponentsTest, BasicTest) {
  std::vector<std::vector<int>> components{{3}};  // To be sure we clear.

  std::vector<IndexedInterval> intervals{{0, IntegerValue(0), IntegerValue(3)},
                                         {1, IntegerValue(2), IntegerValue(4)},
                                         {2, IntegerValue(4), IntegerValue(7)},
                                         {3, IntegerValue(8), IntegerValue(10)},
                                         {4, IntegerValue(5), IntegerValue(9)}};

  GetOverlappingIntervalComponents(&intervals, &components);
  EXPECT_THAT(components, ElementsAre(ElementsAre(0, 1), ElementsAre(2, 4, 3)));
}

TEST(GetOverlappingIntervalComponentsAndArticulationPointsTest,
     WithWeirdIndicesAndSomeCornerCases) {
  // Here are our intervals:               2======5   7====9
  // They are indexed from top to      0===2    4=====7 8======11
  // bottom, from left to right,         1===3    5=6 7=8
  // starting at 10.
  std::vector<IndexedInterval> intervals{
      {10, IntegerValue(2), IntegerValue(5)},
      {11, IntegerValue(7), IntegerValue(9)},
      {12, IntegerValue(0), IntegerValue(2)},
      {13, IntegerValue(4), IntegerValue(7)},
      {14, IntegerValue(8), IntegerValue(11)},
      {15, IntegerValue(1), IntegerValue(3)},
      {16, IntegerValue(5), IntegerValue(6)},
      {17, IntegerValue(7), IntegerValue(8)},
  };

  std::vector<std::vector<int>> components;
  GetOverlappingIntervalComponents(&intervals, &components);
  EXPECT_THAT(components, ElementsAre(ElementsAre(12, 15, 10, 13, 16),
                                      ElementsAre(17, 11, 14)));

  EXPECT_THAT(GetIntervalArticulationPoints(&intervals),
              ElementsAre(15, 10, 13, 11));
}

std::vector<IndexedInterval> GenerateRandomIntervalVector(
    absl::BitGenRef random, int num_intervals) {
  std::vector<IndexedInterval> intervals;
  intervals.reserve(num_intervals);
  const int64_t interval_domain =
      absl::LogUniform<int64_t>(random, 1, std::numeric_limits<int64_t>::max());
  const int64_t max_interval_length = absl::Uniform<int64_t>(
      random, std::max<int64_t>(1, interval_domain / (2 * num_intervals + 1)),
      interval_domain);
  for (int i = 0; i < num_intervals; ++i) {
    const int64_t start = absl::Uniform(random, 0, interval_domain);
    const int64_t max_length =
        std::min(interval_domain - start, max_interval_length);
    const int64_t end =
        start + absl::Uniform(absl::IntervalClosed, random, 1, max_length);
    intervals.push_back(
        IndexedInterval{i, IntegerValue(start), IntegerValue(end)});
  }
  return intervals;
}

std::vector<std::vector<int>> GetOverlappingIntervalComponentsBruteForce(
    const std::vector<IndexedInterval>& intervals) {
  // Build the adjacency list.
  std::vector<std::vector<int>> adj(intervals.size());
  for (int i = 1; i < intervals.size(); ++i) {
    for (int j = 0; j < i; ++j) {
      if (std::max(intervals[i].start, intervals[j].start) <
          std::min(intervals[i].end, intervals[j].end)) {
        adj[i].push_back(j);
        adj[j].push_back(i);
      }
    }
  }
  std::vector<int> component_indices =
      util::GetConnectedComponents(intervals.size(), adj);
  if (component_indices.empty()) return {};
  // Transform that into the expected output: a vector of components.
  std::vector<std::vector<int>> components(
      *absl::c_max_element(component_indices) + 1);
  for (int i = 0; i < intervals.size(); ++i) {
    components[component_indices[i]].push_back(i);
  }
  // Sort the components by start, like GetOverlappingIntervalComponents().
  absl::c_sort(components, [&intervals](const std::vector<int>& c1,
                                        const std::vector<int>& c2) {
    CHECK(!c1.empty() && !c2.empty());
    return intervals[c1[0]].start < intervals[c2[0]].start;
  });
  // Inside each component, the intervals should be sorted, too.
  // Moreover, we need to convert our indices to IntervalIndex.index.
  for (std::vector<int>& component : components) {
    absl::c_sort(component, [&intervals](int i, int j) {
      return IndexedInterval::ComparatorByStartThenEndThenIndex()(intervals[i],
                                                                  intervals[j]);
    });
    for (int& index : component) index = intervals[index].index;
  }
  return components;
}

TEST(GetOverlappingIntervalComponentsTest, RandomizedStressTest) {
  // Test duration as of 2021-06: .6s in fastbuild, .3s in opt.
  constexpr int kNumTests = 10000;
  absl::BitGen random;
  for (int test = 0; test < kNumTests; ++test) {
    const int num_intervals = absl::Uniform(random, 0, 16);
    std::vector<IndexedInterval> intervals =
        GenerateRandomIntervalVector(random, num_intervals);
    const std::vector<IndexedInterval> intervals_copy = intervals;
    std::vector<std::vector<int>> components;
    GetOverlappingIntervalComponents(&intervals, &components);
    ASSERT_THAT(
        components,
        ElementsAreArray(GetOverlappingIntervalComponentsBruteForce(intervals)))
        << test << " " << absl::StrJoin(intervals_copy, ",");
    // Also verify that the function only altered the order of "intervals".
    EXPECT_THAT(intervals, UnorderedElementsAreArray(intervals_copy));
    ASSERT_FALSE(HasFailure())
        << test << " " << absl::StrJoin(intervals_copy, ",");
  }
}

TEST(GetIntervalArticulationPointsTest, RandomizedStressTest) {
  // THIS TEST ASSUMES THAT GetOverlappingIntervalComponents() IS CORRECT.
  // -> don't look at it if GetOverlappingIntervalComponentsTest.StressTest
  //    fails, and rather investigate that other test first.

  auto get_num_components = [](const std::vector<IndexedInterval>& intervals) {
    std::vector<IndexedInterval> mutable_intervals = intervals;
    std::vector<std::vector<int>> components;
    GetOverlappingIntervalComponents(&mutable_intervals, &components);
    return components.size();
  };
  // Test duration as of 2021-06: 1s in fastbuild, .4s in opt.
  constexpr int kNumTests = 10000;
  absl::BitGen random;
  for (int test = 0; test < kNumTests; ++test) {
    const int num_intervals = absl::Uniform(random, 0, 16);
    const std::vector<IndexedInterval> intervals =
        GenerateRandomIntervalVector(random, num_intervals);
    const int baseline_num_components = get_num_components(intervals);

    // Compute the expected articulation points: try removing each interval
    // individually and check whether there are more components if we do.
    std::vector<int> expected_articulation_points;
    for (int i = 0; i < num_intervals; ++i) {
      std::vector<IndexedInterval> tmp_intervals = intervals;
      tmp_intervals.erase(tmp_intervals.begin() + i);
      if (get_num_components(tmp_intervals) > baseline_num_components) {
        expected_articulation_points.push_back(i);
      }
    }
    // Sort the articulation points by start, and replace them by their
    // corresponding IndexedInterval.index.
    absl::c_sort(expected_articulation_points, [&intervals](int i, int j) {
      return intervals[i].start < intervals[j].start;
    });
    for (int& idx : expected_articulation_points) idx = intervals[idx].index;

    // Compare our function with the expected values.
    std::vector<IndexedInterval> mutable_intervals = intervals;
    EXPECT_THAT(GetIntervalArticulationPoints(&mutable_intervals),
                ElementsAreArray(expected_articulation_points));

    // Also verify that the function only altered the order of "intervals".
    EXPECT_THAT(mutable_intervals, UnorderedElementsAreArray(intervals));
    ASSERT_FALSE(HasFailure()) << test << " " << absl::StrJoin(intervals, ",");
  }
}

TEST(CapacityProfileTest, BasicApi) {
  CapacityProfile profile;
  profile.AddRectangle(IntegerValue(2), IntegerValue(6), IntegerValue(0),
                       IntegerValue(2));
  profile.AddRectangle(IntegerValue(4), IntegerValue(12), IntegerValue(0),
                       IntegerValue(1));
  profile.AddRectangle(IntegerValue(4), IntegerValue(8), IntegerValue(0),
                       IntegerValue(5));
  std::vector<CapacityProfile::Rectangle> result;
  profile.BuildResidualCapacityProfile(&result);
  EXPECT_THAT(
      result,
      ElementsAre(
          CapacityProfile::Rectangle(kMinIntegerValue, IntegerValue(0)),
          CapacityProfile::Rectangle(IntegerValue(2), IntegerValue(2)),
          CapacityProfile::Rectangle(IntegerValue(4), IntegerValue(5)),
          CapacityProfile::Rectangle(IntegerValue(8), IntegerValue(1)),
          CapacityProfile::Rectangle(IntegerValue(12), IntegerValue(0))));

  // We query it twice to test that it can be done and that the result is not
  // messed up.
  profile.BuildResidualCapacityProfile(&result);
  EXPECT_THAT(
      result,
      ElementsAre(
          CapacityProfile::Rectangle(kMinIntegerValue, IntegerValue(0)),
          CapacityProfile::Rectangle(IntegerValue(2), IntegerValue(2)),
          CapacityProfile::Rectangle(IntegerValue(4), IntegerValue(5)),
          CapacityProfile::Rectangle(IntegerValue(8), IntegerValue(1)),
          CapacityProfile::Rectangle(IntegerValue(12), IntegerValue(0))));
  EXPECT_EQ(IntegerValue(2 * 2 + 4 * 5 + 4 * 1), profile.GetBoundingArea());
}

TEST(CapacityProfileTest, ProfileWithMandatoryPart) {
  CapacityProfile profile;
  profile.AddRectangle(IntegerValue(2), IntegerValue(6), IntegerValue(0),
                       IntegerValue(2));
  profile.AddRectangle(IntegerValue(4), IntegerValue(12), IntegerValue(0),
                       IntegerValue(1));
  profile.AddRectangle(IntegerValue(4), IntegerValue(8), IntegerValue(0),
                       IntegerValue(5));
  profile.AddMandatoryConsumption(IntegerValue(5), IntegerValue(10),
                                  IntegerValue(1));
  std::vector<CapacityProfile::Rectangle> result;

  // Add a dummy rectangle to test the result is cleared. result.push_bask(..);
  result.push_back(
      CapacityProfile::Rectangle(IntegerValue(2), IntegerValue(3)));

  profile.BuildResidualCapacityProfile(&result);
  EXPECT_THAT(
      result,
      ElementsAre(
          CapacityProfile::Rectangle(kMinIntegerValue, IntegerValue(0)),
          CapacityProfile::Rectangle(IntegerValue(2), IntegerValue(2)),
          CapacityProfile::Rectangle(IntegerValue(4), IntegerValue(5)),
          CapacityProfile::Rectangle(IntegerValue(5), IntegerValue(4)),
          CapacityProfile::Rectangle(IntegerValue(8), IntegerValue(0)),
          CapacityProfile::Rectangle(IntegerValue(10), IntegerValue(1)),
          CapacityProfile::Rectangle(IntegerValue(12), IntegerValue(0))));

  // The bounding area should not be impacted by the mandatory consumption.
  EXPECT_EQ(IntegerValue(2 * 2 + 4 * 5 + 4 * 1), profile.GetBoundingArea());
}

IntegerValue NaiveSmallest1DIntersection(IntegerValue range_min,
                                         IntegerValue range_max,
                                         IntegerValue size,
                                         IntegerValue interval_min,
                                         IntegerValue interval_max) {
  IntegerValue min_intersection = std::numeric_limits<IntegerValue>::max();
  for (IntegerValue start = range_min; start + size <= range_max; ++start) {
    // Interval is [start, start + size]
    const IntegerValue intersection_start = std::max(start, interval_min);
    const IntegerValue intersection_end = std::min(start + size, interval_max);
    const IntegerValue intersection_length =
        std::max(IntegerValue(0), intersection_end - intersection_start);
    min_intersection = std::min(min_intersection, intersection_length);
  }
  return min_intersection;
}

TEST(Smallest1DIntersectionTest, BasicTest) {
  absl::BitGen random;
  const int64_t max_size = 20;
  constexpr int num_runs = 400;
  for (int k = 0; k < num_runs; k++) {
    const IntegerValue range_min =
        IntegerValue(absl::Uniform(random, 0, max_size - 1));
    const IntegerValue range_max =
        IntegerValue(absl::Uniform(random, range_min.value() + 1, max_size));
    const IntegerValue size =
        absl::Uniform(random, 1, range_max.value() - range_min.value());

    const IntegerValue interval_min =
        IntegerValue(absl::Uniform(random, 0, max_size - 1));
    const IntegerValue interval_max =
        IntegerValue(absl::Uniform(random, interval_min.value() + 1, max_size));
    EXPECT_EQ(NaiveSmallest1DIntersection(range_min, range_max, size,
                                          interval_min, interval_max),
              Smallest1DIntersection(range_min, range_max, size, interval_min,
                                     interval_max));
  }
}

TEST(RectangleTest, BasicTest) {
  Rectangle r1 = {.x_min = 0, .x_max = 2, .y_min = 0, .y_max = 2};
  Rectangle r2 = {.x_min = 1, .x_max = 3, .y_min = 1, .y_max = 3};
  EXPECT_EQ(r1.Intersect(r2),
            Rectangle({.x_min = 1, .x_max = 2, .y_min = 1, .y_max = 2}));
}

TEST(RectangleTest, RandomRegionDifferenceTest) {
  absl::BitGen random;
  const int64_t size = 20;
  constexpr int num_runs = 400;
  for (int k = 0; k < num_runs; k++) {
    Rectangle ret[2];
    for (int i = 0; i < 2; ++i) {
      ret[i].x_min = IntegerValue(absl::Uniform(random, 0, size - 1));
      ret[i].x_max =
          ret[i].x_min + IntegerValue(absl::Uniform(random, 1, size - 1));
      ret[i].y_min = IntegerValue(absl::Uniform(random, 0, size - 1));
      ret[i].y_max =
          ret[i].y_min + IntegerValue(absl::Uniform(random, 1, size - 1));
    }
    auto set_diff = ret[0].RegionDifference(ret[1]);
    EXPECT_EQ(set_diff.empty(), ret[0].Intersect(ret[1]) == ret[0]);
    IntegerValue diff_area = 0;
    for (int i = 0; i < set_diff.size(); ++i) {
      for (int j = i + 1; j < set_diff.size(); ++j) {
        EXPECT_TRUE(set_diff[i].IsDisjoint(set_diff[j]));
      }
      EXPECT_NE(set_diff[i].Intersect(ret[0]), Rectangle::GetEmpty());
      EXPECT_EQ(set_diff[i].Intersect(ret[1]), Rectangle::GetEmpty());
      IntegerValue area = set_diff[i].Area();
      EXPECT_GT(area, 0);
      diff_area += area;
    }
    EXPECT_EQ(ret[0].IntersectArea(ret[1]) + diff_area, ret[0].Area());
  }
}

TEST(RectangleTest, RandomPavedRegionDifferenceTest) {
  absl::BitGen random;
  constexpr int num_runs = 100;
  for (int k = 0; k < num_runs; k++) {
    const std::vector<Rectangle> set1 =
        GenerateNonConflictingRectanglesWithPacking({100, 100}, 60, random);
    const std::vector<Rectangle> set2 =
        GenerateNonConflictingRectanglesWithPacking({100, 100}, 60, random);

    const std::vector<Rectangle> diff = PavedRegionDifference(set1, set2);
    for (int i = 0; i < diff.size(); ++i) {
      for (int j = i + 1; j < diff.size(); ++j) {
        EXPECT_TRUE(diff[i].IsDisjoint(diff[j]));
      }
    }
    for (const auto& r_diff : diff) {
      for (const auto& r2 : set2) {
        EXPECT_TRUE(r_diff.IsDisjoint(r2));
      }
      IntegerValue area = 0;
      for (const auto& r1 : set1) {
        area += r_diff.IntersectArea(r1);
      }
      EXPECT_EQ(area, r_diff.Area());
    }
  }
}

TEST(GetMinimumOverlapTest, BasicTest) {
  RectangleInRange range_ret = {
      .bounding_area = {.x_min = 0, .x_max = 15, .y_min = 0, .y_max = 15},
      .x_size = 10,
      .y_size = 10};

  // Minimum intersection is when the item is in the bottom-left corner of the
  // allowed space.
  Rectangle r = {.x_min = 3, .x_max = 30, .y_min = 3, .y_max = 30};
  EXPECT_EQ(range_ret.GetMinimumIntersection(r).Area(), 7 * 7);
  EXPECT_EQ(range_ret.GetAtCorner(RectangleInRange::Corner::BOTTOM_LEFT),
            Rectangle({.x_min = 0, .x_max = 10, .y_min = 0, .y_max = 10}));
  EXPECT_EQ(range_ret.GetAtCorner(RectangleInRange::Corner::BOTTOM_LEFT)
                .Intersect(r)
                .Area(),
            7 * 7);
  EXPECT_EQ(r.Intersect(
                Rectangle({.x_min = 0, .x_max = 10, .y_min = 0, .y_max = 10})),
            Rectangle({.x_min = 3, .x_max = 10, .y_min = 3, .y_max = 10}));

  RectangleInRange bigger =
      RectangleInRange::BiggestWithMinIntersection(r, range_ret, 7, 7);
  // This should be a broader range but don't increase the minimum intersection.
  EXPECT_EQ(bigger.GetMinimumIntersection(r).Area(), 7 * 7);
  for (const auto& pos :
       {RectangleInRange::Corner::BOTTOM_LEFT,
        RectangleInRange::Corner::TOP_LEFT, RectangleInRange::Corner::TOP_RIGHT,
        RectangleInRange::Corner::BOTTOM_RIGHT}) {
    EXPECT_EQ(bigger.GetAtCorner(pos).Intersect(r).Area(), 7 * 7);
  }
  EXPECT_EQ(bigger.bounding_area.x_min, 0);
  EXPECT_EQ(bigger.bounding_area.x_max, 33);
  EXPECT_EQ(bigger.bounding_area.y_min, 0);
  EXPECT_EQ(bigger.bounding_area.y_max, 33);
  EXPECT_EQ(r.Intersect(Rectangle(
                {.x_min = 23, .x_max = 33, .y_min = 23, .y_max = 33})),
            Rectangle({.x_min = 23, .x_max = 30, .y_min = 23, .y_max = 30}));

  RectangleInRange range_ret2 = {
      .bounding_area = {.x_min = 0, .x_max = 105, .y_min = 0, .y_max = 120},
      .x_size = 100,
      .y_size = 100};
  Rectangle r2 = {.x_min = 2, .x_max = 4, .y_min = 0, .y_max = 99};
  EXPECT_EQ(range_ret2.GetMinimumIntersection(r2), Rectangle::GetEmpty());
}

IntegerValue RecomputeEnergy(const Rectangle& rectangle,
                             const std::vector<RectangleInRange>& intervals) {
  IntegerValue ret = 0;
  for (const RectangleInRange& range : intervals) {
    const Rectangle min_intersect = range.GetMinimumIntersection(rectangle);
    EXPECT_LE(min_intersect.SizeX(), range.x_size);
    EXPECT_LE(min_intersect.SizeY(), range.y_size);
    ret += min_intersect.Area();
  }
  return ret;
}

IntegerValue RecomputeEnergy(const ProbingRectangle& ranges) {
  return RecomputeEnergy(ranges.GetCurrentRectangle(), ranges.Intervals());
}

void MoveAndCheck(ProbingRectangle& ranges, ProbingRectangle::Edge type) {
  EXPECT_TRUE(ranges.CanShrink(type));
  const IntegerValue expected_area =
      ranges.GetCurrentRectangle().Area() - ranges.GetShrinkDeltaArea(type);
  const IntegerValue expected_min_energy =
      ranges.GetMinimumEnergy() - ranges.GetShrinkDeltaEnergy(type);
  ranges.Shrink(type);
  EXPECT_EQ(ranges.GetMinimumEnergy(), RecomputeEnergy(ranges));
  EXPECT_EQ(ranges.GetMinimumEnergy(), expected_min_energy);
  EXPECT_EQ(ranges.GetCurrentRectangle().Area(), expected_area);
  ranges.ValidateInvariants();
}

TEST(ProbingRectangleTest, BasicTest) {
  RectangleInRange range_ret = {
      .bounding_area = {.x_min = 0, .x_max = 15, .y_min = 0, .y_max = 13},
      .x_size = 10,
      .y_size = 8};
  RectangleInRange range_ret2 = {
      .bounding_area = {.x_min = 1, .x_max = 8, .y_min = 7, .y_max = 14},
      .x_size = 5,
      .y_size = 5};

  std::vector<RectangleInRange> ranges_vec = {range_ret, range_ret2};
  ProbingRectangle ranges(ranges_vec);
  EXPECT_EQ(ranges.GetCurrentRectangle(),
            Rectangle({.x_min = 0, .x_max = 15, .y_min = 0, .y_max = 14}));

  // Start with the full bounding box, thus both are fully inside.
  EXPECT_EQ(ranges.GetMinimumEnergy(), 10 * 8 + 5 * 5);

  EXPECT_EQ(ranges.GetMinimumEnergy(), RecomputeEnergy(ranges));

  MoveAndCheck(ranges, ProbingRectangle::Edge::LEFT);
  EXPECT_EQ(ranges.GetCurrentRectangle(),
            Rectangle({.x_min = 1, .x_max = 15, .y_min = 0, .y_max = 14}));

  MoveAndCheck(ranges, ProbingRectangle::Edge::LEFT);
  EXPECT_EQ(ranges.GetCurrentRectangle(),
            Rectangle({.x_min = 3, .x_max = 15, .y_min = 0, .y_max = 14}));

  MoveAndCheck(ranges, ProbingRectangle::Edge::LEFT);
  EXPECT_EQ(ranges.GetCurrentRectangle(),
            Rectangle({.x_min = 5, .x_max = 15, .y_min = 0, .y_max = 14}));

  MoveAndCheck(ranges, ProbingRectangle::Edge::LEFT);
  EXPECT_EQ(ranges.GetCurrentRectangle(),
            Rectangle({.x_min = 6, .x_max = 15, .y_min = 0, .y_max = 14}));

  MoveAndCheck(ranges, ProbingRectangle::Edge::TOP);
  EXPECT_EQ(ranges.GetCurrentRectangle(),
            Rectangle({.x_min = 6, .x_max = 15, .y_min = 0, .y_max = 13}));

  MoveAndCheck(ranges, ProbingRectangle::Edge::TOP);
  EXPECT_EQ(ranges.GetCurrentRectangle(),
            Rectangle({.x_min = 6, .x_max = 15, .y_min = 0, .y_max = 8}));

  MoveAndCheck(ranges, ProbingRectangle::Edge::TOP);
  EXPECT_EQ(ranges.GetCurrentRectangle(),
            Rectangle({.x_min = 6, .x_max = 15, .y_min = 0, .y_max = 5}));
}

void ReduceUntilDone(ProbingRectangle& ranges, absl::BitGen& random) {
  static constexpr ProbingRectangle::Edge kAllEdgesArr[] = {
      ProbingRectangle::Edge::LEFT,
      ProbingRectangle::Edge::TOP,
      ProbingRectangle::Edge::RIGHT,
      ProbingRectangle::Edge::BOTTOM,
  };
  static constexpr absl::Span<const ProbingRectangle::Edge> kAllMoveTypes(
      kAllEdgesArr);
  while (!ranges.IsMinimal()) {
    ProbingRectangle::Edge type =
        kAllMoveTypes.at(absl::Uniform(random, 0, (int)kAllMoveTypes.size()));
    if (!ranges.CanShrink(type)) continue;
    MoveAndCheck(ranges, type);
  }
}

// This function will find the conflicts for rectangles that have as coordinates
// for the edges one of {min, min + size, max - size, max} for every possible
// item that is at least partially inside the rectangle. Note that we might not
// detect a conflict even if there is one by looking only at those rectangles,
// see the ProbingRectangleTest.CounterExample unit test for a concrete example.
std::optional<Rectangle> FindRectangleWithEnergyTooLargeExhaustive(
    const std::vector<RectangleInRange>& box_ranges) {
  int num_boxes = box_ranges.size();
  std::vector<IntegerValue> x;
  x.reserve(num_boxes * 4);
  std::vector<IntegerValue> y;
  y.reserve(num_boxes * 4);
  for (const auto& box : box_ranges) {
    x.push_back(box.bounding_area.x_min);
    x.push_back(box.bounding_area.x_min + box.x_size);
    x.push_back(box.bounding_area.x_max - box.x_size);
    x.push_back(box.bounding_area.x_max);
    y.push_back(box.bounding_area.y_min);
    y.push_back(box.bounding_area.y_min + box.y_size);
    y.push_back(box.bounding_area.y_max - box.y_size);
    y.push_back(box.bounding_area.y_max);
  }
  std::sort(x.begin(), x.end());
  std::sort(y.begin(), y.end());
  x.erase(std::unique(x.begin(), x.end()), x.end());
  y.erase(std::unique(y.begin(), y.end()), y.end());
  for (int i = 0; i < x.size(); ++i) {
    for (int j = i + 1; j < x.size(); ++j) {
      for (int k = 0; k < y.size(); ++k) {
        for (int l = k + 1; l < y.size(); ++l) {
          IntegerValue used_energy = 0;
          Rectangle rect = {
              .x_min = x[i], .x_max = x[j], .y_min = y[k], .y_max = y[l]};
          for (const auto& box : box_ranges) {
            auto intersection = box.GetMinimumIntersection(rect);
            used_energy += intersection.Area();
          }
          if (used_energy > rect.Area()) {
            std::vector<RectangleInRange> items_inside;
            for (const auto& box : box_ranges) {
              if (box.GetMinimumIntersectionArea(rect) > 0) {
                items_inside.push_back(box);
              }
            }
            if (items_inside.size() == num_boxes) {
              return rect;
            } else {
              // Call it again after removing items that are outside.
              auto try2 =
                  FindRectangleWithEnergyTooLargeExhaustive(items_inside);
              if (try2.has_value()) {
                return try2;
              }
            }
          }
        }
      }
    }
  }
  return std::nullopt;
}

// This function should give exactly the same result as the
// `FindRectangleWithEnergyTooLargeExhaustive` above, but exercising the
// `ProbingRectangle` class.
std::optional<Rectangle> FindRectangleWithEnergyTooLargeWithProbingRectangle(
    std::vector<RectangleInRange>& box_ranges) {
  int left_shrinks = 0;
  int right_shrinks = 0;
  int top_shrinks = 0;

  ProbingRectangle ranges(box_ranges);

  while (true) {
    // We want to do the equivalent of what
    // `FindRectangleWithEnergyTooLargeExhaustive` does: for every
    // left/right/top coordinates, try all possible bottom for conflicts. But
    // since we cannot fix the coordinates with ProbingRectangle, we fix the
    // number of shrinks instead.
    ranges.Reset();
    for (int i = 0; i < left_shrinks; i++) {
      CHECK(ranges.CanShrink(ProbingRectangle::Edge::LEFT));
      ranges.Shrink(ProbingRectangle::Edge::LEFT);
    }
    const bool left_end = !ranges.CanShrink(ProbingRectangle::Edge::LEFT);
    for (int i = 0; i < top_shrinks; i++) {
      CHECK(ranges.CanShrink(ProbingRectangle::Edge::TOP));
      ranges.Shrink(ProbingRectangle::Edge::TOP);
    }
    const bool top_end = !ranges.CanShrink(ProbingRectangle::Edge::TOP);
    for (int i = 0; i < right_shrinks; i++) {
      CHECK(ranges.CanShrink(ProbingRectangle::Edge::RIGHT));
      ranges.Shrink(ProbingRectangle::Edge::RIGHT);
    }
    const bool right_end = !ranges.CanShrink(ProbingRectangle::Edge::RIGHT);
    if (ranges.GetMinimumEnergy() > ranges.GetCurrentRectangleArea()) {
      return ranges.GetCurrentRectangle();
    }
    while (ranges.CanShrink(ProbingRectangle::Edge::BOTTOM)) {
      ranges.Shrink(ProbingRectangle::Edge::BOTTOM);
      if (ranges.GetMinimumEnergy() > ranges.GetCurrentRectangleArea()) {
        return ranges.GetCurrentRectangle();
      }
    }
    if (!right_end) {
      right_shrinks++;
    } else if (!top_end) {
      top_shrinks++;
      right_shrinks = 0;
    } else if (!left_end) {
      left_shrinks++;
      top_shrinks = 0;
      right_shrinks = 0;
    } else {
      break;
    }
  }
  return std::nullopt;
}

TEST(ProbingRectangleTest, Random) {
  absl::BitGen random;
  const int64_t size = 20;
  std::vector<RectangleInRange> rectangles;
  int count = 0;
  int comprehensive_count = 0;
  constexpr int num_runs = 400;
  for (int k = 0; k < num_runs; k++) {
    const int num_intervals = absl::Uniform(random, 1, 20);
    IntegerValue total_area = 0;
    rectangles.clear();
    for (int i = 0; i < num_intervals; ++i) {
      RectangleInRange& range = rectangles.emplace_back();
      range.bounding_area.x_min = IntegerValue(absl::Uniform(random, 0, size));
      range.bounding_area.x_max = IntegerValue(
          absl::Uniform(random, range.bounding_area.x_min.value() + 1, size));
      range.x_size = absl::Uniform(random, 1,
                                   range.bounding_area.x_max.value() -
                                       range.bounding_area.x_min.value());

      range.bounding_area.y_min = IntegerValue(absl::Uniform(random, 0, size));
      range.bounding_area.y_max = IntegerValue(
          absl::Uniform(random, range.bounding_area.y_min.value() + 1, size));
      range.y_size = absl::Uniform(random, 1,
                                   range.bounding_area.y_max.value() -
                                       range.bounding_area.y_min.value());
      total_area += range.x_size * range.y_size;
    }
    auto ret = FindRectanglesWithEnergyConflictMC(rectangles, random, 1.0, 0.8);
    count += !ret.conflicts.empty();
    ProbingRectangle ranges(rectangles);
    EXPECT_EQ(total_area, ranges.GetMinimumEnergy());
    const bool has_possible_conflict =
        FindRectangleWithEnergyTooLargeExhaustive(rectangles).has_value();
    if (has_possible_conflict) {
      EXPECT_TRUE(
          FindRectangleWithEnergyTooLargeWithProbingRectangle(rectangles)
              .has_value());
    }
    ReduceUntilDone(ranges, random);
    comprehensive_count += has_possible_conflict;
  }
  LOG(INFO) << count << "/" << num_runs << " had an heuristic (out of "
            << comprehensive_count << " possible).";
}

// Counterexample for proposition 5.4 of Clautiaux, François, et al. "A new
// constraint programming approach for the orthogonal packing problem."
// Computers & Operations Research 35.3 (2008): 944-959.
TEST(ProbingRectangleTest, CounterExample) {
  const std::vector<RectangleInRange> rectangles = {
      {.bounding_area = {.x_min = 6, .x_max = 10, .y_min = 11, .y_max = 16},
       .x_size = 3,
       .y_size = 2},
      {.bounding_area = {.x_min = 5, .x_max = 17, .y_min = 12, .y_max = 13},
       .x_size = 2,
       .y_size = 1},
      {.bounding_area = {.x_min = 15, .x_max = 18, .y_min = 11, .y_max = 14},
       .x_size = 1,
       .y_size = 1},
      {.bounding_area = {.x_min = 4, .x_max = 14, .y_min = 4, .y_max = 19},
       .x_size = 8,
       .y_size = 7},
      {.bounding_area = {.x_min = 0, .x_max = 16, .y_min = 5, .y_max = 18},
       .x_size = 8,
       .y_size = 9},
      {.bounding_area = {.x_min = 4, .x_max = 14, .y_min = 12, .y_max = 16},
       .x_size = 5,
       .y_size = 1},
      {.bounding_area = {.x_min = 1, .x_max = 16, .y_min = 12, .y_max = 18},
       .x_size = 6,
       .y_size = 1},
      {.bounding_area = {.x_min = 5, .x_max = 19, .y_min = 14, .y_max = 15},
       .x_size = 2,
       .y_size = 1}};
  const Rectangle rect = {.x_min = 6, .x_max = 10, .y_min = 7, .y_max = 16};
  // The only other possible rectangle with a conflict is x(7..9), y(7..16),
  // but none of {y_min, y_min + y_size, y_max - y_size, y_max} is equal to 7.
  const IntegerValue energy = RecomputeEnergy(rect, rectangles);
  EXPECT_GT(energy, rect.Area());
  EXPECT_FALSE(
      FindRectangleWithEnergyTooLargeExhaustive(rectangles).has_value());
}

void BM_FindRectangles(benchmark::State& state) {
  absl::BitGen random;
  std::vector<std::vector<RectangleInRange>> problems;
  static constexpr int kNumProblems = 20;
  for (int i = 0; i < kNumProblems; i++) {
    problems.push_back(MakeItemsFromRectangles(
        GenerateNonConflictingRectangles(state.range(0), random),
        state.range(1) / 100.0, random));
  }
  int idx = 0;
  for (auto s : state) {
    CHECK(FindRectanglesWithEnergyConflictMC(problems[idx], random, 1.0, 0.8)
              .conflicts.empty());
    ++idx;
    if (idx == kNumProblems) idx = 0;
  }
}

BENCHMARK(BM_FindRectangles)
    ->ArgPair(5, 1)
    ->ArgPair(10, 1)
    ->ArgPair(20, 1)
    ->ArgPair(30, 1)
    ->ArgPair(40, 1)
    ->ArgPair(80, 1)
    ->ArgPair(100, 1)
    ->ArgPair(200, 1)
    ->ArgPair(1000, 1)
    ->ArgPair(10000, 1)
    ->ArgPair(5, 100)
    ->ArgPair(10, 100)
    ->ArgPair(20, 100)
    ->ArgPair(30, 100)
    ->ArgPair(40, 100)
    ->ArgPair(80, 100)
    ->ArgPair(100, 100)
    ->ArgPair(200, 100)
    ->ArgPair(1000, 100)
    ->ArgPair(10000, 100);

TEST(FindPairwiseRestrictionsTest, Random) {
  absl::BitGen random;
  constexpr int num_runs = 400;
  for (int k = 0; k < num_runs; k++) {
    const int num_rectangles = absl::Uniform(random, 1, 20);
    const std::vector<Rectangle> rectangles =
        GenerateNonConflictingRectangles(num_rectangles, random);
    const std::vector<ItemForPairwiseRestriction> items =
        GenerateItemsRectanglesWithNoPairwiseConflict(
            rectangles, absl::Uniform(random, 0, 1.0), random);
    std::vector<PairwiseRestriction> results;
    AppendPairwiseRestrictions(items, &results);
    for (const PairwiseRestriction& result : results) {
      EXPECT_NE(result.type,
                PairwiseRestriction::PairwiseRestrictionType::CONFLICT);
    }
  }
}

void BM_FindPairwiseRestrictions(benchmark::State& state) {
  absl::BitGen random;
  // In the vast majority of the cases the propagator doesn't find any pairwise
  // condition to propagate. Thus we choose to benchmark for this particular
  // case.
  const std::vector<ItemForPairwiseRestriction> items =
      GenerateItemsRectanglesWithNoPairwisePropagation(
          state.range(0), state.range(1) / 100.0, random);
  std::vector<PairwiseRestriction> results;
  for (auto s : state) {
    AppendPairwiseRestrictions(items, &results);
    CHECK(results.empty());
  }
}

BENCHMARK(BM_FindPairwiseRestrictions)
    ->ArgPair(5, 1)
    ->ArgPair(10, 1)
    ->ArgPair(20, 1)
    ->ArgPair(30, 1)
    ->ArgPair(40, 1)
    ->ArgPair(80, 1)
    ->ArgPair(100, 1)
    ->ArgPair(200, 1)
    ->ArgPair(1000, 1)
    ->ArgPair(10000, 1)
    ->ArgPair(5, 100)
    ->ArgPair(10, 100)
    ->ArgPair(20, 100)
    ->ArgPair(30, 100)
    ->ArgPair(40, 100)
    ->ArgPair(80, 100)
    ->ArgPair(100, 100)
    ->ArgPair(200, 100)
    ->ArgPair(1000, 100)
    ->ArgPair(10000, 100);

}  // namespace
}  // namespace sat
}  // namespace operations_research
