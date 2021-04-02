// Copyright 2010-2021 Google LLC
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
using System.Collections.Generic;
using Google.OrTools.ConstraintSolver;

namespace Google.OrTools.Tests
{
    public class Issue18Test
    {
        [Fact]
        public void NewSearchTest()
        {
            Solver solver = new Google.OrTools.ConstraintSolver.Solver("p");

            // creating dummy variables
            List<IntVar> vars = new List<IntVar>();
            for (int i = 0; i < 100000; i++)
            {
                vars.Add(solver.MakeIntVar(0, 1));
            }

            IntExpr globalSum = solver.MakeSum(vars.ToArray());

            DecisionBuilder db = solver.MakePhase(vars.ToArray(), Google.OrTools.ConstraintSolver.Solver.INT_VAR_SIMPLE,
                                                  Google.OrTools.ConstraintSolver.Solver.INT_VALUE_SIMPLE);

            solver.NewSearch(db, new OptimizeVar(solver, true, globalSum.Var(), 100));

            // force Garbage Collector
            GC.Collect();
            GC.WaitForPendingFinalizers();

            // Try to read all solutions
            int count = 0;
            while (solver.NextSolution())
            {
                count++;
                // Console.WriteLine("solution " + globalSum.Var().Value());
            }
            Console.WriteLine("Solutions: " + count);
        }
    }
} // namespace Google.OrTools.Tests
