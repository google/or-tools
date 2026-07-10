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
import com.google.common.collect.ImmutableMap;
import com.google.ortools.mathopt.Objectives.Diff;
import com.google.ortools.mathopt.Objectives.UpdateResult;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

public final class ObjectivesTest {
  private ModelId modelId;
  private Variables variables;

  @BeforeEach
  public void setUp() {
    modelId = new ModelId("test_model");
    variables = new Variables(modelId);
  }

  @Test
  public void newObjectives_isEmpty_gettersOk() {
    var objectives = new Objectives(modelId, "my_first_obj");

    assertThat(objectives.numObjectives()).isEqualTo(1);
    assertThat(objectives.getNextId()).isEqualTo(0L);
    assertThat(objectives.getAuxiliaryObjective(0L)).isNull();
    Objective primary = objectives.getPrimaryObjective();
    assertThat(primary.getUnmodifiableLinearTerms()).isEmpty();
    assertThat(primary.getPriority()).isEqualTo(0L);
    assertThat(primary.getOffset()).isEqualTo(0.0);
    assertThat(primary.isMaximize()).isFalse();
    assertThat(primary.getName()).isEqualTo("my_first_obj");
    assertThat(objectives.sortedAuxiliaryObjectives()).isEmpty();
    objectives.checkOwned(primary); // test that this does not throw.
  }

  @Test
  public void newObjectives_unnamedObjective_nameIsEmpty() {
    var objectives = new Objectives(modelId, "");
    assertThat(objectives.getPrimaryObjective().getName()).isEmpty();
  }

  @Test
  public void addAuxiliaryObjective_isEmpty_auxGettersOk() {
    var objectives = new Objectives(modelId, "");

    AuxiliaryObjective aux = objectives.addAuxiliaryObjective(4L, "aux");

    assertThat(aux.getId()).isEqualTo(0L);
    assertThat(aux.isDeleted()).isFalse();
    assertThat(aux.getUnmodifiableLinearTerms()).isEmpty();
    assertThat(aux.getPriority()).isEqualTo(4L);
    assertThat(aux.getOffset()).isEqualTo(0.0);
    assertThat(aux.isMaximize()).isFalse();
    assertThat(aux.getName()).isEqualTo("aux");
    objectives.checkOwned(aux); // test that this does not throw.
  }

  @Test
  public void addAuxiliaryObjective_isEmpty_objectivesGettersOk() {
    var objectives = new Objectives(modelId, "");

    AuxiliaryObjective auxA = objectives.addAuxiliaryObjective(6L, "auxA");
    AuxiliaryObjective auxB = objectives.addAuxiliaryObjective(4L, "auxB");

    assertThat(objectives.numObjectives()).isEqualTo(3);
    assertThat(objectives.getNextId()).isEqualTo(2L);
    assertThat(objectives.getAuxiliaryObjective(0L)).isSameInstanceAs(auxA);
    assertThat(objectives.getAuxiliaryObjective(1L)).isSameInstanceAs(auxB);
    assertThat(objectives.getAuxiliaryObjective(2L)).isNull();
    assertThat(objectives.sortedAuxiliaryObjectives()).containsExactly(auxA, auxB);
  }

  @Test
  public void sortedAuxiliaryObjectives_sparseObjectives_hasObjectives() {
    var objectives = new Objectives(modelId, "");

    AuxiliaryObjective auxA = objectives.addAuxiliaryObjective(6L, "auxA");
    AuxiliaryObjective auxB = objectives.addAuxiliaryObjective(4L, "auxB");
    AuxiliaryObjective auxC = objectives.addAuxiliaryObjective(8L, "auxC");
    AuxiliaryObjective auxD = objectives.addAuxiliaryObjective(1L, "auxD");
    objectives.deleteAuxiliaryObjective(auxB);
    objectives.deleteAuxiliaryObjective(auxD);

    assertThat(objectives.sortedAuxiliaryObjectives()).containsExactly(auxA, auxC);
  }

  @Test
  public void addAuxiliaryObjective_withAutomaticPriorityOnEmptyModel_priorityIsOne() {
    var objectives = new Objectives(modelId, "");

    AuxiliaryObjective objective = objectives.addAuxiliaryObjective("");

    assertThat(objective.getPriority()).isEqualTo(1L);
  }

  @Test
  public void addAuxiliaryObjective_withAutomaticPriorityOnSingleObjectiveModel_priorityIsLarger() {
    var objectives = new Objectives(modelId, "");
    objectives.getPrimaryObjective().setPriority(10L);

    AuxiliaryObjective objective = objectives.addAuxiliaryObjective("");

    assertThat(objective.getPriority()).isEqualTo(11L);
  }

  @Test
  public void addAuxiliaryObjective_withAutomaticPriorityAndExistingObjectives_priorityIsLarger() {
    var objectives = new Objectives(modelId, "");
    var unused = objectives.addAuxiliaryObjective(5L, "");
    var unused2 = objectives.addAuxiliaryObjective(2L, "");

    AuxiliaryObjective objective = objectives.addAuxiliaryObjective("");

    assertThat(objective.getPriority()).isEqualTo(6L);
  }

  @Test
  public void checkOwned_wrongModel_throws() {
    var objectives = new Objectives(modelId, "");
    var otherObjectives = new Objectives(new ModelId("bad_model"), "");

    var error = assertThrows(IllegalArgumentException.class,
        () -> objectives.checkOwned(otherObjectives.getPrimaryObjective()));

    assertThat(error).hasMessageThat().contains("another model");
  }

  @Test
  public void ensureNextIdAtleast_skipIds_nextIdCorrect() {
    var objectives = new Objectives(modelId, "");

    objectives.ensureNextIdAtLeast(3);

    assertThat(objectives.getNextId()).isEqualTo(3);
    assertThat(objectives.addAuxiliaryObjective("o").getId()).isEqualTo(3);
  }

  @Test
  public void deleteAuxObjective_isDeleted() {
    var objectives = new Objectives(modelId, "");
    AuxiliaryObjective aux = objectives.addAuxiliaryObjective(4L, "aux");

    objectives.deleteAuxiliaryObjective(aux);

    assertThat(aux.isDeleted()).isTrue();
  }

  @Test
  public void deleteAuxObjective_deleteTwice_isDeleted() {
    var objectives = new Objectives(modelId, "");
    AuxiliaryObjective aux = objectives.addAuxiliaryObjective(4L, "aux");

    assertThat(objectives.deleteAuxiliaryObjective(aux)).isTrue();
    assertThat(objectives.deleteAuxiliaryObjective(aux)).isFalse();

    assertThat(aux.isDeleted()).isTrue();
  }

  @Test
  public void deleteAuxObjective_hasTwoAuxObjs_oneLeft() {
    var objectives = new Objectives(modelId, "");
    AuxiliaryObjective auxA = objectives.addAuxiliaryObjective(4L, "auxA");
    AuxiliaryObjective auxB = objectives.addAuxiliaryObjective(6L, "auxB");

    assertThat(objectives.deleteAuxiliaryObjective(auxA)).isTrue();

    assertThat(auxA.isDeleted()).isTrue();
    assertThat(auxB.isDeleted()).isFalse();
    assertThat(objectives.numObjectives()).isEqualTo(2);
    assertThat(objectives.hasAuxiliaryObjectiveWithId(0L)).isFalse();
    assertThat(objectives.hasAuxiliaryObjectiveWithId(1L)).isTrue();
    assertThat(objectives.getAuxiliaryObjective(0L)).isNull();
    assertThat(objectives.getAuxiliaryObjective(1L)).isSameInstanceAs(auxB);
    assertThat(objectives.getNextId()).isEqualTo(2L);
  }

  @Test
  public void deleteAuxObjective_wrongModel_throws() {
    var objectives = new Objectives(modelId, "");
    var badObjectives = new Objectives(new ModelId("bad_model"), "");
    AuxiliaryObjective badAux = badObjectives.addAuxiliaryObjective(4L, "aux");

    var error = assertThrows(
        IllegalArgumentException.class, () -> objectives.deleteAuxiliaryObjective(badAux));

    assertThat(error).hasMessageThat().contains("another model");
  }

  @Test
  public void onVariableDeleted_wrongModel_throws() {
    var objectives = new Objectives(modelId, "");
    var badVariables = new Variables(new ModelId("bad_model"));
    Variable badVar = badVariables.addVariable("bad_x");

    var error =
        assertThrows(IllegalArgumentException.class, () -> objectives.onVariableDeleted(badVar));

    assertThat(error).hasMessageThat().contains("another model");
  }

  @Test
  public void onVariableDeleted_variableInSomeObjectives_isRemoved() {
    var objectives = new Objectives(modelId, "");
    Variable x = variables.addVariable("x");
    Variable y = variables.addVariable("y");
    Objective primary = objectives.getPrimaryObjective();
    AuxiliaryObjective auxA = objectives.addAuxiliaryObjective(1L, "A");
    AuxiliaryObjective auxB = objectives.addAuxiliaryObjective(2L, "B");
    primary.setLinearTerm(x, 2.0);
    primary.setLinearTerm(y, 4.0);
    auxB.setLinearTerm(x, 3.0);
    auxB.setLinearTerm(y, 5.0);

    objectives.onVariableDeleted(x);

    assertThat(primary.getUnmodifiableLinearTerms()).containsExactly(y, 4.0);
    assertThat(auxA.getUnmodifiableLinearTerms()).isEmpty();
    assertThat(auxB.getUnmodifiableLinearTerms()).containsExactly(y, 5.0);
  }

  @Test
  public void auxiliaryObjectivesToProto_noAuxObjectives_isEmpty() {
    var objectives = new Objectives(modelId, "");

    assertThat(objectives.auxiliaryObjectivesToProto()).isEmpty();
  }

  @Test
  public void auxiliaryObjectivesToProto_emptyObjectives_arePresent() {
    var objectives = new Objectives(modelId, "");
    var unused = objectives.addAuxiliaryObjective(6L, "A");
    var unused2 = objectives.addAuxiliaryObjective(4L, "B");

    ImmutableMap<Long, ObjectiveProto> auxObjProtos = objectives.auxiliaryObjectivesToProto();

    assertThat(auxObjProtos.keySet()).containsExactly(0L, 1L);
    assertThat(auxObjProtos.get(0L))
        .ignoringFieldAbsence()
        .isEqualTo(ObjectiveProto.newBuilder().setName("A").setPriority(6L).build());
    assertThat(auxObjProtos.get(1L))
        .ignoringFieldAbsence()
        .isEqualTo(ObjectiveProto.newBuilder().setName("B").setPriority(4L).build());
  }

  @Test
  public void auxiliaryObjectivesToProto_objectiveWithCoefs_isPresent() {
    var objectives = new Objectives(modelId, "");
    Variable x = variables.addVariable("x");
    var unused = variables.addVariable("y");
    Variable z = variables.addVariable("z");
    var auxA = objectives.addAuxiliaryObjective(6L, "A");
    auxA.setMaximize(true).setOffset(4.0).setLinearTerm(x, 5.5).setLinearTerm(z, -2.0);

    ImmutableMap<Long, ObjectiveProto> auxObjProtos = objectives.auxiliaryObjectivesToProto();

    assertThat(auxObjProtos.keySet()).containsExactly(0L);
    var expectedObj =
        ObjectiveProto.newBuilder().setName("A").setPriority(6L).setMaximize(true).setOffset(4.0);
    expectedObj.getLinearCoefficientsBuilder().addIds(0).addValues(5.5).addIds(2).addValues(-2.0);
    assertThat(auxObjProtos.get(0L)).isEqualTo(expectedObj.build());
  }

  private static ModelUpdateProto updateToProto(UpdateResult update) {
    var updateProto = ModelUpdateProto.newBuilder();
    update.setInProto(updateProto);
    return updateProto.build();
  }

  @Test
  public void diffUpdate_primaryObjectiveEmpty_isEmpty() {
    var objectives = new Objectives(modelId, "");
    Diff diff = objectives.addDiff(variables.getNextId());

    assertThat(updateToProto(diff.update(ImmutableList.of()))).isEqualToDefaultInstance();
  }

  @Test
  public void diffUpdate_primaryObjectiveUnchanged_isEmpty() {
    var objectives = new Objectives(modelId, "");
    Variable x = variables.addVariable("x");
    Objective obj = objectives.getPrimaryObjective();
    obj.setOffset(2.0).setPriority(1L).setMaximize(true).setLinearTerm(x, 3.0);
    Diff diff = objectives.addDiff(variables.getNextId());

    assertThat(updateToProto(diff.update(ImmutableList.of()))).isEqualToDefaultInstance();
  }

  @Test
  public void diffUpdate_primaryObjectiveModified_hasUpdate() {
    var objectives = new Objectives(modelId, "");
    Variable x = variables.addVariable("x");
    Diff diff = objectives.addDiff(variables.getNextId());
    Variable y = variables.addVariable("y");
    Objective obj = objectives.getPrimaryObjective();
    obj.setMaximize(true).setPriority(2L).setOffset(2.5).setLinearTerm(x, 1.1).setLinearTerm(
        y, 2.2);

    ModelUpdateProto update = updateToProto(diff.update(ImmutableList.of(y)));

    var expectedPrimaryObjective =
        ObjectiveUpdatesProto.newBuilder()
            .setPriorityUpdate(2L)
            .setOffsetUpdate(2.5)
            .setDirectionUpdate(true)
            .setLinearCoefficients(
                SparseDoubleVectorProto.newBuilder().addIds(0).addValues(1.1).addIds(1).addValues(
                    2.2))
            .build();
    var expected = ModelUpdateProto.newBuilder();
    expected.setObjectiveUpdates(expectedPrimaryObjective);
    assertThat(update).isEqualTo(expected.build());
  }

  @Test
  public void diffUpdate_auxiliaryObjectiveModified_hasUpdate() {
    var objectives = new Objectives(modelId, "");
    Variable x = variables.addVariable("x");
    AuxiliaryObjective auxObj = objectives.addAuxiliaryObjective(4L, "");
    Diff diff = objectives.addDiff(variables.getNextId());
    Variable y = variables.addVariable("y");
    auxObj.setMaximize(true).setPriority(2L).setOffset(2.5).setLinearTerm(x, 1.1).setLinearTerm(
        y, 2.2);

    ModelUpdateProto update = updateToProto(diff.update(ImmutableList.of(y)));

    var expectedAuxObjective =
        ObjectiveUpdatesProto.newBuilder()
            .setPriorityUpdate(2L)
            .setOffsetUpdate(2.5)
            .setDirectionUpdate(true)
            .setLinearCoefficients(
                SparseDoubleVectorProto.newBuilder().addIds(0).addValues(1.1).addIds(1).addValues(
                    2.2))
            .build();
    var expected = ModelUpdateProto.newBuilder();
    expected.getAuxiliaryObjectivesUpdatesBuilder().putObjectiveUpdates(0L, expectedAuxObjective);
    assertThat(update).isEqualTo(expected.build());
  }

  @Test
  public void diffUpdate_auxiliaryObjectiveDeleted_hasDelete() {
    var objectives = new Objectives(modelId, "");
    AuxiliaryObjective auxObj = objectives.addAuxiliaryObjective(4L, "");
    Diff diff = objectives.addDiff(variables.getNextId());
    objectives.deleteAuxiliaryObjective(auxObj);

    ModelUpdateProto update = updateToProto(diff.update(ImmutableList.of()));

    var expected = ModelUpdateProto.newBuilder();
    expected.getAuxiliaryObjectivesUpdatesBuilder().addDeletedObjectiveIds(0L);
    assertThat(update).isEqualTo(expected.build());
  }

  @Test
  public void diffUpdate_auxiliaryObjectiveDeletedAfterModification_hasModificationWithoutDelete() {
    var objectives = new Objectives(modelId, "");
    AuxiliaryObjective auxObj = objectives.addAuxiliaryObjective(4L, "");
    Diff diff = objectives.addDiff(variables.getNextId());
    auxObj.setOffset(4.0);
    objectives.deleteAuxiliaryObjective(auxObj);

    ModelUpdateProto update = updateToProto(diff.update(ImmutableList.of()));

    var expected = ModelUpdateProto.newBuilder();
    expected.getAuxiliaryObjectivesUpdatesBuilder().addDeletedObjectiveIds(0L);
    assertThat(update).isEqualTo(expected.build());
  }

  @Test
  public void diffUpdate_auxiliaryObjectiveDeletedAfterCheckpoint_doesNotHaveDelete() {
    var objectives = new Objectives(modelId, "");
    Diff diff = objectives.addDiff(variables.getNextId());
    AuxiliaryObjective auxObj = objectives.addAuxiliaryObjective(4L, "");
    objectives.deleteAuxiliaryObjective(auxObj);

    ModelUpdateProto update = updateToProto(diff.update(ImmutableList.of()));

    assertThat(update).isEqualToDefaultInstance();
  }

  @Test
  public void diffUpdate_auxiliaryObjectiveAdded_hasNewObj() {
    var objectives = new Objectives(modelId, "");
    Variable x = variables.addVariable("x");
    Diff diff = objectives.addDiff(variables.getNextId());
    AuxiliaryObjective auxObj = objectives.addAuxiliaryObjective(4L, "");
    Variable y = variables.addVariable("y");
    auxObj.setMaximize(true).setPriority(2L).setOffset(2.5).setLinearTerm(x, 1.1).setLinearTerm(
        y, 2.2);

    ModelUpdateProto update = updateToProto(diff.update(ImmutableList.of(y)));

    var expectedAuxObjective =
        ObjectiveProto.newBuilder()
            .setPriority(2L)
            .setOffset(2.5)
            .setMaximize(true)
            .setLinearCoefficients(
                SparseDoubleVectorProto.newBuilder().addIds(0).addValues(1.1).addIds(1).addValues(
                    2.2))
            .build();
    var expected = ModelUpdateProto.newBuilder();
    expected.getAuxiliaryObjectivesUpdatesBuilder().putNewObjectives(0L, expectedAuxObjective);
    assertThat(update).isEqualTo(expected.build());
  }

  @Test
  public void diffUpdate_modifiedVariableDeleted_deletionsExcluded() {
    var objectives = new Objectives(modelId, "");
    Variable x = variables.addVariable("x");
    Variable y = variables.addVariable("y");
    Objective primary = objectives.getPrimaryObjective();
    primary.setLinearTerm(x, 2.0).setLinearTerm(y, 3.0);
    AuxiliaryObjective auxObj = objectives.addAuxiliaryObjective(4L, "");
    auxObj.setLinearTerm(x, 4.0).setLinearTerm(y, 5.0);
    Diff diff = objectives.addDiff(variables.getNextId());
    primary.setLinearTerm(x, 6.0).setLinearTerm(y, 7.0);
    auxObj.setLinearTerm(x, 8.0).setLinearTerm(y, 9.0);

    objectives.onVariableDeleted(x);
    ModelUpdateProto update = updateToProto(diff.update(ImmutableList.of()));

    var expectedPrimObj =
        ObjectiveUpdatesProto.newBuilder()
            .setLinearCoefficients(
                SparseDoubleVectorProto.newBuilder().addIds(1).addValues(7.0).build())
            .build();
    var expectedAuxObj =
        ObjectiveUpdatesProto.newBuilder()
            .setLinearCoefficients(
                SparseDoubleVectorProto.newBuilder().addIds(1).addValues(9.0).build())
            .build();

    var expected = ModelUpdateProto.newBuilder();
    expected.getAuxiliaryObjectivesUpdatesBuilder().putObjectiveUpdates(0L, expectedAuxObj);
    expected.setObjectiveUpdates(expectedPrimObj);
    assertThat(update).isEqualTo(expected.build());
  }

  @Test
  public void diffAdvance_hadUpdates_updatesCleared() {
    var objectives = new Objectives(modelId, "");
    Variable x = variables.addVariable("x");
    Objective primary = objectives.getPrimaryObjective();
    AuxiliaryObjective auxObj = objectives.addAuxiliaryObjective(4L, "");
    AuxiliaryObjective auxObj2 = objectives.addAuxiliaryObjective(5L, "");
    Diff diff = objectives.addDiff(variables.getNextId());

    primary.setMaximize(true).setPriority(1L).setOffset(1.0).setLinearTerm(x, 2.3);
    auxObj.setMaximize(true).setPriority(3L).setOffset(-1.0).setLinearTerm(x, -2.3);
    objectives.deleteAuxiliaryObjective(auxObj2);
    diff.advance(variables.getNextId());
    ModelUpdateProto update = updateToProto(diff.update(ImmutableList.of()));

    assertThat(update).isEqualToDefaultInstance();
  }

  @Test
  public void diffAdvance_variableCreated_advanceMovesVariableCheckpoint() {
    var objectives = new Objectives(modelId, "");
    var unused = variables.addVariable("x");
    Objective primary = objectives.getPrimaryObjective();
    Diff diff = objectives.addDiff(variables.getNextId());
    Variable y = variables.addVariable("y");

    diff.advance(variables.getNextId());
    primary.setLinearTerm(y, 1.0);
    // Note: the list of new variables is empty, so the y term will only show up in the update proto
    // if variable checkpoint was advanced and the term was stored in the diff.
    ModelUpdateProto update = updateToProto(diff.update(ImmutableList.of()));

    var expected = ModelUpdateProto.newBuilder();
    expected.getObjectiveUpdatesBuilder().getLinearCoefficientsBuilder().addIds(1).addValues(1.0);
    assertThat(update).isEqualTo(expected.build());
  }

  @Test
  public void diffAdvance_auxObjCreated_advanceMovesObjCheckpoint() {
    var objectives = new Objectives(modelId, "");
    Diff diff = objectives.addDiff(variables.getNextId());
    AuxiliaryObjective auxA = objectives.addAuxiliaryObjective(1L, "A");

    diff.advance(variables.getNextId());
    auxA.setOffset(4.0);
    // Note: because advance moves the aux objective checkpoint, this change should show up as a
    // modification of an existing objective, rather than a new one.
    ModelUpdateProto update = updateToProto(diff.update(ImmutableList.of()));

    var expectedAuxObj = AuxiliaryObjectivesUpdatesProto.newBuilder()
                             .putObjectiveUpdates(0L,
                                 ObjectiveUpdatesProto.newBuilder().setOffsetUpdate(4.0).build())
                             .build();
    var expected =
        ModelUpdateProto.newBuilder().setAuxiliaryObjectivesUpdates(expectedAuxObj).build();
    assertThat(update).isEqualTo(expected);
  }

  @Test
  public void removeDiff_makeModification_noUpdate() {
    var objectives = new Objectives(modelId, "");
    Diff diff = objectives.addDiff(variables.getNextId());
    objectives.removeDiff(diff);
    objectives.getPrimaryObjective().setOffset(4.0);

    ModelUpdateProto update = updateToProto(diff.update(ImmutableList.of()));

    assertThat(update).isEqualToDefaultInstance();
  }

  @Test
  public void removeDiff_sameDiffTwice_throws() {
    var objectives = new Objectives(modelId, "");
    Diff diff = objectives.addDiff(variables.getNextId());
    objectives.removeDiff(diff);
    var error = assertThrows(IllegalArgumentException.class, () -> objectives.removeDiff(diff));
    assertThat(error).hasMessageThat().contains("cannot remove");
  }
}
