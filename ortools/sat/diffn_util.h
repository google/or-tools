// Copyright 2010-2021 Google LLC
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

#ifndef OR_TOOLS_SAT_DIFFN_UTIL_H_
#define OR_TOOLS_SAT_DIFFN_UTIL_H_

#include <cstdint>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_format.h"
#include "ortools/graph/connected_components.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/intervals.h"

namespace operations_research {
namespace sat {

struct Rectangle {
  IntegerValue x_min;
  IntegerValue x_max;
  IntegerValue y_min;
  IntegerValue y_max;

  void TakeUnionWith(const Rectangle& other) {
    x_min = std::min(x_min, other.x_min);
    y_min = std::min(y_min, other.y_min);
    x_max = std::max(x_max, other.x_max);
    y_max = std::max(y_max, other.y_max);
  }

  IntegerValue Area() const { return (x_max - x_min) * (y_max - y_min); }

  bool IsDisjoint(const Rectangle& other) const;

  std::string DebugString() const {
    return absl::StrFormat("rectangle(x(%i..%i), y(%i..%i))", x_min.value(),
                           x_max.value(), y_min.value(), y_max.value());
  }
};

// Creates a graph when two nodes are connected iif their rectangles overlap.
// Then partition into connected components.
//
// This method removes all singleton components. It will modify the
// active_rectangle span in place.
std::vector<absl::Span<int>> GetOverlappingRectangleComponents(
    const std::vector<Rectangle>& rectangles,
    absl::Span<int> active_rectangles);

// Visible for testing. The algo is in O(n^4) so shouldn't be used directly.
// Returns true if there exist a bounding box with too much energy.
bool BoxesAreInEnergyConflict(const std::vector<Rectangle>& rectangles,
                              const std::vector<IntegerValue>& energies,
                              absl::Span<const int> boxes,
                              Rectangle* conflict = nullptr);

// Checks that there is indeed a conflict for the given bounding_box and
// report it. This returns false for convenience as we usually want to return
// false on a conflict.
//
// TODO(user): relax the bounding box dimension to have a relaxed explanation.
// We can also minimize the number of required intervals.
bool ReportEnergyConflict(Rectangle bounding_box, absl::Span<const int> boxes,
                          SchedulingConstraintHelper* x,
                          SchedulingConstraintHelper* y);

// A O(n^2) algorithm to analyze all the relevant X intervals and infer a
// threshold of the y size of a bounding box after which there is no point
// checking for energy overload.
//
// Returns false on conflict, and fill the bounding box that caused the
// conflict.
//
// If transpose is true, we analyze the relevant Y intervals instead.
bool AnalyzeIntervals(bool transpose, absl::Span<const int> boxes,
                      const std::vector<Rectangle>& rectangles,
                      const std::vector<IntegerValue>& rectangle_energies,
                      IntegerValue* x_threshold, IntegerValue* y_threshold,
                      Rectangle* conflict = nullptr);

// Removes boxes with a size above the thresholds. Also randomize the order.
// Because we rely on various heuristic, this allow to change the order from
// one call to the next.
absl::Span<int> FilterBoxesAndRandomize(
    const std::vector<Rectangle>& cached_rectangles, absl::Span<int> boxes,
    IntegerValue threshold_x, IntegerValue threshold_y, absl::BitGenRef random);

// Given the total energy of all rectangles (sum of energies[box]) we know that
// any box with an area greater than that cannot participate in any "bounding
// box" conflict. As we remove this box, the total energy decrease, so we might
// remove more. This works in O(n log n).
absl::Span<int> FilterBoxesThatAreTooLarge(
    const std::vector<Rectangle>& cached_rectangles,
    const std::vector<IntegerValue>& energies, absl::Span<int> boxes);

struct IndexedInterval {
  int index;
  IntegerValue start;
  IntegerValue end;

  bool operator==(const IndexedInterval& rhs) const {
    return std::tie(start, end, index) ==
           std::tie(rhs.start, rhs.end, rhs.index);
  }

  // NOTE(user): We would like to use TUPLE_DEFINE_STRUCT, but sadly it doesn't
  // support //buildenv/target:non_prod.
  struct ComparatorByStartThenEndThenIndex {
    bool operator()(const IndexedInterval& a, const IndexedInterval& b) const {
      return std::tie(a.start, a.end, a.index) <
             std::tie(b.start, b.end, b.index);
    }
  };
  struct ComparatorByStart {
    bool operator()(const IndexedInterval& a, const IndexedInterval& b) const {
      return a.start < b.start;
    }
  };
};
std::ostream& operator<<(std::ostream& out, const IndexedInterval& interval);

// Given n fixed intervals, returns the subsets of intervals that overlap during
// at least one time unit. Note that we only return "maximal" subset and filter
// subset strictly included in another.
//
// All Intervals must have a positive size.
//
// The algo is in O(n log n) + O(result_size) which is usually O(n^2).
void ConstructOverlappingSets(bool already_sorted,
                              std::vector<IndexedInterval>* intervals,
                              std::vector<std::vector<int>>* result);

// Given n intervals, returns the set of connected components (using the overlap
// relation between 2 intervals). Components are sorted by their start, and
// inside a component, the intervals are also sorted by start.
// `intervals` is only sorted (by start), and not modified otherwise.
void GetOverlappingIntervalComponents(
    std::vector<IndexedInterval>* intervals,
    std::vector<std::vector<int>>* components);

// Similar to GetOverlappingIntervalComponents(), but returns the indices of
// all intervals whose removal would create one more connected component in the
// interval graph. Those are sorted by start. See:
// https://en.wikipedia.org/wiki/Glossary_of_graph_theory#articulation_point.
std::vector<int> GetIntervalArticulationPoints(
    std::vector<IndexedInterval>* intervals);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_DIFFN_UTIL_H_
