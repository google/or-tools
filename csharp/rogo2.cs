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

public class Rogo2
{

  /**
   *
   * Build the table of valid connections of the grid.
   *
   */
  public static int[,] ValidConnections(int rows, int cols)
  {

    IEnumerable<int> ROWS = Enumerable.Range(0, rows);
    IEnumerable<int> COLS = Enumerable.Range(0, cols);
    var result_tmp = (
                      from i1 in ROWS 
                      from j1 in COLS 
                      from i2 in ROWS
                      from j2 in COLS 
                      where 
                      (Math.Abs(j1-j2) == 1 && i1 == i2) ||
                      (Math.Abs(i1-i2) == 1 && j1 % cols == j2 % cols)
                      select new int[] {i1*cols+j1, i2*cols+j2}
                      ).ToArray();

    // Convert to len x 2 matrix
    int len = result_tmp.Length;
    int[,] result = new int[len, 2];
    int i = 0;
    foreach(int[] r in result_tmp) {
      for(int j = 0; j < 2; j++) {
        result[i, j] = r[j];
      }
      i++;
    }

    return result;

  }



  /**
   *
   * Rogo puzzle solver.
   *
   * From http://www.rogopuzzle.co.nz/
   * """
   * The object is to collect the biggest score possible using a given
   * number of steps in a loop around a grid. The best possible score
   * for a puzzle is given with it, so you can easily check that you have
   * solved the puzzle. Rogo puzzles can also include forbidden squares,
   * which must be avoided in your loop.
   * """
   *
   * Also see Mike Trick:
   * "Operations Research, Sudoko, Rogo, and Puzzles"
   * http://mat.tepper.cmu.edu/blog/?p=1302
   *
   *
   * Also see, http://www.hakank.org/or-tools/rogo2.py
   *
   */
  private static void Solve(String problem_name, 
                            int[,] problem, 
                            int rows, 
                            int cols, 
                            int max_steps,
                            int best)
  {

    Solver solver = new Solver("Rogo2");


    Console.WriteLine("\n");
    Console.WriteLine("**********************************************");
    Console.WriteLine("    {0}", problem_name);
    Console.WriteLine("**********************************************\n");

    //
    // Data
    //
    int B = -1;

    Console.WriteLine("Rows: {0} Cols: {1} Max Steps: {2}", rows, cols, max_steps);

    int[] problem_flatten = problem.Cast<int>().ToArray();
    int max_point = problem_flatten.Max();
    int max_sum = problem_flatten.Sum();
    Console.WriteLine("max_point: {0} max_sum: {1} best: {2}", max_point, max_sum, best);

    IEnumerable<int> STEPS = Enumerable.Range(0, max_steps);
    IEnumerable<int> STEPS1 = Enumerable.Range(0, max_steps-1);


    //
    // Decision variables
    //
    IntVar[] x = solver.MakeIntVarArray(max_steps, 0, rows-1, "x");
    IntVar[] y = solver.MakeIntVarArray(max_steps, 0, cols-1, "y");

    // Note: we channel (x,y) <=> path below
    IntVar[] path = solver.MakeIntVarArray(max_steps, 0, rows*cols-1, "path");

    IntVar[] points = solver.MakeIntVarArray(max_steps, 0, best, "points");
    IntVar sum_points = points.Sum().VarWithName("sum_points");
    
    int[,] valid_connections = ValidConnections(rows, cols);


    //
    // Constraints
    //  

    // calculate the points (to maximize)
    foreach(int s in STEPS) {
      solver.Add(points[s] == problem_flatten.Element(path[s]));
    }

    // channel path <-> (x,y)
    solver.Add(path.AllDifferent());

    foreach(int s in STEPS) {
      solver.Add(path[s] == (x[s]*cols + y[s]));
    }


    // ensure that there are no black cells in
    // the path
    foreach(int s in STEPS) {
      solver.Add(problem_flatten.Element(path[s]) != B);
    }
    

    // valid connections
    foreach(int s in STEPS1) {
      solver.Add(new IntVar[] {path[s], path[s+1]}.AllowedAssignments(valid_connections));
    }
    // around the corner
    solver.Add(new IntVar[] {path[max_steps-1], path[0]}.AllowedAssignments(valid_connections));


    // Symmetry breaking
    for(int s = 1; s < max_steps; s++) {
      solver.Add(path[0] < path[s]);
    }


    //
    // Objective
    //
    OptimizeVar obj = sum_points.Maximize(1);


    //
    // Search
    //
    DecisionBuilder db = solver.MakePhase(path,
                                          Solver.INT_VAR_DEFAULT,
                                          Solver.INT_VALUE_DEFAULT);

    solver.NewSearch(db, obj);

    while (solver.NextSolution()) {
      Console.WriteLine("sum_points: {0}", sum_points.Value());
      Console.Write("path: ");
      foreach(int s in STEPS) {
        Console.Write("{0} ", path[s].Value());
      }
      Console.WriteLine();
      Console.WriteLine("(Adding 1 to coords...)");
      foreach(int s in STEPS) {
        Console.WriteLine("{0} {1}: {2} points", x[s].Value()+1, y[s].Value()+1, points[s].Value());
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

    int W = 0;
    int B = -1;

    
    // Default problem
    // Data from
    // Mike Trick: "Operations Research, Sudoko, Rogo, and Puzzles"
    // http://mat.tepper.cmu.edu/blog/?p=1302
    //
    // This has 48 solutions with symmetries;
    // 4 when the path symmetry is removed.
    //
    int rows1 = 5;
    int cols1 = 9;
    int max_steps1 = 12;
    int best1 = 8;
    int[,] problem1 = {
      {2,W,W,W,W,W,W,W,W},
      {W,3,W,W,1,W,W,2,W},
      {W,W,W,W,W,W,B,W,2},
      {W,W,2,B,W,W,W,W,W},
      {W,W,W,W,2,W,W,1,W}
    };
    String problem_name1 = "Problem Mike Trick";

    // 
    //  Rogo problem from 2011-01-06
    //  16 steps, good: 28, best: 31
    // 
    int rows2 = 9;
    int cols2 = 7;
    int max_steps2 = 16;
    int best2 = 31;
    int[,] problem2 = {
      {B,W,6,W,W,3,B}, //  1
      {2,W,3,W,W,6,W}, //  2
      {6,W,W,2,W,W,2}, //  3
      {W,3,W,W,B,B,B}, //  4
      {W,W,W,2,W,2,B}, //  5
      {W,W,W,3,W,W,W}, //  6
      {6,W,6,B,W,W,3}, //  7
      {3,W,W,W,W,W,6}, //  8
      {B,2,W,6,W,2,B}  //  9
    };
    String problem_name2 = "Problem 2011-01-06";


    //
    //  The Rogo problem from 2011-01-07
    //  best: 36
    //  
    int rows3 = 12;
    int cols3 = 7;
    int max_steps3 = 16;
    int best3 = 36;
    int [,] problem3 = {
      {4,7,W,W,W,W,3}, //  1
      {W,W,W,W,3,W,4}, //  2
      {W,W,4,W,7,W,W}, //  3
      {7,W,3,W,W,W,W}, //  4
      {B,B,B,W,3,W,W}, //  5
      {B,B,W,7,W,W,7}, //  6
      {B,B,W,W,W,4,B}, //  7
      {B,4,4,W,W,W,B}, //  8
      {B,W,W,W,W,3,B}, //  9
      {W,W,3,W,4,B,B}, // 10
      {3,W,W,W,W,B,B}, // 11
      {7,W,7,4,B,B,B}  // 12
    };
    String problem_name3 = "Problem 2011-01-07";


    // The Rogo problem Intro 8
    // 
    // http://www.rogopuzzle.co.nz/paper-rogos/intro-rogo-8/
    //
    int rows4 = 9;
    int cols4 = 9;
    int max_steps4 = 20;
    int best4 = 37;
    int[,] problem4 = {
      {5,4,W,W,3,W,1,W,3}, // 1
      {W,W,W,4,W,W,3,W,W}, // 2
      {W,1,W,W,W,W,5,4,4}, // 3
      {3,W,3,W,W,3,W,W,W}, // 4
      {W,W,W,W,W,W,W,W,W}, // 5
      {W,W,W,5,W,W,3,W,W}, // 6
      {4,W,3,3,W,4,1,W,4}, // 7
      {W,5,W,W,W,W,W,3,W}, // 8
      {5,W,W,W,W,3,W,W,W}, // 9
    };
    String problem_name4 = "Problem Intro 8";

    //Solve(problem_name1, problem1, rows1, cols1, max_steps1, best1);
    //Solve(problem_name2, problem2, rows2, cols2, max_steps2, best2);
    //Solve(problem_name3, problem3, rows3, cols3, max_steps3, best3);
    Solve(problem_name4, problem4, rows4, cols4, max_steps4, best4);

  }
}
