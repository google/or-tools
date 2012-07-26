// Copyright 2010-2011 Google
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
 * Shows how to write a custom lns operator.
 */

public class MoveOneVar : BaseLNS
{
  public MoveOneVar(IntVar[] vars) : base(vars) {}

  public override void InitFragments()
  {
    index_ = 0;
  }

  public override bool NextFragment(IntVector fragment)
  {
    int size = Size();
    if (index_ < size)
    {
      fragment.Add(index_);
      ++index_;
      return true;
    }
    else
    {
      return false;
    }
  }

  private int index_;
}


public class CsLsApi
{
  /**
   * Solves the rabbits + pheasants problem.  We are seing 20 heads
   * and 56 legs. How many rabbits and how many pheasants are we thus
   * seeing?
   */
  private static void BasicLns()
  {
    Solver solver = new Solver("BasicLns");
    IntVar[] vars = solver.MakeIntVarArray(4, 0, 4, "vars");
    IntVar sum_var = vars.Sum().Var();
    OptimizeVar obj = sum_var.Minimize(1);
    DecisionBuilder db = solver.MakePhase(vars,
                                          Solver.CHOOSE_FIRST_UNBOUND,
                                          Solver.ASSIGN_MAX_VALUE);
    MoveOneVar one_var_lns = new MoveOneVar(vars);
    LocalSearchPhaseParameters ls_params =
        solver.MakeLocalSearchPhaseParameters(one_var_lns, db);
    DecisionBuilder ls = solver.MakeLocalSearchPhase(vars, db, ls_params);
    SolutionCollector collector = solver.MakeLastSolutionCollector();
    collector.AddObjective(sum_var);
    SearchMonitor log = solver.MakeSearchLog(1000, obj);
    solver.Solve(ls, collector, obj, log);
    Console.WriteLine("Objective value = {0}", collector.ObjectiveValue(0));
}

  public static void Main(String[] args)
  {
    BasicLns();
  }
}
