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

import static java.util.Comparator.comparingLong;
import static java.util.Map.Entry.comparingByKey;

import com.google.common.base.Preconditions;
import com.google.common.collect.ImmutableMap;
import com.google.ortools.mathopt.BasisProto;
import com.google.ortools.mathopt.BasisStatusProto;
import com.google.ortools.mathopt.SparseBasisStatusVector;
import java.util.EnumMap;
import java.util.Optional;

/**
 * A combinatorial characterization for a solution to a linear program.
 *
 * <p>The simplex method for solving linear programs always returns a "basic feasible solution"
 * which can be described combinatorially as a Basis. A basis assigns a BasisStatus for every
 * variable and linear constraint.
 *
 * <p>E.g. consider a standard form LP: min c * x s.t. A * x = b; x >= 0 that has more variables
 * than constraints and with full row rank A.
 *
 * <p>Let n be the number of variables and m the number of linear constraints. A valid basis for
 * this problem can be constructed as follows:
 *
 * <ul>
 *   <li>All constraints will have basis status FIXED.
 *   <li>Pick m variables such that the columns of A are linearly independent and assign the status
 *       BASIC
 *   <li>Assign the status AT_LOWER for the remaining n - m variables.
 * </ul>
 *
 * <p>The basic solution for this basis is the unique solution of A * x = b that has all variables
 * with status AT_LOWER fixed to their lower bounds (all zero). The resulting solution is called a
 * basic feasible solution if it also satisfies x >= 0.
 *
 * <p>See go/mathopt-basis for treatment of the general case and an explanation of how a dual
 * solution is determined for a basis. This class along with its nested classes are immutable.
 *
 * <p>This class is immutable.
 */
public final class Basis {
  private final ImmutableMap<LinearConstraint, BasisStatus> constraintStatus;
  private final ImmutableMap<Variable, BasisStatus> variableStatus;
  private final Optional<SolutionStatus> basicDualFeasibility;

  /** Status of a variable/constraint in a LP basis. */
  public enum BasisStatus {
    /** The variable/constraint is free (it has no finite bounds). */
    FREE(BasisStatusProto.BASIS_STATUS_FREE),

    /** The variable/constraint is at its lower bound (which must be finite). */
    AT_LOWER_BOUND(BasisStatusProto.BASIS_STATUS_AT_LOWER_BOUND),

    /** The variable/constraint is at its upper bound (which must be finite). */
    AT_UPPER_BOUND(BasisStatusProto.BASIS_STATUS_AT_UPPER_BOUND),

    /** The variable/constraint has identical finite lower and upper bounds. */
    FIXED_VALUE(BasisStatusProto.BASIS_STATUS_FIXED_VALUE),

    /** The variable/constraint is basic. */
    BASIC(BasisStatusProto.BASIS_STATUS_BASIC);

    private static class ProtoMap {
      private static final EnumMap<BasisStatusProto, BasisStatus> map =
          new EnumMap<>(BasisStatusProto.class);
    }

    private final BasisStatusProto proto;

    BasisStatus(BasisStatusProto proto) {
      ProtoMap.map.put(proto, this);
      this.proto = proto;
    }

    /** Returns the corresponding {@link BasisStatusProto} for the enum. */
    public BasisStatusProto toProto() {
      return this.proto;
    }

    /**
     * Returns a {@link BasisStatus} from the given {@code proto}.
     *
     * @throws IllegalArgumentException if {@code proto} is unspecified.
     */
    public static BasisStatus fromProto(BasisStatusProto proto) {
      BasisStatus status = ProtoMap.map.get(proto);
      if (status == null) {
        throw new IllegalArgumentException("basis status must be set");
      }
      return status;
    }
  }

  /** Returns the status for each constraint in the basis. */
  public ImmutableMap<LinearConstraint, BasisStatus> getConstraintStatus() {
    return constraintStatus;
  }

  /** Returns the status for each variable in the basis. */
  public ImmutableMap<Variable, BasisStatus> getVariableStatus() {
    return variableStatus;
  }

  /**
   * This is an advanced feature used by MathOpt to characterize feasibility of suboptimal LP
   * solutions (optimal solutions will always have status {@link SolutionStatus#FEASIBLE}).
   *
   * <p>For single-sided LPs it should be equal to the feasibility status of the associated dual
   * solution. For two-sided LPs it may be different in some edge cases (e.g. incomplete solves with
   * primal simplex). For more details see go/mathopt-basis-advanced#dualfeasibility.
   *
   * <p>If you are providing a starting basis via {@link ModelSolveParameters#getBasis()}, this
   * value is ignored and can by empty. It is only relevant for the basis returned by {@link
   * Solution#getBasis()}, and is always present when returned from {@code solve()}.
   */
  public Optional<SolutionStatus> getBasicDualFeasibility() {
    return basicDualFeasibility;
  }

  /**
   * Creates a new basis by providing all members, prefer {@link Basis(ImmutableMap, ImmutableMap)}
   * when giving a starting basis (the basicDualFeasibility is ignored in this case).
   */
  public Basis(ImmutableMap<LinearConstraint, BasisStatus> constraintStatus,
      ImmutableMap<Variable, BasisStatus> variableStatus,
      Optional<SolutionStatus> basicDualFeasibility) {
    this.constraintStatus = constraintStatus;
    this.variableStatus = variableStatus;
    this.basicDualFeasibility = basicDualFeasibility;
  }

  /**
   * Creates a new basis with basic dual feasibility undetermined (good enough for a user-specified
   * starting basis).
   */
  public Basis(ImmutableMap<LinearConstraint, BasisStatus> constraintStatus,
      ImmutableMap<Variable, BasisStatus> variableStatus) {
    this(constraintStatus, variableStatus, Optional.empty());
  }

  /**
   * Creates a new Basis object from an equivalent BasisProto.
   *
   * <p>This is the inverse operation of {@link #toProto()}.
   *
   * @param proto is the BasisProto that is converted to a new Basis.
   * @param model stores all relevant information about the variables and constraints.
   * @throws IllegalArgumentException if the {@link #constraintStatus} or {@link #variableStatus}
   *     cannot be converted from the {@code proto}, or if the SolutionStatus is unset.
   */
  public Basis(Model model, BasisProto proto) {
    this(linearConstraintBasisFromProto(model, proto.getConstraintStatus()),
        variableBasisFromProto(model, proto.getVariableStatus()),
        SolutionStatus.optionalFromProto(proto.getBasicDualFeasibility()));
  }

  /**
   * Returns an equivalent protocol buffer to this.
   *
   * <p>This is the inverse operation of {@link #Basis(Model, BasisProto)}.
   */
  public BasisProto toProto() {
    return BasisProto.newBuilder()
        .setBasicDualFeasibility(SolutionStatus.optionalToProto(basicDualFeasibility))
        .setVariableStatus(basisVectorToProto(variableStatus))
        .setConstraintStatus(basisVectorToProto(constraintStatus))
        .build();
  }

  /**
   * Returns an ImmutableMap mapping a {@link Variable} to a {@link BasisStatus}. This mapping is
   * created from the given {@code proto}.
   *
   * <p>This is the inverse operation of {@link #basisVectorToProto(ImmutableMap)}.
   *
   * @throws IllegalArgumentException if the {@code proto} is invalid or if it has a variable ID
   *     that does not exist in {@code model}.
   */
  public static ImmutableMap<Variable, BasisStatus> variableBasisFromProto(
      Model model, SparseBasisStatusVector proto) {
    Preconditions.checkArgument(proto.getIdsCount() == proto.getValuesCount(),
        "Ids size=%s and values size=%s must be the same", proto.getIdsCount(),
        proto.getValuesCount());
    ImmutableMap.Builder<Variable, BasisStatus> map = ImmutableMap.builder();
    for (int i = 0; i < proto.getIdsCount(); i++) {
      long id = proto.getIds(i);
      Preconditions.checkArgument(model.hasVariableWithId(id), "no variable with ID %s exists", id);
      map.put(model.getVariable(id), BasisStatus.fromProto(proto.getValues(i)));
    }
    return map.buildOrThrow();
  }

  /**
   * Returns an ImmutableMap mapping a {@link LinearConstraint} to a {@link BasisStatus}. This
   * mapping is created from the given {@code proto}.
   *
   * <p>This is the inverse operation of {@link #basisVectorToProto(ImmutableMap)}.
   *
   * @throws IllegalArgumentException if the {@code proto} is invalid and if it has a linear
   * constraint ID that does not exist in {@code model}.
   */
  public static ImmutableMap<LinearConstraint, BasisStatus> linearConstraintBasisFromProto(
      Model model, SparseBasisStatusVector proto) {
    Preconditions.checkArgument(proto.getIdsCount() == proto.getValuesCount(),
        "Ids size=%s and values size=%s must be the same", proto.getIdsCount(),
        proto.getValuesCount());
    ImmutableMap.Builder<LinearConstraint, BasisStatus> map = ImmutableMap.builder();
    for (int i = 0; i < proto.getIdsCount(); i++) {
      long id = proto.getIds(i);
      Preconditions.checkArgument(
          model.hasLinearConstraintWithId(id), "no linear constraint with ID %s exists", id);
      map.put(model.getLinearConstraint(id), BasisStatus.fromProto(proto.getValues(i)));
    }
    return map.buildOrThrow();
  }

  /**
   * Returns the SparseBasisStatusVector protocol buffer equivalent of {@code basisVector}, where
   * the keys of type {@code T} are replaced by their id.
   *
   * <p>This is the inverse operation of {@link #variableBasisFromProto(Model,
   * SparseBasisStatusVector)} and {@link #linearConstraintBasisFromProto(Model,
   * SparseBasisStatusVector)}.
   */
  public static <T extends ModelElement> SparseBasisStatusVector basisVectorToProto(
      ImmutableMap<T, BasisStatus> basisVector) {
    var result = SparseBasisStatusVector.newBuilder();
    basisVector.entrySet()
        .stream()
        .sorted(comparingByKey(comparingLong(T::getId)))
        .forEachOrdered((entry) -> {
          result.addIds(entry.getKey().getId());
          result.addValues(entry.getValue().toProto());
        });
    return result.build();
  }
}
