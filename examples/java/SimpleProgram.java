// Copyright 2018 Google LLC
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
import com.google.ortools.linearsolver.MPConstraint;
import com.google.ortools.linearsolver.MPObjective;
import com.google.ortools.linearsolver.MPSolver;
import com.google.ortools.linearsolver.MPVariable;

public class SimpleProgram {
  static { System.loadLibrary("jniortools"); }

  public static void main(String[] args) throws Exception {
    MPSolver solver = new MPSolver("SimpleProgram",
        MPSolver.OptimizationProblemType.valueOf("GLOP_LINEAR_PROGRAMMING"));
    // Create the variables x and y.
    MPVariable x = solver.makeNumVar(0.0, 1.0, "x");
    MPVariable y = solver.makeNumVar(0.0, 2.0, "y");
    // Create the objective function, x + y.
    MPObjective objective = solver.objective();
    objective.setCoefficient(y, 1);
    objective.setMaximization();
    // Call the solver and display the results.
    solver.solve();
    System.out.println("Solution:");
    System.out.println("x = " + x.solutionValue());
    System.out.println("y = " + y.solutionValue());
  }
}
// [END program]
