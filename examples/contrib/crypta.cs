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

public class Crypta
{
    /**
     *
     * Cryptarithmetic puzzle.
     *
     * Prolog benchmark problem GNU Prolog (crypta.pl)
     * """
     * Name           : crypta.pl
     * Title          : crypt-arithmetic
     * Original Source: P. Van Hentenryck's book
     * Adapted by     : Daniel Diaz - INRIA France
     * Date           : September 1992
     *
     * Solve the operation:
     *
     *    B A I J J A J I I A H F C F E B B J E A
     *  + D H F G A B C D I D B I F F A G F E J E
     *  -----------------------------------------
     *  = G J E G A C D D H F A F J B F I H E E F
     * """
     *
     *
     * Also see http://hakank.org/or-tools/crypta.py
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("Crypta");

        //
        // Decision variables
        //
        IntVar A = solver.MakeIntVar(0, 9, "A");
        IntVar B = solver.MakeIntVar(0, 9, "B");
        IntVar C = solver.MakeIntVar(0, 9, "C");
        IntVar D = solver.MakeIntVar(0, 9, "D");
        IntVar E = solver.MakeIntVar(0, 9, "E");
        IntVar F = solver.MakeIntVar(0, 9, "F");
        IntVar G = solver.MakeIntVar(0, 9, "G");
        IntVar H = solver.MakeIntVar(0, 9, "H");
        IntVar I = solver.MakeIntVar(0, 9, "I");
        IntVar J = solver.MakeIntVar(0, 9, "J");

        IntVar[] LD = new IntVar[] { A, B, C, D, E, F, G, H, I, J };

        IntVar Sr1 = solver.MakeIntVar(0, 1, "Sr1");
        IntVar Sr2 = solver.MakeIntVar(0, 1, "Sr2");

        //
        // Constraints
        //
        solver.Add(LD.AllDifferent());
        solver.Add(B >= 1);
        solver.Add(D >= 1);
        solver.Add(G >= 1);

        solver.Add((A + 10 * E + 100 * J + 1000 * B + 10000 * B + 100000 * E + 1000000 * F + E + 10 * J + 100 * E +
                    1000 * F + 10000 * G + 100000 * A + 1000000 * F) ==
                   (F + 10 * E + 100 * E + 1000 * H + 10000 * I + 100000 * F + 1000000 * B + 10000000 * Sr1));

        solver.Add((C + 10 * F + 100 * H + 1000 * A + 10000 * I + 100000 * I + 1000000 * J + F + 10 * I + 100 * B +
                    1000 * D + 10000 * I + 100000 * D + 1000000 * C + Sr1) ==
                   (J + 10 * F + 100 * A + 1000 * F + 10000 * H + 100000 * D + 1000000 * D + 10000000 * Sr2));

        solver.Add((A + 10 * J + 100 * J + 1000 * I + 10000 * A + 100000 * B + B + 10 * A + 100 * G + 1000 * F +
                    10000 * H + 100000 * D + Sr2) == (C + 10 * A + 100 * G + 1000 * E + 10000 * J + 100000 * G));

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(LD, Solver.INT_VAR_DEFAULT, Solver.INT_VALUE_DEFAULT);

        solver.NewSearch(db);

        while (solver.NextSolution())
        {
            for (int i = 0; i < 10; i++)
            {
                Console.Write(LD[i].ToString() + " ");
            }
            Console.WriteLine();
        }

        Console.WriteLine("\nWallTime: " + solver.WallTime() + "ms ");
        Console.WriteLine("Failures: " + solver.Failures());
        Console.WriteLine("Branches: " + solver.Branches());

        solver.EndSearch();
    }

    public static void Main(String[] args)
    {
        Solve();
    }
}
