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

import java.util.logging.Logger;
import com.google.ortools.constraintsolver.VoidToString;
import com.google.ortools.constraintsolver.Solver;
import com.google.ortools.constraintsolver.OptimizeVar;
import com.google.ortools.constraintsolver.IntVar;
import com.google.ortools.constraintsolver.SearchMonitor;
import com.google.ortools.constraintsolver.SearchLog;
import com.google.ortools.constraintsolver.main;

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

  static class SearchCount extends VoidToString {
    public SearchCount(Integer count_) {
      count = count_;
    }
    @Override
    public String run() {
      count++;
      return "display callback...";
    }
    private Integer count;
  }

  static void testSearchLogWithCallback(boolean enableGC) {
    logger.info("testSearchLogWithCallback (enable gc:" + enableGC + ")...");
    Solver solver = new Solver("TestSearchLog");
    IntVar var = solver.makeIntVar(1, 1, "Variable");
    OptimizeVar objective = solver.makeMinimize(var, 1);
    Integer count = new Integer(0);
    SearchMonitor searchlog = solver.makeSearchLog(
        0,  // branch period
        new SearchCount(count));
    if (enableGC) {
      System.gc(); // verify SearchCount is kept alive
    }
    runSearchLog(searchlog);
    assert 1 == count;
    logger.info("testSearchLogWithCallback (enable gc:" + enableGC + ")...DONE");
  }

  static void testSearchLogWithObjectiveCallback(boolean enableGC) {
    logger.info("testSearchLogWithCallback (enable gc:" + enableGC + ")...");
    Solver solver = new Solver("TestSearchLog");
    IntVar var = solver.makeIntVar(1, 1, "Variable");
    OptimizeVar objective = solver.makeMinimize(var, 1);
    Integer count = new Integer(0);
    SearchMonitor searchlog = solver.makeSearchLog(
        0,  // branch period
        objective, // objective var to monitor
        new SearchCount(count));
    if (enableGC) {
      System.gc(); // verify SearchCount is kept alive
    }
    runSearchLog(searchlog);
    assert 1 == count;
    logger.info("testSearchLogWithCallback (enable gc:" + enableGC + ")...DONE");
  }

  public static void main(String[] args) throws Exception {
    testSearchLog();
    testSearchLogWithCallback(/*enableGC=*/false);
    testSearchLogWithCallback(/*enableGC=*/true);
    testSearchLogWithObjectiveCallback(/*enableGC=*/false);
    testSearchLogWithObjectiveCallback(/*enableGC=*/true);
  }
}
