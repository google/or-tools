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

import com.google.common.collect.ImmutableMap;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Optional;

/**
 * Parameters to control a single solve that are specific to the input model (e.g. a solution hint).
 *
 * <p>See {@link SolveParameters} for model independent parameters (e.g. time limit).
 */
public final class ModelSolveParameters {
  private Optional<Basis> basis = Optional.empty();
  private final List<SolutionHint> solutionHints = new ArrayList<>();
  private ImmutableMap<Variable, Integer> branchingPriorities = ImmutableMap.of();

  /** Creates a new empty ModelSolveParameters. */
  public ModelSolveParameters() {}

  /**
   * Creates a new ModelSolveParameters equivalent to {@code proto}.
   *
   * @throws IllegalArgumentException if {@code proto} refers to variables or constraints that are
   *     not in the model, or if {@code proto} is otherwise invalid (e.g. the {@link
   *     com.google.ortools.mathopt.SparseDoubleVectorProto} for the variable values in a solution
   *     hint contains two lists of unequal size).
   */
  public ModelSolveParameters(Model model, ModelSolveParametersProto proto) {
    if (proto.hasInitialBasis()) {
      basis = Optional.of(new Basis(model, proto.getInitialBasis()));
    }
    for (SolutionHintProto hint : proto.getSolutionHintsList()) {
      solutionHints.add(new SolutionHint(model, hint));
    }
    branchingPriorities =
        UtilFromProto.variableInt32ValuesFromProto(model, proto.getBranchingPriorities());
  }

  /**
   * Sets the starting basis for solving to {@code basis} and returns {@code this}, see {@link
   * #getBasis()} for details.
   */
  public ModelSolveParameters setBasis(Basis basis) {
    this.basis = Optional.of(basis);
    return this;
  }

  /**
   * Removes the starting basis for solving and returns {@code this}, see {@link #getBasis()} for
   * details.
   */
  public ModelSolveParameters clearBasis() {
    this.basis = Optional.empty();
    return this;
  }

  /** An initial basis to run the simplex algorithm from, see {@link Basis} for details. */
  public Optional<Basis> getBasis() {
    return basis;
  }

  /**
   * Adds {@code hint} to the list of hints for the solver and returns {@code this}, see {@link
   * SolutionHint} and {@link #unmodifiableSolutionHints()} for details.
   */
  public ModelSolveParameters addSolutionHint(SolutionHint hint) {
    solutionHints.add(hint);
    return this;
  }

  /**
   * Removes all solution hints and returns this, see {@link #unmodifiableSolutionHints()} for
   * details.
   */
  public ModelSolveParameters clearSolutionHints() {
    solutionHints.clear();
    return this;
  }

  /**
   * Optional solution hints for the solver. If the underlying solver only accepts a single hint,
   * the first hint is used.
   */
  public List<SolutionHint> unmodifiableSolutionHints() {
    return Collections.unmodifiableList(solutionHints);
  }

  /**
   * Returns a map from variables of the model to their to branching priority.
   *
   * <p>Variables with higher branching priority are branched on first. Variables for which
   * priorities are not set get the solver's default priority (usually zero).
   */
  public ImmutableMap<Variable, Integer> getBranchingPriorities() {
    return branchingPriorities;
  }

  /**
   * Sets the branching priority for each variable to {@code branchingPriorities} and returns this.
   *
   * <p>Any priorities that were previously set are cleared.
   *
   * @see #getBranchingPriorities()
   */
  public ModelSolveParameters setBranchingPriorities(
      ImmutableMap<Variable, Integer> branchingPriorities) {
    this.branchingPriorities = branchingPriorities;
    return this;
  }

  /** Returns an equivalent protocol buffer to {@code this}. */
  public ModelSolveParametersProto toProto() {
    var result = ModelSolveParametersProto.newBuilder();
    if (basis.isPresent()) {
      result.setInitialBasis(basis.get().toProto());
    }
    for (SolutionHint hint : solutionHints) {
      result.addSolutionHints(hint.toProto());
    }
    result.setBranchingPriorities(SparseContainers.toSparseInt32VectorProto(branchingPriorities));
    return result.build();
  }

  /**
   * A suggested starting solution for the solver.
   *
   * <p>The double values in {@link #getVariableValues()} and {@link #getDualValues()} should be
   * finite and not NaN (otherwise, MathOpt validation will raise an exception while solving).
   *
   * <p>MIP solvers generally only want primal information ({@link #variableValues}), while LP
   * solvers want both primal and dual information ({@link #dualValues}).
   *
   * <p>Many MIP solvers can work with: (1) partial solutions that do not specify all variables or
   * (2) infeasible solutions. In these cases, solvers typically solve a sub-MIP to complete/correct
   * the hint.
   *
   * <p>How the hint is used by the solver, if at all, is highly dependent on the solver, the
   * problem type, and the algorithm used. The most reliable way to ensure your hint has an effect
   * is to read the underlying solvers logs with and without the hint.
   *
   * <p>Simplex-based LP solvers typically prefer an initial basis to a solution hint (they need to
   * crossover to convert the hint to a basic feasible solution otherwise).
   *
   * <p>This class is immutable.
   */
  public static final class SolutionHint {
    private final ImmutableMap<Variable, Double> variableValues;
    private final ImmutableMap<LinearConstraint, Double> dualValues;

    /** Creates a new solution hint with both primal and dual values. */
    public SolutionHint(ImmutableMap<Variable, Double> variableValues,
        ImmutableMap<LinearConstraint, Double> dualValues) {
      this.variableValues = variableValues;
      this.dualValues = dualValues;
    }

    /** Creates a new solution hint with only primal values. */
    public SolutionHint(ImmutableMap<Variable, Double> variableValues) {
      this(variableValues, ImmutableMap.of());
    }

    /**
     * Creates a new SolutionHint equivalent to {@code proto} on the {@link Variable} and {@link
     * LinearConstraint} objects owned by {@code model}.
     *
     * @throws IllegalArgumentException if either the variable or dual values from {@code proto} are
     *     invalid or use an id not found in {@code model}.
     */
    public SolutionHint(Model model, SolutionHintProto protoHint) {
      this(UtilFromProto.variableValuesFromProto(model, protoHint.getVariableValues()),
          UtilFromProto.linearConstraintValuesFromProto(model, protoHint.getDualValues()));
    }

    /** Returns variable values for the primal variables in the hint. */
    ImmutableMap<Variable, Double> getVariableValues() {
      return variableValues;
    }

    /** Returns variable values for the dual variables in the hint. */
    ImmutableMap<LinearConstraint, Double> getDualValues() {
      return dualValues;
    }

    /**
     * Returns an equivalent protocol buffer for {@code this}, where {@code Variable} and {@code
     * LinearConstraint} objects are replaced by their id.
     */
    public SolutionHintProto toProto() {
      return SolutionHintProto.newBuilder()
          .setVariableValues(SparseContainers.toSparseDoubleVectorProto(variableValues))
          .setDualValues(SparseContainers.toSparseDoubleVectorProto(dualValues))
          .build();
    }
  }
}
