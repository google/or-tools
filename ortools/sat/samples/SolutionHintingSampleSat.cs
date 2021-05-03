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

// [START program]
using System;
using Google.OrTools.Sat;

// [START print_solution]
public class VarArraySolutionPrinter : CpSolverSolutionCallback
{
    public VarArraySolutionPrinter(IntVar[] variables)
    {
        variables_ = variables;
    }

    public override void OnSolutionCallback()
    {
        {
            Console.WriteLine(String.Format("Solution #{0}: time = {1:F2} s", solution_count_, WallTime()));
            foreach (IntVar v in variables_)
            {
                Console.WriteLine(String.Format("  {0} = {1}", v.ShortString(), Value(v)));
            }
            solution_count_++;
        }
    }

    public int SolutionCount()
    {
        return solution_count_;
    }

    private int solution_count_;
    private IntVar[] variables_;
}
// [END print_solution]

public class SolutionHintingSampleSat
{
    static void Main()
    {
        // Creates the model.
        // [START model]
        CpModel model = new CpModel();
        // [END model]

        // Creates the variables.
        // [START variables]
        int num_vals = 3;

        IntVar x = model.NewIntVar(0, num_vals - 1, "x");
        IntVar y = model.NewIntVar(0, num_vals - 1, "y");
        IntVar z = model.NewIntVar(0, num_vals - 1, "z");
        // [END variables]

        // Creates the constraints.
        // [START constraints]
        model.Add(x != y);
        // [END constraints]

        // Solution hinting: x <- 1, y <- 2
        model.AddHint(x, 1);
        model.AddHint(y, 2);

        // [START objective]
        model.Maximize(LinearExpr.ScalProd(new IntVar[] { x, y, z }, new int[] { 1, 2, 3 }));
        // [END objective]

        // Creates a solver and solves the model.
        // [START solve]
        CpSolver solver = new CpSolver();
        VarArraySolutionPrinter cb = new VarArraySolutionPrinter(new IntVar[] { x, y, z });
        CpSolverStatus status = solver.Solve(model, cb);
        // [END solve]
    }
}
// [END program]
