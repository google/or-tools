// Copyright 2010-2021 Google LLC
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

package com.google.ortools.constraintsolver;

import static com.google.common.truth.Truth.assertThat;
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertNotNull;
import static org.junit.jupiter.api.Assertions.assertTrue;

import com.google.ortools.Loader;
import com.google.common.collect.Iterables;
import com.google.ortools.constraintsolver.ConstraintSolverParameters;
import com.google.ortools.constraintsolver.RegularLimitParameters;
import java.util.ArrayList;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.function.Consumer;
import java.util.function.Supplier;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

/** Tests the Constraint solver java interface. */
public final class ConstraintSolverTest {
  @BeforeEach
  public void setUp() {
    Loader.loadNativeLibraries();
  }

  @Test
  public void testSolverCtor() {
    final Solver solver = new Solver("TestSolver");
    assertNotNull(solver);
    assertEquals("TestSolver", solver.model_name());
    assertNotNull(solver.toString());
  }

  @Test
  public void testIntVar() {
    final Solver solver = new Solver("Solver");
    final IntVar var = solver.makeIntVar(3, 11, "IntVar");
    assertEquals(3, var.min());
    assertEquals(11, var.max());
  }

  @Test
  public void testIntVarArray() {
    final Solver solver = new Solver("Solver");
    final IntVar[] vars = solver.makeIntVarArray(7, 3, 5, "vars");
    assertThat(vars).hasLength(7);
    for (IntVar var : vars) {
      assertEquals(3, var.min());
      assertEquals(5, var.max());
    }
  }

  @Test
  public void testRabbitsPheasants() {
    final Solver solver = new Solver("testRabbitsPheasants");
    final IntVar rabbits = solver.makeIntVar(0, 100, "rabbits");
    final IntVar pheasants = solver.makeIntVar(0, 100, "pheasants");
    solver.addConstraint(solver.makeEquality(solver.makeSum(rabbits, pheasants), 20));
    solver.addConstraint(solver.makeEquality(
        solver.makeSum(solver.makeProd(rabbits, 4), solver.makeProd(pheasants, 2)), 56));
    final DecisionBuilder db =
        solver.makePhase(rabbits, pheasants, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MIN_VALUE);
    solver.newSearch(db);
    solver.nextSolution();
    assertEquals(8, rabbits.value());
    assertEquals(12, pheasants.value());
    solver.endSearch();
  }

  // A Decision builder that does nothing.
  static class DummyDecisionBuilder extends JavaDecisionBuilder {
    @Override
    public Decision next(Solver solver) throws Solver.FailException {
      System.out.println("In Dummy Decision Builder");
      return null;
    }

    @Override
    public String toString() {
      return "DummyDecisionBuilder";
    }
  }

  @Test
  public void testDummyDecisionBuilder() {
    final Solver solver = new Solver("testDummyDecisionBuilder");
    final DecisionBuilder db = new DummyDecisionBuilder();
    assertTrue(solver.solve(db));
  }

  /**
   * A decision builder that fails.
   */
  static class FailDecisionBuilder extends JavaDecisionBuilder {
    @Override
    public Decision next(Solver solver) throws Solver.FailException {
      System.out.println("In Fail Decision Builder");
      solver.fail();
      return null;
    }
  }

  @Test
  public void testFailDecisionBuilder() {
    final Solver solver = new Solver("testFailDecisionBuilder");
    final DecisionBuilder db = new FailDecisionBuilder();
    assertFalse(solver.solve(db));
  }

  /** Golomb Ruler test */
  @Test
  public void testGolombRuler() {
    final int m = 8;
    final Solver solver = new Solver("GR " + m);

    final IntVar[] ticks = solver.makeIntVarArray(m, 0, (1 << (m + 1)) - 1, "ticks");

    solver.addConstraint(solver.makeEquality(ticks[0], 0));

    for (int i = 0; i < ticks.length - 1; i++) {
      solver.addConstraint(solver.makeLess(ticks[i], ticks[i + 1]));
    }

    final ArrayList<IntVar> diff = new ArrayList<>();
    for (int i = 0; i < m - 1; i++) {
      for (int j = i + 1; j < m; j++) {
        diff.add(solver.makeDifference(ticks[j], ticks[i]).var());
      }
    }

    solver.addConstraint(solver.makeAllDifferent(diff.toArray(new IntVar[0]), true));

    // break symetries
    if (m > 2) {
      solver.addConstraint(solver.makeLess(diff.get(0), Iterables.getLast(diff)));
    }

    final OptimizeVar opt = solver.makeMinimize(ticks[m - 1], 1);
    final DecisionBuilder db =
        solver.makePhase(ticks, Solver.CHOOSE_MIN_SIZE_LOWEST_MIN, Solver.ASSIGN_MIN_VALUE);
    final SearchMonitor log = solver.makeSearchLog(10000, opt);
    assertTrue(solver.solve(db, opt, log));
    assertEquals(34, opt.best());
  }

  @Test
  public void testElementFunction() {
    final Solver solver = new Solver("testElementFunction");
    final IntVar index = solver.makeIntVar(0, 10, "index");
    final IntExpr element = solver.makeElement((long x) -> x * 2, index);
    assertEquals(0, element.min());
    assertEquals(20, element.max());
  }

  @Test
  public void testSolverParameters() {
    final ConstraintSolverParameters parameters = ConstraintSolverParameters.newBuilder()
                                                      .mergeFrom(Solver.defaultSolverParameters())
                                                      .setTraceSearch(true)
                                                      .build();
    final Solver solver = new Solver("testSolverParameters", parameters);
    final ConstraintSolverParameters stored = solver.parameters();
    assertTrue(stored.getTraceSearch());
  }

  @Test
  public void testRegularLimitParameters() {
    final Solver solver = new Solver("testRegularLimitParameters");
    final RegularLimitParameters protoLimit =
        RegularLimitParameters.newBuilder()
            .mergeFrom(solver.makeDefaultRegularLimitParameters())
            .setFailures(20000)
            .build();
    assertEquals(20000, protoLimit.getFailures());
    final SearchLimit limit = solver.makeLimit(protoLimit);
    assertNotNull(limit);
  }

  // verify Closure in Decision.
  @Test
  public void testClosureDecision() {
    final StringProperty call = new StringProperty("");
    final Solver solver = new Solver("ClosureDecisionTest");
    final Decision decision = solver.makeDecision(
        (Solver s) -> call.setValue("Apply"), (Solver s) -> call.setValue("Refute"));
    System.gc();

    decision.apply(solver);
    assertEquals("Apply", call.toString());

    decision.refute(solver);
    assertEquals("Refute", call.toString());
  }

  @Test
  public void testSolverInClosureDecision() {
    final Solver solver = new Solver("SolverTestName");
    final String modelName = solver.model_name();
    // Lambda can only capture final or implicit final.
    final AtomicInteger countApply = new AtomicInteger(0);
    final AtomicInteger countRefute = new AtomicInteger(0);
    assertEquals(0, countApply.intValue());
    assertEquals(0, countRefute.intValue());
    final Decision decision = solver.makeDecision(
        (Solver s) -> {
          assertEquals(s.model_name(), modelName);
          countApply.addAndGet(1);
        },
        (Solver s) -> {
          assertEquals(s.model_name(), modelName);
          countRefute.addAndGet(1);
        });
    System.gc(); // verify lambda are kept alive

    decision.apply(solver);
    assertEquals(1, countApply.intValue());
    assertEquals(0, countRefute.intValue());

    decision.refute(solver);
    assertEquals(1, countApply.intValue());
    assertEquals(1, countRefute.intValue());
  }

  // A Decision builder that does nothing
  public static class ActionDecisionBuilder extends JavaDecisionBuilder {
    Consumer<Solver> apply;
    Consumer<Solver> refute;
    boolean passed;

    public ActionDecisionBuilder(Consumer<Solver> a, Consumer<Solver> r) {
      apply = a;
      refute = r;
      passed = false;
    }

    @Override
    public Decision next(Solver solver) throws Solver.FailException {
      if (passed) {
        return null;
      }
      passed = true;
      return solver.makeDecision(apply, refute);
    }

    @Override
    public String toString() {
      return "ActionDecisionBuilder";
    }
  }

  // Tests the ActionDecisionBuilder.
  @Test
  public void testActionDecisionBuilder() {
    final Solver solver = new Solver("testActionDecisionBuilder");
    // Lambda can only capture final or implicit final
    final AtomicInteger countApply = new AtomicInteger(0);
    final AtomicInteger countRefute = new AtomicInteger(0);
    assertEquals(0, countApply.intValue());
    assertEquals(0, countRefute.intValue());
    final DecisionBuilder db = new ActionDecisionBuilder(
        (Solver s) -> countApply.addAndGet(1),
        (Solver s) -> countRefute.addAndGet(1));
    solver.newSearch(db);
    assertTrue(solver.nextSolution());
    assertEquals(1, countApply.intValue());
    assertEquals(0, countRefute.intValue());
    assertTrue(solver.nextSolution());
    assertEquals(1, countApply.intValue());
    assertEquals(1, countRefute.intValue());
    solver.endSearch();
  }

  // ----- LocalSearch Test -----
  private static class MoveOneVar extends IntVarLocalSearchOperator {
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

  @Test
  public void testSolver() {
    final Solver solver = new Solver("Solver");
    final IntVar[] vars = solver.makeIntVarArray(4, 0, 4, "vars");
    final IntVar sumVar = solver.makeSum(vars).var();
    final OptimizeVar obj = solver.makeMinimize(sumVar, 1);
    final DecisionBuilder db =
        solver.makePhase(vars, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MAX_VALUE);
    final MoveOneVar moveOneVar = new MoveOneVar(vars);
    final LocalSearchPhaseParameters lsParams =
        solver.makeLocalSearchPhaseParameters(sumVar, moveOneVar, db);
    final DecisionBuilder ls = solver.makeLocalSearchPhase(vars, db, lsParams);
    final SolutionCollector collector = solver.makeLastSolutionCollector();
    collector.addObjective(sumVar);
    final SearchMonitor log = solver.makeSearchLog(1000, obj);

    assertTrue(solver.solve(ls, collector, obj, log));
  }

  private static class SumFilter extends IntVarLocalSearchFilter {
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
    public boolean accept(Assignment delta, Assignment unusedDeltadelta, long unusedObjectiveMin,
        long unusedObjectiveMax) {
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

  private static class StringProperty {
    public StringProperty(String initialValue) {
      value = initialValue;
    }
    public void setValue(String newValue) {
      value = newValue;
    }

    @Override
    public String toString() {
      return value;
    }
    private String value;
  }

  @Test
  public void testSolverWithLocalSearchFilter() {
    final Solver solver = new Solver("Solver");
    final IntVar[] vars = solver.makeIntVarArray(4, 0, 4, "vars");
    final IntVar sumVar = solver.makeSum(vars).var();
    final OptimizeVar obj = solver.makeMinimize(sumVar, 1);
    final DecisionBuilder db =
        solver.makePhase(vars, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MAX_VALUE);
    MoveOneVar moveOneVar = new MoveOneVar(vars);
    SumFilter filter = new SumFilter(vars);
    IntVarLocalSearchFilter[] filters = new IntVarLocalSearchFilter[1];
    filters[0] = filter;
    LocalSearchFilterManager filterManager = new LocalSearchFilterManager(filters);
    LocalSearchPhaseParameters lsParams =
        solver.makeLocalSearchPhaseParameters(sumVar, moveOneVar, db, null, filterManager);
    DecisionBuilder ls = solver.makeLocalSearchPhase(vars, db, lsParams);
    SolutionCollector collector = solver.makeLastSolutionCollector();
    collector.addObjective(sumVar);
    SearchMonitor log = solver.makeSearchLog(1000, obj);
    solver.solve(ls, collector, obj, log);
  }

  private static class OneVarLns extends BaseLns {
    public OneVarLns(IntVar[] vars) {
      super(vars);
    }

    @Override
    public void initFragments() {
      index = 0;
    }

    @Override
    public boolean nextFragment() {
      int size = size();
      if (index < size) {
        appendToFragment(index);
        ++index;
        return true;
      } else {
        return false;
      }
    }
    private int index;
  }

  @Test
  public void testSolverLns() {
    final Solver solver = new Solver("Solver");
    final IntVar[] vars = solver.makeIntVarArray(4, 0, 4, "vars");
    final IntVar sumVar = solver.makeSum(vars).var();
    final OptimizeVar obj = solver.makeMinimize(sumVar, 1);
    final DecisionBuilder db =
        solver.makePhase(vars, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MAX_VALUE);
    final OneVarLns oneVarLns = new OneVarLns(vars);
    final LocalSearchPhaseParameters lsParams =
        solver.makeLocalSearchPhaseParameters(sumVar, oneVarLns, db);
    final DecisionBuilder ls = solver.makeLocalSearchPhase(vars, db, lsParams);
    final SolutionCollector collector = solver.makeLastSolutionCollector();
    collector.addObjective(sumVar);
    final SearchMonitor log = solver.makeSearchLog(1000, obj);
    solver.solve(ls, collector, obj, log);
  }

  // ----- SearchLog Test -----
  // TODO(user): Improve search log tests; currently only tests coverage and callback.
  // note: this is more or less what is done in search_test.cc
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
  @Test
  public void testSearchLog() {
    final Solver solver = new Solver("TestSearchLog");
    final IntVar var = solver.makeIntVar(1, 1, "Variable");
    solver.makeMinimize(var, 1);
    final SearchMonitor searchlog = solver.makeSearchLog(0);
    runSearchLog(searchlog);
  }

  private static class SearchCount implements Supplier<String> {
    public SearchCount(AtomicInteger initialCount) {
      count = initialCount;
    }
    @Override
    public String get() {
      count.addAndGet(1);
      return "display callback called...";
    }
    private final AtomicInteger count;
  }

  @Test
  public void testSearchLogWithCallback() {
    final Solver solver = new Solver("TestSearchLog");
    final AtomicInteger count = new AtomicInteger(0);
    final SearchMonitor searchlog = solver.makeSearchLog(0, // branch period
        new SearchCount(count));
    System.gc(); // verify SearchCount is kept alive by the searchlog
    runSearchLog(searchlog);
    assertEquals(1, count.intValue());
  }

  @Test
  public void testSearchLogWithLambdaCallback() {
    final Solver solver = new Solver("TestSearchLog");
    final AtomicInteger count = new AtomicInteger(0);
    final SearchMonitor searchlog = solver.makeSearchLog(0, // branch period
        () -> {
          count.addAndGet(1);
          return "display callback called...";
        });
    System.gc(); // verify lambda is kept alive by the searchlog
    runSearchLog(searchlog);
    assertEquals(1, count.intValue());
  }

  @Test
  public void testSearchLogWithIntVarCallback() {
    final Solver solver = new Solver("TestSearchLog");
    final IntVar var = solver.makeIntVar(1, 1, "Variable");
    final AtomicInteger count = new AtomicInteger(0);
    final SearchMonitor searchlog = solver.makeSearchLog(0, // branch period
        var, // IntVar to monitor
        new SearchCount(count));
    System.gc();
    runSearchLog(searchlog);
    assertEquals(1, count.intValue());
  }

  @Test
  public void testSearchLogWithObjectiveCallback() {
    final Solver solver = new Solver("TestSearchLog");
    final IntVar var = solver.makeIntVar(1, 1, "Variable");
    final OptimizeVar objective = solver.makeMinimize(var, 1);
    final AtomicInteger count = new AtomicInteger(0);
    final SearchMonitor searchlog = solver.makeSearchLog(0, // branch period
        objective, // objective var to monitor
        new SearchCount(count));
    System.gc();
    runSearchLog(searchlog);
    assertEquals(1, count.intValue());
  }
}
