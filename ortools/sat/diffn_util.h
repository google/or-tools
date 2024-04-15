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

#ifndef OR_TOOLS_SAT_DIFFN_UTIL_H_
#define OR_TOOLS_SAT_DIFFN_UTIL_H_

#include <algorithm>
#include <iosfwd>
#include <limits>
#include <optional>
#include <ostream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/intervals.h"
#include "ortools/util/strong_integers.h"

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

  IntegerValue Area() const { return SizeX() * SizeY(); }

  IntegerValue SizeX() const { return x_max - x_min; }
  IntegerValue SizeY() const { return y_max - y_min; }

  bool IsDisjoint(const Rectangle& other) const;

  // Returns an empty rectangle if no intersection.
  Rectangle Intersect(const Rectangle& other) const;
  IntegerValue IntersectArea(const Rectangle& other) const;

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const Rectangle& r) {
    absl::Format(&sink, "rectangle(x(%i..%i), y(%i..%i))", r.x_min.value(),
                 r.x_max.value(), r.y_min.value(), r.y_max.value());
  }

  bool operator==(const Rectangle& other) const {
    return std::tie(x_min, x_max, y_min, y_max) ==
           std::tie(other.x_min, other.x_max, other.y_min, other.y_max);
  }

  static Rectangle GetEmpty() {
    return Rectangle{.x_min = IntegerValue(0),
                     .x_max = IntegerValue(0),
                     .y_min = IntegerValue(0),
                     .y_max = IntegerValue(0)};
  }
};

inline Rectangle Rectangle::Intersect(const Rectangle& other) const {
  const IntegerValue ret_x_min = std::max(x_min, other.x_min);
  const IntegerValue ret_y_min = std::max(y_min, other.y_min);
  const IntegerValue ret_x_max = std::min(x_max, other.x_max);
  const IntegerValue ret_y_max = std::min(y_max, other.y_max);

  if (ret_x_min >= ret_x_max || ret_y_min >= ret_y_max) {
    return GetEmpty();
  } else {
    return Rectangle{.x_min = ret_x_min,
                     .x_max = ret_x_max,
                     .y_min = ret_y_min,
                     .y_max = ret_y_max};
  }
}

inline IntegerValue Rectangle::IntersectArea(const Rectangle& other) const {
  const IntegerValue ret_x_min = std::max(x_min, other.x_min);
  const IntegerValue ret_y_min = std::max(y_min, other.y_min);
  const IntegerValue ret_x_max = std::min(x_max, other.x_max);
  const IntegerValue ret_y_max = std::min(y_max, other.y_max);

  if (ret_x_min >= ret_x_max || ret_y_min >= ret_y_max) {
    return 0;
  } else {
    return (ret_x_max - ret_x_min) * (ret_y_max - ret_y_min);
  }
}

// Creates a graph when two nodes are connected iff their rectangles overlap.
// Then partition into connected components.
//
// This method removes all singleton components. It will modify the
// active_rectangle span in place.
std::vector<absl::Span<int>> GetOverlappingRectangleComponents(
    absl::Span<const Rectangle> rectangles, absl::Span<int> active_rectangles);

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
                      absl::Span<const Rectangle> rectangles,
                      absl::Span<const IntegerValue> rectangle_energies,
                      IntegerValue* x_threshold, IntegerValue* y_threshold,
                      Rectangle* conflict = nullptr);

// Removes boxes with a size above the thresholds. Also randomize the order.
// Because we rely on various heuristic, this allow to change the order from
// one call to the next.
absl::Span<int> FilterBoxesAndRandomize(
    absl::Span<const Rectangle> cached_rectangles, absl::Span<int> boxes,
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

struct ItemForPairwiseRestriction {
  int index;
  struct Interval {
    IntegerValue start_min;
    IntegerValue start_max;
    IntegerValue end_min;
    IntegerValue end_max;
  };
  Interval x;
  Interval y;
};

struct PairwiseRestriction {
  enum class PairwiseRestrictionType {
    CONFLICT,
    FIRST_BELOW_SECOND,
    FIRST_ABOVE_SECOND,
    FIRST_LEFT_OF_SECOND,
    FIRST_RIGHT_OF_SECOND,
  };

  int first_index;
  int second_index;
  PairwiseRestrictionType type;

  bool operator==(const PairwiseRestriction& rhs) const {
    return first_index == rhs.first_index && second_index == rhs.second_index &&
           type == rhs.type;
  }
};

// Find pair of items that are either in conflict or could have their range
// shrinked to avoid conflict.
void AppendPairwiseRestrictions(
    absl::Span<const ItemForPairwiseRestriction> items,
    std::vector<PairwiseRestriction>* result);

// Same as above, but test `items` against `other_items` and append the
// restrictions found to `result`.
void AppendPairwiseRestrictions(
    absl::Span<const ItemForPairwiseRestriction> items,
    absl::Span<const ItemForPairwiseRestriction> other_items,
    std::vector<PairwiseRestriction>* result);

// This class is used by the no_overlap_2d constraint to maintain the envelope
// of a set of rectangles. This envelope is not the convex hull, but the exact
// polyline (aligned with the x and y axis) that contains all the rectangles
// passed with the AddRectangle() call.
class CapacityProfile {
 public:
  // Simple start of a rectangle. This is used to represent the residual
  // capacity profile.
  struct Rectangle {
    Rectangle(IntegerValue start, IntegerValue height)
        : start(start), height(height) {}

    bool operator<(const Rectangle& other) const { return start < other.start; }
    bool operator==(const Rectangle& other) const {
      return start == other.start && height == other.height;
    }

    IntegerValue start = IntegerValue(0);
    IntegerValue height = IntegerValue(0);
  };

  void Clear();

  // Adds a rectangle to the current shape.
  void AddRectangle(IntegerValue x_min, IntegerValue x_max, IntegerValue y_min,
                    IntegerValue y_max);

  // Adds a mandatory profile consumption. All mandatory usages will be
  // subtracted from the y_max-y_min profile to build the residual capacity.
  void AddMandatoryConsumption(IntegerValue x_min, IntegerValue x_max,
                               IntegerValue y_height);

  // Returns the profile of the function:
  // capacity(x) = max(y_max of rectangles overlapping x) - min(y_min of
  //     rectangle overlapping x) - sum(y_height of mandatory rectangles
  //     overlapping x) where a rectangle overlaps x if x_min <= x < x_max.
  //
  // Note the profile can contain negative heights in case the mandatory part
  // exceeds the range on the y axis.
  //
  // Note that it adds a sentinel (kMinIntegerValue, 0) at the start. It is
  // useful when we reverse the direction on the x axis.
  void BuildResidualCapacityProfile(std::vector<Rectangle>* result);

  // Returns the exact area of the bounding polyline of all rectangles added.
  //
  // Note that this will redo the computation each time.
  IntegerValue GetBoundingArea();

 private:
  // Type for the capacity events.
  enum EventType { START_RECTANGLE, END_RECTANGLE, CHANGE_MANDATORY_PROFILE };

  // Individual events.
  struct Event {
    IntegerValue time;
    IntegerValue y_min;
    IntegerValue y_max;
    EventType type;
    int index;

    bool operator<(const Event& other) const { return time < other.time; }
  };

  // Element of the integer_pq heap.
  struct QueueElement {
    int Index() const { return index; }
    bool operator<(const QueueElement& o) const { return value < o.value; }

    int index;
    IntegerValue value;
  };

  static Event StartRectangleEvent(int index, IntegerValue x_min,
                                   IntegerValue y_min, IntegerValue y_max) {
    return {x_min, y_min, y_max, START_RECTANGLE, index};
  }

  static Event EndRectangleEvent(int index, IntegerValue x_max) {
    return {x_max, kMinIntegerValue, kMinIntegerValue, END_RECTANGLE, index};
  }

  static Event ChangeMandatoryProfileEvent(IntegerValue x, IntegerValue delta) {
    return {x, /*y_min=*/delta, /*y_max=*/kMinIntegerValue,
            CHANGE_MANDATORY_PROFILE, /*index=*/-1};
  }

  std::vector<Event> events_;
  int num_rectangles_added_ = 0;
};

// 1D counterpart of RectangleInRange::GetMinimumIntersectionArea.
// Finds the minimum possible overlap of a interval of size `size` that fits in
// [range_min, range_max] and a second interval [interval_min, interval_max].
IntegerValue Smallest1DIntersection(IntegerValue range_min,
                                    IntegerValue range_max, IntegerValue size,
                                    IntegerValue interval_min,
                                    IntegerValue interval_max);

// A rectangle of size (`x_size`, `y_size`) that can freely move inside the
// `bounding_area` rectangle.
struct RectangleInRange {
  int box_index;
  Rectangle bounding_area;
  IntegerValue x_size;
  IntegerValue y_size;

  enum Corner {
    BOTTOM_LEFT = 0,
    TOP_LEFT = 1,
    BOTTOM_RIGHT = 2,
    TOP_RIGHT = 3,
  };

  // Returns the position of the rectangle fixed to one of the corner of its
  // range.
  Rectangle GetAtCorner(Corner p) const {
    switch (p) {
      case Corner::BOTTOM_LEFT:
        return Rectangle{.x_min = bounding_area.x_min,
                         .x_max = bounding_area.x_min + x_size,
                         .y_min = bounding_area.y_min,
                         .y_max = bounding_area.y_min + y_size};
      case Corner::TOP_LEFT:
        return Rectangle{.x_min = bounding_area.x_min,
                         .x_max = bounding_area.x_min + x_size,
                         .y_min = bounding_area.y_max - y_size,
                         .y_max = bounding_area.y_max};
      case Corner::BOTTOM_RIGHT:
        return Rectangle{.x_min = bounding_area.x_max - x_size,
                         .x_max = bounding_area.x_max,
                         .y_min = bounding_area.y_min,
                         .y_max = bounding_area.y_min + y_size};
      case Corner::TOP_RIGHT:
        return Rectangle{.x_min = bounding_area.x_max - x_size,
                         .x_max = bounding_area.x_max,
                         .y_min = bounding_area.y_max - y_size,
                         .y_max = bounding_area.y_max};
    }
  }

  // Returns an empty rectangle if it is possible for no intersection to happen.
  Rectangle GetMinimumIntersection(const Rectangle& containing_area) const {
    IntegerValue smallest_area = std::numeric_limits<IntegerValue>::max();
    Rectangle best_intersection;
    for (int corner_idx = 0; corner_idx < 4; ++corner_idx) {
      const Corner p = static_cast<Corner>(corner_idx);
      Rectangle intersection = containing_area.Intersect(GetAtCorner(p));
      const IntegerValue intersection_area = intersection.Area();
      if (intersection_area == 0) {
        return Rectangle::GetEmpty();
      }
      if (intersection_area < smallest_area) {
        smallest_area = intersection_area;
        best_intersection = std::move(intersection);
      }
    }
    return best_intersection;
  }

  IntegerValue GetMinimumIntersectionArea(
      const Rectangle& containing_area) const {
    return Smallest1DIntersection(bounding_area.x_min, bounding_area.x_max,
                                  x_size, containing_area.x_min,
                                  containing_area.x_max) *
           Smallest1DIntersection(bounding_area.y_min, bounding_area.y_max,
                                  y_size, containing_area.y_min,
                                  containing_area.y_max);
  }

  static RectangleInRange BiggestWithMinIntersection(
      const Rectangle& containing_area, const RectangleInRange& original,
      const IntegerValue& min_intersect_x,
      const IntegerValue& min_intersect_y) {
    const IntegerValue x_size = original.x_size;
    const IntegerValue y_size = original.y_size;

    RectangleInRange result;
    result.x_size = x_size;
    result.y_size = y_size;
    result.box_index = original.box_index;

    // We cannot intersect more units than the whole item.
    DCHECK_GE(x_size, min_intersect_x);
    DCHECK_GE(y_size, min_intersect_y);

    // Units that can *not* intersect per dimension.
    const IntegerValue x_headroom = x_size - min_intersect_x;
    const IntegerValue y_headroom = y_size - min_intersect_y;

    result.bounding_area.x_min = containing_area.x_min - x_headroom;
    result.bounding_area.x_max = containing_area.x_max + x_headroom;
    result.bounding_area.y_min = containing_area.y_min - y_headroom;
    result.bounding_area.y_max = containing_area.y_max + y_headroom;

    return result;
  }

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const RectangleInRange& r) {
    absl::Format(&sink, "item(size=%vx%v, BB=%v)", r.x_size, r.y_size,
                 r.bounding_area);
  }
};

// Cheaply test several increasingly smaller rectangles for energy conflict.
// More precisely, each call to `Shrink()` cost O(k + n) operations, where k is
// the number of points that shrinking the probing rectangle will cross and n is
// the number of items which are in a range that overlaps the probing rectangle
// in both sides in the dimension that is getting shrinked. When calling
// repeatedely `Shrink()` until the probing rectangle collapse into a single
// point, the O(k) component adds up to a O(M) cost, where M is the number of
// items. This means this procedure is linear in time if the ranges of the items
// are small.
//
// The energy is defined as the minimum occupied area inside the probing
// rectangle. For more details, see Clautiaux, François, et al. "A new
// constraint programming approach for the orthogonal packing problem."
// Computers & Operations Research 35.3 (2008): 944-959.
//
// This is used by FindRectanglesWithEnergyConflictMC() below.
class ProbingRectangle {
 public:
  // It will initialize with the bounding box of the whole set.
  explicit ProbingRectangle(const std::vector<RectangleInRange>& intervals);

  enum Edge { TOP = 0, LEFT = 1, BOTTOM = 2, RIGHT = 3 };

  // Reset to the bounding box of the whole set.
  void Reset();

  // Shrink the rectangle by moving one of its four edges to the next
  // "interesting" value. The interesting values for x or y are the ones that
  // correspond to a boundary, ie., a value that corresponds to one of {min,
  // min + size, max - size, max} of a rectangle.
  void Shrink(Edge edge);

  bool CanShrink(Edge edge) const;

  bool IsMinimal() const {
    // We only need to know if there is slack on both dimensions. Actually
    // CanShrink(BOTTOM) == CanShrink(TOP) and conversely.
    return !(CanShrink(Edge::BOTTOM) || CanShrink(Edge::LEFT));
  }

  // Test-only method that check that all internal incremental counts are
  // correct by comparing with recalculating them from scratch.
  void ValidateInvariants() const;

  // How much of GetMinimumEnergy() will change if Shrink() is called.
  IntegerValue GetShrinkDeltaEnergy(Edge edge) const {
    return cached_delta_energy_[edge];
  }

  // How much of GetCurrentRectangleArea() will change if Shrink() is called.
  IntegerValue GetShrinkDeltaArea(Edge edge) const;

  Rectangle GetCurrentRectangle() const;
  IntegerValue GetCurrentRectangleArea() const { return probe_area_; }

  // This is equivalent of, for every item:
  // - Call GetMinimumIntersectionArea() with GetCurrentRectangle().
  // - Return the total sum of the areas.
  IntegerValue GetMinimumEnergy() const { return minimum_energy_; }

  const std::vector<RectangleInRange>& Intervals() const { return intervals_; }

  enum Direction {
    LEFT_AND_RIGHT = 0,
    TOP_AND_BOTTOM = 1,
  };

 private:
  void CacheShrinkDeltaEnergy(int dimension);

  template <Edge edge>
  void ShrinkImpl();

  struct IntervalPoint {
    IntegerValue value;
    int index;
  };

  std::vector<IntervalPoint> interval_points_sorted_by_x_;
  std::vector<IntervalPoint> interval_points_sorted_by_y_;

  // Those two vectors are not strictly needed, we could instead iterate
  // directly on the two vectors above, but the code would be much uglier.
  struct PointsForCoordinate {
    IntegerValue coordinate;
    absl::Span<IntervalPoint> items_touching_coordinate;
  };
  std::vector<PointsForCoordinate> grouped_intervals_sorted_by_x_;
  std::vector<PointsForCoordinate> grouped_intervals_sorted_by_y_;

  const std::vector<RectangleInRange>& intervals_;

  IntegerValue full_energy_;
  IntegerValue minimum_energy_;
  IntegerValue probe_area_;
  int indexes_[4];
  int next_indexes_[4];

  absl::flat_hash_set<int> ranges_touching_both_boundaries_[2];
  IntegerValue corner_count_[4] = {0, 0, 0, 0};
  IntegerValue intersect_length_[4] = {0, 0, 0, 0};
  IntegerValue cached_delta_energy_[4];
};

// Monte-Carlo inspired heuristic to find a rectangles with an energy conflict:
// - start with a rectangle equals to the full bounding box of the elements;
// - shrink the rectangle by an edge to the next "interesting" value. Choose
//   the edge randomly, but biased towards the change that increases the ratio
//   area_inside / area_rectangle;
// - collect a result at every conflict or every time the ratio
//   used_energy/available_energy is more than `candidate_energy_usage_factor`;
// - stop when the rectangle is empty.
struct FindRectanglesResult {
  // Known conflicts: the minimal energy used inside the rectangle is larger
  // than the area of the rectangle.
  std::vector<Rectangle> conflicts;
  // Rectangles without a conflict but having used_energy/available_energy >
  // candidate_energy_usage_factor. Those are good candidates for finding
  // conflicts using more sophisticated heuristics. Those rectangles are
  // ordered so the n-th rectangle is always fully inside the n-1-th one.
  std::vector<Rectangle> candidates;
};
FindRectanglesResult FindRectanglesWithEnergyConflictMC(
    const std::vector<RectangleInRange>& intervals, absl::BitGenRef random,
    double temperature, double candidate_energy_usage_factor);

// Render a packing solution as a Graphviz dot file. Only works in the "neato"
// or "fdp" Graphviz backends.
std::string RenderDot(std::pair<IntegerValue, IntegerValue> bb_sizes,
                      absl::Span<const Rectangle> solution);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_DIFFN_UTIL_H_
