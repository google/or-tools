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

import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableSet;
import com.google.ortools.mathopt.LinearConstraints.Diff;
import com.google.ortools.mathopt.LinearConstraints.UpdateResult;
import com.google.ortools.mathopt.ModelUpdateProto;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

public final class LinearConstraintsUpdateTest {
  private ModelId modelId;
  private Variables variables;
  private LinearConstraints constraints;

  @BeforeEach
  public void setUp() {
    modelId = new ModelId("test_model");
    variables = new Variables(modelId);
    constraints = new LinearConstraints(modelId);
  }

  private static ModelUpdateProto updateToProto(UpdateResult update) {
    var updateProto = ModelUpdateProto.newBuilder();
    update.setInProto(updateProto);
    return updateProto.build();
  }

  @Test
  public void diffUpdate_addConstraintAfterCheckpoint_hasNewConstraint() {
    Diff diff = constraints.addDiff(variables.getNextId());
    var unused = constraints.addLinearConstraint(-1.0, 2.0, "c");

    UpdateResult update = diff.update(ImmutableSet.of(), ImmutableList.of());

    ModelUpdateProto.Builder expectedUpdates = ModelUpdateProto.newBuilder();
    expectedUpdates.getNewLinearConstraintsBuilder()
        .addIds(0)
        .addLowerBounds(-1.0)
        .addUpperBounds(2.0)
        .addNames("c");
    assertThat(updateToProto(update)).ignoringFieldAbsence().isEqualTo(expectedUpdates.build());
  }

  @Test
  public void diffUpdate_addConstraintBeforeCheckpoint_updateIsEmpty() {
    var unused = constraints.addLinearConstraint(-1.0, 2.0, "c");
    Diff diff = constraints.addDiff(variables.getNextId());

    UpdateResult update = diff.update(ImmutableSet.of(), ImmutableList.of());

    assertThat(updateToProto(update))
        .ignoringFieldAbsence()
        .isEqualTo(ModelUpdateProto.getDefaultInstance());
  }

  @Test
  public void diffUpdate_modifyLbOldConstraint_hasLbUpdate() {
    System.out.println("diffUpdate_modifyLbOldConstraint_hasLbUpdate");
    var c = constraints.addLinearConstraint(-1.0, 2.0, "c");
    Diff diff = constraints.addDiff(variables.getNextId());
    c.setLowerBound(-3.0);

    UpdateResult update = diff.update(ImmutableSet.of(), ImmutableList.of());

    ModelUpdateProto.Builder expectedUpdates = ModelUpdateProto.newBuilder();
    expectedUpdates.getLinearConstraintUpdatesBuilder().getLowerBoundsBuilder().addIds(0).addValues(
        -3.0);
    assertThat(updateToProto(update)).ignoringFieldAbsence().isEqualTo(expectedUpdates.build());
  }

  @Test
  public void diffUpdate_modifyUbOldConstraint_hasUbUpdate() {
    System.out.println("diffUpdate_modifyUbOldConstraint_hasLbUpdate");
    var c = constraints.addLinearConstraint(-1.0, 2.0, "c");
    Diff diff = constraints.addDiff(variables.getNextId());
    c.setUpperBound(3.0);

    UpdateResult update = diff.update(ImmutableSet.of(), ImmutableList.of());

    ModelUpdateProto.Builder expectedUpdates = ModelUpdateProto.newBuilder();
    expectedUpdates.getLinearConstraintUpdatesBuilder().getUpperBoundsBuilder().addIds(0).addValues(
        3.0);
    assertThat(updateToProto(update)).ignoringFieldAbsence().isEqualTo(expectedUpdates.build());
  }

  @Test
  public void diffUpdate_modifyLbNewConstraint_newConstraintRightBound() {
    Diff diff = constraints.addDiff(variables.getNextId());
    var c = constraints.addLinearConstraint(-1.0, 2.0, "c");
    c.setLowerBound(-3.0);

    UpdateResult update = diff.update(ImmutableSet.of(), ImmutableList.of());

    ModelUpdateProto.Builder expectedUpdates = ModelUpdateProto.newBuilder();
    expectedUpdates.getNewLinearConstraintsBuilder()
        .addIds(0)
        .addLowerBounds(-3.0)
        .addUpperBounds(2.0)
        .addNames("c");
    assertThat(updateToProto(update)).ignoringFieldAbsence().isEqualTo(expectedUpdates.build());
  }

  @Test
  public void diffUpdate_modifyUbNewConstraint_newConstraintRightBound() {
    Diff diff = constraints.addDiff(variables.getNextId());
    var c = constraints.addLinearConstraint(-1.0, 2.0, "c");
    c.setUpperBound(3.0);

    UpdateResult update = diff.update(ImmutableSet.of(), ImmutableList.of());

    ModelUpdateProto.Builder expectedUpdates = ModelUpdateProto.newBuilder();
    expectedUpdates.getNewLinearConstraintsBuilder()
        .addIds(0)
        .addLowerBounds(-1.0)
        .addUpperBounds(3.0)
        .addNames("c");
    assertThat(updateToProto(update)).ignoringFieldAbsence().isEqualTo(expectedUpdates.build());
  }

  @Test
  public void diffUpdate_updateCoefficientOldVarOldConstraint_hasMatrixUpdate() {
    Variable x = variables.addVariable("x");
    LinearConstraint c = constraints.addLinearConstraint("c");
    Diff diff = constraints.addDiff(variables.getNextId());

    c.setTerm(x, 2.0);
    UpdateResult update = diff.update(ImmutableSet.of(), ImmutableList.of());

    ModelUpdateProto.Builder expectedUpdates = ModelUpdateProto.newBuilder();
    expectedUpdates.getLinearConstraintMatrixUpdatesBuilder()
        .addRowIds(0L)
        .addColumnIds(0L)
        .addCoefficients(2.0);
    assertThat(updateToProto(update)).ignoringFieldAbsence().isEqualTo(expectedUpdates.build());
  }

  @Test
  public void diffUpdate_updateCoefficientOldVarNewConstraint_hasMatrixUpdate() {
    Variable x = variables.addVariable("x");
    Diff diff = constraints.addDiff(variables.getNextId());
    LinearConstraint c = constraints.addLinearConstraint("c");

    c.setTerm(x, 2.0);
    UpdateResult update = diff.update(ImmutableSet.of(), ImmutableList.of());

    ModelUpdateProto.Builder expectedUpdates = ModelUpdateProto.newBuilder();
    expectedUpdates.getNewLinearConstraintsBuilder()
        .addIds(0L)
        .addNames("c")
        .addLowerBounds(Double.NEGATIVE_INFINITY)
        .addUpperBounds(Double.POSITIVE_INFINITY);
    expectedUpdates.getLinearConstraintMatrixUpdatesBuilder()
        .addRowIds(0L)
        .addColumnIds(0L)
        .addCoefficients(2.0);
    assertThat(updateToProto(update)).ignoringFieldAbsence().isEqualTo(expectedUpdates.build());
  }

  @Test
  public void diffUpdate_updateCoefficientNewVarOldConstraint_hasMatrixUpdate() {
    LinearConstraint c = constraints.addLinearConstraint("c");
    Diff diff = constraints.addDiff(variables.getNextId());
    Variable x = variables.addVariable("x");

    c.setTerm(x, 2.0);
    UpdateResult update = diff.update(ImmutableSet.of(), ImmutableList.of(x));

    ModelUpdateProto.Builder expectedUpdates = ModelUpdateProto.newBuilder();
    expectedUpdates.getLinearConstraintMatrixUpdatesBuilder()
        .addRowIds(0L)
        .addColumnIds(0L)
        .addCoefficients(2.0);
    assertThat(updateToProto(update)).ignoringFieldAbsence().isEqualTo(expectedUpdates.build());
  }

  @Test
  public void diffUpdate_updateCoefficientNewVarNewConstraint_hasMatrixUpdate() {
    Diff diff = constraints.addDiff(variables.getNextId());
    Variable x = variables.addVariable("x");
    LinearConstraint c = constraints.addLinearConstraint("c");

    c.setTerm(x, 2.0);
    UpdateResult update = diff.update(ImmutableSet.of(), ImmutableList.of(x));

    ModelUpdateProto.Builder expectedUpdates = ModelUpdateProto.newBuilder();
    expectedUpdates.getNewLinearConstraintsBuilder()
        .addIds(0L)
        .addNames("c")
        .addLowerBounds(Double.NEGATIVE_INFINITY)
        .addUpperBounds(Double.POSITIVE_INFINITY);
    expectedUpdates.getLinearConstraintMatrixUpdatesBuilder()
        .addRowIds(0L)
        .addColumnIds(0L)
        .addCoefficients(2.0);
    assertThat(updateToProto(update)).ignoringFieldAbsence().isEqualTo(expectedUpdates.build());
  }

  @Test
  public void
  diffUpdate_updateCoefficientOldVarOldConstraintWasNonzeroAndChanged_hasMatrixUpdate() {
    Variable x = variables.addVariable("x");
    LinearConstraint c = constraints.addLinearConstraint("c");
    c.setTerm(x, 3.0);
    Diff diff = constraints.addDiff(variables.getNextId());

    c.setTerm(x, 4.0);
    UpdateResult update = diff.update(ImmutableSet.of(), ImmutableList.of());

    ModelUpdateProto.Builder expectedUpdates = ModelUpdateProto.newBuilder();
    expectedUpdates.getLinearConstraintMatrixUpdatesBuilder()
        .addRowIds(0L)
        .addColumnIds(0L)
        .addCoefficients(4.0);
    assertThat(updateToProto(update)).ignoringFieldAbsence().isEqualTo(expectedUpdates.build());
  }

  @Test
  public void diffUpdate_updateCoefficientOldVarOldConstraintWasNonzeroAndUnchanged_emptyUpdate() {
    Variable x = variables.addVariable("x");
    LinearConstraint c = constraints.addLinearConstraint("c");
    c.setTerm(x, 3.0);
    Diff diff = constraints.addDiff(variables.getNextId());

    c.setTerm(x, 3.0);
    UpdateResult update = diff.update(ImmutableSet.of(), ImmutableList.of());

    assertThat(updateToProto(update))
        .ignoringFieldAbsence()
        .isEqualTo(ModelUpdateProto.getDefaultInstance());
  }

  @Test
  public void diffUpdate_updateCoefficientOldVarOldConstraintWasNonzeroAndSetToZero_hasUpdate() {
    Variable x = variables.addVariable("x");
    LinearConstraint c = constraints.addLinearConstraint("c");
    c.setTerm(x, 3.0);
    Diff diff = constraints.addDiff(variables.getNextId());

    c.setTerm(x, 0.0);
    UpdateResult update = diff.update(ImmutableSet.of(), ImmutableList.of());

    ModelUpdateProto.Builder expectedUpdates = ModelUpdateProto.newBuilder();
    expectedUpdates.getLinearConstraintMatrixUpdatesBuilder()
        .addRowIds(0L)
        .addColumnIds(0L)
        .addCoefficients(0.0);
    assertThat(updateToProto(update)).ignoringFieldAbsence().isEqualTo(expectedUpdates.build());
  }

  @Test
  public void diffUpdate_updateCoefficientNewVarOldConstraintNonzeroThenZero_emptyUpdate() {
    LinearConstraint c = constraints.addLinearConstraint("c");
    Diff diff = constraints.addDiff(variables.getNextId());
    Variable x = variables.addVariable("x");
    c.setTerm(x, 4.0);

    c.setTerm(x, 0.0);
    UpdateResult update = diff.update(ImmutableSet.of(), ImmutableList.of(x));

    assertThat(updateToProto(update))
        .ignoringFieldAbsence()
        .isEqualTo(ModelUpdateProto.getDefaultInstance());
  }

  @Test
  public void diffUpdate_updateCoefficientOldVarNewConstraintNonzeroThenZero_emptyMatrix() {
    Variable x = variables.addVariable("x");
    Diff diff = constraints.addDiff(variables.getNextId());
    LinearConstraint c = constraints.addLinearConstraint("c");
    c.setTerm(x, 4.0);

    c.setTerm(x, 0.0);
    UpdateResult update = diff.update(ImmutableSet.of(), ImmutableList.of());

    ModelUpdateProto.Builder expectedUpdates = ModelUpdateProto.newBuilder();
    expectedUpdates.getNewLinearConstraintsBuilder()
        .addIds(0L)
        .addNames("c")
        .addLowerBounds(Double.NEGATIVE_INFINITY)
        .addUpperBounds(Double.POSITIVE_INFINITY);
    assertThat(updateToProto(update)).ignoringFieldAbsence().isEqualTo(expectedUpdates.build());
  }

  @Test
  public void diffUpdate_updateCoefficientNewVarNewConstraintNonzeroThenZero_emptyMatrix() {
    Diff diff = constraints.addDiff(variables.getNextId());
    Variable x = variables.addVariable("x");
    LinearConstraint c = constraints.addLinearConstraint("c");
    c.setTerm(x, 4.0);

    c.setTerm(x, 0.0);
    UpdateResult update = diff.update(ImmutableSet.of(), ImmutableList.of(x));

    ModelUpdateProto.Builder expectedUpdates = ModelUpdateProto.newBuilder();
    expectedUpdates.getNewLinearConstraintsBuilder()
        .addIds(0L)
        .addNames("c")
        .addLowerBounds(Double.NEGATIVE_INFINITY)
        .addUpperBounds(Double.POSITIVE_INFINITY);
    assertThat(updateToProto(update)).ignoringFieldAbsence().isEqualTo(expectedUpdates.build());
  }

  @Test
  public void diffUpdate_deleteOldLinearConstraint_hasDelete() {
    LinearConstraint c = constraints.addLinearConstraint("c");
    Diff diff = constraints.addDiff(variables.getNextId());

    constraints.deleteLinearConstraint(c);
    UpdateResult update = diff.update(ImmutableSet.of(), ImmutableList.of());

    assertThat(updateToProto(update))
        .ignoringFieldAbsence()
        .isEqualTo(ModelUpdateProto.newBuilder().addDeletedLinearConstraintIds(0).build());
  }

  @Test
  public void diffUpdate_deleteNewLinearConstraint_notInUpdate() {
    Diff diff = constraints.addDiff(variables.getNextId());
    LinearConstraint c = constraints.addLinearConstraint("c");

    constraints.deleteLinearConstraint(c);
    UpdateResult update = diff.update(ImmutableSet.of(), ImmutableList.of());

    assertThat(updateToProto(update))
        .ignoringFieldAbsence()
        .isEqualTo(ModelUpdateProto.getDefaultInstance());
  }

  @Test
  public void diffUpdate_deleteOldLinearConstraintWithOldNonzeroChanges_changesDeleted() {
    LinearConstraint c = constraints.addLinearConstraint("c");
    Variable x = variables.addVariable("x");
    c.setTerm(x, 2.0);
    Diff diff = constraints.addDiff(variables.getNextId());

    c.setTerm(x, 3.0);
    constraints.deleteLinearConstraint(c);
    UpdateResult update = diff.update(ImmutableSet.of(), ImmutableList.of());

    assertThat(updateToProto(update))
        .ignoringFieldAbsence()
        .isEqualTo(ModelUpdateProto.newBuilder().addDeletedLinearConstraintIds(0).build());
  }

  @Test
  public void diffUpdate_deleteOldLinearConstraintWithOldVariableSetToZero_changesDeleted() {
    LinearConstraint c = constraints.addLinearConstraint("c");
    Variable x = variables.addVariable("x");
    c.setTerm(x, 2.0);
    Diff diff = constraints.addDiff(variables.getNextId());

    c.setTerm(x, 0.0);
    constraints.deleteLinearConstraint(c);
    UpdateResult update = diff.update(ImmutableSet.of(), ImmutableList.of());

    assertThat(updateToProto(update))
        .ignoringFieldAbsence()
        .isEqualTo(ModelUpdateProto.newBuilder().addDeletedLinearConstraintIds(0).build());
  }

  @Test
  public void diffUpdate_deleteOldVariableModifiedToNonzeroInOldLinearConstraint_changesDeleted() {
    LinearConstraint c = constraints.addLinearConstraint("c");
    Variable x = variables.addVariable("x");
    c.setTerm(x, 2.0);
    Diff diff = constraints.addDiff(variables.getNextId());

    c.setTerm(x, 3.0);
    constraints.onVariableDeleted(x);
    UpdateResult update = diff.update(ImmutableSet.of(0L), ImmutableList.of());

    assertThat(updateToProto(update))
        .ignoringFieldAbsence()
        .isEqualTo(ModelUpdateProto.getDefaultInstance());
  }

  @Test
  public void
  diffUpdate_deleteOldVariableModifiedToNonzeroInOldLinearConstraintWithTwoVariables_changesDeleted() {
    LinearConstraint c = constraints.addLinearConstraint("c");
    Variable x = variables.addVariable("x");
    Variable y = variables.addVariable("y");
    c.setTerm(x, 2.0);
    c.setTerm(y, 3.0);
    Diff diff = constraints.addDiff(variables.getNextId());

    c.setTerm(x, 4.0);
    c.setTerm(y, 5.0);
    constraints.onVariableDeleted(x);
    UpdateResult update = diff.update(ImmutableSet.of(0L), ImmutableList.of());

    var expected = ModelUpdateProto.newBuilder();
    expected.getLinearConstraintMatrixUpdatesBuilder()
        .addRowIds(0L)
        .addColumnIds(1L)
        .addCoefficients(5.0);
    assertThat(updateToProto(update)).ignoringFieldAbsence().isEqualTo(expected.build());
  }

  @Test
  public void diffUpdate_deleteOldVariableModifiedToZeroInOldLinearConstraint_changesDeleted() {
    LinearConstraint c = constraints.addLinearConstraint("c");
    Variable x = variables.addVariable("x");
    c.setTerm(x, 2.0);
    Diff diff = constraints.addDiff(variables.getNextId());

    c.setTerm(x, 0.0);
    constraints.onVariableDeleted(x);
    UpdateResult update = diff.update(ImmutableSet.of(0L), ImmutableList.of());

    assertThat(updateToProto(update))
        .ignoringFieldAbsence()
        .isEqualTo(ModelUpdateProto.getDefaultInstance());
  }

  @Test
  public void diffUpdate_deleteNewVariableInOldLinearConstraint_changesDeleted() {
    LinearConstraint c = constraints.addLinearConstraint("c");
    Diff diff = constraints.addDiff(variables.getNextId());
    Variable x = variables.addVariable("x");
    c.setTerm(x, 2.0);

    constraints.onVariableDeleted(x);
    // Note that new and deleted variables are intentionally both empty.
    UpdateResult update = diff.update(ImmutableSet.of(), ImmutableList.of());

    assertThat(updateToProto(update))
        .ignoringFieldAbsence()
        .isEqualTo(ModelUpdateProto.getDefaultInstance());
  }

  @Test
  public void diffUpdate_deleteOldVariableInNewLinearConstraint_changesDeleted() {
    Variable x = variables.addVariable("x");
    Diff diff = constraints.addDiff(variables.getNextId());
    LinearConstraint c = constraints.addLinearConstraint(0.0, 1.0, "c");
    c.setTerm(x, 2.0);

    constraints.onVariableDeleted(x);

    UpdateResult update = diff.update(ImmutableSet.of(x.getId()), ImmutableList.of());

    var expectedUpdate = ModelUpdateProto.newBuilder();
    expectedUpdate.getNewLinearConstraintsBuilder()
        .addNames("c")
        .addLowerBounds(0.0)
        .addUpperBounds(1.0)
        .addIds(0L);
    assertThat(updateToProto(update)).ignoringFieldAbsence().isEqualTo(expectedUpdate.build());
  }

  @Test
  public void removeDiff_removeTwice_throws() {
    Diff diff = constraints.addDiff(variables.getNextId());

    constraints.removeDiff(diff);
    var secondRemoveException =
        assertThrows(IllegalArgumentException.class, () -> constraints.removeDiff(diff));

    assertThat(secondRemoveException).hasMessageThat().contains("Diff not present");
  }

  @Test
  public void removeDiff_setLb_noUpdate() {
    LinearConstraint c = constraints.addLinearConstraint("c");
    Diff diff = constraints.addDiff(variables.getNextId());

    constraints.removeDiff(diff);
    c.setLowerBound(-1.0);
    UpdateResult update = diff.update(ImmutableSet.of(), ImmutableList.of());

    assertThat(updateToProto(update))
        .ignoringFieldAbsence()
        .isEqualTo(ModelUpdateProto.getDefaultInstance());
  }

  @Test
  public void advance_allUpdatesPresent_allUpdatesCleared() {
    LinearConstraint c = constraints.addLinearConstraint("c");
    Variable x = variables.addVariable("x");
    c.setTerm(x, 2.0);
    Diff diff = constraints.addDiff(variables.getNextId());
    c.setLowerBound(-1.0);
    c.setUpperBound(2.0);
    c.setTerm(x, 3.0);
    LinearConstraint d = constraints.addLinearConstraint(-4.0, 5.0, "d");
    d.setTerm(x, 6.0);

    diff.advance(variables.getNextId());
    UpdateResult update = diff.update(ImmutableSet.of(), ImmutableList.of());

    assertThat(updateToProto(update))
        .ignoringFieldAbsence()
        .isEqualTo(ModelUpdateProto.getDefaultInstance());
  }

  @Test
  public void advance_modifyAfterAdvance_isUpdateNotNew() {
    Diff diff = constraints.addDiff(variables.getNextId());
    LinearConstraint c = constraints.addLinearConstraint("c");
    Variable x = variables.addVariable("x");
    c.setTerm(x, 2.0);
    diff.advance(variables.getNextId());

    c.setLowerBound(-1.0);
    c.setUpperBound(2.0);
    c.setTerm(x, 0.0);

    UpdateResult update = diff.update(ImmutableSet.of(), ImmutableList.of());

    var expectedUpdate = ModelUpdateProto.newBuilder();
    expectedUpdate.getLinearConstraintMatrixUpdatesBuilder()
        .addRowIds(0L)
        .addColumnIds(0L)
        .addCoefficients(0.0);
    expectedUpdate.getLinearConstraintUpdatesBuilder().getLowerBoundsBuilder().addIds(0L).addValues(
        -1.0);
    expectedUpdate.getLinearConstraintUpdatesBuilder().getUpperBoundsBuilder().addIds(0L).addValues(
        2.0);
    assertThat(updateToProto(update)).ignoringFieldAbsence().isEqualTo(expectedUpdate.build());
  }

  @Test
  public void advance_deletedConstraint_isClearedAfterAdvance() {
    LinearConstraint c = constraints.addLinearConstraint("c");
    Diff diff = constraints.addDiff(variables.getNextId());
    constraints.deleteLinearConstraint(c);

    diff.advance(variables.getNextId());
    UpdateResult update = diff.update(ImmutableSet.of(), ImmutableList.of());

    assertThat(updateToProto(update))
        .ignoringFieldAbsence()
        .isEqualTo(ModelUpdateProto.getDefaultInstance());
  }

  @Test
  public void diffUpdate_twoDiffsDifferentCheckpoints_diffIsDifferent() {
    Diff diff1 = constraints.addDiff(variables.getNextId());
    var unused1 = constraints.addLinearConstraint(-1.0, 2.0, "c");
    Diff diff2 = constraints.addDiff(variables.getNextId());
    var unused2 = constraints.addLinearConstraint(-3.0, 4.0, "d");

    UpdateResult update1 = diff1.update(ImmutableSet.of(), ImmutableList.of());
    UpdateResult update2 = diff2.update(ImmutableSet.of(), ImmutableList.of());

    var updateProto1 = ModelUpdateProto.newBuilder();
    updateProto1.getNewLinearConstraintsBuilder()
        .addIds(0)
        .addIds(1)
        .addLowerBounds(-1.0)
        .addLowerBounds(-3.0)
        .addUpperBounds(2.0)
        .addUpperBounds(4.0)
        .addNames("c")
        .addNames("d");
    assertThat(updateToProto(update1)).ignoringFieldAbsence().isEqualTo(updateProto1.build());
    var updateProto2 = ModelUpdateProto.newBuilder();
    updateProto2.getNewLinearConstraintsBuilder()
        .addIds(1)
        .addLowerBounds(-3.0)
        .addUpperBounds(4.0)
        .addNames("d");
    assertThat(updateToProto(update2)).ignoringFieldAbsence().isEqualTo(updateProto2.build());
  }

  @Test
  public void diffUpdate_updateOldAndNewVariablesAndConstraints_constraintMatrixCorrectOrder() {
    Variable w = variables.addVariable("w");
    Variable x = variables.addVariable("x");
    LinearConstraint c = constraints.addLinearConstraint("c");
    LinearConstraint d = constraints.addLinearConstraint("d");
    c.setTerm(w, 3.0);
    c.setTerm(x, 4.0);
    d.setTerm(w, 5.0);
    d.setTerm(x, 6.0);
    Diff diff = constraints.addDiff(variables.getNextId());
    Variable y = variables.addVariable("y");
    Variable z = variables.addVariable("z");
    LinearConstraint e = constraints.addLinearConstraint(-1.0, 1.0, "e");
    LinearConstraint f = constraints.addLinearConstraint(-2.0, 2.0, "f");

    c.setTerm(w, 10.0);
    c.setTerm(x, 11.0);
    c.setTerm(y, 12.0);
    c.setTerm(z, 13.0);

    d.setTerm(w, 20.0);
    d.setTerm(x, 21.0);
    d.setTerm(y, 22.0);
    d.setTerm(z, 23.0);

    e.setTerm(w, 30.0);
    e.setTerm(x, 31.0);
    e.setTerm(y, 32.0);
    e.setTerm(z, 33.0);

    f.setTerm(w, 40.0);
    f.setTerm(x, 41.0);
    f.setTerm(y, 42.0);
    f.setTerm(z, 43.0);

    UpdateResult update = diff.update(ImmutableSet.of(), ImmutableList.of(y, z));

    ModelUpdateProto.Builder expectedUpdates = ModelUpdateProto.newBuilder();
    expectedUpdates.getNewLinearConstraintsBuilder()
        .addIds(2)
        .addIds(3)
        .addLowerBounds(-1.0)
        .addLowerBounds(-2.0)
        .addUpperBounds(1.0)
        .addUpperBounds(2.0)
        .addNames("e")
        .addNames("f");
    expectedUpdates
        .getLinearConstraintMatrixUpdatesBuilder()
        // constraint c
        .addRowIds(0L)
        .addColumnIds(0L)
        .addCoefficients(10.0)
        .addRowIds(0L)
        .addColumnIds(1L)
        .addCoefficients(11.0)
        .addRowIds(0L)
        .addColumnIds(2L)
        .addCoefficients(12.0)
        .addRowIds(0L)
        .addColumnIds(3L)
        .addCoefficients(13.0)
        // Constraint d
        .addRowIds(1L)
        .addColumnIds(0L)
        .addCoefficients(20.0)
        .addRowIds(1L)
        .addColumnIds(1L)
        .addCoefficients(21.0)
        .addRowIds(1L)
        .addColumnIds(2L)
        .addCoefficients(22.0)
        .addRowIds(1L)
        .addColumnIds(3L)
        .addCoefficients(23.0)
        // Constraint e
        .addRowIds(2L)
        .addColumnIds(0L)
        .addCoefficients(30.0)
        .addRowIds(2L)
        .addColumnIds(1L)
        .addCoefficients(31.0)
        .addRowIds(2L)
        .addColumnIds(2L)
        .addCoefficients(32.0)
        .addRowIds(2L)
        .addColumnIds(3L)
        .addCoefficients(33.0)
        // Constraint f
        .addRowIds(3L)
        .addColumnIds(0L)
        .addCoefficients(40.0)
        .addRowIds(3L)
        .addColumnIds(1L)
        .addCoefficients(41.0)
        .addRowIds(3L)
        .addColumnIds(2L)
        .addCoefficients(42.0)
        .addRowIds(3L)
        .addColumnIds(3L)
        .addCoefficients(43.0);
    assertThat(updateToProto(update)).ignoringFieldAbsence().isEqualTo(expectedUpdates.build());
  }
}
