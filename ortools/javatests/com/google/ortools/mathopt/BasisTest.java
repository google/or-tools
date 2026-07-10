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
import static com.google.ortools.mathopt.testing.MathOptProtoSubject.assertThat;
import static org.junit.jupiter.api.Assertions.assertThrows;

import com.google.common.collect.ImmutableMap;
import com.google.ortools.mathopt.Basis.BasisStatus;
import java.util.Optional;
import org.junit.jupiter.api.Test;

public final class BasisTest {
  @Test
  public void variableBasisFromProto_validConversion() {
    Model model = new Model("test_model");
    Variable x = model.addBinaryVariable("x");
    Variable y = model.addVariable("y");
    SparseBasisStatusVector proto = SparseBasisStatusVector.newBuilder()
                                        .addIds(x.getId())
                                        .addValues(BasisStatusProto.BASIS_STATUS_FREE)
                                        .addIds(y.getId())
                                        .addValues(BasisStatusProto.BASIS_STATUS_AT_UPPER_BOUND)
                                        .build();

    assertThat(Basis.variableBasisFromProto(model, proto))
        .containsExactly(x, Basis.BasisStatus.FREE, y, Basis.BasisStatus.AT_UPPER_BOUND);
  }

  @Test
  public void variableBasisFromProto_protoInvalid_throws() {
    Model model = new Model("test_model");
    Variable x = model.addBinaryVariable("x");
    Variable y = model.addVariable("y");
    SparseBasisStatusVector proto = SparseBasisStatusVector.newBuilder()
                                        .addIds(x.getId())
                                        .addValues(BasisStatusProto.BASIS_STATUS_FREE)
                                        .addIds(y.getId())
                                        .build();

    var error = assertThrows(
        IllegalArgumentException.class, () -> Basis.variableBasisFromProto(model, proto));
    assertThat(error).hasMessageThat().contains("must be the same");
  }

  @Test
  public void variableBasisFromProto_absentVariableInModel_throws() {
    Model model1 = new Model("test_model");
    Model model2 = new Model("test_model_2");
    Variable x = model1.addBinaryVariable("x");
    Variable unused = model2.addVariable("y");
    Variable z = model2.addVariable("z");
    SparseBasisStatusVector proto = SparseBasisStatusVector.newBuilder()
                                        .addIds(x.getId())
                                        .addValues(BasisStatusProto.BASIS_STATUS_FREE)
                                        .addIds(z.getId())
                                        .addValues(BasisStatusProto.BASIS_STATUS_AT_UPPER_BOUND)
                                        .build();

    var error = assertThrows(
        IllegalArgumentException.class, () -> Basis.variableBasisFromProto(model1, proto));
    assertThat(error).hasMessageThat().contains("no variable");
  }

  @Test
  public void linearConstraintBasisFromProto_validConversion() {
    Model model = new Model("test_model");
    LinearConstraint l1 = model.addLinearConstraint("lc_1");
    LinearConstraint l2 = model.addLinearConstraint("lc_2");
    SparseBasisStatusVector proto = SparseBasisStatusVector.newBuilder()
                                        .addIds(l1.getId())
                                        .addValues(BasisStatusProto.BASIS_STATUS_FREE)
                                        .addIds(l2.getId())
                                        .addValues(BasisStatusProto.BASIS_STATUS_AT_UPPER_BOUND)
                                        .build();

    assertThat(Basis.linearConstraintBasisFromProto(model, proto))
        .containsExactly(l1, Basis.BasisStatus.FREE, l2, Basis.BasisStatus.AT_UPPER_BOUND);
  }

  @Test
  public void linearConstraintBasisFromProto_protoInvalid_throws() {
    Model model = new Model("test_model");
    LinearConstraint l1 = model.addLinearConstraint("lc_1");
    LinearConstraint l2 = model.addLinearConstraint("lc_2");
    SparseBasisStatusVector proto = SparseBasisStatusVector.newBuilder()
                                        .addIds(l1.getId())
                                        .addValues(BasisStatusProto.BASIS_STATUS_FREE)
                                        .addIds(l2.getId())
                                        .build();

    var error = assertThrows(
        IllegalArgumentException.class, () -> Basis.linearConstraintBasisFromProto(model, proto));
    assertThat(error).hasMessageThat().contains("must be the same");
  }

  @Test
  public void linearConstraintBasisFromProto_absentConstraintInModel_throws() {
    Model model1 = new Model("test_model");
    Model model2 = new Model("test_model_2");
    LinearConstraint l1 = model1.addLinearConstraint("lc_1");
    LinearConstraint unused = model2.addLinearConstraint("lc_2");
    LinearConstraint l3 = model2.addLinearConstraint("lc_3");
    SparseBasisStatusVector proto = SparseBasisStatusVector.newBuilder()
                                        .addIds(l1.getId())
                                        .addValues(BasisStatusProto.BASIS_STATUS_FREE)
                                        .addIds(l3.getId())
                                        .addValues(BasisStatusProto.BASIS_STATUS_AT_UPPER_BOUND)
                                        .build();

    var error = assertThrows(
        IllegalArgumentException.class, () -> Basis.linearConstraintBasisFromProto(model1, proto));
    assertThat(error).hasMessageThat().contains("no linear constraint");
  }

  @Test
  public void basisVectorToProto_onVariables_isInOrder() {
    var model = new Model();
    Variable x = model.addBinaryVariable();
    Variable y = model.addBinaryVariable();
    Variable z = model.addBinaryVariable();
    ImmutableMap<Variable, BasisStatus> basisVector =
        ImmutableMap.of(x, BasisStatus.BASIC, y, BasisStatus.FREE, z, BasisStatus.FIXED_VALUE);

    SparseBasisStatusVector expected = SparseBasisStatusVector.newBuilder()
                                           .addIds(0)
                                           .addIds(1)
                                           .addIds(2)
                                           .addValues(BasisStatusProto.BASIS_STATUS_BASIC)
                                           .addValues(BasisStatusProto.BASIS_STATUS_FREE)
                                           .addValues(BasisStatusProto.BASIS_STATUS_FIXED_VALUE)
                                           .build();

    assertThat(Basis.basisVectorToProto(basisVector)).isEquivalentTo(expected);
  }

  @Test
  public void basisStatus_toProto() {
    assertThat(Basis.BasisStatus.FREE.toProto()).isEqualTo(BasisStatusProto.BASIS_STATUS_FREE);
    assertThat(Basis.BasisStatus.AT_LOWER_BOUND.toProto())
        .isEqualTo(BasisStatusProto.BASIS_STATUS_AT_LOWER_BOUND);
    assertThat(Basis.BasisStatus.AT_UPPER_BOUND.toProto())
        .isEqualTo(BasisStatusProto.BASIS_STATUS_AT_UPPER_BOUND);
    assertThat(Basis.BasisStatus.FIXED_VALUE.toProto())
        .isEqualTo(BasisStatusProto.BASIS_STATUS_FIXED_VALUE);
    assertThat(Basis.BasisStatus.BASIC.toProto()).isEqualTo(BasisStatusProto.BASIS_STATUS_BASIC);
  }

  @Test
  public void newBasis_withoutBasicDualFeasibility_hasMembers() {
    var model = new Model();
    Variable x = model.addVariable();
    LinearConstraint c = model.addLinearConstraint();

    var basis =
        new Basis(ImmutableMap.of(c, BasisStatus.FREE), ImmutableMap.of(x, BasisStatus.BASIC));

    assertThat(basis.getConstraintStatus()).containsExactly(c, BasisStatus.FREE);
    assertThat(basis.getVariableStatus()).containsExactly(x, BasisStatus.BASIC);
    assertThat(basis.getBasicDualFeasibility()).isEmpty();
  }

  @Test
  public void newBasis_withBasicDualFeasibility_hasMembers() {
    var model = new Model();
    Variable x = model.addVariable();
    LinearConstraint c = model.addLinearConstraint();

    var basis = new Basis(ImmutableMap.of(c, BasisStatus.FREE),
        ImmutableMap.of(x, BasisStatus.BASIC), Optional.of(SolutionStatus.INFEASIBLE));

    assertThat(basis.getConstraintStatus()).containsExactly(c, BasisStatus.FREE);
    assertThat(basis.getVariableStatus()).containsExactly(x, BasisStatus.BASIC);
    assertThat(basis.getBasicDualFeasibility()).hasValue(SolutionStatus.INFEASIBLE);
  }

  @Test
  public void basis_fromProto_hasMembers() {
    var model = new Model("test_model");
    Variable x = model.addBinaryVariable("x");
    Variable y = model.addVariable("y");
    LinearConstraint l1 = model.addLinearConstraint("lc_1");
    LinearConstraint l2 = model.addLinearConstraint("lc_2");
    BasisProto proto = BasisProto.newBuilder()
                           .setConstraintStatus(SparseBasisStatusVector.newBuilder()
                                   .addIds(l1.getId())
                                   .addValues(BasisStatusProto.BASIS_STATUS_FREE)
                                   .addIds(l2.getId())
                                   .addValues(BasisStatusProto.BASIS_STATUS_AT_UPPER_BOUND)
                                   .build())
                           .setVariableStatus(SparseBasisStatusVector.newBuilder()
                                   .addIds(x.getId())
                                   .addValues(BasisStatusProto.BASIS_STATUS_FREE)
                                   .addIds(y.getId())
                                   .addValues(BasisStatusProto.BASIS_STATUS_AT_UPPER_BOUND)
                                   .build())
                           .setBasicDualFeasibility(SolutionStatusProto.SOLUTION_STATUS_FEASIBLE)
                           .build();

    Basis basis = new Basis(model, proto);

    assertThat(basis.getConstraintStatus())
        .containsExactly(l1, Basis.BasisStatus.FREE, l2, Basis.BasisStatus.AT_UPPER_BOUND);
    assertThat(basis.getVariableStatus())
        .containsExactly(x, Basis.BasisStatus.FREE, y, Basis.BasisStatus.AT_UPPER_BOUND);
    assertThat(basis.getBasicDualFeasibility()).hasValue(SolutionStatus.FEASIBLE);
  }

  @Test
  public void basis_toProto_emptyBasisRoundTrips() {
    Model model = new Model("test_model");

    BasisProto proto = BasisProto.getDefaultInstance();
    Basis basis = new Basis(model, proto);

    assertThat(basis.toProto()).isEquivalentTo(proto);
  }

  @Test
  public void basis_toProto_roundTrips() {
    Model model = new Model("test_model");
    Variable x = model.addBinaryVariable("x");
    Variable y = model.addVariable("y");
    LinearConstraint l1 = model.addLinearConstraint("lc_1");
    LinearConstraint l2 = model.addLinearConstraint("lc_2");
    BasisProto proto = BasisProto.newBuilder()
                           .setConstraintStatus(SparseBasisStatusVector.newBuilder()
                                   .addIds(l1.getId())
                                   .addValues(BasisStatusProto.BASIS_STATUS_FREE)
                                   .addIds(l2.getId())
                                   .addValues(BasisStatusProto.BASIS_STATUS_AT_UPPER_BOUND)
                                   .build())
                           .setVariableStatus(SparseBasisStatusVector.newBuilder()
                                   .addIds(x.getId())
                                   .addValues(BasisStatusProto.BASIS_STATUS_FREE)
                                   .addIds(y.getId())
                                   .addValues(BasisStatusProto.BASIS_STATUS_AT_UPPER_BOUND)
                                   .build())
                           .setBasicDualFeasibility(SolutionStatusProto.SOLUTION_STATUS_FEASIBLE)
                           .build();

    Basis basis = new Basis(model, proto);

    assertThat(basis.toProto()).isEquivalentTo(proto);
  }
}
