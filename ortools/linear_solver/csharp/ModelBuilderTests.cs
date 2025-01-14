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

using System;
using System.Collections.Generic;
using Xunit;
using Google.OrTools.ModelBuilder;

namespace Google.OrTools.Tests
{
public class ModelBuilderTest
{

    [Fact]
    public void BasicApiTest()
    {
        Model model = new Model();
        Variable v1 = model.NewIntVar(-10, 10, "v1");
        Variable v2 = model.NewIntVar(-10, 10, "v2");
        Variable v3 = model.NewIntVar(-100000, 100000, "v3");
        model.AddLinearConstraint(v1 + v2, -1000000, 100000);
        model.AddLinearConstraint(v1 + 2 * v2 - v3, 0, 100000);
        model.Maximize(v3);

        Solver solver = new Solver("scip");
        if (!solver.SolverIsSupported())
        {
            return;
        }
        SolveStatus status = solver.Solve(model);
        Assert.Equal(SolveStatus.OPTIMAL, status);

        Assert.Equal(30, solver.ObjectiveValue);
        Assert.Equal(10, solver.Value(v1));
        Assert.Equal(10, solver.Value(v2));
        Assert.Equal(30, solver.Value(v3));
    }

    [Fact]
    public void EnforcedLinearApiTest()
    {
        Model model = new Model();
        model.Name = "minimal enforced linear test";
        double infinity = double.PositiveInfinity;
        Variable x = model.NewNumVar(0.0, infinity, "x");
        Variable y = model.NewNumVar(0.0, infinity, "y");
        Variable z = model.NewBoolVar("z");

        Assert.Equal(3, model.VariablesCount());

        EnforcedLinearConstraint c0 = model.AddEnforced(x + 2 * y >= 10.0, z, false);
        Assert.Equal(1, model.ConstraintsCount());
        Assert.Equal(10.0, c0.LowerBound);
        Assert.Equal(infinity, c0.UpperBound);
        Assert.Equal(c0.IndicatorVariable.Index, z.Index);
        Assert.False(c0.IndicatorValue);
    }
}

} // namespace Google.OrTools.Tests
