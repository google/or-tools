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
import static com.google.common.truth.Truth.assertWithMessage;
import static com.google.common.truth.extensions.proto.ProtoTruth.assertThat;
import static com.google.ortools.mathopt.Expressions.linExpr;
import static org.junit.jupiter.api.Assertions.assertThrows;

import com.google.common.base.Joiner;
import com.google.common.truth.StandardSubjectBuilder;
import com.google.ortools.mathopt.IndicatorConstraintProto;
import com.google.ortools.mathopt.LinearConstraintsProto;
import com.google.ortools.mathopt.LinearExpressionProto;
import com.google.ortools.mathopt.ModelProto;
import com.google.ortools.mathopt.ModelUpdateProto;
import com.google.ortools.mathopt.ObjectiveProto;
import com.google.ortools.mathopt.QuadraticConstraintProto;
import com.google.ortools.mathopt.SecondOrderConeConstraintProto;
import com.google.ortools.mathopt.SosConstraintProto;
import com.google.ortools.mathopt.SparseDoubleMatrixProto;
import com.google.ortools.mathopt.SparseDoubleVectorProto;
import com.google.ortools.mathopt.VariablesProto;
import com.google.ortools.mathopt.testing.MathOptProtoSubject;
import org.junit.jupiter.api.Test;

public final class ModelTest {
  @Test
  public void createModel_unnamed_nameIsEmpty() {
    var model = new Model();
    assertThat(model.getName()).isEmpty();
    assertThat(model.getObjective().getName()).isEmpty();
  }

  @Test
  public void createModel_withName_hasName() {
    var model = new Model("test_model");
    assertThat(model.getName()).isEqualTo("test_model");
    assertThat(model.getObjective().getName()).isEmpty();
  }

  @Test
  public void createModel_namedObjective_hasName() {
    var model = new Model("test_model", "objA");
    assertThat(model.getName()).isEqualTo("test_model");
    assertThat(model.getObjective().getName()).isEqualTo("objA");
  }

  @Test
  public void readVariableProperties_emptyModel() {
    var model = new Model("test_model");

    assertThat(model.getNextVariableId()).isEqualTo(0L);
    assertThat(model.hasVariableWithId(0L)).isFalse();
    assertThat(model.numVariables()).isEqualTo(0);
  }

  @Test
  public void addVariable0_readAttributes() {
    var model = new Model("test_model");

    Variable x = model.addVariable();

    assertThat(x.getId()).isEqualTo(0);
    assertThat(x.getLowerBound()).isNegativeInfinity();
    assertThat(x.getUpperBound()).isPositiveInfinity();
    assertThat(x.isInteger()).isFalse();
    assertThat(x.getName()).isEmpty();
  }

  @Test
  public void addVariable1_readAttributes() {
    var model = new Model("test_model");

    Variable x = model.addVariable("x");

    assertThat(x.getId()).isEqualTo(0);
    assertThat(x.getLowerBound()).isNegativeInfinity();
    assertThat(x.getUpperBound()).isPositiveInfinity();
    assertThat(x.isInteger()).isFalse();
    assertThat(x.getName()).isEqualTo("x");
  }

  @Test
  public void addVariable2_readAttributes() {
    var model = new Model("test_model");

    Variable x = model.addVariable(-1.0, 2.0);

    assertThat(x.getId()).isEqualTo(0);
    assertThat(x.getLowerBound()).isEqualTo(-1.0);
    assertThat(x.getUpperBound()).isEqualTo(2.0);
    assertThat(x.isInteger()).isFalse();
    assertThat(x.getName()).isEmpty();
  }

  @Test
  public void addVariable3Continuous_readAttributes() {
    var model = new Model("test_model");

    Variable x = model.addVariable(-1.0, 2.0, "x");

    assertThat(x.getId()).isEqualTo(0);
    assertThat(x.getLowerBound()).isEqualTo(-1.0);
    assertThat(x.getUpperBound()).isEqualTo(2.0);
    assertThat(x.isInteger()).isFalse();
    assertThat(x.getName()).isEqualTo("x");
  }

  @Test
  public void addVariable3Unnamed_readAttributes() {
    var model = new Model("test_model");

    Variable x = model.addVariable(-1.0, 2.0, true);

    assertThat(x.getId()).isEqualTo(0);
    assertThat(x.getLowerBound()).isEqualTo(-1.0);
    assertThat(x.getUpperBound()).isEqualTo(2.0);
    assertThat(x.isInteger()).isTrue();
    assertThat(x.getName()).isEmpty();
  }

  @Test
  public void addVariable4_readAttributes() {
    var model = new Model("test_model");

    Variable x =
        model.addVariable(/* lowerBound= */ -1.0, /* upperBound=*/2.0, /*isInteger=*/true, "x");

    assertThat(x.getId()).isEqualTo(0);
    assertThat(x.getLowerBound()).isEqualTo(-1.0);
    assertThat(x.getUpperBound()).isEqualTo(2.0);
    assertThat(x.isInteger()).isTrue();
    assertThat(x.getName()).isEqualTo("x");
  }

  @Test
  public void addIntegerVariable0_readAttributes() {
    var model = new Model("test_model");

    Variable x = model.addIntegerVariable();

    assertThat(x.getId()).isEqualTo(0);
    assertThat(x.getLowerBound()).isNegativeInfinity();
    assertThat(x.getUpperBound()).isPositiveInfinity();
    assertThat(x.isInteger()).isTrue();
    assertThat(x.getName()).isEmpty();
  }

  @Test
  public void addIntegerVariable1_readAttributes() {
    var model = new Model("test_model");

    Variable x = model.addIntegerVariable("x");

    assertThat(x.getId()).isEqualTo(0);
    assertThat(x.getLowerBound()).isNegativeInfinity();
    assertThat(x.getUpperBound()).isPositiveInfinity();
    assertThat(x.isInteger()).isTrue();
    assertThat(x.getName()).isEqualTo("x");
  }

  @Test
  public void addIntegerVariable2_readAttributes() {
    var model = new Model("test_model");

    Variable x = model.addIntegerVariable(-1.0, 2.0);

    assertThat(x.getId()).isEqualTo(0);
    assertThat(x.getLowerBound()).isEqualTo(-1.0);
    assertThat(x.getUpperBound()).isEqualTo(2.0);
    assertThat(x.isInteger()).isTrue();
    assertThat(x.getName()).isEmpty();
  }

  @Test
  public void addIntegerVariable3_readAttributes() {
    var model = new Model("test_model");

    Variable x = model.addIntegerVariable(-1.0, 2.0, "x");

    assertThat(x.getId()).isEqualTo(0);
    assertThat(x.getLowerBound()).isEqualTo(-1.0);
    assertThat(x.getUpperBound()).isEqualTo(2.0);
    assertThat(x.isInteger()).isTrue();
    assertThat(x.getName()).isEqualTo("x");
  }

  @Test
  public void addBinaryVariable0_readAttributes() {
    var model = new Model("test_model");

    Variable x = model.addBinaryVariable();

    assertThat(x.getId()).isEqualTo(0);
    assertThat(x.getLowerBound()).isEqualTo(0.0);
    assertThat(x.getUpperBound()).isEqualTo(1.0);
    assertThat(x.isInteger()).isTrue();
    assertThat(x.getName()).isEmpty();
  }

  @Test
  public void addBinaryVariable1_readAttributes() {
    var model = new Model("test_model");

    Variable x = model.addBinaryVariable("x");

    assertThat(x.getId()).isEqualTo(0);
    assertThat(x.getLowerBound()).isEqualTo(0.0);
    assertThat(x.getUpperBound()).isEqualTo(1.0);
    assertThat(x.isInteger()).isTrue();
    assertThat(x.getName()).isEqualTo("x");
  }

  @Test
  public void readVariableProperties_withVariables() {
    var model = new Model("test_model");
    Variable x = model.addBinaryVariable("x");
    Variable y = model.addVariable(-1.0, 2.0, "y");

    assertThat(model.getNextVariableId()).isEqualTo(2L);
    assertThat(model.hasVariableWithId(0)).isTrue();
    assertThat(model.hasVariableWithId(2)).isFalse();
    assertThat(model.numVariables()).isEqualTo(2);
    assertThat(model.getVariable(0)).isEqualTo(x);
    assertThat(model.getVariable(1)).isEqualTo(y);
    assertThat(model.getVariable(2)).isNull();
  }

  @Test
  public void deleteVariable_isDeleted() {
    var model = new Model("test_model");
    Variable x = model.addBinaryVariable("x");

    assertThat(model.deleteVariable(x)).isTrue();

    assertThat(x.isDeleted()).isTrue();
  }

  @Test
  public void readVariableProperties_withDeletedVariable() {
    var model = new Model("test_model");
    Variable x = model.addBinaryVariable("x");
    Variable y = model.addVariable(-1.0, 2.0, "y");
    Variable z = model.addBinaryVariable("z");
    assertThat(model.deleteVariable(y)).isTrue();

    assertThat(model.getNextVariableId()).isEqualTo(3L);
    assertThat(model.hasVariableWithId(0)).isTrue();
    assertThat(model.hasVariableWithId(1)).isFalse();
    assertThat(model.hasVariableWithId(2)).isTrue();
    assertThat(model.hasVariableWithId(3)).isFalse();
    assertThat(model.numVariables()).isEqualTo(2);
    assertThat(model.getVariable(0)).isEqualTo(x);
    assertThat(model.getVariable(1)).isNull();
    assertThat(model.getVariable(2)).isEqualTo(z);
  }

  @Test
  public void deleteVariable_deleteTwice_secondDeleteIgnored() {
    var model = new Model("test_model");
    Variable x = model.addBinaryVariable("x");
    var unused = model.addBinaryVariable("y");
    assertThat(model.deleteVariable(x)).isTrue();
    assertThat(model.numVariables()).isEqualTo(1);
    assertThat(model.getNextVariableId()).isEqualTo(2L);
    assertThat(x.isDeleted()).isTrue();

    assertThat(model.deleteVariable(x)).isFalse();

    assertThat(model.numVariables()).isEqualTo(1);
    assertThat(model.getNextVariableId()).isEqualTo(2L);
    assertThat(x.isDeleted()).isTrue();
  }

  @Test
  public void deleteVariable_notOwned_throws() {
    var model1 = new Model("test_model");
    Variable x1 = model1.addBinaryVariable("x");
    var model2 = new Model("test_model");
    var unused = model2.addBinaryVariable("x");

    assertThrows(IllegalArgumentException.class, () -> model2.deleteVariable(x1));
  }

  @Test
  public void checkOwned_variableCorrectModel_isOk() {
    var model = new Model("test_model");
    Variable x = model.addBinaryVariable("x");
    Variable y = model.addVariable(-1.0, 2.0, "y");

    model.checkOwned(x);
    model.checkOwned(y);
  }

  @Test
  public void checkOwned_deletedVariable_isOk() {
    var model = new Model("test_model");
    Variable x = model.addBinaryVariable("x");
    model.deleteVariable(x);

    model.checkOwned(x);
  }

  @Test
  public void checkOwned_variableWrongModel_throws() {
    var model1 = new Model("test_model");
    Variable x1 = model1.addBinaryVariable("x");
    var model2 = new Model("test_model");
    var unused = model2.addBinaryVariable("x");

    assertThrows(IllegalArgumentException.class, () -> model2.checkOwned(x1));
  }

  @Test
  public void readObjectiveProperties_emptyModel_isEmpty() {
    var model = new Model();
    assertThat(model.numObjectives()).isEqualTo(1);
    assertThat(model.hasAuxiliaryObjectiveWithId(0L)).isFalse();
    assertThat(model.getNextAuxiliaryObjectiveId()).isEqualTo(0L);
    assertThat(model.getAuxiliaryObjective(0L)).isNull();
  }

  @Test
  public void getObjective_emptyModel_isEmpty() {
    var model = new Model();

    Objective objective = model.getObjective();

    assertThat(objective.isMaximize()).isFalse();
    assertThat(objective.getPriority()).isEqualTo(0);
    assertThat(objective.getOffset()).isEqualTo(0);
    assertThat(objective.getUnmodifiableLinearTerms()).isEmpty();
    assertThat(objective.getName()).isEmpty();
  }

  private static StandardSubjectBuilder assertForDirection(boolean isMaximize) {
    return assertWithMessage("direction: %s", (isMaximize ? "maximize" : "minimize"));
  }

  @Test
  public void setObjective_wasEmpty_hasNewObjective() {
    for (boolean isMaximize : new boolean[] {false, true}) {
      var model = new Model();
      Variable x = model.addVariable();

      model.setObjective(linExpr().add(2.0).add(4.0, x), isMaximize);

      assertForDirection(isMaximize).that(model.getObjective().isMaximize()).isEqualTo(isMaximize);
      assertForDirection(isMaximize).that(model.getObjective().getOffset()).isEqualTo(2.0);
      assertForDirection(isMaximize)
          .that(model.getObjective().getUnmodifiableLinearTerms())
          .containsExactly(x, 4.0);
    }
  }

  @Test
  public void setObjective_hadOldObjective_overwrites() {
    for (boolean isMaximize : new boolean[] {false, true}) {
      var model = new Model();
      Variable x = model.addVariable();
      Variable y = model.addVariable();
      Variable z = model.addVariable();
      // The initial objective has the opposite direction.
      model.getObjective().setMaximize(!isMaximize).add(3.0).subtract(5.0, x).subtract(z);

      model.setObjective(linExpr().add(2.0).add(4.0, x).add(y), isMaximize);

      assertForDirection(isMaximize).that(model.getObjective().isMaximize()).isEqualTo(isMaximize);
      assertForDirection(isMaximize).that(model.getObjective().getOffset()).isEqualTo(2.0);
      assertForDirection(isMaximize)
          .that(model.getObjective().getUnmodifiableLinearTerms())
          .containsExactly(x, 4.0, y, 1.0);
    }
  }

  // NOTE: we need only unit test that Objective.replace() will throw, so we only test one case.
  @Test
  public void setObjective_deletedVariable_throws() {
    var model = new Model();
    Variable x = model.addVariable();
    var obj = new HashLinearExpression(x);
    model.deleteVariable(x);

    assertThrows(
        IllegalArgumentException.class, () -> model.setObjective(obj, /* isMaximize= */ true));
  }

  @Test
  public void minimze_wasEmpty_hasNewObjective() {
    var model = new Model();
    Variable x = model.addVariable();

    model.minimize(linExpr().add(2.0).add(4.0, x));

    assertThat(model.getObjective().isMaximize()).isFalse();
    assertThat(model.getObjective().getOffset()).isEqualTo(2.0);
    assertThat(model.getObjective().getUnmodifiableLinearTerms()).containsExactly(x, 4.0);
  }

  @Test
  public void maximize_wasEmpty_hasNewObjective() {
    var model = new Model();
    Variable x = model.addVariable();

    model.maximize(linExpr().add(2.0).add(4.0, x));

    assertThat(model.getObjective().isMaximize()).isTrue();
    assertThat(model.getObjective().getOffset()).isEqualTo(2.0);
    assertThat(model.getObjective().getUnmodifiableLinearTerms()).containsExactly(x, 4.0);
  }

  @Test
  public void addAuxiliaryObjective_unnamedDefaultPriority_isEmpty() {
    var model = new Model();

    AuxiliaryObjective objective = model.addAuxiliaryObjective();

    assertThat(objective.getId()).isEqualTo(0L);
    assertThat(objective.isDeleted()).isFalse();
    assertThat(objective.getPriority()).isEqualTo(1L);
    assertThat(objective.getName()).isEmpty();
    assertThat(objective.getOffset()).isZero();
    assertThat(objective.getUnmodifiableLinearTerms()).isEmpty();
  }

  @Test
  public void addAuxiliaryObjective_existingObjectiveLargePriority_newObjectiveBigger() {
    var model = new Model();
    model.getObjective().setPriority(3L);

    AuxiliaryObjective objective = model.addAuxiliaryObjective();

    assertThat(objective.getPriority()).isEqualTo(4L);
  }

  @Test
  public void addAuxiliaryObjective_withName_hasName() {
    var model = new Model();

    AuxiliaryObjective objective = model.addAuxiliaryObjective("objA");

    assertThat(objective.getName()).isEqualTo("objA");
  }

  @Test
  public void addAuxiliaryObjective_withPriorityAndName_hasPriorityAndName() {
    var model = new Model();

    AuxiliaryObjective objective = model.addAuxiliaryObjective(3L, "objA");

    assertThat(objective.getName()).isEqualTo("objA");
    assertThat(objective.getPriority()).isEqualTo(3L);
  }

  @Test
  public void deleteObjective_isDeleted() {
    var model = new Model();
    AuxiliaryObjective objective = model.addAuxiliaryObjective();

    assertThat(model.deleteAuxiliaryObjective(objective)).isTrue();

    assertThat(objective.isDeleted()).isTrue();
    assertThat(model.numObjectives()).isEqualTo(1L);
    assertThat(model.hasAuxiliaryObjectiveWithId(0L)).isFalse();
    assertThat(model.getAuxiliaryObjective(0L)).isNull();
    assertThat(model.getNextAuxiliaryObjectiveId()).isEqualTo(1L);
  }

  @Test
  public void deleteObjective_deleteTwice_isFalseNoEffect() {
    var model = new Model();
    AuxiliaryObjective objective = model.addAuxiliaryObjective();

    assertThat(model.deleteAuxiliaryObjective(objective)).isTrue();
    assertThat(model.deleteAuxiliaryObjective(objective)).isFalse();

    assertThat(objective.isDeleted()).isTrue();
    assertThat(model.numObjectives()).isEqualTo(1L);
    assertThat(model.hasAuxiliaryObjectiveWithId(0L)).isFalse();
    assertThat(model.getAuxiliaryObjective(0L)).isNull();
    assertThat(model.getNextAuxiliaryObjectiveId()).isEqualTo(1L);
  }

  @Test
  public void deleteObjective_wrongModel_throws() {
    var model = new Model("modelA");
    var otherModel = new Model("modelB");
    AuxiliaryObjective objective = otherModel.addAuxiliaryObjective("modelBObj");

    var error = assertThrows(
        IllegalArgumentException.class, () -> model.deleteAuxiliaryObjective(objective));

    assertThat(error).hasMessageThat().contains("another model");
  }

  @Test
  public void checkOwned_primaryObjectiveIsOwned_isOk() {
    var model = new Model();

    model.checkOwned(model.getObjective());
  }

  @Test
  public void checkOwned_auxiliaryObjectiveIsOwned_isOk() {
    var model = new Model();
    AuxiliaryObjective obj = model.addAuxiliaryObjective();

    model.checkOwned(obj);
  }

  @Test
  public void checkOwned_auxiliaryObjectiveIsOwnedAndDeleted_isOk() {
    var model = new Model();
    AuxiliaryObjective obj = model.addAuxiliaryObjective();
    model.deleteAuxiliaryObjective(obj);

    model.checkOwned(obj);
  }

  @Test
  public void checkOwned_primaryObjectiveOtherModel_throws() {
    var model = new Model("modelA", "objA");
    var otherModel = new Model("modelB", "objB");

    var error = assertThrows(
        IllegalArgumentException.class, () -> model.checkOwned(otherModel.getObjective()));

    assertThat(error).hasMessageThat().contains("another model");
  }

  @Test
  public void checkOwned_auxiliaryObjectiveOtherModel_throws() {
    var model = new Model("modelA");
    var otherModel = new Model("modelB");
    AuxiliaryObjective otherObj = otherModel.addAuxiliaryObjective("auxObjModelB");

    var error = assertThrows(IllegalArgumentException.class, () -> model.checkOwned(otherObj));

    assertThat(error).hasMessageThat().contains("another model");
  }

  @Test
  public void readLinearConstraintProperties_emptyModel_isEmpty() {
    var model = new Model("test_model");

    assertThat(model.getNextLinearConstraintId()).isEqualTo(0L);
    assertThat(model.hasLinearConstraintWithId(0)).isFalse();
    assertThat(model.numLinearConstraint()).isEqualTo(0);
  }

  @Test
  public void addLinearConstraint_unnamedUnboundedEmpty_attributesOk() {
    var model = new Model("test_model");

    LinearConstraint c = model.addLinearConstraint();

    assertThat(c.getId()).isEqualTo(0);
    assertThat(c.getLowerBound()).isNegativeInfinity();
    assertThat(c.getUpperBound()).isPositiveInfinity();
    assertThat(c.getName()).isEmpty();
  }

  @Test
  public void addLinearConstraint_namedUnboundedEmpty_attributesOk() {
    var model = new Model("test_model");

    LinearConstraint c = model.addLinearConstraint("c");

    assertThat(c.getId()).isEqualTo(0);
    assertThat(c.getLowerBound()).isNegativeInfinity();
    assertThat(c.getUpperBound()).isPositiveInfinity();
    assertThat(c.getName()).isEqualTo("c");
  }

  @Test
  public void addLinearConstraint_unnamedBoundedEmpty_attributesOk() {
    var model = new Model("test_model");

    LinearConstraint c = model.addLinearConstraint(/* lowerBound= */ 1.0, /* upperBound= */ 2.0);

    assertThat(c.getId()).isEqualTo(0);
    assertThat(c.getLowerBound()).isEqualTo(1.0);
    assertThat(c.getUpperBound()).isEqualTo(2.0);
    assertThat(c.getName()).isEmpty();
  }

  @Test
  public void addLinearConstraint_namedBoundedEmpty_attributesOk() {
    var model = new Model("test_model");

    LinearConstraint c =
        model.addLinearConstraint(/* lowerBound= */ 1.0, /* upperBound= */ 2.0, "c");

    assertThat(c.getId()).isEqualTo(0);
    assertThat(c.getLowerBound()).isEqualTo(1.0);
    assertThat(c.getUpperBound()).isEqualTo(2.0);
    assertThat(c.getName()).isEqualTo("c");
  }

  @Test
  public void readLinearConstraintProperties_withConstraints_hasConstraints() {
    var model = new Model("test_model");
    LinearConstraint c = model.addLinearConstraint("c");
    LinearConstraint d =
        model.addLinearConstraint(/* lowerBound= */ -1.0, /* upperBound= */ 2.0, "d");

    assertThat(model.getNextLinearConstraintId()).isEqualTo(2L);
    assertThat(model.hasLinearConstraintWithId(0)).isTrue();
    assertThat(model.hasLinearConstraintWithId(2)).isFalse();
    assertThat(model.numLinearConstraint()).isEqualTo(2);
    assertThat(model.getLinearConstraint(0)).isEqualTo(c);
    assertThat(model.getLinearConstraint(1)).isEqualTo(d);
    assertThat(model.getLinearConstraint(2)).isNull();
  }

  @Test
  public void deleteLinearConstraint_deleteConstraint_isDeleted() {
    var model = new Model("test_model");
    LinearConstraint c = model.addLinearConstraint("c");

    assertThat(model.deleteLinearConstraint(c)).isTrue();

    assertThat(c.isDeleted()).isTrue();
  }

  @Test
  public void readLinearConstraintProperties_withDeletedConstraint_countsDeletion() {
    var model = new Model("test_model");
    LinearConstraint c = model.addLinearConstraint();
    LinearConstraint d = model.addLinearConstraint();
    LinearConstraint e = model.addLinearConstraint();
    assertThat(model.deleteLinearConstraint(d)).isTrue();

    assertThat(model.getNextLinearConstraintId()).isEqualTo(3L);
    assertThat(model.hasLinearConstraintWithId(0)).isTrue();
    assertThat(model.hasLinearConstraintWithId(1)).isFalse();
    assertThat(model.hasLinearConstraintWithId(2)).isTrue();
    assertThat(model.hasLinearConstraintWithId(3)).isFalse();
    assertThat(model.numLinearConstraint()).isEqualTo(2);
    assertThat(model.getLinearConstraint(0)).isEqualTo(c);
    assertThat(model.getLinearConstraint(1)).isNull();
    assertThat(model.getLinearConstraint(2)).isEqualTo(e);
  }

  @Test
  public void deleteLinearConstraint_deleteTwice_secondDeleteIgnored() {
    var model = new Model("test_model");
    LinearConstraint c = model.addLinearConstraint("c");
    var unused = model.addLinearConstraint("d");
    assertThat(model.deleteLinearConstraint(c)).isTrue();
    assertThat(model.numLinearConstraint()).isEqualTo(1);
    assertThat(model.getNextLinearConstraintId()).isEqualTo(2L);
    assertThat(c.isDeleted()).isTrue();

    assertThat(model.deleteLinearConstraint(c)).isFalse();

    assertThat(model.numLinearConstraint()).isEqualTo(1);
    assertThat(model.getNextLinearConstraintId()).isEqualTo(2L);
    assertThat(c.isDeleted()).isTrue();
  }

  @Test
  public void deleteLinearConstraint_notOwned_throws() {
    var model1 = new Model("test_model");
    LinearConstraint c1 = model1.addLinearConstraint("c");
    var model2 = new Model("test_model");
    var unused = model2.addLinearConstraint("c");

    assertThrows(IllegalArgumentException.class, () -> model2.deleteLinearConstraint(c1));
  }

  @Test
  public void checkOwned_linearConstraintCorrectModel_isOk() {
    var model = new Model("test_model");
    LinearConstraint c = model.addLinearConstraint("c");
    LinearConstraint d = model.addLinearConstraint("d");

    model.checkOwned(c);
    model.checkOwned(d);
  }

  @Test
  public void checkOwned_deletedLinearConstraint_isOk() {
    var model = new Model("test_model");
    LinearConstraint c = model.addLinearConstraint("c");
    model.deleteLinearConstraint(c);

    model.checkOwned(c);
  }

  @Test
  public void checkOwned_linearConstraintWrongModel_throws() {
    var model1 = new Model("test_model");
    LinearConstraint c1 = model1.addLinearConstraint("c");
    var model2 = new Model("test_model");
    var unused = model2.addLinearConstraint("c");

    assertThrows(IllegalArgumentException.class, () -> model2.checkOwned(c1));
  }

  @Test
  public void getColumn_emptyColumn_isOk() {
    var model = new Model("test_model");
    Variable x = model.addBinaryVariable();

    assertThat(model.getUnmodifiableColumnNonzeros(x)).isEmpty();
  }

  @Test
  public void getColumn_nonEmptyColumn_isOk() {
    var model = new Model("test_model");
    Variable x = model.addBinaryVariable();
    LinearConstraint c = model.addLinearConstraint("c").setTerm(x, 1.0);
    var unused = model.addLinearConstraint("d");
    LinearConstraint e = model.addLinearConstraint("e").setTerm(x, 1.0);

    assertThat(model.getUnmodifiableColumnNonzeros(x)).containsExactly(c, e);
  }

  @Test
  public void getColumn_withZeroedCoefficient_isCleared() {
    var model = new Model("test_model");
    Variable x = model.addBinaryVariable();
    LinearConstraint c = model.addLinearConstraint("c").setTerm(x, 1.0);
    LinearConstraint d = model.addLinearConstraint("d").setTerm(x, 1.0);

    c.setTerm(x, 0.0);

    assertThat(model.getUnmodifiableColumnNonzeros(x)).containsExactly(d);
  }

  @Test
  public void getColumn_withDeletedConstraint_isCleared() {
    var model = new Model("test_model");
    Variable x = model.addBinaryVariable();
    LinearConstraint c = model.addLinearConstraint("c").setTerm(x, 1.0);
    LinearConstraint d = model.addLinearConstraint("d").setTerm(x, 1.0);

    model.deleteLinearConstraint(c);

    assertThat(model.getUnmodifiableColumnNonzeros(x)).containsExactly(d);
  }

  @Test
  public void getColumn_withAllConstraintsDeleted_isEmpty() {
    var model = new Model("test_model");
    Variable x = model.addBinaryVariable();
    LinearConstraint c = model.addLinearConstraint("c").setTerm(x, 1.0);

    model.deleteLinearConstraint(c);

    assertThat(model.getUnmodifiableColumnNonzeros(x)).isEmpty();
  }

  @Test
  public void deleteVariable_inLinearConstraint_constraintUpdated() {
    var model = new Model("test_model");
    Variable x = model.addBinaryVariable();
    Variable y = model.addBinaryVariable();
    LinearConstraint c = model.addLinearConstraint("c").setTerm(x, 1.0).setTerm(y, 2.0);

    model.deleteVariable(x);

    assertThat(c.getUnmodifiableTerms()).containsExactly(y, 2.0);
  }

  @Test
  public void deleteVariable_inObjective_objectiveUpdated() {
    var model = new Model();
    Variable x = model.addBinaryVariable();
    Variable y = model.addBinaryVariable();
    Objective objective = model.getObjective().setLinearTerm(x, 1.0).setLinearTerm(y, 2.0);

    model.deleteVariable(x);

    assertThat(objective.getUnmodifiableLinearTerms()).containsExactly(y, 2.0);
  }

  @Test
  public void deleteVariable_inAuxiliaryObjective_objectiveUpdated() {
    var model = new Model();
    Variable x = model.addBinaryVariable();
    Variable y = model.addBinaryVariable();
    Objective objective = model.addAuxiliaryObjective().setLinearTerm(x, 1.0).setLinearTerm(y, 2.0);

    model.deleteVariable(x);

    assertThat(objective.getUnmodifiableLinearTerms()).containsExactly(y, 2.0);
  }

  @Test
  public void toProto_emptyModel_emptyProto() {
    var model = new Model("test_model");

    assertThat(model.toProto())
        .ignoringFieldAbsence()
        .isEqualTo(ModelProto.newBuilder().setName("test_model").build());
  }

  @Test
  public void toProto_withVariables_hasVariables() {
    var model = new Model("test_model");

    var unused = model.addBinaryVariable("x");
    var unused2 = model.addVariable(-1.0, 2.0, "y");

    var expected = ModelProto.newBuilder();
    expected.setName("test_model")
        .getVariablesBuilder()
        .addIds(0)
        .addIds(1)
        .addLowerBounds(0.0)
        .addLowerBounds(-1.0)
        .addUpperBounds(1.0)
        .addUpperBounds(2.0)
        .addIntegers(true)
        .addIntegers(false)
        .addNames("x")
        .addNames("y");
    assertThat(model.toProto()).ignoringFieldAbsence().isEqualTo(expected.build());
  }

  @Test
  public void toProto_withDeletedVariable() {
    var model = new Model("test_model");
    var unused = model.addBinaryVariable("x");
    Variable y = model.addVariable(-1.0, 2.0, "y");
    var unused2 = model.addBinaryVariable("z");
    assertThat(model.deleteVariable(y)).isTrue();

    var expectedModelProto = ModelProto.newBuilder();
    expectedModelProto.setName("test_model")
        .getVariablesBuilder()
        .addIds(0)
        .addIds(2)
        .addLowerBounds(0.0)
        .addLowerBounds(0.0)
        .addUpperBounds(1.0)
        .addUpperBounds(1.0)
        .addIntegers(true)
        .addIntegers(true)
        .addNames("x")
        .addNames("z");
    assertThat(model.toProto()).ignoringFieldAbsence().isEqualTo(expectedModelProto.build());
  }

  @Test
  public void toProto_deleteVariableTwice_modelProtoOk() {
    var model = new Model("test_model");
    Variable x = model.addBinaryVariable("x");
    var unused = model.addBinaryVariable("y");

    assertThat(model.deleteVariable(x)).isTrue();
    assertThat(model.deleteVariable(x)).isFalse();

    var expectedModelProto = ModelProto.newBuilder().setName("test_model");
    expectedModelProto.getVariablesBuilder()
        .addIds(1)
        .addLowerBounds(0.0)
        .addUpperBounds(1.0)
        .addIntegers(true)
        .addNames("y");
    assertThat(model.toProto()).ignoringFieldAbsence().isEqualTo(expectedModelProto.build());
  }

  @Test
  public void toProto_withObjective_hasObjective() {
    var model = new Model("test_model", "objA");
    Variable x = model.addBinaryVariable("x");
    model.getObjective().setToMaximize().setPriority(2L).setOffset(3.0).setLinearTerm(x, 4.0);

    ModelProto actualModelProto = model.toProto();

    var expectedModelProto = ModelProto.newBuilder().setName("test_model");
    expectedModelProto.getVariablesBuilder()
        .addIds(0)
        .addLowerBounds(0.0)
        .addUpperBounds(1.0)
        .addIntegers(true)
        .addNames("x");
    expectedModelProto.getObjectiveBuilder()
        .setName("objA")
        .setMaximize(true)
        .setPriority(2L)
        .setOffset(3.0)
        .getLinearCoefficientsBuilder()
        .addIds(0)
        .addValues(4.0);
    assertThat(actualModelProto).ignoringFieldAbsence().isEqualTo(expectedModelProto.build());
  }

  @Test
  public void toProto_withAuxiliaryObjective_hasAuxiliaryObjective() {
    var model = new Model("test_model", "");
    Variable x = model.addBinaryVariable("x");
    model.getObjective().setLinearTerm(x, 4.0);
    model.addAuxiliaryObjective("auxA").setToMaximize().setOffset(5.0).setLinearTerm(x, 6.0);

    ModelProto actualModelProto = model.toProto();

    var expectedModelProto = ModelProto.newBuilder().setName("test_model");
    expectedModelProto.getVariablesBuilder()
        .addIds(0)
        .addLowerBounds(0.0)
        .addUpperBounds(1.0)
        .addIntegers(true)
        .addNames("x");
    var auxObj =
        ObjectiveProto.newBuilder().setMaximize(true).setPriority(1L).setOffset(5.0).setName(
            "auxA");
    auxObj.getLinearCoefficientsBuilder().addIds(0).addValues(6.0);
    expectedModelProto.getObjectiveBuilder().getLinearCoefficientsBuilder().addIds(0).addValues(
        4.0);
    expectedModelProto.putAuxiliaryObjectives(0L, auxObj.build());
    assertThat(actualModelProto).ignoringFieldAbsence().isEqualTo(expectedModelProto.build());
  }

  @Test
  public void toProto_withLinearConstraints_hasLinearConstraints() {
    var model = new Model("test_model");

    Variable x = model.addBinaryVariable("x");
    Variable y = model.addBinaryVariable("y");

    model.addLinearConstraint("c").setTerm(x, 1.0);
    model.addLinearConstraint(-1.0, 2.0, "d").setTerm(x, 2.0).setTerm(y, 3.0);

    var expected = ModelProto.newBuilder().setName("test_model");
    expected.getVariablesBuilder()
        .addIds(0)
        .addLowerBounds(0.0)
        .addUpperBounds(1.0)
        .addIntegers(true)
        .addNames("x")
        .addIds(1)
        .addLowerBounds(0.0)
        .addUpperBounds(1.0)
        .addIntegers(true)
        .addNames("y");
    expected.getLinearConstraintsBuilder()
        .addIds(0)
        .addIds(1)
        .addLowerBounds(Double.NEGATIVE_INFINITY)
        .addLowerBounds(-1.0)
        .addUpperBounds(Double.POSITIVE_INFINITY)
        .addUpperBounds(2.0)
        .addNames("c")
        .addNames("d");
    expected.getLinearConstraintMatrixBuilder().addRowIds(0).addColumnIds(0).addCoefficients(1.0);
    expected.getLinearConstraintMatrixBuilder().addRowIds(1).addColumnIds(0).addCoefficients(2.0);
    expected.getLinearConstraintMatrixBuilder().addRowIds(1).addColumnIds(1).addCoefficients(3.0);
    assertThat(model.toProto()).ignoringFieldAbsence().isEqualTo(expected.build());
  }

  @Test
  public void toProto_withDeletedLinearConstraints_deletedConstraintsExcluded() {
    var model = new Model("test_model");
    Variable x = model.addBinaryVariable("x");
    model.addLinearConstraint("c").setTerm(x, 1.0);
    LinearConstraint d = model.addLinearConstraint(-1.0, 2.0, "d").setTerm(x, 2.0);
    model.addLinearConstraint(-4.0, 8.0, "e").setTerm(x, 3.0);
    assertThat(model.deleteLinearConstraint(d)).isTrue();

    var expectedModelProto = ModelProto.newBuilder().setName("test_model");
    expectedModelProto.getVariablesBuilder()
        .addIds(0)
        .addLowerBounds(0.0)
        .addUpperBounds(1.0)
        .addIntegers(true)
        .addNames("x");
    expectedModelProto.getLinearConstraintsBuilder()
        .addIds(0)
        .addIds(2)
        .addLowerBounds(Double.NEGATIVE_INFINITY)
        .addLowerBounds(-4.0)
        .addUpperBounds(Double.POSITIVE_INFINITY)
        .addUpperBounds(8.0)
        .addNames("c")
        .addNames("e");
    expectedModelProto.getLinearConstraintMatrixBuilder()
        .addRowIds(0)
        .addColumnIds(0)
        .addCoefficients(1.0);
    expectedModelProto.getLinearConstraintMatrixBuilder()
        .addRowIds(2)
        .addColumnIds(0)
        .addCoefficients(3.0);
    assertThat(model.toProto()).ignoringFieldAbsence().isEqualTo(expectedModelProto.build());
  }

  @Test
  public void toProto_deleteLinearConstraintTwice_modelProtoOk() {
    var model = new Model("test_model");
    LinearConstraint c = model.addLinearConstraint("c");
    var unused = model.addLinearConstraint(-1.0, 2.0, "d");

    assertThat(model.deleteLinearConstraint(c)).isTrue();
    assertThat(model.deleteLinearConstraint(c)).isFalse();

    var expectedModelProto = ModelProto.newBuilder().setName("test_model");
    expectedModelProto.getLinearConstraintsBuilder()
        .addIds(1)
        .addLowerBounds(-1.0)
        .addUpperBounds(2.0)
        .addNames("d");
    assertThat(model.toProto()).ignoringFieldAbsence().isEqualTo(expectedModelProto.build());
  }

  @Test
  public void toProto_deleteVariableInConstraint_coefficientDeleted() {
    var model = new Model("test_model");
    Variable x = model.addBinaryVariable("x");
    Variable y = model.addBinaryVariable("y");
    model.addLinearConstraint(0.0, 2.0, "c").setTerm(x, 1.0).setTerm(y, 2.0);
    assertThat(model.deleteVariable(x)).isTrue();

    var expectedModelProto = ModelProto.newBuilder().setName("test_model");
    expectedModelProto.getVariablesBuilder()
        .addIds(1)
        .addLowerBounds(0.0)
        .addUpperBounds(1.0)
        .addIntegers(true)
        .addNames("y");
    expectedModelProto.getLinearConstraintsBuilder()
        .addIds(0)
        .addLowerBounds(0.0)
        .addUpperBounds(2.0)
        .addNames("c");
    expectedModelProto.getLinearConstraintMatrixBuilder()
        .addRowIds(0)
        .addColumnIds(1)
        .addCoefficients(2.0);
    assertThat(model.toProto()).ignoringFieldAbsence().isEqualTo(expectedModelProto.build());
  }

  @Test
  public void fromProto_emptyProto_emptyModel() {
    var model = Model.fromProto(ModelProto.getDefaultInstance());
    assertThat(model.numVariables()).isEqualTo(0);
    assertThat(model.numLinearConstraint()).isEqualTo(0);
    assertThat(model.numObjectives()).isEqualTo(1);
    assertThat(model.getObjective().isMaximize()).isFalse();
    assertThat(model.getObjective().getOffset()).isEqualTo(0.0);
    assertThat(model.getObjective().getUnmodifiableLinearTerms()).isEmpty();
    assertThat(model.getName()).isEmpty();
  }

  @Test
  public void fromProto_noVariableOrConstraintNames_stillParsesModel() {
    var variables = VariablesProto.newBuilder()
                        .addIds(0)
                        .addLowerBounds(1.0)
                        .addUpperBounds(3.0)
                        .addIntegers(true)
                        .build();
    var linCons = LinearConstraintsProto.newBuilder()
                      .addIds(4)
                      .addLowerBounds(6.0)
                      .addUpperBounds(8.0)
                      .build();

    var model = Model.fromProto(
        ModelProto.newBuilder().setVariables(variables).setLinearConstraints(linCons).build());

    assertThat(model.numVariables()).isEqualTo(1);
    assertThat(model.hasVariableWithId(0)).isTrue();
    assertThat(model.getVariable(0).getName()).isEmpty();
    assertThat(model.hasLinearConstraintWithId(4)).isTrue();
    assertThat(model.getLinearConstraint(4).getName()).isEmpty();
  }

  @Test
  public void fromProto_largeModel_roundTrips() {
    var variables = VariablesProto.newBuilder()
                        .addIds(0)
                        .addIds(3)
                        .addNames("x")
                        .addNames("y")
                        .addLowerBounds(1.0)
                        .addLowerBounds(2.0)
                        .addUpperBounds(3.0)
                        .addUpperBounds(4.0)
                        .addIntegers(false)
                        .addIntegers(true)
                        .build();
    var linCons = LinearConstraintsProto.newBuilder()
                      .addIds(1)
                      .addIds(4)
                      .addNames("c")
                      .addNames("d")
                      .addLowerBounds(5.0)
                      .addLowerBounds(6.0)
                      .addUpperBounds(7.0)
                      .addUpperBounds(8.0)
                      .build();
    var linConMat = SparseDoubleMatrixProto.newBuilder()
                        .addRowIds(1)
                        .addRowIds(1)
                        .addRowIds(4)
                        .addColumnIds(0)
                        .addColumnIds(3)
                        .addColumnIds(0)
                        .addCoefficients(10.0)
                        .addCoefficients(11.0)
                        .addCoefficients(12.0)
                        .build();
    var obj = ObjectiveProto.newBuilder()
                  .setName("o1")
                  .setMaximize(true)
                  .setOffset(-10.0)
                  .setLinearCoefficients(
                      SparseDoubleVectorProto.newBuilder().addIds(3).addValues(2.5).build())
                  .setPriority(3)
                  .build();
    var auxObj = ObjectiveProto.newBuilder()
                     .setName("a1")
                     .setMaximize(false)
                     .setOffset(3.5)
                     .setLinearCoefficients(SparseDoubleVectorProto.newBuilder()
                             .addIds(0)
                             .addIds(3)
                             .addValues(2.8)
                             .addValues(3.8)
                             .build())
                     .build();

    var modelProto = ModelProto.newBuilder()
                         .setName("mod")
                         .setVariables(variables)
                         .setLinearConstraints(linCons)
                         .setLinearConstraintMatrix(linConMat)
                         .setObjective(obj)
                         .putAuxiliaryObjectives(10, auxObj)
                         .build();

    var model = Model.fromProto(modelProto);
    ModelProto roundTrip = model.toProto();

    MathOptProtoSubject.assertThat(modelProto).isEquivalentTo(roundTrip);
  }

  @Test
  public void fromProto_hasQuadraticConstraint_throws() {
    var modelProto = ModelProto.newBuilder()
                         .putQuadraticConstraints(
                             0, QuadraticConstraintProto.newBuilder().setUpperBound(5.0).build())
                         .build();

    var error =
        assertThrows(UnsupportedOperationException.class, () -> Model.fromProto(modelProto));

    assertThat(error).hasMessageThat().contains("quadratic constraint");
  }

  @Test
  public void fromProto_hasSecondOrderConeConstraint_throws() {
    var modelProto =
        ModelProto.newBuilder()
            .putSecondOrderConeConstraints(0,
                SecondOrderConeConstraintProto.newBuilder()
                    .setUpperBound(LinearExpressionProto.newBuilder().setOffset(3.0).build())
                    .build())
            .build();

    var error =
        assertThrows(UnsupportedOperationException.class, () -> Model.fromProto(modelProto));

    assertThat(error).hasMessageThat().contains("second order cone constraint");
  }

  @Test
  public void fromProto_hasSOS1Constraint_throws() {
    var zero = LinearExpressionProto.getDefaultInstance();
    var modelProto =
        ModelProto.newBuilder()
            .putSos1Constraints(0, SosConstraintProto.newBuilder().addExpressions(zero).build())
            .build();

    var error =
        assertThrows(UnsupportedOperationException.class, () -> Model.fromProto(modelProto));

    assertThat(error).hasMessageThat().contains("SOS1 constraint");
  }

  @Test
  public void fromProto_hasSOS2Constraint_throws() {
    var zero = LinearExpressionProto.getDefaultInstance();
    var modelProto =
        ModelProto.newBuilder()
            .putSos2Constraints(0,
                SosConstraintProto.newBuilder().addExpressions(zero).addExpressions(zero).build())
            .build();

    var error =
        assertThrows(UnsupportedOperationException.class, () -> Model.fromProto(modelProto));

    assertThat(error).hasMessageThat().contains("SOS2 constraint");
  }

  @Test
  public void fromProto_hasQuadraticObjective_throws() {
    // min x^2
    // x binary
    var modelProto = ModelProto.newBuilder()
                         .setVariables(VariablesProto.newBuilder()
                                 .addIds(0)
                                 .addLowerBounds(0)
                                 .addUpperBounds(1)
                                 .addIntegers(true)
                                 .addNames("x")
                                 .build())
                         .setObjective(ObjectiveProto.newBuilder()
                                 .setQuadraticCoefficients(SparseDoubleMatrixProto.newBuilder()
                                         .addRowIds(0)
                                         .addColumnIds(0)
                                         .addCoefficients(1)
                                         .build())
                                 .build())
                         .build();

    var error =
        assertThrows(UnsupportedOperationException.class, () -> Model.fromProto(modelProto));

    assertThat(error).hasMessageThat().contains("quadratic objective");
  }

  @Test
  public void fromProto_hasQuadraticAuxiliaryObjective_throws() {
    // (a) min 0
    // (b) min x^2
    // x binary
    var modelProto = ModelProto.newBuilder()
                         .setVariables(VariablesProto.newBuilder()
                                 .addIds(0)
                                 .addLowerBounds(0)
                                 .addUpperBounds(1)
                                 .addIntegers(true)
                                 .addNames("x")
                                 .build())
                         .putAuxiliaryObjectives(0,
                             ObjectiveProto.newBuilder()
                                 .setQuadraticCoefficients(SparseDoubleMatrixProto.newBuilder()
                                         .addRowIds(0)
                                         .addColumnIds(0)
                                         .addCoefficients(1)
                                         .build())
                                 .build())
                         .build();

    var error =
        assertThrows(UnsupportedOperationException.class, () -> Model.fromProto(modelProto));

    assertThat(error).hasMessageThat().contains("quadratic auxiliary objective");
  }

  @Test
  public void updateTrackerExportProto_noUpdates_emptyProto() {
    var model = new Model("test_model");
    Variable x = model.addBinaryVariable("x");
    LinearConstraint c = model.addLinearConstraint(-1.0, 1.0, "c");
    c.setTerm(x, 1.0);

    Model.UpdateTracker tracker = model.addTracker();

    assertThat(tracker.updateProto())
        .ignoringFieldAbsence()
        .isEqualTo(ModelUpdateProto.getDefaultInstance());
  }

  @Test
  public void updateTrackerExportProto_addVariable_hasNewVariable() {
    var model = new Model("test_model");

    Model.UpdateTracker tracker = model.addTracker();
    var unused = model.addBinaryVariable("x");

    var expectedUpdateProto = ModelUpdateProto.newBuilder();
    expectedUpdateProto.getNewVariablesBuilder()
        .addIds(0)
        .addLowerBounds(0.0)
        .addUpperBounds(1.0)
        .addIntegers(true)
        .addNames("x");
    assertThat(tracker.updateProto()).ignoringFieldAbsence().isEqualTo(expectedUpdateProto.build());
  }

  @Test
  public void updateTrackerExportProto_modifyVariable_hasVariableUpdates() {
    var model = new Model("test_model");
    Variable x = model.addBinaryVariable("x");

    Model.UpdateTracker tracker = model.addTracker();
    x.setUpperBound(2.0);

    var expectedUpdateProto = ModelUpdateProto.newBuilder();
    expectedUpdateProto.getVariableUpdatesBuilder().getUpperBoundsBuilder().addIds(0).addValues(
        2.0);
    assertThat(tracker.updateProto()).ignoringFieldAbsence().isEqualTo(expectedUpdateProto.build());
  }

  @Test
  public void updateTrackerExportProto_deleteVariable_hasVariableDeletes() {
    var model = new Model("test_model");
    Variable x = model.addBinaryVariable("x");

    Model.UpdateTracker tracker = model.addTracker();
    model.deleteVariable(x);

    assertThat(tracker.updateProto())
        .ignoringFieldAbsence()
        .isEqualTo(ModelUpdateProto.newBuilder().addDeletedVariableIds(0).build());
  }

  @Test
  public void updateTrackerExportProto_updateObjective_hasUpdate() {
    var model = new Model("test_model");
    Model.UpdateTracker tracker = model.addTracker();
    model.getObjective().setToMaximize();

    var expectedUpdateProto = ModelUpdateProto.newBuilder();

    expectedUpdateProto.getObjectiveUpdatesBuilder().setDirectionUpdate(true);
    assertThat(tracker.updateProto()).ignoringFieldAbsence().isEqualTo(expectedUpdateProto.build());
  }

  @Test
  public void updateTrackerExportProto_updateObjectiveOldVariable_hasUpdate() {
    var model = new Model("test_model");
    Variable x = model.addBinaryVariable();
    Model.UpdateTracker tracker = model.addTracker();
    model.getObjective().setLinearTerm(x, 2.0);

    var expectedUpdateProto = ModelUpdateProto.newBuilder();
    expectedUpdateProto.getObjectiveUpdatesBuilder()
        .getLinearCoefficientsBuilder()
        .addIds(0)
        .addValues(2.0);
    assertThat(tracker.updateProto()).ignoringFieldAbsence().isEqualTo(expectedUpdateProto.build());
  }

  @Test
  public void updateTrackerExportProto_updateObjectiveNewVariable_hasUpdate() {
    var model = new Model("test_model");
    Model.UpdateTracker tracker = model.addTracker();
    Variable x = model.addBinaryVariable();
    model.getObjective().setLinearTerm(x, 2.0);

    var expectedUpdateProto = ModelUpdateProto.newBuilder();
    expectedUpdateProto.getObjectiveUpdatesBuilder()
        .getLinearCoefficientsBuilder()
        .addIds(0)
        .addValues(2.0);
    expectedUpdateProto.getNewVariablesBuilder()
        .addIds(0)
        .addLowerBounds(0.0)
        .addUpperBounds(1.0)
        .addIntegers(true)
        .addNames("");
    assertThat(tracker.updateProto()).ignoringFieldAbsence().isEqualTo(expectedUpdateProto.build());
  }

  @Test
  public void updateTrackerExportProto_addLinearConstraint_hasNewLinearConstraint() {
    var model = new Model("test_model");

    Model.UpdateTracker tracker = model.addTracker();
    var unused = model.addLinearConstraint("c").setLowerBound(0.0).setUpperBound(1.0);

    var expectedUpdateProto = ModelUpdateProto.newBuilder();
    expectedUpdateProto.getNewLinearConstraintsBuilder()
        .addIds(0)
        .addLowerBounds(0.0)
        .addUpperBounds(1.0)
        .addNames("c");
    assertThat(tracker.updateProto()).ignoringFieldAbsence().isEqualTo(expectedUpdateProto.build());
  }

  @Test
  public void updateTrackerExportProto_modifyLinearConstraint_hasLinearConstraintUpdates() {
    var model = new Model("test_model");
    LinearConstraint c = model.addLinearConstraint("c").setLowerBound(0.0).setUpperBound(1.0);

    Model.UpdateTracker tracker = model.addTracker();
    c.setUpperBound(2.0);

    var expectedUpdateProto = ModelUpdateProto.newBuilder();
    expectedUpdateProto.getLinearConstraintUpdatesBuilder()
        .getUpperBoundsBuilder()
        .addIds(0L)
        .addValues(2.0);
    assertThat(tracker.updateProto()).ignoringFieldAbsence().isEqualTo(expectedUpdateProto.build());
  }

  @Test
  public void updateTrackerExportProto_deleteLinearConstraint_hasLinearConstraintDeletes() {
    var model = new Model("test_model");
    LinearConstraint c = model.addLinearConstraint("c");

    Model.UpdateTracker tracker = model.addTracker();
    model.deleteLinearConstraint(c);

    assertThat(tracker.updateProto())
        .ignoringFieldAbsence()
        .isEqualTo(ModelUpdateProto.newBuilder().addDeletedLinearConstraintIds(0L).build());
  }

  @Test
  public void updateTrackerExportProto_updateLinearConstraintMatrix_hasUpdated() {
    var model = new Model("test_model");
    Variable x = model.addBinaryVariable("x");
    LinearConstraint c = model.addLinearConstraint("c").setLowerBound(0.0).setUpperBound(1.0);

    Model.UpdateTracker tracker = model.addTracker();
    c.setTerm(x, 2.0);

    var expectedUpdateProto = ModelUpdateProto.newBuilder();
    expectedUpdateProto.getLinearConstraintMatrixUpdatesBuilder()
        .addRowIds(0)
        .addColumnIds(0)
        .addCoefficients(2.0);
    assertThat(tracker.updateProto()).ignoringFieldAbsence().isEqualTo(expectedUpdateProto.build());
  }

  @Test
  public void
  updateTrackerExportProto_deleteVariableWithModifiedLinearConstraintCoefficient_coefficientExcluded() {
    var model = new Model("test_model");
    Variable x = model.addBinaryVariable("x");
    LinearConstraint c = model.addLinearConstraint("c").setLowerBound(0.0).setUpperBound(1.0);

    Model.UpdateTracker tracker = model.addTracker();
    c.setTerm(x, 2.0);
    model.deleteVariable(x);

    var expectedUpdateProto = ModelUpdateProto.newBuilder().addDeletedVariableIds(0).build();
    // NOTE: the matrix of updates should be empty.
    assertThat(tracker.updateProto()).ignoringFieldAbsence().isEqualTo(expectedUpdateProto);
  }

  @Test
  public void updateTrackerAdvance_allVariableChangesBeforeAdvance_emptyProtoExport() {
    var model = new Model("test_model");
    Variable x = model.addBinaryVariable("x");
    Variable y = model.addBinaryVariable("y");
    Model.UpdateTracker tracker = model.addTracker();

    var unused = model.addBinaryVariable("z");
    x.setUpperBound(2.0);
    model.deleteVariable(y);
    tracker.advance();

    assertThat(tracker.updateProto())
        .ignoringFieldAbsence()
        .isEqualTo(ModelUpdateProto.getDefaultInstance());
  }

  @Test
  public void updateTrackerAdvance_allLinearConstraintChangesBeforeAdvance_emptyProtoExport() {
    var model = new Model("test_model");
    Variable x = model.addBinaryVariable("x");
    LinearConstraint c = model.addLinearConstraint("c").setLowerBound(0.0).setUpperBound(1.0);
    LinearConstraint d = model.addLinearConstraint("d").setLowerBound(0.0).setUpperBound(1.0);
    Model.UpdateTracker tracker = model.addTracker();

    var unused = model.addLinearConstraint("e");
    c.setLowerBound(-1.0);
    model.deleteLinearConstraint(d);
    c.setTerm(x, 1.0);
    tracker.advance();

    assertThat(tracker.updateProto())
        .ignoringFieldAbsence()
        .isEqualTo(ModelUpdateProto.getDefaultInstance());
  }

  @Test
  public void updateTrackerAdvance_objectiveUpdatesBeforeAdvance_emptyProtoExport() {
    var model = new Model("test_model");
    Model.UpdateTracker tracker = model.addTracker();

    model.getObjective().setOffset(4.0);
    tracker.advance();
    ModelUpdateProto actual = tracker.updateProto();

    assertThat(actual).ignoringFieldAbsence().isEqualTo(ModelUpdateProto.getDefaultInstance());
    // TODO(b/264680603): the line below is not needed once we drop ignoringFieldAbsence() above.
    assertThat(actual.getObjectiveUpdates().hasOffsetUpdate()).isFalse();
  }

  @Test
  public void updateTrackerAdvance_indicatorConstraintChangesBeforeAdvance_emptyProtoExport() {
    var model = new Model("test_model");
    Variable z = model.addBinaryVariable("z");
    Model.UpdateTracker tracker = model.addTracker();

    model.addLeIndicatorConstraint(z, true, linExpr(), 1.0, "c");
    tracker.advance();

    assertThat(tracker.updateProto())
        .ignoringFieldAbsence()
        .isEqualTo(ModelUpdateProto.getDefaultInstance());
  }

  @Test
  public void removeUpdateTracker_deleteIndicatorConstraintAfterRemove_stopsTrackingDeletes() {
    var model = new Model("test_model");
    Variable z = model.addBinaryVariable("z");
    IndicatorConstraint c = model.addLeIndicatorConstraint(z, true, linExpr(), 1.0, "c");
    Model.UpdateTracker tracker = model.addTracker();

    model.removeTracker(tracker);
    model.deleteIndicatorConstraint(c);

    assertThat(tracker.updateProto())
        .ignoringFieldAbsence()
        .isEqualTo(ModelUpdateProto.getDefaultInstance());
  }

  @Test
  public void removeUpdateTracker_changeVariableAfterRemove_stopsTrackingVariableUpdates() {
    var model = new Model("test_model");
    Variable x = model.addBinaryVariable("x");
    Model.UpdateTracker tracker = model.addTracker();
    x.setUpperBound(2.0);

    model.removeTracker(tracker);
    x.setLowerBound(-1.0);

    var expectedUpdateProto = ModelUpdateProto.newBuilder();
    expectedUpdateProto.getVariableUpdatesBuilder().getUpperBoundsBuilder().addIds(0).addValues(
        2.0);
    assertThat(tracker.updateProto()).ignoringFieldAbsence().isEqualTo(expectedUpdateProto.build());
  }

  @Test
  public void
  removeUpdateTracker_changeLinearConstraintAfterRemove_stopsTrackingConstraintUpdates() {
    var model = new Model("test_model");
    LinearConstraint c = model.addLinearConstraint("c").setLowerBound(0.0).setUpperBound(1.0);
    Model.UpdateTracker tracker = model.addTracker();
    c.setUpperBound(2.0);

    model.removeTracker(tracker);
    c.setLowerBound(-1.0);

    var expectedUpdateProto = ModelUpdateProto.newBuilder();
    expectedUpdateProto.getLinearConstraintUpdatesBuilder()
        .getUpperBoundsBuilder()
        .addIds(0)
        .addValues(2.0);
    assertThat(tracker.updateProto()).ignoringFieldAbsence().isEqualTo(expectedUpdateProto.build());
  }

  @Test
  public void removeUpdateTracker_changeObjectiveAfterRemove_stopsTrackingConstraintUpdates() {
    var model = new Model("test_model");
    Model.UpdateTracker tracker = model.addTracker();
    model.getObjective().setOffset(4.0);

    model.removeTracker(tracker);
    model.getObjective().setMaximize(true);
    ModelUpdateProto actual = tracker.updateProto();

    var expectedUpdateProto = ModelUpdateProto.newBuilder();
    expectedUpdateProto.getObjectiveUpdatesBuilder().setOffsetUpdate(4.0);
    assertThat(actual).ignoringFieldAbsence().isEqualTo(expectedUpdateProto.build());
    // TODO(b/264680603): the line below is not needed once we drop ignoringFieldAbsence() above.
    assertThat(actual.getObjectiveUpdates().hasDirectionUpdate()).isFalse();
  }

  @Test
  public void removeUpdateTracker_removeTwice_throws() {
    var model = new Model("test_model");
    Model.UpdateTracker tracker = model.addTracker();
    model.removeTracker(tracker);

    assertThrows(IllegalArgumentException.class, () -> model.removeTracker(tracker));
  }

  // This test is very fragile, try and keep it small/minimize how many of these we have.
  @Test
  public void toCompleteString_simpleMip() {
    var model = new Model("test_model");
    Variable x = model.addBinaryVariable("x");
    Variable y = model.addBinaryVariable("y");
    model.maximize(Expressions.linExpr(2).add(3, x).add(y));
    model.addLe(Expressions.linExpr().add(x).add(y), 1, "c");
    model.addLe(Expressions.linExpr().add(x).add(3, y), 2, "d");

    String expected = Joiner.on('\n').join("model name: \"test_model\"",
        "Variables:", "  x (binary)", "  y (binary)",
        "Objective:", "  Primary Objective: maximize 3.0 * x + y + 2.0 priority: 0",
        "Linear Constraints:", "  c: -Infinity ≤ x + y ≤ 1.0",
        "  d: -Infinity ≤ x + 3.0 * y ≤ 2.0");
    assertThat(model.toCompleteString()).isEqualTo(expected);
  }

  // This test is very fragile, try and keep it small/minimize how many of these we have.
  @Test
  public void toCompleteString_multipleObjectives() {
    var model = new Model("test_model", "obj1");
    Variable x = model.addBinaryVariable("x");
    model.maximize(x);
    AuxiliaryObjective obj2 = model.addAuxiliaryObjective(3, "obj2");
    obj2.setLinearTerm(x, 3);
    obj2.setToMaximize();

    String expected =
        Joiner.on('\n').join("model name: \"test_model\"", "Variables:", "  x (binary)",
            "Objective:", "  obj1 (Primary Objective): maximize x priority: 0",
            "Auxiliary Objectives:", "  obj2 (Objective 0): maximize 3.0 * x priority: 3");
    assertThat(model.toCompleteString()).isEqualTo(expected);
  }

  @Test
  public void addLeIndicatorConstraint_attributesOk() {
    var model = new Model("test_model");
    Variable z = model.addBinaryVariable("z");
    Variable x = model.addVariable("x");

    IndicatorConstraint c = model.addLeIndicatorConstraint(z, true, linExpr(x).add(3.0), 2.0, "c");

    assertThat(model.getNextIndicatorConstraintId()).isEqualTo(1);
    assertThat(c.getIndicatorVariable()).isEqualTo(z);
    assertThat(c.getActivateOnZero()).isTrue();
    assertThat(c.getLowerBound()).isNegativeInfinity();
    // 2.0 - 3.0 = -1.0
    assertThat(c.getUpperBound()).isEqualTo(-1.0);
    assertThat(c.getUnmodifiableImpliedTerms()).containsExactly(x, 1.0);
    assertThat(c.getName()).isEqualTo("c");
  }

  @Test
  public void addGeIndicatorConstraint_attributesOk() {
    var model = new Model("test_model");
    Variable z = model.addBinaryVariable("z");
    Variable x = model.addVariable("x");

    IndicatorConstraint c = model.addGeIndicatorConstraint(z, true, linExpr(x).add(3.0), 2.0, "c");

    assertThat(model.getNextIndicatorConstraintId()).isEqualTo(1);
    assertThat(c.getIndicatorVariable()).isEqualTo(z);
    assertThat(c.getActivateOnZero()).isTrue();
    // 2.0 - 3.0 = -1.0
    assertThat(c.getLowerBound()).isEqualTo(-1.0);
    assertThat(c.getUpperBound()).isPositiveInfinity();
    assertThat(c.getUnmodifiableImpliedTerms()).containsExactly(x, 1.0);
    assertThat(c.getName()).isEqualTo("c");
  }

  @Test
  public void addEqIndicatorConstraint_attributesOk() {
    var model = new Model("test_model");
    Variable z = model.addBinaryVariable("z");
    Variable x = model.addVariable("x");

    IndicatorConstraint c = model.addEqIndicatorConstraint(z, true, linExpr(x).add(3.0), 2.0, "c");

    assertThat(model.getNextIndicatorConstraintId()).isEqualTo(1);
    assertThat(c.getIndicatorVariable()).isEqualTo(z);
    assertThat(c.getActivateOnZero()).isTrue();
    // 2.0 - 3.0 = -1.0
    assertThat(c.getLowerBound()).isEqualTo(-1.0);
    assertThat(c.getUpperBound()).isEqualTo(-1.0);
    assertThat(c.getUnmodifiableImpliedTerms()).containsExactly(x, 1.0);
    assertThat(c.getName()).isEqualTo("c");
  }

  @Test
  public void deleteIndicatorConstraint_isDeleted() {
    var model = new Model("test_model");
    Variable z = model.addBinaryVariable("z");
    IndicatorConstraint c = model.addLeIndicatorConstraint(z, true, linExpr(), 1.0, "c");

    assertThat(model.deleteIndicatorConstraint(c)).isTrue();

    assertThat(c.isDeleted()).isTrue();
    assertThat(model.numIndicatorConstraints()).isEqualTo(0);
  }

  @Test
  public void toProto_withIndicatorConstraints_hasConstraints() {
    var model = new Model("test_model");
    Variable z = model.addBinaryVariable("z");
    Variable x = model.addVariable("x");
    model.addLeIndicatorConstraint(z, true, linExpr(x), 2.0, "c1");
    model.addGeIndicatorConstraint(z, true, linExpr(x), 1.0, "c2");
    model.addEqIndicatorConstraint(z, false, linExpr(x), 1.5, "c3");

    ModelProto proto = model.toProto();

    assertThat(proto.getIndicatorConstraintsMap())
        .containsExactly(0L,
            IndicatorConstraintProto.newBuilder()
                .setIndicatorId(0)
                .setActivateOnZero(true)
                .setLowerBound(Double.NEGATIVE_INFINITY)
                .setUpperBound(2.0)
                .setName("c1")
                .setExpression(
                    SparseDoubleVectorProto.newBuilder().addIds(1).addValues(1.0).build())
                .build(),
            1L,
            IndicatorConstraintProto.newBuilder()
                .setIndicatorId(0)
                .setActivateOnZero(true)
                .setLowerBound(1.0)
                .setUpperBound(Double.POSITIVE_INFINITY)
                .setName("c2")
                .setExpression(
                    SparseDoubleVectorProto.newBuilder().addIds(1).addValues(1.0).build())
                .build(),
            2L,
            IndicatorConstraintProto.newBuilder()
                .setIndicatorId(0)
                .setActivateOnZero(false)
                .setLowerBound(1.5)
                .setUpperBound(1.5)
                .setName("c3")
                .setExpression(
                    SparseDoubleVectorProto.newBuilder().addIds(1).addValues(1.0).build())
                .build());
  }

  @Test
  public void fromProto_withIndicatorConstraints_hasConstraints() {
    var proto =
        ModelProto.newBuilder()
            .setVariables(VariablesProto.newBuilder()
                    .addIds(0)
                    .addIds(1)
                    .addNames("z")
                    .addNames("x")
                    .addIntegers(true)
                    .addIntegers(false)
                    .addLowerBounds(0.0)
                    .addLowerBounds(0.0)
                    .addUpperBounds(1.0)
                    .addUpperBounds(1.0))
            .putIndicatorConstraints(0,
                IndicatorConstraintProto.newBuilder()
                    .setIndicatorId(0)
                    .setActivateOnZero(true)
                    .setLowerBound(1.0)
                    .setUpperBound(2.0)
                    .setName("c")
                    .setExpression(
                        SparseDoubleVectorProto.newBuilder().addIds(1).addValues(1.0).build())
                    .build())
            .build();

    Model model = Model.fromProto(proto);

    assertThat(model.numIndicatorConstraints()).isEqualTo(1);
    IndicatorConstraint c = model.getIndicatorConstraint(0);
    assertThat(c.getName()).isEqualTo("c");
    assertThat(c.getIndicatorVariable().getName()).isEqualTo("z");
    assertThat(c.getActivateOnZero()).isTrue();
    assertThat(c.getLowerBound()).isEqualTo(1.0);
    assertThat(c.getUpperBound()).isEqualTo(2.0);
    assertThat(c.getUnmodifiableImpliedTerms()).containsExactly(model.getVariable(1), 1.0);
  }

  @Test
  public void fromProto_indicatorConstraintWithDeletedIndicator_isCreatedWithNullIndicator() {
    var proto =
        ModelProto.newBuilder()
            .setVariables(VariablesProto.newBuilder()
                    .addIds(0)
                    .addNames("x")
                    .addIntegers(true)
                    .addLowerBounds(0.0)
                    .addUpperBounds(1.0))
            .putIndicatorConstraints(0,
                IndicatorConstraintProto.newBuilder()
                    .setIndicatorId(1) // Variable 1 does not exist
                    .setActivateOnZero(true)
                    .setLowerBound(1.0)
                    .setUpperBound(2.0)
                    .setName("c")
                    .setExpression(
                        SparseDoubleVectorProto.newBuilder().addIds(0).addValues(1.0).build())
                    .build())
            .build();

    Model model = Model.fromProto(proto);

    assertThat(model.numIndicatorConstraints()).isEqualTo(1);
    IndicatorConstraint c = model.getIndicatorConstraint(0);
    assertThat(c.getIndicatorVariable()).isNull();
    assertThat(c.getUnmodifiableImpliedTerms()).containsExactly(model.getVariable(0), 1.0);
  }

  @Test
  public void updateTrackerExportProto_addIndicatorConstraint_hasNew() {
    var model = new Model("test_model");
    Variable z = model.addBinaryVariable("z");
    Model.UpdateTracker tracker = model.addTracker();

    IndicatorConstraint c = model.addLeIndicatorConstraint(z, true, linExpr(), 1.0, "c");

    assertThat(tracker.updateProto().getIndicatorConstraintUpdates().getNewConstraintsMap())
        .containsExactly(0L, c.toProto());
  }

  @Test
  public void updateTrackerExportProto_deleteIndicatorConstraint_hasDeleted() {
    var model = new Model("test_model");
    Variable z = model.addBinaryVariable("z");
    IndicatorConstraint c = model.addLeIndicatorConstraint(z, true, linExpr(), 1.0, "c");
    Model.UpdateTracker tracker = model.addTracker();

    model.deleteIndicatorConstraint(c);

    assertThat(tracker.updateProto().getIndicatorConstraintUpdates().getDeletedConstraintIdsList())
        .containsExactly(0L);
  }

  @Test
  public void deleteIndicatorConstraint_notOwned_throws() {
    var model1 = new Model("modelA");
    var z1 = model1.addBinaryVariable("z");
    var c1 = model1.addLeIndicatorConstraint(z1, true, linExpr(), 1.0, "c");
    var model2 = new Model("modelB");

    assertThrows(IllegalArgumentException.class, () -> model2.deleteIndicatorConstraint(c1));
  }

  @Test
  public void deleteIndicatorConstraint_deleteTwice_returnsFalse() {
    var model = new Model("model");
    var z = model.addBinaryVariable("z");
    var c = model.addLeIndicatorConstraint(z, true, linExpr(), 1.0, "c");

    assertThat(model.deleteIndicatorConstraint(c)).isTrue();
    assertThat(model.deleteIndicatorConstraint(c)).isFalse();
  }

  @Test
  public void checkOwned_indicatorConstraint_correctModel_isOk() {
    var model = new Model("model");
    var z = model.addBinaryVariable("z");
    var c = model.addLeIndicatorConstraint(z, true, linExpr(), 1.0, "c");

    model.checkOwned(c);
  }

  @Test
  public void checkOwned_indicatorConstraint_wrongModel_throws() {
    var model1 = new Model("modelA");
    var z1 = model1.addBinaryVariable("z");
    var c1 = model1.addLeIndicatorConstraint(z1, true, linExpr(), 1.0, "c1");
    var model2 = new Model("modelB");

    assertThrows(IllegalArgumentException.class, () -> model2.checkOwned(c1));
  }

  @Test
  public void getUnmodifiableIndicatorConstraints_returnsAllConstraints() {
    var model = new Model("model");
    var z = model.addBinaryVariable("z");
    var c1 = model.addLeIndicatorConstraint(z, true, linExpr(), 1.0, "c1");
    var c2 = model.addLeIndicatorConstraint(z, false, linExpr(), 1.0, "c2");

    assertThat(model.getUnmodifiableIndicatorConstraints()).containsExactly(c1, c2);
  }

  @Test
  public void fromProto_preservesIdsForNewConstraints_andDoesNotDependOnMapOrder() {
    var proto =
        ModelProto.newBuilder()
            .setVariables(VariablesProto.newBuilder()
                    .addIds(0)
                    .addNames("z")
                    .addIntegers(true)
                    .addLowerBounds(0)
                    .addUpperBounds(1))
            .putIndicatorConstraints(200,
                IndicatorConstraintProto.newBuilder().setIndicatorId(0).setName("c200").build())
            .putIndicatorConstraints(100,
                IndicatorConstraintProto.newBuilder().setIndicatorId(0).setName("c100").build())
            .build();

    var model = Model.fromProto(proto);
    var z = model.getVariable(0);
    var cNew = model.addLeIndicatorConstraint(z, true, linExpr(), 1, "cNew");

    assertThat(cNew.getId()).isGreaterThan(200L);
    assertThat(model.getIndicatorConstraint(100)).isNotNull();
    assertThat(model.getIndicatorConstraint(200)).isNotNull();
  }

  @Test
  public void addIndicatorConstraint_indicatorFromOtherModel_throws() {
    var model1 = new Model("model1");
    var z1 = model1.addBinaryVariable("z1");
    var model2 = new Model("model2");
    var x2 = model2.addVariable("x2");
    var leftHandSide = linExpr(x2);

    assertThrows(IllegalArgumentException.class,
        () -> model2.addLeIndicatorConstraint(z1, true, leftHandSide, 1.0, "c"));
  }

  @Test
  public void addIndicatorConstraint_indicatorDeleted_throws() {
    var model = new Model("model");
    var z = model.addBinaryVariable("z");
    model.deleteVariable(z);
    var x = model.addVariable("x");
    var leftHandSide = linExpr(x);

    assertThrows(IllegalArgumentException.class,
        () -> model.addLeIndicatorConstraint(z, true, leftHandSide, 1.0, "c"));
  }

  @Test
  public void addIndicatorConstraint_variableInExpressionFromOtherModel_throws() {
    var model1 = new Model("model1");
    var x1 = model1.addVariable("x1");
    var model2 = new Model("model2");
    var z2 = model2.addBinaryVariable("z2");
    var leftHandSide = linExpr(x1);

    assertThrows(IllegalArgumentException.class,
        () -> model2.addLeIndicatorConstraint(z2, true, leftHandSide, 1.0, "c"));
  }

  @Test
  public void addIndicatorConstraint_variableInExpressionDeleted_throws() {
    var model = new Model("model");
    var z = model.addBinaryVariable("z");
    var x = model.addVariable("x");
    model.deleteVariable(x);
    var leftHandSide = linExpr(x);

    assertThrows(IllegalArgumentException.class,
        () -> model.addLeIndicatorConstraint(z, true, leftHandSide, 1.0, "c"));
  }

  @Test
  public void deleteVariable_inIndicatorConstraint_constraintUpdated() {
    var model = new Model("test_model");
    Variable z = model.addBinaryVariable("z");
    Variable x = model.addVariable("x");
    IndicatorConstraint c = model.addLeIndicatorConstraint(z, true, linExpr(x), 1.0, "c");

    model.deleteVariable(x);

    assertThat(c.getUnmodifiableImpliedTerms()).isEmpty();
  }

  @Test
  public void deleteIndicatorVariable_inIndicatorConstraint_constraintUpdated() {
    var model = new Model("test_model");
    Variable z = model.addBinaryVariable("z");
    Variable x = model.addVariable("x");
    IndicatorConstraint c = model.addLeIndicatorConstraint(z, true, linExpr(x), 1.0, "c");

    model.deleteVariable(z);

    assertThat(c.getIndicatorVariable()).isNull();
  }

  @Test
  public void toString_returnsName() {
    var model = new Model("my_model");
    assertThat(model.toString()).isEqualTo("my_model");
  }

  @Test
  public void hasIndicatorConstraintWithId_works() {
    var model = new Model();
    var z = model.addVariable(0.0, 1.0, true, "z");
    var c = model.addLeIndicatorConstraint(z, true, linExpr(), 1.0, "c");
    assertThat(model.hasIndicatorConstraintWithId(c.getId())).isTrue();
    assertThat(model.hasIndicatorConstraintWithId(c.getId() + 1)).isFalse();

    model.deleteIndicatorConstraint(c);
    assertThat(model.hasIndicatorConstraintWithId(c.getId())).isFalse();
  }

  @Test
  public void fromProto_indicatorConstraintWithUnknownVariableInExpression_throws() {
    var proto =
        ModelProto.newBuilder()
            .setVariables(VariablesProto.newBuilder()
                    .addIds(0)
                    .addNames("z")
                    .addIntegers(true)
                    .addLowerBounds(0.0)
                    .addUpperBounds(1.0))
            .putIndicatorConstraints(0,
                IndicatorConstraintProto.newBuilder()
                    .setIndicatorId(0)
                    .setActivateOnZero(true)
                    .setLowerBound(1.0)
                    .setUpperBound(2.0)
                    .setName("c")
                    .setExpression(
                        SparseDoubleVectorProto.newBuilder().addIds(1).addValues(1.0).build())
                    .build())
            .build();
    var e = assertThrows(IllegalArgumentException.class, () -> Model.fromProto(proto));
    assertThat(e).hasMessageThat().contains("unknown variable id 1");
  }

  @Test
  public void deleteIndicatorConstraint_preservesOtherConstraintsOnImpliedTerms() {
    var model = new Model("test_model");
    Variable z1 = model.addBinaryVariable("z1");
    Variable z2 = model.addBinaryVariable("z2");
    Variable x = model.addVariable("x");
    IndicatorConstraint c1 = model.addLeIndicatorConstraint(z1, true, linExpr(x), 1.0, "c1");
    IndicatorConstraint c2 = model.addLeIndicatorConstraint(z2, false, linExpr(x), 2.0, "c2");

    // Delete c1, which should remove c1 from x's constraint set but NOT remove x from
    // variableToConstraints.
    model.deleteIndicatorConstraint(c1);

    // Delete x. This should notify c2 that x was deleted.
    model.deleteVariable(x);

    // If the mutation removed x from variableToConstraints entirely, c2 will not be notified
    // and its expression would incorrectly still contain x.
    assertThat(c2.getUnmodifiableImpliedTerms()).isEmpty();
  }

  @Test
  public void deleteIndicatorConstraint_preservesOtherConstraintsOnVariable() {
    var model = new Model("test_model");
    Variable z = model.addBinaryVariable("z");
    Variable x = model.addVariable("x");
    IndicatorConstraint c1 = model.addLeIndicatorConstraint(z, true, linExpr(x), 1.0, "c1");
    IndicatorConstraint c2 = model.addLeIndicatorConstraint(z, false, linExpr(x), 2.0, "c2");

    // Delete c1, which should remove c1 from z's constraint set but NOT remove z from
    // variableToConstraints.
    model.deleteIndicatorConstraint(c1);

    // Delete z. This should notify c2 that z was deleted.
    model.deleteVariable(z);

    // If the mutation removed z from variableToConstraints entirely, c2 will not be notified
    // and its indicator variable would incorrectly still be z instead of null.
    assertThat(c2.getIndicatorVariable()).isNull();
  }
}
