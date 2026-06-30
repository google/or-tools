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

import com.google.ortools.mathopt.ModelUpdateProto;
import com.google.ortools.mathopt.Variables.Diff;
import com.google.ortools.mathopt.VariablesProto;
import org.junit.jupiter.api.Test;

public final class VariablesTest {
  @Test
  public void variablesGetters_okOnEmptyVariables() {
    Variables variables = new Variables(new ModelId(""));

    assertThat(variables.getNextId()).isEqualTo(0L);
    assertThat(variables.numVariables()).isEqualTo(0);
    assertThat(variables.hasVariableWithId(0L)).isFalse();
    assertThat(variables.getVariable(0L)).isNull();
    assertThat(variables.toProto()).isEqualToDefaultInstance();
    assertThat(variables.sortedVariables()).isEmpty();
  }

  @Test
  public void ownsVariable_ownedOk() {
    Variables vars1 = new Variables(new ModelId("m1"));
    Variable x1 = vars1.addVariable("x1");
    vars1.checkOwned(x1); // Ensure doesn't throw an exception.
  }

  @Test
  public void ownsVariable_unownedThrowsException() {
    Variables vars1 = new Variables(new ModelId("m1"));
    Variable x1 = vars1.addVariable("x1");
    Variables vars2 = new Variables(new ModelId("m2"));
    Variable x2 = vars2.addVariable("x2");

    assertThat(assertThrows(IllegalArgumentException.class, () -> vars1.checkOwned(x2)))
        .hasMessageThat()
        .contains("another model");
    assertThat(assertThrows(IllegalArgumentException.class, () -> vars2.checkOwned(x1)))
        .hasMessageThat()
        .contains("another model");
  }

  @Test
  public void getVariablesFromId_denseVariables_hasVars() {
    Variables variables = new Variables(new ModelId(""));
    Variable x = variables.addVariable("x");
    Variable y = variables.addVariable("y");

    assertThat(variables.getVariablesFromId(-1)).containsExactly(x, y);
    assertThat(variables.getVariablesFromId(0)).containsExactly(x, y);
    assertThat(variables.getVariablesFromId(1)).containsExactly(y);
    assertThat(variables.getVariablesFromId(2)).isEmpty();
    assertThat(variables.getVariablesFromId(3)).isEmpty();
  }

  @Test
  public void getVariablesFromId_sparseVariables_hasVars() {
    Variables variables = new Variables(new ModelId(""));
    Variable w = variables.addVariable("w");
    Variable x = variables.addVariable("x");
    Variable y = variables.addVariable("y");
    Variable z = variables.addVariable("z");
    variables.deleteVariable(x);
    variables.deleteVariable(y);

    assertThat(variables.getVariablesFromId(-1)).containsExactly(w, z);
    assertThat(variables.getVariablesFromId(0)).containsExactly(w, z);
    assertThat(variables.getVariablesFromId(1)).containsExactly(z);
    assertThat(variables.getVariablesFromId(2)).containsExactly(z);
    assertThat(variables.getVariablesFromId(3)).containsExactly(z);
    assertThat(variables.getVariablesFromId(4)).isEmpty();
    assertThat(variables.getVariablesFromId(5)).isEmpty();
  }

  @Test
  public void sortedVariables_sparseVariables_hasVars() {
    Variables variables = new Variables(new ModelId(""));
    Variable w = variables.addVariable("w");
    Variable x = variables.addVariable("x");
    Variable y = variables.addVariable("y");
    Variable z = variables.addVariable("z");
    variables.deleteVariable(x);
    variables.deleteVariable(y);

    assertThat(variables.sortedVariables()).containsExactly(w, z);
  }

  @Test
  public void ensureNextIdAtleast_skipIds_nextIdCorrect() {
    var variables = new Variables(new ModelId(""));

    variables.ensureNextIdAtLeast(3);

    assertThat(variables.getNextId()).isEqualTo(3);
    assertThat(variables.addVariable("x").getId()).isEqualTo(3);
  }

  @Test
  public void variablesToProto_createsSimpleNonemptyProto() {
    Variables variables = new Variables(new ModelId(""));
    variables.addVariable("w").setLowerBound(0.0).setUpperBound(1.0);
    Variable x = variables.addVariable("x").setLowerBound(2.0).setUpperBound(3.0);
    variables.addVariable("").setLowerBound(4.0).setUpperBound(5.0);
    variables.addVariable("z").setInteger(true);
    variables.deleteVariable(x);

    VariablesProto.Builder expected = VariablesProto.newBuilder();
    expected.addIds(0).addLowerBounds(0.0).addUpperBounds(1.0).addIntegers(false).addNames("w");
    expected.addIds(2).addLowerBounds(4.0).addUpperBounds(5.0).addIntegers(false).addNames("");
    expected.addIds(3)
        .addLowerBounds(Double.NEGATIVE_INFINITY)
        .addUpperBounds(Double.POSITIVE_INFINITY)
        .addIntegers(true)
        .addNames("z");
    assertThat(variables.toProto()).isEqualTo(expected.build());
  }

  @Test
  public void diffUpdate_diffHasNewVariable() {
    Variables variables = new Variables(new ModelId(""));
    Diff diff = variables.addDiff();
    var unused =
        variables.addVariable(/*lowerBound=*/-1.0, /*upperBound=*/3.5, /*isInteger=*/false, "x");

    ModelUpdateProto.Builder expectedUpdates = ModelUpdateProto.newBuilder();
    expectedUpdates.getNewVariablesBuilder()
        .addIds(0)
        .addLowerBounds(-1.0)
        .addUpperBounds(3.5)
        .addIntegers(false)
        .addNames("x");
    assertThat(fullModelUpdate(diff)).ignoringFieldAbsence().isEqualTo(expectedUpdates.build());
  }

  @Test
  public void diffUpdate_setLowerBoundBeforeCheckpoint() {
    Variables variables = new Variables(new ModelId(""));
    Variable x =
        variables.addVariable(/*lowerBound=*/-1.0, /*upperBound=*/3.5, /*isInteger=*/false, "x");
    Diff diff = variables.addDiff();
    x.setLowerBound(-2.0);

    ModelUpdateProto.Builder expectedUpdates = ModelUpdateProto.newBuilder();
    expectedUpdates.getVariableUpdatesBuilder().getLowerBoundsBuilder().addIds(0).addValues(-2.0);
    assertThat(fullModelUpdate(diff)).ignoringFieldAbsence().isEqualTo(expectedUpdates.build());
  }

  @Test
  public void diffUpdate_setLowerBoundBeforeCheckpointUnchanged() {
    Variables variables = new Variables(new ModelId(""));
    Variable x =
        variables.addVariable(/*lowerBound=*/-1.0, /*upperBound=*/3.5, /*isInteger=*/false, "x");
    Diff diff = variables.addDiff();
    x.setLowerBound(-1.0);

    assertThat(fullModelUpdate(diff))
        .ignoringFieldAbsence()
        .isEqualTo(ModelUpdateProto.getDefaultInstance());
  }

  @Test
  public void diffUpdate_setLowerBoundAfterCheckpoint() {
    Variables variables = new Variables(new ModelId(""));
    Diff diff = variables.addDiff();
    Variable x =
        variables.addVariable(/*lowerBound=*/-1.0, /*upperBound=*/3.5, /*isInteger=*/false, "x");
    x.setLowerBound(-2.0);

    ModelUpdateProto.Builder expectedUpdates = ModelUpdateProto.newBuilder();
    expectedUpdates.getNewVariablesBuilder()
        .addIds(0)
        .addLowerBounds(-2.0)
        .addUpperBounds(3.5)
        .addIntegers(false)
        .addNames("x");
    assertThat(fullModelUpdate(diff)).ignoringFieldAbsence().isEqualTo(expectedUpdates.build());
  }

  @Test
  public void diffUpdate_setUpperBoundBeforeCheckpoint() {
    Variables variables = new Variables(new ModelId(""));
    Variable x =
        variables.addVariable(/*lowerBound=*/-1.0, /*upperBound=*/3.5, /*isInteger=*/false, "x");
    Diff diff = variables.addDiff();
    x.setUpperBound(2.0);

    ModelUpdateProto.Builder expectedUpdates = ModelUpdateProto.newBuilder();
    expectedUpdates.getVariableUpdatesBuilder().getUpperBoundsBuilder().addIds(0).addValues(2.0);
    assertThat(fullModelUpdate(diff)).ignoringFieldAbsence().isEqualTo(expectedUpdates.build());
  }

  @Test
  public void diffUpdate_setUpperBoundBeforeCheckpointUnchanged() {
    Variables variables = new Variables(new ModelId(""));
    Variable x =
        variables.addVariable(/*lowerBound=*/-1.0, /*upperBound=*/3.5, /*isInteger=*/false, "x");
    Diff diff = variables.addDiff();
    x.setUpperBound(3.5);

    assertThat(fullModelUpdate(diff))
        .ignoringFieldAbsence()
        .isEqualTo(ModelUpdateProto.getDefaultInstance());
  }

  @Test
  public void diffUpdate_setUpperBoundAfterCheckpoint() {
    Variables variables = new Variables(new ModelId(""));
    Diff diff = variables.addDiff();
    Variable x =
        variables.addVariable(/*lowerBound=*/-1.0, /*upperBound=*/3.5, /*isInteger=*/false, "x");
    x.setUpperBound(2.0);

    ModelUpdateProto.Builder expectedUpdates = ModelUpdateProto.newBuilder();
    expectedUpdates.getNewVariablesBuilder()
        .addIds(0)
        .addLowerBounds(-1.0)
        .addUpperBounds(2.0)
        .addIntegers(false)
        .addNames("x");
    assertThat(fullModelUpdate(diff)).ignoringFieldAbsence().isEqualTo(expectedUpdates.build());
  }

  @Test
  public void diffUpdate_setIntegerBeforeCheckpoint() {
    Variables variables = new Variables(new ModelId(""));
    Variable x =
        variables.addVariable(/*lowerBound=*/-1.0, /*upperBound=*/3.5, /*isInteger=*/false, "x");
    Diff diff = variables.addDiff();
    x.setInteger(true);

    ModelUpdateProto.Builder expectedUpdates = ModelUpdateProto.newBuilder();
    expectedUpdates.getVariableUpdatesBuilder().getIntegersBuilder().addIds(0).addValues(true);
    assertThat(fullModelUpdate(diff)).ignoringFieldAbsence().isEqualTo(expectedUpdates.build());
  }

  @Test
  public void diffUpdate_setIntegerBeforeCheckpointUnchanged() {
    Variables variables = new Variables(new ModelId(""));
    Variable x =
        variables.addVariable(/*lowerBound=*/-1.0, /*upperBound=*/3.5, /*isInteger=*/false, "x");
    Diff diff = variables.addDiff();
    x.setInteger(false);

    assertThat(fullModelUpdate(diff))
        .ignoringFieldAbsence()
        .isEqualTo(ModelUpdateProto.getDefaultInstance());
  }

  @Test
  public void diffUpdate_setIntegerAfterCheckpoint() {
    Variables variables = new Variables(new ModelId(""));
    Diff diff = variables.addDiff();
    Variable x =
        variables.addVariable(/*lowerBound=*/-1.0, /*upperBound=*/3.5, /*isInteger=*/false, "x");
    x.setInteger(true);

    ModelUpdateProto.Builder expectedUpdates = ModelUpdateProto.newBuilder();
    expectedUpdates.getNewVariablesBuilder()
        .addIds(0)
        .addLowerBounds(-1.0)
        .addUpperBounds(3.5)
        .addIntegers(true)
        .addNames("x");
    assertThat(fullModelUpdate(diff)).ignoringFieldAbsence().isEqualTo(expectedUpdates.build());
  }

  @Test
  public void diffNewVariables_noneNew_isEmpty() {
    Variables variables = new Variables(new ModelId(""));
    var unused = variables.addVariable("x");
    Diff diff = variables.addDiff();

    assertThat(diff.newVariables()).isEmpty();
  }

  @Test
  public void diffNewVariables_newVarsPresent() {
    Variables variables = new Variables(new ModelId(""));
    var unused = variables.addVariable("w");
    Diff diff = variables.addDiff();
    Variable x = variables.addVariable("x");
    Variable y = variables.addVariable("y");

    assertThat(diff.newVariables()).containsExactly(x, y);
  }

  @Test
  public void deleteVariable_updatesVariablesState() {
    Variables variables = new Variables(new ModelId(""));
    Variable x = variables.addVariable("x").setLowerBound(-1.0).setUpperBound(3.5);
    assertThat(variables.numVariables()).isEqualTo(1);
    assertThat(variables.getNextId()).isEqualTo(1);
    assertThat(variables.hasVariableWithId(0)).isTrue();
    assertThat(variables.getVariable(0)).isEqualTo(x);
    assertThat(x.isDeleted()).isFalse();

    variables.deleteVariable(x);

    assertThat(variables.numVariables()).isEqualTo(0);
    assertThat(variables.getNextId()).isEqualTo(1);
    assertThat(variables.hasVariableWithId(0)).isFalse();
    assertThat(variables.getVariable(0)).isNull();
    assertThat(x.isDeleted()).isTrue();
  }

  @Test
  public void deleteVariable_setLbAfter_throws() {
    Variables variables = new Variables(new ModelId(""));
    Variable x = variables.addVariable("x");

    variables.deleteVariable(x);

    assertThrows(IllegalArgumentException.class, () -> x.setLowerBound(-2.0));
  }

  @Test
  public void deleteVariable_setUbAfter_throws() {
    Variables variables = new Variables(new ModelId(""));
    Variable x = variables.addVariable("x");

    variables.deleteVariable(x);

    assertThrows(IllegalArgumentException.class, () -> x.setUpperBound(4.0));
  }

  @Test
  public void deleteVariable_setIntegerAfter_throws() {
    Variables variables = new Variables(new ModelId(""));
    Variable x = variables.addVariable("x");

    variables.deleteVariable(x);

    assertThrows(IllegalArgumentException.class, () -> x.setInteger(true));
  }

  @Test
  public void deleteVariable_twiceOnVariable_returnsFalse() {
    Variables variables = new Variables(new ModelId(""));
    Variable x = variables.addVariable("x");

    assertThat(variables.deleteVariable(x)).isTrue();
    assertThat(variables.deleteVariable(x)).isFalse();
  }

  @Test
  public void deleteVariable_afterCheckpoint_deleteExcludedFromUpdate() {
    Variables variables = new Variables(new ModelId(""));
    Diff diff = variables.addDiff();
    Variable x = variables.addVariable("x");

    variables.deleteVariable(x);

    assertThat(diff.getDeleted()).isEmpty();
    assertThat(fullModelUpdate(diff))
        .ignoringFieldAbsence()
        .isEqualTo(ModelUpdateProto.getDefaultInstance());
  }

  @Test
  public void deleteVariable_beforeCheckpoint_deleteInUpdate() {
    Variables variables = new Variables(new ModelId(""));
    Variable x = variables.addVariable("x");
    Diff diff = variables.addDiff();

    variables.deleteVariable(x);

    assertThat(diff.getDeleted()).containsExactly(x.getId());
    ModelUpdateProto.Builder expected = ModelUpdateProto.newBuilder();
    expected.addDeletedVariableIds(0);
    assertThat(fullModelUpdate(diff)).ignoringFieldAbsence().isEqualTo(expected.build());
  }

  @Test
  public void deleteVariable_beforeCheckpoint_otherUpdatesCleared() {
    Variables variables = new Variables(new ModelId(""));
    Variable x = variables.addVariable("x").setLowerBound(-1.0).setUpperBound(2.5);
    Diff diff = variables.addDiff();
    x.setLowerBound(-2.0);
    x.setUpperBound(3.0);
    x.setInteger(true);

    ModelUpdateProto.Builder expectedPreDelete = ModelUpdateProto.newBuilder();
    expectedPreDelete.getVariableUpdatesBuilder().getLowerBoundsBuilder().addIds(0).addValues(-2.0);
    expectedPreDelete.getVariableUpdatesBuilder().getUpperBoundsBuilder().addIds(0).addValues(3.0);
    expectedPreDelete.getVariableUpdatesBuilder().getIntegersBuilder().addIds(0).addValues(true);
    assertThat(fullModelUpdate(diff)).ignoringFieldAbsence().isEqualTo(expectedPreDelete.build());

    variables.deleteVariable(x);

    ModelUpdateProto.Builder expected = ModelUpdateProto.newBuilder();
    expected.addDeletedVariableIds(0);
    assertThat(fullModelUpdate(diff)).ignoringFieldAbsence().isEqualTo(expected.build());
  }

  @Test
  public void advance_clearsUpdate() {
    Variables variables = new Variables(new ModelId(""));
    Variable x = variables.addVariable("x").setLowerBound(0.0).setUpperBound(1.0);
    Variable y = variables.addVariable("y").setLowerBound(2.0).setUpperBound(3.0);
    Diff diff = variables.addDiff();
    var unused =
        variables.addVariable(/*lowerBound=*/4.0, /*upperBound=*/5.0, /*isInteger=*/false, "z");
    variables.deleteVariable(x);
    y.setInteger(true);
    y.setLowerBound(6.0);
    y.setUpperBound(7.0);

    ModelUpdateProto.Builder expectedPreAdvance = ModelUpdateProto.newBuilder();
    expectedPreAdvance.addDeletedVariableIds(0);
    expectedPreAdvance.getVariableUpdatesBuilder().getLowerBoundsBuilder().addIds(1).addValues(6.0);
    expectedPreAdvance.getVariableUpdatesBuilder().getUpperBoundsBuilder().addIds(1).addValues(7.0);
    expectedPreAdvance.getVariableUpdatesBuilder().getIntegersBuilder().addIds(1).addValues(true);
    expectedPreAdvance.getNewVariablesBuilder()
        .addIds(2)
        .addLowerBounds(4.0)
        .addUpperBounds(5.0)
        .addIntegers(false)
        .addNames("z");
    assertThat(fullModelUpdate(diff)).ignoringFieldAbsence().isEqualTo(expectedPreAdvance.build());

    diff.advance();

    assertThat(fullModelUpdate(diff))
        .ignoringFieldAbsence()
        .isEqualTo(ModelUpdateProto.getDefaultInstance());
  }

  @Test
  public void removeDiff_updateAfter_noLongerGetsUpdates() {
    var variables = new Variables(new ModelId(""));
    Variable x = variables.addVariable("x");
    Diff diff1 = variables.addDiff();
    Diff diff2 = variables.addDiff();

    variables.removeDiff(diff1);

    variables.deleteVariable(x);
    assertThat(diff1.getDeleted()).isEmpty();
    assertThat(diff2.getDeleted()).containsExactly(0L);
  }

  @Test
  public void removeDiff_sameDiffTwice_throws() {
    var variables = new Variables(new ModelId(""));
    Diff diff = variables.addDiff();
    variables.removeDiff(diff);
    assertThat(assertThrows(IllegalArgumentException.class, () -> variables.removeDiff(diff)))
        .hasMessageThat()
        .contains("Diff not present");
  }

  private static ModelUpdateProto fullModelUpdate(Diff diff) {
    ModelUpdateProto.Builder actual = ModelUpdateProto.newBuilder();
    diff.update().setInProto(actual);
    return actual.build();
  }
}
