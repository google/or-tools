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

// [START solution_printing]
public class VarArraySolutionPrinter : CpSolverSolutionCallback
{
    public VarArraySolutionPrinter(IntVar[] variables)
    {
        variables_ = variables;
    }

    public override void OnSolutionCallback()
    {
        {
            foreach (IntVar v in variables_)
            {
                Console.Write(String.Format("  {0}={1}", v.ShortString(), Value(v)));
            }
            Console.WriteLine();
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
// [END solution_printing]

public class CpIsFunSat
{
    // Solve the CP+IS+FUN==TRUE cryptarithm.
    static void Main()
    {
        // Constraint programming engine
        CpModel model = new CpModel();

        // [START variables]
        int kBase = 10;

        IntVar c = model.NewIntVar(1, kBase - 1, "C");
        IntVar p = model.NewIntVar(0, kBase - 1, "P");
        IntVar i = model.NewIntVar(1, kBase - 1, "I");
        IntVar s = model.NewIntVar(0, kBase - 1, "S");
        IntVar f = model.NewIntVar(1, kBase - 1, "F");
        IntVar u = model.NewIntVar(0, kBase - 1, "U");
        IntVar n = model.NewIntVar(0, kBase - 1, "N");
        IntVar t = model.NewIntVar(1, kBase - 1, "T");
        IntVar r = model.NewIntVar(0, kBase - 1, "R");
        IntVar e = model.NewIntVar(0, kBase - 1, "E");

        // We need to group variables in a list to use the constraint AllDifferent.
        IntVar[] letters = new IntVar[] { c, p, i, s, f, u, n, t, r, e };
        // [END variables]

        // [START constraints]
        // Define constraints.
        model.AddAllDifferent(letters);

        // CP + IS + FUN = TRUE
        model.Add(c * kBase + p + i * kBase + s + f * kBase * kBase + u * kBase + n ==
                  t * kBase * kBase * kBase + r * kBase * kBase + u * kBase + e);
        // [END constraints]

        // [START solve]
        // Creates a solver and solves the model.
        CpSolver solver = new CpSolver();
        VarArraySolutionPrinter cb = new VarArraySolutionPrinter(letters);
        // Search for all solutions.
        solver.StringParameters = "enumerate_all_solutions:true";
        // And solve.
        solver.Solve(model, cb);
        // [END solve]

        Console.WriteLine("Statistics");
        Console.WriteLine($"  - conflicts : {solver.NumConflicts()}");
        Console.WriteLine($"  - branches  : {solver.NumBranches()}");
        Console.WriteLine($"  - wall time : {solver.WallTime()} s");
        Console.WriteLine($"  - number of solutions found: {cb.SolutionCount()}");
    }
}
// [END program]
