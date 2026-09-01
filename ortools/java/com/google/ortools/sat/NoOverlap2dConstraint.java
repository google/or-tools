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

import com.google.ortools.sat.CpModelProto;
import com.google.ortools.sat.NoOverlap2DConstraintProto;

/**
 * Specialized NoOverlap2D constraint.
 *
 * <p>This constraint allows adding rectangles to the NoOverlap2D constraint incrementally.
 */
public class NoOverlap2dConstraint extends Constraint {
  public NoOverlap2dConstraint(CpModelProto.Builder builder) {
    super(builder);
  }

  /**
   * Adds a rectangle (xInterval, yInterval) to the constraint.
   *
   * @param xInterval the x interval of the rectangle.
   * @param yInterval the y interval of the rectangle.
   * @return this constraint
   */
  public NoOverlap2dConstraint addRectangle(IntervalVar xInterval, IntervalVar yInterval) {
    NoOverlap2DConstraintProto.Builder noOverlap2d = getBuilder().getNoOverlap2DBuilder();
    noOverlap2d.addXIntervals(xInterval.getIndex());
    noOverlap2d.addYIntervals(yInterval.getIndex());
    return this;
  }
}
