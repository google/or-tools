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
import static org.junit.jupiter.api.Assertions.assertThrows;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Objects;
import java.util.Optional;
import java.util.OptionalDouble;
import org.jspecify.annotations.Nullable;
import org.junit.jupiter.api.Test;

public final class LinearConstraintTest {
  private enum Callback { LB, UB, TERM }

  /**
   * Representation of an invocation of a function on LinearConstraint.OnChangeListener.
   *
   * <p>This is used with TestListener (defined below) to record all callback invocations of a
   * LinearConstraint to test that the correct callbacks are invoked. This class is a union type for
   * the arguments of the callback functions.
   *
   * <p>Of note, {@link #variable} and {@link #coefficient} are set only when {@link #callback} is
   * {@code TERM}. Users must help maintain this invariant (marking these fields private has no
   * effect, see go/java-practices/redundancy#visibility-specifiers-in-private-classes). In
   * particular, use the factory methods below ({@link #lb(LinearConstraint)}, {@link
   * #ub(LinearConstraint)}, and {@link #term(LinearConstraint, Variable, double)} to create
   * instances, rather than the constructor, use {@link #equals(Object)} to compare two instances
   * rather than reaching into the fields.
   */
  private static final class CallbackArgs {
    // Which callback of LinearConstraint.OnChangeListener was invoked.
    final Callback callback;
    // Used for all callbacks.
    final LinearConstraint linearConstraint;
    // Used only when callback is TERM (otherwise is null).
    final Optional<Variable> variable;
    // Used only when callback is TERM (otherwise is isPresent() is false).
    final OptionalDouble coefficient;

    static CallbackArgs lb(LinearConstraint constraint) {
      return new CallbackArgs(Callback.LB, constraint, Optional.empty(), OptionalDouble.empty());
    }

    static CallbackArgs ub(LinearConstraint constraint) {
      return new CallbackArgs(Callback.UB, constraint, Optional.empty(), OptionalDouble.empty());
    }

    static CallbackArgs term(LinearConstraint constraint, Variable variable, double coefficient) {
      return new CallbackArgs(
          Callback.TERM, constraint, Optional.of(variable), OptionalDouble.of(coefficient));
    }

    @Override
    public String toString() {
      var result = new StringBuilder()
                       .append("{callback: ")
                       .append(callback)
                       .append(", linearConstraint: ")
                       .append(linearConstraint);
      if (callback == Callback.TERM) {
        result.append(", variable: ")
            .append(variable.map(Variable::toString))
            .append(", coefficient: ")
            .append(coefficient);
      }
      return result.append("}").toString();
    }

    @Override
    public boolean equals(@Nullable Object other) {
      if (!(other instanceof CallbackArgs)) {
        return false;
      }
      var that = (CallbackArgs) other;
      return this.callback.equals(that.callback) && this.linearConstraint == that.linearConstraint
          && this.variable.equals(that.variable) && this.coefficient.equals(that.coefficient);
    }

    @Override
    public int hashCode() {
      return Objects.hash(callback, linearConstraint, variable, coefficient);
    }

    /** For use within this class only, prefer the factory functions above. */
    CallbackArgs(Callback callback, LinearConstraint linearConstraint, Optional<Variable> variable,
        OptionalDouble coefficient) {
      this.callback = callback;
      this.linearConstraint = linearConstraint;
      this.variable = variable;
      this.coefficient = coefficient;
    }
  }

  private static final class TestListener implements LinearConstraint.OnChangeListener {
    final List<CallbackArgs> invokedCallbacks = new ArrayList<>();

    @Override
    public void onLowerBoundChanged(LinearConstraint constraint) {
      invokedCallbacks.add(CallbackArgs.lb(constraint));
    }

    @Override
    public void onUpperBoundChanged(LinearConstraint constraint) {
      invokedCallbacks.add(CallbackArgs.ub(constraint));
    }

    @Override
    public void onTermChanged(LinearConstraint constraint, Variable variable, double coefficient) {
      invokedCallbacks.add(CallbackArgs.term(constraint, variable, coefficient));
    }

    List<CallbackArgs> getUnmodifiableInvokedCallbacks() {
      return Collections.unmodifiableList(invokedCallbacks);
    }

    void clearInvokedCallbacks() {
      invokedCallbacks.clear();
    }
  }

  @Test
  public void linearConstraintGetters_simpleAccessOk() {
    var modelId = new ModelId("test_model");
    var listener = new TestListener();
    var constraint = new LinearConstraint(
        /* lowerBound= */ 1.0, /* upperBound= */ 2.0, /* id= */ 3L, "c", listener, modelId);

    assertThat(constraint.getLowerBound()).isEqualTo(1.0);
    assertThat(constraint.getUpperBound()).isEqualTo(2.0);
    assertThat(constraint.getId()).isEqualTo(3L);
    assertThat(constraint.getName()).isEqualTo("c");
    assertThat(constraint.getUnmodifiableTerms()).isEmpty();
    assertThat(constraint.isDeleted()).isFalse();
    assertThat(constraint.getModelId()).isSameInstanceAs(modelId);
    assertThat(listener.getUnmodifiableInvokedCallbacks()).isEmpty();
  }

  @Test
  public void setLowerBound_boundChanged_hasNewValueAndCallbackInvoked() {
    var modelId = new ModelId("test_model");
    var listener = new TestListener();
    var constraint = new LinearConstraint(
        /* lowerBound= */ 1.0, /* upperBound= */ 2.0, /* id= */ 3L, "c", listener, modelId);

    assertThat(constraint.setLowerBound(-2.0)).isSameInstanceAs(constraint);

    assertThat(constraint.getLowerBound()).isEqualTo(-2.0);
    assertThat(listener.getUnmodifiableInvokedCallbacks())
        .containsExactly(CallbackArgs.lb(constraint));
  }

  @Test
  public void setLowerBound_boundSame_callbackNotInvoked() {
    var modelId = new ModelId("test_model");
    var listener = new TestListener();
    var constraint = new LinearConstraint(
        /* lowerBound= */ 1.0, /* upperBound= */ 2.0, /* id= */ 3L, "c", listener, modelId);

    assertThat(constraint.setLowerBound(1.0)).isSameInstanceAs(constraint);

    assertThat(constraint.getLowerBound()).isEqualTo(1.0);
    assertThat(listener.getUnmodifiableInvokedCallbacks()).isEmpty();
  }

  @Test
  public void setUpperBound_boundChanged_hasNewValueAndCallbackInvoked() {
    var modelId = new ModelId("test_model");
    var listener = new TestListener();
    var constraint = new LinearConstraint(
        /* lowerBound= */ 1.0, /* upperBound= */ 2.0, /* id= */ 3L, "c", listener, modelId);

    assertThat(constraint.setUpperBound(4.0)).isSameInstanceAs(constraint);

    assertThat(constraint.getUpperBound()).isEqualTo(4.0);
    assertThat(listener.getUnmodifiableInvokedCallbacks())
        .containsExactly(CallbackArgs.ub(constraint));
  }

  @Test
  public void setUpperBound_boundSame_callbackNotInvoked() {
    var modelId = new ModelId("test_model");
    var listener = new TestListener();
    var constraint = new LinearConstraint(
        /* lowerBound= */ 1.0, /* upperBound= */ 2.0, /* id= */ 3L, "c", listener, modelId);

    assertThat(constraint.setUpperBound(2.0)).isSameInstanceAs(constraint);

    assertThat(constraint.getUpperBound()).isEqualTo(2.0);
    assertThat(listener.getUnmodifiableInvokedCallbacks()).isEmpty();
  }

  @Test
  public void setTermWasZero_coefficientChanged_hasNewValueAndCallbackInvoked() {
    var modelId = new ModelId("test_model");
    var listener = new TestListener();
    var c = new LinearConstraint(
        /* lowerBound= */ 1.0, /* upperBound= */ 2.0, /* id= */ 3L, "c", listener, modelId);
    var variables = new Variables(modelId);
    Variable x = variables.addVariable("x");

    assertThat(c.setTerm(x, 3.0)).isSameInstanceAs(c);

    assertThat(c.getUnmodifiableTerms()).containsExactly(x, 3.0);
    assertThat(c.getTerm(x)).isEqualTo(3.0);
    assertThat(listener.getUnmodifiableInvokedCallbacks())
        .containsExactly(CallbackArgs.term(c, x, 3.0));
  }

  @Test
  public void setTermWasZero_coefficientSame_callbackNotInvoked() {
    var modelId = new ModelId("test_model");
    var listener = new TestListener();
    var c = new LinearConstraint(
        /* lowerBound= */ 1.0, /* upperBound= */ 2.0, /* id= */ 3L, "c", listener, modelId);
    var variables = new Variables(modelId);
    Variable x = variables.addVariable("x");

    assertThat(c.setTerm(x, 0.0)).isSameInstanceAs(c);

    assertThat(c.getUnmodifiableTerms()).isEmpty();
    assertThat(c.getTerm(x)).isEqualTo(0.0);
    assertThat(listener.getUnmodifiableInvokedCallbacks()).isEmpty();
  }

  @Test
  public void setTermWasNonzero_coefficientChangedStillNonzero_hasNewValueAndCallbackInvoked() {
    var modelId = new ModelId("test_model");
    var listener = new TestListener();
    var c = new LinearConstraint(
        /* lowerBound= */ 1.0, /* upperBound= */ 2.0, /* id= */ 3L, "c", listener, modelId);
    var variables = new Variables(modelId);
    Variable x = variables.addVariable("x");
    c.setTerm(x, 3.0);
    listener.clearInvokedCallbacks();

    assertThat(c.setTerm(x, 5.0)).isSameInstanceAs(c);

    assertThat(c.getUnmodifiableTerms()).containsExactly(x, 5.0);
    assertThat(c.getTerm(x)).isEqualTo(5.0);
    assertThat(listener.getUnmodifiableInvokedCallbacks())
        .containsExactly(CallbackArgs.term(c, x, 5.0));
  }

  @Test
  public void setTermWasNonzero_coefficientChangedToZero_hasNewValueAndCallbackInvoked() {
    var modelId = new ModelId("test_model");
    var listener = new TestListener();
    var constraint = new LinearConstraint(
        /* lowerBound= */ 1.0, /* upperBound= */ 2.0, /* id= */ 3L, "c", listener, modelId);
    var variables = new Variables(modelId);
    Variable x = variables.addVariable("x");
    constraint.setTerm(x, 3.0);
    listener.clearInvokedCallbacks();

    assertThat(constraint.setTerm(x, 0.0)).isSameInstanceAs(constraint);

    assertThat(constraint.getUnmodifiableTerms()).isEmpty();
    assertThat(constraint.getTerm(x)).isEqualTo(0.0);
    assertThat(listener.getUnmodifiableInvokedCallbacks())
        .containsExactly(CallbackArgs.term(constraint, x, 0.0));
  }

  @Test
  public void setTermWasNonzero_coefficientSame_callbackNotInvoked() {
    var modelId = new ModelId("test_model");
    var listener = new TestListener();
    var constraint = new LinearConstraint(
        /* lowerBound= */ 1.0, /* upperBound= */ 2.0, /* id= */ 3L, "c", listener, modelId);
    var variables = new Variables(modelId);
    Variable x = variables.addVariable("x");
    constraint.setTerm(x, 3.0);
    listener.clearInvokedCallbacks();

    assertThat(constraint.setTerm(x, 3.0)).isSameInstanceAs(constraint);

    assertThat(constraint.getUnmodifiableTerms()).containsExactly(x, 3.0);
    assertThat(constraint.getTerm(x)).isEqualTo(3.0);
    assertThat(listener.getUnmodifiableInvokedCallbacks()).isEmpty();
  }

  @Test
  public void markDeleted_isDeletedAfter() {
    var modelId = new ModelId("test_model");
    var listener = new TestListener();
    var constraint = new LinearConstraint(
        /* lowerBound= */ 0.0, /* upperBound= */ 1.0, /* id= */ 34L, "", listener, modelId);

    constraint.markDeleted();

    assertThat(constraint.isDeleted()).isTrue();
  }

  @Test
  public void setLowerBound_afterDeleted_throws() {
    var modelId = new ModelId("test_model");
    var listener = new TestListener();
    var constraint = new LinearConstraint(
        /* lowerBound= */ 0.0, /* upperBound= */ 1.0, /* id= */ 34L, "c", listener, modelId);
    constraint.markDeleted();

    assertThat(assertThrows(IllegalArgumentException.class, () -> constraint.setLowerBound(-2.0)))
        .hasMessageThat()
        .contains("deleted");
  }

  @Test
  public void setUpperBound_afterDeleted_throws() {
    var modelId = new ModelId("test_model");
    var listener = new TestListener();
    var constraint = new LinearConstraint(
        /* lowerBound= */ 0.0, /* upperBound= */ 1.0, /* id= */ 34L, "c", listener, modelId);
    constraint.markDeleted();

    assertThat(assertThrows(IllegalArgumentException.class, () -> constraint.setUpperBound(2.0)))
        .hasMessageThat()
        .contains("deleted");
  }

  @Test
  public void setTerm_afterConstraintDeleted_throws() {
    var modelId = new ModelId("test_model");
    var listener = new TestListener();
    var constraint = new LinearConstraint(
        /* lowerBound= */ 0.0, /* upperBound= */ 1.0, /* id= */ 34L, "c", listener, modelId);
    var variables = new Variables(modelId);
    Variable x = variables.addVariable("x");
    constraint.markDeleted();

    assertThat(assertThrows(IllegalArgumentException.class, () -> constraint.setTerm(x, 1.0)))
        .hasMessageThat()
        .contains("deleted");
  }

  @Test
  public void setTerm_afterVariableDeleted_throws() {
    var modelId = new ModelId("test_model");
    var listener = new TestListener();
    var constraint = new LinearConstraint(
        /* lowerBound= */ 0.0, /* upperBound= */ 1.0, /* id= */ 34L, "c", listener, modelId);
    var variables = new Variables(modelId);
    Variable x = variables.addVariable("x");
    variables.deleteVariable(x);

    assertThat(assertThrows(IllegalArgumentException.class, () -> constraint.setTerm(x, 1.0)))
        .hasMessageThat()
        .contains("deleted");
  }

  @Test
  public void setTerm_variableWrongModel_throws() {
    var modelId = new ModelId("test_model");
    var listener = new TestListener();
    var constraint = new LinearConstraint(
        /* lowerBound= */ 0.0, /* upperBound= */ 1.0, /* id= */ 34L, "c", listener, modelId);
    var modelId2 = new ModelId("test_model2");
    var variables = new Variables(modelId2);
    Variable x = variables.addVariable("x");

    assertThat(assertThrows(IllegalArgumentException.class, () -> constraint.setTerm(x, 1.0)))
        .hasMessageThat()
        .contains("another model");
  }

  @Test
  public void getTerm_variableWrongModel_throws() {
    var modelId = new ModelId("test_model");
    var listener = new TestListener();
    var constraint = new LinearConstraint(
        /* lowerBound= */ 0.0, /* upperBound= */ 1.0, /* id= */ 34L, "c", listener, modelId);
    var modelId2 = new ModelId("test_model2");
    var variables = new Variables(modelId2);
    Variable x = variables.addVariable("x");

    assertThat(assertThrows(IllegalArgumentException.class, () -> constraint.getTerm(x)))
        .hasMessageThat()
        .contains("another model");
  }

  @Test
  public void toString_emptyName_hasId() {
    var modelId = new ModelId("test_model");
    var listener = new TestListener();
    var constraint = new LinearConstraint(
        /* lowerBound= */ 0.0, /* upperBound= */ 1.0, /* id= */ 34L, "", listener, modelId);

    assertThat(constraint.toString()).contains("34");
  }

  @Test
  public void toString_hasName_matchesName() {
    var modelId = new ModelId("test_model");
    var listener = new TestListener();
    var constraint = new LinearConstraint(
        /* lowerBound= */ 0.0, /* upperBound= */ 1.0, /* id= */ 34L, "agj", listener, modelId);

    assertThat(constraint.toString()).isEqualTo("agj");
  }

  @Test
  public void appendToProto_simpleConstraint() {
    var modelId = new ModelId("test_model");
    var listener = new TestListener();
    var constraint = new LinearConstraint(
        /* lowerBound= */ 0.0, /* upperBound= */ 1.0, /* id= */ 3L, "c", listener, modelId);
    var linearConstraintProto = LinearConstraintsProto.newBuilder();

    constraint.appendToProto(linearConstraintProto);

    assertThat(linearConstraintProto.build())
        .ignoringFieldAbsence()
        .isEqualTo(LinearConstraintsProto.newBuilder()
                .addIds(3)
                .addLowerBounds(0.0)
                .addUpperBounds(1.0)
                .addNames("c")
                .build());
  }

  @Test
  public void toCompleteString_constraintWithoutTerms_usesZeroForTerms() {
    var modelId = new ModelId("test_model");
    var listener = new TestListener();
    var constraint = new LinearConstraint(
        /* lowerBound= */ -1.0, /* upperBound= */ 1.0, /* id= */ 3L, "agj", listener, modelId);
    assertThat(constraint.toCompleteString()).isEqualTo("agj: -1.0 ≤ 0.0 ≤ 1.0");
  }

  @Test
  public void toCompleteString_constraintIsDeleted_markedDeleted() {
    var modelId = new ModelId("test_model");
    var listener = new TestListener();
    var constraint = new LinearConstraint(
        /* lowerBound= */ -1.0, /* upperBound= */ 1.0, /* id= */ 3L, "agj", listener, modelId);
    constraint.markDeleted();
    assertThat(constraint.toCompleteString()).isEqualTo("agj: -1.0 ≤ 0.0 ≤ 1.0 (deleted)");
  }

  @Test
  public void toCompleteString_constraintWithTerms_hasTerms() {
    var modelId = new ModelId("test_model");
    var listener = new TestListener();
    var constraint = new LinearConstraint(
        /* lowerBound= */ -1.0, /* upperBound= */ 1.0, /* id= */ 3L, "agj", listener, modelId);
    var variables = new Variables(modelId);
    Variable x = variables.addVariable("x");
    Variable y = variables.addVariable("y");
    constraint.setTerm(x, 2.0);
    constraint.setTerm(y, 1.0);
    assertThat(constraint.toCompleteString()).isEqualTo("agj: -1.0 ≤ 2.0 * x + y ≤ 1.0");
  }

  @Test
  public void toCompleteString_withNaNs_rightSignNoCrash() {
    var modelId = new ModelId("test_model");
    var listener = new TestListener();
    var constraint = new LinearConstraint(
        /* lowerBound= */ Double.NaN,
        /* upperBound= */ Double.NaN,
        /* id= */ 3L, "agj", listener, modelId);
    var variables = new Variables(modelId);
    Variable x = variables.addVariable("x");
    Variable y = variables.addVariable("y");
    constraint.setTerm(x, Double.NaN);
    constraint.setTerm(y, Double.NaN);
    assertThat(constraint.toCompleteString()).isEqualTo("agj: NaN ≤ NaN * x + NaN * y ≤ NaN");
  }

  @Test
  public void onVariableDeleted_hasCoefficient_isRemoved() {
    var modelId = new ModelId("test_model");
    var listener = new TestListener();
    var constraint = new LinearConstraint(
        /* lowerBound= */ 0.0, /* upperBound= */ 1.0, /* id= */ 3L, "c", listener, modelId);
    var variables = new Variables(modelId);
    Variable x = variables.addVariable("x");
    Variable y = variables.addVariable("y");
    constraint.setTerm(x, 2.0);
    constraint.setTerm(y, 1.0);

    constraint.onVariableDeleted(x);

    assertThat(constraint.getUnmodifiableTerms()).containsExactly(y, 1.0);
    assertThat(constraint.isDeleted()).isFalse();
  }

  @Test
  public void onVariableDeleted_hadNoCoefficient_noEffect() {
    var modelId = new ModelId("test_model");
    var listener = new TestListener();
    var constraint = new LinearConstraint(
        /* lowerBound= */ 0.0, /* upperBound= */ 1.0, /* id= */ 3L, "c", listener, modelId);
    var variables = new Variables(modelId);
    Variable x = variables.addVariable("x");
    Variable y = variables.addVariable("y");
    constraint.setTerm(y, 1.0);

    constraint.onVariableDeleted(x);

    assertThat(constraint.getUnmodifiableTerms()).containsExactly(y, 1.0);
    assertThat(constraint.isDeleted()).isFalse();
  }

  @Test
  public void accumulatorTest_addExpression_updatesTermsAndBounds() {
    var modelId = new ModelId("test_model");
    var listener = new TestListener();
    var constraint = new LinearConstraint(
        /* lowerBound= */ 0.0, /* upperBound= */ 5.0, /* id= */ 3L, "c", listener, modelId);
    var variables = new Variables(modelId);
    Variable x = variables.addVariable("x");
    constraint.setTerm(x, 2.0);
    listener.clearInvokedCallbacks();

    var expr = new HashLinearExpression(3.0).add(2.0, x);
    expr.addTo(2.0, constraint.getTermsSink());

    assertThat(constraint.getLowerBound()).isEqualTo(-6.0);
    assertThat(constraint.getUpperBound()).isEqualTo(-1.0);
    assertThat(constraint.getUnmodifiableTerms()).containsExactly(x, 6.0);
    assertThat(listener.getUnmodifiableInvokedCallbacks())
        .containsExactly(CallbackArgs.lb(constraint), CallbackArgs.ub(constraint),
            CallbackArgs.term(constraint, x, 6.0));
  }
}
