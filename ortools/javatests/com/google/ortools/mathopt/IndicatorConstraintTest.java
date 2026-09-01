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

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.extensions.proto.ProtoTruth.assertThat;

import com.google.common.collect.ImmutableMap;
import com.google.ortools.mathopt.IndicatorConstraintProto;
import com.google.ortools.mathopt.SparseDoubleVectorProto;
import org.junit.jupiter.api.Test;

public final class IndicatorConstraintTest {
  @Test
  public void testGetters() {
    ModelId modelId = new ModelId("model");
    Variable indicator = new Variable(0, 1, true, "z", 0, null, modelId);
    Variable x = new Variable(0, 1, false, "x", 1, null, modelId);
    ImmutableMap<Variable, Double> terms = ImmutableMap.of(x, 2.0);

    IndicatorConstraint constraint =
        new IndicatorConstraint(10, "c", modelId, indicator, true, 3.0, 4.0, terms);

    assertThat(constraint.getId()).isEqualTo(10);
    assertThat(constraint.getName()).isEqualTo("c");
    assertThat(constraint.getIndicatorVariable()).isEqualTo(indicator);
    assertThat(constraint.getActivateOnZero()).isTrue();
    assertThat(constraint.getLowerBound()).isEqualTo(3.0);
    assertThat(constraint.getUpperBound()).isEqualTo(4.0);
    assertThat(constraint.getUnmodifiableImpliedTerms()).containsExactly(x, 2.0);
    assertThat(constraint.getModelId()).isEqualTo(modelId);
    assertThat(constraint.isDeleted()).isFalse();
  }

  @Test
  public void testToProto() {
    ModelId modelId = new ModelId("model");
    Variable indicator = new Variable(0, 1, true, "z", 0, null, modelId);
    Variable x = new Variable(0, 1, false, "x", 1, null, modelId);
    ImmutableMap<Variable, Double> terms = ImmutableMap.of(x, 2.0);

    IndicatorConstraint constraint =
        new IndicatorConstraint(10, "c", modelId, indicator, true, 3.0, 4.0, terms);

    IndicatorConstraintProto proto = constraint.toProto();

    assertThat(proto).isEqualTo(IndicatorConstraintProto.newBuilder()
            .setIndicatorId(0)
            .setActivateOnZero(true)
            .setLowerBound(3.0)
            .setUpperBound(4.0)
            .setName("c")
            .setExpression(SparseDoubleVectorProto.newBuilder().addIds(1).addValues(2.0).build())
            .build());
  }

  @Test
  public void testToString() {
    ModelId modelId = new ModelId("model");
    Variable indicator = new Variable(0, 1, true, "z", 0, null, modelId);
    Variable x = new Variable(0, 1, false, "x", 1, null, modelId);
    ImmutableMap<Variable, Double> terms = ImmutableMap.of(x, 2.0);

    IndicatorConstraint constraint =
        new IndicatorConstraint(10, "c", modelId, indicator, true, 3.0, 4.0, terms);

    assertThat(constraint.toString()).isEqualTo("c");
    assertThat(constraint.toCompleteString()).isEqualTo("c: z = 0 => 3.0 ≤ 2.0 * x ≤ 4.0");
  }

  @Test
  public void testToString_deleted() {
    ModelId modelId = new ModelId("model");
    Variable indicator = new Variable(0, 1, true, "z", 0, null, modelId);
    IndicatorConstraint constraint =
        new IndicatorConstraint(10, "c", modelId, indicator, true, 3.0, 4.0, ImmutableMap.of());

    constraint.markDeleted();

    assertThat(constraint.toCompleteString()).isEqualTo("c: z = 0 => 3.0 ≤ 0.0 ≤ 4.0 (deleted)");
  }

  @Test
  public void testToString_indicatorDeleted() {
    ModelId modelId = new ModelId("model");
    Variable indicator = new Variable(0, 1, true, "z", 0, null, modelId);
    Variable x = new Variable(0, 1, false, "x", 1, null, modelId);
    ImmutableMap<Variable, Double> terms = ImmutableMap.of(x, 2.0);
    IndicatorConstraint constraint =
        new IndicatorConstraint(10, "c", modelId, indicator, true, 3.0, 4.0, terms);

    constraint.onVariableDeleted(indicator);

    assertThat(constraint.toCompleteString()).isEqualTo("c: unset => 3.0 ≤ 2.0 * x ≤ 4.0");
  }

  @Test
  public void testOnVariableDeleted_expressionVar() {
    ModelId modelId = new ModelId("model");
    Variable indicator = new Variable(0, 1, true, "z", 0, null, modelId);
    Variable x = new Variable(0, 1, false, "x", 1, null, modelId);
    ImmutableMap<Variable, Double> terms = ImmutableMap.of(x, 2.0);

    IndicatorConstraint constraint =
        new IndicatorConstraint(10, "c", modelId, indicator, true, 3.0, 4.0, terms);

    constraint.onVariableDeleted(x);

    assertThat(constraint.getUnmodifiableImpliedTerms()).isEmpty();
  }

  @Test
  public void testOnVariableDeleted_indicatorVar() {
    ModelId modelId = new ModelId("model");
    Variable indicator = new Variable(0, 1, true, "z", 0, null, modelId);
    Variable x = new Variable(0, 1, false, "x", 1, null, modelId);
    ImmutableMap<Variable, Double> terms = ImmutableMap.of(x, 2.0);

    IndicatorConstraint constraint =
        new IndicatorConstraint(10, "c", modelId, indicator, true, 3.0, 4.0, terms);

    constraint.onVariableDeleted(indicator);

    assertThat(constraint.getIndicatorVariable()).isNull();
    assertThat(constraint.getUnmodifiableImpliedTerms()).containsExactly(x, 2.0);

    // Verify proto output when indicator is null
    assertThat(constraint.toProto())
        .isEqualTo(IndicatorConstraintProto.newBuilder()
                .setActivateOnZero(true)
                .setLowerBound(3.0)
                .setUpperBound(4.0)
                .setName("c")
                .setExpression(
                    SparseDoubleVectorProto.newBuilder().addIds(1).addValues(2.0).build())
                .build());
  }
}
