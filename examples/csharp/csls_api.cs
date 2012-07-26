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

public class OneVarLns : BaseLNS
{
  public OneVarLns(IntVar[] vars) : base(vars) {}

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

class MoveOneVar : IntVarLocalSearchOperator {
 public MoveOneVar(IntVar[] variables) : base(variables)
  {
    variable_index_ = 0;
    move_up_ = false;
  }

  protected override bool MakeOneNeighbor() {
    long current_value = OldValue(variable_index_);
    if (move_up_) {
      SetValue(variable_index_, current_value  + 1);
      variable_index_ = (variable_index_ + 1) % Size();
    } else {
      SetValue(variable_index_, current_value  - 1);
    }
    move_up_ = !move_up_;
    return true;
  }

  protected  override void OnStart() {}

  // Index of the next variable to try to restore
  private long variable_index_;
  // Direction of the modification.
  private bool move_up_;
};



public class CsLsApi
{
  private static void BasicLns()
  {
    Solver solver = new Solver("BasicLns");
    IntVar[] vars = solver.MakeIntVarArray(4, 0, 4, "vars");
    IntVar sum_var = vars.Sum().Var();
    OptimizeVar obj = sum_var.Minimize(1);
    DecisionBuilder db = solver.MakePhase(vars,
                                          Solver.CHOOSE_FIRST_UNBOUND,
                                          Solver.ASSIGN_MAX_VALUE);
    OneVarLns one_var_lns = new OneVarLns(vars);
    LocalSearchPhaseParameters ls_params =
        solver.MakeLocalSearchPhaseParameters(one_var_lns, db);
    DecisionBuilder ls = solver.MakeLocalSearchPhase(vars, db, ls_params);
    SolutionCollector collector = solver.MakeLastSolutionCollector();
    collector.AddObjective(sum_var);
    SearchMonitor log = solver.MakeSearchLog(1000, obj);
    solver.Solve(ls, collector, obj, log);
    Console.WriteLine("Objective value = {0}", collector.ObjectiveValue(0));
  }

  private static void BasicLs()
  {
    Solver solver = new Solver("BasicLs");
    IntVar[] vars = solver.MakeIntVarArray(4, 0, 4, "vars");
    IntVar sum_var = vars.Sum().Var();
    OptimizeVar obj = sum_var.Minimize(1);
    DecisionBuilder db = solver.MakePhase(vars,
                                          Solver.CHOOSE_FIRST_UNBOUND,
                                          Solver.ASSIGN_MAX_VALUE);
    MoveOneVar move_one_var = new MoveOneVar(vars);
    LocalSearchPhaseParameters ls_params =
        solver.MakeLocalSearchPhaseParameters(move_one_var, db);
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
    BasicLs();
  }
}
