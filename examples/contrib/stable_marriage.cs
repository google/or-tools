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
using System.Collections;
using System.IO;
using System.Text.RegularExpressions;
using Google.OrTools.ConstraintSolver;

public class StableMarriage
{
    /**
     *
     * Solves some stable marriage problems.
     * See http://www.hakank.org/or-tools/stable_marriage.py
     *
     */
    private static void Solve(int[][][] ranks, String problem_name)
    {
        Solver solver = new Solver("StableMarriage");

        //
        // data
        //
        int n = ranks[0].Length;

        Console.WriteLine("\n#####################");
        Console.WriteLine("Problem: " + problem_name);

        int[][] rankWomen = ranks[0];
        int[][] rankMen = ranks[1];

        //
        // Decision variables
        //
        IntVar[] wife = solver.MakeIntVarArray(n, 0, n - 1, "wife");
        IntVar[] husband = solver.MakeIntVarArray(n, 0, n - 1, "husband");

        //
        // Constraints
        //
        // (The comments below are the Comet code)

        //
        //   forall(m in Men)
        //      cp.post(husband[wife[m]] == m);
        for (int m = 0; m < n; m++)
        {
            solver.Add(husband.Element(wife[m]) == m);
        }

        //   forall(w in Women)
        //     cp.post(wife[husband[w]] == w);
        for (int w = 0; w < n; w++)
        {
            solver.Add(wife.Element(husband[w]) == w);
        }

        //   forall(m in Men, o in Women)
        //       cp.post(rankMen[m,o] < rankMen[m, wife[m]] =>
        //               rankWomen[o,husband[o]] < rankWomen[o,m]);
        for (int m = 0; m < n; m++)
        {
            for (int o = 0; o < n; o++)
            {
                IntVar b1 = rankMen[m].Element(wife[m]) > rankMen[m][o];
                IntVar b2 = rankWomen[o].Element(husband[o]) < rankWomen[o][m];
                solver.Add(b1 <= b2);
            }
        }

        //   forall(w in Women, o in Men)
        //      cp.post(rankWomen[w,o] < rankWomen[w,husband[w]] =>
        //              rankMen[o,wife[o]] < rankMen[o,w]);
        for (int w = 0; w < n; w++)
        {
            for (int o = 0; o < n; o++)
            {
                IntVar b1 = rankWomen[w].Element(husband[w]) > rankWomen[w][o];
                IntVar b2 = rankMen[o].Element(wife[o]) < rankMen[o][w];
                solver.Add(b1 <= b2);
            }
        }

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(wife, Solver.INT_VAR_DEFAULT, Solver.INT_VALUE_DEFAULT);

        solver.NewSearch(db);

        while (solver.NextSolution())
        {
            Console.Write("wife   : ");
            for (int i = 0; i < n; i++)
            {
                Console.Write(wife[i].Value() + " ");
            }
            Console.Write("\nhusband: ");
            for (int i = 0; i < n; i++)
            {
                Console.Write(husband[i].Value() + " ");
            }
            Console.WriteLine("\n");
        }

        Console.WriteLine("\nSolutions: {0}", solver.Solutions());
        Console.WriteLine("WallTime: {0}ms", solver.WallTime());
        Console.WriteLine("Failures: {0}", solver.Failures());
        Console.WriteLine("Branches: {0} ", solver.Branches());

        solver.EndSearch();
    }

    public static void Main(String[] args)
    {
        //
        // From Pascal Van Hentenryck's OPL book
        //
        int[][][] van_hentenryck = {
            // rankWomen
            new int[][] { new int[] { 1, 2, 4, 3, 5 }, new int[] { 3, 5, 1, 2, 4 }, new int[] { 5, 4, 2, 1, 3 },
                          new int[] { 1, 3, 5, 4, 2 }, new int[] { 4, 2, 3, 5, 1 } },

            // rankMen
            new int[][] { new int[] { 5, 1, 2, 4, 3 }, new int[] { 4, 1, 3, 2, 5 }, new int[] { 5, 3, 2, 4, 1 },
                          new int[] { 1, 5, 4, 3, 2 }, new int[] { 4, 3, 2, 1, 5 } }
        };

        //
        // Data from MathWorld
        // http://mathworld.wolfram.com/StableMarriageProblem.html
        //
        int[][][] mathworld = {
            // rankWomen
            new int[][] { new int[] { 3, 1, 5, 2, 8, 7, 6, 9, 4 }, new int[] { 9, 4, 8, 1, 7, 6, 3, 2, 5 },
                          new int[] { 3, 1, 8, 9, 5, 4, 2, 6, 7 }, new int[] { 8, 7, 5, 3, 2, 6, 4, 9, 1 },
                          new int[] { 6, 9, 2, 5, 1, 4, 7, 3, 8 }, new int[] { 2, 4, 5, 1, 6, 8, 3, 9, 7 },
                          new int[] { 9, 3, 8, 2, 7, 5, 4, 6, 1 }, new int[] { 6, 3, 2, 1, 8, 4, 5, 9, 7 },
                          new int[] { 8, 2, 6, 4, 9, 1, 3, 7, 5 } },

            // rankMen
            new int[][] { new int[] { 7, 3, 8, 9, 6, 4, 2, 1, 5 }, new int[] { 5, 4, 8, 3, 1, 2, 6, 7, 9 },
                          new int[] { 4, 8, 3, 9, 7, 5, 6, 1, 2 }, new int[] { 9, 7, 4, 2, 5, 8, 3, 1, 6 },
                          new int[] { 2, 6, 4, 9, 8, 7, 5, 1, 3 }, new int[] { 2, 7, 8, 6, 5, 3, 4, 1, 9 },
                          new int[] { 1, 6, 2, 3, 8, 5, 4, 9, 7 }, new int[] { 5, 6, 9, 1, 2, 8, 4, 3, 7 },
                          new int[] { 6, 1, 4, 7, 5, 8, 3, 9, 2 } }
        };

        //
        // Data from
        // http://www.csee.wvu.edu/~ksmani/courses/fa01/random/lecnotes/lecture5.pdf
        //
        int[][][] problem3 = {// rankWomen
                              new int[][] { new int[] { 1, 2, 3, 4 }, new int[] { 4, 3, 2, 1 },
                                            new int[] { 1, 2, 3, 4 }, new int[] { 3, 4, 1, 2 } },

                              // rankMen
                              new int[][] { new int[] { 1, 2, 3, 4 }, new int[] { 2, 1, 3, 4 },
                                            new int[] { 1, 4, 3, 2 }, new int[] { 4, 3, 1, 2 } }
        };

        //
        // Data from
        // http://www.comp.rgu.ac.uk/staff/ha/ZCSP/additional_problems/stable_marriage/stable_marriage.pdf
        // page 4
        //
        int[][][] problem4 = {// rankWomen
                              new int[][] { new int[] { 1, 5, 4, 6, 2, 3 }, new int[] { 4, 1, 5, 2, 6, 3 },
                                            new int[] { 6, 4, 2, 1, 5, 3 }, new int[] { 1, 5, 2, 4, 3, 6 },
                                            new int[] { 4, 2, 1, 5, 6, 3 }, new int[] { 2, 6, 3, 5, 1, 4 } },

                              // rankMen
                              new int[][] { new int[] { 1, 4, 2, 5, 6, 3 }, new int[] { 3, 4, 6, 1, 5, 2 },
                                            new int[] { 1, 6, 4, 2, 3, 5 }, new int[] { 6, 5, 3, 4, 2, 1 },
                                            new int[] { 3, 1, 2, 4, 5, 6 }, new int[] { 2, 3, 1, 6, 5, 4 } }
        };

        Solve(van_hentenryck, "Van Hentenryck");
        Solve(mathworld, "MathWorld");
        Solve(problem3, "Problem 3");
        Solve(problem4, "Problem 4");
    }
}
