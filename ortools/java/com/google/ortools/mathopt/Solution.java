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
import com.google.ortools.mathopt.DualRayProto;
import com.google.ortools.mathopt.DualSolutionProto;
import com.google.ortools.mathopt.PrimalRayProto;
import com.google.ortools.mathopt.PrimalSolutionProto;
import com.google.ortools.mathopt.SolutionProto;
import java.util.Optional;

/**
 * What is included in a solution depends on the kind of problem and solver. The current common
 * patterns are
 *
 * <ul>
 *   <li>1. MIP solvers return only a primal solution.
 *   <li>2. Simplex LP solvers often return a basis and the primal and dual solutions associated to
 *       this basis.
 *   <li>3. Other continuous solvers often return a primal and dual solution that are connected in a
 *       solver-dependent form.
 * </ul>
 *
 * <p>This class along with its nested classes are immutable.
 */
final class Solution {
  private final Optional<PrimalSolution> primalSolution;
  private final Optional<DualSolution> dualSolution;
  private final Optional<Basis> basis;

  /**
   * A solution to an optimization problem.
   *
   * <p>E.g. consider a simple linear program: min c * x s.t. A * x >= b; x >=0.
   *
   * <p>A primal solution is assignment values to x. It is feasible if it satisfies A * x >= b and x
   * >=0 from above. In the class {@link PrimalSolution}, {@link #variableValues} is x and {@link
   * #objectiveValue} is c * x.
   *
   * <p>For the general case of a MathOpt optimization model, see go/mathopt-solutions for details.
   */
  public static final class PrimalSolution {
    private final ImmutableMap<Variable, Double> variableValues;
    private final double objectiveValue;
    private final SolutionStatus feasibilityStatus;

    public ImmutableMap<Variable, Double> getVariableValues() {
      return variableValues;
    }

    public double getObjectiveValue() {
      return objectiveValue;
    }

    public SolutionStatus getFeasibilityStatus() {
      return feasibilityStatus;
    }

    /**
     * Creates a new PrimalSolution from a PrimalSolutionProto.
     *
     * @param proto is the PrimalSolutionProto that is converted to a new PrimalSolution.
     * @param model stores all relevant information about the variables and constraints.
     * @throws IllegalArgumentException if the {@link #variableValues} cannot be converted from the
     *     {@code proto}, or if the SolutionStatus is unset.
     */
    PrimalSolution(Model model, PrimalSolutionProto proto) {
      this.variableValues = UtilFromProto.variableValuesFromProto(model, proto.getVariableValues());
      this.objectiveValue = proto.getObjectiveValue();
      this.feasibilityStatus = SolutionStatus.fromProto(proto.getFeasibilityStatus());
    }
  }

  /**
   * A direction of unbounded improvement to an optimization problem; equivalently, a certificate of
   * infeasibility for the dual of the optimization problem.
   *
   * <p>E.g. consider a simple linear program: min c * x s.t. A * x >= b; x >= 0.
   *
   * <p>A primal ray is an x that satisfies:
   *
   * <ul>
   *   <li>1. c * x < 0
   *   <li>2. A * x >= 0
   *   <li>3. x >= 0
   * </ul>
   *
   * <p>Observe that given a feasible solution, any positive multiple of the primal ray plus that
   * solution is still feasible, and gives a better objective value. A primal ray also proves the
   * dual optimization problem infeasible.
   *
   * <p>In the class {@link PrimalRay}, {@link #variableValues} is this x.
   *
   * <p>For the general case of a MathOpt optimization model, see go/mathopt-solutions for details.
   */
  public static final class PrimalRay {
    private final ImmutableMap<Variable, Double> variableValues;

    public ImmutableMap<Variable, Double> getVariableValues() {
      return variableValues;
    }

    /**
     * Creates a new PrimalRay from a PrimalRayProto.
     *
     * @param proto is the PrimalRayProto that is converted to a new PrimalRay.
     * @param model stores all relevant information about the variables and constraints.
     * @throws IllegalArgumentException if the {@link #variableValues} cannot be converted from the
     *     {@code proto}.
     */
    PrimalRay(Model model, PrimalRayProto proto) {
      this.variableValues = UtilFromProto.variableValuesFromProto(model, proto.getVariableValues());
    }
  }

  /**
   * A solution to the dual of an optimization problem.
   *
   * <p>E.g. consider the primal dual pair linear program:
   *
   * <p>(Primal) min c * x s.t. A * x >= b; x >= 0
   *
   * <p>(Dual) max b * y s.t. y * A + r = c; y,r >= 0.
   *
   * <p>The dual solutions is the pair (y, r). It is feasible if it satisfies the constrains from
   * the (Dual) above.
   *
   * <p>Below, y is {@link #dualValues}, r is {@link reducedCosts}, and b * y is {@link
   * objectiveValue}.
   *
   * <p>For the general case, see go/mathopt-solutions and go/mathopt-dual (and note that the dual
   * objective depends on r in the general case).
   */
  public static final class DualSolution {
    private final ImmutableMap<LinearConstraint, Double> dualValues;
    private final ImmutableMap<Variable, Double> reducedCosts;
    private final Optional<Double> objectiveValue;
    private final SolutionStatus feasibilityStatus;

    public ImmutableMap<LinearConstraint, Double> getDualValues() {
      return dualValues;
    }

    public ImmutableMap<Variable, Double> getReducedCosts() {
      return reducedCosts;
    }

    public Optional<Double> getObjectiveValue() {
      return objectiveValue;
    }

    public SolutionStatus getFeasibilityStatus() {
      return feasibilityStatus;
    }

    /**
     * Creates a new DualSolution from a DualSolutionProto.
     *
     * @param proto is the DualSolutionProto that is converted to a new DualSolution.
     * @param model stores all relevant information about the variables and constraints.
     * @throws IllegalArgumentException if the {@link #dualValues} or {@link #reducedCosts} cannot
     *     be converted from the {@code proto}, or if the SolutionStatus is unset.
     */
    DualSolution(Model model, DualSolutionProto proto) {
      this.dualValues = UtilFromProto.linearConstraintValuesFromProto(model, proto.getDualValues());
      this.reducedCosts = UtilFromProto.variableValuesFromProto(model, proto.getReducedCosts());
      this.objectiveValue =
          proto.hasObjectiveValue() ? Optional.of(proto.getObjectiveValue()) : Optional.empty();
      this.feasibilityStatus = SolutionStatus.fromProto(proto.getFeasibilityStatus());
    }
  }

  /**
   * A direction of unbounded improvement to the dual of an optimization problem; equivalently, a
   * certificate of primal infeasibility.
   *
   * <p>E.g. consider the primal dual linear program pair:
   *
   * <p>(Primal) min c * x s.t. A * x >= b; x >= 0
   *
   * <p>(Dual) max b * y s.t. y * A + r = c; y,r >= 0
   *
   * <p>The dual ray is the pair (y, r) satisfying:
   *
   * <ul>
   *   <li>1. b * y > 0
   *   <li>2. y * A + r = 0
   *   <li>3. y, r >= 0
   * </ul>
   *
   * <p>Observe that adding a positive multiplier of (y, r) to dual feasible solution maintains dual
   * feasibility and improves the objective (proving the dual is unbounded). The dual ray also
   * proves the primal problem is infeasible.
   *
   * <p>In the class {@link DualRay}, y is {@link #dualValues} and r is {@link #reducedCosts}.
   *
   * <p>For the general case, see go/mathopt-solutions and go/mathopt-dual (and note that the dual
   * objective depends on r in the general case).
   */
  public static final class DualRay {
    private final ImmutableMap<LinearConstraint, Double> dualValues;
    private final ImmutableMap<Variable, Double> reducedCosts;

    public ImmutableMap<LinearConstraint, Double> getDualValues() {
      return dualValues;
    }

    public ImmutableMap<Variable, Double> getReducedCosts() {
      return reducedCosts;
    }

    /**
     * Creates a new DualRay from a DualRayProto.
     *
     * @param proto is the DualRayProto that is converted to a new DualRay.
     * @param model stores all relevant information about the variables and constraints.
     * @throws IllegalArgumentException if the {@link #dualValues} or {@link #reducedCosts} cannot
     *     be converted from the {@code proto}.
     */
    DualRay(Model model, DualRayProto proto) {
      this.dualValues = UtilFromProto.linearConstraintValuesFromProto(model, proto.getDualValues());
      this.reducedCosts = UtilFromProto.variableValuesFromProto(model, proto.getReducedCosts());
    }
  }

  public Optional<PrimalSolution> getPrimalSolution() {
    return primalSolution;
  }

  public Optional<DualSolution> getDualSolution() {
    return dualSolution;
  }

  public Optional<Basis> getBasis() {
    return basis;
  }

  /**
   * Creates a new Solution from a SolutionProto.
   *
   * @param proto is a SolutionProto that is converted to a new Solution.
   * @param model is the optimization problem that was solved to generate {@code proto} which
   *     defines the {@link Variable} and {@link LinearConstraint} objects that this SolveResult
   *     will be expressed in terms.
   * @throws IllegalArgumentException if the {@link #primalSolution}, {@link #dualSolution}, or
   *     {@link #basis} cannot be converted from the {@code proto}.
   */
  Solution(Model model, SolutionProto proto) {
    this.primalSolution = proto.hasPrimalSolution()
        ? Optional.of(new PrimalSolution(model, proto.getPrimalSolution()))
        : Optional.empty();
    this.dualSolution = proto.hasDualSolution()
        ? Optional.of(new DualSolution(model, proto.getDualSolution()))
        : Optional.empty();
    this.basis =
        proto.hasBasis() ? Optional.of(new Basis(model, proto.getBasis())) : Optional.empty();
  }
}
