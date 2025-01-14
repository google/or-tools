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

using Google.OrTools.Sat;

namespace Google.OrTools.Tests {
  public class SatSolverTest {
    [Theory]
    [InlineData(false)]
    [InlineData(true)]
    public void SolverTest(bool callGC) {
      CpModel model = new CpModel();

      int num_vals = 3;
      IntVar x = model.NewIntVar(0, num_vals - 1, "x");
      IntVar y = model.NewIntVar(0, num_vals - 1, "y");
      IntVar z = model.NewIntVar(0, num_vals - 1, "z");

      model.Add(x != y);

      CpSolver solver = new CpSolver();
      if (callGC) {
        GC.Collect();
      }
      CpSolverStatus status = solver.Solve(model);
    }
  }
} // namespace Google.Sample.Tests
