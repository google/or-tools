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

package com.google.ortools.samples;

import com.google.ortools.constraintsolver.Assignment;
import com.google.ortools.constraintsolver.AssignmentIntContainer;
import com.google.ortools.constraintsolver.BaseLns;
import com.google.ortools.constraintsolver.DecisionBuilder;
import com.google.ortools.constraintsolver.IntVar;
import com.google.ortools.constraintsolver.IntVarLocalSearchFilter;
import com.google.ortools.constraintsolver.IntVarLocalSearchOperator;
import com.google.ortools.constraintsolver.LocalSearchPhaseParameters;
import com.google.ortools.constraintsolver.OptimizeVar;
import com.google.ortools.constraintsolver.SearchMonitor;
import com.google.ortools.constraintsolver.SolutionCollector;
import com.google.ortools.constraintsolver.Solver;

/**
 * Sample showing how to model using the constraint programming solver.
 *
 */
public class LsApi {

  static {
    System.loadLibrary("jniortools");
  }



  static class OneVarLns extends BaseLns {
    public OneVarLns(IntVar[] vars) {
      super(vars);
    }

    @Override
    public void initFragments() {
      index_ = 0;
    }

    @Override
    public boolean nextFragment() {
      int size = size();
      if (index_ < size) {
        appendToFragment(index_);
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
      variableIndex = 0;
      moveUp = false;
    }

    @Override
    protected boolean oneNeighbor() {
      long currentValue = oldValue(variableIndex);
      if (moveUp) {
        setValue(variableIndex, currentValue + 1);
        variableIndex = (variableIndex + 1) % size();
      } else {
        setValue(variableIndex, currentValue - 1);
      }
      moveUp = !moveUp;
      return true;
    }

    @Override
    public void onStart() {}

    // Index of the next variable to try to restore
    private long variableIndex;
    // Direction of the modification.
    private boolean moveUp;
  }

  static class SumFilter extends IntVarLocalSearchFilter {
    public SumFilter(IntVar[] vars) {
      super(vars);
      sum = 0;
    }

    @Override
    protected void onSynchronize(Assignment unusedDelta) {
      sum = 0;
      for (int index = 0; index < size(); ++index) {
        sum += value(index);
      }
    }

    @Override
    public boolean accept(Assignment delta, Assignment unusedDeltadelta) {
      AssignmentIntContainer solutionDelta = delta.intVarContainer();
      int solutionDeltaSize = solutionDelta.size();

      for (int i = 0; i < solutionDeltaSize; ++i) {
        if (!solutionDelta.element(i).activated()) {
          return true;
        }
      }
      long newSum = sum;
      for (int index = 0; index < solutionDeltaSize; ++index) {
        int touchedVar = index(solutionDelta.element(index).var());
        long oldValue = value(touchedVar);
        long newValue = solutionDelta.element(index).value();
        newSum += newValue - oldValue;
      }
      return newSum < sum;
    }

    private long sum;
  }

  private static void basicLns() {
    System.out.println("basicLns");
    Solver solver = new Solver("basicLns");
    IntVar[] vars = solver.makeIntVarArray(4, 0, 4, "vars");
    IntVar sumVar = solver.makeSum(vars).var();
    OptimizeVar obj = solver.makeMinimize(sumVar, 1);
    DecisionBuilder db =
        solver.makePhase(vars, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MAX_VALUE);
    OneVarLns oneVarLns = new OneVarLns(vars);
    LocalSearchPhaseParameters lsParams = solver.makeLocalSearchPhaseParameters(oneVarLns, db);
    DecisionBuilder ls = solver.makeLocalSearchPhase(vars, db, lsParams);
    SolutionCollector collector = solver.makeLastSolutionCollector();
    collector.addObjective(sumVar);
    SearchMonitor log = solver.makeSearchLog(1000, obj);
    solver.solve(ls, collector, obj, log);
    System.out.println("Objective value = " + collector.objectiveValue(0));
  }

  private static void basicLs() {
    System.out.println("basicLs");
    Solver solver = new Solver("basicLs");
    IntVar[] vars = solver.makeIntVarArray(4, 0, 4, "vars");
    IntVar sumVar = solver.makeSum(vars).var();
    OptimizeVar obj = solver.makeMinimize(sumVar, 1);
    DecisionBuilder db =
        solver.makePhase(vars, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MAX_VALUE);
    MoveOneVar moveOneVar = new MoveOneVar(vars);
    LocalSearchPhaseParameters lsParams = solver.makeLocalSearchPhaseParameters(moveOneVar, db);
    DecisionBuilder ls = solver.makeLocalSearchPhase(vars, db, lsParams);
    SolutionCollector collector = solver.makeLastSolutionCollector();
    collector.addObjective(sumVar);
    SearchMonitor log = solver.makeSearchLog(1000, obj);
    solver.solve(ls, collector, obj, log);
    System.out.println("Objective value = " + collector.objectiveValue(0));
  }

  private static void basicLsWithFilter() {
    System.out.println("basicLsWithFilter");
    Solver solver = new Solver("basicLs");
    IntVar[] vars = solver.makeIntVarArray(4, 0, 4, "vars");
    IntVar sumVar = solver.makeSum(vars).var();
    OptimizeVar obj = solver.makeMinimize(sumVar, 1);
    DecisionBuilder db =
        solver.makePhase(vars, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MAX_VALUE);
    MoveOneVar moveOneVar = new MoveOneVar(vars);
    SumFilter filter = new SumFilter(vars);
    IntVarLocalSearchFilter[] filters = new IntVarLocalSearchFilter[1];
    filters[0] = filter;
    LocalSearchPhaseParameters lsParams =
        solver.makeLocalSearchPhaseParameters(moveOneVar, db, null, filters);
    DecisionBuilder ls = solver.makeLocalSearchPhase(vars, db, lsParams);
    SolutionCollector collector = solver.makeLastSolutionCollector();
    collector.addObjective(sumVar);
    SearchMonitor log = solver.makeSearchLog(1000, obj);
    solver.solve(ls, collector, obj, log);
    System.out.println("Objective value = " + collector.objectiveValue(0));
  }

  public static void main(String[] args) throws Exception {
    LsApi.basicLns();
    LsApi.basicLs();
    LsApi.basicLsWithFilter();
  }
}
