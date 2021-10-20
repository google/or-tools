// Copyright 2010-2021 Google LLC
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

import com.google.ortools.sat.ConstraintProto;
import com.google.ortools.sat.CpModelProto;
import com.google.ortools.sat.IntervalConstraintProto;
import com.google.ortools.sat.LinearExpressionProto;

/** An interval variable. This class must be constructed from the CpModel class. */
public final class IntervalVar {
  IntervalVar(CpModelProto.Builder builder, LinearExpressionProto.Builder startBuilder,
      LinearExpressionProto.Builder sizeBuilder, LinearExpressionProto.Builder endBuilder,
      String name) {
    this.modelBuilder = builder;
    this.constraintIndex = modelBuilder.getConstraintsCount();
    ConstraintProto.Builder ct = modelBuilder.addConstraintsBuilder();
    ct.setName(name);
    this.intervalBuilder = ct.getIntervalBuilder();
    this.intervalBuilder.setStart(startBuilder);
    this.intervalBuilder.setSize(sizeBuilder);
    this.intervalBuilder.setEnd(endBuilder);
  }

  IntervalVar(CpModelProto.Builder builder, LinearExpressionProto.Builder startBuilder,
      LinearExpressionProto.Builder sizeBuilder, LinearExpressionProto.Builder endBuilder,
      int isPresentIndex, String name) {
    this.modelBuilder = builder;
    this.constraintIndex = modelBuilder.getConstraintsCount();
    ConstraintProto.Builder ct = modelBuilder.addConstraintsBuilder();
    ct.setName(name);
    ct.addEnforcementLiteral(isPresentIndex);
    this.intervalBuilder = ct.getIntervalBuilder();
    this.intervalBuilder.setStart(startBuilder);
    this.intervalBuilder.setSize(sizeBuilder);
    this.intervalBuilder.setEnd(endBuilder);
  }

  @Override
  public String toString() {
    return modelBuilder.getConstraints(constraintIndex).toString();
  }

  /** Returns the index of the interval constraint in the model. */
  public int getIndex() {
    return constraintIndex;
  }

  /** Returns the interval builder. */
  public IntervalConstraintProto.Builder getBuilder() {
    return intervalBuilder;
  }

  /** Returns the name passed in the constructor. */
  public String getName() {
    return modelBuilder.getConstraints(constraintIndex).getName();
  }

  /** Returns the start expression. */
  public LinearExpr getStartExpr() {
    return LinearExpr.rebuildFromLinearExpressionProto(intervalBuilder.getStart(), modelBuilder);
  }

  /** Returns the size expression. */
  public LinearExpr getSizeExpr() {
    return LinearExpr.rebuildFromLinearExpressionProto(intervalBuilder.getSize(), modelBuilder);
  }

  /** Returns the size expression. */
  public LinearExpr getEndExpr() {
    return LinearExpr.rebuildFromLinearExpressionProto(intervalBuilder.getEnd(), modelBuilder);
  }

  private final CpModelProto.Builder modelBuilder;
  private final int constraintIndex;
  private final IntervalConstraintProto.Builder intervalBuilder;
}
