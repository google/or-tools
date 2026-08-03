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
import static com.google.ortools.mathopt.Expressions.linExpr;

import com.google.common.collect.ImmutableMap;
import com.google.ortools.mathopt.CallbackResultProto.GeneratedLinearConstraint;
import org.junit.jupiter.api.Test;

public final class CallbackResultTest {
  @Test
  public void new_empty_isEmpty() {
    var result = new CallbackResult();
    assertThat(result.toProto()).isEqualToDefaultInstance();
  }

  @Test
  public void setTerminate_falseToTrue_isTrue() {
    var result = new CallbackResult();
    result.setTerminate(true);
    assertThat(result.getTerminate()).isTrue();
  }

  @Test
  public void setTerminate_trueToFalse_isFalse() {
    var result = new CallbackResult();
    result.setTerminate(true);
    result.setTerminate(false);
    assertThat(result.getTerminate()).isFalse();
  }

  @Test
  public void toProto_terminate_isTermiante() {
    var result = new CallbackResult();
    result.setTerminate(true);
    assertThat(result.toProto())
        .isEqualTo(CallbackResultProto.newBuilder().setTerminate(true).build());
  }

  @Test
  public void toProto_withHints_hasHints() {
    var model = new Model();
    Variable x = model.addVariable();
    Variable y = model.addVariable();
    var result = new CallbackResult();
    result.suggestSolution(ImmutableMap.of(x, 2.0, y, 4.0));
    result.suggestSolution(ImmutableMap.of(y, 3.5));
    assertThat(result.toProto())
        .isEqualTo(CallbackResultProto.newBuilder()
                .addSuggestedSolutions(SparseDoubleVectorProto.newBuilder()
                        .addIds(0)
                        .addValues(2.0)
                        .addIds(1)
                        .addValues(4.0)
                        .build())
                .addSuggestedSolutions(
                    SparseDoubleVectorProto.newBuilder().addIds(1).addValues(3.5).build())
                .build());
  }

  @Test
  public void toProto_withConstraints_hasConstraints() {
    var model = new Model();
    Variable x = model.addVariable();
    Variable y = model.addVariable();
    var result = new CallbackResult();
    result.addLazyConstraint(0.0, ImmutableMap.of(x, 1.0, y, 2.0), 3.0);
    result.addUserCut(-1.0, ImmutableMap.of(y, 1.5), 4.0);

    var expected = CallbackResultProto.newBuilder();
    expected.addCuts(GeneratedLinearConstraint.newBuilder()
            .setIsLazy(true)
            .setLowerBound(0.0)
            .setUpperBound(3.0)
            .setLinearExpression(SparseDoubleVectorProto.newBuilder()
                    .addIds(0)
                    .addValues(1.0)
                    .addIds(1)
                    .addValues(2.0)
                    .build()));
    expected.addCuts(GeneratedLinearConstraint.newBuilder()
            .setLowerBound(-1.0)
            .setUpperBound(4.0)
            .setLinearExpression(
                SparseDoubleVectorProto.newBuilder().addIds(1).addValues(1.5).build()));

    assertThat(result.toProto()).isEqualTo(expected.build());
  }

  @Test
  public void toProto_withEqUserCuts_hasConstraints() {
    var model = new Model();
    Variable x = model.addVariable();
    Variable y = model.addVariable();
    LinearExpressionView lhs = linExpr().add(x).add(2.0, y).add(3.0);
    LinearExpressionView rhs = linExpr().add(1.5, y).add(4.0);
    var result = new CallbackResult();
    result.addEqUserCut(1.0, rhs);
    result.addEqUserCut(lhs, 1.0);
    result.addEqUserCut(lhs, rhs);

    var expected = CallbackResultProto.newBuilder();
    expected.addCuts(GeneratedLinearConstraint.newBuilder()
            .setLowerBound(-3.0)
            .setUpperBound(-3.0)
            .setLinearExpression(
                SparseDoubleVectorProto.newBuilder().addIds(1).addValues(1.5).build()));
    expected.addCuts(GeneratedLinearConstraint.newBuilder()
            .setLowerBound(-2.0)
            .setUpperBound(-2.0)
            .setLinearExpression(SparseDoubleVectorProto.newBuilder()
                    .addIds(0)
                    .addValues(1.0)
                    .addIds(1)
                    .addValues(2.0)
                    .build()));
    expected.addCuts(GeneratedLinearConstraint.newBuilder()
            .setLowerBound(1.0)
            .setUpperBound(1.0)
            .setLinearExpression(SparseDoubleVectorProto.newBuilder()
                    .addIds(0)
                    .addValues(1.0)
                    .addIds(1)
                    .addValues(0.5)
                    .build()));

    assertThat(result.toProto()).isEqualTo(expected.build());
  }

  @Test
  public void toProto_withLeUserCuts_hasConstraints() {
    var model = new Model();
    Variable x = model.addVariable();
    Variable y = model.addVariable();
    LinearExpressionView lhs = linExpr().add(x).add(2.0, y).add(3.0);
    LinearExpressionView rhs = linExpr().add(1.5, y).add(4.0);
    var result = new CallbackResult();
    result.addLeUserCut(1.0, rhs);
    result.addLeUserCut(lhs, 1.0);
    result.addLeUserCut(lhs, rhs);

    var expected = CallbackResultProto.newBuilder();
    expected.addCuts(GeneratedLinearConstraint.newBuilder()
            .setLowerBound(-3.0)
            .setUpperBound(Double.POSITIVE_INFINITY)
            .setLinearExpression(
                SparseDoubleVectorProto.newBuilder().addIds(1).addValues(1.5).build()));
    expected.addCuts(GeneratedLinearConstraint.newBuilder()
            .setLowerBound(Double.NEGATIVE_INFINITY)
            .setUpperBound(-2.0)
            .setLinearExpression(SparseDoubleVectorProto.newBuilder()
                    .addIds(0)
                    .addValues(1.0)
                    .addIds(1)
                    .addValues(2.0)
                    .build()));
    expected.addCuts(GeneratedLinearConstraint.newBuilder()
            .setLowerBound(Double.NEGATIVE_INFINITY)
            .setUpperBound(1.0)
            .setLinearExpression(SparseDoubleVectorProto.newBuilder()
                    .addIds(0)
                    .addValues(1.0)
                    .addIds(1)
                    .addValues(0.5)
                    .build()));

    assertThat(result.toProto()).isEqualTo(expected.build());
  }

  @Test
  public void toProto_withGeUserCuts_hasConstraints() {
    var model = new Model();
    Variable x = model.addVariable();
    Variable y = model.addVariable();
    LinearExpressionView lhs = linExpr().add(x).add(2.0, y).add(3.0);
    LinearExpressionView rhs = linExpr().add(1.5, y).add(4.0);
    var result = new CallbackResult();
    result.addGeUserCut(1.0, rhs);
    result.addGeUserCut(lhs, 1.0);
    result.addGeUserCut(lhs, rhs);

    var expected = CallbackResultProto.newBuilder();
    expected.addCuts(GeneratedLinearConstraint.newBuilder()
            .setLowerBound(Double.NEGATIVE_INFINITY)
            .setUpperBound(-3.0)
            .setLinearExpression(
                SparseDoubleVectorProto.newBuilder().addIds(1).addValues(1.5).build()));
    expected.addCuts(GeneratedLinearConstraint.newBuilder()
            .setLowerBound(-2.0)
            .setUpperBound(Double.POSITIVE_INFINITY)
            .setLinearExpression(SparseDoubleVectorProto.newBuilder()
                    .addIds(0)
                    .addValues(1.0)
                    .addIds(1)
                    .addValues(2.0)
                    .build()));
    expected.addCuts(GeneratedLinearConstraint.newBuilder()
            .setLowerBound(1.0)
            .setUpperBound(Double.POSITIVE_INFINITY)
            .setLinearExpression(SparseDoubleVectorProto.newBuilder()
                    .addIds(0)
                    .addValues(1.0)
                    .addIds(1)
                    .addValues(0.5)
                    .build()));

    assertThat(result.toProto()).isEqualTo(expected.build());
  }

  @Test
  public void toProto_withRangeUserCut_hasConstraint() {
    var model = new Model();
    Variable x = model.addVariable();
    Variable y = model.addVariable();
    var result = new CallbackResult();
    result.addRangeUserCut(1.0, linExpr().add(x).add(2.0, y).add(3.0), 5.0);

    var expected = CallbackResultProto.newBuilder();
    expected.addCuts(GeneratedLinearConstraint.newBuilder()
            .setLowerBound(-2.0)
            .setUpperBound(2.0)
            .setLinearExpression(SparseDoubleVectorProto.newBuilder()
                    .addIds(0)
                    .addValues(1.0)
                    .addIds(1)
                    .addValues(2.0)
                    .build()));

    assertThat(result.toProto()).isEqualTo(expected.build());
  }

  @Test
  public void toProto_withEqLazyConstraints_hasConstraints() {
    var model = new Model();
    Variable x = model.addVariable();
    Variable y = model.addVariable();
    LinearExpressionView lhs = linExpr().add(x).add(2.0, y).add(3.0);
    LinearExpressionView rhs = linExpr().add(1.5, y).add(4.0);
    var result = new CallbackResult();
    result.addEqLazyConstraint(1.0, rhs);
    result.addEqLazyConstraint(lhs, 1.0);
    result.addEqLazyConstraint(lhs, rhs);

    var expected = CallbackResultProto.newBuilder();
    expected.addCuts(GeneratedLinearConstraint.newBuilder()
            .setIsLazy(true)
            .setLowerBound(-3.0)
            .setUpperBound(-3.0)
            .setLinearExpression(
                SparseDoubleVectorProto.newBuilder().addIds(1).addValues(1.5).build()));
    expected.addCuts(GeneratedLinearConstraint.newBuilder()
            .setIsLazy(true)
            .setLowerBound(-2.0)
            .setUpperBound(-2.0)
            .setLinearExpression(SparseDoubleVectorProto.newBuilder()
                    .addIds(0)
                    .addValues(1.0)
                    .addIds(1)
                    .addValues(2.0)
                    .build()));
    expected.addCuts(GeneratedLinearConstraint.newBuilder()
            .setIsLazy(true)
            .setLowerBound(1.0)
            .setUpperBound(1.0)
            .setLinearExpression(SparseDoubleVectorProto.newBuilder()
                    .addIds(0)
                    .addValues(1.0)
                    .addIds(1)
                    .addValues(0.5)
                    .build()));

    assertThat(result.toProto()).isEqualTo(expected.build());
  }

  @Test
  public void toProto_withLeLazyConstraints_hasConstraints() {
    var model = new Model();
    Variable x = model.addVariable();
    Variable y = model.addVariable();
    LinearExpressionView lhs = linExpr().add(x).add(2.0, y).add(3.0);
    LinearExpressionView rhs = linExpr().add(1.5, y).add(4.0);
    var result = new CallbackResult();
    result.addLeLazyConstraint(1.0, rhs);
    result.addLeLazyConstraint(lhs, 1.0);
    result.addLeLazyConstraint(lhs, rhs);

    var expected = CallbackResultProto.newBuilder();
    expected.addCuts(GeneratedLinearConstraint.newBuilder()
            .setIsLazy(true)
            .setLowerBound(-3.0)
            .setUpperBound(Double.POSITIVE_INFINITY)
            .setLinearExpression(
                SparseDoubleVectorProto.newBuilder().addIds(1).addValues(1.5).build()));
    expected.addCuts(GeneratedLinearConstraint.newBuilder()
            .setIsLazy(true)
            .setLowerBound(Double.NEGATIVE_INFINITY)
            .setUpperBound(-2.0)
            .setLinearExpression(SparseDoubleVectorProto.newBuilder()
                    .addIds(0)
                    .addValues(1.0)
                    .addIds(1)
                    .addValues(2.0)
                    .build()));
    expected.addCuts(GeneratedLinearConstraint.newBuilder()
            .setIsLazy(true)
            .setLowerBound(Double.NEGATIVE_INFINITY)
            .setUpperBound(1.0)
            .setLinearExpression(SparseDoubleVectorProto.newBuilder()
                    .addIds(0)
                    .addValues(1.0)
                    .addIds(1)
                    .addValues(0.5)
                    .build()));

    assertThat(result.toProto()).isEqualTo(expected.build());
  }

  @Test
  public void toProto_withGeLazyConstraints_hasConstraints() {
    var model = new Model();
    Variable x = model.addVariable();
    Variable y = model.addVariable();
    LinearExpressionView lhs = linExpr().add(x).add(2.0, y).add(3.0);
    LinearExpressionView rhs = linExpr().add(1.5, y).add(4.0);
    var result = new CallbackResult();
    result.addGeLazyConstraint(1.0, rhs);
    result.addGeLazyConstraint(lhs, 1.0);
    result.addGeLazyConstraint(lhs, rhs);

    var expected = CallbackResultProto.newBuilder();
    expected.addCuts(GeneratedLinearConstraint.newBuilder()
            .setIsLazy(true)
            .setLowerBound(Double.NEGATIVE_INFINITY)
            .setUpperBound(-3.0)
            .setLinearExpression(
                SparseDoubleVectorProto.newBuilder().addIds(1).addValues(1.5).build()));
    expected.addCuts(GeneratedLinearConstraint.newBuilder()
            .setIsLazy(true)
            .setLowerBound(-2.0)
            .setUpperBound(Double.POSITIVE_INFINITY)
            .setLinearExpression(SparseDoubleVectorProto.newBuilder()
                    .addIds(0)
                    .addValues(1.0)
                    .addIds(1)
                    .addValues(2.0)
                    .build()));
    expected.addCuts(GeneratedLinearConstraint.newBuilder()
            .setIsLazy(true)
            .setLowerBound(1.0)
            .setUpperBound(Double.POSITIVE_INFINITY)
            .setLinearExpression(SparseDoubleVectorProto.newBuilder()
                    .addIds(0)
                    .addValues(1.0)
                    .addIds(1)
                    .addValues(0.5)
                    .build()));

    assertThat(result.toProto()).isEqualTo(expected.build());
  }

  @Test
  public void toProto_withRangeLazyConstraints_hasConstraint() {
    var model = new Model();
    Variable x = model.addVariable();
    Variable y = model.addVariable();
    var result = new CallbackResult();
    result.addRangeLazyConstraint(1.0, linExpr().add(x).add(2.0, y).add(3.0), 5.0);

    var expected = CallbackResultProto.newBuilder();
    expected.addCuts(GeneratedLinearConstraint.newBuilder()
            .setIsLazy(true)
            .setLowerBound(-2.0)
            .setUpperBound(2.0)
            .setLinearExpression(SparseDoubleVectorProto.newBuilder()
                    .addIds(0)
                    .addValues(1.0)
                    .addIds(1)
                    .addValues(2.0)
                    .build()));

    assertThat(result.toProto()).isEqualTo(expected.build());
  }
}
