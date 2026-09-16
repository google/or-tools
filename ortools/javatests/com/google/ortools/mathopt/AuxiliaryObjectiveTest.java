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
import static org.junit.jupiter.api.Assertions.assertThrows;

import com.google.common.collect.ImmutableList;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

public final class AuxiliaryObjectiveTest {
  private ModelId modelId;
  private Variables variables;
  private TestListener listener;

  private static final class TestListener implements Objective.OnChangeListener {
    boolean invoked = false;

    @Override
    public void onSenseChanged(Objective objective) {
      invoked = true;
    }

    @Override
    public void onOffsetChanged(Objective objective) {
      invoked = true;
    }

    @Override
    public void onLinearTermChange(Objective objective, Variable variable, double coefficient) {
      invoked = true;
    }

    @Override
    public void onPriorityChanged(Objective objective) {
      invoked = true;
    }

    public boolean wasInvoked() {
      return invoked;
    }
  }

  @BeforeEach
  public void setUp() {
    modelId = new ModelId("test_model");
    variables = new Variables(modelId);
    listener = new TestListener();
  }

  @Test
  public void auxObjectiveGetId_correctId() {
    var objective = new AuxiliaryObjective(/* priority= */ 0L, "", /* id= */ 2L, listener, modelId);
    assertThat(objective.getId()).isEqualTo(2L);
  }

  @Test
  public void auxObjectiveToString_emptyName_showsId() {
    var objective = new AuxiliaryObjective(/* priority= */ 0L, "", /* id= */ 2L, listener, modelId);
    assertThat(objective.toString()).isEqualTo("Objective 2");
  }

  @Test
  public void auxObjectiveToString_hasName_showsNameAndId() {
    var objective =
        new AuxiliaryObjective(/* priority= */ 0L, "a cat", /* id= */ 2L, listener, modelId);
    assertThat(objective.toString()).isEqualTo("a cat (Objective 2)");
  }

  @Test
  public void auxObjectiveMarkDeleted_isDeletedAfter() {
    var objective = new AuxiliaryObjective(/* priority= */ 0L, "", /* id= */ 2L, listener, modelId);

    objective.markDeleted();

    assertThat(objective.isDeleted()).isTrue();
  }

  @Test
  public void auxObjectiveSetMaximize_wasDeleted_throws() {
    var objective = new AuxiliaryObjective(/* priority= */ 0L, "", /* id= */ 2L, listener, modelId);
    objective.markDeleted();

    var error = assertThrows(IllegalArgumentException.class, () -> objective.setMaximize(true));

    assertThat(error).hasMessageThat().contains("deleted");
    assertThat(listener.wasInvoked()).isFalse();
  }

  @Test
  public void auxObjectiveSetOffset_wasDeleted_throws() {
    var objective = new AuxiliaryObjective(/* priority= */ 0L, "", /* id= */ 2L, listener, modelId);
    objective.markDeleted();

    var error = assertThrows(IllegalArgumentException.class, () -> objective.setOffset(4.0));

    assertThat(error).hasMessageThat().contains("deleted");
    assertThat(listener.wasInvoked()).isFalse();
  }

  @Test
  public void auxObjectiveSetLinearTerm_wasDeleted_throws() {
    var objective = new AuxiliaryObjective(/* priority= */ 0L, "", /* id= */ 2L, listener, modelId);
    Variable x = variables.addVariable("x");
    objective.markDeleted();

    var error = assertThrows(IllegalArgumentException.class, () -> objective.setLinearTerm(x, 1.0));

    assertThat(error).hasMessageThat().contains("deleted");
    assertThat(listener.wasInvoked()).isFalse();
  }

  @Test
  public void auxObjectiveSetPriority_wasDeleted_throws() {
    var objective = new AuxiliaryObjective(/* priority= */ 0L, "", /* id= */ 2L, listener, modelId);
    objective.markDeleted();

    var error = assertThrows(IllegalArgumentException.class, () -> objective.setPriority(5L));

    assertThat(error).hasMessageThat().contains("deleted");
    assertThat(listener.wasInvoked()).isFalse();
  }

  @Test
  public void auxObjectiveClear_wasDeleted_throws() {
    var objective = new AuxiliaryObjective(/* priority= */ 0L, "", /* id= */ 2L, listener, modelId);
    objective.markDeleted();

    var error = assertThrows(IllegalArgumentException.class, objective::clear);

    assertThat(error).hasMessageThat().contains("deleted");
    assertThat(listener.wasInvoked()).isFalse();
  }

  @Test
  public void auxObjectiveToCompleteString_deletedObjective_markedDeleted() {
    var objective = new AuxiliaryObjective(/* priority= */ 0L, "", /* id= */ 3L, listener, modelId);
    Variable x = variables.addVariable("x");
    objective.setLinearTerm(x, 1.0);
    objective.setOffset(-4.0);
    objective.markDeleted();
    assertThat(objective.toCompleteString())
        .isEqualTo("Objective 3: minimize x - 4.0 priority: 0 (deleted)");
  }

  @Test
  public void auxObjectiveAddAll_wasDeleted_throws() {
    var objective = new AuxiliaryObjective(/* priority= */ 0L, "", /* id= */ 2L, listener, modelId);
    objective.markDeleted();

    var error =
        assertThrows(IllegalArgumentException.class, () -> objective.addAll(ImmutableList.of()));

    assertThat(error).hasMessageThat().contains("deleted");
  }

  @Test
  public void auxObjectiveAddInnerProduct_wasDeleted_throws() {
    var objective = new AuxiliaryObjective(/* priority= */ 0L, "", /* id= */ 2L, listener, modelId);
    objective.markDeleted();

    var error = assertThrows(IllegalArgumentException.class,
        () -> objective.addInnerProduct(ImmutableList.of(), ImmutableList.of()));

    assertThat(error).hasMessageThat().contains("deleted");
  }
}
