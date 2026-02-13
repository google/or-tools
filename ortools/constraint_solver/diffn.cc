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

#include "ortools/constraint_solver/diffn.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraints.h"
#include "ortools/constraint_solver/utilities.h"
#include "ortools/util/string_array.h"

namespace operations_research {

void Diffn::Post() {
  Solver* s = solver();
  for (int i = 0; i < size_; ++i) {
    Demon* demon = MakeConstraintDemon1(s, this, &Diffn::OnBoxRangeChange,
                                        "OnBoxRangeChange", i);
    x_[i]->WhenRange(demon);
    y_[i]->WhenRange(demon);
    dx_[i]->WhenRange(demon);
    dy_[i]->WhenRange(demon);
  }
  delayed_demon_ = MakeDelayedConstraintDemon0(s, this, &Diffn::PropagateAll,
                                               "PropagateAll");
  if (solver()->parameters().diffn_use_cumulative() &&
      IsArrayInRange<int64_t>(x_, 0, std::numeric_limits<int64_t>::max()) &&
      IsArrayInRange<int64_t>(y_, 0, std::numeric_limits<int64_t>::max())) {
    Constraint* ct1 = nullptr;
    Constraint* ct2 = nullptr;
    {
      // We can add redundant cumulative constraints.  This is done
      // inside a c++ block to avoid leaking memory if adding the
      // constraints leads to a failure. A cumulative constraint is
      // a scheduling constraint that will perform finer energy
      // based reasoning to do more propagation. (see Solver::MakeCumulative).
      const int64_t min_x = MinVarArray(x_);
      const int64_t max_x = MaxVarArray(x_);
      const int64_t max_size_x = MaxVarArray(dx_);
      const int64_t min_y = MinVarArray(y_);
      const int64_t max_y = MaxVarArray(y_);
      const int64_t max_size_y = MaxVarArray(dy_);
      if (AreAllBound(dx_)) {
        std::vector<int64_t> size_x;
        FillValues(dx_, &size_x);
        ct1 = MakeCumulativeConstraint(x_, size_x, dy_,
                                       max_size_y + max_y - min_y);
      }
      if (AreAllBound(dy_)) {
        std::vector<int64_t> size_y;
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

void Diffn::InitialPropagate() {
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

std::string Diffn::DebugString() const {
  return absl::StrFormat(
      "Diffn(x = [%s], y = [%s], dx = [%s], dy = [%s]))",
      JoinDebugStringPtr(x_, ", "), JoinDebugStringPtr(y_, ", "),
      JoinDebugStringPtr(dx_, ", "), JoinDebugStringPtr(dy_, ", "));
}

void Diffn::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kDisjunctive, this);
  visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kPositionXArgument,
                                             x_);
  visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kPositionYArgument,
                                             y_);
  visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kSizeXArgument, dx_);
  visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kSizeYArgument, dy_);
  visitor->EndVisitConstraint(ModelVisitor::kDisjunctive, this);
}

void Diffn::PropagateAll() {
  for (const int box : to_propagate_) {
    FillNeighbors(box);
    FailWhenEnergyIsTooLarge(box);
    PushOverlappingBoxes(box);
  }
  to_propagate_.clear();
  fail_stamp_ = solver()->fail_stamp();
}

void Diffn::OnBoxRangeChange(int box) {
  if (solver()->fail_stamp() > fail_stamp_ && !to_propagate_.empty()) {
    // We have failed in the last propagation and the to_propagate_
    // was not cleared.
    fail_stamp_ = solver()->fail_stamp();
    to_propagate_.clear();
  }
  to_propagate_.insert(box);
  EnqueueDelayedDemon(delayed_demon_);
}

bool Diffn::CanBoxedOverlap(int i, int j) const {
  if (AreBoxedDisjoingHorizontallyForSure(i, j) ||
      AreBoxedDisjoingVerticallyForSure(i, j)) {
    return false;
  }
  return true;
}

bool Diffn::AreBoxedDisjoingHorizontallyForSure(int i, int j) const {
  return (x_[i]->Min() >= x_[j]->Max() + dx_[j]->Max()) ||
         (x_[j]->Min() >= x_[i]->Max() + dx_[i]->Max()) ||
         (!strict_ && (dx_[i]->Min() == 0 || dx_[j]->Min() == 0));
}

bool Diffn::AreBoxedDisjoingVerticallyForSure(int i, int j) const {
  return (y_[i]->Min() >= y_[j]->Max() + dy_[j]->Max()) ||
         (y_[j]->Min() >= y_[i]->Max() + dy_[i]->Max()) ||
         (!strict_ && (dy_[i]->Min() == 0 || dy_[j]->Min() == 0));
}

void Diffn::FillNeighbors(int box) {
  // TODO(user): We could maintain a non reversible list of
  // neighbors and clean it after each failure.
  neighbors_.clear();
  for (int other = 0; other < size_; ++other) {
    if (other != box && CanBoxedOverlap(other, box)) {
      neighbors_.push_back(other);
    }
  }
}

void Diffn::FailWhenEnergyIsTooLarge(int box) {
  int64_t area_min_x = x_[box]->Min();
  int64_t area_max_x = x_[box]->Max() + dx_[box]->Max();
  int64_t area_min_y = y_[box]->Min();
  int64_t area_max_y = y_[box]->Max() + dy_[box]->Max();
  int64_t sum_of_areas = dx_[box]->Min() * dy_[box]->Min();
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
    const int64_t bounding_area =
        (area_max_x - area_min_x) * (area_max_y - area_min_y);
    if (sum_of_areas > bounding_area) {
      solver()->Fail();
    }
  }
}

void Diffn::PushOverlappingBoxes(int box) {
  for (int i = 0; i < neighbors_.size(); ++i) {
    PushOneBox(box, neighbors_[i]);
  }
}

void Diffn::PushOneBox(int box, int other) {
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
    default: {
      break;
    }
  }
}

Constraint* Diffn::MakeCumulativeConstraint(
    const std::vector<IntVar*>& positions, absl::Span<const int64_t> sizes,
    const std::vector<IntVar*>& demands, int64_t capacity) {
  std::vector<IntervalVar*> intervals;
  solver()->MakeFixedDurationIntervalVarArray(positions, sizes, "interval",
                                              &intervals);
  return solver()->MakeCumulative(intervals, demands, capacity, "cumul");
}

}  // namespace operations_research
