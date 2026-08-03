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

import static com.google.ortools.mathopt.Expressions.linExpr;

import com.google.common.collect.ImmutableMap;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/** The return value for the callback on {@link Solve#solve(Model, SolverType, Solve.Args)}. */
public final class CallbackResult {
  private boolean terminate = false;
  List<GeneratedLinearConstraint> newConstraints = new ArrayList<>();
  List<ImmutableMap<Variable, Double>> suggestedSolutions = new ArrayList<>();

  /** An empty result that does not request termination, suggest hints, or add constraints. */
  public CallbackResult() {}

  /** See {@link #getTerminate()}. */
  public CallbackResult setTerminate(boolean termiante) {
    this.terminate = termiante;
    return this;
  }

  /**
   * When true it tells the solver to interrupt the solve as soon as possible.
   *
   * <p>It can be set from any event. This is equivalent to using a {@link SolveInterrupter} and
   * triggering it from the callback.
   *
   * <p>Some solvers don't support interruption, in that case this is simply ignored and the solve
   * terminates as usual. On top of that solvers may not immediately stop the solve. Thus the user
   * should expect the callback to still be called after they set {@code terminate} to true in a
   * previous call. Returning with {@code terminate} false after having previously returned true
   * won't cancel the interruption.
   */
  public boolean getTerminate() {
    return terminate;
  }

  /**
   * Suggest a complete or partial solution to the solver, can be called from {@link
   * CallbackEvent#MIP_NODE} or {@link CallbackEvent#MIP_SOLUTION}.
   */
  public void suggestSolution(ImmutableMap<Variable, Double> variableValues) {
    suggestedSolutions.add(variableValues);
  }

  /**
   * Adds a "user cut" to the model, call only when {@link CallbackData#getEvent()} is {@link
   * CallbackEvent#MIP_NODE}.
   *
   * <p>A "user cut" is a linear constraint that excludes the current LP solution but does not make
   * any integer points infeasible. User cuts cannot affect the feasibility of a solution, they only
   * improve the quality of the LP relaxation. They can be discarded by the underlying solver,
   * immediately or at any point while solving.
   */
  public void addUserCut(
      double lowerBound, ImmutableMap<Variable, Double> linearTerms, double upperBound) {
    newConstraints.add(
        new GeneratedLinearConstraint(lowerBound, linearTerms, upperBound, /* isLazy= */ false));
  }

  /**
   * Adds the "user cut" lhs == rhs to the model.
   *
   * @see #addUserCut()
   */
  public void addEqUserCut(double lhs, LinearExpressionView rhs) {
    newConstraints.add(new GeneratedLinearConstraint(lhs, rhs, lhs, /* isLazy= */ false));
  }

  /**
   * Adds the "user cut" lhs == rhs to the model.
   *
   * @see #addUserCut()
   */
  public void addEqUserCut(LinearExpressionView lhs, double rhs) {
    newConstraints.add(new GeneratedLinearConstraint(rhs, lhs, rhs, /* isLazy= */ false));
  }

  /**
   * Adds the "user cut" lhs == rhs to the model.
   *
   * @see #addUserCut()
   */
  public void addEqUserCut(LinearExpressionView lhs, LinearExpressionView rhs) {
    addEqUserCut(linExpr().add(lhs).subtract(rhs), 0.0);
  }

  /**
   * Adds the "user cut" lhs <= rhs to the model.
   *
   * @see #addUserCut()
   */
  public void addLeUserCut(double lhs, LinearExpressionView rhs) {
    newConstraints.add(
        new GeneratedLinearConstraint(lhs, rhs, Double.POSITIVE_INFINITY, /* isLazy= */ false));
  }

  /**
   * Adds the "user cut" lhs <= rhs to the model.
   *
   * @see #addUserCut()
   */
  public void addLeUserCut(LinearExpressionView lhs, double rhs) {
    newConstraints.add(
        new GeneratedLinearConstraint(Double.NEGATIVE_INFINITY, lhs, rhs, /* isLazy= */ false));
  }

  /**
   * Adds the "user cut" lhs <= rhs to the model.
   *
   * @see #addUserCut()
   */
  public void addLeUserCut(LinearExpressionView lhs, LinearExpressionView rhs) {
    addLeUserCut(linExpr().add(lhs).subtract(rhs), 0.0);
  }

  /**
   * Adds the "user cut" lhs >= rhs to the model.
   *
   * @see #addUserCut()
   */
  public void addGeUserCut(double lhs, LinearExpressionView rhs) {
    newConstraints.add(
        new GeneratedLinearConstraint(Double.NEGATIVE_INFINITY, rhs, lhs, /* isLazy= */ false));
  }

  /**
   * Adds the "user cut" lhs >= rhs to the model.
   *
   * @see #addUserCut()
   */
  public void addGeUserCut(LinearExpressionView lhs, double rhs) {
    newConstraints.add(
        new GeneratedLinearConstraint(rhs, lhs, Double.POSITIVE_INFINITY, /* isLazy= */ false));
  }

  /**
   * Adds the "user cut" lhs >= rhs to the model.
   *
   * @see #addUserCut()
   */
  public void addGeUserCut(LinearExpressionView lhs, LinearExpressionView rhs) {
    addGeUserCut(linExpr().add(lhs).subtract(rhs), 0.0);
  }

  /**
   * Adds the "user cut" lowerBound <= expression <= upperBound to the model.
   *
   * @see #addUserCut()
   */
  public void addRangeUserCut(
      double lowerBound, LinearExpressionView expression, double upperBound) {
    newConstraints.add(
        new GeneratedLinearConstraint(lowerBound, expression, upperBound, /* isLazy= */ false));
  }

  /**
   * Adds a "lazy constraint" to the model, call only when {@link CallbackData#getEvent()} is {@link
   * CallbackEvent#MIP_NODE} or {@link CallbackEvent#MIP_SOLUTION}.
   *
   * <p>A "lazy constraint" is a linear constraint that can change the feasibility of an integer
   * solution. Lazy constraints may be discarded or temporarily ignored by the underlying solver,
   * but solvers must always give a final chance to mark solutions infeasible on event {@link
   * CallbackEvent#MIP_SOLUTION}.
   *
   * <p>{@link CallbackRegistration#getAddLazyConstraints()} must be true to add lazy constraints.
   */
  public void addLazyConstraint(
      double lowerBound, ImmutableMap<Variable, Double> linearTerms, double upperBound) {
    newConstraints.add(
        new GeneratedLinearConstraint(lowerBound, linearTerms, upperBound, /* isLazy= */ true));
  }

  /**
   * Adds the "lazy constraint" lhs == rhs to the model.
   *
   * @see #addLazyConstraint()
   */
  public void addEqLazyConstraint(double lhs, LinearExpressionView rhs) {
    newConstraints.add(new GeneratedLinearConstraint(lhs, rhs, lhs, /* isLazy= */ true));
  }

  /**
   * Adds the "lazy constraint" lhs == rhs to the model.
   *
   * @see #addLazyConstraint()
   */
  public void addEqLazyConstraint(LinearExpressionView lhs, double rhs) {
    newConstraints.add(new GeneratedLinearConstraint(rhs, lhs, rhs, /* isLazy= */ true));
  }

  /**
   * Adds the "lazy constraint" lhs == rhs to the model.
   *
   * @see #addLazyConstraint()
   */
  public void addEqLazyConstraint(LinearExpressionView lhs, LinearExpressionView rhs) {
    addEqLazyConstraint(linExpr().add(lhs).subtract(rhs), 0.0);
  }

  /**
   * Adds the "lazy constraint" lhs <= rhs to the model.
   *
   * @see #addLazyConstraint()
   */
  public void addLeLazyConstraint(double lhs, LinearExpressionView rhs) {
    newConstraints.add(
        new GeneratedLinearConstraint(lhs, rhs, Double.POSITIVE_INFINITY, /* isLazy= */ true));
  }

  /**
   * Adds the "lazy constraint" lhs <= rhs to the model.
   *
   * @see #addLazyConstraint()
   */
  public void addLeLazyConstraint(LinearExpressionView lhs, double rhs) {
    newConstraints.add(
        new GeneratedLinearConstraint(Double.NEGATIVE_INFINITY, lhs, rhs, /* isLazy= */ true));
  }

  /**
   * Adds the "lazy constraint" lhs <= rhs to the model.
   *
   * @see #addLazyConstraint()
   */
  public void addLeLazyConstraint(LinearExpressionView lhs, LinearExpressionView rhs) {
    addLeLazyConstraint(linExpr().add(lhs).subtract(rhs), 0.0);
  }

  /**
   * Adds the "lazy constraint" lhs >= rhs to the model.
   *
   * @see #addLazyConstraint()
   */
  public void addGeLazyConstraint(double lhs, LinearExpressionView rhs) {
    newConstraints.add(
        new GeneratedLinearConstraint(Double.NEGATIVE_INFINITY, rhs, lhs, /* isLazy= */ true));
  }

  /**
   * Adds the "lazy constraint" lhs >= rhs to the model.
   *
   * @see #addLazyConstraint()
   */
  public void addGeLazyConstraint(LinearExpressionView lhs, double rhs) {
    newConstraints.add(
        new GeneratedLinearConstraint(rhs, lhs, Double.POSITIVE_INFINITY, /* isLazy= */ true));
  }

  /**
   * Adds the "lazy constraint" lhs >= rhs to the model.
   *
   * @see #addLazyConstraint()
   */
  public void addGeLazyConstraint(LinearExpressionView lhs, LinearExpressionView rhs) {
    addGeLazyConstraint(linExpr().add(lhs).subtract(rhs), 0.0);
  }

  /**
   * Adds a "lazy constraint" lowerBound <= expression <= upperBound to the model.
   *
   * @see #addLazyConstraint()
   */
  public void addRangeLazyConstraint(
      double lowerBound, LinearExpressionView expression, double upperBound) {
    newConstraints.add(
        new GeneratedLinearConstraint(lowerBound, expression, upperBound, /* isLazy= */ true));
  }

  /** Returns a protocol buffer equivalent to {@code this}. */
  CallbackResultProto toProto() {
    var result = CallbackResultProto.newBuilder();
    result.setTerminate(terminate);
    for (ImmutableMap<Variable, Double> solution : suggestedSolutions) {
      result.addSuggestedSolutions(SparseContainers.toSparseDoubleVectorProto(solution));
    }
    for (GeneratedLinearConstraint constraint : newConstraints) {
      result.addCuts(constraint.toProto());
    }
    return result.build();
  }

  private static final class GeneratedLinearConstraint {
    final double lowerBound;

    /** Must not be modified. Triggers an exception otherwise. */
    final Iterable<Map.Entry<Variable, Double>> linearTerms;

    final double upperBound;
    final boolean isLazy;

    GeneratedLinearConstraint(double lowerBound, ImmutableMap<Variable, Double> linearTerms,
        double upperBound, boolean isLazy) {
      this.lowerBound = lowerBound;
      this.linearTerms = linearTerms.entrySet();
      this.upperBound = upperBound;
      this.isLazy = isLazy;
    }

    GeneratedLinearConstraint(double lowerBound, LinearExpressionView linearExpressionView,
        double upperBound, boolean isLazy) {
      this.lowerBound = lowerBound - linearExpressionView.offset();
      this.linearTerms = linearExpressionView.unmodifiableTerms();
      this.upperBound = upperBound - linearExpressionView.offset();
      this.isLazy = isLazy;
    }

    CallbackResultProto.GeneratedLinearConstraint toProto() {
      var result = CallbackResultProto.GeneratedLinearConstraint.newBuilder();
      result.setIsLazy(isLazy);
      result.setLowerBound(lowerBound);
      result.setUpperBound(upperBound);
      result.setLinearExpression(SparseContainers.toSparseDoubleVectorProto(linearTerms));
      return result.build();
    }
  }
}
