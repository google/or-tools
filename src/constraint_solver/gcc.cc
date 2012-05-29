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


#include "base/int-type.h"
#include "base/int-type-indexed-vector.h"
#include "base/stringprintf.h"
#include "constraint_solver/constraint_solveri.h"
#include "constraint_solver/constraint_solver.h"
#include "util/const_ptr_array.h"
#include "util/string_array.h"
#include "util/vector_map.h"

namespace operations_research {
namespace {
DEFINE_INT_TYPE(Index, int);

struct Interval {
  Interval() : min_value(0), max_value(0), min_rank(0), max_rank(0) {}
  int64 min_value;
  int64 max_value;	 // start, end of Interval
  int64 min_rank;
  int64 max_rank; // rank of min & max in bounds_[]
  string DebugString() const {
    return StringPrintf("Interval(value = [%lld, %lld], rank = [%lld, %lld])",
                        min_value,
                        max_value,
                        min_rank,
                        max_rank);
  }
};

struct CompareIntervalMin {
  inline bool operator() (Interval* const i1, Interval* const i2) {
    return (i1->min_value < i2->min_value);
  }
};

struct CompareIntervalMax {
  inline bool operator() (Interval* const i1, Interval* const i2) {
    return (i1->max_value < i2->max_value);
  }
};

class PartialSum {
 public:
  // Create a partial sum data structure adapted to the
  // filterLower{Min,Max} and filterUpper{Min,Max} functions.
  // Two elements before and after the element list will be added
  // with a weight of 1.
  PartialSum(int64 offset,
             int64 count,
             const std::vector<int64>& elements) :
      offset_(offset - 3),  // We add 3 elements at the beginning.
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

  PartialSum(int64 offset,
             int64 count,
             const std::vector<int>& elements) :
      offset_(offset - 3),
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

  int64 MinValue() const {
    return offset_ + 3;
  }

  int64 MaxValue() const {
    return last_value_ - 2;
  }

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
      return sum_[to - offset_] - sum_[from -1 - offset_];
    } else {
      DCHECK((offset_ <= to) && (from <= last_value_));
      return sum_[to - 1 - offset_] - sum_[from - offset_];
    }
  }

  int64 offset() const {
    return offset_;
  }

  int64 last_value() const {
    return last_value_;
  }

  string DebugString() const {
    return StringPrintf(
        "PartialSum(offset=%lld, last_value = %lld, sum = %s, ds = %s)",
        offset_,
        last_value_,
        Int64VectorToString(sum_, ", ").c_str(),
        Int64VectorToString(ds_, ", ").c_str());
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
  GccConstraint(Solver* const solver,
                const std::vector<IntVar*>& vars,
                int64 first_domain_value,
                int64 number_of_values,
                const std::vector<int64>& min_occurrences,
                const std::vector<int64>& max_occurrences)
      : Constraint(solver),
        variables_(vars),
        size_(vars.size()),
        current_level_(1),
        last_level_(0),
        max_occurrences_(number_of_values + 1, 0),
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

    last_level_ = -1;

    for (int64 i = 0; i < number_of_values; i++) {
      max_occurrences_.SetValue(solver, i, max_occurrences[i]);
    }

    for(Index i(0); i < size_; i++) {
      sorted_by_min_[i.value()] = &intervals_[i];
      sorted_by_max_[i.value()] = &intervals_[i];
    }
  }

  GccConstraint(Solver* const solver,
                const std::vector<IntVar*>& vars,
                int64 first_domain_value,
                int64 number_of_values,
                const std::vector<int>& min_occurrences,
                const std::vector<int>& max_occurrences)
      : Constraint(solver),
        variables_(vars),
        size_(vars.size()),
        current_level_(1),
        last_level_(0),
        max_occurrences_(number_of_values, 0),
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
    last_level_ = -1;

    for(int64 i = 0; i < number_of_values; i++) {
      max_occurrences_.SetValue(solver, i, max_occurrences[i]);
    }

    for(Index i(0); i < size_; i++) {
      sorted_by_min_[i.value()] = &intervals_[i];
      sorted_by_max_[i.value()] = &intervals_[i];
    }

    // LOG(INFO) << IntVectorToString(max_occurrences, ", ");
    // LOG(INFO) << "Upper = " << upper_sum_.DebugString();
    // LOG(INFO) << IntVectorToString(min_occurrences, ", ");
    // LOG(INFO) << "Lower = " << lower_sum_.DebugString();
    // LOG(INFO) << DebugStringVector(sorted_by_min_, ", ");
  }

  virtual ~GccConstraint() {}

  void Post() {
    for (Index i(0); i < size_; ++i) {
      Demon* const bound_demon =
          MakeConstraintDemon1(solver(),
                               this,
                               &GccConstraint::PropagateValue,
                               "PropagateValue",
                               i.value());

      variables_[i]->WhenBound(bound_demon);
    }
    Demon* const initial_demon =
        solver()->MakeDelayedConstraintInitialPropagateCallback(this);
    for (Index i(0); i < size_; ++i) {
      variables_[i]->WhenRange(initial_demon);
    }
  }

  void InitialPropagate() {
    bool has_changed = false;

    current_level_.Incr(solver());

    if (last_level_ != (current_level_.Value() - 1)) {
      // not incremental
      has_changed = true;
      for (Index i(0); i < size_; ++i) {
        intervals_[i].min_value = variables_[i]->Min();
        intervals_[i].max_value = variables_[i]->Max();
      }
    } else {
      // incremental
      has_changed = false;
      for(Index i(0); i < size_; i++) {
        const int64 old_min = intervals_[i].min_value;
        const int64 old_max = intervals_[i].max_value;
        intervals_[i].min_value = variables_[i]->Min();
        intervals_[i].max_value = variables_[i]->Max();
        if (old_min != intervals_[i].min_value ||
            old_max != intervals_[i].max_value) {
          has_changed = true;
        }
      }
    }

    last_level_ = current_level_.Value();

    if (!has_changed) {
      return;
    }

    SortIntervals();

    // The variable domains must be inside the domain defined by
    // the lower bounds_ (l) and the upper bounds_ (u).
    //assert(MinValue(l) == MinValue(u));
    //assert(MaxValue(l) == MaxValue(u));
    //assert(MinValue(l) <= sorted_by_min_[0]->min);
    //assert(sorted_by_max_[n-1]->max <= MaxValue(u));

    // Checks if there are values that must be assigned before the
    // smallest interval or after the last interval. If this is
    // the case, there is no solution to the problem
    // This is not an optimization since
    // filterLower{Min,Max} and
    // filterUpper{Min,Max} do not check for this case.
    if ((lower_sum_.Sum(lower_sum_.MinValue(),
                        sorted_by_min_[0]->min_value - 1) > 0) ||
        (lower_sum_.Sum(sorted_by_max_[size_ - 1]->max_value + 1,
                        lower_sum_.MaxValue()) > 0)) {
      solver()->Fail();
    }

    has_changed = FilterLowerMax();
    has_changed = FilterLowerMin() || has_changed;
    has_changed = FilterUpperMax() || has_changed;
    has_changed = FilterUpperMin() || has_changed;

    if (has_changed) {
      for (Index i(0); i < size_; ++i) {
        variables_[i]->SetRange(intervals_[i].min_value,
                                intervals_[i].max_value);
      }
    }
  }

  void PropagateValue(int index) {
    const int64 value = variables_[Index(index)]->Value();
    const int64 cap = max_occurrences_.Value(value) - 1;
    max_occurrences_.SetValue(solver(), value, cap);

    if (cap == 0) {
      for(Index j(0); j < size_; j++) {
        if (!variables_[j]->Bound()) {
          variables_[j]->RemoveValue(value);
        }
      }
    }
  }

 private:
  void PathSet(std::vector<int64>* const tree,
               int64 start,
               int64 end,
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

  void sortmin( Interval *v[], int n ) {
    int i, current;
    bool sorted;
    Interval *t;

    current = n-1;
    sorted = false;
    while( !sorted ) {
      sorted = true;
      for( i = 0; i < current; i++ ) {
        if( v[i]->min_value > v[i+1]->min_value ) {
          t = v[i];
          v[i] = v[i+1];
          v[i+1] = t;
          sorted = false;
        }
      }
      current--;
    }
  }

  void sortmax( Interval *v[], int n ) {
    int i, current;
    bool sorted;
    Interval *t;

    current = 0;
    sorted = false;
    while( !sorted ) {
      sorted = true;
      for( i = n-1; i > current; i-- ) {
        if( v[i]->max_value < v[i-1]->max_value ) {
          t = v[i];
          v[i] = v[i-1];
          v[i-1] = t;
          sorted = false;
        }
      }
      current++;
    }
  }


  void SortIntervals() {
    sortmin(sorted_by_min_.data(), size_);
    sortmax(sorted_by_max_.data(), size_);
    // std::sort(sorted_by_min_.begin(),
    //           sorted_by_min_.end(),
    //           CompareIntervalMin());
    // std::sort(sorted_by_max_.begin(),
    //           sorted_by_max_.end(),
    //           CompareIntervalMax());

    int64 min = sorted_by_min_[0]->min_value;
    int64 max = sorted_by_max_[0]->max_value + 1;
    int64 last = lower_sum_.offset() + 1;
    //MODIFIED: bounds_[0] = last = min - 2;
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
    //MODIFIED: bounds_[active_index+1] = bounds_[active_index] + 2;
    bounds_[active_index + 1] = upper_sum_.last_value() + 1;
    // LOG(INFO) << DebugStringVector(sorted_by_min_, ", ");
    // LOG(INFO) << DebugStringVector(sorted_by_max_, ", ");
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
      int64 x = sorted_by_max_[i]->min_rank;
      int64 y = sorted_by_max_[i]->max_rank;
      int64 z = PathMax(tree_, x + 1);
      int64 j = tree_[z];
      if (--diffs_[z] == 0) {
        tree_[z = PathMax(tree_, tree_[z]=z+1)] = j;
        // tree_[z] = z + 1;
        // z = PathMax(tree_, z + 1);
        // tree_[z] = j;
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

    // Visit intervals in decreasing min order

    for (int64 i=size_; --i>=0; ) { // visit intervals in decreasing min order

      //    for (int64 i = size_ - 1; i >= 0; --i) {
      // Get interval bounds.
      int64 x = sorted_by_min_[i]->max_rank;
      int64 y = sorted_by_min_[i]->min_rank;
      int64 z = PathMin(tree_, x - 1);
      int64 j = tree_[z];
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
        hall_[i-1] = w;
      }
      else {
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
      int64 x = sorted_by_max_[i]->min_rank;
      int64 y = sorted_by_max_[i]->max_rank;
      int64 z = PathMax(tree_, x + 1);
      int64 j = tree_[z];
      if (z != x + 1) {
        // if bounds_[z] - 1 belongs to a stable set,
        // [bounds_[x], bounds_[z]) is a sub set of this stable set
        w = PathMax(potential_stable_sets_, x + 1);
        int64 v = potential_stable_sets_[w];
        PathSet(&potential_stable_sets_, x + 1, w, w); // path compression
        w = std::min(y, z);
        PathSet(&potential_stable_sets_, potential_stable_sets_[w], v , w);
        potential_stable_sets_[w] = v;
      }

      if (diffs_[z] <= lower_sum_.Sum(bounds_[y], bounds_[z] - 1)) {
        // (potential_stable_sets_[y], y] is a stable set
        w = PathMax(stable_intervals_, potential_stable_sets_[y]);
        // Path compression
        PathSet(&stable_intervals_, potential_stable_sets_[y], w, w);
        int64 v = stable_intervals_[w];
        PathSet(&stable_intervals_,
                stable_intervals_[y],
                v,
                y);
        stable_intervals_[y] = v;
      } else {
        // Decrease the capacity between the two bounds_
        if (--diffs_[z] == 0) {
          tree_[z = PathMax(tree_, tree_[z] = z + 1)] = j;
        }

        // If the lower bound belongs to an unstable or a stable set,
        // remind the new value we might assigned to the lower bound
        // in case the variable doesn't belong to a stable set.
        if (hall_[x] > x) {
          w = PathMax(hall_, x);
          new_min_[i] = w;
          PathSet(&hall_, x, w, w); // path compression
        }
        else {
          new_min_[i] = x; // Do not shrink the variable
        }

        // If an unstable set is discovered
        if (diffs_[z] == lower_sum_.Sum(bounds_[y], bounds_[z] - 1)) {
          // Consider stable and unstable sets beyong y
          if (hall_[y] > y) {
            // Equivalent to PathMax since the path is fully compressed.
            y = hall_[y];
          }
          PathSet(&hall_,
                  hall_[y],
                  j - 1,
                  y); // mark the new unstable set
          hall_[y] = j - 1;
        }
      }
      PathSet(&tree_, x + 1, z, z); // path compression
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
      int64 x = sorted_by_max_[i]->min_rank;
      int64 y = sorted_by_max_[i]->max_rank;
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
    hall_[w] = active_size_ + 1;;

    for (int64 i = size_; --i>=0;) { // visit intervals in decreasing min order
      // Get interval bounds_
      int64 x = sorted_by_min_[i]->max_rank;
      int64 y = sorted_by_min_[i]->min_rank;
      int64 z = PathMin(tree_, x - 1);
      // Solve the lower bound problem
      int64 j = tree_[z];

      // If the variable is not in a discovered stable set Possible
      // optimization: Use the array stable_intervals_ to perform this
      // test
      if (diffs_[z] > lower_sum_.Sum(bounds_[z], bounds_[y]-1)) {
        if (--diffs_[z] == 0) {
          tree_[z = PathMin(tree_, tree_[z]=z-1)] = j;
        }
        if (hall_[x] < x) {
          w = PathMin(hall_, hall_[x]);
          new_min_[i] = w;
          PathSet(&hall_, x, w, w); // path compression
        } else {
          new_min_[i] = x;
        }
        if (diffs_[z] == lower_sum_.Sum(bounds_[z], bounds_[y]-1)) {
          if (hall_[y] < y) {
            y = hall_[y];
          }
          PathSet(&hall_,
                  hall_[y],
                  j + 1,
                  y);
          hall_[y] = j + 1;
        }
      }
      PathSet(&tree_, x - 1, z, z);
    }

    // For all variables that are not subsets of a stable set, shrink
    // the lower bound
    for (int64 i = size_ - 1; i >= 0; i--) {
      int64 x = sorted_by_min_[i]->min_rank;
      int64 y = sorted_by_min_[i]->max_rank;
      if ((stable_intervals_[x] <= x) || (y > stable_intervals_[x]))
        sorted_by_min_[i]->max_value =
            lower_sum_.SkipNonNullElementsLeft(bounds_[new_min_[i]] - 1);
      changed = true;
    }

    return changed;
  }

  TypedConstPtrArray<Index, IntVar> variables_;
  const int64 size_;
  NumericalRev<int> current_level_;
  int last_level_;
  NumericalRevArray<int64> max_occurrences_;
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


// struct Interval {
//   int64 min, max;		// start, end of interval
//   int64 minrank, maxrank; // rank of min & max in bounds[]
// };

// struct PartialSum {
//   int64 firstValue;
//   int64 lastValue;
//   int64* sum;
//   int64* ds;
// };

// class GccConstraint : public Constraint {
//  public:
//   // A value "v" must be assigned to at least
//   // minOccurrences[v - firstDomainValue] variables and at most
//   // maxOccurrences[v - firstDomainValue] variables
//   GccConstraint(Solver* const s,
// 		const std::vector<IntVar*>& vars,
// 		int64 firstDomainValue,
// 		int64 size_values,
// 		const int* const minOccurrences,
// 		const int* const maxOccurrences);
//   ~GccConstraint();
//   void Post();
//   void InitialPropagate();
//   void propagateValue(int index);
//  private:
//   std::vector<IntVar*> vars_;
//   NumericalRevArray<int> max_occurences_;
//   int64 n_;
//   int64 *t;			// tree links
//   int64 *d;			// diffs between critical capacities
//   int64 *h;			// hall Interval links
//   int64 *stable_interval_;	// stable sets
//   int64 *potentialStableSets;	// links elements that potentialy belong to same stable set
//   int64 *newMin;
//   Interval *iv;
//   Interval **minsorted;
//   Interval **maxsorted;
//   int64 *bounds;  // bounds[1..nb_] hold set of min & max of the n Intervals
//   // while bounds[0] and bounds[nb_+1] allow sentinels
//   int64 nb_;

//   PartialSum* l;
//   PartialSum* u;
//   PartialSum* initializePartialSum(int64 firstValue, int64 count,
//                                    const int* const elements);
//   void destroyPartialSum(PartialSum *p);
//   int64  sum(PartialSum *p, int64 from, int64 to);
//   int64  searchValue(PartialSum *p, int64 value);
//   int64  minValue(PartialSum *p);
//   int64  maxValue(PartialSum *p);
//   int64  skipNonNullElementsRight(PartialSum *p, int64 value);
//   int64  skipNonNullElementsLeft(PartialSum *p, int64 value);

//   void sortit();
//   bool filterLowerMax();
//   bool filterUpperMax();
//   bool filterLowerMin(int64 *tl, int64 *c,
//                       int64* stableAndUnstableSets,
//                       int64* stable_interval,
//                       int64* potentialStableSets,
//                       int64* newMin);
//   bool filterUpperMin(int64 *tl, int64 *c,
//                       int64* stableAndUnstableSets,
//                       int64* stable_interval,
//                       int64* newMax);
// };

// GccConstraint::GccConstraint(
//     Solver* const s,
//     const std::vector<IntVar*>& vars,
//     int64 firstDomainValue,
//     int64 size_values,
//     const int* const minOccurrences,
//     const int* const maxOccurrences )
//     : Constraint(s),
//       vars_( vars ),
//       max_occurences_(size_values, 0) {
//   int64 i;

//   n_ = vars.size();

//   for( i = 0; i < size_values; i++ ) {
//     max_occurences_.SetValue( solver(), i, maxOccurrences[i] );
//   }

//   iv        = (Interval  *)calloc(n_, sizeof(Interval  ));
//   minsorted = (Interval **)calloc(n_, sizeof(Interval *));
//   maxsorted = (Interval **)calloc(n_, sizeof(Interval *));
//   bounds    = (int64 *)calloc(2*n_+2, sizeof(int));

//   for( i = 0; i < n_; i++ ) {
//     minsorted[i] = maxsorted[i] = &iv[i];
//   }

//   t = (int64 *)calloc(2*n_+2, sizeof(int));
//   d = (int64 *)calloc(2*n_+2, sizeof(int));
//   h = (int64 *)calloc(2*n_+2, sizeof(int));

//   stable_interval_      = (int64 *)calloc(2*n_+2, sizeof(int));
//   potentialStableSets = (int64 *)calloc(2*n_+2, sizeof(int));
//   newMin              = (int64 *)calloc(  n_,   sizeof(int));

//   l = initializePartialSum(firstDomainValue, size_values, minOccurrences);
//   u = initializePartialSum(firstDomainValue, size_values, maxOccurrences);
// }


// GccConstraint::~GccConstraint() {
//   free(bounds);
//   free(maxsorted);
//   free(minsorted);
//   free(iv);
//   free(h);
//   free(d);
//   free(t);
//   free(newMin);
//   free(potentialStableSets);
//   free(stable_interval_);
//   destroyPartialSum(u);
//   destroyPartialSum(l);
// }


// void GccConstraint::Post() {
//   Demon* const range = solver()->MakeConstraintInitialPropagateCallback(this);
//   for (int i = 0; i < n_; ++i) {
//     Demon* const demon =
//         MakeConstraintDemon1(solver(),
//                              this,
//                              &GccConstraint::propagateValue,
//                              "propagateValue",
//                              i);
//     vars_[i]->WhenBound(demon);
//     vars_[i]->WhenRange(range);
//   }
// }


// void GccConstraint::propagateValue(int index) {
//   int64 j, v, cap;

//   v   = vars_[index]->Value();
//   cap = max_occurences_.Value(v) - 1;
//   max_occurences_.SetValue(solver(), v, cap );

//   if( cap == 0 ) {
//     for( j = 0; j < n_; j++ ) {
//       if( !vars_[j]->Bound() ) {
//         vars_[j]->RemoveValue( v );
//       }
//     }
//   }
// }


// void sortmin( Interval *v[], int64 n ) {
//   int64 i, current;
//   bool sorted;
//   Interval *t;

//   current = n-1;
//   sorted = false;
//   while( !sorted ) {
//     sorted = true;
//     for( i = 0; i < current; i++ ) {
//       if( v[i]->min > v[i+1]->min ) {
//         t = v[i];
//         v[i] = v[i+1];
//         v[i+1] = t;
//         sorted = false;
//       }
//     }
//     current--;
//   }
// }

// void sortmax( Interval *v[], int64 n ) {
//   int64 i, current;
//   bool sorted;
//   Interval *t;

//   current = 0;
//   sorted = false;
//   while( !sorted ) {
//     sorted = true;
//     for( i = n-1; i > current; i-- ) {
//       if( v[i]->max < v[i-1]->max ) {
//         t = v[i];
//         v[i] = v[i-1];
//         v[i-1] = t;
//         sorted = false;
//       }
//     }
//     current++;
//   }
// }

// void GccConstraint::sortit() {
//   int64 i,j,nbl,min,max,last;

//   sortmin(minsorted, n_);
//   sortmax(maxsorted, n_);

//   min = minsorted[0]->min;
//   max = maxsorted[0]->max + 1;
//   //MODIFIED: bounds[0] = last = min-2;
//   bounds[0] = last = l->firstValue + 1;

//   for (i=j=nbl=0;;) { // merge minsorted[] and maxsorted[] into bounds[]
//     if (i<n_ && min<=max) {	// make sure minsorted exhausted first
//       if (min != last)
//         bounds[++nbl] = last = min;
//       minsorted[i]->minrank = nbl;
//       if (++i < n_)
//         min = minsorted[i]->min;
//     } else {
//       if (max != last)
//         bounds[++nbl] = last = max;
//       maxsorted[j]->maxrank = nbl;
//       if (++j == n_) break;
//       max = maxsorted[j]->max + 1;
//     }
//   }
//   nb_ = nbl;
//   //MODIFIED: bounds[nb+1] = bounds[nb_] + 2;
//   bounds[nbl+1] = u->lastValue + 1;
// }


// void pathset(int64 *t, int64 start, int64 end, int64 to) {
//   int64 k, l;
//   for (l=start; (k=l) != end; t[k]=to) {
//     l = t[k];
//   }
// }

// int64 pathmin(int64 *t, int64 i) {
//   for (; t[i] < i; i=t[i]) {
//     ;
//   }
//   return i;
// }

// int64 pathmax(int64 *t, int64 i) {
//   for (; t[i] > i; i=t[i]) {
//     ;
//   }
//   return i;
// }

// /*
//  * Shrink the lower bounds for the max occurrences problem.
//  */
// bool GccConstraint::filterLowerMax() {
//   int64 i,j,w,x,y,z;
//   bool changes = false;

//   for (i=1; i<=nb_+1; i++) {
//     t[i] = h[i] = i-1;
//     d[i] = sum(u, bounds[i-1], bounds[i]-1);
//   }
//   for (i=0; i<n_; i++) { // visit Intervals in increasing max order
//     // get Interval bounds
//     x = maxsorted[i]->minrank; y = maxsorted[i]->maxrank;
//     j = t[z = pathmax(t, x+1)];
//     if (--d[z] == 0) {
//       t[z = pathmax(t, t[z]=z+1)] = j;
//     }
//     pathset(t, x+1, z, z);
//     if (d[z] < sum(u, bounds[y], bounds[z] - 1)) {
//       solver()->Fail();
//     }
//     if (h[x] > x) {
//       maxsorted[i]->min = bounds[w = pathmax(h, h[x])];
//       pathset(h, x, w, w);
//       changes = true;
//     }
//     if (d[z] == sum(u, bounds[y], bounds[z] - 1)) {
//       pathset(h, h[y], j-1, y); // mark hall Interval
//       h[y] = j-1; //("hall Interval [%d,%d)\n",bounds[j],bounds[y]);
//     }
//   }
//   return changes;
// }

// /*
//  * Shrink the upper bounds for the max occurrences problem.
//  */
// bool GccConstraint::filterUpperMax() {
//   // Assertion: filterLowerMax returns true
//   int64 i,j,w,x,y,z;
//   bool changes = false;

//   for (i=0; i<=nb_; i++) {
//     d[i] = sum(u, bounds[i], bounds[t[i]=h[i]=i+1]-1);
//   }
//   for (i=n_; --i>=0; ) { // visit Intervals in decreasing min order
//     // get Interval bounds
//     x = minsorted[i]->maxrank; y = minsorted[i]->minrank;
//     j = t[z = pathmin(t, x-1)];
//     if (--d[z] == 0) {
//       t[z = pathmin(t, t[z]=z-1)] = j;
//     }
//     pathset(t, x-1, z, z);
//     if (d[z] < sum(u, bounds[z], bounds[y]-1)) {
//       solver()->Fail(); // no solution
//     }
//     if (h[x] < x) {
//       minsorted[i]->max = bounds[w = pathmin(h, h[x])] - 1;
//       pathset(h, x, w, w);
//       changes = true;
//     }
//     if (d[z] == sum(u, bounds[z], bounds[y]-1)) {
//       pathset(h, h[y], j+1, y);
//       h[y] = j+1;
//     }
//   }
//   return changes;
// }


// /*
//  * Shrink the lower bounds for the min occurrences problem.
//  * called as: filterLowerMin(t, d, h, stableInterval, potentialStableSets, newMin);
//  */
// bool GccConstraint::filterLowerMin(int64 *tl, int64 *c,
//                                    int64* stableAndUnstableSets,
//                                    int64* stable_interval,
//                                    int64* potentialStableSets,
//                                    int64* newMin) {
//   int64 i,j,w,x,y,z,v;
//   bool changes = false;

//   for (w=i=nb_+1; i>0; i--) {
//     //c[i] = sum(l, bounds[potentialStableSets[i]=stableInterval[i]=i-1], bounds[i]-1);
//     potentialStableSets[i]=stable_interval[i]=i-1;
//     c[i] = sum(l, bounds[i-1], bounds[i]-1);
//     // If the capacity between both bounds is zero, we have
//     // an unstable set between these two bounds.
//     if (c[i] == 0) {
//       stableAndUnstableSets[i-1] = w;
//     }
//     else {
//       w = stableAndUnstableSets[w] = i - 1;
//     }
//   }

//   for (i = w = nb_ + 1; i >= 0; i--) {
//     if (c[i] == 0)
//       tl[i] = w;
//     else
//       w = tl[w] = i;
//   }

//   for (i = 0; i < n_; i++) { // visit Intervals in increasing max order
//     // Get Interval bounds
//     x = maxsorted[i]->minrank; y = maxsorted[i]->maxrank;
//     j = tl[z = pathmax(tl, x+1)];
//     if (z != x+1) {
//       // if bounds[z] - 1 belongs to a stable set,
//       // [bounds[x], bounds[z]) is a sub set of this stable set
//       v = potentialStableSets[w = pathmax(potentialStableSets, x + 1)];
//       pathset(potentialStableSets, x+1, w, w); // path compression
//       w = y < z ? y : z;
//       pathset(potentialStableSets, potentialStableSets[w], v , w);
//       potentialStableSets[w] = v;
//     }

//     if (c[z] <= sum(l, bounds[y], bounds[z] - 1)) {
//       // (potentialStableSets[y], y] is a stable set w = pathmax(stable_interval, potentialStableSets[y]);
//       pathset(stable_interval, potentialStableSets[y], w, w); // Path compression
//       pathset(stable_interval, stable_interval[y], v=stable_interval[w], y);
//       stable_interval[y] = v;
//     }
//     else {
//       // Decrease the capacity between the two bounds
//       if (--c[z] == 0) {
// 	tl[z = pathmax(tl, tl[z]=z+1)] = j;
//       }

//       // If the lower bound belongs to an unstable or a stable set,
//       // remind the new value we might assigned to the lower bound
//       // in case the variable doesn't belong to a stable set.
//       if (stableAndUnstableSets[x] > x) {
// 	w = newMin[i] = pathmax(stableAndUnstableSets, x);
// 	pathset(stableAndUnstableSets, x, w, w); // path compression
//       }
//       else {
// 	newMin[i] = x; // Do not shrink the variable
//       }

//       // If an unstable set is discovered
//       if (c[z] == sum(l, bounds[y], bounds[z] - 1)) {
//  	if (stableAndUnstableSets[y] > y) // Consider stable and unstable sets beyong y
//  	  y = stableAndUnstableSets[y]; // Equivalent to pathmax since the path is fully compressed
// 	pathset(stableAndUnstableSets, stableAndUnstableSets[y], j-1, y); // mark the new unstable set
// 	stableAndUnstableSets[y] = j-1;
//       }
//     }
//     pathset(tl, x+1, z, z); // path compression
//   }

//   // If there is a failure set
//   if (stableAndUnstableSets[nb_] != 0) {
//     solver()->Fail(); // no solution
//   }

//   // Perform path compression over all elements in
//   // the stable Interval data structure. This data
//   // structure will no longer be modified and will be
//   // accessed n or 2n times. Therefore, we can afford
//   // a linear time compression.
//   for (i = nb_+1; i > 0; i--) {
//     if (stable_interval[i] > i)
//       stable_interval[i] = w;
//     else
//       w = i;
//   }

//   // For all variables that are not a subset of a stable set, shrink the lower bound
//   for (i=n_-1; i>=0; i--) {
//     x = maxsorted[i]->minrank; y = maxsorted[i]->maxrank;
//     if ((stable_interval[x] <= x) || (y > stable_interval[x])) {
//       maxsorted[i]->min = skipNonNullElementsRight(l, bounds[newMin[i]]);
//       changes = true;
//     }
//   }
//   return changes;
// }


// /*
//  * Shrink the upper bounds for the min occurrences problem.
//  * called as: filterUpperMin(t, d, h, stable_interval, newMin);
//  */
// bool GccConstraint::filterUpperMin(int64 *tl, int64 *c,
//                                    int64* stableAndUnstableSets,
//                                    int64* stable_interval,
//                                    int64* newMax) {
//   // ASSERTION: filterLowerMin returns true
//   int64 i,j,w,x,y,z;
//   bool changes = false;

//   for (w=i=0; i<=nb_; i++) {
//     //    d[i] = bounds[t[i]=h[i]=i+1] - bounds[i];
//     c[i] = sum(l, bounds[i], bounds[i+1]-1);
//     if (c[i] == 0)
//       tl[i]=w;
//     else
//       w=tl[w]=i;
//   }
//   tl[w]=i;
//   for (i = 1, w = 0; i<=nb_; i++) {
//     if (c[i-1] == 0)
//       stableAndUnstableSets[i] = w;
//     else
//       w = stableAndUnstableSets[w] = i;
//   }
//   stableAndUnstableSets[w] = i;

//   for (i=n_; --i>=0; ) { // visit Intervals in decreasing min order
//     // Get Interval bounds
//     x = minsorted[i]->maxrank; y = minsorted[i]->minrank;

//     // Solve the lower bound problem
//     j = tl[z = pathmin(tl, x-1)];

//     // If the variable is not in a discovered stable set
//     // Possible optimization: Use the array stable_interval to perform this test
//     if (c[z] > sum(l, bounds[z], bounds[y]-1)) {
//       if (--c[z] == 0) {
// 	tl[z = pathmin(tl, tl[z]=z-1)] = j;
//       }
//       if (stableAndUnstableSets[x] < x) {
// 	newMax[i] = w = pathmin(stableAndUnstableSets, stableAndUnstableSets[x]);
// 	pathset(stableAndUnstableSets, x, w, w); // path compression
//       }
//       else {
// 	newMax[i] = x;
//       }
//       if (c[z] == sum(l, bounds[z], bounds[y]-1)) {
// 	if (stableAndUnstableSets[y] < y) {
// 	  y = stableAndUnstableSets[y];
//         }
// 	pathset(stableAndUnstableSets, stableAndUnstableSets[y], j+1, y);
// 	stableAndUnstableSets[y] = j+1;
//       }
//     }
//     pathset(tl, x-1, z, z);
//   }

//   // For all variables that are not subsets of a stable set, shrink the lower bound
//   for (i=n_-1; i>=0; i--) {
//     x = minsorted[i]->minrank; y = minsorted[i]->maxrank;
//     if ((stable_interval[x] <= x) || (y > stable_interval[x]))
//       minsorted[i]->max = skipNonNullElementsLeft(l, bounds[newMax[i]]-1);
//     changes = true;
//   }
//   return changes;
// }


// void GccConstraint::InitialPropagate() {
//   bool statusLower, statusUpper;
//   bool statusLowerMin, statusUpperMin;
//   bool statusLowerMax, statusUpperMax;
//   int64 i;
//   int64 dl, du;

//   statusLower = true;
//   statusUpper = true;
//   for (int64 i = 0; i < vars_.size(); ++i) {
//     iv[i].min = vars_[i]->Min();
//     iv[i].max = vars_[i]->Max();
//     i++;
//   }

//   if (!statusLower && !statusUpper) {
//     return;
//   }

//   sortit();

//   // The variable domains must be inside the domain defined by
//   // the lower bounds (l) and the upper bounds (u).
//   //assert(minValue(l) == minValue(u));
//   //assert(maxValue(l) == maxValue(u));
//   //assert(minValue(l) <= minsorted[0]->min);
//   //assert(maxsorted[n-1]->max <= maxValue(u));

//   // Checks if there are values that must be assigned before the
//   // smallest Interval or after the last Interval. If this is
//   // the case, there is no solution to the problem
//   // This is not an optimization since
//   // filterLower{Min,Max} and
//   // filterUpper{Min,Max} do not check for this case.
//   if ((sum(l, minValue(l), minsorted[0]->min - 1) > 0) ||
//       (sum(l, maxsorted[n_-1]->max + 1, maxValue(l)) > 0)) {
//     solver()->Fail();
//   }

//   statusLowerMax = filterLowerMax();
//   statusLowerMin = filterLowerMin(t,
//                                   d,
//                                   h,
//                                   stable_interval_,
//                                   potentialStableSets,
//                                   newMin);

//   statusUpperMax = filterUpperMax();
//   statusUpperMin = filterUpperMin(t, d, h, stable_interval_, newMin);

//   if (statusLowerMax || statusLowerMin || statusUpperMax || statusUpperMin) {
//     for (int64 i = 0; i < vars_.size(); ++i) {
//       vars_[i]->SetRange(iv[i].min, iv[i].max);
//     }
//   }
// }

// // Create a partial sum data structure adapted to the
// // filterLower{Min,Max} and filterUpper{Min,Max} functions.
// // Two elements before and after the element list will be added
// // with a weight of 1.
// PartialSum* GccConstraint::initializePartialSum(int64 firstValue,
//                                                 int64 count,
//                                                 const int* const elements) {
//   int64 i,j;
//   PartialSum* res = new PartialSum;
//   res->sum = new int64[count+1+2+2];
//   res->firstValue = firstValue - 3; // We add three elements at the beginning
//   res->lastValue = firstValue + count + 1;
//   res->sum[0] = 0;
//   res->sum[1] = 1;
//   res->sum[2] = 2;
//   for (i = 0; i < count; i++) {
//     res->sum[i+3] = res->sum[i+2] + elements[i];
//   }
//   res->sum[count + 3] = res->sum[count + 2] + 1;
//   res->sum[count + 4] = res->sum[count + 3] + 1;

//   res->ds = new int64[count+1+2+2];

//   for (j=(i=count+3)+1; i > 0;) {
//     while (res->sum[i] == res->sum[i-1]) {
//       res->ds[i--]=j;
//     }
//     j=res->ds[j]=i--;
//   }
//   res->ds[j]=0;
//   return res;
// }

// void GccConstraint::destroyPartialSum(PartialSum *p) {
//   delete p->ds;
//   delete p->sum;
//   delete p;
// }

// int64 GccConstraint::sum(PartialSum *p, int64 from, int64 to) {
//   if (from <= to) {
//     //assert((p->firstValue <= from) && (to <= p->lastValue));
//     return p->sum[to - p->firstValue] - p->sum[from - p->firstValue - 1];
//   }
//   else {
//     //assert((p->firstValue <= to) && (from <= p->lastValue));
//     return p->sum[to - p->firstValue - 1] - p->sum[from - p->firstValue];
//   }
// }

// int64 GccConstraint::minValue(PartialSum *p) {
//   return p->firstValue + 3;
// }

// int64 GccConstraint::maxValue(PartialSum *p) {
//   return p->lastValue - 2;
// }

// int64 GccConstraint::skipNonNullElementsRight(PartialSum *p, int64 value) {
//   value -= p->firstValue;
//   return (p->ds[value] < value ? value : p->ds[value]) + p->firstValue;
// }

// int64 GccConstraint::skipNonNullElementsLeft(PartialSum *p, int64 value) {
//   value -= p->firstValue;
//   return (p->ds[value] > value ? p->ds[p->ds[value]] : value) + p->firstValue;
// }
} // namespace

Constraint* MakeGcc(Solver* const solver,
                    const std::vector<IntVar*>& vars,
                    int64 first_domain_value,
                    const std::vector<int64>& min_occurrences,
                    const std::vector<int64>& max_occurrences) {
  return solver->RevAlloc(
      new GccConstraint(
          solver,
          vars,
          first_domain_value,
          min_occurrences.size(),
          min_occurrences,
          max_occurrences));
}

Constraint* MakeGcc(Solver* const solver,
                    const std::vector<IntVar*>& vars,
                    int64 offset,
                    const std::vector<int>& min_occurrences,
                    const std::vector<int>& max_occurrences) {
  return solver->RevAlloc(
      new GccConstraint(
          solver,
          vars,
          offset,
          min_occurrences.size(),
          min_occurrences,
          max_occurrences));
}
} // namespace operations_research
