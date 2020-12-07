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

// Note: During compilation, there are a couple of
//       warnings about assigned but never used variables.
//       It's the characters a..z so it's quite benign.

public class Crossword
{
    /**
     *
     * Solving a simple crossword.
     * See http://www.hakank.org/or-tools/crossword2.py
     *
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("Crossword");

        //
        // data
        //
        String[] alpha = { "_", "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m",
                           "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z" };

        int a = 1;
        int b = 2;
        int c = 3;
        int d = 4;
        int e = 5;
        int f = 6;
        int g = 7;
        int h = 8;
        int i = 9;
        int j = 10;
        int k = 11;
        int l = 12;
        int m = 13;
        int n = 14;
        int o = 15;
        int p = 16;
        int q = 17;
        int r = 18;
        int s = 19;
        int t = 20;
        int u = 21;
        int v = 22;
        int w = 23;
        int x = 24;
        int y = 25;
        int z = 26;

        const int num_words = 15;
        int word_len = 5;

        int[,] AA = { { h, o, s, e, s },   //  HOSES
                      { l, a, s, e, r },   //  LASER
                      { s, a, i, l, s },   //  SAILS
                      { s, h, e, e, t },   //  SHEET
                      { s, t, e, e, r },   //  STEER
                      { h, e, e, l, 0 },   //  HEEL
                      { h, i, k, e, 0 },   //  HIKE
                      { k, e, e, l, 0 },   //  KEEL
                      { k, n, o, t, 0 },   //  KNOT
                      { l, i, n, e, 0 },   //  LINE
                      { a, f, t, 0, 0 },   //  AFT
                      { a, l, e, 0, 0 },   //  ALE
                      { e, e, l, 0, 0 },   //  EEL
                      { l, e, e, 0, 0 },   //  LEE
                      { t, i, e, 0, 0 } }; //  TIE

        int num_overlapping = 12;
        int[,] overlapping = { { 0, 2, 1, 0 }, //  s
                               { 0, 4, 2, 0 }, //  s

                               { 3, 1, 1, 2 }, //  i
                               { 3, 2, 4, 0 }, //  k
                               { 3, 3, 2, 2 }, //  e

                               { 6, 0, 1, 3 }, //  l
                               { 6, 1, 4, 1 }, //  e
                               { 6, 2, 2, 3 }, //  e

                               { 7, 0, 5, 1 },   //  l
                               { 7, 2, 1, 4 },   //  s
                               { 7, 3, 4, 2 },   //  e
                               { 7, 4, 2, 4 } }; //  r

        int N = 8;

        //
        // Decision variables
        //
        // for labeling on A and E
        IntVar[,] A = solver.MakeIntVarMatrix(num_words, word_len, 0, 26, "A");
        IntVar[] A_flat = A.Flatten();
        IntVar[] all = new IntVar[(num_words * word_len) + N];
        for (int I = 0; I < num_words; I++)
        {
            for (int J = 0; J < word_len; J++)
            {
                all[I * word_len + J] = A[I, J];
            }
        }

        IntVar[] E = solver.MakeIntVarArray(N, 0, num_words, "E");
        for (int I = 0; I < N; I++)
        {
            all[num_words * word_len + I] = E[I];
        }

        //
        // Constraints
        //
        solver.Add(E.AllDifferent());

        for (int I = 0; I < num_words; I++)
        {
            for (int J = 0; J < word_len; J++)
            {
                solver.Add(A[I, J] == AA[I, J]);
            }
        }

        // This contraint handles the overlappings.
        //
        // It's coded in MiniZinc as
        //
        //   forall(i in 1..num_overlapping) (
        //      A[E[overlapping[i,1]], overlapping[i,2]] =
        //      A[E[overlapping[i,3]], overlapping[i,4]]
        //   )
        // and in or-tools/Python as
        //   solver.Add(
        //      solver.Element(A_flat,E[overlapping[I][0]]*word_len+overlapping[I][1])
        //      ==
        //      solver.Element(A_flat,E[overlapping[I][2]]*word_len+overlapping[I][3]))
        //
        for (int I = 0; I < num_overlapping; I++)
        {
            solver.Add(A_flat.Element(E[overlapping[I, 0]] * word_len + overlapping[I, 1]) ==
                       A_flat.Element(E[overlapping[I, 2]] * word_len + overlapping[I, 3]));
        }

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(all, Solver.INT_VAR_DEFAULT, Solver.INT_VALUE_DEFAULT);

        solver.NewSearch(db);

        while (solver.NextSolution())
        {
            Console.WriteLine("E: ");
            for (int ee = 0; ee < N; ee++)
            {
                int e_val = (int)E[ee].Value();
                Console.Write(ee + ": (" + e_val + ") ");
                for (int ii = 0; ii < word_len; ii++)
                {
                    Console.Write(alpha[(int)A[ee, ii].Value()]);
                }
                Console.WriteLine();
            }

            Console.WriteLine();
        }

        Console.WriteLine("\nSolutions: {0}", solver.Solutions());
        Console.WriteLine("WallTime: {0}ms", solver.WallTime());
        Console.WriteLine("Failures: {0}", solver.Failures());
        Console.WriteLine("Branches: {0} ", solver.Branches());

        solver.EndSearch();
    }

    public static void Main(String[] args)
    {
        Solve();
    }
}
