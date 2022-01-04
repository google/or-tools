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

import com.google.ortools.sat.ReservoirConstraintProto;

/**
 * Specialized reservoir constraint.
 *
 * <p>This constraint allows adding events (time, levelChange, isActive (optional)) to the reservoir
 * constraint incrementally.
 */
public class ReservoirConstraint extends Constraint {
  public ReservoirConstraint(CpModel model) {
    super(model.getBuilder());
    this.model = model;
  }

  /**
   * Adds a mandatory event
   *
   * <p>It will increase the used capacity by `levelChange` at time `time`. `time` must be an affine
   * expression.
   */
  void addEvent(LinearArgument time, long levelChange) {
    ReservoirConstraintProto.Builder reservoir = getBuilder().getReservoirBuilder();
    reservoir.addTimeExprs(
        model.getLinearExpressionProtoBuilderFromLinearArgument(time, /*negate=*/false));
    reservoir.addLevelChanges(levelChange);
    reservoir.addActiveLiterals(model.trueLiteral().getIndex());
  }

  /**
   * Adds a mandatory event at a fixed time
   *
   * <p>It will increase the used capacity by `levelChange` at time `time`.
   */
  void addEvent(long time, long levelChange) {
    ReservoirConstraintProto.Builder reservoir = getBuilder().getReservoirBuilder();
    reservoir.addTimeExprs(model.getLinearExpressionProtoBuilderFromLong(time));
    reservoir.addLevelChanges(levelChange);
    reservoir.addActiveLiterals(model.trueLiteral().getIndex());
  }

  /**
   * Adds an optional event
   *
   * <p>If `isActive` is true, It will increase the used capacity by `levelChange` at time `time`.
   * `time` must be an affine expression.
   */
  void addOptionalEvent(LinearExpr time, long levelChange, Literal isActive) {
    ReservoirConstraintProto.Builder reservoir = getBuilder().getReservoirBuilder();
    reservoir.addTimeExprs(
        model.getLinearExpressionProtoBuilderFromLinearArgument(time, /*negate=*/false));
    reservoir.addLevelChanges(levelChange);
    reservoir.addActiveLiterals(isActive.getIndex());
  }

  /**
   * Adds an optional event at a fixed time
   *
   * <p>If `isActive` is true, It will increase the used capacity by `levelChange` at time `time`.
   */
  void addOptionalEvent(long time, long levelChange, Literal isActive) {
    ReservoirConstraintProto.Builder reservoir = getBuilder().getReservoirBuilder();
    reservoir.addTimeExprs(model.getLinearExpressionProtoBuilderFromLong(time));
    reservoir.addLevelChanges(levelChange);
    reservoir.addActiveLiterals(isActive.getIndex());
  }

  private final CpModel model;
}
