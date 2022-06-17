// Copyright 2010-2022 Google LLC
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

package com.google.ortools.modelbuilder;

import static com.google.common.truth.Truth.assertThat;

import com.google.ortools.Loader;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

public final class ModelBuilderTest {
  @BeforeEach
  public void setUp() {
    Loader.loadNativeLibraries();
  }

  @Test
  public void runMinimalLinearExample_ok() {
    ModelBuilder model = new ModelBuilder();
    model.setName("minimal_linear_example");
    double infinity = java.lang.Double.POSITIVE_INFINITY;
    Variable x1 = model.newNumVar(0.0, infinity, "x1");
    Variable x2 = model.newNumVar(0.0, infinity, "x2");
    Variable x3 = model.newNumVar(0.0, infinity, "x3");

    assertThat(model.numVariables()).isEqualTo(3);
    assertThat(x1.getIntegrality()).isFalse();
    assertThat(x1.getLowerBound()).isEqualTo(0.0);
    assertThat(x2.getUpperBound()).isEqualTo(infinity);
    x1.setLowerBound(1.0);
    assertThat(x1.getLowerBound()).isEqualTo(1.0);

    LinearConstraint c0 = model.addLessOrEqual(LinearExpr.sum(new Variable[] {x1, x2, x3}), 100.0);
    assertThat(c0.getUpperBound()).isEqualTo(100.0);
    LinearConstraint c1 =
        model
            .addLessOrEqual(
                LinearExpr.newBuilder().addTerm(x1, 10.0).addTerm(x2, 4.0).addTerm(x3, 5.0), 600.0)
            .withName("c1");
    assertThat(c1.getName()).isEqualTo("c1");
    LinearConstraint c2 = model.addLessOrEqual(
        LinearExpr.newBuilder().addTerm(x1, 2.0).addTerm(x2, 2.0).addTerm(x3, 6.0), 300.0);
    assertThat(c2.getUpperBound()).isEqualTo(300.0);

    model.maximize(
        LinearExpr.weightedSum(new Variable[] {x1, x2, x3}, new double[] {10.0, 6, 4.0}));
    assertThat(x3.getObjectiveCoefficient()).isEqualTo(4.0);
    assertThat(model.getObjectiveOffset()).isEqualTo(0.0);
    model.setObjectiveOffset(-5.5);
    assertThat(model.getObjectiveOffset()).isEqualTo(-5.5);

    ModelSolver solver = new ModelSolver("glop");
    assertThat(solver.solverIsSupported()).isTrue();
    assertThat(solver.solve(model)).isEqualTo(SolveStatus.OPTIMAL);

    assertThat(solver.getObjectiveValue())
        .isWithin(1e-5)
        .of(733.333333 + model.getObjectiveOffset());
    assertThat(solver.getValue(x1)).isWithin(1e-5).of(33.333333);
    assertThat(solver.getValue(x2)).isWithin(1e-5).of(66.6666673);
    assertThat(solver.getValue(x3)).isWithin(1e-5).of(0.0);

    double dualObjectiveValue = solver.getDualValue(c0) * c0.getUpperBound()
        + solver.getDualValue(c1) * c1.getUpperBound()
        + solver.getDualValue(c2) * c2.getUpperBound() + model.getObjectiveOffset();
    assertThat(solver.getObjectiveValue()).isWithin(1e-5).of(dualObjectiveValue);

    assertThat(solver.getReducedCost(x1)).isWithin(1e-5).of(0.0);
    assertThat(solver.getReducedCost(x2)).isWithin(1e-5).of(0.0);
    assertThat(solver.getReducedCost(x3))
        .isWithin(1e-5)
        .of(4.0 - 1.0 * solver.getDualValue(c0) - 5.0 * solver.getDualValue(c1));

    assertThat(model.exportToLpString(false)).contains("minimal_linear_example");
    assertThat(model.exportToMpsString(false)).contains("minimal_linear_example");
  }
}
