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
using Google.OrTools.ConstraintSolver;

/**
 * Shows how to write a custom decision builder.
 */
public class AssignFirstUnboundToMin : NetDecisionBuilder
{
  public AssignFirstUnboundToMin(IntVar[] vars)
  {
    vars_ = vars;
  }

  public override Decision Next(Solver solver)
  {
    foreach (IntVar var in vars_)
    {
      if (!var.Bound())
      {
        return solver.MakeAssignVariableValue(var, var.Min());
      }
    }
    return null;
  }

  private IntVar[] vars_;
}


public class CsRabbitsPheasants
{
  /**
   * Solves the rabbits + pheasants problem.  We are seing 20 heads
   * and 56 legs. How many rabbits and how many pheasants are we thus
   * seeing?
   */
  private static void Solve()
  {
    Solver solver = new Solver("RabbitsPheasants");
    IntVar rabbits = solver.MakeIntVar(0, 100, "rabbits");
    IntVar pheasants = solver.MakeIntVar(0, 100, "pheasants");
    solver.Add(rabbits + pheasants == 20);
    solver.Add(rabbits * 4 + pheasants * 2 == 56);
    DecisionBuilder db =
        new AssignFirstUnboundToMin(new IntVar[] {rabbits, pheasants});
    solver.NewSearch(db);
    solver.NextSolution();
    Console.WriteLine(
        "Solved Rabbits + Pheasants in {0} ms, and {1} search tree branches.",
        solver.WallTime(),  solver.Branches());
    Console.WriteLine(rabbits.ToString());
    Console.WriteLine(pheasants.ToString());
    solver.EndSearch();
  }

  public static void Main(String[] args)
  {
    Solve();
  }
}
