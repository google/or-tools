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
using System.Linq;
using System.Text.RegularExpressions;
using Google.OrTools.ConstraintSolver;

public class ARoundOfGolf
{
    /**
     *
     * A Round of Golf puzzle (Dell Logic Puzzles) in Google CP Solver.
     *
     * From http://brownbuffalo.sourceforge.net/RoundOfGolfClues.html
     * """
     * Title: A Round of Golf
     * Author: Ellen K. Rodehorst
     * Publication: Dell Favorite Logic Problems
     * Issue: Summer, 2000
     * Puzzle #: 9
     * Stars: 1
     *
     * When the Sunny Hills Country Club golf course isn't in use by club members,
     * of course, it's open to the club's employees. Recently, Jack and three
     * other workers at the golf course got together on their day off to play a
     * round of eighteen holes of golf. Afterward, all four, including Mr. Green,
     * went to the clubhouse to total their scorecards. Each man works at a
     * different job (one is a short-order cook), and each shot a different score
     * in the game. No one scored below 70 or above 85 strokes. From the clues
     * below, can you discover each man's full name, job and golf score?
     *
     * 1. Bill, who is not the maintenance man, plays golf often and had the
     * lowest score of the foursome.
     * 2. Mr. Clubb, who isn't Paul, hit several balls into the woods and scored
     * ten strokes more than the pro-shop clerk.
     * 3. In some order, Frank and the caddy scored four and seven more strokes
     * than Mr. Sands.
     * 4. Mr. Carter thought his score of 78 was one of his better games, even
     *    though Frank's score  was lower.
     * 5. None of the four scored exactly 81 strokes.
     *
     * Determine: First Name - Last Name - Job - Score
     * """
     *
     * See http://www.hakank.org/google_or_tools/a_round_of_golf.py
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("ARoundOfGolf");

        // number of speakers
        int n = 4;

        int Jack = 0;
        int Bill = 1;
        int Paul = 2;
        int Frank = 3;

        //
        // Decision variables
        //
        IntVar[] last_name = solver.MakeIntVarArray(n, 0, n - 1, "last_name");
        // IntVar Green  = last_name[0]; // not used
        IntVar Clubb = last_name[1];
        IntVar Sands = last_name[2];
        IntVar Carter = last_name[3];

        IntVar[] job = solver.MakeIntVarArray(n, 0, n - 1, "job");
        // IntVar cook            = job[0]; // not used
        IntVar maintenance_man = job[1];
        IntVar clerk = job[2];
        IntVar caddy = job[3];

        IntVar[] score = solver.MakeIntVarArray(n, 70, 85, "score");

        // for search
        IntVar[] all = new IntVar[n * 3];
        for (int i = 0; i < n; i++)
        {
            all[i] = last_name[i];
            all[i + n] = job[i];
            all[i + 2 * n] = score[i];
        }

        //
        // Constraints
        //
        solver.Add(last_name.AllDifferent());
        solver.Add(job.AllDifferent());
        solver.Add(score.AllDifferent());

        // 1. Bill, who is not the maintenance man, plays golf often and had
        //    the lowest score of the foursome.
        solver.Add(maintenance_man != Bill);
        solver.Add(score[Bill] < score[Jack]);
        solver.Add(score[Bill] < score[Paul]);
        solver.Add(score[Bill] < score[Frank]);

        // 2. Mr. Clubb, who isn't Paul, hit several balls into the woods and
        //    scored ten strokes more than the pro-shop clerk.
        solver.Add(Clubb != Paul);
        solver.Add(score.Element(Clubb) == score.Element(clerk) + 10);

        // 3. In some order, Frank and the caddy scored four and seven more
        //    strokes than Mr. Sands.
        solver.Add(caddy != Frank);
        solver.Add(Sands != Frank);
        solver.Add(caddy != Sands);

        IntVar b3_a_1 = score.Element(Sands) + 4 == score[Frank];
        IntVar b3_a_2 = score.Element(caddy) == score.Element(Sands) + 7;
        IntVar b3_b_1 = score.Element(Sands) + 7 == score[Frank];
        IntVar b3_b_2 = score.Element(caddy) == score.Element(Sands) + 4;
        solver.Add((b3_a_1 * b3_a_2) + (b3_b_1 * b3_b_2) == 1);

        // 4. Mr. Carter thought his score of 78 was one of his better games,
        //    even though Frank's score was lower.
        solver.Add(Carter != Frank);
        solver.Add(score.Element(Carter) == 78);
        solver.Add(score[Frank] < score.Element(Carter));

        // 5. None of the four scored exactly 81 strokes.
        for (int i = 0; i < n; i++)
        {
            solver.Add(score[i] != 81);
        }

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(all, Solver.INT_VAR_DEFAULT, Solver.INT_VALUE_DEFAULT);

        solver.NewSearch(db);

        while (solver.NextSolution())
        {
            Console.WriteLine("Last name: " +
                              String.Join(",  ", (from i in last_name select i.Value().ToString()).ToArray()));
            Console.WriteLine("Job      : " +
                              String.Join(",  ", (from i in job select i.Value().ToString()).ToArray()));
            Console.WriteLine("Score    : " +
                              String.Join(", ", (from i in score select i.Value().ToString()).ToArray()));
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
