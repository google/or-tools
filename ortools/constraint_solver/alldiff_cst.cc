// Copyright 2010-2017 Google
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

//
//  AllDifferent constraints

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/stringprintf.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/util/string_array.h"

namespace operations_research {
namespace {

class BaseAllDifferent : public Constraint {
 public:
  BaseAllDifferent(Solver* const s, const std::vector<IntVar*>& vars)
      : Constraint(s), vars_(vars) {}
  ~BaseAllDifferent() override {}
  std::string DebugStringInternal(const std::string& name) const {
    return StringPrintf("%s(%s)", name.c_str(),
                        JoinDebugStringPtr(vars_, ", ").c_str());
  }

 protected:
  const std::vector<IntVar*> vars_;
  int64 size() const { return vars_.size(); }
};

//-----------------------------------------------------------------------------
// ValueAllDifferent

class ValueAllDifferent : public BaseAllDifferent {
 public:
  ValueAllDifferent(Solver* const s, const std::vector<IntVar*>& vars)
      : BaseAllDifferent(s, vars) {}
  ~ValueAllDifferent() override {}

  void Post() override;
  void InitialPropagate() override;
  void OneMove(int index);
  bool AllMoves();

  std::string DebugString() const override {
    return DebugStringInternal("ValueAllDifferent");
  }
  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kAllDifferent, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               vars_);
    visitor->VisitIntegerArgument(ModelVisitor::kRangeArgument, 0);
    visitor->EndVisitConstraint(ModelVisitor::kAllDifferent, this);
  }

 private:
  RevSwitch all_instantiated_;
};

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
    const int64 val = vars_[index]->Value();
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
  std::unique_ptr<int64[]> values(new int64[size()]);
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

// ---------- Bounds All Different ----------
// See http://www.cs.uwaterloo.ca/~cquimper/Papers/ijcai03_TR.pdf for details.

class RangeBipartiteMatching {
 public:
  struct Interval {
    int64 min;
    int64 max;
    int min_rank;
    int max_rank;
  };

  RangeBipartiteMatching(Solver* const solver, int size)
      : solver_(solver),
        size_(size),
        intervals_(new Interval[size + 1]),
        min_sorted_(new Interval* [size]),
        max_sorted_(new Interval* [size]),
        bounds_(new int64[2 * size + 2]),
        tree_(new int[2 * size + 2]),
        diff_(new int64[2 * size + 2]),
        hall_(new int[2 * size + 2]),
        active_size_(0) {
    for (int i = 0; i < size; ++i) {
      max_sorted_[i] = &intervals_[i];
      min_sorted_[i] = max_sorted_[i];
    }
  }

  void SetRange(int index, int64 imin, int64 imax) {
    intervals_[index].min = imin;
    intervals_[index].max = imax;
  }

  bool Propagate() {
    SortArray();

    const bool modified1 = PropagateMin();
    const bool modified2 = PropagateMax();
    return modified1 || modified2;
  }

  int64 Min(int index) const { return intervals_[index].min; }

  int64 Max(int index) const { return intervals_[index].max; }

 private:
  // This method sorts the min_sorted_ and max_sorted_ arrays and fill
  // the bounds_ array (and set the active_size_ counter).
  void SortArray() {
    std::sort(min_sorted_.get(), min_sorted_.get() + size_,
              CompareIntervalMin());
    std::sort(max_sorted_.get(), max_sorted_.get() + size_,
              CompareIntervalMax());

    int64 min = min_sorted_[0]->min;
    int64 max = max_sorted_[0]->max + 1;
    int64 last = min - 2;
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

  // These two methods will actually do the new bounds computation.
  bool PropagateMin() {
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

  bool PropagateMax() {
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

  // TODO(user) : use better sort, use bounding boxes of modifications to
  //                 improve the sorting (only modified vars).

  // This method is used by the STL sort.
  struct CompareIntervalMin {
    bool operator()(const Interval* i1, const Interval* i2) const {
      return (i1->min < i2->min);
    }
  };

  // This method is used by the STL sort.
  struct CompareIntervalMax {
    bool operator()(const Interval* i1, const Interval* i2) const {
      return (i1->max < i2->max);
    }
  };

  void PathSet(int start, int end, int to, int* const tree) {
    int l = start;
    while (l != end) {
      int k = l;
      l = tree[k];
      tree[k] = to;
    }
  }

  int PathMin(const int* const tree, int index) {
    int i = index;
    while (tree[i] < i) {
      i = tree[i];
    }
    return i;
  }

  int PathMax(const int* const tree, int index) {
    int i = index;
    while (tree[i] > i) {
      i = tree[i];
    }
    return i;
  }

  Solver* const solver_;
  const int size_;
  std::unique_ptr<Interval[]> intervals_;
  std::unique_ptr<Interval* []> min_sorted_;
  std::unique_ptr<Interval* []> max_sorted_;
  // bounds_[1..active_size_] hold set of min & max in the n intervals_
  // while bounds_[0] and bounds_[active_size_ + 1] allow sentinels.
  std::unique_ptr<int64[]> bounds_;
  std::unique_ptr<int[]> tree_;    // tree links.
  std::unique_ptr<int64[]> diff_;  // diffs between critical capacities.
  std::unique_ptr<int[]> hall_;    // hall interval links.
  int active_size_;
};

class BoundsAllDifferent : public BaseAllDifferent {
 public:
  BoundsAllDifferent(Solver* const s, const std::vector<IntVar*>& vars)
      : BaseAllDifferent(s, vars), matching_(s, vars.size()) {}

  ~BoundsAllDifferent() override {}

  void Post() override {
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

  void InitialPropagate() override {
    IncrementalPropagate();
    for (int i = 0; i < size(); ++i) {
      if (vars_[i]->Bound()) {
        PropagateValue(i);
      }
    }
  }

  virtual void IncrementalPropagate() {
    for (int i = 0; i < size(); ++i) {
      matching_.SetRange(i, vars_[i]->Min(), vars_[i]->Max());
    }

    if (matching_.Propagate()) {
      for (int i = 0; i < size(); ++i) {
        vars_[i]->SetRange(matching_.Min(i), matching_.Max(i));
      }
    }
  }

  void PropagateValue(int index) {
    const int64 to_remove = vars_[index]->Value();
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

  std::string DebugString() const override {
    return DebugStringInternal("BoundsAllDifferent");
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kAllDifferent, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               vars_);
    visitor->VisitIntegerArgument(ModelVisitor::kRangeArgument, 1);
    visitor->EndVisitConstraint(ModelVisitor::kAllDifferent, this);
  }

 private:
  RangeBipartiteMatching matching_;
};

class SortConstraint : public Constraint {
 public:
  SortConstraint(Solver* const solver,
                 const std::vector<IntVar*>& original_vars,
                 const std::vector<IntVar*>& sorted_vars)
      : Constraint(solver),
        ovars_(original_vars),
        svars_(sorted_vars),
        mins_(original_vars.size(), 0),
        maxs_(original_vars.size(), 0),
        matching_(solver, original_vars.size()) {}

  ~SortConstraint() override {}

  void Post() override {
    Demon* const demon =
        solver()->MakeDelayedConstraintInitialPropagateCallback(this);
    for (int i = 0; i < size(); ++i) {
      ovars_[i]->WhenRange(demon);
      svars_[i]->WhenRange(demon);
    }
  }

  void InitialPropagate() override {
    for (int i = 0; i < size(); ++i) {
      int64 vmin = 0;
      int64 vmax = 0;
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
      int64 imin = 0;
      int64 imax = 0;
      FindIntersectionRange(i, &imin, &imax);
      matching_.SetRange(i, imin, imax);
    }
    matching_.Propagate();
    for (int i = 0; i < size(); ++i) {
      const int64 vmin = svars_[matching_.Min(i)]->Min();
      const int64 vmax = svars_[matching_.Max(i)]->Max();
      ovars_[i]->SetRange(vmin, vmax);
    }
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kSortingConstraint, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               ovars_);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kTargetArgument,
                                               svars_);
    visitor->EndVisitConstraint(ModelVisitor::kSortingConstraint, this);
  }

  std::string DebugString() const override {
    return StringPrintf("Sort(%s, %s)",
                        JoinDebugStringPtr(ovars_, ", ").c_str(),
                        JoinDebugStringPtr(svars_, ", ").c_str());
  }

 private:
  int64 size() const { return ovars_.size(); }

  void FindIntersectionRange(int index, int64* const range_min,
                             int64* const range_max) const {
    // Naive version.
    // TODO(user): Implement log(n) version.
    int64 imin = 0;
    while (imin < size() && NotIntersect(index, imin)) {
      imin++;
    }
    if (imin == size()) {
      solver()->Fail();
    }
    int64 imax = size() - 1;
    while (imax > imin && NotIntersect(index, imax)) {
      imax--;
    }
    *range_min = imin;
    *range_max = imax;
  }

  bool NotIntersect(int oindex, int sindex) const {
    return ovars_[oindex]->Min() > svars_[sindex]->Max() ||
           ovars_[oindex]->Max() < svars_[sindex]->Min();
  }

  const std::vector<IntVar*> ovars_;
  const std::vector<IntVar*> svars_;
  std::vector<int64> mins_;
  std::vector<int64> maxs_;
  RangeBipartiteMatching matching_;
};

// All variables are pairwise different, unless they are assigned to
// the escape value.
class AllDifferentExcept : public Constraint {
 public:
  AllDifferentExcept(Solver* const s, std::vector<IntVar*> vars,
                     int64 escape_value)
      : Constraint(s), vars_(std::move(vars)), escape_value_(escape_value) {}

  ~AllDifferentExcept() override {}

  void Post() override {
    for (int i = 0; i < vars_.size(); ++i) {
      IntVar* const var = vars_[i];
      Demon* const d = MakeConstraintDemon1(
          solver(), this, &AllDifferentExcept::Propagate, "Propagate", i);
      var->WhenBound(d);
    }
  }

  void InitialPropagate() override {
    for (int i = 0; i < vars_.size(); ++i) {
      if (vars_[i]->Bound()) {
        Propagate(i);
      }
    }
  }

  void Propagate(int index) {
    const int64 val = vars_[index]->Value();
    if (val != escape_value_) {
      for (int j = 0; j < vars_.size(); ++j) {
        if (index != j) {
          vars_[j]->RemoveValue(val);
        }
      }
    }
  }

  std::string DebugString() const override {
    return StringPrintf("AllDifferentExcept([%s], %" GG_LL_FORMAT "d",
                        JoinDebugStringPtr(vars_, ", ").c_str(), escape_value_);
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kAllDifferent, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               vars_);
    visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, escape_value_);
    visitor->EndVisitConstraint(ModelVisitor::kAllDifferent, this);
  }

 private:
  std::vector<IntVar*> vars_;
  const int64 escape_value_;
};

// Creates a constraint that states that all variables in the first
// vector are different from all variables from the second group,
// unless they are assigned to the escape value if it is defined. Thus
// the set of values in the first vector minus the escape value does
// not intersect the set of values in the second vector.
class NullIntersectArrayExcept : public Constraint {
 public:
  NullIntersectArrayExcept(Solver* const s, std::vector<IntVar*> first_vars,
                           std::vector<IntVar*> second_vars, int64 escape_value)
      : Constraint(s),
        first_vars_(std::move(first_vars)),
        second_vars_(std::move(second_vars)),
        escape_value_(escape_value),
        has_escape_value_(true) {}

  NullIntersectArrayExcept(Solver* const s, std::vector<IntVar*> first_vars,
                           std::vector<IntVar*> second_vars)
      : Constraint(s),
        first_vars_(std::move(first_vars)),
        second_vars_(std::move(second_vars)),
        escape_value_(0),
        has_escape_value_(false) {}

  ~NullIntersectArrayExcept() override {}

  void Post() override {
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

  void InitialPropagate() override {
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

  void PropagateFirst(int index) {
    const int64 val = first_vars_[index]->Value();
    if (!has_escape_value_ || val != escape_value_) {
      for (int j = 0; j < second_vars_.size(); ++j) {
        second_vars_[j]->RemoveValue(val);
      }
    }
  }

  void PropagateSecond(int index) {
    const int64 val = second_vars_[index]->Value();
    if (!has_escape_value_ || val != escape_value_) {
      for (int j = 0; j < first_vars_.size(); ++j) {
        first_vars_[j]->RemoveValue(val);
      }
    }
  }

  std::string DebugString() const override {
    return StringPrintf(
        "NullIntersectArray([%s], [%s], escape = %" GG_LL_FORMAT "d",
        JoinDebugStringPtr(first_vars_, ", ").c_str(),
        JoinDebugStringPtr(second_vars_, ", ").c_str(), escape_value_);
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kNullIntersect, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kLeftArgument,
                                               first_vars_);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kRightArgument,
                                               second_vars_);
    visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, escape_value_);
    visitor->EndVisitConstraint(ModelVisitor::kNullIntersect, this);
  }

 private:
  std::vector<IntVar*> first_vars_;
  std::vector<IntVar*> second_vars_;
  const int64 escape_value_;
  const bool has_escape_value_;
};
}  // namespace

Constraint* Solver::MakeAllDifferent(const std::vector<IntVar*>& vars) {
  return MakeAllDifferent(vars, true);
}

Constraint* Solver::MakeAllDifferent(const std::vector<IntVar*>& vars,
                                     bool stronger_propagation) {
  const int size = vars.size();
  for (int i = 0; i < size; ++i) {
    CHECK_EQ(this, vars[i]->solver());
  }
  if (size < 2) {
    return MakeTrueConstraint();
  } else if (size == 2) {
    return MakeNonEquality(const_cast<IntVar* const>(vars[0]),
                           const_cast<IntVar* const>(vars[1]));
  } else {
    if (stronger_propagation) {
      return RevAlloc(new BoundsAllDifferent(this, vars));
    } else {
      return RevAlloc(new ValueAllDifferent(this, vars));
    }
  }
}

Constraint* Solver::MakeSortingConstraint(const std::vector<IntVar*>& vars,
                                          const std::vector<IntVar*>& sorted) {
  CHECK_EQ(vars.size(), sorted.size());
  return RevAlloc(new SortConstraint(this, vars, sorted));
}

Constraint* Solver::MakeAllDifferentExcept(const std::vector<IntVar*>& vars,
                                           int64 escape_value) {
  int escape_candidates = 0;
  for (int i = 0; i < vars.size(); ++i) {
    escape_candidates += (vars[i]->Contains(escape_value));
  }
  if (escape_candidates <= 1) {
    return MakeAllDifferent(vars);
  } else {
    return RevAlloc(new AllDifferentExcept(this, vars, escape_value));
  }
}

Constraint* Solver::MakeNullIntersect(const std::vector<IntVar*>& first_vars,
                                      const std::vector<IntVar*>& second_vars) {
  return RevAlloc(new NullIntersectArrayExcept(this, first_vars, second_vars));
}

Constraint* Solver::MakeNullIntersectExcept(
    const std::vector<IntVar*>& first_vars,
    const std::vector<IntVar*>& second_vars, int64 escape_value) {
  int first_escape_candidates = 0;
  for (int i = 0; i < first_vars.size(); ++i) {
    first_escape_candidates += (first_vars[i]->Contains(escape_value));
  }
  int second_escape_candidates = 0;
  for (int i = 0; i < second_vars.size(); ++i) {
    second_escape_candidates += (second_vars[i]->Contains(escape_value));
  }
  if (first_escape_candidates == 0 || second_escape_candidates == 0) {
    return RevAlloc(
        new NullIntersectArrayExcept(this, first_vars, second_vars));
  } else {
    return RevAlloc(new NullIntersectArrayExcept(this, first_vars, second_vars,
                                                 escape_value));
  }
}
}  // namespace operations_research
