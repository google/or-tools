// Copyright 2011-2012 Claude-Guy Quimper
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// This is an implementation of :
// Alejandro López-Ortiz, Claude-Guy Quimper, John Tromp, and Peter
// van Beek. A fast and simple algorithm for bounds consistency of the
// alldifferent constraint. In Proceedings of the 18th International
// Joint Conference on Artificial Intelligence (IJCAI 03), Acapulco,
// Mexico, pages 245-250, 2003.


#include "base/integral_types.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/join.h"
#include "base/int_type_indexed_vector.h"
#include "base/int_type.h"
#include "base/map_util.h"
#include "base/stl_util.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"
#include "util/vector_map.h"
#include "base/stringprintf.h"

namespace operations_research {
namespace {
DEFINE_INT_TYPE(Index, int);

struct Interval {
  Interval() : min_value(0), max_value(0), min_rank(0), max_rank(0) {}
  int64 min_value;
  int64 max_value;  // start, end of Interval
  int64 min_rank;
  int64 max_rank;  // rank of min & max in bounds_[]
  std::string DebugString() const {
    return StringPrintf("Interval(value = [%lld, %lld], rank = [%lld, %lld])",
                        min_value, max_value, min_rank, max_rank);
  }
};

struct CompareIntervalMin {
  inline bool operator()(Interval* const i1, Interval* const i2) {
    return (i1->min_value < i2->min_value);
  }
};

struct CompareIntervalMax {
  inline bool operator()(Interval* const i1, Interval* const i2) {
    return (i1->max_value < i2->max_value);
  }
};

class PartialSum {
 public:
  // Create a partial sum data structure adapted to the
  // filterLower{Min,Max} and filterUpper{Min,Max} functions.
  // Two elements before and after the element list will be added
  // with a weight of 1.
  PartialSum(int64 offset, int64 count, const std::vector<int64>& elements)
      : offset_(offset - 3),  // We add 3 elements at the beginning.
        last_value_(offset + count + 1),
        sum_(count + 5),
        ds_(count + 5) {
    sum_[0] = 0;
    sum_[1] = 1;
    sum_[2] = 2;
    for (int64 i = 0; i < count; i++) {
      sum_[i + 3] = sum_[i + 2] + elements[i];
    }
    sum_[count + 3] = sum_[count + 2] + 1;
    sum_[count + 4] = sum_[count + 3] + 1;

    int64 i = count + 3;
    int64 j = count + 4;
    while (i > 0) {
      while (sum_[i] == sum_[i - 1]) {
        ds_[i--] = j;
      }
      ds_[j] = i--;
      j = ds_[j];
    }
    ds_[j] = 0;
  }

  PartialSum(int64 offset, int64 count, const std::vector<int>& elements)
      : offset_(offset - 3),
        last_value_(offset + count + 1),
        sum_(count + 5),
        ds_(count + 5) {
    sum_[0] = 0;
    sum_[1] = 1;
    sum_[2] = 2;
    for (int64 i = 0; i < count; i++) {
      sum_[i + 3] = sum_[i + 2] + elements[i];
    }
    sum_[count + 3] = sum_[count + 2] + 1;
    sum_[count + 4] = sum_[count + 3] + 1;

    int64 i = count + 3;
    int64 j = count + 4;
    while (i > 0) {
      while (sum_[i] == sum_[i - 1]) {
        ds_[i--] = j;
      }
      ds_[j] = i--;
      j = ds_[j];
    }
    ds_[j] = 0;
  }

  int64 MinValue() const { return offset_ + 3; }

  int64 MaxValue() const { return last_value_ - 2; }

  int64 SkipNonNullElementsRight(int64 value) {
    value -= offset_;
    return (ds_[value] < value ? value : ds_[value]) + offset_;
  }

  int64 SkipNonNullElementsLeft(int64 value) {
    value -= offset_;
    return (ds_[value] > value ? ds_[ds_[value]] : value) + offset_;
  }

  int64 Sum(int64 from, int64 to) const {
    if (from <= to) {
      DCHECK((offset_ <= from) && (to <= last_value_));
      return sum_[to - offset_] - sum_[from - 1 - offset_];
    } else {
      DCHECK((offset_ <= to) && (from <= last_value_));
      return sum_[to - 1 - offset_] - sum_[from - offset_];
    }
  }

  int64 offset() const { return offset_; }

  int64 last_value() const { return last_value_; }

  std::string DebugString() const {
    return StringPrintf(
        "PartialSum(offset=%lld, last_value = %lld, sum = %s, ds = %s)",
        offset_, last_value_, strings::Join(sum_, ", ").c_str(),
        strings::Join(ds_, ", ").c_str());
  }

 private:
  int64 offset_;
  int64 last_value_;
  std::vector<int64> sum_;
  std::vector<int64> ds_;
};

// A value "v" must be assigned to at least
// min_occurrences[v - firstDomainValue] variables and at most
// max_occurrences[v - firstDomainValue] variables
class GccConstraint : public Constraint {
 public:
  GccConstraint(Solver* const solver, const std::vector<IntVar*>& vars,
                int64 first_domain_value, int64 number_of_values,
                const std::vector<int64>& min_occurrences,
                const std::vector<int64>& max_occurrences)
      : Constraint(solver),
        variables_(vars.begin(), vars.end()),
        size_(vars.size()),
        max_occurrences_(number_of_values + 1, 0),
        first_domain_value_(first_domain_value),
        tree_(2 * size_ + 2),
        diffs_(2 * size_ + 2),
        hall_(2 * size_ + 2),
        stable_intervals_(2 * size_ + 2),
        potential_stable_sets_(2 * size_ + 2),
        new_min_(size_),
        intervals_(size_),
        sorted_by_min_(size_),
        sorted_by_max_(size_),
        bounds_(2 * size_ + 2),
        active_size_(0),
        lower_sum_(first_domain_value, number_of_values, min_occurrences),
        upper_sum_(first_domain_value, number_of_values, max_occurrences) {
    for (int64 i = 0; i < number_of_values; i++) {
      max_occurrences_.SetValue(solver, i, max_occurrences[i]);
    }

    for (Index i(0); i < size_; i++) {
      sorted_by_min_[i.value()] = &intervals_[i];
      sorted_by_max_[i.value()] = &intervals_[i];
    }
  }

  GccConstraint(Solver* const solver, const std::vector<IntVar*>& vars,
                int64 first_domain_value, int64 number_of_values,
                const std::vector<int>& min_occurrences,
                const std::vector<int>& max_occurrences)
      : Constraint(solver),
        variables_(vars.begin(), vars.end()),
        size_(vars.size()),
        max_occurrences_(number_of_values, 0),
        first_domain_value_(first_domain_value),
        tree_(2 * size_ + 2),
        diffs_(2 * size_ + 2),
        hall_(2 * size_ + 2),
        stable_intervals_(2 * size_ + 2),
        potential_stable_sets_(2 * size_ + 2),
        new_min_(size_),
        intervals_(size_),
        sorted_by_min_(size_),
        sorted_by_max_(size_),
        bounds_(2 * size_ + 2),
        active_size_(0),
        lower_sum_(first_domain_value, number_of_values, min_occurrences),
        upper_sum_(first_domain_value, number_of_values, max_occurrences) {
    for (int64 i = 0; i < number_of_values; i++) {
      max_occurrences_.SetValue(solver, i, max_occurrences[i]);
    }

    for (Index i(0); i < size_; i++) {
      sorted_by_min_[i.value()] = &intervals_[i];
      sorted_by_max_[i.value()] = &intervals_[i];
    }
  }

  virtual ~GccConstraint() {}

  virtual void Post() {
    for (Index i(0); i < size_; ++i) {
      Demon* const bound_demon =
          MakeConstraintDemon1(solver(), this, &GccConstraint::PropagateValue,
                               "PropagateValue", i.value());

      variables_[i]->WhenBound(bound_demon);
    }
    Demon* const demon = MakeDelayedConstraintDemon0(
        solver(), this, &GccConstraint::PropagateRange, "PropagateRange");
    for (Index i(0); i < size_; ++i) {
      variables_[i]->WhenRange(demon);
    }
  }

  virtual void InitialPropagate() {
    // Sets the range.
    for (Index i(0); i < size_; ++i) {
      variables_[i]
          ->SetRange(first_domain_value_,
                     first_domain_value_ + max_occurrences_.size() - 1);
    }
    // Removes value with max card = 0;
    std::vector<int64> to_remove;
    for (int64 i = 0; i < max_occurrences_.size(); ++i) {
      if (max_occurrences_[i] == 0) {
        to_remove.push_back(first_domain_value_ + i);
      }
    }
    if (!to_remove.empty()) {
      for (Index i(0); i < size_; ++i) {
        variables_[i]->RemoveValues(to_remove);
      }
    }
    PropagateRange();
  }

  void PropagateRange() {
    bool has_changed = false;

    for (Index i(0); i < size_; ++i) {
      intervals_[i].min_value = variables_[i]->Min();
      intervals_[i].max_value = variables_[i]->Max();
    }

    SortIntervals();

    // The variable domains must be inside the domain defined by
    // the lower bounds_ (l) and the upper bounds_ (u).
    // assert(MinValue(l) == MinValue(u));
    // assert(MaxValue(l) == MaxValue(u));
    // assert(MinValue(l) <= sorted_by_min_[0]->min);
    // assert(sorted_by_max_[n-1]->max <= MaxValue(u));

    // Checks if there are values that must be assigned before the
    // smallest interval or after the last interval. If this is
    // the case, there is no solution to the problem
    // This is not an optimization since
    // filterLower{Min,Max} and
    // filterUpper{Min,Max} do not check for this case.
    if ((lower_sum_.Sum(lower_sum_.MinValue(),
                        sorted_by_min_[0]->min_value - 1) >
         0) ||
        (lower_sum_.Sum(sorted_by_max_[size_ - 1]->max_value + 1,
                        lower_sum_.MaxValue()) >
         0)) {
      solver()->Fail();
    }

    has_changed = FilterLowerMax();
    has_changed = FilterLowerMin() || has_changed;
    has_changed = FilterUpperMax() || has_changed;
    has_changed = FilterUpperMin() || has_changed;

    if (has_changed) {
      for (Index i(0); i < size_; ++i) {
        variables_[i]
            ->SetRange(intervals_[i].min_value, intervals_[i].max_value);
      }
    }
  }

  void PropagateValue(int index) {
    const int64 value = variables_[Index(index)]->Value();
    const int vindex = value - first_domain_value_;
    const int64 cap = max_occurrences_.Value(vindex) - 1;
    max_occurrences_.SetValue(solver(), vindex, cap);

    if (cap == 0) {
      for (Index j(0); j < size_; j++) {
        if (!variables_[j]->Bound()) {
          variables_[j]->RemoveValue(value);
        }
      }
    }
  }

  virtual std::string DebugString() const {
    // TODO(user): Improve me.
    return "GccConstraint";
  }

  void Accept(ModelVisitor* const visitor) const {
    LOG(FATAL) << "Not yet implemented";
    // TODO(user): IMPLEMENT ME.
  }

 private:
  void PathSet(std::vector<int64>* const tree, int64 start, int64 end,
               int64 to) {
    int64 l = start;
    while (l != end) {
      int64 k = l;
      l = (*tree)[k];
      (*tree)[k] = to;
    }
  }

  int64 PathMin(const std::vector<int64>& tree, int64 index) {
    int64 i = index;
    while (tree[i] < i) {
      i = tree[i];
    }
    return i;
  }

  int64 PathMax(const std::vector<int64>& tree, int64 index) {
    int64 i = index;
    while (tree[i] > i) {
      i = tree[i];
    }
    return i;
  }

  void SortIntervals() {
    std::sort(sorted_by_min_.begin(), sorted_by_min_.end(),
              CompareIntervalMin());
    std::sort(sorted_by_max_.begin(), sorted_by_max_.end(),
              CompareIntervalMax());

    int64 min = sorted_by_min_[0]->min_value;
    int64 max = sorted_by_max_[0]->max_value + 1;
    int64 last = lower_sum_.offset() + 1;
    // MODIFIED: bounds_[0] = last = min - 2;
    bounds_[0] = last;

    // merge sorted_by_min_[] and sorted_by_max_[] into bounds_[]
    int64 min_index = 0;
    int64 max_index = 0;
    int64 active_index = 0;
    // merge minsorted[] and maxsorted[] into bounds[]
    for (;;) {
      // make sure sorted_by_min_ exhausted first
      if (min_index < size_ && min <= max) {
        if (min != last) {
          bounds_[++active_index] = min;
          last = min;
        }
        sorted_by_min_[min_index]->min_rank = active_index;
        if (++min_index < size_) {
          min = sorted_by_min_[min_index]->min_value;
        }
      } else {
        if (max != last) {
          bounds_[++active_index] = max;
          last = max;
        }
        sorted_by_max_[max_index]->max_rank = active_index;
        if (++max_index == size_) {
          break;
        }
        max = sorted_by_max_[max_index]->max_value + 1;
      }
    }
    active_size_ = active_index;
    // MODIFIED: bounds_[active_index+1] = bounds_[active_index] + 2;
    bounds_[active_index + 1] = upper_sum_.last_value() + 1;
  }

  // Shrink the lower bounds_ for the max occurrences problem.

  bool FilterLowerMax() {
    bool changed = false;

    for (int64 i = 1; i <= active_size_ + 1; i++) {
      tree_[i] = i - 1;
      hall_[i] = i - 1;
      diffs_[i] = upper_sum_.Sum(bounds_[i - 1], bounds_[i] - 1);
    }
    // visit intervals in increasing max order
    for (int64 i = 0; i < size_; i++) {
      // get interval bounds_
      const int64 x = sorted_by_max_[i]->min_rank;
      const int64 y = sorted_by_max_[i]->max_rank;
      int64 z = PathMax(tree_, x + 1);
      const int64 j = tree_[z];
      if (--diffs_[z] == 0) {
        tree_[z] = z + 1;
        z = PathMax(tree_, z + 1);
        tree_[z] = j;
      }
      PathSet(&tree_, x + 1, z, z);
      if (diffs_[z] < upper_sum_.Sum(bounds_[y], bounds_[z] - 1)) {
        solver()->Fail();
      }
      if (hall_[x] > x) {
        const int64 w = PathMax(hall_, hall_[x]);
        sorted_by_max_[i]->min_value = bounds_[w];
        PathSet(&hall_, x, w, w);
        changed = true;
      }
      if (diffs_[z] == upper_sum_.Sum(bounds_[y], bounds_[z] - 1)) {
        PathSet(&hall_, hall_[y], j - 1, y);  // mark hall interval
        // hall interval [bounds_[j], bounds_[y]]
        hall_[y] = j - 1;
      }
    }
    return changed;
  }

  // Shrink the upper bounds_ for the max occurrences problem.
  bool FilterUpperMax() {
    // Assertion: FilterLowerMax returns true
    bool changed = false;

    for (int64 i = 0; i <= active_size_; i++) {
      hall_[i] = i + 1;
      tree_[i] = i + 1;
      diffs_[i] = upper_sum_.Sum(bounds_[i], bounds_[i + 1] - 1);
    }

    // Visits intervals in decreasing min order.
    for (int64 i = size_ - 1; i >= 0; --i) {
      // Gets interval bounds.
      const int64 x = sorted_by_min_[i]->max_rank;
      const int64 y = sorted_by_min_[i]->min_rank;
      int64 z = PathMin(tree_, x - 1);
      const int64 j = tree_[z];
      if (--diffs_[z] == 0) {
        tree_[z] = z - 1;
        z = PathMin(tree_, z - 1);
        tree_[z] = j;
      }
      PathSet(&tree_, x - 1, z, z);
      if (diffs_[z] < upper_sum_.Sum(bounds_[z], bounds_[y] - 1)) {
        solver()->Fail();
      }
      if (hall_[x] < x) {
        const int64 w = PathMin(hall_, hall_[x]);
        sorted_by_min_[i]->max_value = bounds_[w] - 1;
        PathSet(&hall_, x, w, w);
        changed = true;
      }
      if (diffs_[z] == upper_sum_.Sum(bounds_[z], bounds_[y] - 1)) {
        PathSet(&hall_, hall_[y], j + 1, y);
        hall_[y] = j + 1;
      }
    }
    return changed;
  }

  // Shrink the lower bounds_ for the min occurrences problem. called
  // as: FilterLowerMin(t, d, h, stable_intervals_, potential_stable_sets_,
  // new_min_);
  //
  bool FilterLowerMin() {
    bool changed = false;
    int64 w = active_size_ + 1;
    for (int64 i = active_size_ + 1; i > 0; i--) {
      // diffs_[i] = Sum(l, bounds_[potential_stable_sets_[i] =
      // stable_intervals_[i]=i-1], bounds_[i]-1);
      potential_stable_sets_[i] = stable_intervals_[i] = i - 1;
      diffs_[i] = lower_sum_.Sum(bounds_[i - 1], bounds_[i] - 1);
      // If the capacity between both bounds_ is zero, we have
      // an unstable set between these two bounds_.
      if (diffs_[i] == 0) {
        hall_[i - 1] = w;
      } else {
        w = hall_[w] = i - 1;
      }
    }

    w = active_size_ + 1;
    for (int64 i = active_size_ + 1; i >= 0; i--) {
      if (diffs_[i] == 0) {
        tree_[i] = w;
      } else {
        w = tree_[w] = i;
      }
    }

    // visit intervals in increasing max order
    for (int64 i = 0; i < size_; i++) {
      // Get interval bounds_
      const int64 x = sorted_by_max_[i]->min_rank;
      int64 y = sorted_by_max_[i]->max_rank;
      int64 z = PathMax(tree_, x + 1);
      const int64 j = tree_[z];
      if (z != x + 1) {
        // if bounds_[z] - 1 belongs to a stable set,
        // [bounds_[x], bounds_[z]) is a sub set of this stable set
        w = PathMax(potential_stable_sets_, x + 1);
        int64 v = potential_stable_sets_[w];
        PathSet(&potential_stable_sets_, x + 1, w, w);  // path compression
        w = std::min(y, z);
        PathSet(&potential_stable_sets_, potential_stable_sets_[w], v, w);
        potential_stable_sets_[w] = v;
      }

      if (diffs_[z] <= lower_sum_.Sum(bounds_[y], bounds_[z] - 1)) {
        // (potential_stable_sets_[y], y] is a stable set
        w = PathMax(stable_intervals_, potential_stable_sets_[y]);
        // Path compression
        PathSet(&stable_intervals_, potential_stable_sets_[y], w, w);
        int64 v = stable_intervals_[w];
        PathSet(&stable_intervals_, stable_intervals_[y], v, y);
        stable_intervals_[y] = v;
      } else {
        // Decrease the capacity between the two bounds_
        if (--diffs_[z] == 0) {
          tree_[z] = z + 1;
          z = PathMax(tree_, z + 1);
          tree_[z] = j;
        }

        // If the lower bound belongs to an unstable or a stable set,
        // remind the new value we might assigned to the lower bound
        // in case the variable doesn't belong to a stable set.
        if (hall_[x] > x) {
          w = PathMax(hall_, x);
          new_min_[i] = w;
          PathSet(&hall_, x, w, w);  // path compression
        } else {
          new_min_[i] = x;  // Do not shrink the variable
        }

        // If an unstable set is discovered
        if (diffs_[z] == lower_sum_.Sum(bounds_[y], bounds_[z] - 1)) {
          // Consider stable and unstable sets beyong y
          if (hall_[y] > y) {
            // Equivalent to PathMax since the path is fully compressed.
            y = hall_[y];
          }
          PathSet(&hall_, hall_[y], j - 1, y);  // mark the new unstable set
          hall_[y] = j - 1;
        }
      }
      PathSet(&tree_, x + 1, z, z);  // path compression
    }

    // If there is a failure set
    if (hall_[active_size_] != 0) {
      solver()->Fail();
    }

    // Perform path compression over all elements in
    // the stable interval data structure. This data
    // structure will no longer be modified and will be
    // accessed n or 2n times. Therefore, we can afford
    // a linear time compression.
    for (int64 i = active_size_ + 1; i > 0; i--) {
      if (stable_intervals_[i] > i)
        stable_intervals_[i] = w;
      else
        w = i;
    }

    // For all variables that are not a subset of a stable set, shrink
    // the lower bound
    for (int64 i = size_ - 1; i >= 0; i--) {
      const int64 x = sorted_by_max_[i]->min_rank;
      const int64 y = sorted_by_max_[i]->max_rank;
      if ((stable_intervals_[x] <= x) || (y > stable_intervals_[x])) {
        sorted_by_max_[i]->min_value =
            lower_sum_.SkipNonNullElementsRight(bounds_[new_min_[i]]);
        changed = true;
      }
    }

    return changed;
  }

  //
  // Shrink the upper bounds_ for the min occurrences problem.
  // called as: FilterUpperMin(t, d, h, stable_intervals_, new_min_);
  //
  bool FilterUpperMin() {
    // ASSERTION: FilterLowerMin returns true
    bool changed = false;
    int64 w = 0;
    for (int64 i = 0; i <= active_size_; i++) {
      //  diffs_[i] = bounds_[tree_[i]=hall_[i]=i+1] - bounds_[i];
      diffs_[i] = lower_sum_.Sum(bounds_[i], bounds_[i + 1] - 1);
      if (diffs_[i] == 0) {
        tree_[i] = w;
      } else {
        tree_[w] = i;
        w = i;
      }
    }
    tree_[w] = active_size_ + 1;
    w = 0;
    for (int64 i = 1; i <= active_size_; i++) {
      if (diffs_[i - 1] == 0) {
        hall_[i] = w;
      } else {
        hall_[w] = i;
        w = i;
      }
    }
    hall_[w] = active_size_ + 1;

    for (int64 i = size_;
         --i >= 0;) {  // visit intervals in decreasing min order
      // Get interval bounds_
      const int64 x = sorted_by_min_[i]->max_rank;
      int64 y = sorted_by_min_[i]->min_rank;
      int64 z = PathMin(tree_, x - 1);
      // Solve the lower bound problem
      const int64 j = tree_[z];

      // If the variable is not in a discovered stable set Possible
      // optimization: Use the array stable_intervals_ to perform this
      // test
      if (diffs_[z] > lower_sum_.Sum(bounds_[z], bounds_[y] - 1)) {
        if (--diffs_[z] == 0) {
          tree_[z = PathMin(tree_, tree_[z] = z - 1)] = j;
        }
        if (hall_[x] < x) {
          w = PathMin(hall_, hall_[x]);
          new_min_[i] = w;
          PathSet(&hall_, x, w, w);  // path compression
        } else {
          new_min_[i] = x;
        }
        if (diffs_[z] == lower_sum_.Sum(bounds_[z], bounds_[y] - 1)) {
          if (hall_[y] < y) {
            y = hall_[y];
          }
          PathSet(&hall_, hall_[y], j + 1, y);
          hall_[y] = j + 1;
        }
      }
      PathSet(&tree_, x - 1, z, z);
    }

    // For all variables that are not subsets of a stable set, shrink
    // the lower bound
    for (int64 i = size_ - 1; i >= 0; i--) {
      const int64 x = sorted_by_min_[i]->min_rank;
      const int64 y = sorted_by_min_[i]->max_rank;
      if ((stable_intervals_[x] <= x) || (y > stable_intervals_[x]))
        sorted_by_min_[i]->max_value =
            lower_sum_.SkipNonNullElementsLeft(bounds_[new_min_[i]] - 1);
      changed = true;
    }

    return changed;
  }

  ITIVector<Index, IntVar*> variables_;
  const int64 size_;
  // NumericalRev<int> current_level_;
  // int last_level_;
  NumericalRevArray<int64> max_occurrences_;
  const int64 first_domain_value_;
  std::vector<int64> tree_;   // tree links
  std::vector<int64> diffs_;  // diffs between critical capacities
  std::vector<int64> hall_;   // hall interval links
  std::vector<int64> stable_intervals_;
  std::vector<int64> potential_stable_sets_;
  std::vector<int64> new_min_;
  ITIVector<Index, Interval> intervals_;
  std::vector<Interval*> sorted_by_min_;
  std::vector<Interval*> sorted_by_max_;
  // bounds_[1..active_size_] hold set of min & max of the n intervals
  std::vector<int64> bounds_;
  // while bounds_[0] and bounds_[active_size_+1] allow sentinels
  int64 active_size_;
  PartialSum lower_sum_;
  PartialSum upper_sum_;
};
}  // namespace

Constraint* MakeGcc(Solver* const solver, const std::vector<IntVar*>& vars,
                    int64 first_domain_value,
                    const std::vector<int64>& min_occurrences,
                    const std::vector<int64>& max_occurrences) {
  return solver->RevAlloc(new GccConstraint(
      solver, vars, first_domain_value, min_occurrences.size(), min_occurrences,
      max_occurrences));
}

Constraint* MakeGcc(Solver* const solver, const std::vector<IntVar*>& vars,
                    int64 offset, const std::vector<int>& min_occurrences,
                    const std::vector<int>& max_occurrences) {
  return solver->RevAlloc(
      new GccConstraint(solver, vars, offset, min_occurrences.size(),
                        min_occurrences, max_occurrences));
}
}  // namespace operations_research
