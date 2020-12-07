//
// Copyright 2012 Hakan Kjellerstrand
//
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
using Google.OrTools.ConstraintSolver;

public class Marathon2
{
    /**
     *
     * Marathon puzzle.
     *
     * From Xpress example
     * http://www.dashoptimization.com/home/cgi-bin/example.pl?id=mosel_puzzle_5_3
     * """
     * Dominique, Ignace, Naren, Olivier, Philippe, and Pascal
     * have arrived as the first six at the Paris marathon.
     * Reconstruct their arrival order from the following
     * information:
     * a) Olivier has not arrived last
     * b) Dominique, Pascal and Ignace have arrived before Naren
     *    and Olivier
     * c) Dominique who was third last year has improved this year.
     * d) Philippe is among the first four.
     * e) Ignace has arrived neither in second nor third position.
     * f) Pascal has beaten Naren by three positions.
     * g) Neither Ignace nor Dominique are on the fourth position.
     *
     * (c) 2002 Dash Associates
     * author: S. Heipcke, Mar. 2002
     * """
     *
     * Also see http://www.hakank.org/or-tools/marathon2.py
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("Marathon2");

        //
        // Data
        //
        int n = 6;
        String[] runners_str = { "Dominique", "Ignace", "Naren", "Olivier", "Philippe", "Pascal" };

        //
        // Decision variables
        //
        IntVar[] runners = solver.MakeIntVarArray(n, 1, n, "runners");
        IntVar Dominique = runners[0];
        IntVar Ignace = runners[1];
        IntVar Naren = runners[2];
        IntVar Olivier = runners[3];
        IntVar Philippe = runners[4];
        IntVar Pascal = runners[5];

        //
        // Constraints
        //
        solver.Add(runners.AllDifferent());

        // a: Olivier not last
        solver.Add(Olivier != n);

        // b: Dominique, Pascal and Ignace before Naren and Olivier
        solver.Add(Dominique < Naren);
        solver.Add(Dominique < Olivier);
        solver.Add(Pascal < Naren);
        solver.Add(Pascal < Olivier);
        solver.Add(Ignace < Naren);
        solver.Add(Ignace < Olivier);

        // c: Dominique better than third
        solver.Add(Dominique < 3);

        // d: Philippe is among the first four
        solver.Add(Philippe <= 4);

        // e: Ignace neither second nor third
        solver.Add(Ignace != 2);
        solver.Add(Ignace != 3);

        // f: Pascal three places earlier than Naren
        solver.Add(Pascal + 3 == Naren);

        // g: Neither Ignace nor Dominique on fourth position
        solver.Add(Ignace != 4);
        solver.Add(Dominique != 4);

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(runners, Solver.CHOOSE_MIN_SIZE_LOWEST_MIN, Solver.ASSIGN_CENTER_VALUE);

        solver.NewSearch(db);

        while (solver.NextSolution())
        {
            int[] runners_val = new int[n];
            Console.Write("runners: ");
            for (int i = 0; i < n; i++)
            {
                runners_val[i] = (int)runners[i].Value();
                Console.Write(runners_val[i] + " ");
            }
            Console.WriteLine("\nPlaces:");
            for (int i = 1; i < n + 1; i++)
            {
                for (int j = 0; j < n; j++)
                {
                    if (runners_val[j] == i)
                    {
                        Console.WriteLine("{0}: {1}", i, runners_str[j]);
                    }
                }
            }
        }

        Console.WriteLine("\nSolutions: " + solver.Solutions());
        Console.WriteLine("WallTime: " + solver.WallTime() + "ms ");
        Console.WriteLine("Failures: " + solver.Failures());
        Console.WriteLine("Branches: " + solver.Branches());

        solver.EndSearch();
    }

    public static void Main(String[] args)
    {
        Solve();
    }
}
