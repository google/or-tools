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

#ifndef ORTOOLS_CONSTRAINT_SOLVER_ALLDIFF_CST_H_
#define ORTOOLS_CONSTRAINT_SOLVER_ALLDIFF_CST_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/utilities.h"

namespace operations_research {

class BaseAllDifferent : public Constraint {
 public:
  BaseAllDifferent(Solver* s, const std::vector<IntVar*>& vars)
      : Constraint(s), vars_(vars) {}
  ~BaseAllDifferent() override {}
  std::string DebugStringInternal(absl::string_view name) const;

 protected:
  const std::vector<IntVar*> vars_;
  int64_t size() const { return vars_.size(); }
};

//-----------------------------------------------------------------------------
// ValueAllDifferent

class ValueAllDifferent : public BaseAllDifferent {
 public:
  ValueAllDifferent(Solver* s, const std::vector<IntVar*>& vars)
      : BaseAllDifferent(s, vars) {}
  ~ValueAllDifferent() override {}

  void Post() override;
  void InitialPropagate() override;
  void OneMove(int index);
  bool AllMoves();

  std::string DebugString() const override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  RevSwitch all_instantiated_;
};

// ---------- Bounds All Different ----------
// See http://www.cs.uwaterloo.ca/~cquimper/Papers/ijcai03_TR.pdf for details.

class RangeBipartiteMatching {
 public:
  struct Interval {
    int64_t min;
    int64_t max;
    int min_rank;
    int max_rank;
  };

  RangeBipartiteMatching(Solver* solver, int size);

  void SetRange(int index, int64_t imin, int64_t imax);

  bool Propagate();

  int64_t Min(int index) const;

  int64_t Max(int index) const;

 private:
  // This method sorts the min_sorted_ and max_sorted_ arrays and fill
  // the bounds_ array (and set the active_size_ counter).
  void SortArray();

  // These two methods will actually do the new bounds computation.
  bool PropagateMin();
  bool PropagateMax();

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

  void PathSet(int start, int end, int to, int* tree);

  int PathMin(const int* tree, int index);

  int PathMax(const int* tree, int index);

  Solver* const solver_;
  const int size_;
  std::unique_ptr<Interval[]> intervals_;
  std::unique_ptr<Interval*[]> min_sorted_;
  std::unique_ptr<Interval*[]> max_sorted_;
  // bounds_[1..active_size_] hold set of min & max in the n intervals_
  // while bounds_[0] and bounds_[active_size_ + 1] allow sentinels.
  std::unique_ptr<int64_t[]> bounds_;
  std::unique_ptr<int[]> tree_;      // tree links.
  std::unique_ptr<int64_t[]> diff_;  // diffs between critical capacities.
  std::unique_ptr<int[]> hall_;      // hall interval links.
  int active_size_;
};

class BoundsAllDifferent : public BaseAllDifferent {
 public:
  BoundsAllDifferent(Solver* s, const std::vector<IntVar*>& vars)
      : BaseAllDifferent(s, vars), matching_(s, vars.size()) {}

  ~BoundsAllDifferent() override {}

  void Post() override;

  void InitialPropagate() override;

  virtual void IncrementalPropagate();

  void PropagateValue(int index);

  std::string DebugString() const override;

  void Accept(ModelVisitor* visitor) const override;

 private:
  RangeBipartiteMatching matching_;
};

class SortConstraint : public Constraint {
 public:
  SortConstraint(Solver* solver, const std::vector<IntVar*>& original_vars,
                 const std::vector<IntVar*>& sorted_vars)
      : Constraint(solver),
        ovars_(original_vars),
        svars_(sorted_vars),
        mins_(original_vars.size(), 0),
        maxs_(original_vars.size(), 0),
        matching_(solver, original_vars.size()) {}

  ~SortConstraint() override {}

  void Post() override;

  void InitialPropagate() override;

  void Accept(ModelVisitor* visitor) const override;

  std::string DebugString() const override;

 private:
  int64_t size() const { return ovars_.size(); }

  void FindIntersectionRange(int index, int64_t* range_min,
                             int64_t* range_max) const;

  bool NotIntersect(int oindex, int sindex) const;

  const std::vector<IntVar*> ovars_;
  const std::vector<IntVar*> svars_;
  std::vector<int64_t> mins_;
  std::vector<int64_t> maxs_;
  RangeBipartiteMatching matching_;
};

// All variables are pairwise different, unless they are assigned to
// the escape value.
class AllDifferentExcept : public Constraint {
 public:
  AllDifferentExcept(Solver* s, std::vector<IntVar*> vars, int64_t escape_value)
      : Constraint(s), vars_(std::move(vars)), escape_value_(escape_value) {}

  ~AllDifferentExcept() override {}

  void Post() override;

  void InitialPropagate() override;

  void Propagate(int index);

  std::string DebugString() const override;

  void Accept(ModelVisitor* visitor) const override;

 private:
  std::vector<IntVar*> vars_;
  const int64_t escape_value_;
};

// Creates a constraint that states that all variables in the first
// vector are different from all variables from the second group,
// unless they are assigned to the escape value if it is defined. Thus
// the set of values in the first vector minus the escape value does
// not intersect the set of values in the second vector.
class NullIntersectArrayExcept : public Constraint {
 public:
  NullIntersectArrayExcept(Solver* s, std::vector<IntVar*> first_vars,
                           std::vector<IntVar*> second_vars,
                           int64_t escape_value)
      : Constraint(s),
        first_vars_(std::move(first_vars)),
        second_vars_(std::move(second_vars)),
        escape_value_(escape_value),
        has_escape_value_(true) {}

  NullIntersectArrayExcept(Solver* s, std::vector<IntVar*> first_vars,
                           std::vector<IntVar*> second_vars)
      : Constraint(s),
        first_vars_(std::move(first_vars)),
        second_vars_(std::move(second_vars)),
        escape_value_(0),
        has_escape_value_(false) {}

  ~NullIntersectArrayExcept() override {}

  void Post() override;

  void InitialPropagate() override;

  void PropagateFirst(int index);

  void PropagateSecond(int index);

  std::string DebugString() const override;

  void Accept(ModelVisitor* visitor) const override;

 private:
  std::vector<IntVar*> first_vars_;
  std::vector<IntVar*> second_vars_;
  const int64_t escape_value_;
  const bool has_escape_value_;
};

}  // namespace operations_research

#endif  // ORTOOLS_CONSTRAINT_SOLVER_ALLDIFF_CST_H_
