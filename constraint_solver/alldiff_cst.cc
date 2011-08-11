// Copyright 2010-2011 Google
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
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "constraint_solver/constraint_solveri.h"

namespace operations_research {
namespace {

class BaseAllDifferent : public Constraint {
 public:
  BaseAllDifferent(Solver* const s, const IntVar* const* vars, int size);
  ~BaseAllDifferent() {}
  string DebugStringInternal(const string& name) const;
 protected:
  scoped_array<IntVar*> vars_;
  int size_;
};

BaseAllDifferent::BaseAllDifferent(Solver* const s,
                                   const IntVar* const* vars,
                                   int size)
    : Constraint(s), vars_(NULL), size_(size) {
  CHECK_GE(size_, 0);
  if (size_ > 0) {
    vars_.reset(new IntVar*[size_]);
    memcpy(vars_.get(), vars, size_ * sizeof(*vars));
  }
}

string BaseAllDifferent::DebugStringInternal(const string& name) const {
  string out = name + "(";
  out.append(DebugStringArray(vars_.get(), size_, ", "));
  out.append(")");
  return out;
}


//-----------------------------------------------------------------------------
// ValueAllDifferent

class ValueAllDifferent : public BaseAllDifferent {
 public:
  ValueAllDifferent(Solver* const s, const IntVar* const* vars, int size);
  virtual ~ValueAllDifferent() {}

  virtual void Post();
  virtual void InitialPropagate();
  void OneMove(int index);
  bool AllMoves();

  virtual string DebugString() const;
  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kAllDifferent, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               vars_.get(),
                                               size_);
    visitor->VisitIntegerArgument(ModelVisitor::kRangeArgument, 0);
    visitor->EndVisitConstraint(ModelVisitor::kAllDifferent, this);
  }
 private:
  int checked_;
};

ValueAllDifferent::ValueAllDifferent(Solver* const s,
                                     const IntVar* const* vars,
                                     int size)
    : BaseAllDifferent(s, vars, size), checked_(0) {}

string ValueAllDifferent::DebugString() const {
  return DebugStringInternal("ValueAllDifferent");
}

void ValueAllDifferent::Post() {
  for (int i = 0; i < size_; ++i) {
    IntVar* var = vars_[i];
    Demon* d = MakeConstraintDemon1(solver(),
                                    this,
                                    &ValueAllDifferent::OneMove,
                                    "OneMove",
                                    i);
    var->WhenBound(d);
  }
}

void ValueAllDifferent::InitialPropagate() {
  for (int i = 0; i < size_; ++i) {
    if (vars_[i]->Bound()) {
      OneMove(i);
    }
  }
}

void ValueAllDifferent::OneMove(int index) {
  if (!AllMoves()) {
    const int64 val = vars_[index]->Value();
    for (int j = 0; j < size_; ++j) {
      if (index != j) {
        vars_[j]->RemoveValue(val);
      }
    }
  }
}

bool ValueAllDifferent::AllMoves() {
  if (checked_ || size_ == 0) {
    return true;
  }
  for (int i = 0; i < size_; ++i) {
    if (!vars_[i]->Bound()) {
      return false;
    }
  }
  scoped_array<int64> values(new int64[size_]);
  for (int i = 0; i < size_; ++i) {
    values[i] = vars_[i]->Value();
  }
  std::sort(values.get(), values.get() + size_);
  for (int i = 0; i < size_ - 1; ++i) {
    if (values[i] == values[i + 1]) {
      values.reset(NULL);   // prevent leaks (solver()->Fail() won't return)
      solver()->Fail();
    }
  }
  solver()->SaveAndSetValue(&checked_, 1);
  return true;
}

// ---------- Bounds All Different ----------
// See http://www.cs.uwaterloo.ca/~cquimper/Papers/ijcai03_TR.pdf for details.

struct Interval {
  int64 min;
  int64 max;
  int min_rank;
  int max_rank;
};

// TODO(user) : use better sort, use bounding boxes of modifications to
//                 improve the sorting (only modified vars).

// This method is used by the STL sort.
struct CompareIntervalMin {
  bool operator()(const Interval* i1, const Interval* i2) {
    return (i1->min < i2->min);
  }
};

// This method is used by the STL sort.
struct CompareIntervalMax {
  bool operator()(const Interval* i1, const Interval* i2) {
    return (i1->max < i2->max);
  }
};

void PathSet(int start, int end, int to, int* const tree) {
  int k = start;
  int l = start;
  while (l != end) {
    k = l;
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

class BoundsAllDifferent : public BaseAllDifferent {
 public:
  BoundsAllDifferent(Solver* const s, const IntVar* const * vars, int size)
      : BaseAllDifferent(s, vars, size),
        intervals_(new Interval[size + 1]),
        min_sorted_(new Interval*[size]),
        max_sorted_(new Interval*[size]),
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
  virtual ~BoundsAllDifferent() {}
  virtual void Post() {
    Demon* range = MakeDelayedConstraintDemon0(
        solver(),
        this,
        &BoundsAllDifferent::IncrementalPropagate,
        "IncrementalPropagate");

    for (int i = 0; i < size_; ++i) {
      vars_[i]->WhenRange(range);
      Demon* bound = MakeConstraintDemon1(solver(),
                                          this,
                                          &BoundsAllDifferent::PropagateValue,
                                          "PropagateValue",
                                          i);
      vars_[i]->WhenBound(bound);
    }
  }

  virtual void InitialPropagate() {
    IncrementalPropagate();
    for (int i = 0; i < size_; ++i) {
      if (vars_[i]->Bound()) {
        PropagateValue(i);
      }
    }
  }

  virtual void IncrementalPropagate() {
    for (int i = 0; i < size_; ++i) {
      intervals_[i].min = vars_[i]->Min();
      intervals_[i].max = vars_[i]->Max();
    }

    SortArray();

    bool modified = PropagateMin();
    modified |= PropagateMax();

    if (modified) {
      for (int i = 0; i < size_; ++i) {
        vars_[i]->SetRange(intervals_[i].min, intervals_[i].max);
      }
    }
  }

  void PropagateValue(int index) {
    const int64 to_remove = vars_[index]->Value();
    for (int j = 0; j < index; j++) {
      vars_[j]->RemoveValue(to_remove);
    }
    for (int j = index + 1; j < size_; j++) {
      vars_[j]->RemoveValue(to_remove);
    }
  }

  virtual string DebugString() const {
    return DebugStringInternal("BoundsAllDifferent");
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kAllDifferent, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               vars_.get(),
                                               size_);
    visitor->VisitIntegerArgument(ModelVisitor::kRangeArgument, 1);
    visitor->EndVisitConstraint(ModelVisitor::kAllDifferent, this);
  }

 private:
  // This method with sort the min_sorted_ and max_sorted_ arrays and fill
  // the bounds_ array (and set the active_size_ counter).
  void SortArray();

  // These two methods will actually do the new bounds computation.
  bool PropagateMin();
  bool PropagateMax();

  int64 stamp_;
  scoped_array<Interval> intervals_;
  scoped_array<Interval*> min_sorted_;
  scoped_array<Interval*> max_sorted_;
  // bounds_[1..active_size_] hold set of min & max in the n intervals_
  // while bounds_[0] and bounds_[active_size_ + 1] allow sentinels.
  scoped_array<int64> bounds_;
  scoped_array<int> tree_;              // tree links.
  scoped_array<int64> diff_;            // diffs between critical capacities.
  scoped_array<int> hall_;              // hall interval links.
  int active_size_;
};

void BoundsAllDifferent::SortArray() {
  std::sort(min_sorted_.get(), min_sorted_.get() + size_, CompareIntervalMin());
  std::sort(max_sorted_.get(), max_sorted_.get() + size_, CompareIntervalMax());

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

bool BoundsAllDifferent::PropagateMin() {
  bool modified = false;

  for (int i = 1; i <= active_size_ + 1; ++i) {
    hall_[i] = i - 1;
    tree_[i] = i - 1;
    diff_[i] = bounds_[i] - bounds_[i - 1];
  }
  for (int i = 0; i < size_; ++i) {  // visit intervals in increasing max order
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
      solver()->Fail();
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


bool BoundsAllDifferent::PropagateMax() {
  bool modified = false;

  for (int i = 0; i<= active_size_; i++) {
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
      solver()->Fail();
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
}  // namespace

Constraint* Solver::MakeAllDifferent(const std::vector<IntVar*>& vars, bool range) {
  return MakeAllDifferent(vars.data(), vars.size(), range);
}

Constraint* Solver::MakeAllDifferent(const IntVar* const* vars,
                                     int size,
                                     bool range) {
  for (int i = 0; i < size; ++i) {
    CHECK_EQ(this, vars[i]->solver());
  }
  if (size < 2) {
    return MakeTrueConstraint();
  } else if (size == 2) {
    return MakeNonEquality(const_cast<IntVar* const>(vars[0]),
                           const_cast<IntVar* const>(vars[1]));
  } else {
    if (range) {
      return RevAlloc(new BoundsAllDifferent(this, vars, size));
    } else {
      return RevAlloc(new ValueAllDifferent(this, vars, size));
    }
  }
}
}  // namespace operations_research
