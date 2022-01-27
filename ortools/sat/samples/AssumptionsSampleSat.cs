// Copyright 2021 Xiang Chen
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

// [START program]
using System;
using Google.OrTools.Sat;

public class AssumptionsSampleSat
{
    static void Main()
    {
        // Creates the model.
        // [START model]
        CpModel model = new CpModel();
        // [END model]

        // Creates the variables.
        // [START variables]
        IntVar x = model.NewIntVar(0, 10, "x");
        IntVar y = model.NewIntVar(0, 10, "y");
        IntVar z = model.NewIntVar(0, 10, "z");
        ILiteral a = model.NewBoolVar("a");
        ILiteral b = model.NewBoolVar("b");
        ILiteral c = model.NewBoolVar("c");
        // [END variables]

        // Creates the constraints.
        // [START constraints]
        model.Add(x > y).OnlyEnforceIf(a);
        model.Add(y > z).OnlyEnforceIf(b);
        model.Add(z > x).OnlyEnforceIf(c);
        // [END constraints]

        // Add assumptions
        model.AddAssumptions(new ILiteral[] { a, b, c });

        // Creates a solver and solves the model.
        // [START solve]
        CpSolver solver = new CpSolver();
        CpSolverStatus status = solver.Solve(model);
        Console.WriteLine(solver.SufficientAssumptionsForInfeasibility());
        // [END solve]
    }
}
// [END program]
