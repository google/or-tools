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
import static org.junit.jupiter.api.Assertions.assertThrows;

import org.junit.jupiter.api.Test;

public final class SolutionTest {
  @Test
  public void primalSolution_validConversion() {
    Model model = new Model("test_model");
    Variable x = model.addBinaryVariable("x");
    Variable y = model.addVariable("y");
    var proto = PrimalSolutionProto.newBuilder();
    proto.setObjectiveValue(15.0)
        .setFeasibilityStatus(SolutionStatusProto.SOLUTION_STATUS_FEASIBLE)
        .getVariableValuesBuilder()
        .addIds(x.getId())
        .addValues(10.0)
        .addIds(y.getId())
        .addValues(20.0);

    Solution.PrimalSolution primalSolution = new Solution.PrimalSolution(model, proto.build());

    assertThat(primalSolution.getVariableValues()).containsExactly(x, 10.0, y, 20.0);
    assertThat(primalSolution.getObjectiveValue()).isEqualTo(15.0);
    assertThat(primalSolution.getFeasibilityStatus()).isEqualTo(SolutionStatus.FEASIBLE);
  }

  @Test
  public void primalSolution_unspecifiedStatus_throws() {
    Model model = new Model("test_model");
    PrimalSolutionProto proto = PrimalSolutionProto.getDefaultInstance();

    var error = assertThrows(
        IllegalArgumentException.class, () -> new Solution.PrimalSolution(model, proto));
    assertThat(error).hasMessageThat().contains("must be set");
  }

  @Test
  public void primalRay_validConversion() {
    Model model = new Model("test_model");
    Variable x = model.addBinaryVariable("x");
    Variable y = model.addVariable("y");
    var primalRayProto = PrimalRayProto.newBuilder();
    primalRayProto.getVariableValuesBuilder()
        .addIds(x.getId())
        .addValues(10.0)
        .addIds(y.getId())
        .addValues(20.0);

    Solution.PrimalRay primalRay = new Solution.PrimalRay(model, primalRayProto.build());

    assertThat(primalRay.getVariableValues()).containsExactly(x, 10.0, y, 20.0);
  }

  @Test
  public void dualSolution_validConversion() {
    Model model = new Model("test_model");
    Variable x = model.addBinaryVariable("x");
    Variable y = model.addVariable("y");
    LinearConstraint l1 = model.addLinearConstraint("lc_1");
    LinearConstraint l2 = model.addLinearConstraint("lc_2");
    var proto = DualSolutionProto.newBuilder();
    proto.setObjectiveValue(15.0)
        .setFeasibilityStatus(SolutionStatusProto.SOLUTION_STATUS_FEASIBLE)
        .getDualValuesBuilder()
        .addIds(l1.getId())
        .addValues(10.0)
        .addIds(l2.getId())
        .addValues(20.0);
    proto.getReducedCostsBuilder().addIds(x.getId()).addValues(-10.0).addIds(y.getId()).addValues(
        -20.0);

    Solution.DualSolution dualSolution = new Solution.DualSolution(model, proto.build());

    assertThat(dualSolution.getDualValues()).containsExactly(l1, 10.0, l2, 20.0);
    assertThat(dualSolution.getReducedCosts()).containsExactly(x, -10.0, y, -20.0);
    assertThat(dualSolution.getObjectiveValue()).hasValue(15.0);
    assertThat(dualSolution.getFeasibilityStatus()).isEqualTo(SolutionStatus.FEASIBLE);
  }

  @Test
  public void dualSolution_unspecifiedStatus_throws() {
    Model model = new Model("test_model");
    DualSolutionProto proto = DualSolutionProto.getDefaultInstance();

    var error =
        assertThrows(IllegalArgumentException.class, () -> new Solution.DualSolution(model, proto));
    assertThat(error).hasMessageThat().contains("must be set");
  }

  @Test
  public void dualRay_validConversion() {
    Model model = new Model("test_model");
    Variable x = model.addBinaryVariable("x");
    Variable y = model.addVariable("y");
    LinearConstraint l1 = model.addLinearConstraint("lc_1");
    LinearConstraint l2 = model.addLinearConstraint("lc_2");
    var proto = DualRayProto.newBuilder();
    proto.getDualValuesBuilder()
        .addIds(l1.getId())
        .addValues(10.0)
        .addIds(l2.getId())
        .addValues(20.0);
    proto.getReducedCostsBuilder().addIds(x.getId()).addValues(-10.0).addIds(y.getId()).addValues(
        -20.0);

    Solution.DualRay dualRay = new Solution.DualRay(model, proto.build());

    assertThat(dualRay.getDualValues()).containsExactly(l1, 10.0, l2, 20.0);
    assertThat(dualRay.getReducedCosts()).containsExactly(x, -10.0, y, -20.0);
  }

  @Test
  public void solution_validConversion() {
    Model model = new Model("test_model");
    Variable x = model.addBinaryVariable("x");
    LinearConstraint l1 = model.addLinearConstraint("lc_1");
    var proto = SolutionProto.newBuilder();
    proto.getPrimalSolutionBuilder()
        .setObjectiveValue(15.0)
        .setFeasibilityStatus(SolutionStatusProto.SOLUTION_STATUS_FEASIBLE)
        .getVariableValuesBuilder()
        .addIds(x.getId())
        .addValues(10.0);
    proto.getDualSolutionBuilder()
        .setObjectiveValue(15.0)
        .setFeasibilityStatus(SolutionStatusProto.SOLUTION_STATUS_FEASIBLE)
        .getDualValuesBuilder()
        .addIds(l1.getId())
        .addValues(10.0);
    proto.getDualSolutionBuilder().getReducedCostsBuilder().addIds(x.getId()).addValues(-10.0);
    proto.getBasisBuilder()
        .setBasicDualFeasibility(SolutionStatusProto.SOLUTION_STATUS_FEASIBLE)
        .getConstraintStatusBuilder()
        .addIds(l1.getId())
        .addValues(BasisStatusProto.BASIS_STATUS_FREE);
    proto.getBasisBuilder().getVariableStatusBuilder().addIds(x.getId()).addValues(
        BasisStatusProto.BASIS_STATUS_FREE);

    Solution solution = new Solution(model, proto.build());

    assertThat(solution.getPrimalSolution()).isNotNull();
    assertThat(solution.getPrimalSolution().get().getVariableValues()).containsExactly(x, 10.0);
    assertThat(solution.getPrimalSolution().get().getObjectiveValue()).isEqualTo(15.0);
    assertThat(solution.getPrimalSolution().get().getFeasibilityStatus())
        .isEqualTo(SolutionStatus.FEASIBLE);
    assertThat(solution.getDualSolution()).isNotNull();
    assertThat(solution.getDualSolution().get().getDualValues()).containsExactly(l1, 10.0);
    assertThat(solution.getDualSolution().get().getReducedCosts()).containsExactly(x, -10.0);
    assertThat(solution.getDualSolution().get().getObjectiveValue()).hasValue(15.0);
    assertThat(solution.getDualSolution().get().getFeasibilityStatus())
        .isEqualTo(SolutionStatus.FEASIBLE);
    assertThat(solution.getBasis()).isNotNull();
    assertThat(solution.getBasis().get().getConstraintStatus())
        .containsExactly(l1, Basis.BasisStatus.FREE);
    assertThat(solution.getBasis().get().getVariableStatus())
        .containsExactly(x, Basis.BasisStatus.FREE);
    assertThat(solution.getBasis().get().getBasicDualFeasibility())
        .hasValue(SolutionStatus.FEASIBLE);
  }

  @Test
  public void solution_invalidPrimalSolution_throws() {
    Model model = new Model("test_model");
    SolutionProto proto = SolutionProto.newBuilder()
                              .setPrimalSolution(PrimalSolutionProto.getDefaultInstance())
                              .build();

    assertThrows(IllegalArgumentException.class, () -> new Solution(model, proto));
  }

  @Test
  public void solution_invalidDualSolution_throws() {
    Model model = new Model("test_model");
    SolutionProto proto =
        SolutionProto.newBuilder().setDualSolution(DualSolutionProto.getDefaultInstance()).build();

    assertThrows(IllegalArgumentException.class, () -> new Solution(model, proto));
  }

  @Test
  public void solution_invalidBasis_throws() {
    Model model = new Model("test_model");
    var badBasis = BasisProto.newBuilder()
                       .setVariableStatus(SparseBasisStatusVector.newBuilder().addIds(4).build())
                       .build();
    SolutionProto proto = SolutionProto.newBuilder().setBasis(badBasis).build();

    assertThrows(IllegalArgumentException.class, () -> new Solution(model, proto));
  }
}
