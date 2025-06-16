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
using Xunit;

using Google.OrTools.ConstraintSolver;

namespace Google.OrTools.Tests
{
public class ConstraintSolverTest
{
    [Theory]
    [InlineData(false)]
    [InlineData(true)]
    public void SolverTest(bool callGC)
    {
        Solver solver = new Solver("Solver");
        IntVar x = solver.MakeIntVar(3, 7, "x");

        if (callGC)
        {
            GC.Collect();
        }

        Assert.Equal(3, x.Min());
        Assert.Equal(7, x.Max());
        Assert.Equal("x(3..7)", x.ToString());
    }
}
} // namespace Google.Sample.Tests
