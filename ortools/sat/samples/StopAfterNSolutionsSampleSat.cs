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

public class VarArraySolutionPrinterWithLimit : CpSolverSolutionCallback
{
    public VarArraySolutionPrinterWithLimit(IntVar[] variables, int solution_limit)
    {
        variables_ = variables;
        solution_limit_ = solution_limit;
    }

    public override void OnSolutionCallback()
    {
        Console.WriteLine(String.Format("Solution #{0}: time = {1:F2} s", solution_count_, WallTime()));
        foreach (IntVar v in variables_)
        {
            Console.WriteLine(String.Format("  {0} = {1}", v.ShortString(), Value(v)));
        }
        solution_count_++;
        if (solution_count_ >= solution_limit_)
        {
            Console.WriteLine(String.Format("Stopping search after {0} solutions", solution_limit_));
            StopSearch();
        }
    }

    public int SolutionCount()
    {
        return solution_count_;
    }

    private int solution_count_;
    private IntVar[] variables_;
    private int solution_limit_;
}

public class StopAfterNSolutionsSampleSat
{
    static void Main()
    {
        // Creates the model.
        CpModel model = new CpModel();
        // Creates the variables.
        int num_vals = 3;

        IntVar x = model.NewIntVar(0, num_vals - 1, "x");
        IntVar y = model.NewIntVar(0, num_vals - 1, "y");
        IntVar z = model.NewIntVar(0, num_vals - 1, "z");

        // Creates a solver and solves the model.
        CpSolver solver = new CpSolver();
        VarArraySolutionPrinterWithLimit cb = new VarArraySolutionPrinterWithLimit(new IntVar[] { x, y, z }, 5);
        solver.StringParameters = "enumerate_all_solutions:true";
        solver.Solve(model, cb);
        Console.WriteLine(String.Format("Number of solutions found: {0}", cb.SolutionCount()));
    }
}
// [END program]
