// Copyright 2010-2017 Google
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
using System.IO;
using System.Text.RegularExpressions;
using Google.OrTools.ConstraintSolver;

public class OrTools
{
  private static long Solve(long num_buses_check = 0)
  {
    SolverParameters sPrm = new SolverParameters();
    sPrm.compress_trail = 0;
    sPrm.trace_level = 0;
    sPrm.profile_level = 0;
    Solver solver = new Solver("OrTools",sPrm);

    //this works
    // IntVar[,] x = solver.MakeIntVarMatrix(2,2, new int[] {-2,0,1,2}, "x");

    //this doesn't work
    IntVar[,] x = solver.MakeIntVarMatrix(2, 2, new int[] { 0, 1, 2 }, "x");

    for (int w = 0; w < 2; w++)
    {
      IntVar[] b = new IntVar[2];
      for (int i = 0; i < 2; i++)
      {
        b[i] = solver.MakeIsEqualCstVar(x[w, i], 0);
      }
      solver.Add(solver.MakeSumGreaterOrEqual(b, 2));
    }

    IntVar[] x_flat = x.Flatten();
    DecisionBuilder db = solver.MakePhase(x_flat,
                                          Solver.CHOOSE_FIRST_UNBOUND,
                                          Solver.ASSIGN_MIN_VALUE);
    solver.NewSearch(db);
    while (solver.NextSolution())
    {
      Console.WriteLine("x: ");
      for (int j = 0; j < 2; j++)
      {
        Console.Write("worker" + (j + 1).ToString() + ":");
        for (int i = 0; i < 2; i++)
        {
          Console.Write(" {0,2} ", x[j, i].Value());
        }
        Console.Write("\n");
      }
      Console.WriteLine("End   at---->" + DateTime.Now);
    }

    Console.WriteLine("\nSolutions: {0}", solver.Solutions());
    Console.WriteLine("WallTime: {0}ms", solver.WallTime());
    Console.WriteLine("Failures: {0}", solver.Failures());
    Console.WriteLine("Branches: {0} ", solver.Branches());

    solver.EndSearch();
    return 1;
  }

  public static void Main(String[] args)
  {
    Console.WriteLine("Check for minimum number of buses: ");
    long num_buses = Solve();
    Console.WriteLine("\n... got {0} as minimal value.", num_buses);
    Console.WriteLine("\nAll solutions: ", num_buses);
  }
}
