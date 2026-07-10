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
import static org.junit.jupiter.api.Assertions.assertThrows;

import com.google.common.collect.ImmutableList;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Objects;
import java.util.Optional;
import java.util.OptionalDouble;
import java.util.OptionalLong;
import org.jspecify.annotations.Nullable;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

public final class ObjectiveTest {
  private ModelId modelId;
  private Variables variables;
  private TestListener listener;

  private enum Callback {
    SENSE,
    OFFSET,
    LINEAR_TERM,
    PRIORITY,
  }

  private static final class ObjectiveValues {
    final Callback callback;
    final @Nullable Boolean objectiveMaximize;
    final OptionalDouble objectiveOffset;
    final OptionalDouble objectiveVariableValue;
    final OptionalLong priorityValue;

    static ObjectiveValues sense(boolean isMaximize) {
      return new ObjectiveValues(Callback.SENSE, isMaximize, OptionalDouble.empty(),
          OptionalDouble.empty(), OptionalLong.empty());
    }

    static ObjectiveValues offset(double offset) {
      return new ObjectiveValues(Callback.SENSE, null, OptionalDouble.of(offset),
          OptionalDouble.empty(), OptionalLong.empty());
    }

    static ObjectiveValues linearTerm(double coefficient) {
      return new ObjectiveValues(Callback.SENSE, null, OptionalDouble.empty(),
          OptionalDouble.of(coefficient), OptionalLong.empty());
    }

    static ObjectiveValues priority(long priority) {
      return new ObjectiveValues(Callback.SENSE, null, OptionalDouble.empty(),
          OptionalDouble.empty(), OptionalLong.of(priority));
    }

    @Override
    public String toString() {
      var result = new StringBuilder().append("{callback: ").append(callback);
      switch (callback) {
        case SENSE -> result.append(", objectiveMaximize: ").append(objectiveMaximize);
        case OFFSET -> result.append(", objectiveOffset: ").append(objectiveOffset.getAsDouble());
        case LINEAR_TERM ->
          result.append(", objectiveVariableValue: ").append(objectiveVariableValue.getAsDouble());
        case PRIORITY -> result.append(", priorityValue: ").append(priorityValue.getAsLong());
      }
      result.append("}");
      return result.toString();
    }

    @Override
    public boolean equals(@Nullable Object other) {
      if (!(other instanceof ObjectiveValues)) {
        return false;
      }
      var that = (ObjectiveValues) other;
      return this.callback.equals(that.callback)
          && Objects.equals(this.objectiveMaximize, that.objectiveMaximize)
          && this.objectiveOffset.equals(that.objectiveOffset)
          && this.objectiveVariableValue.equals(that.objectiveVariableValue)
          && this.priorityValue.equals(that.priorityValue);
    }

    @Override
    public int hashCode() {
      return Objects.hash(
          callback, objectiveMaximize, objectiveOffset, objectiveVariableValue, priorityValue);
    }

    ObjectiveValues(Callback callback, @Nullable Boolean isObjectiveMaximize,
        OptionalDouble objectiveOffset, OptionalDouble objectiveVariableValue,
        OptionalLong priorityValue) {
      this.callback = callback;
      objectiveMaximize = isObjectiveMaximize;
      this.objectiveOffset = objectiveOffset;
      this.objectiveVariableValue = objectiveVariableValue;
      this.priorityValue = priorityValue;
    }
  }

  /**
   * Representation of an invocation of a function on Objective.OnChangeListener.
   *
   * <p>This is used with TestListener (defined below) to record all callback invocations of an
   * Objective to test that the correct callbacks are invoked. This class is a union type for the
   * arguments of the callback functions.
   *
   * <p>Of note, {@link #variable} and {@link #coefficient} are set only when {@link #callback} is
   * {@code LINEAR_TERM}. Users must help maintain this invariant (marking these fields private has
   * no effect, see go/java-practices/redundancy#visibility-specifiers-in-private-classes). In
   * particular, use the factory methods below ({@link #sense(Objective)}, {@link
   * #offset(Objective)}, and {@link #linearTerm(Objective, Variable, double)} to create instances,
   * rather than the constructor, use {@link #equals(Object)} to compare two instances rather than
   * reaching into the fields.
   */
  private static final class CallbackArgsAndObjectiveValues {
    // Which callback of LinearConstraint.OnChangeListener was invoked.
    final Callback callback;
    // The (relevant) state of the objective when the callback was invoked.
    final ObjectiveValues objectiveValues;
    // Used for all callbacks.
    final Objective objective;
    // `variable` is used only when callback is LINEAR_TERM and contains the variable whose
    // coefficient changed, otherwise the Optional is empty.
    final Optional<Variable> variable;
    // `coefficient` is used only when callback is LINEAR_TERM and contains the value of the
    // linear objective coefficient that changed, otherwise the Optional is empty.
    final OptionalDouble coefficient;

    // Use for expected
    static CallbackArgsAndObjectiveValues sense(Objective objective, boolean isMaximize) {
      return new CallbackArgsAndObjectiveValues(Callback.SENSE, objective, Optional.empty(),
          OptionalDouble.empty(), ObjectiveValues.sense(isMaximize));
    }

    // Use for actual
    static CallbackArgsAndObjectiveValues sense(Objective objective) {
      return sense(objective, objective.isMaximize());
    }

    // Use for expected
    static CallbackArgsAndObjectiveValues offset(Objective objective, double offset) {
      return new CallbackArgsAndObjectiveValues(Callback.OFFSET, objective, Optional.empty(),
          OptionalDouble.empty(), ObjectiveValues.offset(offset));
    }

    // Use for actual
    static CallbackArgsAndObjectiveValues offset(Objective objective) {
      return offset(objective, objective.getOffset());
    }

    // Use for expected
    static CallbackArgsAndObjectiveValues linearTerm(Objective objective, Variable variable,
        double callbackCoefficient, double objectiveCoefficient) {
      return new CallbackArgsAndObjectiveValues(Callback.LINEAR_TERM, objective,
          Optional.of(variable), OptionalDouble.of(callbackCoefficient),
          ObjectiveValues.linearTerm(objectiveCoefficient));
    }

    // Use for actual
    static CallbackArgsAndObjectiveValues linearTerm(
        Objective objective, Variable variable, double coefficient) {
      return linearTerm(objective, variable, coefficient, objective.getLinearTerm(variable));
    }

    // Use for expected
    static CallbackArgsAndObjectiveValues priority(Objective objective, long priorityValue) {
      return new CallbackArgsAndObjectiveValues(Callback.PRIORITY, objective, Optional.empty(),
          OptionalDouble.empty(), ObjectiveValues.priority(priorityValue));
    }

    // Use for actual
    static CallbackArgsAndObjectiveValues priority(Objective objective) {
      return priority(objective, objective.getPriority());
    }

    @Override
    public String toString() {
      var result = new StringBuilder()
                       .append("{callback: ")
                       .append(callback)
                       .append(", objective: ")
                       .append(objective);
      if (callback == Callback.LINEAR_TERM) {
        result.append(", variable: ")
            .append(variable.map(Variable::toString))
            .append(", coefficient: ")
            .append(coefficient);
      }
      result.append(", objectiveValues: ").append(objectiveValues);
      return result.append("}").toString();
    }

    @Override
    public boolean equals(@Nullable Object other) {
      if (!(other instanceof CallbackArgsAndObjectiveValues)) {
        return false;
      }
      var that = (CallbackArgsAndObjectiveValues) other;
      return this.callback.equals(that.callback) && this.objective == that.objective
          && this.variable.equals(that.variable) && this.coefficient.equals(that.coefficient)
          && this.objectiveValues.equals(that.objectiveValues);
    }

    @Override
    public int hashCode() {
      return Objects.hash(callback, objective, variable, coefficient, objectiveValues);
    }

    /** For use within this class only, prefer the factory functions above. */
    CallbackArgsAndObjectiveValues(Callback callback, Objective objective,
        Optional<Variable> variable, OptionalDouble coefficient, ObjectiveValues objectiveValues) {
      this.callback = callback;
      this.objective = objective;
      this.variable = variable;
      this.coefficient = coefficient;
      this.objectiveValues = objectiveValues;
    }
  }

  private static final class TestListener implements Objective.OnChangeListener {
    final List<CallbackArgsAndObjectiveValues> invokedCallbacks = new ArrayList<>();

    List<CallbackArgsAndObjectiveValues> getUnmodifiableInvokedCallbacks() {
      return Collections.unmodifiableList(invokedCallbacks);
    }

    void clearInvokedCallbacks() {
      invokedCallbacks.clear();
    }

    @Override
    public void onSenseChanged(Objective objective) {
      invokedCallbacks.add(CallbackArgsAndObjectiveValues.sense(objective));
    }

    @Override
    public void onOffsetChanged(Objective objective) {
      invokedCallbacks.add(CallbackArgsAndObjectiveValues.offset(objective));
    }

    @Override
    public void onLinearTermChange(Objective objective, Variable variable, double coefficient) {
      invokedCallbacks.add(
          CallbackArgsAndObjectiveValues.linearTerm(objective, variable, coefficient));
    }

    @Override
    public void onPriorityChanged(Objective objective) {
      invokedCallbacks.add(CallbackArgsAndObjectiveValues.priority(objective));
    }
  }

  @BeforeEach
  public void setUp() {
    modelId = new ModelId("test_model");
    variables = new Variables(modelId);
    listener = new TestListener();
  }

  @Test
  public void newObjective_isEmpty_gettersAreEmpty() {
    var objective = new Objective(/* priority= */ 4L, "agx", listener, modelId);

    assertThat(objective.isMaximize()).isFalse();
    assertThat(objective.getOffset()).isEqualTo(0.0);
    assertThat(objective.getPriority()).isEqualTo(4L);
    assertThat(objective.getName()).isEqualTo("agx");
    assertThat(objective.getUnmodifiableLinearTerms()).isEmpty();
    assertThat(objective.getModelId()).isSameInstanceAs(modelId);
    assertThat(listener.getUnmodifiableInvokedCallbacks()).isEmpty();
  }

  @Test
  public void getLinearTerm_variableNotSet_isZero() {
    var objective = new Objective(0L, "", listener, modelId);
    Variable x = variables.addVariable("x");

    assertThat(objective.getLinearTerm(x)).isEqualTo(0.0);
  }

  @Test
  public void setMaximize_senseChanged_senseUpdatedAndCallbackInvoked() {
    var objective = new Objective(0L, "", listener, modelId);

    assertThat(objective.setMaximize(true)).isSameInstanceAs(objective);

    assertThat(objective.isMaximize()).isTrue();
    assertThat(listener.getUnmodifiableInvokedCallbacks())
        .containsExactly(CallbackArgsAndObjectiveValues.sense(objective, /* isMaximize= */ true));
  }

  @Test
  public void setMaximize_alreadyMaximizeSenseUnchanged_noCallbackInvoked() {
    var objective = new Objective(0L, "", listener, modelId);
    objective.setMaximize(true);
    listener.clearInvokedCallbacks();

    assertThat(objective.setMaximize(true)).isSameInstanceAs(objective);

    assertThat(objective.isMaximize()).isTrue();
    assertThat(listener.getUnmodifiableInvokedCallbacks()).isEmpty();
  }

  @Test
  public void setMaximize_changedToMin_senseUpdatedAndCallbackInvoked() {
    var objective = new Objective(0L, "", listener, modelId);
    objective.setMaximize(true);
    listener.clearInvokedCallbacks();

    assertThat(objective.setMaximize(false)).isSameInstanceAs(objective);

    assertThat(objective.isMaximize()).isFalse();
    assertThat(listener.getUnmodifiableInvokedCallbacks())
        .containsExactly(CallbackArgsAndObjectiveValues.sense(objective, /* isMaximize= */ false));
  }

  @Test
  public void setMaximize_alreadyMinimizeSenseUnchanged_noCallbackInvoked() {
    var objective = new Objective(0L, "", listener, modelId);

    assertThat(objective.setMaximize(false)).isSameInstanceAs(objective);

    assertThat(objective.isMaximize()).isFalse();
    assertThat(listener.getUnmodifiableInvokedCallbacks()).isEmpty();
  }

  @Test
  public void setOffset_offsetChanged_offsetUpdatedAndCallbackInvoked() {
    var objective = new Objective(/* priority= */ 0L, "", listener, modelId);

    assertThat(objective.setOffset(4.0)).isSameInstanceAs(objective);

    assertThat(objective.getOffset()).isEqualTo(4.0);
    assertThat(listener.getUnmodifiableInvokedCallbacks())
        .containsExactly(CallbackArgsAndObjectiveValues.offset(objective, 4.0));
  }

  @Test
  public void setOffset_offsetUnchanged_noCallbackInvoked() {
    var objective = new Objective(/* priority= */ 0L, "", listener, modelId);
    objective.setOffset(4.0);
    listener.clearInvokedCallbacks();

    assertThat(objective.setOffset(4.0)).isSameInstanceAs(objective);

    assertThat(objective.getOffset()).isEqualTo(4.0);
    assertThat(listener.getUnmodifiableInvokedCallbacks()).isEmpty();
  }

  @Test
  public void setPriority_priorityChanged_priorityUpdatedAndCallbackInvoked() {
    var objective = new Objective(/* priority= */ 0L, "", listener, modelId);

    assertThat(objective.setPriority(6)).isSameInstanceAs(objective);

    assertThat(objective.getPriority()).isEqualTo(6);
    assertThat(listener.getUnmodifiableInvokedCallbacks())
        .containsExactly(CallbackArgsAndObjectiveValues.priority(objective, 6));
  }

  @Test
  public void setPriority_priorityUnchanged_noCallbackInvoked() {
    var objective = new Objective(/* priority= */ 6L, "", listener, modelId);

    assertThat(objective.setPriority(6L)).isSameInstanceAs(objective);

    assertThat(objective.getPriority()).isEqualTo(6L);
    assertThat(listener.getUnmodifiableInvokedCallbacks()).isEmpty();
  }

  @Test
  public void setPriority_negativePriority_throws() {
    var objective = new Objective(/* priority= */ 6L, "", listener, modelId);

    var error = assertThrows(IllegalArgumentException.class, () -> objective.setPriority(-2L));

    assertThat(error).hasMessageThat().contains("nonnegative");
    assertThat(listener.getUnmodifiableInvokedCallbacks()).isEmpty();
  }

  @Test
  public void setLinearTerm_termChanged_termIsSetAndCallbackInvoked() {
    var objective = new Objective(/* priority= */ 0L, "", listener, modelId);
    Variable x = variables.addVariable("x");

    assertThat(objective.setLinearTerm(x, 4.0)).isSameInstanceAs(objective);

    assertThat(objective.getLinearTerm(x)).isEqualTo(4.0);
    assertThat(objective.getUnmodifiableLinearTerms()).containsExactly(x, 4.0);
    assertThat(listener.getUnmodifiableInvokedCallbacks())
        .containsExactly(CallbackArgsAndObjectiveValues.linearTerm(objective, x, 4.0, 4.0));
  }

  @Test
  public void setLinearTerm_termUnchanged_noCallbackInvoked() {
    var objective = new Objective(/* priority= */ 0L, "", listener, modelId);
    Variable x = variables.addVariable("x");
    objective.setLinearTerm(x, 4.0);
    listener.clearInvokedCallbacks();

    assertThat(objective.setLinearTerm(x, 4.0)).isSameInstanceAs(objective);

    assertThat(objective.getLinearTerm(x)).isEqualTo(4.0);
    assertThat(objective.getUnmodifiableLinearTerms()).containsExactly(x, 4.0);
    assertThat(listener.getUnmodifiableInvokedCallbacks()).isEmpty();
  }

  @Test
  public void setLinearTerm_wasZeroSetToZero_notStored() {
    var objective = new Objective(/* priority= */ 0L, "", listener, modelId);
    Variable x = variables.addVariable("x");

    objective.setLinearTerm(x, 0.0);

    assertThat(objective.getLinearTerm(x)).isEqualTo(0.0);
    assertThat(objective.getUnmodifiableLinearTerms()).isEmpty();
    assertThat(listener.getUnmodifiableInvokedCallbacks()).isEmpty();
  }

  @Test
  public void setLinearTerm_wasNonzeroSetToZero_clearedFromStorage() {
    var objective = new Objective(/* priority= */ 0L, "", listener, modelId);
    Variable x = variables.addVariable("x");
    objective.setLinearTerm(x, 4.0);
    listener.clearInvokedCallbacks();

    objective.setLinearTerm(x, 0.0);

    assertThat(objective.getLinearTerm(x)).isEqualTo(0.0);
    assertThat(objective.getUnmodifiableLinearTerms()).isEmpty();
    assertThat(listener.getUnmodifiableInvokedCallbacks())
        .containsExactly(CallbackArgsAndObjectiveValues.linearTerm(objective, x, 0.0, 0.0));
  }

  @Test
  public void setLinearTerm_variableWasDeleted_throws() {
    var objective = new Objective(/* priority= */ 0L, "", listener, modelId);
    Variable x = variables.addVariable("x");
    x.markDeleted();

    var error = assertThrows(IllegalArgumentException.class, () -> objective.setLinearTerm(x, 1.0));

    assertThat(error).hasMessageThat().contains("deleted");
    assertThat(listener.getUnmodifiableInvokedCallbacks()).isEmpty();
  }

  @Test
  public void setLinearTerm_variableWrongModel_throws() {
    var objective = new Objective(/* priority= */ 0L, "", listener, modelId);
    var wrongModel = new Variables(new ModelId("wrong_model"));
    Variable x = wrongModel.addVariable("x");

    var error = assertThrows(IllegalArgumentException.class, () -> objective.setLinearTerm(x, 1.0));

    assertThat(error).hasMessageThat().contains("from another model");
    assertThat(error).hasMessageThat().contains("wrong_model");
    assertThat(listener.getUnmodifiableInvokedCallbacks()).isEmpty();
  }

  @Test
  public void getLinearTerm_variableWrongModel_throws() {
    var objective = new Objective(/* priority= */ 0L, "", listener, modelId);
    var wrongModel = new Variables(new ModelId("wrong_model"));
    Variable x = wrongModel.addVariable("x");

    var error = assertThrows(IllegalArgumentException.class, () -> objective.getLinearTerm(x));

    assertThat(error).hasMessageThat().contains("from another model");
    assertThat(error).hasMessageThat().contains("wrong_model");
    assertThat(listener.getUnmodifiableInvokedCallbacks()).isEmpty();
  }

  @Test
  public void clear_emptyObjective_noChanges() {
    var objective = new Objective(/* priority= */ 0L, "", listener, modelId);

    objective.clear();

    assertThat(objective.getOffset()).isEqualTo(0.0);
    assertThat(objective.getUnmodifiableLinearTerms()).isEmpty();
    assertThat(listener.getUnmodifiableInvokedCallbacks()).isEmpty();
  }

  @Test
  public void clear_fullObjective_isEmptyAndCallbackInvoked() {
    var objective = new Objective(/* priority= */ 0L, "", listener, modelId);
    Variable x = variables.addVariable("x");
    Variable y = variables.addVariable("y");
    objective.setMaximize(true);
    objective.setLinearTerm(x, 2.0);
    objective.setLinearTerm(y, 3.0);
    objective.setOffset(4.0);
    listener.clearInvokedCallbacks();

    objective.clear();

    assertThat(objective.isMaximize()).isTrue();
    assertThat(objective.getOffset()).isEqualTo(0.0);
    assertThat(objective.getUnmodifiableLinearTerms()).isEmpty();
    assertThat(listener.getUnmodifiableInvokedCallbacks())
        .containsExactly(CallbackArgsAndObjectiveValues.offset(objective, 0.0),
            CallbackArgsAndObjectiveValues.linearTerm(objective, x, 0.0, 0.0),
            CallbackArgsAndObjectiveValues.linearTerm(objective, y, 0.0, 0.0));
  }

  @Test
  public void toProto_emptyObjective_emptyProto() {
    var objective = new Objective(/* priority= */ 0L, "", listener, modelId);

    ObjectiveProto proto = objective.toProto();

    var expected = ObjectiveProto.getDefaultInstance();
    assertThat(proto).ignoringFieldAbsence().isEqualTo(expected);
  }

  @Test
  public void toProto_nonEmptyObjective_hasSenseOffsetAndTerms() {
    var objective = new Objective(/* priority= */ 5L, "agx", listener, modelId);
    Variable x = variables.addVariable("x");
    var unused = variables.addVariable("y");
    Variable z = variables.addVariable("z");
    objective.setMaximize(true);
    objective.setOffset(4.0);
    objective.setLinearTerm(x, 2.0);
    objective.setLinearTerm(z, 3.0);

    ObjectiveProto proto = objective.toProto();

    var expected =
        ObjectiveProto.newBuilder().setMaximize(true).setOffset(4.0).setPriority(5).setName("agx");
    expected.getLinearCoefficientsBuilder().addIds(0).addIds(2).addValues(2.0).addValues(3.0);

    assertThat(proto).isEqualTo(expected.build());
  }

  @Test
  public void toString_emptyName_justLabel() {
    var objective = new Objective(/* priority= */ 0L, "", listener, modelId);
    assertThat(objective.toString()).isEqualTo("Primary Objective");
  }

  @Test
  public void toString_hasName_nameAndLabel() {
    var objective = new Objective(/* priority= */ 0L, "abc", listener, modelId);
    assertThat(objective.toString()).isEqualTo("abc (Primary Objective)");
  }

  @Test
  public void toCompleteString_emptyObjective_isZero() {
    var objective = new Objective(/* priority= */ 0L, "", listener, modelId);
    assertThat(objective.toCompleteString())
        .isEqualTo("Primary Objective: minimize 0.0 priority: 0");
    assertThat(objective.toCompleteString(/* printPriority= */ false))
        .isEqualTo("Primary Objective: minimize 0.0");
    assertThat(objective.toCompleteString(/* printPriority= */ true))
        .isEqualTo("Primary Objective: minimize 0.0 priority: 0");
  }

  @Test
  public void toCompleteString_hasPriority_showsPriority() {
    var objective = new Objective(/* priority= */ 5L, "", listener, modelId);
    assertThat(objective.toCompleteString(true))
        .isEqualTo("Primary Objective: minimize 0.0 priority: 5");
  }

  @Test
  public void toCompleteString_offsetOnly_isLeading() {
    var objective = new Objective(/* priority= */ 0L, "", listener, modelId);
    objective.setMaximize(true);
    objective.setOffset(-4.0);
    assertThat(objective.toCompleteString(/* printPriority= */ false))
        .isEqualTo("Primary Objective: maximize -4.0");
  }

  @Test
  public void toCompleteString_linearTermsOnly_offsetSkipped() {
    var objective = new Objective(/* priority= */ 0L, "", listener, modelId);
    Variable x = variables.addVariable("x");
    var unused = variables.addVariable("y");
    Variable z = variables.addVariable("z");
    objective.setLinearTerm(x, 2.0);
    objective.setLinearTerm(z, 3.0);
    assertThat(objective.toCompleteString(/* printPriority= */ false))
        .isEqualTo("Primary Objective: minimize 2.0 * x + 3.0 * z");
  }

  @Test
  public void toCompleteString_offsetAndLinearTerms_offsetSubtracted() {
    var objective = new Objective(/* priority= */ 0L, "", listener, modelId);
    Variable x = variables.addVariable("x");
    Variable y = variables.addVariable("y");
    objective.setLinearTerm(x, 1.0);
    objective.setLinearTerm(y, -3.0);
    objective.setOffset(-4.0);
    assertThat(objective.toCompleteString(/* printPriority= */ false))
        .isEqualTo("Primary Objective: minimize x - 3.0 * y - 4.0");
  }

  //////////////////////////////////////////////////////////////////////////////////////////////////
  // Arithmetic operations on objective. These are implemented in terms of set and get. We only test
  // that arithmetic is correct, the listeners have already been tested.
  //////////////////////////////////////////////////////////////////////////////////////////////////

  @Test
  public void add_double_offsetCorrect() {
    var objective = new Objective(/* priority= */ 0L, "", listener, modelId);
    objective.setOffset(4.0);

    objective.add(2.0);

    assertThat(objective.getOffset()).isEqualTo(6.0);
  }

  @Test
  public void add_expression_hasSum() {
    var objective = new Objective(/* priority= */ 0L, "", listener, modelId);
    Variable x = variables.addVariable("x");
    Variable y = variables.addVariable("y");
    objective.setOffset(4.0).setLinearTerm(x, 2.0);

    objective.add(linExpr().add(2.0).add(3.0, x).add(4.0, y));

    assertThat(objective.getOffset()).isEqualTo(6.0);
    assertThat(objective.getUnmodifiableLinearTerms()).containsExactly(x, 5.0, y, 4.0);
  }

  @Test
  public void add_scaledExpression_hasSum() {
    var objective = new Objective(/* priority= */ 0L, "", listener, modelId);
    Variable x = variables.addVariable("x");
    Variable y = variables.addVariable("y");
    objective.setOffset(4.0).setLinearTerm(x, 2.0);

    objective.add(3.0, linExpr().add(2.0).add(3.0, x).add(4.0, y));

    assertThat(objective.getOffset()).isEqualTo(10.0);
    assertThat(objective.getUnmodifiableLinearTerms()).containsExactly(x, 11.0, y, 12.0);
  }

  @Test
  public void addAll_variableList_hasSum() {
    var objective = new Objective(/* priority= */ 0L, "", listener, modelId);
    Variable x = variables.addVariable("x");
    Variable y = variables.addVariable("y");
    objective.setOffset(4.0).setLinearTerm(x, 2.0);

    objective.addAll(ImmutableList.of(x, y, x));

    assertThat(objective.getOffset()).isEqualTo(4.0);
    assertThat(objective.getUnmodifiableLinearTerms()).containsExactly(x, 4.0, y, 1.0);
  }

  @Test
  public void addInnerProduct_variableList_hasSum() {
    var objective = new Objective(/* priority= */ 0L, "", listener, modelId);
    Variable x = variables.addVariable("x");
    Variable y = variables.addVariable("y");
    objective.setOffset(4.0).setLinearTerm(x, 2.0);

    objective.addInnerProduct(ImmutableList.of(2, 3, 4), ImmutableList.of(x, y, x));

    assertThat(objective.getOffset()).isEqualTo(4.0);
    assertThat(objective.getUnmodifiableLinearTerms()).containsExactly(x, 8.0, y, 3.0);
  }

  @Test
  public void subtract_double_offsetCorrect() {
    var objective = new Objective(/* priority= */ 0L, "", listener, modelId);
    objective.setOffset(5.0);

    objective.subtract(2.0);

    assertThat(objective.getOffset()).isEqualTo(3.0);
  }

  @Test
  public void subtract_expression_hasSum() {
    var objective = new Objective(/* priority= */ 0L, "", listener, modelId);
    Variable x = variables.addVariable("x");
    Variable y = variables.addVariable("y");
    objective.setOffset(5.0).setLinearTerm(x, 2.0);

    objective.subtract(linExpr().add(2.0).add(3.0, x).add(4.0, y));

    assertThat(objective.getOffset()).isEqualTo(3.0);
    assertThat(objective.getUnmodifiableLinearTerms()).containsExactly(x, -1.0, y, -4.0);
  }

  @Test
  public void subtract_scaledExpression_hasSum() {
    var objective = new Objective(/* priority= */ 0L, "", listener, modelId);
    Variable x = variables.addVariable("x");
    Variable y = variables.addVariable("y");
    objective.setOffset(4.0).setLinearTerm(x, 2.0);

    objective.subtract(3.0, linExpr().add(2.0).add(3.0, x).add(4.0, y));

    assertThat(objective.getOffset()).isEqualTo(-2.0);
    assertThat(objective.getUnmodifiableLinearTerms()).containsExactly(x, -7.0, y, -12.0);
  }
}
