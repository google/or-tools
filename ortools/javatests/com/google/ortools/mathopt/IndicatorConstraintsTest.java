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

import com.google.common.collect.ImmutableMap;
import com.google.ortools.mathopt.IndicatorConstraintUpdatesProto;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

public final class IndicatorConstraintsTest {
  private ModelId modelId;
  private Variables variables;
  private IndicatorConstraints indicatorConstraints;

  @BeforeEach
  public void setUp() {
    modelId = new ModelId("model");
    variables = new Variables(modelId);
    indicatorConstraints = new IndicatorConstraints(modelId);
  }

  @Test
  public void addIndicatorConstraint_addsConstraint() {
    Variable z = variables.addVariable("z");
    Variable x = variables.addVariable("x");

    IndicatorConstraint c =
        indicatorConstraints.addIndicatorConstraint(z, true, 1.0, 2.0, linExpr(x).add(3.0), "c");

    assertThat(c.getId()).isEqualTo(0);
    assertThat(c.getName()).isEqualTo("c");
    assertThat(c.getIndicatorVariable()).isEqualTo(z);
    assertThat(c.getActivateOnZero()).isTrue();
    // Offset 3.0 subtracted from bounds
    assertThat(c.getLowerBound()).isEqualTo(-2.0);
    assertThat(c.getUpperBound()).isEqualTo(-1.0);
    assertThat(c.getUnmodifiableImpliedTerms()).containsExactly(x, 1.0);

    assertThat(indicatorConstraints.numIndicatorConstraints()).isEqualTo(1);
    assertThat(indicatorConstraints.hasIndicatorConstraintWithId(0)).isTrue();
    assertThat(indicatorConstraints.getIndicatorConstraint(0)).isEqualTo(c);
  }

  @Test
  public void deleteIndicatorConstraint_deletes() {
    Variable z = variables.addVariable("z");
    IndicatorConstraint c =
        indicatorConstraints.addIndicatorConstraint(z, false, 0.0, 0.0, linExpr(), "c");

    assertThat(indicatorConstraints.deleteIndicatorConstraint(c)).isTrue();

    assertThat(c.isDeleted()).isTrue();
    assertThat(indicatorConstraints.numIndicatorConstraints()).isEqualTo(0);
    assertThat(indicatorConstraints.hasIndicatorConstraintWithId(0)).isFalse();
  }

  @Test
  public void diff_tracksCreation() {
    Variable z = variables.addVariable("z");
    IndicatorConstraints.Diff diff = indicatorConstraints.addDiff();

    IndicatorConstraint c =
        indicatorConstraints.addIndicatorConstraint(z, false, 0.0, 0.0, linExpr(), "c");

    IndicatorConstraintUpdatesProto indicatorConstraintUpdatesProto = diff.update();

    assertThat(indicatorConstraintUpdatesProto.getNewConstraintsMap())
        .containsExactly(0L, c.toProto());
  }

  @Test
  public void diff_tracksDeletion() {
    Variable z = variables.addVariable("z");
    IndicatorConstraint c =
        indicatorConstraints.addIndicatorConstraint(z, false, 0.0, 0.0, linExpr(), "c");
    IndicatorConstraints.Diff diff = indicatorConstraints.addDiff();

    indicatorConstraints.deleteIndicatorConstraint(c);

    IndicatorConstraintUpdatesProto indicatorConstraintUpdatesProto = diff.update();

    assertThat(indicatorConstraintUpdatesProto.getDeletedConstraintIdsList()).containsExactly(0L);
  }

  @Test
  public void diff_advance_clearsUpdates() {
    Variable z = variables.addVariable("z");
    IndicatorConstraints.Diff diff = indicatorConstraints.addDiff();
    var unused = indicatorConstraints.addIndicatorConstraint(z, false, 0.0, 0.0, linExpr(), "c");

    diff.advance();

    IndicatorConstraintUpdatesProto indicatorConstraintUpdatesProto = diff.update();

    assertThat(indicatorConstraintUpdatesProto.getNewConstraintsMap()).isEmpty();
  }

  @Test
  public void diff_advance_clearsDeleted() {
    Variable z = variables.addVariable("z");
    IndicatorConstraint c1 =
        indicatorConstraints.addIndicatorConstraint(z, false, 0.0, 0.0, linExpr(), "c1");
    IndicatorConstraint c2 =
        indicatorConstraints.addIndicatorConstraint(z, false, 0.0, 0.0, linExpr(), "c2");
    IndicatorConstraints.Diff diff = indicatorConstraints.addDiff();

    indicatorConstraints.deleteIndicatorConstraint(c1);
    diff.advance();
    indicatorConstraints.deleteIndicatorConstraint(c2);

    IndicatorConstraintUpdatesProto indicatorConstraintUpdatesProto = diff.update();
    assertThat(indicatorConstraintUpdatesProto.getDeletedConstraintIdsList())
        .containsExactly(c2.getId());
  }

  @Test
  public void diff_deleteNewConstraint_notTrackedAsDeleted() {
    Variable z = variables.addVariable("z");
    IndicatorConstraints.Diff diff = indicatorConstraints.addDiff();

    // Add a constraint AFTER creating the diff
    IndicatorConstraint c =
        indicatorConstraints.addIndicatorConstraint(z, false, 0.0, 0.0, linExpr(), "c");
    // Delete it
    indicatorConstraints.deleteIndicatorConstraint(c);

    IndicatorConstraintUpdatesProto indicatorConstraintUpdatesProto = diff.update();

    // Should not be in deletedConstraintIds because it was created after checkpoint
    assertThat(indicatorConstraintUpdatesProto.getDeletedConstraintIdsList()).isEmpty();
    // Should not be in newConstraints because it is deleted
    assertThat(indicatorConstraintUpdatesProto.getNewConstraintsMap()).isEmpty();
  }

  @Test
  public void variableDeletion_updatesIndicatorConstraint() {
    Variable z = variables.addVariable("z");
    IndicatorConstraint c =
        indicatorConstraints.addIndicatorConstraint(z, true, 0.0, 0.0, linExpr(), "c");

    indicatorConstraints.onVariableDeleted(z);

    assertThat(c.getIndicatorVariable()).isNull();
  }

  @Test
  public void deleteIndicatorConstraint_stopsTrackingIndicatorVariable() {
    Variable z = variables.addVariable("z");
    IndicatorConstraint c =
        indicatorConstraints.addIndicatorConstraint(z, true, 0.0, 0.0, linExpr(), "c");

    indicatorConstraints.deleteIndicatorConstraint(c);
    // Simulate model behavior:
    indicatorConstraints.onVariableDeleted(z);

    // If c was correctly removed from internal maps of indicatorConstraints, onVariableDeleted(z)
    // should not have found c, and thus should not have called c.onVariableDeleted(z).
    assertThat(c.getIndicatorVariable()).isEqualTo(z);
  }

  @Test
  public void deleteIndicatorConstraint_stopsTrackingImpliedTermVariable() {
    Variable z = variables.addVariable("z");
    Variable x = variables.addVariable("x");
    IndicatorConstraint c =
        indicatorConstraints.addIndicatorConstraint(z, true, 0.0, 0.0, linExpr(x), "c");

    indicatorConstraints.deleteIndicatorConstraint(c);
    // Simulate model behavior:
    indicatorConstraints.onVariableDeleted(x);

    // If c was correctly removed from internal maps of indicatorConstraints, onVariableDeleted(x)
    // should not have found c, and thus should not have called c.onVariableDeleted(x).
    // c.onVariableDeleted(x) would remove x from terms.
    assertThat(c.getUnmodifiableImpliedTerms()).containsKey(x);
  }

  @Test
  public void deleteIndicatorConstraintWithDeletedIndicator_stopsTrackingImpliedTermVariable() {
    Variable z = variables.addVariable("z");
    Variable x = variables.addVariable("x");
    IndicatorConstraint c =
        indicatorConstraints.addIndicatorConstraint(z, true, 0.0, 0.0, linExpr(x), "c");
    // Delete the indicator variable.
    variables.deleteVariable(z);
    indicatorConstraints.onVariableDeleted(z);

    // Delete the indicator constraint.
    indicatorConstraints.deleteIndicatorConstraint(c);
    indicatorConstraints.onVariableDeleted(x);

    // If c was correctly removed from internal maps of indicatorConstraints, onVariableDeleted(x)
    // should not have found c, and thus should not have called c.onVariableDeleted(x).
    // c.onVariableDeleted(x) would remove x from terms.
    assertThat(c.getUnmodifiableImpliedTerms()).containsKey(x);
  }

  @Test
  public void removeDiff_removesDiff() {
    IndicatorConstraints.Diff diff = indicatorConstraints.addDiff();
    indicatorConstraints.removeDiff(diff);
    // If we can't access diffs list, we verify behavior (e.g., adding constraint doesn't explode,
    // but diff is already invalid?)
    // Actually, Model.java handles removeTracker logic.
    // We can verify that calling removeDiff with a removed diff throws.
    assertThrows(IllegalArgumentException.class, () -> indicatorConstraints.removeDiff(diff));
  }

  @Test
  public void getNextId_returnsNextId() {
    assertThat(indicatorConstraints.getNextId()).isEqualTo(0);
    Variable z = variables.addVariable("z");
    var unused = indicatorConstraints.addIndicatorConstraint(z, false, 0.0, 0.0, linExpr(), "c");
    assertThat(indicatorConstraints.getNextId()).isEqualTo(1);
  }

  @Test
  public void ensureNextIdAtLeast_updatesId() {
    indicatorConstraints.ensureNextIdAtLeast(10);
    assertThat(indicatorConstraints.getNextId()).isEqualTo(10);
  }

  @Test
  public void deleteIndicatorConstraint_removesIndicatorVariableFromLookupMap() {
    Variable z = variables.addVariable("z");
    IndicatorConstraint c =
        indicatorConstraints.addIndicatorConstraint(z, true, 0.0, 0.0, linExpr(), "c");
    IndicatorConstraint constraintForLookup =
        new IndicatorConstraint(10, "c", modelId, z, true, 3.0, 4.0, ImmutableMap.of());

    indicatorConstraints.deleteIndicatorConstraint(c);
    // Simulate model behavior:
    indicatorConstraints.onVariableDeleted(z);

    NullPointerException e = assertThrows(NullPointerException.class,
        () -> indicatorConstraints.deleteIndicatorConstraint(constraintForLookup));
    assertThat(e).hasMessageThat().contains("Variable z not present in variableToConstraint");
  }
}
