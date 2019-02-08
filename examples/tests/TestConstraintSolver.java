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

import com.google.ortools.constraintsolver.Solver;
import com.google.ortools.constraintsolver.OptimizeVar;
import com.google.ortools.constraintsolver.IntVar;
import com.google.ortools.constraintsolver.SearchMonitor;
import com.google.ortools.constraintsolver.SearchLog;
import com.google.ortools.constraintsolver.Decision;
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
  static void testSearchLog() {
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

  static void testSearchLogWithCallback(boolean enableGC) {
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

  static void testSearchLogWithIntVarCallback(boolean enableGC) {
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

  static void testSearchLogWithObjectiveCallback(boolean enableGC) {
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

  static void testClosureDecision(boolean enableGC) {
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


  static void testSolverInClosureDecision(boolean enableGC) {
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
