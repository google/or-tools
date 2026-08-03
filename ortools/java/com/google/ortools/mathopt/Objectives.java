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

import static java.lang.Math.max;

import com.google.common.base.Preconditions;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableMap;
import com.google.ortools.mathopt.Objective.OnChangeListener;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.Set;
import org.jspecify.annotations.Nullable;

/** Maintains the primary (and optionally auxiliary) objectives of the model and tracks changes. */
final class Objectives {
  private final ModelId modelId;
  private final Objective primaryObjective;
  private final Map<Long, AuxiliaryObjective> auxiliaryObjectives = new HashMap<>();
  private long nextId = 0L;
  private final List<Diff> diffs = new ArrayList<>();
  private final OnChangeListener objectiveListener = new OnChangeListener() {
    @Override
    public void onSenseChanged(Objective objective) {
      for (Diff diff : diffs) {
        diff.markSenseChanged(objective);
      }
    }

    @Override
    public void onOffsetChanged(Objective objective) {
      for (Diff diff : diffs) {
        diff.markOffsetChanged(objective);
      }
    }

    @Override
    public void onLinearTermChange(Objective objective, Variable variable, double coefficient) {
      for (Diff diff : diffs) {
        diff.markLinearTermChanged(objective, variable);
      }
    }

    @Override
    public void onPriorityChanged(Objective objective) {
      for (Diff diff : diffs) {
        diff.markPriorityChanged(objective);
      }
    }
  };

  Objectives(ModelId modelId, String primaryObjectiveName) {
    this.modelId = modelId;
    this.primaryObjective =
        new Objective(/* priority= */ 0, primaryObjectiveName, objectiveListener, modelId);
  }

  Objective getPrimaryObjective() {
    return primaryObjective;
  }

  /** Creates and returns a new empty objective in the model. */
  AuxiliaryObjective addAuxiliaryObjective(long priority, String name) {
    AuxiliaryObjective result =
        new AuxiliaryObjective(priority, name, nextId, objectiveListener, modelId);
    nextId++;
    auxiliaryObjectives.put(result.getId(), result);
    return result;
  }

  /**
   * Creates and returns a new empty objective in the model with priority one above the largest
   * priority of all objectives in the model.
   *
   * <p>Note: this runs in time linear in the number of objectives in the model.
   */
  AuxiliaryObjective addAuxiliaryObjective(String name) {
    AuxiliaryObjective result =
        new AuxiliaryObjective(maxPriority() + 1, name, nextId, objectiveListener, modelId);
    nextId++;
    auxiliaryObjectives.put(result.getId(), result);
    return result;
  }

  /** Returns the number of objectives in the model (primary plus auxiliary). */
  int numObjectives() {
    return 1 + auxiliaryObjectives.size();
  }

  /**
   * Checks that {@code objective} is owned by the model, and throws an {@link
   * IllegalArgumentException} if not (it is from another model).
   */
  void checkOwned(Objective objective) {
    Preconditions.checkArgument(objective.getModelId() == modelId,
        "Objective %s is not from this model (named: %s), it is from another model (named: %s).",
        objective, modelId.getName(), objective.getModelId().getName());
  }

  /**
   * Returns true if the model contains a {@link AuxiliaryObjective} with id of {@code id}, or false
   * otherwise (either the objective never existed, or was deleted).
   */
  boolean hasAuxiliaryObjectiveWithId(long id) {
    return auxiliaryObjectives.containsKey(id);
  }

  /**
   * Returns the {@link AuxiliaryObjective} from the model with id of {@code id}, or null if there
   * is no such objective (the objective never existed or was deleted).
   *
   * <p>Prefer maintaining references to the {@link AuxiliaryObjective} objects returned by {@link
   * #addAuxiliaryObjective(long, String)} as this function can return null if {@code id} is
   * invalid.
   */
  @Nullable
  AuxiliaryObjective getAuxiliaryObjective(long id) {
    return auxiliaryObjectives.get(id);
  }

  /**
   * Attempts to delete {@code objective} from the model and return true on success.
   *
   * <p>Note: will return false only when {@code objective} was previously deleted.
   *
   * @throws IllegalArgumentException if {@code objective} is from another model.
   */
  boolean deleteAuxiliaryObjective(AuxiliaryObjective objective) {
    checkOwned(objective);
    if (objective.isDeleted()) {
      return false;
    }
    objective.markDeleted();
    for (Diff diff : diffs) {
      diff.deleteObjective(objective);
    }
    auxiliaryObjectives.remove(objective.getId());
    return true;
  }

  /**
   * Returns the id that will be assigned to the next {@link AuxiliaryObjective} created.
   *
   * <p>All previously created auxiliary objectives have id less than this value.
   */
  long getNextId() {
    return nextId;
  }

  /** Makes the value of {@link #getNextId()} the max of the current value and {@code id}. */
  void ensureNextIdAtLeast(long id) {
    nextId = max(nextId, id);
  }

  /** Returns the {@link AuxiliaryObjective}s of the model sorted by id. */
  ImmutableList<AuxiliaryObjective> sortedAuxiliaryObjectives() {
    return ImmutableList.sortedCopyOf(
        Comparator.comparingLong(AuxiliaryObjective::getId), auxiliaryObjectives.values());
  }

  /**
   * Removes {@code variable} from all objectives; called when deleting {@code variable}.
   *
   * @throws IllegalArgumentException if {@code variable} is from another model
   */
  void onVariableDeleted(Variable variable) {
    checkVariableOwned(variable);
    for (Diff diff : diffs) {
      diff.onVariableDeleted(variable);
    }
    primaryObjective.onVariableDeleted(variable);
    for (AuxiliaryObjective auxiliaryObjective : auxiliaryObjectives.values()) {
      auxiliaryObjective.onVariableDeleted(variable);
    }
  }

  /** Returns a new {@link Diff} to track changes to this from now forward. */
  Diff addDiff(long variableCheckpoint) {
    var result = new Diff(variableCheckpoint);
    diffs.add(result);
    return result;
  }

  /**
   * Deletes {@code diff} from this, or throws an {@link IllegalArgumentException} if {@code diff}
   * was from another model or was already deleted.
   */
  void removeDiff(Diff diff) {
    Preconditions.checkArgument(diffs.remove(diff), "Diff not present, cannot remove it");
  }

  /** Returns a protocol buffer representation of all the auxiliary objectives in the model. */
  ImmutableMap<Long, ObjectiveProto> auxiliaryObjectivesToProto() {
    var auxObjectivesProto = ImmutableMap.<Long, ObjectiveProto>builder();
    for (Map.Entry<Long, AuxiliaryObjective> entry : auxiliaryObjectives.entrySet()) {
      auxObjectivesProto.put(entry.getKey(), entry.getValue().toProto());
    }
    return auxObjectivesProto.buildOrThrow();
  }

  /** Returns the highest priority of all objectives in the model. */
  private long maxPriority() {
    long result = primaryObjective.getPriority();
    for (AuxiliaryObjective auxObj : auxiliaryObjectives.values()) {
      result = max(result, auxObj.getPriority());
    }
    return result;
  }

  private void checkVariableOwned(Variable variable) {
    Preconditions.checkArgument(variable.getModelId() == modelId,
        "Variable %s is not from this model (named: %s), it is from another model (named: %s).",
        variable, modelId.getName(), variable.getModelId().getName());
  }

  /**
   * A summary of a set of changes on {@link Objectives}.
   *
   * @see Objectives.Diff#update(ImmutableList)
   */
  static final class UpdateResult {
    private final Optional<ObjectiveUpdatesProto> objectiveUpdates;
    private final AuxiliaryObjectivesUpdatesProto auxiliaryObjectiveUpdates;

    /**
     * Creates an {@link Objectives.UpdateResult}, typically prefer {@link
     * Objectives.Diff#update(ImmutableList)}.
     */
    UpdateResult(Optional<ObjectiveUpdatesProto> objectiveUpdates,
        AuxiliaryObjectivesUpdatesProto auxiliaryObjectiveUpdates) {
      this.objectiveUpdates = objectiveUpdates;
      this.auxiliaryObjectiveUpdates = auxiliaryObjectiveUpdates;
    }

    /** Sets this in {@code modelUpdateProto}. */
    void setInProto(ModelUpdateProto.Builder modelUpdateProto) {
      if (objectiveUpdates.isPresent()) {
        modelUpdateProto.setObjectiveUpdates(objectiveUpdates.get());
      }
      if (auxiliaryObjectiveUpdates.getDeletedObjectiveIdsCount() > 0
          || auxiliaryObjectiveUpdates.getObjectiveUpdatesCount() > 0
          || auxiliaryObjectiveUpdates.getNewObjectivesCount() > 0) {
        modelUpdateProto.setAuxiliaryObjectivesUpdates(auxiliaryObjectiveUpdates);
      }
    }
  }

  private static final class LazyObjectiveUpdatesProtoBuilder {
    ObjectiveUpdatesProto.Builder objectiveUpdates = null;

    /** Calling this function results in {@link #build()} returning a nonempty optional. */
    ObjectiveUpdatesProto.Builder getOrCreate() {
      if (objectiveUpdates == null) {
        objectiveUpdates = ObjectiveUpdatesProto.newBuilder();
      }
      return objectiveUpdates;
    }

    Optional<ObjectiveUpdatesProto> build() {
      return Optional.ofNullable(objectiveUpdates).map(value -> value.build());
    }
  }

  /**
   * Tracks the changes for a single {@link Objective} from a previous checkpoint to present (either
   * primary or auxiliary).
   */
  private static final class SingleObjectiveDiff {
    final Objective objective;
    boolean senseDirty = false;
    boolean offsetDirty = false;
    boolean priorityDirty = false;
    final Set<Variable> linearCoefficientDirty = new HashSet<>();

    SingleObjectiveDiff(Objective objective) {
      this.objective = objective;
    }

    void onVariableDeleted(Variable variable) {
      linearCoefficientDirty.remove(variable);
    }

    /**
     * Returns a description of all the changes to {@link #objective} since the last call to {@link
     * #advance()} (or from creation time if {@link #advance()} was never called).
     *
     * @param newVariables the {@link Variable} objects that have been added to (and not deleted
     *     from) the model since the last checkpoint, must be sorted by {@link Variable#getId()}
     */
    Optional<ObjectiveUpdatesProto> update(ImmutableList<Variable> newVariables) {
      var updates = new LazyObjectiveUpdatesProtoBuilder();
      if (senseDirty) {
        updates.getOrCreate().setDirectionUpdate(objective.isMaximize());
      }
      if (offsetDirty) {
        updates.getOrCreate().setOffsetUpdate(objective.getOffset());
      }
      if (priorityDirty) {
        updates.getOrCreate().setPriorityUpdate(objective.getPriority());
      }
      var linearCoeffs = SparseDoubleVectorProto.newBuilder();
      for (Variable v : ImmutableList.sortedCopyOf(
               Comparator.comparingLong(Variable::getId), linearCoefficientDirty)) {
        linearCoeffs.addIds(v.getId());
        linearCoeffs.addValues(objective.getLinearTerm(v));
      }
      for (Variable v : newVariables) {
        double term = objective.getLinearTerm(v);
        if (term != 0.0) {
          linearCoeffs.addIds(v.getId());
          linearCoeffs.addValues(term);
        }
      }
      // Do not trigger lazy creation for an empty message
      if (linearCoeffs.getIdsCount() > 0) {
        updates.getOrCreate().setLinearCoefficients(linearCoeffs.build());
      }
      return updates.build();
    }

    /**
     * Clears the existing tracked changes and track future changes on {@link #objective} from now
     * forward.
     */
    void advance() {
      senseDirty = false;
      offsetDirty = false;
      priorityDirty = false;
      linearCoefficientDirty.clear();
    }
  }

  /**
   * Tracks the changes for all objectives (primary and auxiliary) from a previous checkpoint.
   *
   * <p>Not all changes are stored explicitly. Instead, we maintain a {@link #checkpoint} which is
   * the id of an {@link AuxiliaryObjective}, and a {@link #variableCheckpoint} that is the id of a
   * {@link Variable}, and we only store changes to the primary objective and auxiliary objectives
   * before the checkpoint using variables before the variable checkpoint. When an {@link
   * #update(ImmutableList)} is requested, we return data before the checkpoint that changed, and
   * all data after the checkpoint.
   */
  final class Diff {
    long checkpoint;
    long variableCheckpoint;
    final Set<Long> deleted = new HashSet<>();
    final Map<Objective, SingleObjectiveDiff> objectiveDiffs = new HashMap<>();

    Diff(long variableCheckpoint) {
      this.checkpoint = nextId;
      this.variableCheckpoint = variableCheckpoint;
      objectiveDiffs.put(primaryObjective, new SingleObjectiveDiff(primaryObjective));
      addAuxiliaryObjectiveDiffs(0, checkpoint);
    }

    private void addAuxiliaryObjectiveDiffs(long startId, long endId) {
      for (AuxiliaryObjective objective : auxiliaryObjectives.values()) {
        if (objective.getId() >= startId && objective.getId() < endId) {
          objectiveDiffs.put(objective, new SingleObjectiveDiff(objective));
        }
      }
    }

    /** Removes tracked changes on {@code variable}, call when deleting {@code variable}. */
    void onVariableDeleted(Variable variable) {
      if (variable.getId() < checkpoint) {
        for (SingleObjectiveDiff auxDiff : objectiveDiffs.values()) {
          auxDiff.onVariableDeleted(variable);
        }
      }
    }

    /** Track that objective direction (maximize or minimize) has changed. */
    void markSenseChanged(Objective objective) {
      SingleObjectiveDiff diff = objectiveDiffs.get(objective);
      if (diff != null) {
        diff.senseDirty = true;
      }
    }

    /** Track that objective offset has changed. */
    void markOffsetChanged(Objective objective) {
      SingleObjectiveDiff diff = objectiveDiffs.get(objective);
      if (diff != null) {
        diff.offsetDirty = true;
      }
    }

    /**
     * Track that the linear coefficient in the objective for {@code variable} has changed.
     *
     * <p>Requires that the variable has not been deleted.
     */
    void markLinearTermChanged(Objective objective, Variable variable) {
      SingleObjectiveDiff diff = objectiveDiffs.get(objective);
      if (diff != null && variable.getId() < variableCheckpoint) {
        diff.linearCoefficientDirty.add(variable);
      }
    }

    /** Track that priority of an objective has changed. */
    void markPriorityChanged(Objective objective) {
      SingleObjectiveDiff diff = objectiveDiffs.get(objective);
      if (diff != null) {
        diff.priorityDirty = true;
      }
    }

    /** Track that {@code objective} is now deleted from the model. */
    void deleteObjective(AuxiliaryObjective objective) {
      if (objective.getId() < checkpoint) {
        deleted.add(objective.getId());
        objectiveDiffs.remove(objective);
      }
    }

    /**
     * Returns a description of all changes to the model since the checkpoint was advanced.
     *
     * @param newVariables the {@link Variable} objects that have been added to (and not deleted
     *     from) the model since the last checkpoint, must be sorted by {@link Variable#getId()}
     */
    UpdateResult update(ImmutableList<Variable> newVariables) {
      var auxObjectiveUpdates = AuxiliaryObjectivesUpdatesProto.newBuilder();
      auxObjectiveUpdates.addAllDeletedObjectiveIds(ImmutableList.sortedCopyOf(deleted));
      for (AuxiliaryObjective auxObj : auxiliaryObjectives.values()) {
        if (auxObj.getId() < checkpoint) {
          Optional<ObjectiveUpdatesProto> update = objectiveDiffs.get(auxObj).update(newVariables);
          if (update.isPresent()) {
            auxObjectiveUpdates.putObjectiveUpdates(auxObj.getId(), update.get());
          }
        } else {
          auxObjectiveUpdates.putNewObjectives(auxObj.getId(), auxObj.toProto());
        }
      }
      return new UpdateResult(
          objectiveDiffs.get(primaryObjective).update(newVariables), auxObjectiveUpdates.build());
    }

    /**
     * Clears existing tracked changes and track future changes on the enclosing {@link Objectives}
     * from this point forward.
     */
    void advance(long variableCheckpoint) {
      for (SingleObjectiveDiff diff : objectiveDiffs.values()) {
        diff.advance();
      }
      addAuxiliaryObjectiveDiffs(checkpoint, nextId);
      checkpoint = nextId;
      this.variableCheckpoint = variableCheckpoint;
      deleted.clear();
    }
  }
}
