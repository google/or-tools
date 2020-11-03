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
using System.IO;
using System.Text.RegularExpressions;
using Google.OrTools.ConstraintSolver;

public class WordSquare
{
    /**
     *
     * Word square.
     *
     * From http://en.wikipedia.org/wiki/Word_square
     * """
     * A word square is a special case of acrostic. It consists of a set of words,
     * all having the same number of letters as the total number of words (the
     * 'order' of the square); when the words are written out in a square grid
     * horizontally, the same set of words can be read vertically.
     * """
     *
     * See http://www.hakank.org/or-tools/word_square.py
     *
     */
    private static void Solve(String[] words, int word_len, int num_answers)
    {
        Solver solver = new Solver("WordSquare");

        int num_words = words.Length;
        Console.WriteLine("num_words: " + num_words);
        int n = word_len;
        IEnumerable<int> WORDLEN = Enumerable.Range(0, word_len);

        //
        //  convert a character to integer
        //

        String alpha = "abcdefghijklmnopqrstuvwxyz";
        Hashtable d = new Hashtable();
        Hashtable rev = new Hashtable();
        int count = 1;
        for (int a = 0; a < alpha.Length; a++)
        {
            d[alpha[a]] = count;
            rev[count] = a;
            count++;
        }

        int num_letters = alpha.Length;

        //
        // Decision variables
        //
        IntVar[,] A = solver.MakeIntVarMatrix(num_words, word_len, 0, num_letters, "A");
        IntVar[] A_flat = A.Flatten();
        IntVar[] E = solver.MakeIntVarArray(n, 0, num_words, "E");

        //
        // Constraints
        //
        solver.Add(E.AllDifferent());

        // copy the words to a matrix
        for (int i = 0; i < num_words; i++)
        {
            char[] s = words[i].ToArray();
            foreach (int j in WORDLEN)
            {
                int t = (int)d[s[j]];
                solver.Add(A[i, j] == t);
            }
        }

        foreach (int i in WORDLEN)
        {
            foreach (int j in WORDLEN)
            {
                solver.Add(A_flat.Element(E[i] * word_len + j) == A_flat.Element(E[j] * word_len + i));
            }
        }

        //
        // Search
        //
        DecisionBuilder db =
            solver.MakePhase(E.Concat(A_flat).ToArray(), Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MIN_VALUE);

        solver.NewSearch(db);

        int num_sols = 0;
        while (solver.NextSolution())
        {
            num_sols++;
            for (int i = 0; i < n; i++)
            {
                Console.WriteLine(words[E[i].Value()] + " ");
            }
            Console.WriteLine();

            if (num_answers > 0 && num_sols >= num_answers)
            {
                break;
            }
        }

        Console.WriteLine("\nSolutions: {0}", solver.Solutions());
        Console.WriteLine("WallTime: {0}ms", solver.WallTime());
        Console.WriteLine("Failures: {0}", solver.Failures());
        Console.WriteLine("Branches: {0} ", solver.Branches());

        solver.EndSearch();
    }

    /*
     *
     * Read the words from a word list with a specific word length.
     *
     */
    public static String[] ReadWords(String word_list, int word_len)
    {
        Console.WriteLine("ReadWords {0} {1}", word_list, word_len);
        List<String> all_words = new List<String>();

        TextReader inr = new StreamReader(word_list);
        String str;
        int count = 0;
        Hashtable d = new Hashtable();
        while ((str = inr.ReadLine()) != null)
        {
            str = str.Trim().ToLower();
            // skip weird words
            if (Regex.Match(str, @"[^a-z]").Success || d.Contains(str) || str.Length == 0 || str.Length != word_len)
            {
                continue;
            }

            d[str] = 1;
            all_words.Add(str);
            count++;

        } // end while

        inr.Close();

        return all_words.ToArray();
    }

    public static void Main(String[] args)
    {
        String word_list = "/usr/share/dict/words";
        int word_len = 4;
        int num_answers = 20;

        if (args.Length > 0)
        {
            word_list = args[0];
        }

        if (args.Length > 1)
        {
            word_len = Convert.ToInt32(args[1]);
        }

        if (args.Length > 2)
        {
            num_answers = Convert.ToInt32(args[2]);
        }

        String[] words = ReadWords(word_list, word_len);

        Solve(words, word_len, num_answers);
    }
}
