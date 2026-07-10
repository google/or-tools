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

package com.google.ortools.mathopt;

import com.google.common.base.Preconditions;
import com.google.common.collect.ImmutableMap;

/** Util class for converting protos to objects. */
final class UtilFromProto {
  /**
   * Returns an ImmutableMap mapping a {@link Variable} to double value. This mapping is created
   * from the given {@code proto}.
   *
   * @throws IllegalArgumentException if the {@code proto} is invalid or if it has a variable ID
   *     that does not exist in {@code model}.
   */
  public static ImmutableMap<Variable, Double> variableValuesFromProto(
      Model model, SparseDoubleVectorProto proto) {
    Preconditions.checkArgument(proto.getIdsCount() == proto.getValuesCount(),
        "Ids size=%s and values size=%s must be the same", proto.getIdsCount(),
        proto.getValuesCount());
    ImmutableMap.Builder<Variable, Double> map = ImmutableMap.builder();
    for (int i = 0; i < proto.getIdsCount(); i++) {
      long id = proto.getIds(i);
      Preconditions.checkArgument(model.hasVariableWithId(id), "no variable with ID %s exists", id);
      map.put(model.getVariable(id), proto.getValues(i));
    }
    return map.buildOrThrow();
  }

  /**
   * Returns an ImmutableMap mapping a {@link Variable} to double value. This mapping is created
   * from the given {@code proto}.
   *
   * @throws IllegalArgumentException if the {@code proto} is invalid or if it has a variable id
   *     that does not exist in {@code model}.
   */
  public static ImmutableMap<Variable, Integer> variableInt32ValuesFromProto(
      Model model, SparseInt32VectorProto proto) {
    Preconditions.checkArgument(proto.getIdsCount() == proto.getValuesCount(),
        "Ids size=%s and values size=%s must be the same", proto.getIdsCount(),
        proto.getValuesCount());
    ImmutableMap.Builder<Variable, Integer> map = ImmutableMap.builder();
    for (int i = 0; i < proto.getIdsCount(); i++) {
      long id = proto.getIds(i);
      Preconditions.checkArgument(model.hasVariableWithId(id), "no variable with ID %s exists", id);
      map.put(model.getVariable(id), proto.getValues(i));
    }
    return map.buildOrThrow();
  }

  /**
   * Returns an ImmutableMap mapping a {@link LinearConstraint} to a double value. This mapping is
   * created from the given {@code proto}.
   *
   * @throws IllegalArgumentException if the {@code proto} is invalid or if it has a linear
   *     constraint ID that does not exist in {@code model}.
   */
  public static ImmutableMap<LinearConstraint, Double> linearConstraintValuesFromProto(
      Model model, SparseDoubleVectorProto proto) {
    Preconditions.checkArgument(proto.getIdsCount() == proto.getValuesCount(),
        "Ids size=%s and values size=%s must be the same", proto.getIdsCount(),
        proto.getValuesCount());
    ImmutableMap.Builder<LinearConstraint, Double> map = ImmutableMap.builder();
    for (int i = 0; i < proto.getIdsCount(); i++) {
      long id = proto.getIds(i);
      Preconditions.checkArgument(
          model.hasLinearConstraintWithId(id), "no linear constraint with ID %s exists", id);
      map.put(model.getLinearConstraint(id), proto.getValues(i));
    }
    return map.buildOrThrow();
  }

  private UtilFromProto() {}
}
