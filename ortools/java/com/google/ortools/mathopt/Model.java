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

import com.google.common.base.Preconditions;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableSet;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import org.jspecify.annotations.Nullable;

/**
 * A mathematical optimization model for the MathOpt library.
 *
 * <p>As an example, to build the following optimization problem:
 *
 * <pre>{@code
 * max    2x + 5y
 * s.t.   x + y <= 1.5
 *        x in {0, 1}   (boolean)
 *        y in [0, 1]   (continuous)
 * }</pre>
 *
 * <p>Using the imports:
 *
 * <pre>{@code
 * import static com.google.ortools.mathopt.Expressions.linExpr;
 * import static com.google.ortools.mathopt.Expressions.sum;
 * import static java.util.Arrays.asList;
 *
 * import com.google.ortools.mathopt.LinearConstraint;
 * import com.google.ortools.mathopt.Model;
 * import com.google.ortools.mathopt.Variable;
 * }</pre>
 *
 * <p>You can build the model above with the code:
 *
 * <pre>{@code
 * var model = new Model("simple_model");
 * Variable x = model.addBinaryVariable("x");
 * Variable y = model.addContinuousVariable(0.0, 1.0, "y");
 * model.maximize(linExpr().add(2.0, x).add(5.0, y));
 * LinearConstraint c = model.addLe(sum(asList(x, y)), 1.5, "c");
 * }</pre>
 *
 * <p>You can also set the objective and constraints directly instead of building linear expressions
 * as follows:
 *
 * <pre>{@code
 * var model = new Model("simple_model");
 * Variable x = model.addBinaryVariable("x");
 * Variable y = model.addContinuousVariable(0.0, 1.0, "y");
 * model.getObjective().setToMaximize().setLinearTerm(x, 2.0).setLinearTerm(y, 5.0);
 * LinearConstraint c = model.addLinearConstraint("c").setUpperBound(1.5).setTerm(x, 1.0)
 *                           .setTerm(y, 1.0);
 * }</pre>
 *
 * <p>After building your model, you typically want to solve it to find the optimal values for your
 * decision variables, see {@link Solve#solve(Model, SolverType)} for details.
 *
 * <p>A model consists of decision variables, an objective, and constraints. The objective is to
 * either maximize or minimize some function of the decision variables, and the constraints restrict
 * the values the decision variables are allowed to take.
 *
 * <p>The decision variables are modeled with {@link Variable} objects that specify a basic domain
 * (potentially infinite upper and lower bounds, the option of restricting to integer values). The
 * {@link Objective} is a linear function of the decision variables. The most common constraints are
 * {@link LinearConstraints}, but other constraint types are supported.
 *
 * <p>MathOpt models allow variables, constraints, and objectives to be optionally named (or have
 * the empty string as a name). The names do not affect the solution process, but are included when
 * printing the models or exporting them to a standard file format. MathOpt will reject models at
 * solve time if two model components of the same type (e.g. two variables, two linear constraints)
 * have the same nonempty name (the primary and auxiliary objectives are considered to be of "the
 * same type").
 */
public final class Model {
  private final ModelId modelId;
  private final Variables variables;
  private final LinearConstraints linearConstraints;
  private final IndicatorConstraints indicatorConstraints;
  private final Objectives objectives;
  private final Set<UpdateTracker> updateTrackers = new HashSet<>();

  /**
   * Creates a new unnamed optimization model.
   *
   * <p>The primary objective is unnamed.
   */
  public Model() {
    this("", "");
  }

  /**
   * Creates a new optimization model named {@code name}.
   *
   * <p>The primary objective is unnamed.
   */
  public Model(String name) {
    this(name, "");
  }

  /**
   * Creates a new optimization model named {@code name} with a primary objective named {@code
   * primaryObjectiveName}.
   */
  public Model(String name, String primaryObjectiveName) {
    modelId = new ModelId(name);
    variables = new Variables(modelId);
    linearConstraints = new LinearConstraints(modelId);
    indicatorConstraints = new IndicatorConstraints(modelId);
    objectives = new Objectives(modelId, primaryObjectiveName);
  }

  /** Returns the name of the model, which is used for display and error messages. */
  public String getName() {
    return modelId.getName();
  }

  /**
   * Creates and returns a new unnamed integer {@link Variable} with bounds [0.0, 1.0] in the model.
   */
  public Variable addBinaryVariable() {
    return addVariable(
        /* lowerBound= */ 0.0, /* upperBound= */ 1.0, /* isInteger= */ true, /* name= */ "");
  }

  /** Creates and returns a new integer {@link Variable} with bounds [0.0, 1.0] in the model. */
  public Variable addBinaryVariable(String name) {
    return addVariable(/* lowerBound= */ 0.0, /* upperBound= */ 1.0, /* isInteger= */ true, name);
  }

  /** Creates and returns a new unnamed unbounded integer {@link Variable} in the model. */
  public Variable addIntegerVariable() {
    return addVariable(
        /* lowerBound= */ Double.NEGATIVE_INFINITY,
        /* upperBound= */ Double.POSITIVE_INFINITY,
        /* isInteger= */ true,
        /* name= */ "");
  }

  /** Creates and returns a new unbounded integer {@link Variable} in the model. */
  public Variable addIntegerVariable(String name) {
    return addVariable(
        /* lowerBound= */ Double.NEGATIVE_INFINITY,
        /* upperBound= */ Double.POSITIVE_INFINITY,
        /* isInteger= */ true, name);
  }

  /**
   * Creates and returns a new unnamed integer {@link Variable} with bounds [lowerBound, upperBound]
   * in the model.
   */
  public Variable addIntegerVariable(double lowerBound, double upperBound) {
    return addVariable(lowerBound, upperBound, /* isInteger= */ true, /* name= */ "");
  }

  /**
   * Creates and returns a new integer {@link Variable} with bounds [lowerBound, upperBound] in the
   * model.
   */
  public Variable addIntegerVariable(double lowerBound, double upperBound, String name) {
    return addVariable(lowerBound, upperBound, /* isInteger= */ true, name);
  }

  /** Creates and returns a new unnamed unbounded continuous {@link Variable} in the model. */
  public Variable addVariable() {
    return addVariable(
        /* lowerBound= */ Double.NEGATIVE_INFINITY,
        /* upperBound= */ Double.POSITIVE_INFINITY,
        /* isInteger= */ false,
        /* name= */ "");
  }

  /** Creates and returns a new unbounded continuous {@link Variable} in the model. */
  public Variable addVariable(String name) {
    return addVariable(
        /* lowerBound= */ Double.NEGATIVE_INFINITY,
        /* upperBound= */ Double.POSITIVE_INFINITY,
        /* isInteger= */ false, name);
  }

  /**
   * Creates and returns a new unnamed continuous {@link Variable} with bounds [lowerBound,
   * upperBound] in the model.
   */
  public Variable addVariable(double lowerBound, double upperBound) {
    return addVariable(lowerBound, upperBound, /* isInteger= */ false, /* name= */ "");
  }

  /**
   * Creates and returns a new continuous {@link Variable} with bounds [lowerBound, upperBound] in
   * the model.
   */
  public Variable addVariable(double lowerBound, double upperBound, String name) {
    return addVariable(lowerBound, upperBound, /* isInteger= */ false, name);
  }

  /**
   * Creates and returns a new unnamed {@link Variable} with bounds [lowerBound, upperBound] that is
   * restricted to integer values if {@code isInteger} is true (otherwise the variable is
   * continuous) in the model.
   *
   * <p>Most users should prefer one of the more specific methods {@link #addBinaryVariable()},
   * {@link #addVariable(double, double)}, or {@link #addIntegerVariable(double, double)}, or
   * writing "fluent" style code, e.g.: {@code Variable x =
   * model.addVariable().setLowerBound(0.0).setUpperBound(1.0);}
   */
  public Variable addVariable(double lowerBound, double upperBound, boolean isInteger) {
    return variables.addVariable(lowerBound, upperBound, isInteger, /* name= */ "");
  }

  /**
   * Creates and returns a new {@link Variable} with bounds [lowerBound, upperBound] that is
   * restricted to integer values if {@code isInteger} is true (otherwise the variable is
   * continuous) in the model.
   *
   * <p>Most users should prefer one of the more specific methods {@link
   * #addBinaryVariable(String)}, {@link #addVariable(double, double, String)}, or {@link
   * #addIntegerVariable(double, double, String)}, or writing "fluent" style code, e.g.: {@code
   * Variable x = model.addVariable("x").setLowerBound(0.0).setUpperBound(1.0);}
   */
  public Variable addVariable(
      double lowerBound, double upperBound, boolean isInteger, String name) {
    return variables.addVariable(lowerBound, upperBound, isInteger, name);
  }

  /** Returns the number of variables in the model. */
  public int numVariables() {
    return variables.numVariables();
  }

  /**
   * Returns the id that will be assigned to the next {@link Variable} created.
   *
   * <p>All previously created variables will have id less than this value.
   */
  long getNextVariableId() {
    return variables.getNextId();
  }

  /**
   * Returns true if the model contains a {@link Variable} with id of {@code id}, or false otherwise
   * (either the variable never existed, or was deleted).
   */
  public boolean hasVariableWithId(long id) {
    return variables.hasVariableWithId(id);
  }

  /**
   * Returns the {@link Variable} from the model with id of {@code id}, or null if there is no such
   * variable (the variable never existed or was deleted).
   *
   * <p>Prefer maintaining references to the {@link Variable} objects returned by {@link
   * #addVariable(double, double, boolean, String)} as this function can return null if {@code id}
   * is invalid.
   */
  public @Nullable Variable getVariable(long id) {
    return variables.getVariable(id);
  }

  /**
   * Attempts to delete {@code variable} from the model and returns true on success.
   *
   * <p>Note: will return false only when {@code variable} was previously deleted.
   *
   * @throws IllegalArgumentException if {@code variable} is from another model.
   */
  public boolean deleteVariable(Variable variable) {
    objectives.onVariableDeleted(variable);
    linearConstraints.onVariableDeleted(variable);
    indicatorConstraints.onVariableDeleted(variable);
    return variables.deleteVariable(variable);
  }

  /** Returns the primary objective for this model. */
  public Objective getObjective() {
    return objectives.getPrimaryObjective();
  }

  /**
   * Sets the primary objective to {@code linearExpression} with direction {@code isMaximize}.
   *
   * @throws IllegalArgumentException if any variable in {@code linearExpression} is from another
   *     model or was deleted, or if the objective was deleted.
   */
  public void setObjective(LinearExpressionView linearExpression, boolean isMaximize) {
    Objective obj = getObjective();
    obj.setMaximize(isMaximize);
    obj.replace(linearExpression);
  }

  /**
   * Sets the primary objective to maximize {@code linearExpression}.
   *
   * @throws IllegalArgumentException if any variable in {@code linearExpression} is from another
   *     model or was deleted, or if the objective was deleted.
   */
  public void maximize(LinearExpressionView linearExpression) {
    setObjective(linearExpression, /* isMaximize= */ true);
  }

  /**
   * Sets the primary objective to minimize {@code linearExpression}.
   *
   * @throws IllegalArgumentException if any variable in {@code linearExpression} is from another
   *     model or was deleted, or if the objective was deleted.
   */
  public void minimize(LinearExpressionView linearExpression) {
    setObjective(linearExpression, /* isMaximize= */ false);
  }

  /**
   * Creates and returns a new unnamed {@link AuxiliaryObjective} with priority one above the
   * largest priority of all objectives in the model.
   *
   * <p>Note: this runs in time linear in the number of objectives in the model.
   */
  public AuxiliaryObjective addAuxiliaryObjective() {
    return objectives.addAuxiliaryObjective("");
  }

  /**
   * Creates and returns a new {@link AuxiliaryObjective} named {@code name} with priority one above
   * the largest priority of all objectives in the model.
   *
   * <p>Note: this runs in time linear in the number of objectives in the model.
   */
  public AuxiliaryObjective addAuxiliaryObjective(String name) {
    return objectives.addAuxiliaryObjective(name);
  }

  /**
   * Creates and returns a new {@link AuxiliaryObjective} named {@code name} with priority {@code
   * priority}.
   *
   * <p>The priorities of the objectives in the model must be distinct at solve time, or the model
   * will be rejected.
   *
   * <p>Note: this runs in amortized O(1).
   */
  public AuxiliaryObjective addAuxiliaryObjective(long priority, String name) {
    return objectives.addAuxiliaryObjective(priority, name);
  }

  /** Returns the number of objectives in the model (primary plus auxiliary). */
  public int numObjectives() {
    return objectives.numObjectives();
  }

  /**
   * Returns the id that will be assigned to the next {@link AuxiliaryObjective} created.
   *
   * <p>All previously created auxiliary objectives have id less than this value.
   */
  public long getNextAuxiliaryObjectiveId() {
    return objectives.getNextId();
  }

  /**
   * Returns true if the model contains a {@link AuxiliaryObjective} with id of {@code id}, or false
   * otherwise (either the objective never existed, or was deleted).
   */
  public boolean hasAuxiliaryObjectiveWithId(long id) {
    return objectives.hasAuxiliaryObjectiveWithId(id);
  }

  /**
   * Returns the {@link AuxiliaryObjective} from the model with id of {@code id}, or null if there
   * is no such objective (the objective never existed or was deleted).
   *
   * <p>Prefer maintaining references to the {@link AuxiliaryObjective} objects returned by {@link
   * #addAuxiliaryObjective()} as this function can return null if {@code id} is invalid.
   */
  public @Nullable AuxiliaryObjective getAuxiliaryObjective(long id) {
    return objectives.getAuxiliaryObjective(id);
  }

  /**
   * Attempts to delete {@code objective} from the model and return true on success.
   *
   * <p>Note: will return false only when {@code objective} was previously deleted.
   *
   * @throws IllegalArgumentException if {@code objective} is from another model.
   */
  public boolean deleteAuxiliaryObjective(AuxiliaryObjective objective) {
    return objectives.deleteAuxiliaryObjective(objective);
  }

  /** Creates and returns a new unnamed, unbounded, empty {@link LinearConstraint} in the model. */
  public LinearConstraint addLinearConstraint() {
    return addLinearConstraint("");
  }

  /**
   * Creates and returns a new unbounded, empty {@link LinearConstraint} in the model named {@code
   * name}.
   */
  public LinearConstraint addLinearConstraint(String name) {
    return addLinearConstraint(
        /* lowerBound= */ Double.NEGATIVE_INFINITY,
        /* upperBound= */ Double.POSITIVE_INFINITY, name);
  }

  /**
   * Creates and returns a new unnamed {@link LinearConstraint} in the model with bounds
   * [lowerBound, upperBound].
   */
  public LinearConstraint addLinearConstraint(double lowerBound, double upperBound) {
    return addLinearConstraint(lowerBound, upperBound, "");
  }

  /**
   * Creates and returns a new {@link LinearConstraint} in the model with bounds [lowerBound,
   * upperBound] named {@code name}.
   */
  public LinearConstraint addLinearConstraint(double lowerBound, double upperBound, String name) {
    return linearConstraints.addLinearConstraint(lowerBound, upperBound, name);
  }

  /**
   * Creates and returns the unnamed linear constraint lhs == rhs in the model for constant lhs.
   *
   * <p>The returned constraint preserves the sign of the terms in {@code rhs}.
   */
  public LinearConstraint addEq(double lhs, LinearExpressionView rhs) {
    return linearConstraints.addEq(lhs, rhs, "");
  }

  /**
   * Creates and returns the linear constraint lhs == rhs in the model for constant lhs named {@code
   * name}.
   *
   * <p>The returned constraint preserves the sign of the terms in {@code rhs}.
   */
  public LinearConstraint addEq(double lhs, LinearExpressionView rhs, String name) {
    return linearConstraints.addEq(lhs, rhs, name);
  }

  /**
   * Creates and returns the unnamed linear constraint lhs == rhs in the model for constant rhs.
   *
   * <p>The returned constraint preserves the sign of the terms from {@code lhs}.
   */
  public LinearConstraint addEq(LinearExpressionView lhs, double rhs) {
    return linearConstraints.addEq(lhs, rhs, "");
  }

  /**
   * Creates and returns the linear constraint lhs == rhs in the model for constant rhs with name
   * {@code name}.
   *
   * <p>The returned constraint preserves the sign of the terms from {@code lhs}.
   */
  public LinearConstraint addEq(LinearExpressionView lhs, double rhs, String name) {
    return linearConstraints.addEq(lhs, rhs, name);
  }

  /**
   * Creates and returns the unnamed linear constraint lhs == rhs in the model.
   *
   * <p>Letting b = rhs.offset - lhs.offset, implemented as b <= lhs.terms - rhs.terms <= b to
   * preserve the sign of the terms on lhs.
   *
   * <p>Warning: {@code addEq(new HashLinearExpression(3.0), rhs)} uses the opposite sign *
   * convention of {@code addEq(3.0, rhs)}.
   */
  public LinearConstraint addEq(LinearExpressionView lhs, LinearExpressionView rhs) {
    return linearConstraints.addEq(lhs, rhs, "");
  }

  /**
   * Creates and returns the linear constraint lhs == rhs in the model with name {@code name}.
   *
   * <p>Letting b = rhs.offset - lhs.offset, implemented as b <= lhs.terms - rhs.terms <= b to
   * preserve the sign of the terms on lhs.
   *
   * <p>Warning: {@code addEq(new HashLinearExpression(3.0), rhs, name)} uses the opposite sign *
   * convention of {@code addEq(3.0, rhs, name)}.
   */
  public LinearConstraint addEq(LinearExpressionView lhs, LinearExpressionView rhs, String name) {
    return linearConstraints.addEq(lhs, rhs, name);
  }

  /**
   * Creates and returns the unnamed linear constraint lhs <= rhs in the model for constant lhs.
   *
   * <p>Implemented as lhs - rhs.offset <= rhs.terms <= +inf, to preserves the sign of the terms in
   * rhs.
   */
  public LinearConstraint addLe(double lhs, LinearExpressionView rhs) {
    return linearConstraints.addLe(lhs, rhs, "");
  }

  /**
   * Creates and returns the linear constraint lhs <= rhs in the model for constant lhs.
   *
   * <p>Implemented as lhs - rhs.offset <= rhs.terms <= +inf, to preserves the sign of the terms in
   * rhs.
   */
  public LinearConstraint addLe(double lhs, LinearExpressionView rhs, String name) {
    return linearConstraints.addLe(lhs, rhs, name);
  }

  /**
   * Creates and returns the unnamed linear constraint lhs <= rhs in the model for constant rhs.
   *
   * <p>Implemented as -inf <= lhs.terms <= rhs - lhs.offset, to preserve the sign of the terms from
   * lhs.
   */
  public LinearConstraint addLe(LinearExpressionView lhs, double rhs) {
    return linearConstraints.addLe(lhs, rhs, "");
  }

  /**
   * Creates and returns the linear constraint lhs <= rhs in the model for constant rhs with name
   * {@code name}.
   *
   * <p>Implemented as -inf <= lhs.terms <= rhs - lhs.offset, to preserve the sign of the terms from
   * lhs.
   */
  public LinearConstraint addLe(LinearExpressionView lhs, double rhs, String name) {
    return linearConstraints.addLe(lhs, rhs, name);
  }

  /**
   * Creates and returns the unnamed linear constraint lhs <= rhs in the model.
   *
   * <p>Implemented as -inf <= lhs.terms - rhs.terms <= rhs.offset - lhs.offset to preserve the sign
   * of the terms from lhs.
   *
   * <p>Warning: {@code addLe(new HashLinearExpression(3.0), rhs)} uses the opposite sign convention
   * of {@code addLe(3.0, rhs)}.
   */
  public LinearConstraint addLe(LinearExpressionView lhs, LinearExpressionView rhs) {
    return linearConstraints.addLe(lhs, rhs, "");
  }

  /**
   * Creates and returns the linear constraint lhs <= rhs in the model with name {@code name}.
   *
   * <p>Implemented as -inf <= lhs.terms - rhs.terms <= rhs.offset - lhs.offset to preserve the sign
   * of the terms from lhs.
   *
   * <p>Warning: {@code addLe(new HashLinearExpression(3.0), rhs, name)} uses the opposite sign
   * convention of {@code addLe(3.0, rhs, name)}.
   */
  public LinearConstraint addLe(LinearExpressionView lhs, LinearExpressionView rhs, String name) {
    return linearConstraints.addLe(lhs, rhs, name);
  }

  /**
   * Creates and returns the unnamed linear constraint lhs >= rhs in the model for constant lhs.
   *
   * <p>Implemented as -inf <= rhs.terms <= lhs - rhs.offset to preserve the sign of the terms on
   * rhs.
   */
  public LinearConstraint addGe(double lhs, LinearExpressionView rhs) {
    return linearConstraints.addGe(lhs, rhs, "");
  }

  /**
   * Creates and returns the linear constraint lhs >= rhs in the model for constant lhs with name
   * {@code name}.
   *
   * <p>Implemented as -inf <= rhs.terms <= lhs - rhs.offset to preserve the sign of the terms on
   * rhs.
   */
  public LinearConstraint addGe(double lhs, LinearExpressionView rhs, String name) {
    return linearConstraints.addGe(lhs, rhs, name);
  }

  /**
   * Creates and returns the unnamed linear constraint lhs >= rhs in the model for constant rhs.
   *
   * <p>Implemented as rhs - lhs.offset <= lhs.terms <= +inf, to preserve the sign of the terms from
   * lhs.
   */
  public LinearConstraint addGe(LinearExpressionView lhs, double rhs) {
    return linearConstraints.addGe(lhs, rhs, "");
  }

  /**
   * Creates and returns the linear constraint lhs >= rhs in the model for constant rhs with name
   * {@code name}.
   *
   * <p>Implemented as rhs - lhs.offset <= lhs.terms <= +inf, to preserve the sign of the terms from
   * lhs.
   */
  public LinearConstraint addGe(LinearExpressionView lhs, double rhs, String name) {
    return linearConstraints.addGe(lhs, rhs, name);
  }

  /**
   * Creates and returns the unnamed linear constraint lhs <= rhs in the model.
   *
   * <p>Implemented as rhs.offset - lhs.offset <= lhs.terms - rhs.terms <= +inf to preserve the sign
   * of the terms from lhs.
   *
   * <p>Warning: {@code addGe(new HashLinearExpression(3.0), rhs)} uses the opposite sign convention
   * of {@code addGe(3.0, rhs)}.
   */
  public LinearConstraint addGe(LinearExpressionView lhs, LinearExpressionView rhs) {
    return linearConstraints.addGe(lhs, rhs, "");
  }

  /**
   * Creates and returns the linear constraint lhs <= rhs in the model with name {@code name}.
   *
   * <p>Implemented as rhs.offset - lhs.offset <= lhs.terms - rhs.terms <= +inf to preserve the sign
   * of the terms from lhs.
   *
   * <p>Warning: {@code addGe(new HashLinearExpression(3.0), rhs, name)} uses the opposite sign *
   * convention of {@code addGe(3.0, rhs, name)}.
   */
  public LinearConstraint addGe(LinearExpressionView lhs, LinearExpressionView rhs, String name) {
    return linearConstraints.addGe(lhs, rhs, name);
  }

  /**
   * Creates and returns the unnamed linear constraint lowerBound <= expression <= upperBound in the
   * model.
   *
   * <p>Implemented as lowerBound - expression.offset <= expression.terms <= upperBound -
   * expression.offset to preserve the sign of the terms from expression.
   */
  public LinearConstraint addRange(
      double lowerBound, LinearExpressionView expression, double upperBound) {
    return linearConstraints.addRange(lowerBound, expression, upperBound, "");
  }

  /**
   * Creates and returns the linear constraint lowerBound <= expression <= upperBound in the model
   * with name {@code name}.
   *
   * <p>Implemented as lowerBound - expression.offset <= expression.terms <= upperBound -
   * expression.offset to preserve the sign of the terms from expression.
   */
  public LinearConstraint addRange(
      double lowerBound, LinearExpressionView expression, double upperBound, String name) {
    return linearConstraints.addRange(lowerBound, expression, upperBound, name);
  }

  /** Returns the number of linear constraints in the model. */
  public int numLinearConstraint() {
    return linearConstraints.numLinearConstraints();
  }

  /**
   * Returns the id that will be assigned to the next {@link LinearConstraint} created.
   *
   * <p>All previously created linear constraints will have id less than this value.
   */
  long getNextLinearConstraintId() {
    return linearConstraints.getNextId();
  }

  /**
   * Returns the id that will be assigned to the next {@link IndicatorConstraint} created.
   *
   * <p>All previously created indicator constraints will have id less than this value.
   */
  long getNextIndicatorConstraintId() {
    return indicatorConstraints.getNextId();
  }

  /**
   * Returns true if the model contains a {@link LinearConstraint} with id of {@code id}, or false
   * otherwise (either the variable never existed, or was deleted).
   */
  public boolean hasLinearConstraintWithId(long id) {
    return linearConstraints.hasLinearConstraintWithId(id);
  }

  /**
   * Returns the {@link LinearConstraint} from the model with id of {@code id}, or null if there is
   * no such linear constraint (the constraint never existed or was deleted).
   *
   * <p>Prefer maintaining references to the {@link LinearConstraint} objects returned by {@link
   * #addLinearConstraint(double, double, String)} as this function can return null if {@code id} is
   * invalid.
   */
  public @Nullable LinearConstraint getLinearConstraint(long id) {
    return linearConstraints.getLinearConstraint(id);
  }

  /**
   * Attempts to delete {@code linearConstraint} from the model and returns true on success.
   *
   * <p>Note: will return false only when {@code linearConstraint} was previously deleted.
   *
   * @throws IllegalArgumentException if {@code linearConstraint} is from another model.
   */
  public boolean deleteLinearConstraint(LinearConstraint linearConstraint) {
    return linearConstraints.deleteLinearConstraint(linearConstraint);
  }

  /**
   * Creates and returns a new indicator lesser than or equal to constraint.
   *
   * <p>The constraint states that if {@code indicator} is 1 (if {@code activateOnZero} is false) or
   * 0 (if {@code activateOnZero} is true), then {@code leftHandSide} <= {@code rightHandSide} must
   * hold.
   *
   * @throws IllegalArgumentException if any variable in {@code expression} or {@code indicator} is
   *     from another model or was deleted.
   * @throws NullPointerException if {@code indicator} is null.
   */
  public IndicatorConstraint addLeIndicatorConstraint(Variable indicator, boolean activateOnZero,
      LinearExpressionView leftHandSide, double rightHandSide, String name) {
    Preconditions.checkNotNull(indicator,
        "When creating an indicator constraint, the indicator variable must not be null");
    return addIndicatorConstraint(
        indicator, activateOnZero, Double.NEGATIVE_INFINITY, rightHandSide, leftHandSide, name);
  }

  /**
   * Creates and returns a new indicator greater than or equal to constraint.
   *
   * <p>The constraint states that if {@code indicator} is 1 (if {@code activateOnZero} is false) or
   * 0 (if {@code activateOnZero} is true), then {@code leftHandSide} >= {@code rightHandSide} must
   * hold.
   *
   * @throws IllegalArgumentException if any variable in {@code expression} or {@code indicator} is
   *     from another model or was deleted.
   * @throws NullPointerException if {@code indicator} is null.
   */
  public IndicatorConstraint addGeIndicatorConstraint(Variable indicator, boolean activateOnZero,
      LinearExpressionView leftHandSide, double rightHandSide, String name) {
    Preconditions.checkNotNull(indicator,
        "When creating an indicator constraint, the indicator variable must not be null");
    return addIndicatorConstraint(
        indicator, activateOnZero, rightHandSide, Double.POSITIVE_INFINITY, leftHandSide, name);
  }

  /**
   * Creates and returns a new indicator equality constraint.
   *
   * <p>The constraint states that if {@code indicator} is 1 (if {@code activateOnZero} is false) or
   * 0 (if {@code activateOnZero} is true), then {@code leftHandSide} == {@code rightHandSide} must
   * hold.
   *
   * @throws IllegalArgumentException if any variable in {@code expression} or {@code indicator} is
   *     from another model or was deleted.
   * @throws NullPointerException if {@code indicator} is null.
   */
  public IndicatorConstraint addEqIndicatorConstraint(Variable indicator, boolean activateOnZero,
      LinearExpressionView leftHandSide, double rightHandSide, String name) {
    Preconditions.checkNotNull(indicator,
        "When creating an indicator constraint, the indicator variable must not be null");
    return addIndicatorConstraint(
        indicator, activateOnZero, rightHandSide, rightHandSide, leftHandSide, name);
  }

  /**
   * Private internal method that creates and returns a new indicator constraint.
   *
   * <p>The constraint states that if {@code indicator} is 1 (if {@code activateOnZero} is false) or
   * 0 (if {@code activateOnZero} is true), then {@code lowerBound} <= {@code expression} <= {@code
   * upperBound} must hold.
   *
   * <p>This method permits null indicator variables as it is used internally when reading indicator
   * constraints with deleted indicator variables from proto messages.
   *
   * @throws IllegalArgumentException if any variable in {@code expression} or {@code indicator} is
   *     from another model or was deleted.
   */
  private IndicatorConstraint addIndicatorConstraint(Variable indicator, boolean activateOnZero,
      double lowerBound, double upperBound, LinearExpressionView expression, String name) {
    if (indicator != null) {
      checkOwned(indicator);
      Preconditions.checkArgument(!indicator.isDeleted(),
          "Indicator variable %s is deleted and should not be used to create an indicator"
              + " constraint",
          indicator);
    }
    Preconditions.checkNotNull(
        expression, "When creating an indicator constraint, the expression must not be null");
    for (Map.Entry<Variable, Double> entry : expression.unmodifiableTerms()) {
      checkOwned(entry.getKey());
      Preconditions.checkArgument(
          !entry.getKey().isDeleted(), "Variable %s in expression is deleted", entry.getKey());
    }
    return indicatorConstraints.addIndicatorConstraint(
        indicator, activateOnZero, lowerBound, upperBound, expression, name);
  }

  /** Returns the number of indicator constraints in the model. */
  public int numIndicatorConstraints() {
    return indicatorConstraints.numIndicatorConstraints();
  }

  /**
   * Returns true if the model contains an {@link IndicatorConstraint} with id of {@code id}, or
   * false otherwise (either the constraint never existed, or was deleted).
   */
  public boolean hasIndicatorConstraintWithId(long id) {
    return indicatorConstraints.hasIndicatorConstraintWithId(id);
  }

  /**
   * Returns the {@link IndicatorConstraint} from the model with id of {@code id}, or null if there
   * is no such constraint (the constraint never existed or was deleted).
   */
  public @Nullable IndicatorConstraint getIndicatorConstraint(long id) {
    return indicatorConstraints.getIndicatorConstraint(id);
  }

  /**
   * Returns the {@link IndicatorConstraint} from the model with id of {@code id}, or null if there
   * is no such constraint (the constraint never existed or was deleted).
   */
  public @Nullable Collection<IndicatorConstraint> getUnmodifiableIndicatorConstraints() {
    return indicatorConstraints.getUnmodifiableIndicatorConstraints();
  }

  /**
   * Attempts to delete {@code indicatorConstraint} from the model and returns true on success.
   *
   * <p>Note: will return false only when {@code indicatorConstraint} was previously deleted.
   *
   * @throws IllegalArgumentException if {@code indicatorConstraint} is from another model.
   */
  public boolean deleteIndicatorConstraint(IndicatorConstraint indicatorConstraint) {
    return indicatorConstraints.deleteIndicatorConstraint(indicatorConstraint);
  }

  /**
   * Returns an unmodifiable set of {@link LinearConstraint} objects for constraints where {@code
   * variable} has a nonzero coefficient.
   *
   * @throws IllegalArgumentException if {@code variable} is from another model
   */
  public Set<LinearConstraint> getUnmodifiableColumnNonzeros(Variable variable) {
    return linearConstraints.getUnmodifiableColumnNonzeros(variable);
  }

  /**
   * Checks that {@code variable} is owned by the model, and throws an {@link
   * IllegalArgumentException} if not (it is from another model).
   *
   * <p>Calling this on a deleted variable from this model will not throw an exception.
   */
  void checkOwned(Variable variable) {
    variables.checkOwned(variable);
  }

  /**
   * Checks that {@code linearConstraint} is owned by the model, and throws an {@link
   * IllegalArgumentException} if not (it is from another model).
   *
   * <p>Calling this on a deleted linear constraint from this model will not throw an exception.
   */
  void checkOwned(LinearConstraint linearConstraint) {
    linearConstraints.checkOwned(linearConstraint);
  }

  /**
   * Checks that {@code objective} is owned by the model, and throws an {@link
   * IllegalArgumentException} if not (it is from another model).
   *
   * <p>Calling this on a deleted {@link AuxiliaryObjective} from this model will not throw an
   * exception.
   */
  void checkOwned(Objective objective) {
    objectives.checkOwned(objective);
  }

  /**
   * Checks that {@code indicatorConstraint} is owned by the model, and throws an {@link
   * IllegalArgumentException} if not (it is from another model).
   *
   * <p>Calling this on a deleted indicator constraint from this model will not throw an exception.
   */
  void checkOwned(IndicatorConstraint indicatorConstraint) {
    indicatorConstraints.checkOwned(indicatorConstraint);
  }

  /** Returns a protocol buffer describing the entire model. */
  public ModelProto toProto() {
    return ModelProto.newBuilder()
        .setName(modelId.getName())
        .setVariables(variables.toProto())
        .setObjective(objectives.getPrimaryObjective().toProto())
        .putAllAuxiliaryObjectives(objectives.auxiliaryObjectivesToProto())
        .setLinearConstraints(linearConstraints.toProto())
        .setLinearConstraintMatrix(linearConstraints.matrixProto())
        .putAllIndicatorConstraints(indicatorConstraints.toProto())
        .build();
  }

  /**
   * Returns a model equivalent to the input protocol buffer, or raises {@link
   * UnsupportedOperationException} if {@code modelProto} uses features not yet supported by {@link
   * Model}.
   */
  public static Model fromProto(ModelProto modelProto) {
    if (modelProto.getQuadraticConstraintsCount() > 0) {
      throw new UnsupportedOperationException(
          "modelProto has quadratic constraints, not yet implemented for MathOpt Java API.");
    }
    if (modelProto.getSecondOrderConeConstraintsCount() > 0) {
      throw new UnsupportedOperationException(
          "modelProto has second order cone constraints, not yet implemented for MathOpt Java"
          + " API.");
    }
    if (modelProto.getSos1ConstraintsCount() > 0) {
      throw new UnsupportedOperationException(
          "modelProto has SOS1 constraints, not yet implemented for MathOpt Java API.");
    }
    if (modelProto.getSos2ConstraintsCount() > 0) {
      throw new UnsupportedOperationException(
          "modelProto has SOS2 constraints, not yet implemented for MathOpt Java API.");
    }
    if (modelProto.getObjective().getQuadraticCoefficients().getRowIdsCount() > 0) {
      throw new UnsupportedOperationException(
          "modelProto has quadratic objective, not yet implemented for MathOpt Java API.");
    }
    for (var auxObj : modelProto.getAuxiliaryObjectivesMap().values()) {
      if (auxObj.getQuadraticCoefficients().getRowIdsCount() > 0) {
        throw new UnsupportedOperationException(
            "modelProto has quadratic auxiliary objective, not yet implemented for MathOpt Java"
            + " API.");
      }
    }
    var model = new Model(modelProto.getName(), modelProto.getObjective().getName());
    var variables = new HashMap<Long, Variable>();
    var variablesProto = modelProto.getVariables();
    for (int i = 0; i < variablesProto.getIdsCount(); ++i) {
      long varId = variablesProto.getIds(i);
      model.variables.ensureNextIdAtLeast(varId);
      Variable v =
          model.addVariable(variablesProto.getNamesCount() > 0 ? variablesProto.getNames(i) : "")
              .setLowerBound(variablesProto.getLowerBounds(i))
              .setUpperBound(variablesProto.getUpperBounds(i))
              .setInteger(variablesProto.getIntegers(i));
      variables.put(varId, v);
    }
    var linearCons = new HashMap<Long, LinearConstraint>();
    var linearConsProto = modelProto.getLinearConstraints();
    for (int i = 0; i < linearConsProto.getIdsCount(); ++i) {
      long linConId = linearConsProto.getIds(i);
      model.linearConstraints.ensureNextIdAtLeast(linConId);
      LinearConstraint c =
          model
              .addLinearConstraint(
                  linearConsProto.getNamesCount() > 0 ? linearConsProto.getNames(i) : "")
              .setLowerBound(linearConsProto.getLowerBounds(i))
              .setUpperBound(linearConsProto.getUpperBounds(i));
      linearCons.put(linConId, c);
    }
    var linConsMat = modelProto.getLinearConstraintMatrix();
    for (int i = 0; i < linConsMat.getRowIdsCount(); ++i) {
      LinearConstraint c = linearCons.get(linConsMat.getRowIds(i));
      Variable v = variables.get(linConsMat.getColumnIds(i));
      c.setTerm(v, linConsMat.getCoefficients(i));
    }

    // Unlike other constraints, the indicator constraints are not guaranteed
    // to be sorted in the proto. The current implementation sorts the indicator constraints
    // by id. See CL comment thread for discussion; the time required for the sort is not currently
    // anticipated to be an issue. However at the time of the Elemental refactor it may be
    // worthwhile to revisit this decision and consider a more efficient approach.
    for (var idAndIndCon : ImmutableList.sortedCopyOf(
             Map.Entry.comparingByKey(), modelProto.getIndicatorConstraintsMap().entrySet())) {
      model.indicatorConstraints.ensureNextIdAtLeast(idAndIndCon.getKey());
      var indConProto = idAndIndCon.getValue();
      Variable indicator = null;
      if (indConProto.hasIndicatorId()) {
        indicator = variables.get(indConProto.getIndicatorId());
      }
      var expr = new HashLinearExpression();
      var exprProto = indConProto.getExpression();
      for (int i = 0; i < exprProto.getIdsCount(); ++i) {
        Variable v = variables.get(exprProto.getIds(i));
        if (v == null) {
          throw new IllegalArgumentException(
              "Indicator constraint expression refers to unknown variable id "
              + exprProto.getIds(i));
        }
        expr.add(exprProto.getValues(i), v);
      }
      model.addIndicatorConstraint(indicator, indConProto.getActivateOnZero(),
          indConProto.getLowerBound(), indConProto.getUpperBound(), expr, indConProto.getName());
    }

    var objProto = modelProto.getObjective();
    model.getObjective().setMaximize(objProto.getMaximize());
    model.getObjective().setOffset(objProto.getOffset());
    for (int i = 0; i < objProto.getLinearCoefficients().getIdsCount(); ++i) {
      Variable v = variables.get(objProto.getLinearCoefficients().getIds(i));
      model.getObjective().setLinearTerm(v, objProto.getLinearCoefficients().getValues(i));
    }
    model.getObjective().setPriority(objProto.getPriority());

    for (var idAndAuxObj : ImmutableList.sortedCopyOf(
             Map.Entry.comparingByKey(), modelProto.getAuxiliaryObjectivesMap().entrySet())) {
      model.objectives.ensureNextIdAtLeast(idAndAuxObj.getKey());
      var auxObjProto = idAndAuxObj.getValue();
      AuxiliaryObjective obj =
          model.addAuxiliaryObjective(auxObjProto.getPriority(), auxObjProto.getName());
      obj.setMaximize(auxObjProto.getMaximize()).setOffset(auxObjProto.getOffset());
      for (int i = 0; i < auxObjProto.getLinearCoefficients().getIdsCount(); ++i) {
        Variable v = variables.get(auxObjProto.getLinearCoefficients().getIds(i));
        obj.setLinearTerm(v, auxObjProto.getLinearCoefficients().getValues(i));
      }
    }
    return model;
  }

  /** Returns the name of the model, see also {@link #toCompleteString()}. */
  @Override
  public String toString() {
    return modelId.getName();
  }

  /**
   * Returns a multiline string describing the variables, objectives, and constraints.
   *
   * <p>This is for debug purposes only, do not rely on the exact implementation.
   */
  public String toCompleteString() {
    // TODO: b/332607722 - properly escape modelId.getName() below.
    var result = new StringBuilder("model name: \"").append(modelId.getName()).append("\"");
    result.append("\nVariables:");
    for (Variable v : variables.sortedVariables()) {
      result.append("\n  ").append(v.toCompleteString());
    }
    result.append("\nObjective:");
    result.append("\n  ").append(objectives.getPrimaryObjective().toCompleteString());
    ImmutableList<AuxiliaryObjective> auxiliaryObjectives = objectives.sortedAuxiliaryObjectives();
    if (!auxiliaryObjectives.isEmpty()) {
      result.append("\nAuxiliary Objectives:");
      for (AuxiliaryObjective o : auxiliaryObjectives) {
        result.append("\n  ").append(o.toCompleteString());
      }
    }
    ImmutableList<LinearConstraint> linCons = linearConstraints.sortedLinearConstraints();
    if (!linCons.isEmpty()) {
      result.append("\nLinear Constraints:");
      for (LinearConstraint c : linCons) {
        result.append("\n  ").append(c.toCompleteString());
      }
    }
    return result.toString();
  }

  /**
   * Tracks the changes to the model from the previous checkpoint to present.
   *
   * <p>The tracked changes are the addition, deletion, or modification of any variable, constraint,
   * or the objective. Changes are tracked from the most recent checkpoint until present. The
   * initial checkpoint is the time of creation (via {@link Model#addTracker()}), and is updated on
   * each call to {@link #advance()}. The changes to the model are represented as a {@link
   * ModelUpdateProto}, produced by {@link #updateProto()}.
   *
   * <p>This is an advanced feature that most users won't need. It is used internally to implement
   * incrementalism, but users don't have to understand how it works to use incremental solving.
   */
  public static final class UpdateTracker {
    private final Variables.Diff variablesDiff;
    private final Objectives.Diff objectivesDiff;
    private final LinearConstraints.Diff linearConstraintsDiff;
    private final IndicatorConstraints.Diff indicatorConstraintsDiff;

    private UpdateTracker(Variables.Diff variablesDiff, Objectives.Diff objectivesDiff,
        LinearConstraints.Diff linearConstraintsDiff,
        IndicatorConstraints.Diff indicatorConstraintsDiff) {
      this.variablesDiff = variablesDiff;
      this.objectivesDiff = objectivesDiff;
      this.linearConstraintsDiff = linearConstraintsDiff;
      this.indicatorConstraintsDiff = indicatorConstraintsDiff;
    }

    /** Discards all tracked changes and tracks changes from now (the new checkpoint) forward. */
    public void advance() {
      long variableCheckpoint = variablesDiff.advance();
      objectivesDiff.advance(variableCheckpoint);
      linearConstraintsDiff.advance(variableCheckpoint);
      indicatorConstraintsDiff.advance();
    }

    /** Returns a protocol buffer with all changes to the model since the last checkpoint. */
    public ModelUpdateProto updateProto() {
      var modelUpdateProto = ModelUpdateProto.newBuilder();
      ImmutableSet<Long> deletedVars = variablesDiff.getDeleted();
      ImmutableList<Variable> newVars = variablesDiff.newVariables();
      variablesDiff.update().setInProto(modelUpdateProto);
      objectivesDiff.update(newVars).setInProto(modelUpdateProto);
      linearConstraintsDiff.update(deletedVars, newVars).setInProto(modelUpdateProto);
      return modelUpdateProto.setIndicatorConstraintUpdates(indicatorConstraintsDiff.update())
          .build();
    }
  }

  /**
   * Returns a new {@link UpdateTracker} tracking changes from now forward.
   *
   * <p>This is an advanced feature that most users won't need to use directly.
   */
  public UpdateTracker addTracker() {
    long variableCheckpoint = variables.getNextId();
    UpdateTracker result =
        new UpdateTracker(variables.addDiff(), objectives.addDiff(variableCheckpoint),
            linearConstraints.addDiff(variableCheckpoint), indicatorConstraints.addDiff());
    updateTrackers.add(result);
    return result;
  }

  /**
   * Stop tracking changes to the model with {@code tracker}.
   *
   * <p>It is generally not necessary to call this function (you can rely on GC to clean everything
   * up). However, tracking updates both consumes memory and slows down model modifications. If you
   * are going to create a large number of trackers on the same model, it can be helpful to clear
   * out unused ones.
   *
   * <p>This is an advanced feature that most users won't need to use directly.
   *
   * @throws IllegalArgumentException if called more than once on the same {@link UpdateTracker}.
   */
  public void removeTracker(UpdateTracker tracker) {
    Preconditions.checkArgument(updateTrackers.contains(tracker));
    variables.removeDiff(tracker.variablesDiff);
    objectives.removeDiff(tracker.objectivesDiff);
    linearConstraints.removeDiff(tracker.linearConstraintsDiff);
    indicatorConstraints.removeDiff(tracker.indicatorConstraintsDiff);
    updateTrackers.remove(tracker);
  }
}
