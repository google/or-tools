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

#ifndef OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_FILTER_COMMITTABLES_H_
#define OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_FILTER_COMMITTABLES_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/util/bitset.h"
#include "ortools/util/saturated_arithmetic.h"

namespace operations_research {

// A vector that allows to revert back to a previously committed state,
// get the set of changed indices, and get current and committed values.
template <typename T>
class CommittableValue {
 public:
  explicit CommittableValue(const T& value)
      : current_(value), committed_(value) {}

  const T& Get() const { return current_; }
  const T& GetCommitted() const { return committed_; }

  void Set(const T& value) { current_ = value; }

  void SetAndCommit(const T& value) {
    Set(value);
    Commit();
  }

  void Revert() { current_ = committed_; }

  void Commit() { committed_ = current_; }

 private:
  T current_;
  T committed_;
};

template <typename T>
class CommittableVector {
 public:
  // Makes a vector with initial elements all committed to value.
  CommittableVector<T>(size_t num_elements, const T& value)
      : elements_(num_elements, {value, value}), changed_(num_elements) {}

  // Return the size of the vector.
  size_t Size() const { return elements_.size(); }

  // Returns a copy of the value stored at index in the current state.
  // Does not return a reference, because the class needs to know when elements
  // are modified.
  T Get(size_t index) const {
    DCHECK_LT(index, elements_.size());
    return elements_[index].current;
  }

  // Set the value stored at index in the current state to given value.
  void Set(size_t index, const T& value) {
    DCHECK_GE(index, 0);
    DCHECK_LT(index, elements_.size());
    changed_.Set(index);
    elements_[index].current = value;
  }

  // Changes the values of the vector to those in the last Commit().
  void Revert() {
    for (const size_t index : changed_.PositionsSetAtLeastOnce()) {
      elements_[index].current = elements_[index].committed;
    }
    changed_.ClearAll();
  }

  // Makes the current state committed, clearing all changes.
  void Commit() {
    for (const size_t index : changed_.PositionsSetAtLeastOnce()) {
      elements_[index].committed = elements_[index].current;
    }
    changed_.ClearAll();
  }

  // Sets all elements of this vector to given value, and commits to this state.
  // Supposes that there are no changes since the last Commit() or Revert().
  void SetAllAndCommit(const T& value) {
    DCHECK_EQ(0, changed_.NumberOfSetCallsWithDifferentArguments());
    elements_.assign(elements_.size(), {value, value});
  }

  // Returns a copy of the value stored at index in the last committed state.
  T GetCommitted(size_t index) const {
    DCHECK_LT(index, elements_.size());
    return elements_[index].committed;
  }

  // Return true iff the value at index has been Set() since the last Commit()
  // or Revert(), even if the current value is the same as the committed value.
  bool HasChanged(size_t index) const { return changed_[index]; }

  // Returns the set of indices that have been Set() since the last Commit() or
  // Revert().
  const std::vector<size_t>& ChangedIndices() const {
    return changed_.PositionsSetAtLeastOnce();
  }

  // TODO(user): NotifyReverted(), to tell the class that the changes
  // have brought the vector back to the committed state. This allows O(1)
  // Revert(), Commit() and empty changed indices.

 private:
  struct VersionedElement {
    T current;
    T committed;
  };
  // Holds current and committed versions of values of this vector.
  std::vector<VersionedElement> elements_;
  // Holds indices that were Set() since the last Commit() or Revert().
  SparseBitset<size_t> changed_;
};

// This class allows to represent a state of dimension values for all paths of
// a vehicle routing problem. Values of interest for each path are:
// - nodes,
// - cumuls (min/max),
// - transit times,
// - sum of transit times since the beginning of the path,
// - span (min/max).
//
// This class can maintain two states at once: a committed state and a current
// state. The current state can be modified by first describing a path p to be
// modified with PushNode() and MakePathFromNewNodes(). Then the dimension
// values of this path can be modified with views returned by MutableXXX()
// methods.
//
// When a set of paths has been modified, the caller can decide to definitely
// change the committed state to the new state, or to revert to the committed
// state.
//
// Operations are meant to be efficient:
// - all path modifications, i.e. PushNode(), MakePathFromNewNodes(),
//   MutableXXX(), MutableSpan() operations are O(1).
// - Revert() is O(num changed paths).
// - Commit() has two behaviors:
//   - if there are less than max_num_committed_elements_ elements in the
//     committed state, then Commit() is O(num changed paths).
//   - otherwise, Commit() does a compaction of the committed state, in
//     O(num_nodes + num_paths).
//   The amortized cost of Commit(), when taking modifications into account,
//   is O(size of changed paths), because all modifications pay at worst
//   O(1) for its own compaction.
//
// Note that this class does not support the semantics associated with its
// fields names, for instance it does not make sure that cumul_min <= cumul_max.
// The field names are meant for readability for the user.
// However, path sizes are enforced: if a path has n nodes, then it has
// n fields for cumul min/max, n for transit_sums, and max(0, n-1) for transits.
class DimensionValues {
 public:
  DimensionValues(int num_paths, int num_nodes)
      : range_of_path_(num_paths, {.begin = 0, .end = 0}),
        committed_range_of_path_(num_paths, {.begin = 0, .end = 0}),
        span_(num_paths, Interval::AllIntegers()),
        committed_span_(num_paths, Interval::AllIntegers()),
        vehicle_breaks_(num_paths),
        committed_vehicle_breaks_(num_paths),
        changed_paths_(num_paths),
        max_num_committed_elements_(16 * num_nodes) {
    nodes_.reserve(max_num_committed_elements_);
    transit_.reserve(max_num_committed_elements_);
    travel_.reserve(max_num_committed_elements_);
    travel_sum_.reserve(max_num_committed_elements_);
    cumul_.reserve(max_num_committed_elements_);
  }

  struct Interval {
    int64_t min;
    int64_t max;
    // Tests inequality between intervals.
    bool operator!=(const Interval& other) const {
      return min != other.min || max != other.max;
    }
    // Tests equality between intervals.
    bool operator==(const Interval& other) const {
      return min == other.min && max == other.max;
    }
    // Returns true iff the interval is empty.
    bool IsEmpty() const { return min > max; }
    // Increases the min to be at least lower_bound,
    // returns true iff the interval is nonempty.
    bool IncreaseMin(int64_t lower_bound) {
      min = std::max(min, lower_bound);
      return min <= max;
    }
    // Decreases the max to be at most upper_bound,
    // returns true iff the interval is nonempty.
    bool DecreaseMax(int64_t upper_bound) {
      max = std::min(max, upper_bound);
      return min <= max;
    }
    // Intersects this interval with the other, returns true iff the interval
    // is nonempty.
    bool IntersectWith(const Interval& other) {
      min = std::max(min, other.min);
      max = std::min(max, other.max);
      return min <= max;
    }
    // A set addition, with intervals: adds other.min to the min, other.max to
    // the max, with CapAdd().
    void Add(const Interval& other) {
      DCHECK(!IsEmpty());
      DCHECK(!other.IsEmpty());
      min = CapAdd(min, other.min);
      max = CapAdd(max, other.max);
    }
    // A set subtraction, with intervals: subtracts other.max from the min,
    // other.min from the max, with CapSub().
    void Subtract(const Interval& other) {
      DCHECK(!IsEmpty());
      DCHECK(!other.IsEmpty());
      min = CapSub(min, other.max);
      max = CapSub(max, other.min);
    }
    // Returns an interval containing all integers: {kint64min, kint64max}.
    static Interval AllIntegers() {
      return {.min = std::numeric_limits<int64_t>::min(),
              .max = std::numeric_limits<int64_t>::max()};
    }
  };

  struct VehicleBreak {
    Interval start;
    Interval end;
    Interval duration;
    Interval is_performed;
    bool operator==(const VehicleBreak& other) const {
      return start == other.start && end == other.end &&
             duration == other.duration && is_performed == other.is_performed;
    }
  };

  // Adds a node to new nodes.
  void PushNode(int node) { nodes_.push_back(node); }

  // Turns new nodes into a new path, allocating dimension values for it.
  void MakePathFromNewNodes(int path) {
    DCHECK_GE(path, 0);
    DCHECK_LT(path, range_of_path_.size());
    DCHECK(!changed_paths_[path]);
    range_of_path_[path] = {.begin = num_current_elements_,
                            .end = nodes_.size()};
    changed_paths_.Set(path);
    // Allocate dimension values. We allocate n cells for all dimension values,
    // even transits, so they can all be indexed by the same range_of_path.
    transit_.resize(nodes_.size(), Interval::AllIntegers());
    travel_.resize(nodes_.size(), 0);
    travel_sum_.resize(nodes_.size(), 0);
    cumul_.resize(nodes_.size(), Interval::AllIntegers());
    num_current_elements_ = nodes_.size();
    span_[path] = Interval::AllIntegers();
  }

  // Resets all path to empty, in both committed and current state.
  void Reset() {
    const int num_paths = range_of_path_.size();
    range_of_path_.assign(num_paths, {.begin = 0, .end = 0});
    committed_range_of_path_.assign(num_paths, {.begin = 0, .end = 0});
    changed_paths_.SparseClearAll();
    num_current_elements_ = 0;
    num_committed_elements_ = 0;
    nodes_.clear();
    transit_.clear();
    travel_.clear();
    travel_sum_.clear();
    cumul_.clear();
    committed_span_.assign(num_paths, Interval::AllIntegers());
  }

  // Clears the changed state, make it point to the committed state.
  void Revert() {
    for (const int path : changed_paths_.PositionsSetAtLeastOnce()) {
      range_of_path_[path] = committed_range_of_path_[path];
    }
    changed_paths_.SparseClearAll();
    num_current_elements_ = num_committed_elements_;
    nodes_.resize(num_current_elements_);
    transit_.resize(num_current_elements_);
    travel_.resize(num_current_elements_);
    travel_sum_.resize(num_current_elements_);
    cumul_.resize(num_current_elements_);
  }

  // Makes the committed state point to the current state.
  // If the state representation is too large, reclaims memory by compacting
  // the committed state.
  void Commit() {
    for (const int path : changed_paths_.PositionsSetAtLeastOnce()) {
      committed_range_of_path_[path] = range_of_path_[path];
      committed_span_[path] = span_[path];
      committed_vehicle_breaks_[path] = vehicle_breaks_[path];
    }
    changed_paths_.SparseClearAll();
    num_committed_elements_ = num_current_elements_;
    // If the committed data would take too much space, compact the data:
    // copy committed data to the end of vectors, erase old data, refresh
    // indexing (range_of_path_).
    if (num_current_elements_ <= max_num_committed_elements_) return;
    temp_nodes_.clear();
    temp_transit_.clear();
    temp_travel_.clear();
    temp_travel_sum_.clear();
    temp_cumul_.clear();
    for (int path = 0; path < range_of_path_.size(); ++path) {
      if (committed_range_of_path_[path].Size() == 0) continue;
      const size_t new_begin = temp_nodes_.size();
      const auto [begin, end] = committed_range_of_path_[path];
      temp_nodes_.insert(temp_nodes_.end(), nodes_.begin() + begin,
                         nodes_.begin() + end);
      temp_transit_.insert(temp_transit_.end(), transit_.begin() + begin,
                           transit_.begin() + end);
      temp_travel_.insert(temp_travel_.end(), travel_.begin() + begin,
                          travel_.begin() + end);
      temp_travel_sum_.insert(temp_travel_sum_.end(),
                              travel_sum_.begin() + begin,
                              travel_sum_.begin() + end);
      temp_cumul_.insert(temp_cumul_.end(), cumul_.begin() + begin,
                         cumul_.begin() + end);
      committed_range_of_path_[path] = {.begin = new_begin,
                                        .end = temp_nodes_.size()};
    }
    std::swap(nodes_, temp_nodes_);
    std::swap(transit_, temp_transit_);
    std::swap(travel_, temp_travel_);
    std::swap(travel_sum_, temp_travel_sum_);
    std::swap(cumul_, temp_cumul_);
    range_of_path_ = committed_range_of_path_;
    num_committed_elements_ = nodes_.size();
    num_current_elements_ = nodes_.size();
  }

  // Returns a const view of the nodes of the path, in the committed state.
  absl::Span<const int> CommittedNodes(int path) const {
    const auto [begin, end] = committed_range_of_path_[path];
    return absl::MakeConstSpan(nodes_.data() + begin, nodes_.data() + end);
  }

  // Returns a const view of the nodes of the path, in the current state.
  absl::Span<const int> Nodes(int path) const {
    const auto [begin, end] = range_of_path_[path];
    return absl::MakeConstSpan(nodes_.data() + begin, nodes_.data() + end);
  }

  // Returns a const view of the transits of the path, in the current state.
  absl::Span<const Interval> Transits(int path) const {
    auto [begin, end] = range_of_path_[path];
    // When the path is not empty, #transits = #nodes - 1.
    // When the path is empty, begin = end, return empty span.
    if (begin < end) --end;
    return absl::MakeConstSpan(transit_.data() + begin, transit_.data() + end);
  }

  // Returns a mutable view of the transits of the path, in the current state.
  absl::Span<Interval> MutableTransits(int path) {
    auto [begin, end] = range_of_path_[path];
    // When the path is not empty, #transits = #nodes - 1.
    // When the path is empty, begin = end, return empty span.
    if (begin < end) --end;
    return absl::MakeSpan(transit_.data() + begin, transit_.data() + end);
  }

  // Returns a const view of the travels of the path, in the current
  // state.
  absl::Span<const int64_t> Travels(int path) const {
    auto [begin, end] = range_of_path_[path];
    if (begin < end) --end;
    return absl::MakeConstSpan(travel_.data() + begin, travel_.data() + end);
  }

  // Returns a mutable view of the travels of the path, in the current
  // state.
  absl::Span<int64_t> MutableTravels(int path) {
    auto [begin, end] = range_of_path_[path];
    if (begin < end) --end;
    return absl::MakeSpan(travel_.data() + begin, travel_.data() + end);
  }

  // Returns a const view of the travel sums of the path, in the current state.
  absl::Span<const int64_t> TravelSums(int path) const {
    const auto [begin, end] = range_of_path_[path];
    return absl::MakeConstSpan(travel_sum_.data() + begin,
                               travel_sum_.data() + end);
  }

  // Returns a mutable view of the travel sums of the path in the current state.
  absl::Span<int64_t> MutableTravelSums(int path) {
    const auto [begin, end] = range_of_path_[path];
    return absl::MakeSpan(travel_sum_.data() + begin, travel_sum_.data() + end);
  }

  // Returns a const view of the cumul mins of the path, in the current state.
  absl::Span<const Interval> Cumuls(int path) const {
    const auto [begin, end] = range_of_path_[path];
    return absl::MakeConstSpan(cumul_.data() + begin, cumul_.data() + end);
  }

  // Returns a mutable view of the cumul mins of the path, in the current state.
  absl::Span<Interval> MutableCumuls(int path) {
    const auto [begin, end] = range_of_path_[path];
    return absl::MakeSpan(cumul_.data() + begin, cumul_.data() + end);
  }

  // Returns the span interval of the path, in the current state.
  Interval Span(int path) const {
    return changed_paths_[path] ? span_[path] : committed_span_[path];
  }
  // Returns a mutable view of the span of the path, in the current state.
  // The path must have been changed since the last commit.
  Interval& MutableSpan(int path) {
    DCHECK(changed_paths_[path]);
    return span_[path];
  }

  // Returns a const view of the vehicle breaks of the path, in the current
  // state.
  absl::Span<const VehicleBreak> VehicleBreaks(int path) const {
    return absl::MakeConstSpan(changed_paths_[path]
                                   ? vehicle_breaks_[path]
                                   : committed_vehicle_breaks_[path]);
  }

  // Returns a mutable vector of the vehicle breaks of the path, in the current
  // state. The path must have been changed since the last commit.
  std::vector<VehicleBreak>& MutableVehicleBreaks(int path) {
    DCHECK(changed_paths_[path]);
    return vehicle_breaks_[path];
  }

  // Returns the number of nodes of the path, in the current state.
  int NumNodes(int path) const { return range_of_path_[path].Size(); }
  // Returns a const view of the set of paths changed, in the current state.
  absl::Span<const int> ChangedPaths() const {
    return absl::MakeConstSpan(changed_paths_.PositionsSetAtLeastOnce());
  }
  // Returns whether the given path was changed, in the current state.
  bool PathHasChanged(int path) const { return changed_paths_[path]; }

 private:
  // These vectors hold the data of both committed and current states.
  // The ranges below determine which indices are associated to each path and
  // each state. It is up to the user to maintain the following invariants:
  // If range_of_path_[p] == {.begin = b, .end = e}, then, in the current
  // state:
  // - nodes_[i] for i in [b, e) are the nodes of the path p.
  // - cumul_[r] + transit_[r] == cumul_[r+1] for r in [b, e-1).
  // - travel_[r] <= transit_[r].min for r in [b, e-1).
  // - travel_sum_[r] == sum_{r' in [0, r')} travel_[r'], for r in [b+1, e)
  // - cumul[b] + span_[p] == cumul[e-1].
  //
  // The same invariants should hold for committed_range_of_path_ and the
  // committed state.
  std::vector<int> nodes_;
  std::vector<Interval> transit_;
  std::vector<int64_t> travel_;
  std::vector<int64_t> travel_sum_;
  std::vector<Interval> cumul_;
  // Temporary vectors used in Commit() during compaction.
  std::vector<int> temp_nodes_;
  std::vector<Interval> temp_transit_;
  std::vector<int64_t> temp_travel_;
  std::vector<int64_t> temp_travel_sum_;
  std::vector<Interval> temp_cumul_;
  // A path has a range of indices in the committed state and another one in the
  // current state.
  struct Range {
    size_t begin = 0;
    size_t end = 0;
    int Size() const { return end - begin; }
  };
  std::vector<Range> range_of_path_;
  std::vector<Range> committed_range_of_path_;
  // Associates span to each path.
  std::vector<Interval> span_;
  std::vector<Interval> committed_span_;
  // Associates vehicle breaks with each path.
  std::vector<std::vector<VehicleBreak>> vehicle_breaks_;
  std::vector<std::vector<VehicleBreak>> committed_vehicle_breaks_;
  // Stores whether each path has been changed since last committed state.
  SparseBitset<int> changed_paths_;
  // Threshold for the size of the committed vector. This is purely heuristic:
  // it should be more than the number of nodes so compactions do not occur at
  // each submit, but ranges should not be too far apart to avoid cache misses.
  const size_t max_num_committed_elements_;
  // This locates the start of new nodes.
  size_t num_current_elements_ = 0;
  size_t num_committed_elements_ = 0;
};

}  // namespace operations_research

#endif  // OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_FILTER_COMMITTABLES_H_
