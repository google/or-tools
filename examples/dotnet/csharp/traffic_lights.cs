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

public class TrafficLights
{

  /**
   *
   * Traffic lights problem.
   *
   * CSPLib problem 16
   * http://www.cs.st-andrews.ac.uk/~ianm/CSPLib/prob/prob016/index.html
   * """
   * Specification:
   * Consider a four way traffic junction with eight traffic lights. Four of the traffic
   * lights are for the vehicles and can be represented by the variables V1 to V4 with domains
   * {r,ry,g,y} (for red, red-yellow, green and yellow). The other four traffic lights are
   * for the pedestrians and can be represented by the variables P1 to P4 with domains {r,g}.
   *
   * The constraints on these variables can be modelled by quaternary constraints on
   * (Vi, Pi, Vj, Pj ) for 1<=i<=4, j=(1+i)mod 4 which allow just the tuples
   * {(r,r,g,g), (ry,r,y,r), (g,g,r,r), (y,r,ry,r)}.
   *
   * It would be interesting to consider other types of junction (e.g. five roads
   * intersecting) as well as modelling the evolution over time of the traffic light sequence.
   * ...
   *
   * Results
   * Only 2^2 out of the 2^12 possible assignments are solutions.
   *
   * (V1,P1,V2,P2,V3,P3,V4,P4) =
   * {(r,r,g,g,r,r,g,g), (ry,r,y,r,ry,r,y,r), (g,g,r,r,g,g,r,r), (y,r,ry,r,y,r,ry,r)}
   * [(1,1,3,3,1,1,3,3), ( 2,1,4,1, 2,1,4,1), (3,3,1,1,3,3,1,1), (4,1, 2,1,4,1, 2,1)}
   * The problem has relative few constraints, but each is very
   * tight. Local propagation appears to be rather ineffective on this
   * problem.
   *
   * """
   * Note: In this model we use only the constraint
   *  solver.AllowedAssignments().
   *
   *
   * See http://www.hakank.org/or-tools/traffic_lights.py
   *
   */
  private static void Solve()
  {

    Solver solver = new Solver("TrafficLights");

    //
    // data
    //
    int n = 4;

    int r = 0;
    int ry = 1;
    int g = 2;
    int y = 3;

    string[] lights = {"r", "ry", "g", "y"};

    // The allowed combinations
    IntTupleSet allowed = new IntTupleSet(4);
    allowed.InsertAll(new int[,] {{r,r,g,g},
                                  {ry,r,y,r},
                                  {g,g,r,r},
                                  {y,r,ry,r}});
    //
    // Decision variables
    //
    IntVar[] V = solver.MakeIntVarArray(n, 0, n-1, "V");
    IntVar[] P = solver.MakeIntVarArray(n, 0, n-1, "P");

    // for search
    IntVar[] VP = new IntVar[2 * n];
    for(int i = 0; i < n; i++) {
      VP[i] = V[i];
      VP[i+n] = P[i];
    }

    //
    // Constraints
    //
    for(int i = 0; i < n; i++) {
      int j = (1+i) % n;
      IntVar[] tmp = new IntVar[] {V[i],P[i],V[j],P[j]};
      solver.Add(tmp.AllowedAssignments(allowed));
    }

    //
    // Search
    //
    DecisionBuilder db = solver.MakePhase(VP,
                                          Solver.CHOOSE_FIRST_UNBOUND,
                                          Solver.ASSIGN_MIN_VALUE);


    solver.NewSearch(db);

    while (solver.NextSolution()) {
      for(int i = 0; i < n; i++) {
        Console.Write("{0,2} {1,2} ",
                      lights[V[i].Value()],
                      lights[P[i].Value()]);
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
