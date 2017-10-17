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

#include <algorithm>
#include <string>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/stringprintf.h"
#include "ortools/base/int_type.h"
#include "ortools/base/int_type_indexed_vector.h"
#include "ortools/base/hash.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/util/string_array.h"


namespace operations_research {
// Diffn constraint, Non overlapping boxs.
namespace {
DEFINE_INT_TYPE(Box, int);
class Diffn : public Constraint {
 public:
  Diffn(Solver* const solver, const std::vector<IntVar*>& x_vars,
        const std::vector<IntVar*>& y_vars, const std::vector<IntVar*>& x_size,
        const std::vector<IntVar*>& y_size, bool strict)
      : Constraint(solver),
        x_(x_vars),
        y_(y_vars),
        dx_(x_size),
        dy_(y_size),
        strict_(strict),
        size_(x_vars.size()),
        fail_stamp_(0) {
    CHECK_EQ(x_vars.size(), y_vars.size());
    CHECK_EQ(x_vars.size(), x_size.size());
    CHECK_EQ(x_vars.size(), y_size.size());
  }

  ~Diffn() override {}

  void Post() override {
    Solver* const s = solver();
    for (int i = 0; i < size_; ++i) {
      Demon* const demon = MakeConstraintDemon1(
          s, this, &Diffn::OnBoxRangeChange, "OnBoxRangeChange", i);
      x_[i]->WhenRange(demon);
      y_[i]->WhenRange(demon);
      dx_[i]->WhenRange(demon);
      dy_[i]->WhenRange(demon);
    }
    delayed_demon_ = MakeDelayedConstraintDemon0(s, this, &Diffn::PropagateAll,
                                                 "PropagateAll");
    if (solver()->parameters().diffn_use_cumulative() &&
        IsArrayInRange(x_, 0LL, kint64max) &&
        IsArrayInRange(y_, 0LL, kint64max)) {
      Constraint* ct1 = nullptr;
      Constraint* ct2 = nullptr;
      {
        // We can add redundant cumulative constraints.  This is done
        // inside a c++ block to avoid leaking memory if adding the
        // constraints leads to a failure. A cumulative constraint is
        // a scheduling constraint that will perform finer energy
        // based reasoning to do more propagation. (see Solver::MakeCumulative).
        const int64 min_x = MinVarArray(x_);
        const int64 max_x = MaxVarArray(x_);
        const int64 max_size_x = MaxVarArray(dx_);
        const int64 min_y = MinVarArray(y_);
        const int64 max_y = MaxVarArray(y_);
        const int64 max_size_y = MaxVarArray(dy_);
        if (AreAllBound(dx_)) {
          std::vector<int64> size_x;
          FillValues(dx_, &size_x);
          ct1 = MakeCumulativeConstraint(x_, size_x, dy_,
                                         max_size_y + max_y - min_y);
        }
        if (AreAllBound(dy_)) {
          std::vector<int64> size_y;
          FillValues(dy_, &size_y);
          ct2 = MakeCumulativeConstraint(y_, size_y, dx_,
                                         max_size_x + max_x - min_x);
        }
      }
      if (ct1 != nullptr) {
        s->AddConstraint(ct1);
      }
      if (ct2 != nullptr) {
        s->AddConstraint(ct2);
      }
    }
  }

  void InitialPropagate() override {
    // All sizes should be >= 0.
    for (int i = 0; i < size_; ++i) {
      dx_[i]->SetMin(0);
      dy_[i]->SetMin(0);
    }

    // Force propagation on all boxes.
    to_propagate_.clear();
    for (int i = 0; i < size_; i++) {
      to_propagate_.insert(i);
    }
    PropagateAll();
  }

  std::string DebugString() const override {
    return StringPrintf("Diffn(x = [%s], y = [%s], dx = [%s], dy = [%s]))",
                        JoinDebugStringPtr(x_, ", ").c_str(),
                        JoinDebugStringPtr(y_, ", ").c_str(),
                        JoinDebugStringPtr(dx_, ", ").c_str(),
                        JoinDebugStringPtr(dy_, ", ").c_str());
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kDisjunctive, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kPositionXArgument,
                                               x_);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kPositionYArgument,
                                               y_);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kSizeXArgument,
                                               dx_);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kSizeYArgument,
                                               dy_);
    visitor->EndVisitConstraint(ModelVisitor::kDisjunctive, this);
  }

 private:
  void PropagateAll() {
    for (const int box : to_propagate_) {
      FillNeighbors(box);
      FailWhenEnergyIsTooLarge(box);
      PushOverlappingBoxes(box);
    }
    to_propagate_.clear();
    fail_stamp_ = solver()->fail_stamp();
  }

  void OnBoxRangeChange(int box) {
    if (solver()->fail_stamp() > fail_stamp_ && !to_propagate_.empty()) {
      // We have failed in the last propagation and the to_propagate_
      // was not cleared.
      fail_stamp_ = solver()->fail_stamp();
      to_propagate_.clear();
    }
    to_propagate_.insert(box);
    EnqueueDelayedDemon(delayed_demon_);
  }

  bool CanBoxedOverlap(int i, int j) const {
    if (AreBoxedDisjoingHorizontallyForSure(i, j) ||
        AreBoxedDisjoingVerticallyForSure(i, j)) {
      return false;
    }
    return true;
  }

  bool AreBoxedDisjoingHorizontallyForSure(int i, int j) const {
    return (x_[i]->Min() >= x_[j]->Max() + dx_[j]->Max()) ||
           (x_[j]->Min() >= x_[i]->Max() + dx_[i]->Max()) ||
           (!strict_ && (dx_[i]->Min() == 0 || dx_[j]->Min() == 0));
  }

  bool AreBoxedDisjoingVerticallyForSure(int i, int j) const {
    return (y_[i]->Min() >= y_[j]->Max() + dy_[j]->Max()) ||
           (y_[j]->Min() >= y_[i]->Max() + dy_[i]->Max()) ||
           (!strict_ && (dy_[i]->Min() == 0 || dy_[j]->Min() == 0));
  }

  // Fill neighbors_ with all boxes that can overlap the given box.
  void FillNeighbors(int box) {
    // TODO(user): We could maintain a non reversible list of
    // neighbors and clean it after each failure.
    neighbors_.clear();
    for (int other = 0; other < size_; ++other) {
      if (other != box && CanBoxedOverlap(other, box)) {
        neighbors_.push_back(other);
      }
    }
  }

  // Fails if the minimum area of the given box plus the area of its neighbors
  // (that must already be computed in neighbors_) is greater than the area of a
  // bounding box that necessarily contains all these boxes.
  void FailWhenEnergyIsTooLarge(int box) {
    int64 area_min_x = x_[box]->Min();
    int64 area_max_x = x_[box]->Max() + dx_[box]->Max();
    int64 area_min_y = y_[box]->Min();
    int64 area_max_y = y_[box]->Max() + dy_[box]->Max();
    int64 sum_of_areas = dx_[box]->Min() * dy_[box]->Min();
    // TODO(user): Is there a better order, maybe sort by distance
    // with the current box.
    for (int i = 0; i < neighbors_.size(); ++i) {
      const int other = neighbors_[i];
      // Update Bounding box.
      area_min_x = std::min(area_min_x, x_[other]->Min());
      area_max_x = std::max(area_max_x, x_[other]->Max() + dx_[other]->Max());
      area_min_y = std::min(area_min_y, y_[other]->Min());
      area_max_y = std::max(area_max_y, y_[other]->Max() + dy_[other]->Max());
      // Update sum of areas.
      sum_of_areas += dx_[other]->Min() * dy_[other]->Min();
      const int64 bounding_area =
          (area_max_x - area_min_x) * (area_max_y - area_min_y);
      if (sum_of_areas > bounding_area) {
        solver()->Fail();
      }
    }
  }

  // Changes the domain of all the neighbors of a given box (that must
  // already be computed in neighbors_) so that they can't overlap the
  // mandatory part of the given box.
  void PushOverlappingBoxes(int box) {
    for (int i = 0; i < neighbors_.size(); ++i) {
      PushOneBox(box, neighbors_[i]);
    }
  }

  // Changes the domain of the two given box by excluding the value that
  // make them overlap for sure. Note that this function is symmetric in
  // the sense that its argument can be swapped for the same result.
  void PushOneBox(int box, int other) {
    const int state =
        (x_[box]->Min() + dx_[box]->Min() <= x_[other]->Max()) +
        2 * (x_[other]->Min() + dx_[other]->Min() <= x_[box]->Max()) +
        4 * (y_[box]->Min() + dy_[box]->Min() <= y_[other]->Max()) +
        8 * (y_[other]->Min() + dy_[other]->Min() <= y_[box]->Max());
    // This is an "hack" to be able to easily test for none or for one
    // and only one of the conditions below.
    switch (state) {
      case 0: {
        solver()->Fail();
        break;
      }
      case 1: {  // We push other left (x increasing).
        x_[other]->SetMin(x_[box]->Min() + dx_[box]->Min());
        x_[box]->SetMax(x_[other]->Max() - dx_[box]->Min());
        dx_[box]->SetMax(x_[other]->Max() - x_[box]->Min());
        break;
      }
      case 2: {  // We push other right (x decreasing).
        x_[box]->SetMin(x_[other]->Min() + dx_[other]->Min());
        x_[other]->SetMax(x_[box]->Max() - dx_[other]->Min());
        dx_[other]->SetMax(x_[box]->Max() - x_[other]->Min());
        break;
      }
      case 4: {  // We push other up (y increasing).
        y_[other]->SetMin(y_[box]->Min() + dy_[box]->Min());
        y_[box]->SetMax(y_[other]->Max() - dy_[box]->Min());
        dy_[box]->SetMax(y_[other]->Max() - y_[box]->Min());
        break;
      }
      case 8: {  // We push other down (y decreasing).
        y_[box]->SetMin(y_[other]->Min() + dy_[other]->Min());
        y_[other]->SetMax(y_[box]->Max() - dy_[other]->Min());
        dy_[other]->SetMax(y_[box]->Max() - y_[other]->Min());
        break;
      }
      default: { break; }
    }
  }

  Constraint* MakeCumulativeConstraint(const std::vector<IntVar*>& positions,
                                       const std::vector<int64>& sizes,
                                       const std::vector<IntVar*>& demands,
                                       int64 capacity) {
    std::vector<IntervalVar*> intervals;
    solver()->MakeFixedDurationIntervalVarArray(positions, sizes, "interval",
                                                &intervals);
    return solver()->MakeCumulative(intervals, demands, capacity, "cumul");
  }

  std::vector<IntVar*> x_;
  std::vector<IntVar*> y_;
  std::vector<IntVar*> dx_;
  std::vector<IntVar*> dy_;
  const bool strict_;
  const int64 size_;
  Demon* delayed_demon_;
  std::unordered_set<int> to_propagate_;
  std::vector<int> neighbors_;
  uint64 fail_stamp_;
};
}  // namespace

Constraint* Solver::MakeNonOverlappingBoxesConstraint(
    const std::vector<IntVar*>& x_vars, const std::vector<IntVar*>& y_vars,
    const std::vector<IntVar*>& x_size, const std::vector<IntVar*>& y_size) {
  return RevAlloc(new Diffn(this, x_vars, y_vars, x_size, y_size, true));
}

Constraint* Solver::MakeNonOverlappingBoxesConstraint(
    const std::vector<IntVar*>& x_vars, const std::vector<IntVar*>& y_vars,
    const std::vector<int64>& x_size, const std::vector<int64>& y_size) {
  std::vector<IntVar*> dx(x_size.size());
  std::vector<IntVar*> dy(y_size.size());
  for (int i = 0; i < x_size.size(); ++i) {
    dx[i] = MakeIntConst(x_size[i]);
    dy[i] = MakeIntConst(y_size[i]);
  }
  return RevAlloc(new Diffn(this, x_vars, y_vars, dx, dy, true));
}

Constraint* Solver::MakeNonOverlappingBoxesConstraint(
    const std::vector<IntVar*>& x_vars, const std::vector<IntVar*>& y_vars,
    const std::vector<int>& x_size, const std::vector<int>& y_size) {
  std::vector<IntVar*> dx(x_size.size());
  std::vector<IntVar*> dy(y_size.size());
  for (int i = 0; i < x_size.size(); ++i) {
    dx[i] = MakeIntConst(x_size[i]);
    dy[i] = MakeIntConst(y_size[i]);
  }
  return RevAlloc(new Diffn(this, x_vars, y_vars, dx, dy, true));
}

Constraint* Solver::MakeNonOverlappingNonStrictBoxesConstraint(
    const std::vector<IntVar*>& x_vars, const std::vector<IntVar*>& y_vars,
    const std::vector<IntVar*>& x_size, const std::vector<IntVar*>& y_size) {
  return RevAlloc(new Diffn(this, x_vars, y_vars, x_size, y_size, false));
}

Constraint* Solver::MakeNonOverlappingNonStrictBoxesConstraint(
    const std::vector<IntVar*>& x_vars, const std::vector<IntVar*>& y_vars,
    const std::vector<int64>& x_size, const std::vector<int64>& y_size) {
  std::vector<IntVar*> dx(x_size.size());
  std::vector<IntVar*> dy(y_size.size());
  for (int i = 0; i < x_size.size(); ++i) {
    dx[i] = MakeIntConst(x_size[i]);
    dy[i] = MakeIntConst(y_size[i]);
  }
  return RevAlloc(new Diffn(this, x_vars, y_vars, dx, dy, false));
}

Constraint* Solver::MakeNonOverlappingNonStrictBoxesConstraint(
    const std::vector<IntVar*>& x_vars, const std::vector<IntVar*>& y_vars,
    const std::vector<int>& x_size, const std::vector<int>& y_size) {
  std::vector<IntVar*> dx(x_size.size());
  std::vector<IntVar*> dy(y_size.size());
  for (int i = 0; i < x_size.size(); ++i) {
    dx[i] = MakeIntConst(x_size[i]);
    dy[i] = MakeIntConst(y_size[i]);
  }
  return RevAlloc(new Diffn(this, x_vars, y_vars, dx, dy, false));
}
}  // namespace operations_research
