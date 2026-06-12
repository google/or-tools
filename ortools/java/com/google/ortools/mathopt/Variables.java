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
import com.google.common.collect.ImmutableSet;
import com.google.ortools.mathopt.ModelUpdateProto;
import com.google.ortools.mathopt.VariableUpdatesProto;
import com.google.ortools.mathopt.VariablesProto;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import org.jspecify.annotations.Nullable;

/** Maintains the collection of variables in the model and tracks updates to them. */
final class Variables {
  private final ModelId modelId;
  private final Map<Long, Variable> variables = new HashMap<>();
  private long nextId = 0L;
  private final List<Diff> diffs = new ArrayList<>();
  private final Variable.OnChangeListener variableListener = new Variable.OnChangeListener() {
    @Override
    public void onLowerBoundChanged(Variable variable) {
      for (Diff diff : diffs) {
        diff.markLowerBoundDirty(variable);
      }
    }

    @Override
    public void onUpperBoundChanged(Variable variable) {
      for (Diff diff : diffs) {
        diff.markUpperBoundDirty(variable);
      }
    }

    @Override
    public void onIntegerChanged(Variable variable) {
      for (Diff diff : diffs) {
        diff.markIntegerBoundDirty(variable);
      }
    }
  };

  Variables(ModelId modelId) {
    this.modelId = modelId;
  }

  /** Creates and returns a new continuous unbounded variable in the model. */
  Variable addVariable(String name) {
    return addVariable(
        /* lowerBound= */ Double.NEGATIVE_INFINITY,
        /*upperBound=*/Double.POSITIVE_INFINITY,
        /* isInteger= */ false, name);
  }

  /**
   * Creates and returns a new variable with bounds [lowerBound, upperBound] that is restricted to
   * integer values if {@code isInteger} is true (otherwise the variable is continuous) in the
   * model.
   */
  Variable addVariable(double lowerBound, double upperBound, boolean isInteger, String name) {
    Variable var =
        new Variable(lowerBound, upperBound, isInteger, name, nextId, variableListener, modelId);
    variables.put(nextId, var);
    nextId++;
    return var;
  }

  /** Returns the number of variables in the model. */
  int numVariables() {
    return variables.size();
  }

  /**
   * Checks that {@code variable} is owned by the model, and throws an {@link
   * IllegalArgumentException} if not (it is from another model).
   */
  void checkOwned(Variable variable) {
    Preconditions.checkArgument(variable.getModelId() == modelId,
        "Variable %s is not from this model (named: %s), it is from another model (named: %s).",
        variable, modelId.getName(), variable.getModelId().getName());
  }

  /**
   * Returns true if the model contains a {@link Variable} with id of {@code id}, or false otherwise
   * (either the variable never existed, or was deleted).
   */
  boolean hasVariableWithId(long id) {
    return variables.containsKey(id);
  }

  /**
   * Returns the {@link Variable} from the model with id of {@code id}, or null if there is no such
   * variable (the variable never existed or was deleted).
   *
   * <p>Prefer maintaining references to the {@link Variable} objects returned by {@link
   * #addVariable(double, double, boolean, String)} as this function can return null if {@code id}
   * is invalid.
   */
  @Nullable
  Variable getVariable(long id) {
    return variables.get(id);
  }

  /**
   * Attempts to delete {@code variable} from the model and return true on success.
   *
   * <p>Note: will return false only when {@code variable} was previously deleted.
   *
   * @throws IllegalArgumentException if {@code variable} is from another model.
   */
  boolean deleteVariable(Variable variable) {
    checkOwned(variable);
    if (variable.isDeleted()) {
      return false;
    }
    variable.markDeleted();
    for (Diff diff : diffs) {
      diff.deleteVariable(variable);
    }
    variables.remove(variable.getId());
    return true;
  }

  /**
   * Returns the id that will be assigned to the next {@link Variable} created.
   *
   * <p>All previously created variables will have id less than this value.
   */
  long getNextId() {
    return nextId;
  }

  /** Makes the value of {@link #getNextId()} the max of the current value and {@code id}. */
  void ensureNextIdAtLeast(long id) {
    nextId = max(nextId, id);
  }

  /** Creates and returns a new list of variables with id at least {@code startId}. */
  ImmutableList<Variable> getVariablesFromId(long startId) {
    var result = ImmutableList.<Variable>builder();
    long nextVarId = getNextId();
    for (long id = startId; id < nextVarId; ++id) {
      Variable v = getVariable(id);
      if (v != null) {
        result.add(v);
      }
    }
    return result.build();
  }

  /** Returns a new {@link Diff} to track changes to this from now forward. */
  Diff addDiff() {
    var result = new Diff();
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

  /** Returns the {@link Variable}s of the model sorted by id. */
  ImmutableList<Variable> sortedVariables() {
    return ImmutableList.sortedCopyOf(
        Comparator.comparingLong(Variable::getId), variables.values());
  }

  /** Returns a protocol buffer representation of all the variables in the model. */
  VariablesProto toProto() {
    VariablesProto.Builder variablesProto = VariablesProto.newBuilder();
    variables.values()
        .stream()
        .sorted(Comparator.comparingLong(Variable::getId))
        .forEachOrdered(variable -> variable.appendToProto(variablesProto));
    return variablesProto.build();
  }

  /**
   * A summary of a set of changes on {@link Variables}.
   *
   * @see Diff#update()
   */
  static final class UpdateResult {
    private final List<Long> deletes;
    private final VariableUpdatesProto updates;
    private final VariablesProto creates;

    /** Creates an {@link UpdateResult}, typically prefer {@link Diff#update()}. */
    UpdateResult(List<Long> deletes, VariableUpdatesProto updates, VariablesProto creates) {
      this.deletes = deletes;
      this.updates = updates;
      this.creates = creates;
    }

    /** Sets this in {@code modelUpdateProto}. */
    void setInProto(ModelUpdateProto.Builder modelUpdateProto) {
      modelUpdateProto.clearDeletedVariableIds();
      modelUpdateProto.addAllDeletedVariableIds(deletes);
      modelUpdateProto.setVariableUpdates(updates);
      modelUpdateProto.setNewVariables(creates);
    }
  }

  /**
   * Tracks the changes to {@link Variables} from a previous checkpoint to present.
   *
   * <p>The checkpoint is represented by the id used for the next {@link Variable} created, from
   * {@link Variables#nextId}. Changes to the model on variables with id at least checkpoint are not
   * tracked explicitly. Instead, we build a summary of changes in {@link #update()} with the
   * tracked changes to variables with id less than checkpoint, and all variables created after the
   * checkpoint. Calling {@link #advance()} discards all tracked changes and updates the checkpoint.
   */
  final class Diff {
    private long checkpoint;
    private final HashSet<Long> deleted = new HashSet<>();
    private final HashSet<Variable> dirtyLowerBounds = new HashSet<>();
    private final HashSet<Variable> dirtyUpperBounds = new HashSet<>();
    private final HashSet<Variable> dirtyIntegers = new HashSet<>();

    /** Do not create directly, call {@link Variables#addDiff()} instead. */
    private Diff() {
      checkpoint = nextId;
    }

    /** Creates and returns a new set of ids for the variables that have been deleted. */
    ImmutableSet<Long> getDeleted() {
      return ImmutableSet.copyOf(deleted);
    }

    /**
     * Creates and returns a new list of the variables created since the most recent checkpoint
     * sorted by id.
     */
    ImmutableList<Variable> newVariables() {
      return getVariablesFromId(checkpoint);
    }

    /**
     * Clears existing tracked changes and track future changes on the enclosing {@link Variables}
     * from this point forward, and returns the new Variable checkpoint.
     */
    long advance() {
      checkpoint = nextId;
      deleted.clear();
      dirtyLowerBounds.clear();
      dirtyUpperBounds.clear();
      dirtyIntegers.clear();
      return checkpoint;
    }

    /**
     * Track that the lower bound of {@code variable} has changed.
     *
     * <p>Requires that {@code variable} has not been deleted (an exception may be thrown).
     */
    void markLowerBoundDirty(Variable variable) {
      if (variable.getId() < checkpoint) {
        dirtyLowerBounds.add(variable);
      }
    }

    /**
     * Track that the upper bound of {@code variable} has changed.
     *
     * <p>Requires that {@code variable} has not been deleted (an exception may be thrown).
     */
    void markUpperBoundDirty(Variable variable) {
      if (variable.getId() < checkpoint) {
        dirtyUpperBounds.add(variable);
      }
    }

    /**
     * Track that the integer status of {@code variable} has changed.
     *
     * <p>Requires that {@code variable} has not been deleted (an exception may be thrown).
     */
    void markIntegerBoundDirty(Variable variable) {
      if (variable.getId() < checkpoint) {
        dirtyIntegers.add(variable);
      }
    }

    /** Track that {@code variable} is now deleted from the model. */
    void deleteVariable(Variable variable) {
      if (variable.getId() < checkpoint) {
        deleted.add(variable.getId());
        dirtyLowerBounds.remove(variable);
        dirtyUpperBounds.remove(variable);
        dirtyIntegers.remove(variable);
      }
    }

    /**
     * Returns a description of all changes to the variables of the model since the checkpoint was
     * last advanced (or creation time if the checkpoint was never advanced).
     */
    UpdateResult update() {
      List<Long> deletes = new ArrayList<>(deleted);
      Collections.sort(deletes);
      VariableUpdatesProto.Builder updates = VariableUpdatesProto.newBuilder();
      updates.setLowerBounds(
          SparseContainers.toSparseDoubleVectorProto(dirtyLowerBounds, Variable::getLowerBound));
      updates.setUpperBounds(
          SparseContainers.toSparseDoubleVectorProto(dirtyUpperBounds, Variable::getUpperBound));
      updates.setIntegers(
          SparseContainers.toSparseBoolVectorProto(dirtyIntegers, Variable::isInteger));
      VariablesProto.Builder creates = VariablesProto.newBuilder();
      for (Variable newVar : newVariables()) {
        newVar.appendToProto(creates);
      }
      return new UpdateResult(deletes, updates.build(), creates.build());
    }
  }
}
