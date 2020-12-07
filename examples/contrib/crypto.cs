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

public class Crypto
{
    /**
     *
     * Crypto problem.
     *
     * This is the standard benchmark "crypto" problem.
     *
     * From GLPK:s model cryto.mod.
     *
     * """
     * This problem comes from the newsgroup rec.puzzle.
     * The numbers from 1 to 26 are assigned to the letters of the alphabet.
     * The numbers beside each word are the total of the values assigned to
     * the letters in the word (e.g. for LYRE: L, Y, R, E might be to equal
     * 5, 9, 20 and 13, or any other combination that add up to 47).
     * Find the value of each letter under the equations:
     *
     * BALLET  45     GLEE  66     POLKA      59     SONG     61
     * CELLO   43     JAZZ  58     QUARTET    50     SOPRANO  82
     * CONCERT 74     LYRE  47     SAXOPHONE 134     THEME    72
     * FLUTE   30     OBOE  53     SCALE      51     VIOLIN  100
     * FUGUE   50     OPERA 65     SOLO       37     WALTZ    34
     *
     * Solution:
     * A, B,C, D, E,F, G, H, I, J, K,L,M, N, O, P,Q, R, S,T,U, V,W, X, Y, Z
     * 5,13,9,16,20,4,24,21,25,17,23,2,8,12,10,19,7,11,15,3,1,26,6,22,14,18
     *
     * Reference:
     * Koalog Constraint Solver <http://www.koalog.com/php/jcs.php>,
     * Simple problems, the crypto-arithmetic puzzle ALPHACIPHER.
     * """
     *
     * Also see http://hakank.org/or-tools/crypto.py
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("Crypto");

        int num_letters = 26;

        int BALLET = 45;
        int CELLO = 43;
        int CONCERT = 74;
        int FLUTE = 30;
        int FUGUE = 50;
        int GLEE = 66;
        int JAZZ = 58;
        int LYRE = 47;
        int OBOE = 53;
        int OPERA = 65;
        int POLKA = 59;
        int QUARTET = 50;
        int SAXOPHONE = 134;
        int SCALE = 51;
        int SOLO = 37;
        int SONG = 61;
        int SOPRANO = 82;
        int THEME = 72;
        int VIOLIN = 100;
        int WALTZ = 34;

        //
        // Decision variables
        //
        IntVar[] LD = solver.MakeIntVarArray(num_letters, 1, num_letters, "LD");

        // Note D is not used in the constraints below
        IntVar A = LD[0];
        IntVar B = LD[1];
        IntVar C = LD[2]; // IntVar D =  LD[3];
        IntVar E = LD[4];
        IntVar F = LD[5];
        IntVar G = LD[6];
        IntVar H = LD[7];
        IntVar I = LD[8];
        IntVar J = LD[9];
        IntVar K = LD[10];
        IntVar L = LD[11];
        IntVar M = LD[12];
        IntVar N = LD[13];
        IntVar O = LD[14];
        IntVar P = LD[15];
        IntVar Q = LD[16];
        IntVar R = LD[17];
        IntVar S = LD[18];
        IntVar T = LD[19];
        IntVar U = LD[20];
        IntVar V = LD[21];
        IntVar W = LD[22];
        IntVar X = LD[23];
        IntVar Y = LD[24];
        IntVar Z = LD[25];

        //
        // Constraints
        //
        solver.Add(LD.AllDifferent());
        solver.Add(B + A + L + L + E + T == BALLET);
        solver.Add(C + E + L + L + O == CELLO);
        solver.Add(C + O + N + C + E + R + T == CONCERT);
        solver.Add(F + L + U + T + E == FLUTE);
        solver.Add(F + U + G + U + E == FUGUE);
        solver.Add(G + L + E + E == GLEE);
        solver.Add(J + A + Z + Z == JAZZ);
        solver.Add(L + Y + R + E == LYRE);
        solver.Add(O + B + O + E == OBOE);
        solver.Add(O + P + E + R + A == OPERA);
        solver.Add(P + O + L + K + A == POLKA);
        solver.Add(Q + U + A + R + T + E + T == QUARTET);
        solver.Add(S + A + X + O + P + H + O + N + E == SAXOPHONE);
        solver.Add(S + C + A + L + E == SCALE);
        solver.Add(S + O + L + O == SOLO);
        solver.Add(S + O + N + G == SONG);
        solver.Add(S + O + P + R + A + N + O == SOPRANO);
        solver.Add(T + H + E + M + E == THEME);
        solver.Add(V + I + O + L + I + N == VIOLIN);
        solver.Add(W + A + L + T + Z == WALTZ);

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(LD, Solver.CHOOSE_MIN_SIZE_LOWEST_MIN, Solver.ASSIGN_CENTER_VALUE);

        solver.NewSearch(db);

        String str = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        while (solver.NextSolution())
        {
            for (int i = 0; i < num_letters; i++)
            {
                Console.WriteLine("{0}: {1,2}", str[i], LD[i].Value());
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
