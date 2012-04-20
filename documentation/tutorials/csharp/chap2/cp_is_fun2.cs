/*
Copyright 2012 Google
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

/*
Cryptoarithmetic puzzle

First attempt to solve equation CP + IS + FUN = TRUE
where each letter represents a unique digit.

This problem has 72 different solutions in base 10.

Use of SolutionCollectors.
Use of Solve().
*/
using System;
using Google.OrTools.ConstraintSolver;


public class cp_is_fun1
{
    //  We don't need helper functions here
    //  Csharp syntax is easier than C++ syntax!

    private static void CPisFun (int kBase)
    {
        //  Constraint Programming engine
        Solver solver = new Solver ("CP is fun!");

        // Decision variables
        IntVar c = solver.MakeIntVar (1, kBase - 1, "C");
        IntVar p = solver.MakeIntVar (0, kBase - 1, "P");
        IntVar i = solver.MakeIntVar (1, kBase - 1, "I");
        IntVar s = solver.MakeIntVar (0, kBase - 1, "S");
        IntVar f = solver.MakeIntVar (1, kBase - 1, "F");
        IntVar u = solver.MakeIntVar (0, kBase - 1, "U");
        IntVar n = solver.MakeIntVar (0, kBase - 1, "N");
        IntVar t = solver.MakeIntVar (1, kBase - 1, "T");
        IntVar r = solver.MakeIntVar (0, kBase - 1, "R");
        IntVar e = solver.MakeIntVar (0, kBase - 1, "E");

        // We need to group variables in a vector to be able to use
        // the global constraint AllDifferent
        IntVar[] letters = new IntVar[] { c, p, i, s, f, u, n, t, r, e};

        // Check if we have enough digits
        if (kBase < letters.Length) {
          throw new Exception("kBase < letters.Length");
        }

        //  Constraints
        solver.Add (letters.AllDifferent ());

        // CP + IS + FUN = TRUE
        solver.Add (p + s + n + kBase * (c + i + u) + kBase * kBase * f ==
               e + kBase * u + kBase * kBase * r + kBase * kBase * kBase * t);

        SolutionCollector all_solutions = solver.MakeAllSolutionCollector();
        //  Add the interesting variables to the SolutionCollector
        all_solutions.Add(c);
        all_solutions.Add(p);
        //  Create the variable kBase * c + p
        IntVar v1 = solver.MakeSum(solver.MakeProd(c, kBase), p).Var();
        //  Add it to the SolutionCollector
        all_solutions.Add(v1);

        //  Decision Builder: hot to scour the search tree
        DecisionBuilder db = solver.MakePhase (letters,
                                               Solver.CHOOSE_FIRST_UNBOUND,
                                               Solver.ASSIGN_MIN_VALUE);
        solver.Solve(db, all_solutions);

        //  Retrieve the solutions
        int numberSolutions = all_solutions.SolutionCount();
        Console.WriteLine ("Number of solutions: " + numberSolutions);

        for (int index = 0; index < numberSolutions; ++index) {
            Assignment solution = all_solutions.Solution(index);
            Console.WriteLine ("Solution found:");
            Console.WriteLine ("v1=" + solution.Value(v1));
        }
    }

    public static void Main (String[] args)
    {
        int kBase = 10;
        if (args.Length > 0) {
            kBase = Convert.ToInt32(args[0]);
        }
        CPisFun(kBase);
    }
}
