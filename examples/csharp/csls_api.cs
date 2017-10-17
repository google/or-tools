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
 * Shows how to write a custom lns operator.
 */

public class OneVarLns : BaseLns
{
  public OneVarLns(IntVar[] vars) : base(vars) {}

  public override void InitFragments()
  {
    index_ = 0;
  }

  public override bool NextFragment()
  {
    int size = Size();
    if (index_ < size)
    {
      AppendToFragment(index_);
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

  protected override bool MakeOneNeighbor()
  {
    long current_value = OldValue(variable_index_);
    if (move_up_)
    {
      SetValue(variable_index_, current_value  + 1);
      variable_index_ = (variable_index_ + 1) % Size();
    }
    else
    {
      SetValue(variable_index_, current_value  - 1);
    }
    move_up_ = !move_up_;
    return true;
  }

  // Index of the next variable to try to restore
  private long variable_index_;
  // Direction of the modification.
  private bool move_up_;
};

public class SumFilter : IntVarLocalSearchFilter {
  public SumFilter(IntVar[] vars) : base(vars)
  {
    sum_ = 0;
  }

  protected override void OnSynchronize(Assignment delta)
  {
    sum_ = 0;
    for (int index = 0; index < Size(); ++index)
    {
      sum_ += Value(index);
    }
  }

  public override bool Accept(Assignment delta, Assignment unused_deltadelta) {
    AssignmentIntContainer solution_delta = delta.IntVarContainer();
    int solution_delta_size = solution_delta.Size();

    for (int i = 0; i < solution_delta_size; ++i)
    {
      if (!solution_delta.Element(i).Activated())
      {
        return true;
      }
    }
    long new_sum = sum_;
    for (int index = 0; index < solution_delta_size; ++index)
    {
      int touched_var = Index(solution_delta.Element(index).Var());
      long old_value = Value(touched_var);
      long new_value = solution_delta.Element(index).Value();
      new_sum += new_value - old_value;
    }
    return new_sum < sum_;
  }

 private long sum_;
};

public class CsLsApi
{
  private static void BasicLns()
  {
    Console.WriteLine("BasicLns");
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
    Console.WriteLine("BasicLs");
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

  private static void BasicLsWithFilter()
  {
    Console.WriteLine("BasicLsWithFilter");
    Solver solver = new Solver("BasicLs");
    IntVar[] vars = solver.MakeIntVarArray(4, 0, 4, "vars");
    IntVar sum_var = vars.Sum().Var();
    OptimizeVar obj = sum_var.Minimize(1);
    DecisionBuilder db = solver.MakePhase(vars,
                                          Solver.CHOOSE_FIRST_UNBOUND,
                                          Solver.ASSIGN_MAX_VALUE);
    MoveOneVar move_one_var = new MoveOneVar(vars);
    SumFilter filter = new SumFilter(vars);
    IntVarLocalSearchFilter[] filters =
        new IntVarLocalSearchFilter[] { filter };
    LocalSearchPhaseParameters ls_params =
        solver.MakeLocalSearchPhaseParameters(move_one_var, db, null, filters);
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
    BasicLsWithFilter();
  }
}
