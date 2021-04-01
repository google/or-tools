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
using Google.OrTools.Sat;

public class RabbitsAndPheasantsSat
{
    static void Main()
    {
        // Creates the model.
        CpModel model = new CpModel();
        // Creates the variables.
        IntVar r = model.NewIntVar(0, 100, "r");
        IntVar p = model.NewIntVar(0, 100, "p");
        // 20 heads.
        model.Add(r + p == 20);
        // 56 legs.
        model.Add(4 * r + 2 * p == 56);

        // Creates a solver and solves the model.
        CpSolver solver = new CpSolver();
        CpSolverStatus status = solver.Solve(model);

        if (status == CpSolverStatus.Optimal)
        {
            Console.WriteLine(solver.Value(r) + " rabbits, and " + solver.Value(p) + " pheasants");
        }
    }
}
