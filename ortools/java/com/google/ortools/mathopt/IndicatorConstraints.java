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
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import org.jspecify.annotations.Nullable;

/** Maintains the collection of indicator constraints in the model and tracks updates to them. */
final class IndicatorConstraints {
  private final ModelId modelId;
  private final Map<Long, IndicatorConstraint> indicatorConstraints = new HashMap<>();
  private final Collection<IndicatorConstraint> unmodifiableIndicatorConstraints =
      Collections.unmodifiableCollection(indicatorConstraints.values());
  private final Map<Variable, Set<IndicatorConstraint>> variableToConstraints = new HashMap<>();
  private long nextId = 0L;
  private final List<Diff> diffs = new ArrayList<>();

  IndicatorConstraints(ModelId modelId) {
    this.modelId = modelId;
  }

  IndicatorConstraint addIndicatorConstraint(Variable indicator, boolean activateOnZero,
      double lowerBound, double upperBound, LinearExpressionView expression, String name) {
    Map<Variable, Double> terms = new HashMap<>();
    for (Map.Entry<Variable, Double> entry : expression.unmodifiableTerms()) {
      terms.merge(entry.getKey(), entry.getValue(), Double::sum);
    }
    // We also need to account for the offset.
    // The constraint is lb <= terms + offset <= ub
    // This is equivalent to lb - offset <= terms <= ub - offset
    double offset = expression.offset();
    IndicatorConstraint constraint = new IndicatorConstraint(nextId, name, modelId, indicator,
        activateOnZero, lowerBound - offset, upperBound - offset, terms);
    indicatorConstraints.put(nextId, constraint);
    variableToConstraints.computeIfAbsent(indicator, k -> new HashSet<>()).add(constraint);
    for (Variable v : terms.keySet()) {
      variableToConstraints.computeIfAbsent(v, k -> new HashSet<>()).add(constraint);
    }
    nextId++;
    return constraint;
  }

  /** Returns the number of indicator constraints in the model. */
  int numIndicatorConstraints() {
    return indicatorConstraints.size();
  }

  /**
   * Checks that {@code constraint} is owned by the model, and throws an {@link
   * IllegalArgumentException} if not.
   */
  void checkOwned(IndicatorConstraint constraint) {
    Preconditions.checkArgument(constraint.getModelId() == modelId,
        "IndicatorConstraint %s is not from this model (named: %s), it is from another model"
            + " (named: %s).",
        constraint, modelId.getName(), constraint.getModelId().getName());
  }

  boolean hasIndicatorConstraintWithId(long id) {
    return indicatorConstraints.containsKey(id);
  }

  @Nullable
  IndicatorConstraint getIndicatorConstraint(long id) {
    return indicatorConstraints.get(id);
  }

  /** Returns the indicator constraints of the model, keyed by id. */
  Collection<IndicatorConstraint> getUnmodifiableIndicatorConstraints() {
    return unmodifiableIndicatorConstraints;
  }

  boolean deleteIndicatorConstraint(IndicatorConstraint constraint) {
    checkOwned(constraint);
    if (constraint.isDeleted()) {
      return false;
    }
    constraint.markDeleted();
    for (Diff diff : diffs) {
      diff.deleteIndicatorConstraint(constraint);
    }
    indicatorConstraints.remove(constraint.getId());
    if (constraint.getIndicatorVariable() != null) {
      Set<IndicatorConstraint> constraints =
          variableToConstraints.get(constraint.getIndicatorVariable());
      Preconditions.checkNotNull(constraints, "Variable %s not present in variableToConstraint",
          constraint.getIndicatorVariable());
      constraints.remove(constraint);
      if (constraints.isEmpty()) {
        variableToConstraints.remove(constraint.getIndicatorVariable());
      }
    }
    for (Variable v : constraint.getUnmodifiableImpliedTerms().keySet()) {
      Set<IndicatorConstraint> constraints = variableToConstraints.get(v);
      Preconditions.checkNotNull(
          constraints, "Variable %s not present in variableToConstraints", v);
      constraints.remove(constraint);
      if (constraints.isEmpty()) {
        variableToConstraints.remove(v);
      }
    }
    return true;
  }

  long getNextId() {
    return nextId;
  }

  void ensureNextIdAtLeast(long id) {
    nextId = max(nextId, id);
  }

  void onVariableDeleted(Variable variable) {
    if (variableToConstraints.containsKey(variable)) {
      for (IndicatorConstraint constraint :
          ImmutableList.copyOf(variableToConstraints.get(variable))) {
        constraint.onVariableDeleted(variable);
      }
      variableToConstraints.remove(variable);
    }
  }

  Diff addDiff() {
    var result = new Diff();
    diffs.add(result);
    return result;
  }

  void removeDiff(Diff diff) {
    Preconditions.checkArgument(diffs.remove(diff), "Diff not present, cannot remove it");
  }

  Map<Long, IndicatorConstraintProto> toProto() {
    Map<Long, IndicatorConstraintProto> result = new HashMap<>();
    for (IndicatorConstraint constraint : indicatorConstraints.values()) {
      result.put(constraint.getId(), constraint.toProto());
    }
    return result;
  }

  final class Diff {
    private long checkpoint;
    private final Set<Long> deleted = new HashSet<>();

    private Diff() {
      this.checkpoint = nextId;
    }

    void advance() {
      this.checkpoint = nextId;
      deleted.clear();
    }

    void deleteIndicatorConstraint(IndicatorConstraint constraint) {
      if (constraint.getId() < checkpoint) {
        deleted.add(constraint.getId());
      }
    }

    IndicatorConstraintUpdatesProto update() {
      ImmutableList<Long> deletes = ImmutableList.sortedCopyOf(deleted);
      Map<Long, IndicatorConstraintProto> creates = new HashMap<>();
      for (long newId = checkpoint; newId < nextId; newId++) {
        IndicatorConstraint constraint = indicatorConstraints.get(newId);
        if (constraint != null) {
          creates.put(newId, constraint.toProto());
        }
      }
      return IndicatorConstraintUpdatesProto.newBuilder()
          .addAllDeletedConstraintIds(deletes)
          .putAllNewConstraints(creates)
          .build();
    }
  }
}
