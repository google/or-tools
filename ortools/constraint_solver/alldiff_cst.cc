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

//  AllDifferent constraints

#include "ortools/constraint_solver/alldiff_cst.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraints.h"
#include "ortools/constraint_solver/utilities.h"
#include "ortools/util/string_array.h"

namespace operations_research {

std::string BaseAllDifferent::DebugStringInternal(
    absl::string_view name) const {
  return absl::StrFormat("%s(%s)", name, JoinDebugStringPtr(vars_, ", "));
}

void ValueAllDifferent::Post() {
  for (int i = 0; i < size(); ++i) {
    IntVar* var = vars_[i];
    Demon* d = MakeConstraintDemon1(solver(), this, &ValueAllDifferent::OneMove,
                                    "OneMove", i);
    var->WhenBound(d);
  }
}

void ValueAllDifferent::InitialPropagate() {
  for (int i = 0; i < size(); ++i) {
    if (vars_[i]->Bound()) {
      OneMove(i);
    }
  }
}

void ValueAllDifferent::OneMove(int index) {
  if (!AllMoves()) {
    const int64_t val = vars_[index]->Value();
    for (int j = 0; j < size(); ++j) {
      if (index != j) {
        if (vars_[j]->Size() < 0xFFFFFF) {
          vars_[j]->RemoveValue(val);
        } else {
          solver()->AddConstraint(solver()->MakeNonEquality(vars_[j], val));
        }
      }
    }
  }
}

bool ValueAllDifferent::AllMoves() {
  if (all_instantiated_.Switched() || size() == 0) {
    return true;
  }
  for (int i = 0; i < size(); ++i) {
    if (!vars_[i]->Bound()) {
      return false;
    }
  }
  std::unique_ptr<int64_t[]> values(new int64_t[size()]);
  for (int i = 0; i < size(); ++i) {
    values[i] = vars_[i]->Value();
  }
  std::sort(values.get(), values.get() + size());
  for (int i = 0; i < size() - 1; ++i) {
    if (values[i] == values[i + 1]) {
      values.reset();  // prevent leaks (solver()->Fail() won't return)
      solver()->Fail();
    }
  }
  all_instantiated_.Switch(solver());
  return true;
}

std::string ValueAllDifferent::DebugString() const {
  return DebugStringInternal("ValueAllDifferent");
}

void ValueAllDifferent::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kAllDifferent, this);
  visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                             vars_);
  visitor->VisitIntegerArgument(ModelVisitor::kRangeArgument, 0);
  visitor->EndVisitConstraint(ModelVisitor::kAllDifferent, this);
}

RangeBipartiteMatching::RangeBipartiteMatching(Solver* solver, int size)
    : solver_(solver),
      size_(size),
      intervals_(new Interval[size + 1]),
      min_sorted_(new Interval*[size]),
      max_sorted_(new Interval*[size]),
      bounds_(new int64_t[2 * size + 2]),
      tree_(new int[2 * size + 2]),
      diff_(new int64_t[2 * size + 2]),
      hall_(new int[2 * size + 2]),
      active_size_(0) {
  for (int i = 0; i < size; ++i) {
    max_sorted_[i] = &intervals_[i];
    min_sorted_[i] = max_sorted_[i];
  }
}

void RangeBipartiteMatching::SetRange(int index, int64_t imin, int64_t imax) {
  intervals_[index].min = imin;
  intervals_[index].max = imax;
}

bool RangeBipartiteMatching::Propagate() {
  SortArray();

  const bool modified1 = PropagateMin();
  const bool modified2 = PropagateMax();
  return modified1 || modified2;
}

int64_t RangeBipartiteMatching::Min(int index) const {
  return intervals_[index].min;
}

int64_t RangeBipartiteMatching::Max(int index) const {
  return intervals_[index].max;
}

void RangeBipartiteMatching::SortArray() {
  std::sort(min_sorted_.get(), min_sorted_.get() + size_, CompareIntervalMin());
  std::sort(max_sorted_.get(), max_sorted_.get() + size_, CompareIntervalMax());

  int64_t min = min_sorted_[0]->min;
  int64_t max = max_sorted_[0]->max + 1;
  int64_t last = min - 2;
  bounds_[0] = last;

  int i = 0;
  int j = 0;
  int nb = 0;
  for (;;) {  // merge min_sorted_[] and max_sorted_[] into bounds_[].
    if (i < size_ && min <= max) {  // make sure min_sorted_ exhausted first.
      if (min != last) {
        last = min;
        bounds_[++nb] = last;
      }
      min_sorted_[i]->min_rank = nb;
      if (++i < size_) {
        min = min_sorted_[i]->min;
      }
    } else {
      if (max != last) {
        last = max;
        bounds_[++nb] = last;
      }
      max_sorted_[j]->max_rank = nb;
      if (++j == size_) {
        break;
      }
      max = max_sorted_[j]->max + 1;
    }
  }
  active_size_ = nb;
  bounds_[nb + 1] = bounds_[nb] + 2;
}

bool RangeBipartiteMatching::PropagateMin() {
  bool modified = false;

  for (int i = 1; i <= active_size_ + 1; ++i) {
    hall_[i] = i - 1;
    tree_[i] = i - 1;
    diff_[i] = bounds_[i] - bounds_[i - 1];
  }
  // visit intervals in increasing max order
  for (int i = 0; i < size_; ++i) {
    const int x = max_sorted_[i]->min_rank;
    const int y = max_sorted_[i]->max_rank;
    int z = PathMax(tree_.get(), x + 1);
    int j = tree_[z];
    if (--diff_[z] == 0) {
      tree_[z] = z + 1;
      z = PathMax(tree_.get(), z + 1);
      tree_[z] = j;
    }
    PathSet(x + 1, z, z, tree_.get());  // path compression
    if (diff_[z] < bounds_[z] - bounds_[y]) {
      solver_->Fail();
    }
    if (hall_[x] > x) {
      int w = PathMax(hall_.get(), hall_[x]);
      max_sorted_[i]->min = bounds_[w];
      PathSet(x, w, w, hall_.get());  // path compression
      modified = true;
    }
    if (diff_[z] == bounds_[z] - bounds_[y]) {
      PathSet(hall_[y], j - 1, y, hall_.get());  // mark hall interval
      hall_[y] = j - 1;
    }
  }
  return modified;
}

bool RangeBipartiteMatching::PropagateMax() {
  bool modified = false;

  for (int i = 0; i <= active_size_; i++) {
    tree_[i] = i + 1;
    hall_[i] = i + 1;
    diff_[i] = bounds_[i + 1] - bounds_[i];
  }
  // visit intervals in decreasing min order
  for (int i = size_ - 1; i >= 0; --i) {
    const int x = min_sorted_[i]->max_rank;
    const int y = min_sorted_[i]->min_rank;
    int z = PathMin(tree_.get(), x - 1);
    int j = tree_[z];
    if (--diff_[z] == 0) {
      tree_[z] = z - 1;
      z = PathMin(tree_.get(), z - 1);
      tree_[z] = j;
    }
    PathSet(x - 1, z, z, tree_.get());
    if (diff_[z] < bounds_[y] - bounds_[z]) {
      solver_->Fail();
      // useless. Should have been caught by the PropagateMin() method.
    }
    if (hall_[x] < x) {
      int w = PathMin(hall_.get(), hall_[x]);
      min_sorted_[i]->max = bounds_[w] - 1;
      PathSet(x, w, w, hall_.get());
      modified = true;
    }
    if (diff_[z] == bounds_[y] - bounds_[z]) {
      PathSet(hall_[y], j + 1, y, hall_.get());
      hall_[y] = j + 1;
    }
  }
  return modified;
}

void RangeBipartiteMatching::PathSet(int start, int end, int to,
                                     int* const tree) {
  int l = start;
  while (l != end) {
    int k = l;
    l = tree[k];
    tree[k] = to;
  }
}

int RangeBipartiteMatching::PathMin(const int* const tree, int index) {
  int i = index;
  while (tree[i] < i) {
    i = tree[i];
  }
  return i;
}

int RangeBipartiteMatching::PathMax(const int* const tree, int index) {
  int i = index;
  while (tree[i] > i) {
    i = tree[i];
  }
  return i;
}

void BoundsAllDifferent::Post() {
  Demon* range = MakeDelayedConstraintDemon0(
      solver(), this, &BoundsAllDifferent::IncrementalPropagate,
      "IncrementalPropagate");

  for (int i = 0; i < size(); ++i) {
    vars_[i]->WhenRange(range);
    Demon* bound = MakeConstraintDemon1(solver(), this,
                                        &BoundsAllDifferent::PropagateValue,
                                        "PropagateValue", i);
    vars_[i]->WhenBound(bound);
  }
}

void BoundsAllDifferent::InitialPropagate() {
  IncrementalPropagate();
  for (int i = 0; i < size(); ++i) {
    if (vars_[i]->Bound()) {
      PropagateValue(i);
    }
  }
}

void BoundsAllDifferent::IncrementalPropagate() {
  for (int i = 0; i < size(); ++i) {
    matching_.SetRange(i, vars_[i]->Min(), vars_[i]->Max());
  }

  if (matching_.Propagate()) {
    for (int i = 0; i < size(); ++i) {
      vars_[i]->SetRange(matching_.Min(i), matching_.Max(i));
    }
  }
}

void BoundsAllDifferent::PropagateValue(int index) {
  const int64_t to_remove = vars_[index]->Value();
  for (int j = 0; j < index; j++) {
    if (vars_[j]->Size() < 0xFFFFFF) {
      vars_[j]->RemoveValue(to_remove);
    } else {
      solver()->AddConstraint(solver()->MakeNonEquality(vars_[j], to_remove));
    }
  }
  for (int j = index + 1; j < size(); j++) {
    if (vars_[j]->Size() < 0xFFFFFF) {
      vars_[j]->RemoveValue(to_remove);
    } else {
      solver()->AddConstraint(solver()->MakeNonEquality(vars_[j], to_remove));
    }
  }
}

std::string BoundsAllDifferent::DebugString() const {
  return DebugStringInternal("BoundsAllDifferent");
}

void BoundsAllDifferent::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kAllDifferent, this);
  visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                             vars_);
  visitor->VisitIntegerArgument(ModelVisitor::kRangeArgument, 1);
  visitor->EndVisitConstraint(ModelVisitor::kAllDifferent, this);
}

void SortConstraint::Post() {
  Demon* const demon =
      solver()->MakeDelayedConstraintInitialPropagateCallback(this);
  for (int i = 0; i < size(); ++i) {
    ovars_[i]->WhenRange(demon);
    svars_[i]->WhenRange(demon);
  }
}

void SortConstraint::InitialPropagate() {
  for (int i = 0; i < size(); ++i) {
    int64_t vmin = 0;
    int64_t vmax = 0;
    ovars_[i]->Range(&vmin, &vmax);
    mins_[i] = vmin;
    maxs_[i] = vmax;
  }
  // Propagates from variables to sorted variables.
  std::sort(mins_.begin(), mins_.end());
  std::sort(maxs_.begin(), maxs_.end());
  for (int i = 0; i < size(); ++i) {
    svars_[i]->SetRange(mins_[i], maxs_[i]);
  }
  // Maintains sortedness.
  for (int i = 0; i < size() - 1; ++i) {
    svars_[i + 1]->SetMin(svars_[i]->Min());
  }
  for (int i = size() - 1; i > 0; --i) {
    svars_[i - 1]->SetMax(svars_[i]->Max());
  }
  // Reverse propagation.
  for (int i = 0; i < size(); ++i) {
    int64_t imin = 0;
    int64_t imax = 0;
    FindIntersectionRange(i, &imin, &imax);
    matching_.SetRange(i, imin, imax);
  }
  matching_.Propagate();
  for (int i = 0; i < size(); ++i) {
    const int64_t vmin = svars_[matching_.Min(i)]->Min();
    const int64_t vmax = svars_[matching_.Max(i)]->Max();
    ovars_[i]->SetRange(vmin, vmax);
  }
}

void SortConstraint::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kSortingConstraint, this);
  visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                             ovars_);
  visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kTargetArgument,
                                             svars_);
  visitor->EndVisitConstraint(ModelVisitor::kSortingConstraint, this);
}

std::string SortConstraint::DebugString() const {
  return absl::StrFormat("Sort(%s, %s)", JoinDebugStringPtr(ovars_, ", "),
                         JoinDebugStringPtr(svars_, ", "));
}

void SortConstraint::FindIntersectionRange(int index, int64_t* const range_min,
                                           int64_t* const range_max) const {
  // Naive version.
  // TODO(user): Implement log(n) version.
  int64_t imin = 0;
  while (imin < size() && NotIntersect(index, imin)) {
    imin++;
  }
  if (imin == size()) {
    solver()->Fail();
  }
  int64_t imax = size() - 1;
  while (imax > imin && NotIntersect(index, imax)) {
    imax--;
  }
  *range_min = imin;
  *range_max = imax;
}

bool SortConstraint::NotIntersect(int oindex, int sindex) const {
  return ovars_[oindex]->Min() > svars_[sindex]->Max() ||
         ovars_[oindex]->Max() < svars_[sindex]->Min();
}

void AllDifferentExcept::Post() {
  for (int i = 0; i < vars_.size(); ++i) {
    IntVar* const var = vars_[i];
    Demon* const d = MakeConstraintDemon1(
        solver(), this, &AllDifferentExcept::Propagate, "Propagate", i);
    var->WhenBound(d);
  }
}

void AllDifferentExcept::InitialPropagate() {
  for (int i = 0; i < vars_.size(); ++i) {
    if (vars_[i]->Bound()) {
      Propagate(i);
    }
  }
}

void AllDifferentExcept::Propagate(int index) {
  const int64_t val = vars_[index]->Value();
  if (val != escape_value_) {
    for (int j = 0; j < vars_.size(); ++j) {
      if (index != j) {
        vars_[j]->RemoveValue(val);
      }
    }
  }
}

std::string AllDifferentExcept::DebugString() const {
  return absl::StrFormat("AllDifferentExcept([%s], %d",
                         JoinDebugStringPtr(vars_, ", "), escape_value_);
}

void AllDifferentExcept::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kAllDifferent, this);
  visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                             vars_);
  visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, escape_value_);
  visitor->EndVisitConstraint(ModelVisitor::kAllDifferent, this);
}

void NullIntersectArrayExcept::Post() {
  for (int i = 0; i < first_vars_.size(); ++i) {
    IntVar* const var = first_vars_[i];
    Demon* const d = MakeConstraintDemon1(
        solver(), this, &NullIntersectArrayExcept::PropagateFirst,
        "PropagateFirst", i);
    var->WhenBound(d);
  }
  for (int i = 0; i < second_vars_.size(); ++i) {
    IntVar* const var = second_vars_[i];
    Demon* const d = MakeConstraintDemon1(
        solver(), this, &NullIntersectArrayExcept::PropagateSecond,
        "PropagateSecond", i);
    var->WhenBound(d);
  }
}

void NullIntersectArrayExcept::InitialPropagate() {
  for (int i = 0; i < first_vars_.size(); ++i) {
    if (first_vars_[i]->Bound()) {
      PropagateFirst(i);
    }
  }
  for (int i = 0; i < second_vars_.size(); ++i) {
    if (second_vars_[i]->Bound()) {
      PropagateSecond(i);
    }
  }
}

void NullIntersectArrayExcept::PropagateFirst(int index) {
  const int64_t val = first_vars_[index]->Value();
  if (!has_escape_value_ || val != escape_value_) {
    for (int j = 0; j < second_vars_.size(); ++j) {
      second_vars_[j]->RemoveValue(val);
    }
  }
}

void NullIntersectArrayExcept::PropagateSecond(int index) {
  const int64_t val = second_vars_[index]->Value();
  if (!has_escape_value_ || val != escape_value_) {
    for (int j = 0; j < first_vars_.size(); ++j) {
      first_vars_[j]->RemoveValue(val);
    }
  }
}

std::string NullIntersectArrayExcept::DebugString() const {
  return absl::StrFormat("NullIntersectArray([%s], [%s], escape = %d",
                         JoinDebugStringPtr(first_vars_, ", "),
                         JoinDebugStringPtr(second_vars_, ", "), escape_value_);
}

void NullIntersectArrayExcept::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kNullIntersect, this);
  visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kLeftArgument,
                                             first_vars_);
  visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kRightArgument,
                                             second_vars_);
  visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, escape_value_);
  visitor->EndVisitConstraint(ModelVisitor::kNullIntersect, this);
}

}  // namespace operations_research
