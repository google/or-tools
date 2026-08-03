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
import com.google.common.collect.ImmutableListMultimap;
import com.google.common.collect.ImmutableSet;
import com.google.ortools.mathopt.LinearConstraint.OnChangeListener;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import org.jspecify.annotations.Nullable;

/**
 * Maintains the collection of linear constraints in the model, the non-zeros of the constraint
 * matrix by column, and tracks updates to them.
 */
final class LinearConstraints {
  private final ModelId modelId;
  private final Map<Long, LinearConstraint> linearConstraints = new HashMap<>();
  private long nextId = 0L;
  private final List<Diff> diffs = new ArrayList<>();
  private final Map<Variable, Set<LinearConstraint>> columns = new HashMap<>();
  private final LinearConstraint.OnChangeListener constraintListener = new OnChangeListener() {
    @Override
    public void onLowerBoundChanged(LinearConstraint constraint) {
      for (Diff diff : diffs) {
        diff.markLowerBoundDirty(constraint);
      }
    }

    @Override
    public void onUpperBoundChanged(LinearConstraint constraint) {
      for (Diff diff : diffs) {
        diff.markUpperBoundDirty(constraint);
      }
    }

    @Override
    public void onTermChanged(LinearConstraint constraint, Variable variable, double coefficient) {
      if (coefficient != 0) {
        Set<LinearConstraint> column = columns.computeIfAbsent(variable, x -> new HashSet<>());
        column.add(constraint);
      } else {
        // Subtle: columns.get() below will always have a non-null map present (in fact, one
        // that contains `constraint` if `coefficient` is zero. We only provide notification
        // when coefficient has changed from the existing value, so it must have previously been
        // nonzero.
        columns.get(variable).remove(constraint);
      }
      for (Diff diff : diffs) {
        diff.markCoefficientDirty(constraint, variable);
      }
    }
  };

  LinearConstraints(ModelId modelId) {
    this.modelId = modelId;
  }

  /** Creates and returns a new empty unbounded linear constraint in the model. */
  LinearConstraint addLinearConstraint(String name) {
    return addLinearConstraint(Double.NEGATIVE_INFINITY, Double.POSITIVE_INFINITY, name);
  }

  /**
   * Creates and returns a new linear constraint with bounds [{@code lowerBound}, {@code
   * upperBound}] in the model.
   */
  LinearConstraint addLinearConstraint(double lowerBound, double upperBound, String name) {
    var result =
        new LinearConstraint(lowerBound, upperBound, nextId, name, constraintListener, modelId);
    linearConstraints.put(nextId, result);
    nextId++;
    return result;
  }

  /**
   * Creates and returns the linear constraint lhs == rhs in the model for constant lhs.
   *
   * <p>The returned constraint preserves the sign of the terms in {@code rhs}.
   */
  LinearConstraint addEq(double lhs, LinearExpressionView rhs, String name) {
    LinearConstraint result = addLinearConstraint(lhs, lhs, name);
    result.getTermsSink().add(rhs);
    return result;
  }

  /**
   * Creates and returns the linear constraint lhs == rhs in the model for constant rhs.
   *
   * <p>The returned constraint preserves the sign of the terms from {@code lhs}.
   */
  LinearConstraint addEq(LinearExpressionView lhs, double rhs, String name) {
    LinearConstraint result = addLinearConstraint(rhs, rhs, name);
    result.getTermsSink().add(lhs);
    return result;
  }

  /**
   * Creates and returns the linear constraint lhs == rhs in the model.
   *
   * <p>Letting b = rhs.offset - lhs.offset, implemented as b <= lhs.terms - rhs.terms <= b to
   * preserve the sign of the terms on lhs.
   *
   * <p>Warning: {@code addEq(new HashLinearExpression(3.0), rhs, name)} uses the opposite sign
   * convention of {@code addEq(3.0, rhs, name)}.
   */
  LinearConstraint addEq(LinearExpressionView lhs, LinearExpressionView rhs, String name) {
    LinearConstraint result = addLinearConstraint(0.0, 0.0, name);
    result.getTermsSink().add(lhs);
    result.getTermsSink().subtract(rhs);
    return result;
  }

  /**
   * Creates and returns the linear constraint lhs <= rhs in the model for constant lhs.
   *
   * <p>Implemented as lhs - rhs.offset <= rhs.terms <= +inf, to preserves the sign of the terms in
   * rhs.
   */
  LinearConstraint addLe(double lhs, LinearExpressionView rhs, String name) {
    LinearConstraint result = addLinearConstraint(lhs, Double.POSITIVE_INFINITY, name);
    result.getTermsSink().add(rhs);
    return result;
  }

  /**
   * Creates and returns the linear constraint lhs <= rhs in the model for constant rhs.
   *
   * <p>Implemented as -inf <= lhs.terms <= rhs - lhs.offset, to preserve the sign of the terms from
   * lhs.
   */
  LinearConstraint addLe(LinearExpressionView lhs, double rhs, String name) {
    LinearConstraint result = addLinearConstraint(Double.NEGATIVE_INFINITY, rhs, name);
    result.getTermsSink().add(lhs);
    return result;
  }

  /**
   * Creates and returns the linear constraint lhs <= rhs in the model.
   *
   * <p>Implemented as -inf <= lhs.terms - rhs.terms <= rhs.offset - lhs.offset to preserve the sign
   * of the terms from lhs.
   *
   * <p>Warning: {@code addLe(new HashLinearExpression(3.0), rhs, name)} uses the opposite sign
   * convention of {@code addLe(3.0, rhs, name)}.
   */
  LinearConstraint addLe(LinearExpressionView lhs, LinearExpressionView rhs, String name) {
    LinearConstraint result = addLinearConstraint(Double.NEGATIVE_INFINITY, 0.0, name);
    result.getTermsSink().add(lhs);
    result.getTermsSink().subtract(rhs);
    return result;
  }

  /**
   * Creates and returns the linear constraint lhs >= rhs in the model for constant lhs.
   *
   * <p>Implemented as -inf <= rhs.terms <= lhs - rhs.offset to preserve the sign of the terms on
   * rhs.
   */
  LinearConstraint addGe(double lhs, LinearExpressionView rhs, String name) {
    LinearConstraint result = addLinearConstraint(Double.NEGATIVE_INFINITY, lhs, name);
    result.getTermsSink().add(rhs);
    return result;
  }

  /**
   * Creates and returns the linear constraint lhs >= rhs in the model for constant rhs.
   *
   * <p>Implemented as rhs - lhs.offset <= lhs.terms <= +inf, to preserve the sign of the terms from
   * lhs.
   */
  LinearConstraint addGe(LinearExpressionView lhs, double rhs, String name) {
    LinearConstraint result = addLinearConstraint(rhs, Double.POSITIVE_INFINITY, name);
    result.getTermsSink().add(lhs);
    return result;
  }

  /**
   * Creates and returns the linear constraint lhs <= rhs in the model.
   *
   * <p>Implemented as rhs.offset - lhs.offset <= lhs.terms - rhs.terms <= +inf to preserve the sign
   * of the terms from lhs.
   *
   * <p>Warning: {@code addGe(new HashLinearExpression(3.0), rhs, name)} uses the opposite sign
   * convention of {@code addGe(3.0, rhs, name)}.
   */
  LinearConstraint addGe(LinearExpressionView lhs, LinearExpressionView rhs, String name) {
    LinearConstraint result = addLinearConstraint(0.0, Double.POSITIVE_INFINITY, name);
    result.getTermsSink().add(lhs);
    result.getTermsSink().subtract(rhs);
    return result;
  }

  /**
   * Creates and returns the linear constraint lowerBound <= expression <= upperBound in the model.
   *
   * <p>Implemented as lowerBound - expression.offset <= expression.terms <= upperBound -
   * expression.offset to preserve the sign of the terms from expression.
   */
  LinearConstraint addRange(
      double lowerBound, LinearExpressionView expression, double upperBound, String name) {
    LinearConstraint result = addLinearConstraint(lowerBound, upperBound, name);
    result.getTermsSink().add(expression);
    return result;
  }

  /** Returns the number of linear constraints in the model. */
  int numLinearConstraints() {
    return linearConstraints.size();
  }

  /**
   * Checks that {@code constraint} is owned by the model, and throws an {@link
   * IllegalArgumentException} if not (it is from another model).
   */
  void checkOwned(LinearConstraint constraint) {
    Preconditions.checkArgument(constraint.getModelId() == modelId,
        "LinearConstraint %s is not from this model (named: %s), it is from another model (named:"
            + " %s).",
        constraint, modelId.getName(), constraint.getModelId().getName());
  }

  /**
   * Returns true if the model contains a {@link LinearConstraint} with id of {@code id}, or false
   * otherwise (either the constraint never existed, or was deleted).
   */
  boolean hasLinearConstraintWithId(long id) {
    return linearConstraints.containsKey(id);
  }

  /**
   * Returns the {@link LinearConstraint} from the model with id of {@code id}, or null if there is
   * no such constraint (the constraint never existed or was deleted).
   *
   * <p>Prefer maintaining references to the {@link LinearConstraint} objects returned by {@link
   * #addLinearConstraint(String)} as this function can return null if {@code id} is invalid.
   */
  @Nullable
  LinearConstraint getLinearConstraint(long id) {
    return linearConstraints.get(id);
  }

  /**
   * Attempts to delete {@code constraint} from the model and return true on success.
   *
   * <p>Note: will return false only when {@code constraint} was previously deleted.
   *
   * @throws IllegalArgumentException if {@code constraint} is from another model.
   */
  boolean deleteLinearConstraint(LinearConstraint constraint) {
    checkOwned(constraint);
    if (constraint.isDeleted()) {
      return false;
    }
    constraint.markDeleted();
    for (Diff diff : diffs) {
      diff.deleteLinearConstraint(constraint);
    }
    for (Variable relatedVar : constraint.getUnmodifiableTerms().keySet()) {
      Set<LinearConstraint> column = columns.get(relatedVar);
      column.remove(constraint);
      if (column.isEmpty()) {
        columns.remove(relatedVar);
      }
    }
    linearConstraints.remove(constraint.getId());
    return true;
  }

  /**
   * Returns the id that will be assigned to the next {@link LinearConstraint} created.
   *
   * <p>All previously created linear constraints will have id less than this value.
   */
  long getNextId() {
    return nextId;
  }

  /** Makes the value of {@link #getNextId()} the max of the current value and {@code id}. */
  void ensureNextIdAtLeast(long id) {
    nextId = max(nextId, id);
  }

  /**
   * Returns an unmodifiable set of {@link LinearConstraint} objects for constraints where {@code
   * variable} has a nonzero coefficient.
   *
   * @throws IllegalArgumentException if {@code variable} is from another model
   */
  Set<LinearConstraint> getUnmodifiableColumnNonzeros(Variable variable) {
    checkVariableOwned(variable);
    return Collections.unmodifiableSet(columns.getOrDefault(variable, ImmutableSet.of()));
  }

  /**
   * Removes {@code variable} from all linear constraints, call when deleting {@code variable}.
   *
   * @throws IllegalArgumentException if {@code variable} is from another model
   */
  void onVariableDeleted(Variable variable) {
    checkVariableOwned(variable);
    // NOTE: variable may still appear in one of the diffs if it is before the checkpoint and its
    // value was set to zero. We cannot efficiently identify these variables here, so instead we
    // have to check in Diff.update() that the variable was not deleted.
    for (LinearConstraint constraint : columns.getOrDefault(variable, ImmutableSet.of())) {
      for (Diff diff : diffs) {
        // Checking the diff is faster than hashing
        if (diff.variableCheckpoint <= variable.getId() || diff.checkpoint <= constraint.getId()) {
          continue;
        }
        if (diff.dirtyCoefficients.containsKey(constraint)) {
          Set<Variable> dirtyVars = diff.dirtyCoefficients.get(constraint);
          dirtyVars.remove(variable);
          if (dirtyVars.isEmpty()) {
            diff.dirtyCoefficients.remove(constraint);
          }
        }
      }
      constraint.onVariableDeleted(variable);
    }
    columns.remove(variable);
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

  /**
   * Returns a protocol buffer representation of all the linear constraints in the model.
   *
   * <p>Note that the constraint matrix is not included in the returned value.
   *
   * @see #matrixProto()
   */
  LinearConstraintsProto toProto() {
    var proto = LinearConstraintsProto.newBuilder();
    linearConstraints.values()
        .stream()
        .sorted(Comparator.comparingLong(LinearConstraint::getId))
        .forEachOrdered(linearConstraint -> linearConstraint.appendToProto(proto));
    return proto.build();
  }

  /** Returns a protocol buffer representation of the constraint matrix. */
  SparseDoubleMatrixProto matrixProto() {
    var builder = SparseDoubleMatrixProto.newBuilder();
    linearConstraints.values()
        .stream()
        .sorted(Comparator.comparingLong(LinearConstraint::getId))
        .forEachOrdered(linearConstraint
            -> linearConstraint.getUnmodifiableTerms()
                .keySet()
                .stream()
                .sorted(Comparator.comparingLong(Variable::getId))
                .forEachOrdered(variable -> {
                  builder.addRowIds(linearConstraint.getId());
                  builder.addColumnIds(variable.getId());
                  builder.addCoefficients(linearConstraint.getTerm(variable));
                }));
    return builder.build();
  }

  /** Returns the {@link LinearConstraint}s of the model sorted by id. */
  ImmutableList<LinearConstraint> sortedLinearConstraints() {
    return ImmutableList.sortedCopyOf(
        Comparator.comparingLong(LinearConstraint::getId), linearConstraints.values());
  }

  private void checkVariableOwned(Variable variable) {
    Preconditions.checkArgument(variable.getModelId() == modelId,
        "Variable %s is not from this model (named: %s), it is from another model (named: %s).",
        variable, modelId.getName(), variable.getModelId().getName());
  }

  /**
   * A summary of a set of changes on {@link LinearConstraint} objects in a model.
   *
   * @see LinearConstraints.Diff#update(ImmutableSet, ImmutableList)
   */
  static final class UpdateResult {
    private final ImmutableList<Long> deletes;
    private final LinearConstraintUpdatesProto updates;
    private final LinearConstraintsProto creates;
    private final SparseDoubleMatrixProto matrix;

    /**
     * Creates a {@link LinearConstraints.UpdateResult}, typically prefer {@link
     * LinearConstraints.Diff#update(ImmutableSet, ImmutableList)}.
     */
    UpdateResult(ImmutableList<Long> deletes, LinearConstraintUpdatesProto updates,
        LinearConstraintsProto creates, SparseDoubleMatrixProto matrix) {
      this.deletes = deletes;
      this.updates = updates;
      this.creates = creates;
      this.matrix = matrix;
    }

    /** Sets this in {@code modelUpdateProto}. */
    void setInProto(ModelUpdateProto.Builder modelUpdateProto) {
      modelUpdateProto.addAllDeletedLinearConstraintIds(deletes);
      modelUpdateProto.setLinearConstraintUpdates(updates);
      modelUpdateProto.setNewLinearConstraints(creates);
      modelUpdateProto.setLinearConstraintMatrixUpdates(matrix);
    }
  }

  /**
   * Tracks the changes to {@link LinearConstraints} from a previous checkpoint to present.
   *
   * <p>The checkpoint is represented by the id used for the next {@link LinearConstraint} created,
   * from {@link LinearConstraints#nextId} and an id for the next {@link Variable} created. Changes
   * to the model on linear constraints with id at least checkpoint are not tracked explicitly (and
   * coefficient changes are not tracked if the variable or the constraint id are past the
   * checkpoint). Instead, we build a summary of changes in {@link #update(ImmutableSet,
   * ImmutableList)} with the tracked changes to linear constraints with id less than checkpoint,
   * and all linear constraints/coefficients created after the checkpoint. Calling {@link
   * #advance(long)} discards all tracked changes and updates the checkpoint.
   */
  final class Diff {
    private long checkpoint;
    private long variableCheckpoint;
    private final Set<Long> deleted = new HashSet<>();
    private final Set<LinearConstraint> dirtyLowerBounds = new HashSet<>();
    private final Set<LinearConstraint> dirtyUpperBounds = new HashSet<>();
    private final Map<LinearConstraint, Set<Variable>> dirtyCoefficients = new HashMap<>();

    /** Do not create directly, call {@link LinearConstraints#addDiff(long)} instead. */
    private Diff(long variableCheckpoint) {
      this.checkpoint = nextId;
      this.variableCheckpoint = variableCheckpoint;
    }

    /**
     * Clears existing tracked changes and track future changes on the enclosing {@link
     * LinearConstraints} from this point forward.
     */
    void advance(long variableCheckpoint) {
      this.checkpoint = nextId;
      this.variableCheckpoint = variableCheckpoint;
      deleted.clear();
      dirtyLowerBounds.clear();
      dirtyUpperBounds.clear();
      dirtyCoefficients.clear();
    }

    /** Track that {@code linearConstraint} is now deleted from the model. */
    void deleteLinearConstraint(LinearConstraint linearConstraint) {
      if (linearConstraint.getId() < checkpoint) {
        deleted.add(linearConstraint.getId());
        dirtyCoefficients.remove(linearConstraint);
      }
    }

    /**
     * Track that the lower bound of {@code linearConstraint} has changed.
     *
     * <p>Requires that {@code linearConstraint} has not been deleted (an exception may be thrown).
     */
    void markLowerBoundDirty(LinearConstraint linearConstraint) {
      if (linearConstraint.getId() < checkpoint) {
        dirtyLowerBounds.add(linearConstraint);
      }
    }

    /**
     * Track that the upper bound of {@code linearConstraint} has changed.
     *
     * <p>Requires that {@code linearConstraint} has not been deleted (an exception may be thrown).
     */
    void markUpperBoundDirty(LinearConstraint linearConstraint) {
      if (linearConstraint.getId() < checkpoint) {
        dirtyUpperBounds.add(linearConstraint);
      }
    }

    void markCoefficientDirty(LinearConstraint linearConstraint, Variable variable) {
      if (linearConstraint.getId() < checkpoint && variable.getId() < variableCheckpoint) {
        Set<Variable> dirty = dirtyCoefficients.computeIfAbsent(
            linearConstraint, (LinearConstraint k) -> new HashSet<>());
        dirty.add(variable);
      }
    }

    /**
     * Returns a description of all changes to the variables of the model since the checkpoint was
     * last advanced (or creation time if the checkpoint was never advanced).
     *
     * @param deletedVariables the ids of {@link Variable} objects with id less than {@link
     *     #variableCheckpoint} that were deleted since the previous checkpoint
     * @param newVariables the {@link Variable} objects that have been added to (and not deleted
     *     from) the model since the last checkpoint in sorted id order
     */
    UpdateResult update(ImmutableSet<Long> deletedVariables, ImmutableList<Variable> newVariables) {
      ImmutableList<Long> deletes = ImmutableList.sortedCopyOf(deleted);
      var updates = LinearConstraintUpdatesProto.newBuilder();
      updates.setLowerBounds(SparseContainers.toSparseDoubleVectorProto(
          dirtyLowerBounds, LinearConstraint::getLowerBound));
      updates.setUpperBounds(SparseContainers.toSparseDoubleVectorProto(
          dirtyUpperBounds, LinearConstraint::getUpperBound));
      var creates = LinearConstraintsProto.newBuilder();
      for (long newId = checkpoint; newId < nextId; newId++) {
        if (hasLinearConstraintWithId(newId)) {
          getLinearConstraint(newId).appendToProto(creates);
        }
      }
      return new UpdateResult(deletes, updates.build(), creates.build(),
          extractMatrixUpdates(deletedVariables, newVariables));
    }

    // The code below exports the terms of the constraint matrix. Note that there are constraints
    // that are before and after the checkpoint, and variables that are before and after
    // the checkpoint (referred to as "old" and "new" below) and whether or not a term is included
    // in the update depends on both if the variable is old and new and if the constraint is old
    // or new. In particular, we must include:
    //   (1) any term where both the variable and constraint are both old, the coefficient has
    //       changed, and neither the variable or constraint was deleted. In particular, if a
    //       coefficient was set to zero, we still include it.
    //   (2) any term where either the variable or constraint is new, neither was deleted, and the
    //       coefficient is nonzero. Note that the list of new variables is given as newVariables.
    // Last, we need to output the result in row major order, merging both these categories.
    //
    // We want this code to be efficient, as the constraint matrix and update can both be very
    // large (100M + entries). In particular, we want to avoid:
    //   * creating an object per entry returned, this will use too much memory
    //   * traversing the entire constraint matrix, this will be too slow when the update is
    //     much smaller than the matrix.
    //
    // The strategy is as follows:
    //   (1) For each old constraint that contains a term with a new variable, build a list of
    //       all new variables in order ("newVariableOldConstraints" below), and sort these
    //       constraints.
    //   (2) Get a sorted list of the old constraints where an old variable was modified.
    //   (3) Merge the sorted lists from (1) and (2), removing duplicates, there are all old
    //       constraints with any modification.
    //   (4) Loop over these constraints in order to fill in the changes to the constraint matrix.
    //       For each constraint:
    //         (a) add the terms for old variables with modifications (from 2)
    //         (b) add the terms for new variables (from 1)
    //   (5) Last, loop over all the new constraints, and all terms in variable sorted order.
    //
    // A few notes on the big-O complexity of this approach:
    //   * We loop over the new-variable, new-linear constraint terms twice, in (1) and (5). This
    //     is okay, as the extra work we are doing is still proportionate to the size of the
    //     output.
    //   * We loop over all the ids of linear constraints from the checkpoint. If many linear
    //     constraints have been deleted, this could be expensive (larger than the size of the
    //     output). However, if we only export the update O(1) times before calling advance, the
    //     cost can be amortized into the cost of deleting the constraint.
    private SparseDoubleMatrixProto extractMatrixUpdates(
        ImmutableSet<Long> deletedVariables, ImmutableList<Variable> newVariables) {
      // Step (1) For each constraint containing a new variable, holds the list of new variables in
      // order of id. Note that because newVariables is sorted, each inner list in
      // newVariableOldConstraints will be sorted as well.
      ImmutableListMultimap<LinearConstraint, Variable> newVariableOldConstraints =
          buildOldConstraintsToNewVariables(newVariables);
      ImmutableList<LinearConstraint> newVariableOldConstraintSortedKeys =
          ImmutableList.sortedCopyOf(Comparator.comparingLong(LinearConstraint::getId),
              newVariableOldConstraints.keySet());
      // Step (2): the list of old constraints where matrix elements have been updated for old
      // variables.
      ImmutableList<LinearConstraint> oldConstraintsWithOldVariableUpdates =
          ImmutableList.sortedCopyOf(
              Comparator.comparingLong(LinearConstraint::getId), dirtyCoefficients.keySet());
      // Step (3): merge to get all old constraints with any matrix updates
      ImmutableList<LinearConstraint> allOld = Merge.mergeSortedSkipDuplicates(
          newVariableOldConstraintSortedKeys, oldConstraintsWithOldVariableUpdates,
          Comparator.comparingLong(LinearConstraint::getId));
      // Step (4): add updates for the old constraints.
      var matrix = SparseDoubleMatrixProto.newBuilder();
      for (LinearConstraint existingConstraint : allOld) {
        if (deleted.contains(existingConstraint.getId())) {
          // Do not export changes to the constraint matrix for deleted constraints. These will
          // mostly be removed, but a few will remain, see comment in
          // LinearConstraints.onVariableDeleted().
          //
          // NOTE(user): with the current implementation, I believe it is actually not possible
          // for this line to execute. This is because we are currently storing the dirty matrix
          // entries as a Map<LinearConstraint, Set<Variable>> so we can efficiently remove all
          // linear constraints. If we instead stored the dirty entries as a set of
          // <LinearConstraint, Variable> pairs (as we do in C++) and relied on querying the model
          // to figure out which dirty entries to clean up, we would miss some and this would be
          // needed.
          continue;
        }
        // Step (4a)
        if (dirtyCoefficients.containsKey(existingConstraint)) {
          dirtyCoefficients.get(existingConstraint)
              .stream()
              .sorted(Comparator.comparingLong(Variable::getId))
              .forEachOrdered(variable -> {
                if (!deletedVariables.contains(variable.getId())) {
                  matrix.addRowIds(existingConstraint.getId());
                  matrix.addColumnIds(variable.getId());
                  matrix.addCoefficients(existingConstraint.getTerm(variable));
                }
              });
        }
        // Step (4b)
        if (newVariableOldConstraints.containsKey(existingConstraint)) {
          // These are already sorted
          for (Variable v : newVariableOldConstraints.get(existingConstraint)) {
            matrix.addRowIds(existingConstraint.getId());
            matrix.addColumnIds(v.getId());
            matrix.addCoefficients(existingConstraint.getTerm(v));
          }
        }
      }
      // Step (5) add the new constraints
      for (long linConId = checkpoint; linConId < nextId; linConId++) {
        LinearConstraint newCon = linearConstraints.get(linConId);
        if (newCon == null) {
          continue;
        }
        newCon.getUnmodifiableTerms()
            .keySet()
            .stream()
            .sorted(Comparator.comparingLong(Variable::getId))
            .forEachOrdered(variable -> {
              matrix.addRowIds(newCon.getId());
              matrix.addColumnIds(variable.getId());
              matrix.addCoefficients(newCon.getTerm(variable));
            });
      }
      return matrix.build();
    }

    // Step (1) for extractMatrixUpdates() above.
    private ImmutableListMultimap<LinearConstraint, Variable> buildOldConstraintsToNewVariables(
        ImmutableList<Variable> newVariables) {
      var result = ImmutableListMultimap.<LinearConstraint, Variable>builder();
      for (Variable v : newVariables) {
        for (LinearConstraint c : columns.getOrDefault(v, ImmutableSet.of())) {
          if (c.getId() < checkpoint) {
            result.put(c, v);
          }
        }
      }
      return result.build();
    }
  }
}
