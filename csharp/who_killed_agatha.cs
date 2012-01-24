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

public class WhoKilledAgatha
{

  /**
   *
   * Implements the Who killed Agatha problem.
   * See http://www.hakank.org/google_or_tools/who_killed_agatha.py
   *
   */
  private static void Solve()
  {
    Solver solver = new Solver("WhoKilledAgatha");

    int n = 3;
    int agatha = 0;
    int butler = 1;
    int charles = 2;

    //
    // Decision variables
    //
    IntVar the_killer = solver.MakeIntVar(0, 2, "the_killer");
    IntVar the_victim = solver.MakeIntVar(0, 2, "the_victim");

    IntVar[] all = new IntVar[2 * n * n]; // for branching

    IntVar[,] hates = new IntVar[n,n];
    IntVar[] hates_flat = new IntVar[n * n];
    for(int i = 0; i < n; i++) {
      for(int j = 0; j < n; j++) {
        hates[i,j] = solver.MakeIntVar(0, 1, "hates[" + i + "," + j + "]");
        hates_flat[i * n + j] = hates[i,j];
        all[i * n + j] = hates[i,j];
      }
    }

    IntVar[,] richer = new IntVar[n,n];
    IntVar[] richer_flat = new IntVar[n * n];
    for(int i = 0; i < n; i++) {
      for(int j = 0; j < n; j++) {
        richer[i,j] = solver.MakeIntVar(0, 1, "richer[" + i + "," + j + "]");
        richer_flat[i * n + j] = richer[i,j];
        all[(n * n) + (i * n + j)] = richer[i,j];
      }
    }



    //
    // Constraints
    //

    // Agatha, the butler, and Charles live in Dreadsbury Mansion, and
    // are the only ones to live there.

    // A killer always hates, and is no richer than his victim.
    //     hates[the_killer, the_victim] == 1
    //     hates_flat[the_killer * n + the_victim] == 1
    solver.Add(hates_flat.Element(the_killer * n + the_victim) == 1);

    //    richer[the_killer, the_victim] == 0
    solver.Add(richer_flat.Element(the_killer * n + the_victim) == 0);

    // define the concept of richer:
    //     no one is richer than him-/herself...
    for(int i = 0; i < n; i++) {
      solver.Add(richer[i,i] == 0);
    }

    // (contd...) if i is richer than j then j is not richer than i
    //   if (i != j) =>
    //       ((richer[i,j] = 1) <=> (richer[j,i] = 0))
    for(int i = 0; i < n; i++) {
      for(int j = 0; j < n; j++) {
        if (i != j) {
          solver.Add(richer[i, j].IsEqual(1) - richer[j, i].IsEqual(0) == 0);
        }
      }
    }

    // Charles hates no one that Agatha hates.
    //    forall i in 0..2:
    //       (hates[agatha, i] = 1) => (hates[charles, i] = 0)
    for(int i = 0; i < n; i++) {
      IntVar b1a = hates[agatha,i].IsEqual(1);
      IntVar b1b = hates[charles,i].IsEqual(0);
      solver.Add(b1a-b1b <= 0);
    }

    // Agatha hates everybody except the butler.
    solver.Add(hates[agatha,charles] == 1);
    solver.Add(hates[agatha,agatha] == 1);
    solver.Add(hates[agatha,butler] == 0);

    // The butler hates everyone not richer than Aunt Agatha.
    //    forall i in 0..2:
    //       (richer[i, agatha] = 0) => (hates[butler, i] = 1)
    for(int i = 0; i < n; i++) {
      IntVar b2a = richer[i,agatha].IsEqual(0);
      IntVar b2b = hates[butler,i].IsEqual(1);
      solver.Add(b2a-b2b<=0);
    }

    // The butler hates everyone whom Agatha hates.
    //     forall i : 0..2:
    //         (hates[agatha, i] = 1) => (hates[butler, i] = 1)
    for(int i = 0; i < n; i++) {
      IntVar b3a = hates[agatha,i].IsEqual(1);
      IntVar b3b = hates[butler,i].IsEqual(1);
      solver.Add(b3a-b3b<=0);
    }

    // Noone hates everyone.
    //     forall i in 0..2:
    //         (sum j in 0..2: hates[i,j]) <= 2
    for(int i = 0; i < n; i++) {
      IntVar[] tmp = new IntVar[n];
      for(int j = 0; j < n; j++) {
        tmp[j] = hates[i,j];
      }
      solver.Add(tmp.Sum() <= 2);
    }


    // Who killed Agatha?
    solver.Add(the_victim == agatha);


    //
    // Search
    //
    DecisionBuilder db = solver.MakePhase(all,
                                          Solver.CHOOSE_FIRST_UNBOUND,
                                          Solver.ASSIGN_MIN_VALUE);

    solver.NewSearch(db);

    while (solver.NextSolution()) {
      Console.WriteLine("the_killer: " + the_killer.Value());
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
