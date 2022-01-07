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
// Cryptarithmetic puzzle
//
// First attempt to solve equation CP + IS + FUN = TRUE
// where each letter represents a unique digit.
//
// This problem has 72 different solutions in base 10.
// [START import]
using System;
using Google.OrTools.ConstraintSolver;
// [END import]

public class CpIsFunCp
{
    public static void Main(String[] args)
    {
        // Instantiate the solver.
        // [START solver]
        Solver solver = new Solver("CP is fun!");
        // [END solver]

        // [START variables]
        const int kBase = 10;

        // Decision variables.
        IntVar c = solver.MakeIntVar(1, kBase - 1, "C");
        IntVar p = solver.MakeIntVar(0, kBase - 1, "P");
        IntVar i = solver.MakeIntVar(1, kBase - 1, "I");
        IntVar s = solver.MakeIntVar(0, kBase - 1, "S");
        IntVar f = solver.MakeIntVar(1, kBase - 1, "F");
        IntVar u = solver.MakeIntVar(0, kBase - 1, "U");
        IntVar n = solver.MakeIntVar(0, kBase - 1, "N");
        IntVar t = solver.MakeIntVar(1, kBase - 1, "T");
        IntVar r = solver.MakeIntVar(0, kBase - 1, "R");
        IntVar e = solver.MakeIntVar(0, kBase - 1, "E");

        // Group variables in a vector so that we can use AllDifferent.
        IntVar[] letters = new IntVar[] { c, p, i, s, f, u, n, t, r, e };

        // Verify that we have enough digits.
        if (kBase < letters.Length)
        {
            throw new Exception("kBase < letters.Length");
        }
        // [END variables]

        // Define constraints.
        // [START constraints]
        solver.Add(letters.AllDifferent());

        // CP + IS + FUN = TRUE
        solver.Add(p + s + n + kBase * (c + i + u) + kBase * kBase * f ==
                   e + kBase * u + kBase * kBase * r + kBase * kBase * kBase * t);
        // [END constraints]

        // [START solve]
        int SolutionCount = 0;
        // Create the decision builder to search for solutions.
        DecisionBuilder db = solver.MakePhase(letters, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MIN_VALUE);
        solver.NewSearch(db);
        while (solver.NextSolution())
        {
            Console.Write("C=" + c.Value() + " P=" + p.Value());
            Console.Write(" I=" + i.Value() + " S=" + s.Value());
            Console.Write(" F=" + f.Value() + " U=" + u.Value());
            Console.Write(" N=" + n.Value() + " T=" + t.Value());
            Console.Write(" R=" + r.Value() + " E=" + e.Value());
            Console.WriteLine();

            // Is CP + IS + FUN = TRUE?
            if (p.Value() + s.Value() + n.Value() + kBase * (c.Value() + i.Value() + u.Value()) +
                    kBase * kBase * f.Value() !=
                e.Value() + kBase * u.Value() + kBase * kBase * r.Value() + kBase * kBase * kBase * t.Value())
            {
                throw new Exception("CP + IS + FUN != TRUE");
            }
            SolutionCount++;
        }
        solver.EndSearch();
        Console.WriteLine($"Number of solutions found: {SolutionCount}");
        // [END solve]
    }
}
// [END program]
