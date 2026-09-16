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
import com.google.ortools.mathopt.Variable.OnChangeListener;
import com.google.ortools.mathopt.VariablesProto;
import java.util.AbstractMap.SimpleImmutableEntry;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

public final class VariableTest {
  private ModelId modelId;
  private TestListener listener;

  enum Callback {
    LB,
    UB,
    INTEGER;
  }

  private static class TestListener implements OnChangeListener {
    private final List<SimpleImmutableEntry<Callback, Variable>> callbacks = new ArrayList<>();

    List<SimpleImmutableEntry<Callback, Variable>> getCallbacks() {
      return Collections.unmodifiableList(callbacks);
    }

    @Override
    public void onLowerBoundChanged(Variable variable) {
      callbacks.add(new SimpleImmutableEntry<>(Callback.LB, variable));
    }

    @Override
    public void onUpperBoundChanged(Variable variable) {
      callbacks.add(new SimpleImmutableEntry<>(Callback.UB, variable));
    }

    @Override
    public void onIntegerChanged(Variable variable) {
      callbacks.add(new SimpleImmutableEntry<>(Callback.INTEGER, variable));
    }
  }

  @BeforeEach
  public void setUp() {
    modelId = new ModelId("test_model");
    listener = new TestListener();
  }

  private Variable newVariable(double lowerBound, double upperBound, boolean isInteger, String name,
      long id, Variable.OnChangeListener listener) {
    return new Variable(lowerBound, upperBound, isInteger, name, id, listener, modelId);
  }

  private Variable newBinaryVariable(String name, long id, Variable.OnChangeListener listener) {
    return newVariable(
        /* lowerBound= */ 0.0, /* upperBound= */ 1.0, /* isInteger= */ true, name, id, listener);
  }

  @Test
  public void variableGetters_simpleAccessOk() {
    // do not use newBinaryVariable(), we are asserting on getModelId().
    Variable v = new Variable(
        /* lowerBound= */ 0.0,
        /* upperBound= */ 1.0,
        /* isInteger= */ true, "x", 2L, listener, modelId);

    assertThat(v.getLowerBound()).isEqualTo(0.0);
    assertThat(v.getUpperBound()).isEqualTo(1.0);
    assertThat(v.isInteger()).isTrue();
    assertThat(v.getId()).isEqualTo(2L);
    assertThat(v.getName()).isEqualTo("x");
    assertThat(v.isDeleted()).isFalse();
    assertThat(v.getModelId()).isSameInstanceAs(modelId);
    assertThat(v.offset()).isEqualTo(0.0);
    assertThat(v.unmodifiableTerms()).containsExactly(Map.entry(v, 1.0));
    assertThat(listener.getCallbacks()).isEmpty();
  }

  @Test
  public void setLowerBound_valueUpdatedAndCallbackInvokedOnChange() {
    Variable v = newBinaryVariable("x", 0L, listener);

    assertThat(v.setLowerBound(-1.0)).isSameInstanceAs(v);

    assertThat(listener.getCallbacks()).containsExactly(new SimpleImmutableEntry<>(Callback.LB, v));
    assertThat(v.getLowerBound()).isEqualTo(-1.0);
  }

  @Test
  public void setLowerBound_callbackNotInvokedNoChange() {
    Variable v = newBinaryVariable("x", 0L, listener);

    assertThat(v.setLowerBound(0.0)).isSameInstanceAs(v);

    assertThat(listener.getCallbacks()).isEmpty();
  }

  @Test
  public void setUpperBound_valueUpdatedAndCallbackInvokedOnChange() {
    Variable v = newBinaryVariable("x", 0L, listener);

    assertThat(v.setUpperBound(2.0)).isSameInstanceAs(v);

    assertThat(listener.getCallbacks()).containsExactly(new SimpleImmutableEntry<>(Callback.UB, v));
    assertThat(v.getUpperBound()).isEqualTo(2.0);
  }

  @Test
  public void setUpperBound_callbackNotInvokedNoChange() {
    Variable v = newBinaryVariable("x", 0L, listener);

    assertThat(v.setUpperBound(1.0)).isSameInstanceAs(v);

    assertThat(listener.getCallbacks()).isEmpty();
  }

  @Test
  public void setInteger_valueUpdatedAndCallbackInvokedOnChange() {
    Variable v = newBinaryVariable("x", 0L, listener);

    assertThat(v.setInteger(false)).isSameInstanceAs(v);

    assertThat(listener.getCallbacks())
        .containsExactly(new SimpleImmutableEntry<>(Callback.INTEGER, v));
    assertThat(v.isInteger()).isFalse();
  }

  @Test
  public void setInteger_callbackNotInvokedNoChange() {
    Variable v = newBinaryVariable("x", 0L, listener);

    assertThat(v.setInteger(true)).isSameInstanceAs(v);

    assertThat(listener.getCallbacks()).isEmpty();
  }

  @Test
  public void addTo_emptyHashLinearExpression_addsSelf() {
    Variable v = newBinaryVariable("x", 0L, listener);
    var expr = new HashLinearExpression();

    // To test Variable::addTo(double scale, LinearExpressionSink sink), we can call
    // HashLinearExpression::add(scale, v);
    expr.add(3.0, v);

    assertThat(expr.offset()).isEqualTo(0.0);
    assertThat(expr.unmodifiableTerms()).containsExactly(Map.entry(v, 3.0));
  }

  @Test
  public void hasSameModel_sameParentAndDifferentParent() {
    Variable v1 = newBinaryVariable("v", 2L, listener);
    Variable w1 = newBinaryVariable("w", 4L, listener);
    var modelId2 = new ModelId("test_model2");
    TestListener listener2 = new TestListener();
    Variable v2 = new Variable(0.0, 1.0, true, "v", 2, listener2, modelId2);

    assertThat(v1.hasSameModel(w1)).isTrue();
    assertThat(v1.hasSameModel(v1)).isTrue();
    assertThat(v1.hasSameModel(v2)).isFalse();
  }

  @Test
  public void toString_emptyName() {
    Variable v = newBinaryVariable("", 2L, listener);

    assertThat(v.toString()).isEqualTo("__var#2__");
    assertThat(listener.getCallbacks()).isEmpty();
  }

  @Test
  public void toString_withName() {
    Variable v = newBinaryVariable("x", 2L, listener);

    assertThat(v.toString()).isEqualTo("x");
    assertThat(listener.getCallbacks()).isEmpty();
  }

  @Test
  public void toCompleteString_binaryVariable() {
    Variable v = newBinaryVariable("", 2L, listener);

    assertThat(v.toCompleteString()).isEqualTo("__var#2__ (binary)");
    assertThat(listener.getCallbacks()).isEmpty();
  }

  @Test
  public void toComlpeteString_binaryVariableThatWasDeleted() {
    Variable v = newBinaryVariable("", 2L, listener);
    v.markDeleted();

    assertThat(v.toCompleteString()).isEqualTo("__var#2__ (binary) (was deleted)");
    assertThat(listener.getCallbacks()).isEmpty();
  }

  @Test
  public void toCompleteString_integerVariable() {
    Variable x = newVariable(
        /* lowerBound= */ 0.0, /* upperBound= */ 2.0, /* isInteger= */ true, "x", 2L, listener);

    assertThat(x.toCompleteString()).isEqualTo("x (integer) in [0.0, 2.0]");
    assertThat(listener.getCallbacks()).isEmpty();
  }

  @Test
  public void toCompleteString_continuousVariable() {
    Variable x = newVariable(
        /* lowerBound= */ 0.0,
        /* upperBound= */ 2.0,
        /* isInteger= */ false, "x", 2L, listener);

    assertThat(x.toCompleteString()).isEqualTo("x in [0.0, 2.0]");
    assertThat(listener.getCallbacks()).isEmpty();
  }

  @Test
  public void toCompleteString_continuousUnboundedVariable() {
    Variable x = newVariable(
        /* lowerBound= */ Double.NEGATIVE_INFINITY,
        /* upperBound= */ Double.POSITIVE_INFINITY,
        /* isInteger= */ false, "x", 2L, listener);

    assertThat(x.toCompleteString()).isEqualTo("x in [-Infinity, Infinity]");
    assertThat(listener.getCallbacks()).isEmpty();
  }

  @Test
  public void linearTermsToShortString_firstCoefOne_isSkipped() {
    Variable x = newBinaryVariable("x", 2L, listener);
    var expr = ImmutableMap.of(x, 1.0);

    assertThat(Variable.linearTermsToShortString(expr)).isEqualTo("x");
    assertThat(listener.getCallbacks()).isEmpty();
  }

  @Test
  public void linearTermsToShortString_firstCoefMinusOne_unaryMinus() {
    Variable x = newBinaryVariable("x", 2L, listener);
    var expr = ImmutableMap.of(x, -1.0);

    assertThat(Variable.linearTermsToShortString(expr)).isEqualTo("-x");
    assertThat(listener.getCallbacks()).isEmpty();
  }

  @Test
  public void linearTermsToShortString_firstCoefPosNumber_isShown() {
    Variable x = newBinaryVariable("x", 2L, listener);
    var expr = ImmutableMap.of(x, 4.0);

    assertThat(Variable.linearTermsToShortString(expr)).isEqualTo("4.0 * x");
    assertThat(listener.getCallbacks()).isEmpty();
  }

  @Test
  public void linearTermsToShortString_firstCoefNegNumber_isShownWithNegation() {
    Variable x = newBinaryVariable("x", 2L, listener);
    var expr = ImmutableMap.of(x, -4.0);

    assertThat(Variable.linearTermsToShortString(expr)).isEqualTo("-4.0 * x");
    assertThat(listener.getCallbacks()).isEmpty();
  }

  @Test
  public void linearTermsToShortString_firstCoefZero_skipsZeroAndRealFirstIsLeader() {
    Variable x = newBinaryVariable("x", 2L, listener);
    Variable y = newBinaryVariable("y", 4L, listener);
    var expr = ImmutableMap.of(x, 0.0, y, -1.0);

    assertThat(Variable.linearTermsToShortString(expr)).isEqualTo("-y");
    assertThat(listener.getCallbacks()).isEmpty();
  }

  @Test
  public void linearTermsToShortString_secondCoefOne_skipsSecondCoef() {
    Variable x = newBinaryVariable("x", 2L, listener);
    Variable y = newBinaryVariable("y", 4L, listener);
    var expr = ImmutableMap.of(x, 1.0, y, 1.0);

    assertThat(Variable.linearTermsToShortString(expr)).isEqualTo("x + y");
    assertThat(listener.getCallbacks()).isEmpty();
  }

  @Test
  public void linearTermsToShortString_secondCoefMinusOne_subtractsAndSkipsSecondCoef() {
    Variable x = newBinaryVariable("x", 2L, listener);
    Variable y = newBinaryVariable("y", 4L, listener);
    var expr = ImmutableMap.of(x, 1.0, y, -1.0);

    assertThat(Variable.linearTermsToShortString(expr)).isEqualTo("x - y");
    assertThat(listener.getCallbacks()).isEmpty();
  }

  @Test
  public void linearTermsToShortString_secondCoefPos_addsWithCoef() {
    Variable x = newBinaryVariable("x", 2L, listener);
    Variable y = newBinaryVariable("y", 4L, listener);
    var expr = ImmutableMap.of(x, 1.0, y, 4.0);

    assertThat(Variable.linearTermsToShortString(expr)).isEqualTo("x + 4.0 * y");
    assertThat(listener.getCallbacks()).isEmpty();
  }

  @Test
  public void linearTermsToShortString_secondCoefNeg_subtractsWithCoef() {
    Variable x = newBinaryVariable("x", 2L, listener);
    Variable y = newBinaryVariable("y", 4L, listener);
    var expr = ImmutableMap.of(x, 1.0, y, -4.0);

    assertThat(Variable.linearTermsToShortString(expr)).isEqualTo("x - 4.0 * y");
    assertThat(listener.getCallbacks()).isEmpty();
  }

  @Test
  public void linearTermsToShortString_withNaNs_rightSignNoCrash() {
    Variable x = newBinaryVariable("x", 2L, listener);
    Variable y = newBinaryVariable("y", 4L, listener);
    var expr = ImmutableMap.of(x, Double.NaN, y, Double.NaN);

    assertThat(Variable.linearTermsToShortString(expr)).isEqualTo("NaN * x + NaN * y");
    assertThat(listener.getCallbacks()).isEmpty();
  }

  @Test
  public void appendToProto_writesVariableToProto() {
    Variable x = newVariable(
        /* lowerBound= */ -1.0,
        /* upperBound= */ 2.0,
        /* isInteger= */ false, "x", 2L, listener);
    VariablesProto.Builder proto = VariablesProto.newBuilder();

    x.appendToProto(proto);

    assertThat(proto.build())
        .isEqualTo(VariablesProto.newBuilder()
                .addIds(2)
                .addLowerBounds(-1.0)
                .addUpperBounds(2.0)
                .addIntegers(false)
                .addNames("x")
                .build());
    assertThat(listener.getCallbacks()).isEmpty();
  }
}
