// Copyright 2010-2018 Google LLC
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

import com.google.ortools.constraintsolver.Assignment;
import com.google.ortools.constraintsolver.AssignmentIntContainer;
import com.google.ortools.constraintsolver.BaseLns;
import com.google.ortools.constraintsolver.Decision;
import com.google.ortools.constraintsolver.DecisionBuilder;
import com.google.ortools.constraintsolver.IntVar;
import com.google.ortools.constraintsolver.IntVarLocalSearchFilter;
import com.google.ortools.constraintsolver.IntVarLocalSearchOperator;
import com.google.ortools.constraintsolver.LocalSearchPhaseParameters;
import com.google.ortools.constraintsolver.OptimizeVar;
import com.google.ortools.constraintsolver.SearchLog;
import com.google.ortools.constraintsolver.SearchMonitor;
import com.google.ortools.constraintsolver.SolutionCollector;
import com.google.ortools.constraintsolver.Solver;
import com.google.ortools.constraintsolver.main;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.function.Consumer;
import java.util.function.Supplier;
import java.util.logging.Logger;

/** Tests the Constraint solver java interface. */
public class TestConstraintSolver {
  static {
    System.loadLibrary("jniortools");
  }

  private static final Logger logger = Logger.getLogger(TestConstraintSolver.class.getName());

  static void testSolverCtor() throws Exception {
    logger.info("testSolverCtor...");
    Solver solver = new Solver("TestSolver");
    if (!solver.model_name().equals("TestSolver")) {throw new AssertionError("Solver ill formed");}
    if (solver.toString().length() < 0) {throw new AssertionError("Solver ill formed");}
    logger.info("testSolverCtor...DONE");
  }

  static void testIntVar() throws Exception {
    logger.info("testIntVar...");
    Solver solver = new Solver("Solver");
    IntVar var = solver.makeIntVar(3, 11, "IntVar");
    if (var.min() != 3) {throw new AssertionError("IntVar Min wrong");}
    if (var.max() != 11) {throw new AssertionError("IntVar Max wrong");}
    logger.info("testIntVar...DONE");
  }

  static void testIntVarArray() throws Exception {
    logger.info("testIntVarArray...");
    Solver solver = new Solver("Solver");
    IntVar[] vars = solver.makeIntVarArray(7, 3, 5, "vars");
    if (vars.length != 7) {throw new AssertionError("Vars length wrong");}
    for(IntVar var: vars) {
      if (var.min() != 3) {throw new AssertionError("IntVar Min wrong");}
      if (var.max() != 5) {throw new AssertionError("IntVar Max wrong");}
    }
    logger.info("testIntVarArray...DONE");
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

  static void testSolver() throws Exception {
    Solver solver = new Solver("Solver");
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
    logger.info("Objective value = " + collector.objectiveValue(0));
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

  static void testSolverWithFilter() throws Exception {
    Solver solver = new Solver("Solver");
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
    logger.info("Objective value = " + collector.objectiveValue(0));
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

  static void testSolverLns() throws Exception {
    Solver solver = new Solver("Solver");
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
    logger.info("Objective value = " + collector.objectiveValue(0));
  }

  private static void runSearchLog(SearchMonitor searchlog) {
    searchlog.enterSearch();
    searchlog.exitSearch();
    searchlog.acceptSolution();
    searchlog.atSolution();
    searchlog.beginFail();
    searchlog.noMoreSolutions();
    searchlog.beginInitialPropagation();
    searchlog.endInitialPropagation();
  }

  // Simple Coverage test...
  static void testSearchLog() throws Exception {
    logger.info("testSearchLog...");
    Solver solver = new Solver("TestSearchLog");
    IntVar var = solver.makeIntVar(1, 1, "Variable");
    OptimizeVar objective = solver.makeMinimize(var, 1);
    SearchMonitor searchlog = solver.makeSearchLog(0);
    runSearchLog(searchlog);
    logger.info("testSearchLog...DONE");
  }

  static class SearchCount implements Supplier<String> {
    public SearchCount(AtomicInteger count_) {
      count = count_;
    }
    @Override
    public String get() {
      count.addAndGet(1);
      return "display callback called...";
    }
    private AtomicInteger count;
  }

  static void testSearchLogWithCallback(boolean enableGC) throws Exception {
    logger.info("testSearchLogWithCallback (enable gc:" + enableGC + ")...");
    Solver solver = new Solver("TestSearchLog");
    IntVar var = solver.makeIntVar(1, 1, "Variable");
    OptimizeVar objective = solver.makeMinimize(var, 1);
    AtomicInteger count = new AtomicInteger(0);
    SearchMonitor searchlog = solver.makeSearchLog(
        0,  // branch period
        new SearchCount(count));
    if (enableGC) {
      System.gc(); // verify SearchCount is kept alive
    }
    runSearchLog(searchlog);
    logger.info("count:" + count.intValue());

    if (count.intValue() != 1) throw new AssertionError("count != 1"); ;
    logger.info("testSearchLogWithCallback (enable gc:" + enableGC + ")...DONE");
  }

  static void testSearchLogWithIntVarCallback(boolean enableGC) throws Exception {
    logger.info("testSearchLogWithIntVarCallback (enable gc:" + enableGC + ")...");
    Solver solver = new Solver("TestSearchLog");
    IntVar var = solver.makeIntVar(1, 1, "Variable");
    OptimizeVar objective = solver.makeMinimize(var, 1);
    AtomicInteger count = new AtomicInteger(0);
    SearchMonitor searchlog = solver.makeSearchLog(
        0,  // branch period
        var, // IntVar to monitor
        new SearchCount(count));
    if (enableGC) {
      System.gc(); // verify SearchCount is kept alive
    }
    runSearchLog(searchlog);
    if (count.intValue() != 1) throw new AssertionError("count != 1"); ;
    logger.info("testSearchLogWithIntVarCallback (enable gc:" + enableGC + ")...DONE");
  }

  static void testSearchLogWithObjectiveCallback(boolean enableGC) throws Exception {
    logger.info("testSearchLogWithObjectiveCallback (enable gc:" + enableGC + ")...");
    Solver solver = new Solver("TestSearchLog");
    IntVar var = solver.makeIntVar(1, 1, "Variable");
    OptimizeVar objective = solver.makeMinimize(var, 1);
    AtomicInteger count = new AtomicInteger(0);
    SearchMonitor searchlog = solver.makeSearchLog(
        0,  // branch period
        objective, // objective var to monitor
        new SearchCount(count));
    if (enableGC) {
      System.gc(); // verify SearchCount is kept alive
    }
    runSearchLog(searchlog);
    if (count.intValue() != 1) throw new AssertionError("count != 1"); ;
    logger.info("testSearchLogWithObjectiveCallback (enable gc:" + enableGC + ")...DONE");
  }

  static class StringProperty {
    public StringProperty(String value) {
      value_ = value;
    }
    public void setValue(String value) {
      value_ = value;;
    }
    public String toString() {
      return value_;
    }
    private String value_;
  }

  static void testClosureDecision(boolean enableGC) throws Exception {
    logger.info("testClosureDecision (enable gc:" + enableGC + ")...");
    final StringProperty call = new StringProperty("");
    Solver solver = new Solver("ClosureDecisionTest");
    Decision decision = solver.makeDecision(
        (Solver s) -> { call.setValue("Apply"); },
        (Solver s) -> { call.setValue("Refute"); });
    if (enableGC) {
      System.gc(); // verify SearchCount is kept alive
    }

    decision.apply(solver);
    if (!call.toString().equals("Apply")) {throw new AssertionError("Apply action not called");}

    decision.refute(solver);
    if (!call.toString().equals("Refute")) {throw new AssertionError("Refute action not called");}
    logger.info("testClosureDecision (enable gc:" + enableGC + ")...DONE");
  }

  static void testSolverInClosureDecision(boolean enableGC) throws Exception {
    logger.info("testSolverInClosureDecision (enable gc:" + enableGC + ")...");
    Solver solver = new Solver("SolverTestName");
    String model_name = solver.model_name();
    Decision decision = solver.makeDecision(
        (Solver s) -> {
          if (!s.model_name().equals(model_name)) {throw new AssertionError("Solver ill formed");}
        },
        (Solver s) -> {
          if (!s.model_name().equals(model_name)) {throw new AssertionError("Solver ill formed");}
        });
    if (enableGC) {
      System.gc(); // verify SearchCount is kept alive
    }

    decision.apply(solver);
    decision.refute(solver);
    logger.info("testSolverInClosureDecision (enable gc:" + enableGC + ")...DONE");
  }

  public static void main(String[] args) throws Exception {
    testSolverCtor();

    testIntVar();
    testIntVarArray();

    testSolver();
    testSolverWithFilter();
    testSolverLns();

    testSearchLog();
    testSearchLogWithCallback(/*enableGC=*/false);
    testSearchLogWithCallback(/*enableGC=*/true);
    testSearchLogWithIntVarCallback(/*enableGC=*/false);
    testSearchLogWithIntVarCallback(/*enableGC=*/true);
    testSearchLogWithObjectiveCallback(/*enableGC=*/false);
    testSearchLogWithObjectiveCallback(/*enableGC=*/true);
    testClosureDecision(/*enableGC=*/false);
    testClosureDecision(/*enableGC=*/true);
    testSolverInClosureDecision(/*enableGC=*/false);
    testSolverInClosureDecision(/*enableGC=*/true);
  }
}
