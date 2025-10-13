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

using Google.OrTools.LinearSolver;

namespace Google.OrTools.Tests
{
public class LinearSolverTest
{
    [Theory]
    [InlineData(false)]
    [InlineData(true)]
    public void SolverTest(bool callGC)
    {
        Solver solver = new Solver("Solver", Solver.OptimizationProblemType.CLP_LINEAR_PROGRAMMING);

        if (callGC)
        {
            GC.Collect();
        }
    }
}
} // namespace Google.Sample.Tests
