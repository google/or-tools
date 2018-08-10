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

import com.google.ortools.sat.CpSolverStatus;
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.CpSolver;
import com.google.ortools.sat.CpSolverSolutionCallback;
import com.google.ortools.sat.IntVar;

class VarArraySolutionPrinter extends CpSolverSolutionCallback {
  public VarArraySolutionPrinter(IntVar[] variables) {
    variables_ = variables;
  }

  @Override
  public void onSolutionCallback() {
    System.out.println(String.format("Solution #%d: time = %.02f s", solutionCount_, wallTime()));
    for (IntVar v : variables_) {
      System.out.println(String.format("  %s = %d", v.getName(), value(v)));
    }
    solutionCount_++;
  }

  public int solutionCount() {
    return solutionCount_;
  }

  private int solutionCount_;
  private IntVar[] variables_;
}

public class SolveAllSolutions {

  static { System.loadLibrary("jniortools"); }

  public static void main(String[] args) throws Exception {
    // Creates the model.
    CpModel model = new CpModel();
    // Creates the variables.
    int numVals = 3;

    IntVar x = model.newIntVar(0, numVals - 1, "x");
    IntVar y = model.newIntVar(0, numVals - 1, "y");
    IntVar z = model.newIntVar(0, numVals - 1, "z");
    // Creates the constraints.
    model.addDifferent(x, y);

    // Creates a solver and solves the model.
    CpSolver solver = new CpSolver();
    VarArraySolutionPrinter cb = new VarArraySolutionPrinter(new IntVar[] {x, y, z});
    CpSolverStatus status = solver.searchAllSolutions(model, cb);

    System.out.println(cb.solutionCount() + " solutions found.");
  }
}
