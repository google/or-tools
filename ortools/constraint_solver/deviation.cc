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
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/stringprintf.h"
#include "ortools/base/mathutil.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/util/string_array.h"

namespace operations_research {
// Deviation Constraint, a constraint for the average absolute
// deviation to the mean.  See paper: Bound Consistent Deviation
// Constraint, Pierre Schaus et. al., CP07
namespace {
class Deviation : public Constraint {
 public:
  Deviation(Solver* const solver, const std::vector<IntVar*>& vars,
            IntVar* const deviation_var, int64 total_sum)
      : Constraint(solver),
        vars_(vars),
        size_(vars.size()),
        deviation_var_(deviation_var),
        total_sum_(total_sum),
        scaled_vars_assigned_value_(new int64[size_]),
        scaled_vars_min_(new int64[size_]),
        scaled_vars_max_(new int64[size_]),
        scaled_sum_max_(0),
        scaled_sum_min_(0),
        maximum_(new int64[size_]),
        overlaps_sup_(new int64[size_]),
        active_sum_(0),
        active_sum_rounded_down_(0),
        active_sum_rounded_up_(0),
        active_sum_nearest_(0) {
    CHECK(deviation_var != nullptr);
  }

  ~Deviation() override {}

  void Post() override {
    Solver* const s = solver();
    Demon* const demon = s->MakeConstraintInitialPropagateCallback(this);
    for (int i = 0; i < size_; ++i) {
      vars_[i]->WhenRange(demon);
    }
    deviation_var_->WhenRange(demon);
    s->AddConstraint(s->MakeSumEquality(vars_, total_sum_));
  }

  void InitialPropagate() override {
    const int64 delta_min = BuildMinimalDeviationAssignment();
    deviation_var_->SetMin(delta_min);
    PropagateBounds(delta_min);
  }

  std::string DebugString() const override {
    return StringPrintf("Deviation([%s], deviation_var = %s, sum = %lld)",
                        JoinDebugStringPtr(vars_, ", ").c_str(),
                        deviation_var_->DebugString().c_str(), total_sum_);
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kDeviation, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               vars_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                            deviation_var_);
    visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, total_sum_);
    visitor->EndVisitConstraint(ModelVisitor::kDeviation, this);
  }

 private:
  // Builds an assignment with minimal deviation and assign it to
  // scaled_vars_assigned_value_. It returns the minimal deviation:
  //   sum_i |scaled_vars_assigned_value_[i] - total_sum_|.
  int64 BuildMinimalDeviationAssignment() {
    RepairGreedySum(BuildGreedySum(true));
    int64 minimal_deviation = 0;
    for (int i = 0; i < size_; ++i) {
      minimal_deviation +=
          std::abs(scaled_vars_assigned_value_[i] - total_sum_);
    }
    return minimal_deviation;
  }

  // Propagates the upper and lower bounds of x[i]'s.
  // It assumes the constraint is consistent:
  //   - the sum constraint is consistent
  //   - min deviation smaller than max allowed deviation
  //  min_delta is the minimum possible deviation
  void PropagateBounds(int64 min_delta) {
    PropagateBounds(min_delta, true);   // Filter upper bounds.
    PropagateBounds(min_delta, false);  // Filter lower bounds.
  }

  // Prunes the upper/lower-bound of vars. We apply a mirroing of the
  // domains wrt 0 to prune the lower bounds such that we can use the
  // same algo to prune both sides of the domains.  upperBounds = true
  // to prune the upper bounds of vars, false to prune the lower
  // bounds.
  void PropagateBounds(int64 min_delta, bool upper_bound) {
    // Builds greedy assignment.
    const int64 greedy_sum = BuildGreedySum(upper_bound);
    // Repairs assignment and store information to be used when pruning.
    RepairSumAndComputeInfo(greedy_sum);
    // Does the actual pruning.
    PruneVars(min_delta, upper_bound);
  }

  // Cache min and max values of variables.
  void ComputeData(bool upper_bound) {
    scaled_sum_max_ = 0;
    scaled_sum_min_ = 0;
    for (int i = 0; i < size_; ++i) {
      scaled_vars_max_[i] =
          size_ * (upper_bound ? vars_[i]->Max() : -vars_[i]->Min());
      scaled_vars_min_[i] =
          size_ * (upper_bound ? vars_[i]->Min() : -vars_[i]->Max());
      scaled_sum_max_ += scaled_vars_max_[i];
      scaled_sum_min_ += scaled_vars_min_[i];
    }
    active_sum_ = (!upper_bound ? -total_sum_ : total_sum_);
    // down is <= sum.
    active_sum_rounded_down_ =
        size_ * MathUtil::FloorOfRatio<int64>(active_sum_, size_);
    // up is > sum, always.
    active_sum_rounded_up_ = active_sum_rounded_down_ + size_;
    active_sum_nearest_ = (active_sum_rounded_up_ - active_sum_ <=
                           active_sum_ - active_sum_rounded_down_)
                              ? active_sum_rounded_up_
                              : active_sum_rounded_down_;
  }

  // Builds an approximate sum in a greedy way.
  int64 BuildGreedySum(bool upper_bound) {
    // Update data structure.
    ComputeData(upper_bound);

    // Number of constraint should be consistent.
    DCHECK_GE(size_ * active_sum_, scaled_sum_min_);
    DCHECK_LE(size_ * active_sum_, scaled_sum_max_);

    int64 sum = 0;
    // Greedily assign variable to nearest value to average.
    overlaps_.clear();
    for (int i = 0; i < size_; ++i) {
      if (scaled_vars_min_[i] >= active_sum_) {
        scaled_vars_assigned_value_[i] = scaled_vars_min_[i];
      } else if (scaled_vars_max_[i] <= active_sum_) {
        scaled_vars_assigned_value_[i] = scaled_vars_max_[i];
      } else {
        // Overlapping variable scaled_vars_min_[i] < active_sum_ <
        // scaled_vars_max_[i].
        scaled_vars_assigned_value_[i] = active_sum_nearest_;
        if (active_sum_ % size_ != 0) {
          overlaps_.push_back(i);
        }
      }
      sum += scaled_vars_assigned_value_[i];
    }
    DCHECK_EQ(0, active_sum_rounded_down_ % size_);
    DCHECK_LE(active_sum_rounded_down_, active_sum_);
    DCHECK_LT(active_sum_ - active_sum_rounded_down_, size_);

    return sum;
  }

  bool Overlap(int var_index) const {
    return scaled_vars_min_[var_index] < active_sum_ &&
           scaled_vars_max_[var_index] > active_sum_;
  }

  // Repairs the greedy sum obtained above to get the correct sum.
  void RepairGreedySum(int64 greedy_sum) {
    // Useful constant: scaled version of the sum.
    const int64 scaled_total_sum = size_ * active_sum_;
    // Step used to make the repair.
    const int64 delta = greedy_sum > scaled_total_sum ? -size_ : size_;

    // Change overlapping variables as long as the sum is not
    // satisfied and there are overlapping vars, we use that ones to
    // repair.
    for (int j = 0; j < overlaps_.size() && greedy_sum != scaled_total_sum;
         j++) {
      scaled_vars_assigned_value_[overlaps_[j]] += delta;
      greedy_sum += delta;
    }
    // Change other variables if the sum is still not satisfied.
    for (int i = 0; i < size_ && greedy_sum != scaled_total_sum; ++i) {
      const int64 old_scaled_vars_i = scaled_vars_assigned_value_[i];
      if (greedy_sum < scaled_total_sum) {
        // Increase scaled_vars_assigned_value_[i] as much as
        // possible to fix the too low sum.
        scaled_vars_assigned_value_[i] += scaled_total_sum - greedy_sum;
        scaled_vars_assigned_value_[i] =
            std::min(scaled_vars_assigned_value_[i], scaled_vars_max_[i]);
      } else {
        // Decrease scaled_vars_assigned_value_[i] as much as
        // possible to fix the too high sum.
        scaled_vars_assigned_value_[i] -= (greedy_sum - scaled_total_sum);
        scaled_vars_assigned_value_[i] =
            std::max(scaled_vars_assigned_value_[i], scaled_vars_min_[i]);
      }
      // Maintain the sum.
      greedy_sum += scaled_vars_assigned_value_[i] - old_scaled_vars_i;
    }
  }

  // Computes the maximum values of variables in the case the repaired
  // greedy sum is actually the active sum.
  void ComputeMaxWhenNoRepair() {
    int num_overlap_sum_rounded_up = 0;
    if (active_sum_nearest_ == active_sum_rounded_up_) {
      num_overlap_sum_rounded_up = overlaps_.size();
    }
    for (int i = 0; i < size_; ++i) {
      maximum_[i] = scaled_vars_assigned_value_[i];
      if (Overlap(i) && active_sum_nearest_ == active_sum_rounded_up_ &&
          active_sum_ % size_ != 0) {
        overlaps_sup_[i] = num_overlap_sum_rounded_up - 1;
      } else {
        overlaps_sup_[i] = num_overlap_sum_rounded_up;
      }
    }
  }

  // Returns the number of variables overlapping the average value,
  // assigned to // the average value rounded up that we can/need to
  // move.
  int ComputeNumOverlapsVariableRoundedUp() {
    if (active_sum_ % size_ == 0) {
      return 0;
    }
    int num_overlap_sum_rounded_up = 0;
    for (int i = 0; i < size_; ++i) {
      if (scaled_vars_assigned_value_[i] > scaled_vars_min_[i] &&
          scaled_vars_assigned_value_[i] == active_sum_rounded_up_) {
        num_overlap_sum_rounded_up++;
      }
    }
    return num_overlap_sum_rounded_up;
  }

  // Returns whether we can push the greedy sum across the scaled
  // total sum in the same direction as going from the nearest rounded
  // sum to the farthest one.
  bool CanPushSumAcrossMean(int64 greedy_sum, int64 scaled_total_sum) const {
    return (greedy_sum > scaled_total_sum &&
            active_sum_nearest_ == active_sum_rounded_up_) ||
           (greedy_sum < scaled_total_sum &&
            active_sum_nearest_ == active_sum_rounded_down_);
  }

  // Repairs the sum and store intermediate information to be used
  // during pruning.
  void RepairSumAndComputeInfo(int64 greedy_sum) {
    const int64 scaled_total_sum = size_ * active_sum_;
    // Computation of key values for the pruning:
    // - overlaps_sup_
    // - maximum_[i]
    if (greedy_sum == scaled_total_sum) {  // No repair needed.
      ComputeMaxWhenNoRepair();
    } else {  // Repair and compute maximums.
      // Try to repair the sum greedily.
      if (CanPushSumAcrossMean(greedy_sum, scaled_total_sum)) {
        const int64 delta = greedy_sum > scaled_total_sum ? -size_ : size_;
        for (int j = 0; j < overlaps_.size() && greedy_sum != scaled_total_sum;
             ++j) {
          scaled_vars_assigned_value_[overlaps_[j]] += delta;
          greedy_sum += delta;
        }
      }

      const int num_overlap_sum_rounded_up =
          ComputeNumOverlapsVariableRoundedUp();

      if (greedy_sum == scaled_total_sum) {
        // Greedy sum is repaired.
        for (int i = 0; i < size_; ++i) {
          if (Overlap(i) && num_overlap_sum_rounded_up > 0) {
            maximum_[i] = active_sum_rounded_up_;
            overlaps_sup_[i] = num_overlap_sum_rounded_up - 1;
          } else {
            maximum_[i] = scaled_vars_assigned_value_[i];
            overlaps_sup_[i] = num_overlap_sum_rounded_up;
          }
        }
      } else if (greedy_sum > scaled_total_sum) {
        // scaled_vars_assigned_value_[i] = active_sum_rounded_down_ or
        // scaled_vars_assigned_value_[i] <= total_sum
        // (there is no more num_overlap_sum_rounded_up).
        for (int i = 0; i < size_; ++i) {
          maximum_[i] = scaled_vars_assigned_value_[i];
          overlaps_sup_[i] = 0;
        }
      } else {  // greedy_sum < scaled_total_sum.
        for (int i = 0; i < size_; ++i) {
          if (Overlap(i) && num_overlap_sum_rounded_up > 0) {
            overlaps_sup_[i] = num_overlap_sum_rounded_up - 1;
          } else {
            overlaps_sup_[i] = num_overlap_sum_rounded_up;
          }

          if (scaled_vars_assigned_value_[i] < scaled_vars_max_[i]) {
            maximum_[i] =
                scaled_vars_assigned_value_[i] + scaled_total_sum - greedy_sum;
          } else {
            maximum_[i] = scaled_vars_assigned_value_[i];
          }
        }
      }
    }
  }

  // Propagates onto variables with all computed data.
  void PruneVars(int64 min_delta, bool upper_bound) {
    // Pruning of upper bound of vars_[i] for var_index in [1..n].
    const int64 increase_down_up = (active_sum_rounded_up_ - active_sum_) -
                                   (active_sum_ - active_sum_rounded_down_);
    for (int var_index = 0; var_index < size_; ++var_index) {
      // Not bound, and a compatible new max.
      if (scaled_vars_max_[var_index] != scaled_vars_min_[var_index] &&
          maximum_[var_index] < scaled_vars_max_[var_index]) {
        const int64 new_max =
            ComputeNewMax(var_index, min_delta, increase_down_up);
        PruneBound(var_index, new_max, upper_bound);
      }
    }
  }

  // Computes new max for a variable.
  int64 ComputeNewMax(int var_index, int64 min_delta, int64 increase_down_up) {
    int64 maximum_value = maximum_[var_index];
    int64 current_min_delta = min_delta;

    if (overlaps_sup_[var_index] > 0 &&
        (current_min_delta +
             overlaps_sup_[var_index] * (size_ - increase_down_up) >=
         deviation_var_->Max())) {
      const int64 delta = deviation_var_->Max() - current_min_delta;
      maximum_value += (size_ * delta) / (size_ - increase_down_up);
      return MathUtil::FloorOfRatio<int64>(maximum_value, size_);
    } else {
      if (maximum_value == active_sum_rounded_down_ &&
          active_sum_rounded_down_ < active_sum_) {
        DCHECK_EQ(0, overlaps_sup_[var_index]);
        current_min_delta += size_ + increase_down_up;
        if (current_min_delta > deviation_var_->Max()) {
          DCHECK_EQ(0, maximum_value % size_);
          return maximum_value / size_;
        }
        maximum_value += size_;
      }
      current_min_delta +=
          overlaps_sup_[var_index] * (size_ - increase_down_up);
      maximum_value += size_ * overlaps_sup_[var_index];
      // Slope of 2 x n.
      const int64 delta = deviation_var_->Max() - current_min_delta;
      maximum_value += delta / 2;  // n * delta / (2 * n);
      return MathUtil::FloorOfRatio<int64>(maximum_value, size_);
    }
  }

  // Sets maximum on var or on its opposite.
  void PruneBound(int var_index, int64 bound, bool upper_bound) {
    if (upper_bound) {
      vars_[var_index]->SetMax(bound);
    } else {
      vars_[var_index]->SetMin(-bound);
    }
  }

  std::vector<IntVar*> vars_;
  const int size_;
  IntVar* const deviation_var_;
  const int64 total_sum_;
  std::unique_ptr<int64[]> scaled_vars_assigned_value_;
  std::unique_ptr<int64[]> scaled_vars_min_;
  std::unique_ptr<int64[]> scaled_vars_max_;
  int64 scaled_sum_max_;
  int64 scaled_sum_min_;
  // Stores the variables overlapping the mean value.
  std::vector<int> overlaps_;
  std::unique_ptr<int64[]> maximum_;
  std::unique_ptr<int64[]> overlaps_sup_;
  // These values are updated by ComputeData().
  int64 active_sum_;
  int64 active_sum_rounded_down_;
  int64 active_sum_rounded_up_;
  int64 active_sum_nearest_;
};
}  // namespace

Constraint* Solver::MakeDeviation(const std::vector<IntVar*>& vars,
                                  IntVar* const deviation_var,
                                  int64 total_sum) {
  return RevAlloc(new Deviation(this, vars, deviation_var, total_sum));
}
}  // namespace operations_research
