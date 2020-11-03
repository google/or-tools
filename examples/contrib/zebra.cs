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
using System.Linq;
using Google.OrTools.ConstraintSolver;

public class NQueens
{
    /**
     *
     * Solves the Zebra problem.
     *
     * This is a port of the or-tools/Python model zebra.py
     *
     *
     * """
     * This is the zebra problem as invented by Lewis Caroll.
     *
     * There are five houses.
     * The Englishman lives in the red house.
     * The Spaniard owns the dog.
     * Coffee is drunk in the green house.
     * The Ukrainian drinks tea.
     * The green house is immediately to the right of the ivory house.
     * The Old Gold smoker owns snails.
     * Kools are smoked in the yellow house.
     * Milk is drunk in the middle house.
     * The Norwegian lives in the first house.
     * The man who smokes Chesterfields lives in the house next to the man
     *   with the fox.
     * Kools are smoked in the house next to the house where the horse is kept.
     * The Lucky Strike smoker drinks orange juice.
     * The Japanese smokes Parliaments.
     * The Norwegian lives next to the blue house.
     *
     * Who owns a zebra and who drinks water?
     * """
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("Zebra");

        int n = 5;

        //
        // Decision variables
        //

        // Colors
        IntVar red = solver.MakeIntVar(1, n, "red");
        IntVar green = solver.MakeIntVar(1, n, "green");
        IntVar yellow = solver.MakeIntVar(1, n, "yellow");
        IntVar blue = solver.MakeIntVar(1, n, "blue");
        IntVar ivory = solver.MakeIntVar(1, n, "ivory");

        // Nationality
        IntVar englishman = solver.MakeIntVar(1, n, "englishman");
        IntVar spaniard = solver.MakeIntVar(1, n, "spaniard");
        IntVar japanese = solver.MakeIntVar(1, n, "japanese");
        IntVar ukrainian = solver.MakeIntVar(1, n, "ukrainian");
        IntVar norwegian = solver.MakeIntVar(1, n, "norwegian");

        // Animal
        IntVar dog = solver.MakeIntVar(1, n, "dog");
        IntVar snails = solver.MakeIntVar(1, n, "snails");
        IntVar fox = solver.MakeIntVar(1, n, "fox");
        IntVar zebra = solver.MakeIntVar(1, n, "zebra");
        IntVar horse = solver.MakeIntVar(1, n, "horse");

        // Drink
        IntVar tea = solver.MakeIntVar(1, n, "tea");
        IntVar coffee = solver.MakeIntVar(1, n, "coffee");
        IntVar water = solver.MakeIntVar(1, n, "water");
        IntVar milk = solver.MakeIntVar(1, n, "milk");
        IntVar fruit_juice = solver.MakeIntVar(1, n, "fruit juice");

        // Smoke
        IntVar old_gold = solver.MakeIntVar(1, n, "old gold");
        IntVar kools = solver.MakeIntVar(1, n, "kools");
        IntVar chesterfields = solver.MakeIntVar(1, n, "chesterfields");
        IntVar lucky_strike = solver.MakeIntVar(1, n, "lucky strike");
        IntVar parliaments = solver.MakeIntVar(1, n, "parliaments");

        // for search
        IntVar[] all_vars = { parliaments, kools,     chesterfields, lucky_strike, old_gold, englishman,  spaniard,
                              japanese,    ukrainian, norwegian,     dog,          snails,   fox,         zebra,
                              horse,       tea,       coffee,        water,        milk,     fruit_juice, red,
                              green,       yellow,    blue,          ivory };

        //
        // Constraints
        //

        // Alldifferents
        solver.Add(new IntVar[] { red, green, yellow, blue, ivory }.AllDifferent());
        solver.Add(new IntVar[] { englishman, spaniard, japanese, ukrainian, norwegian }.AllDifferent());
        solver.Add(new IntVar[] { dog, snails, fox, zebra, horse }.AllDifferent());
        solver.Add(new IntVar[] { tea, coffee, water, milk, fruit_juice }.AllDifferent());
        solver.Add(new IntVar[] { parliaments, kools, chesterfields, lucky_strike, old_gold }.AllDifferent());

        //
        // The clues
        //
        solver.Add(englishman == red);
        solver.Add(spaniard == dog);
        solver.Add(coffee == green);
        solver.Add(ukrainian == tea);
        solver.Add(green == ivory + 1);
        solver.Add(old_gold == snails);
        solver.Add(kools == yellow);
        solver.Add(milk == 3);
        solver.Add(norwegian == 1);
        solver.Add((fox - chesterfields).Abs() == 1);
        solver.Add((horse - kools).Abs() == 1);
        solver.Add(lucky_strike == fruit_juice);
        solver.Add(japanese == parliaments);
        solver.Add((norwegian - blue).Abs() == 1);

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(all_vars, Solver.INT_VAR_DEFAULT, Solver.INT_VALUE_DEFAULT);

        solver.NewSearch(db);

        IntVar[] p = { englishman, spaniard, japanese, ukrainian, norwegian };
        int[] ix = { 0, 1, 2, 3, 4 };
        while (solver.NextSolution())
        {
      int water_drinker = (from i in ix where p [i]
                               .Value() == water.Value() select i)
                              .First();
      int zebra_owner = (from i in ix where p [i]
                             .Value() == zebra.Value() select i)
                            .First();
      Console.WriteLine("The {0} drinks water.", p[water_drinker].ToString());
      Console.WriteLine("The {0} owns the zebra", p[zebra_owner].ToString());
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
