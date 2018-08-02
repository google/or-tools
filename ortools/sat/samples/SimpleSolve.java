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

import com.google.ortools.sat.*;

public class SimpleSolve {

  static {
    System.loadLibrary("jniortools");
  }

  static void SimpleSolve() {
    // Creates the model.
    CpModel model = new CpModel();
    // Creates the variables.
    int num_vals = 3;

    IntVar x = model.newIntVar(0, num_vals - 1, "x");
    IntVar y = model.newIntVar(0, num_vals - 1, "y");
    IntVar z = model.newIntVar(0, num_vals - 1, "z");
    // Creates the constraints.
    model.addDifferent(x, y);

    // Creates a solver and solves the model.
    CpSolver solver = new CpSolver();
    CpSolverStatus status = solver.solve(model);

    if (status == CpSolverStatus.FEASIBLE)
    {
      System.out.println("x = " + solver.value(x));
      System.out.println("y = " + solver.value(y));
      System.out.println("z = " + solver.value(z));
    }

  }

  public static void main(String[] args) throws Exception {
    SimpleSolve();
  }
}
