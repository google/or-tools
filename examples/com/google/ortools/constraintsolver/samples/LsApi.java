// Copyright 2010-2012 Google
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

package com.google.ortools.constraintsolver.samples;

import com.google.ortools.constraintsolver.Assignment;
import com.google.ortools.constraintsolver.AssignmentIntContainer;
import com.google.ortools.constraintsolver.BaseLNS;
import com.google.ortools.constraintsolver.DecisionBuilder;
import com.google.ortools.constraintsolver.IntVar;
import com.google.ortools.constraintsolver.IntVarLocalSearchFilter;
import com.google.ortools.constraintsolver.IntVarLocalSearchOperator;
import com.google.ortools.constraintsolver.IntVector;
import com.google.ortools.constraintsolver.LocalSearchPhaseParameters;
import com.google.ortools.constraintsolver.OptimizeVar;
import com.google.ortools.constraintsolver.SearchMonitor;
import com.google.ortools.constraintsolver.SolutionCollector;
import com.google.ortools.constraintsolver.Solver;

import java.util.logging.Logger;

/**
 * Sample showing how to model using the constraint programming solver.
 *
 */

public class LsApi {
  static class OneVarLns extends BaseLNS {
    public OneVarLns(IntVar[] vars) {
      super(vars);
    }

    @Override
    public void initFragments() {
      index_ = 0;
    }

    @Override
    public boolean nextFragment(IntVector fragment) {
      int size = size();
      if (index_ < size) {
        fragment.add(index_);
        ++index_;
        return true;
      } else {
        return false;
      }
    }

    private int index_;
  }

  static class MoveOneVar extends IntVarLocalSearchOperator {
    public MoveOneVar(IntVar[] variables) {
      super(variables);
      variable_index_ = 0;
      move_up_ = false;
    }

    @Override
    protected boolean makeOneNeighbor() {
      long current_value = oldValue(variable_index_);
      if (move_up_) {
        setValue(variable_index_, current_value  + 1);
        variable_index_ = (variable_index_ + 1) % size();
      } else {
        setValue(variable_index_, current_value  - 1);
      }
      move_up_ = !move_up_;
      return true;
    }

    @Override
    protected void onStart() {}

    // Index of the next variable to try to restore
    private long variable_index_;
    // Direction of the modification.
    private boolean move_up_;
  }

  static class SumFilter extends IntVarLocalSearchFilter {
    public SumFilter(IntVar[] vars) {
      super(vars);
      sum_ = 0;
    }

    @Override
    protected void onSynchronize() {
      sum_ = 0;
      for (int index = 0; index < size(); ++index) {
        sum_ += value(index);
      }
    }

    @Override
    public boolean accept(Assignment delta, Assignment unused_deltadelta) {
      AssignmentIntContainer solution_delta = delta.intVarContainer();
      int solution_delta_size = solution_delta.size();

      for (int i = 0; i < solution_delta_size; ++i) {
        if (!solution_delta.element(i).activated()) {
          return true;
        }
      }
      long new_sum = sum_;
      for (int index = 0; index < solution_delta_size; ++index) {
        int touched_var = index(solution_delta.element(index).var());
        long old_value = value(touched_var);
        long new_value = solution_delta.element(index).value();
        new_sum += new_value - old_value;
      }
      return new_sum < sum_;
    }

    private long sum_;
  }

  private static Logger logger =
      Logger.getLogger(LsApi.class.getName());

  static {
    System.loadLibrary("jniortools");
  }

  private static void BasicLns()
  {
    System.out.println("BasicLns");
    Solver solver = new Solver("BasicLns");
    IntVar[] vars = solver.makeIntVarArray(4, 0, 4, "vars");
    IntVar sum_var = solver.makeSum(vars).var();
    OptimizeVar obj = solver.makeMinimize(sum_var, 1);
    DecisionBuilder db = solver.makePhase(vars,
                                          Solver.CHOOSE_FIRST_UNBOUND,
                                          Solver.ASSIGN_MAX_VALUE);
    OneVarLns one_var_lns = new OneVarLns(vars);
    LocalSearchPhaseParameters ls_params =
        solver.makeLocalSearchPhaseParameters(one_var_lns, db);
    DecisionBuilder ls = solver.makeLocalSearchPhase(vars, db, ls_params);
    SolutionCollector collector = solver.makeLastSolutionCollector();
    collector.addObjective(sum_var);
    SearchMonitor log = solver.makeSearchLog(1000, obj);
    solver.solve(ls, collector, obj, log);
    System.out.println("Objective value = " + collector.objectiveValue(0));
  }

  private static void BasicLs() {
    System.out.println("BasicLs");
    Solver solver = new Solver("BasicLs");
    IntVar[] vars = solver.makeIntVarArray(4, 0, 4, "vars");
    IntVar sum_var = solver.makeSum(vars).var();
    OptimizeVar obj = solver.makeMinimize(sum_var, 1);
    DecisionBuilder db = solver.makePhase(vars,
                                          Solver.CHOOSE_FIRST_UNBOUND,
                                          Solver.ASSIGN_MAX_VALUE);
    MoveOneVar move_one_var = new MoveOneVar(vars);
    LocalSearchPhaseParameters ls_params =
        solver.makeLocalSearchPhaseParameters(move_one_var, db);
    DecisionBuilder ls = solver.makeLocalSearchPhase(vars, db, ls_params);
    SolutionCollector collector = solver.makeLastSolutionCollector();
    collector.addObjective(sum_var);
    SearchMonitor log = solver.makeSearchLog(1000, obj);
    solver.solve(ls, collector, obj, log);
    System.out.println("Objective value = " + collector.objectiveValue(0));
  }

  private static void BasicLsWithFilter() {
    System.out.println("BasicLsWithFilter");
    Solver solver = new Solver("BasicLs");
    IntVar[] vars = solver.makeIntVarArray(4, 0, 4, "vars");
    IntVar sum_var = solver.makeSum(vars).var();
    OptimizeVar obj = solver.makeMinimize(sum_var, 1);
    DecisionBuilder db = solver.makePhase(vars,
                                          Solver.CHOOSE_FIRST_UNBOUND,
                                          Solver.ASSIGN_MAX_VALUE);
    MoveOneVar move_one_var = new MoveOneVar(vars);
    SumFilter filter = new SumFilter(vars);
    IntVarLocalSearchFilter[] filters = new IntVarLocalSearchFilter[1];
    filters[0] = filter;
    LocalSearchPhaseParameters ls_params =
        solver.makeLocalSearchPhaseParameters(move_one_var, db, null, filters);
    DecisionBuilder ls = solver.makeLocalSearchPhase(vars, db, ls_params);
    SolutionCollector collector = solver.makeLastSolutionCollector();
    collector.addObjective(sum_var);
    SearchMonitor log = solver.makeSearchLog(1000, obj);
    solver.solve(ls, collector, obj, log);
    System.out.println("Objective value = " + collector.objectiveValue(0));
  }

  public static void main(String[] args) throws Exception {
    LsApi.BasicLns();
    LsApi.BasicLs();
    LsApi.BasicLsWithFilter();
  }
}
