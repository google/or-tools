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
using System.Collections.Generic;
using System.Linq;
using Google.OrTools.ConstraintSolver;

public class LabeledDice
{
    /**
     *
     * Labeled dice problem.
     *
     * From Jim Orlin 'Colored letters, labeled dice: a logic puzzle'
     * http://jimorlin.wordpress.com/2009/02/17/colored-letters-labeled-dice-a-logic-puzzle/
     * """
     * My daughter Jenn bough a puzzle book, and showed me a cute puzzle.  There
     * are 13 words as follows:  BUOY, CAVE, CELT, FLUB, FORK, HEMP, JUDY,
     * JUNK, LIMN, QUIP, SWAG, VISA, WISH.
     *
     * There are 24 different letters that appear in the 13 words.  The question
     * is:  can one assign the 24 letters to 4 different cubes so that the
     * four letters of each word appears on different cubes.  (There is one
     * letter from each word on each cube.)  It might be fun for you to try
     * it.  I'll give a small hint at the end of this post. The puzzle was
     * created by Humphrey Dudley.
     * """
     *
     * Jim Orlin's followup 'Update on Logic Puzzle':
     * http://jimorlin.wordpress.com/2009/02/21/update-on-logic-puzzle/
     *
     *
     * Also see http://www.hakank.org/or-tools/labeled_dice.py
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("LabeledDice");

        //
        // Data
        //
        int n = 4;
        int m = 24;

        int A = 0;
        int B = 1;
        int C = 2;
        int D = 3;
        int E = 4;
        int F = 5;
        int G = 6;
        int H = 7;
        int I = 8;
        int J = 9;
        int K = 10;
        int L = 11;
        int M = 12;
        int N = 13;
        int O = 14;
        int P = 15;
        int Q = 16;
        int R = 17;
        int S = 18;
        int T = 19;
        int U = 20;
        int V = 21;
        int W = 22;
        int Y = 23;

        String[] letters_str = { "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L",
                                 "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "Y" };

        int num_words = 13;
        int[,] words = { { B, U, O, Y }, { C, A, V, E }, { C, E, L, T }, { F, L, U, B }, { F, O, R, K },
                         { H, E, M, P }, { J, U, D, Y }, { J, U, N, K }, { L, I, M, N }, { Q, U, I, P },
                         { S, W, A, G }, { V, I, S, A }, { W, I, S, H } };

        //
        // Decision variables
        //
        IntVar[] dice = solver.MakeIntVarArray(m, 0, n - 1, "dice");
        IntVar[] gcc = solver.MakeIntVarArray(n, 6, 6, "gcc");

        //
        // Constraints
        //

        // the letters in a word must be on a different die
        for (int i = 0; i < num_words; i++)
        {
            solver.Add((from j in Enumerable.Range(0, n) select dice[words[i, j]]).ToArray().AllDifferent());
        }

        // there must be exactly 6 letters of each die
        /*
        for(int i = 0; i < n; i++) {
          solver.Add( ( from j in Enumerable.Range(0, m)
                        select (dice[j] == i)
                       ).ToArray().Sum() == 6 );
        }
        */
        // Use Distribute (Global Cardinality Count) instead.
        solver.Add(dice.Distribute(gcc));

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(dice, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MIN_VALUE);

        solver.NewSearch(db);

        while (solver.NextSolution())
        {
            for (int d = 0; d < n; d++)
            {
                Console.Write("die {0}: ", d);
                for (int i = 0; i < m; i++)
                {
                    if (dice[i].Value() == d)
                    {
                        Console.Write(letters_str[i]);
                    }
                }
                Console.WriteLine();
            }

            Console.WriteLine("The words with the cube label:");
            for (int i = 0; i < num_words; i++)
            {
                for (int j = 0; j < n; j++)
                {
                    Console.Write("{0} ({1})", letters_str[words[i, j]], dice[words[i, j]].Value());
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
