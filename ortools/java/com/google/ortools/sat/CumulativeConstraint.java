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

package com.google.ortools.sat;

import com.google.ortools.sat.CumulativeConstraintProto;

/**
 * Specialized cumulative constraint.
 *
 * <p>This constraint allows adding (interval, demand) pairs to the cumulative constraint
 * incrementally.
 */
public class CumulativeConstraint extends Constraint {
  public CumulativeConstraint(CpModel model) {
    super(model.getBuilder());
    this.model = model;
  }

  /// Adds a pair (interval, demand) to the constraint.
  public CumulativeConstraint addDemand(IntervalVar interval, LinearArgument demand) {
    CumulativeConstraintProto.Builder cumul = getBuilder().getCumulativeBuilder();
    cumul.addIntervals(interval.getIndex());
    cumul.addDemands(model.getLinearExpressionProtoBuilderFromLinearArgument(demand, false));
    return this;
  }

  /// Adds a pair (interval, demand) to the constraint.
  public CumulativeConstraint addDemand(IntervalVar interval, long demand) {
    CumulativeConstraintProto.Builder cumul = getBuilder().getCumulativeBuilder();
    cumul.addIntervals(interval.getIndex());
    cumul.addDemands(model.getLinearExpressionProtoBuilderFromLong(demand));
    return this;
  }

  /**
   * Adds all pairs (intervals[i], demands[i]) to the constraint.
   *
   * @param intervals an array of interval variables
   * @param demands an array of linear expression
   * @return itself
   * @throws CpModel.MismatchedArrayLengths if intervals and demands have different length
   */
  public CumulativeConstraint addDemands(IntervalVar[] intervals, LinearArgument[] demands) {
    if (intervals.length != demands.length) {
      throw new CpModel.MismatchedArrayLengths(
          "CumulativeConstraint.addDemands", "intervals", "demands");
    }
    for (int i = 0; i < intervals.length; i++) {
      addDemand(intervals[i], demands[i]);
    }
    return this;
  }

  /**
   * Adds all pairs (intervals[i], demands[i]) to the constraint.
   *
   * @param intervals an array of interval variables
   * @param demands an array of long values
   * @return itself
   * @throws CpModel.MismatchedArrayLengths if intervals and demands have different length
   */
  public CumulativeConstraint addDemands(IntervalVar[] intervals, long[] demands) {
    if (intervals.length != demands.length) {
      throw new CpModel.MismatchedArrayLengths(
          "CumulativeConstraint.addDemands", "intervals", "demands");
    }
    for (int i = 0; i < intervals.length; i++) {
      addDemand(intervals[i], demands[i]);
    }
    return this;
  }

  /**
   * Adds all pairs (intervals[i], demands[i]) to the constraint.
   *
   * @param intervals an array of interval variables
   * @param demands an array of integer values
   * @return itself
   * @throws CpModel.MismatchedArrayLengths if intervals and demands have different length
   */
  public CumulativeConstraint addDemands(IntervalVar[] intervals, int[] demands) {
    if (intervals.length != demands.length) {
      throw new CpModel.MismatchedArrayLengths(
          "CumulativeConstraint.addDemands", "intervals", "demands");
    }
    for (int i = 0; i < intervals.length; i++) {
      addDemand(intervals[i], demands[i]);
    }
    return this;
  }

  private final CpModel model;
}
