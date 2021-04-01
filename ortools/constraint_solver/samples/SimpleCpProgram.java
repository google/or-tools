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

// [START program]
package com.google.ortools.constraintsolver.samples;
// [START import]
import com.google.ortools.Loader;
import com.google.ortools.constraintsolver.DecisionBuilder;
import com.google.ortools.constraintsolver.IntVar;
import com.google.ortools.constraintsolver.Solver;
import java.util.logging.Logger;
// [END import]

/** Simple CP Program.*/
public class SimpleCpProgram {
  private SimpleCpProgram() {}

  private static final Logger logger = Logger.getLogger(SimpleCpProgram.class.getName());

  public static void main(String[] args) throws Exception {
    Loader.loadNativeLibraries();
    // Instantiate the solver.
    // [START solver]
    Solver solver = new Solver("CpSimple");
    // [END solver]

    // Create the variables.
    // [START variables]
    final long numVals = 3;
    final IntVar x = solver.makeIntVar(0, numVals - 1, "x");
    final IntVar y = solver.makeIntVar(0, numVals - 1, "y");
    final IntVar z = solver.makeIntVar(0, numVals - 1, "z");
    // [END variables]

    // Constraint 0: x != y..
    // [START constraints]
    solver.addConstraint(solver.makeAllDifferent(new IntVar[] {x, y}));
    logger.info("Number of constraints: " + solver.constraints());
    // [END constraints]

    // Solve the problem.
    // [START solve]
    final DecisionBuilder db = solver.makePhase(
        new IntVar[] {x, y, z}, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MIN_VALUE);
    // [END solve]

    // Print solution on console.
    // [START print_solution]
    int count = 0;
    solver.newSearch(db);
    while (solver.nextSolution()) {
      ++count;
      logger.info(
          String.format("Solution: %d\n x=%d y=%d z=%d", count, x.value(), y.value(), z.value()));
    }
    solver.endSearch();
    logger.info("Number of solutions found: " + solver.solutions());
    // [END print_solution]

    // [START advanced]
    logger.info(String.format("Advanced usage:\nProblem solved in %d ms\nMemory usage: %d bytes",
        solver.wallTime(), Solver.memoryUsage()));
    // [END advanced]
  }
}
// [END program]
