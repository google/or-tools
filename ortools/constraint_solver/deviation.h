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

#ifndef ORTOOLS_CONSTRAINT_SOLVER_DEVIATION_H_
#define ORTOOLS_CONSTRAINT_SOLVER_DEVIATION_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "ortools/constraint_solver/constraint_solver.h"

namespace operations_research {

class Deviation : public Constraint {
 public:
  Deviation(Solver* solver, const std::vector<IntVar*>& vars,
            IntVar* deviation_var, int64_t total_sum)
      : Constraint(solver),
        vars_(vars),
        size_(vars.size()),
        deviation_var_(deviation_var),
        total_sum_(total_sum),
        scaled_vars_assigned_value_(new int64_t[size_]),
        scaled_vars_min_(new int64_t[size_]),
        scaled_vars_max_(new int64_t[size_]),
        scaled_sum_max_(0),
        scaled_sum_min_(0),
        maximum_(new int64_t[size_]),
        overlaps_sup_(new int64_t[size_]),
        active_sum_(0),
        active_sum_rounded_down_(0),
        active_sum_rounded_up_(0),
        active_sum_nearest_(0) {
    CHECK(deviation_var != nullptr);
  }

  ~Deviation() override {}

  void Post() override;

  void InitialPropagate() override;

  std::string DebugString() const override;

  void Accept(ModelVisitor* visitor) const override;

 private:
  // Builds an assignment with minimal deviation and assign it to
  // scaled_vars_assigned_value_. It returns the minimal deviation:
  //   sum_i |scaled_vars_assigned_value_[i] - total_sum_|.
  int64_t BuildMinimalDeviationAssignment();

  // Propagates the upper and lower bounds of x[i]'s.
  // It assumes the constraint is consistent:
  //   - the sum constraint is consistent
  //   - min deviation smaller than max allowed deviation
  //  min_delta is the minimum possible deviation
  void PropagateBounds(int64_t min_delta);

  // Prunes the upper/lower-bound of vars. We apply a mirroring of the
  // domains wrt 0 to prune the lower bounds such that we can use the
  // same algo to prune both sides of the domains.  upperBounds = true
  // to prune the upper bounds of vars, false to prune the lower
  // bounds.
  void PropagateBounds(int64_t min_delta, bool upper_bound);

  // Cache min and max values of variables.
  void ComputeData(bool upper_bound);

  // Builds an approximate sum in a greedy way.
  int64_t BuildGreedySum(bool upper_bound);

  bool Overlap(int var_index) const;

  // Repairs the greedy sum obtained above to get the correct sum.
  void RepairGreedySum(int64_t greedy_sum);

  // Computes the maximum values of variables in the case the repaired
  // greedy sum is actually the active sum.
  void ComputeMaxWhenNoRepair();

  // Returns the number of variables overlapping the average value,
  // assigned to // the average value rounded up that we can/need to
  // move.
  int ComputeNumOverlapsVariableRoundedUp();

  // Returns whether we can push the greedy sum across the scaled
  // total sum in the same direction as going from the nearest rounded
  // sum to the farthest one.
  bool CanPushSumAcrossMean(int64_t greedy_sum, int64_t scaled_total_sum) const;

  // Repairs the sum and store intermediate information to be used
  // during pruning.
  void RepairSumAndComputeInfo(int64_t greedy_sum);

  // Propagates onto variables with all computed data.
  void PruneVars(int64_t min_delta, bool upper_bound);

  // Computes new max for a variable.
  int64_t ComputeNewMax(int var_index, int64_t min_delta,
                        int64_t increase_down_up);

  // Sets maximum on var or on its opposite.
  void PruneBound(int var_index, int64_t bound, bool upper_bound);

  std::vector<IntVar*> vars_;
  const int size_;
  IntVar* const deviation_var_;
  const int64_t total_sum_;
  std::unique_ptr<int64_t[]> scaled_vars_assigned_value_;
  std::unique_ptr<int64_t[]> scaled_vars_min_;
  std::unique_ptr<int64_t[]> scaled_vars_max_;
  int64_t scaled_sum_max_;
  int64_t scaled_sum_min_;
  // Stores the variables overlapping the mean value.
  std::vector<int> overlaps_;
  std::unique_ptr<int64_t[]> maximum_;
  std::unique_ptr<int64_t[]> overlaps_sup_;
  // These values are updated by ComputeData().
  int64_t active_sum_;
  int64_t active_sum_rounded_down_;
  int64_t active_sum_rounded_up_;
  int64_t active_sum_nearest_;
};

}  // namespace operations_research

#endif  // ORTOOLS_CONSTRAINT_SOLVER_DEVIATION_H_
