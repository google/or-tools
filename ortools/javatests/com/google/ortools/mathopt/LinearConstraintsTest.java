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

import com.google.ortools.mathopt.LinearConstraintsProto;
import com.google.ortools.mathopt.SparseDoubleMatrixProto;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

public final class LinearConstraintsTest {
  private ModelId modelId;

  @BeforeEach
  public void setUp() {
    modelId = new ModelId("m");
  }

  @Test
  public void addUnboundedLinearConstraint_constraintIsEmptyAndUnbounded() {
    var constraints = new LinearConstraints(modelId);

    LinearConstraint c = constraints.addLinearConstraint("c");

    assertThat(c.getLowerBound()).isNegativeInfinity();
    assertThat(c.getUpperBound()).isPositiveInfinity();
    assertThat(c.getName()).isEqualTo("c");
    assertThat(c.getUnmodifiableTerms()).isEmpty();
    assertThat(c.isDeleted()).isFalse();
    assertThat(c.getModelId()).isSameInstanceAs(modelId);
  }

  @Test
  public void addBoundedLinearConstraint_constraintIsEmptyWithRightBounds() {
    var constraints = new LinearConstraints(modelId);

    LinearConstraint c =
        constraints.addLinearConstraint(/* lowerBound = */ -1.0, /* upperBound = */ 3.0, "c");

    assertThat(c.getLowerBound()).isEqualTo(-1.0);
    assertThat(c.getUpperBound()).isEqualTo(3.0);
    assertThat(c.getName()).isEqualTo("c");
    assertThat(c.getUnmodifiableTerms()).isEmpty();
    assertThat(c.isDeleted()).isFalse();
    assertThat(c.getModelId()).isSameInstanceAs(modelId);
  }

  @Test
  public void getters_emptyLinearConstraints_hasNoConstraints() {
    var constraints = new LinearConstraints(modelId);

    assertThat(constraints.getNextId()).isEqualTo(0L);
    assertThat(constraints.numLinearConstraints()).isEqualTo(0);
    assertThat(constraints.hasLinearConstraintWithId(0L)).isFalse();
    assertThat(constraints.getLinearConstraint(0L)).isNull();
    assertThat(constraints.sortedLinearConstraints()).isEmpty();
  }

  @Test
  public void getters_nonEmptyLinearConstraints_hasCorrectConstraints() {
    var constraints = new LinearConstraints(modelId);
    LinearConstraint c1 = constraints.addLinearConstraint("c1");
    LinearConstraint c2 = constraints.addLinearConstraint("c2");

    assertThat(constraints.getNextId()).isEqualTo(2L);
    assertThat(constraints.numLinearConstraints()).isEqualTo(2);
    assertThat(constraints.hasLinearConstraintWithId(0L)).isTrue();
    assertThat(constraints.hasLinearConstraintWithId(1L)).isTrue();
    assertThat(constraints.hasLinearConstraintWithId(2L)).isFalse();
    assertThat(constraints.getLinearConstraint(0L)).isSameInstanceAs(c1);
    assertThat(constraints.getLinearConstraint(1L)).isSameInstanceAs(c2);
    assertThat(constraints.getLinearConstraint(2L)).isNull();
    assertThat(constraints.sortedLinearConstraints()).containsExactly(c1, c2);
  }

  @Test
  public void sortedLinearConstraints_sparseConstraints_hasConstraints() {
    var constraints = new LinearConstraints(modelId);
    LinearConstraint c1 = constraints.addLinearConstraint("c1");
    LinearConstraint c2 = constraints.addLinearConstraint("c2");
    LinearConstraint c3 = constraints.addLinearConstraint("c1");
    LinearConstraint c4 = constraints.addLinearConstraint("c2");
    constraints.deleteLinearConstraint(c2);
    constraints.deleteLinearConstraint(c4);

    assertThat(constraints.sortedLinearConstraints()).containsExactly(c1, c3);
  }

  @Test
  public void checkOwned_owned_ok() {
    var constraints = new LinearConstraints(modelId);
    LinearConstraint c = constraints.addLinearConstraint("c");
    constraints.checkOwned(c); // Ensure doesn't throw an exception.
  }

  @Test
  public void checkOwned_unowned_throws() {
    var constraints1 = new LinearConstraints(modelId);
    LinearConstraint c1 = constraints1.addLinearConstraint("c1");
    var constraints2 = new LinearConstraints(new ModelId("m2"));
    LinearConstraint c2 = constraints2.addLinearConstraint("c2");

    var model1OwnsConstraint2Exception =
        assertThrows(IllegalArgumentException.class, () -> constraints1.checkOwned(c2));
    var model1OwnConstraint1Exception =
        assertThrows(IllegalArgumentException.class, () -> constraints2.checkOwned(c1));

    assertThat(model1OwnsConstraint2Exception).hasMessageThat().contains("another model");
    assertThat(model1OwnConstraint1Exception).hasMessageThat().contains("another model");
  }

  @Test
  public void ensureNextIdAtleast_skipIds_nextIdCorrect() {
    var constraints = new LinearConstraints(modelId);

    constraints.ensureNextIdAtLeast(3);

    assertThat(constraints.getNextId()).isEqualTo(3);
    assertThat(constraints.addLinearConstraint("c").getId()).isEqualTo(3);
  }

  @Test
  public void deleteLinearConstraint_constraintInModel_isDeleted() {
    var constraints = new LinearConstraints(modelId);
    LinearConstraint c = constraints.addLinearConstraint("c");

    assertThat(constraints.deleteLinearConstraint(c)).isTrue();

    assertThat(c.isDeleted()).isTrue();
    assertThat(constraints.numLinearConstraints()).isEqualTo(0);
    assertThat(constraints.getNextId()).isEqualTo(1L);
  }

  @Test
  public void deleteLinearConstraint_addAfterDelete_idIncreases() {
    var constraints = new LinearConstraints(modelId);
    LinearConstraint c = constraints.addLinearConstraint("c");

    constraints.deleteLinearConstraint(c);
    LinearConstraint d = constraints.addLinearConstraint("d");

    assertThat(d.getId()).isEqualTo(1L);
    assertThat(constraints.numLinearConstraints()).isEqualTo(1);
    assertThat(constraints.getNextId()).isEqualTo(2L);
  }

  @Test
  public void deleteLinearConstraint_deleteTwice_returnsFalseAndNoEffect() {
    var constraints = new LinearConstraints(modelId);
    LinearConstraint c = constraints.addLinearConstraint("c");
    constraints.deleteLinearConstraint(c);

    assertThat(constraints.deleteLinearConstraint(c)).isFalse();

    assertThat(c.isDeleted()).isTrue();
    assertThat(constraints.numLinearConstraints()).isEqualTo(0);
    assertThat(constraints.getNextId()).isEqualTo(1L);
  }

  @Test
  public void deleteLinearConstraint_unowned_throws() {
    var constraints1 = new LinearConstraints(modelId);
    var unused = constraints1.addLinearConstraint("c1");
    var constraints2 = new LinearConstraints(new ModelId("m2"));
    LinearConstraint c2 = constraints2.addLinearConstraint("c2");

    var deleteConstraintWrongModelException =
        assertThrows(IllegalArgumentException.class, () -> constraints1.deleteLinearConstraint(c2));

    assertThat(deleteConstraintWrongModelException).hasMessageThat().contains("another model");
  }

  @Test
  public void deleteLinearConstraint_withTermsEmptiesColumn_columnEmpty() {
    var variables = new Variables(modelId);
    Variable x = variables.addVariable("x");
    var constraints = new LinearConstraints(modelId);
    LinearConstraint c = constraints.addLinearConstraint("c").setTerm(x, 1.0);

    constraints.deleteLinearConstraint(c);

    assertThat(constraints.getUnmodifiableColumnNonzeros(x)).isEmpty();
  }

  @Test
  public void deleteLinearConstraint_withTermInOtherConstraint_removedFromColumn() {
    var variables = new Variables(modelId);
    Variable x = variables.addVariable("x");
    var constraints = new LinearConstraints(modelId);
    LinearConstraint c = constraints.addLinearConstraint("c").setTerm(x, 1.0);
    LinearConstraint d = constraints.addLinearConstraint("d").setTerm(x, 2.0);

    constraints.deleteLinearConstraint(c);

    assertThat(constraints.getUnmodifiableColumnNonzeros(x)).containsExactly(d);
  }

  @Test
  public void getUnmodifiableColumns_unusedVariable_emptyColumn() {
    var variables = new Variables(modelId);
    Variable x = variables.addVariable("x");
    var constraints = new LinearConstraints(modelId);

    assertThat(constraints.getUnmodifiableColumnNonzeros(x)).isEmpty();
  }

  @Test
  public void getUnmodifiableColumns_nonemptyColumn_hasConstraint() {
    var variables = new Variables(modelId);
    Variable x = variables.addVariable("x");
    var constraints = new LinearConstraints(modelId);
    LinearConstraint c = constraints.addLinearConstraint("c").setTerm(x, 1.0);
    var unused = constraints.addLinearConstraint("d");
    LinearConstraint e = constraints.addLinearConstraint("e").setTerm(x, 3.0);

    assertThat(constraints.getUnmodifiableColumnNonzeros(x)).containsExactly(c, e);
  }

  @Test
  public void getUnmodifiableColumns_zeroTerm_removesFromColumn() {
    var variables = new Variables(modelId);
    Variable x = variables.addVariable("x");
    var constraints = new LinearConstraints(modelId);
    LinearConstraint c = constraints.addLinearConstraint("c").setTerm(x, 1.0);
    LinearConstraint d = constraints.addLinearConstraint("d").setTerm(x, 3.0);

    c.setTerm(x, 0.0);
    assertThat(constraints.getUnmodifiableColumnNonzeros(x)).containsExactly(d);
  }

  @Test
  public void getUnmodifiableColumns_zeroTerm_createsEmptyColumn() {
    var variables = new Variables(modelId);
    Variable x = variables.addVariable("x");
    var constraints = new LinearConstraints(modelId);
    LinearConstraint c = constraints.addLinearConstraint("c").setTerm(x, 1.0);

    c.setTerm(x, 0.0);
    assertThat(constraints.getUnmodifiableColumnNonzeros(x)).isEmpty();
  }

  @Test
  public void getUnmodifiableColumns_wrongModel_throws() {
    // First model
    var variables = new Variables(modelId);
    var x = variables.addVariable("x");
    // Second model
    var modelId2 = new ModelId("m2");
    var variables2 = new Variables(modelId2);
    var unused = variables2.addVariable("x");
    var constraints2 = new LinearConstraints(modelId2);

    var getColumnWrongModelException = assertThrows(
        IllegalArgumentException.class, () -> constraints2.getUnmodifiableColumnNonzeros(x));

    assertThat(getColumnWrongModelException).hasMessageThat().contains("another model");
  }

  @Test
  public void onVariableDeleted_variableInConstraint_rowAndColumnCleanedUp() {
    var variables = new Variables(modelId);
    Variable x = variables.addVariable("x");
    var constraints = new LinearConstraints(modelId);
    LinearConstraint c = constraints.addLinearConstraint("c").setTerm(x, 1.0);

    constraints.onVariableDeleted(x);

    assertThat(c.getUnmodifiableTerms()).isEmpty();
    assertThat(constraints.getUnmodifiableColumnNonzeros(x)).isEmpty();
  }

  @Test
  public void onVariableDeleted_variableNotInAnyConstraint_noEffect() {
    var variables = new Variables(modelId);
    Variable x = variables.addVariable("x");
    Variable y = variables.addVariable("y");
    var constraints = new LinearConstraints(modelId);
    LinearConstraint c = constraints.addLinearConstraint("c").setTerm(x, 1.0);

    constraints.onVariableDeleted(y);

    assertThat(c.getUnmodifiableTerms()).containsExactly(x, 1.0);
    assertThat(constraints.getUnmodifiableColumnNonzeros(x)).containsExactly(c);
    assertThat(constraints.getUnmodifiableColumnNonzeros(y)).isEmpty();
  }

  @Test
  public void onVariableDeleted_wrongModel_throws() {
    var variables = new Variables(modelId);
    Variable x = variables.addVariable("x");
    var modelId2 = new ModelId("m");
    var constraints = new LinearConstraints(modelId2);

    var onVariableDeletedWrongModelException =
        assertThrows(IllegalArgumentException.class, () -> constraints.onVariableDeleted(x));

    assertThat(onVariableDeletedWrongModelException).hasMessageThat().contains("another model");
  }

  @Test
  public void toProto_emptyConstraints() {
    var constraints = new LinearConstraints(modelId);
    assertThat(constraints.toProto())
        .ignoringFieldAbsence()
        .isEqualTo(LinearConstraintsProto.getDefaultInstance());
  }

  @Test
  public void toProto_withConstraints() {
    var constraints = new LinearConstraints(modelId);
    constraints.addLinearConstraint("c1").setLowerBound(-2.0).setUpperBound(4.0);
    constraints.addLinearConstraint("c2").setUpperBound(3.0);

    assertThat(constraints.toProto())
        .ignoringFieldAbsence()
        .isEqualTo(LinearConstraintsProto.newBuilder()
                .addIds(0)
                .addIds(1)
                .addLowerBounds(-2.0)
                .addLowerBounds(Double.NEGATIVE_INFINITY)
                .addUpperBounds(4.0)
                .addUpperBounds(3.0)
                .addNames("c1")
                .addNames("c2")
                .build());
  }

  @Test
  public void toProto_withDeletes() {
    var constraints = new LinearConstraints(modelId);
    constraints.addLinearConstraint("c1").setLowerBound(-2.0).setUpperBound(4.0);
    LinearConstraint c2 = constraints.addLinearConstraint("c2").setUpperBound(3.0);
    constraints.addLinearConstraint("c3").setLowerBound(1.0);

    constraints.deleteLinearConstraint(c2);

    assertThat(constraints.toProto())
        .ignoringFieldAbsence()
        .isEqualTo(LinearConstraintsProto.newBuilder()
                .addIds(0)
                .addIds(2)
                .addLowerBounds(-2.0)
                .addLowerBounds(1.0)
                .addUpperBounds(4.0)
                .addUpperBounds(Double.POSITIVE_INFINITY)
                .addNames("c1")
                .addNames("c3")
                .build());
  }

  @Test
  public void matrixProto_emptyConstraints() {
    var constraints = new LinearConstraints(modelId);
    assertThat(constraints.matrixProto())
        .ignoringFieldAbsence()
        .isEqualTo(SparseDoubleMatrixProto.getDefaultInstance());
  }

  @Test
  public void matrixProto_withConstraintsNoTerms_isEmpty() {
    var constraints = new LinearConstraints(modelId);
    constraints.addLinearConstraint("c1").setLowerBound(-2.0).setUpperBound(4.0);

    assertThat(constraints.matrixProto())
        .ignoringFieldAbsence()
        .isEqualTo(SparseDoubleMatrixProto.getDefaultInstance());
  }

  @Test
  public void matrixProto_withTerms() {
    var variables = new Variables(modelId);
    Variable x = variables.addVariable("x");
    Variable y = variables.addVariable("y");
    var constraints = new LinearConstraints(modelId);
    constraints.addLinearConstraint("c").setTerm(x, 1.0);
    constraints.addLinearConstraint("d").setTerm(x, 3.0).setTerm(y, 2.0);

    assertThat(constraints.matrixProto())
        .ignoringFieldAbsence()
        .isEqualTo(SparseDoubleMatrixProto.newBuilder()
                .addRowIds(0)
                .addRowIds(1)
                .addRowIds(1)
                .addColumnIds(0)
                .addColumnIds(0)
                .addColumnIds(1)
                .addCoefficients(1.0)
                .addCoefficients(3.0)
                .addCoefficients(2.0)
                .build());
  }
}
