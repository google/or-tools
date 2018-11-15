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

import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.CpSolver;
import com.google.ortools.sat.CpSolverStatus;
import com.google.ortools.sat.IntVar;

/**
 * In a field of rabbits and pheasants, there are 20 heads and 56 legs. How many rabbits and
 * pheasants are there?
 */
public class RabbitsAndPheasantsSat {
  static {
    System.loadLibrary("jniortools");
  }

  public static void main(String[] args) throws Exception {
    // Creates the model.
    CpModel model = new CpModel();
    // Creates the variables.
    IntVar r = model.newIntVar(0, 100, "r");
    IntVar p = model.newIntVar(0, 100, "p");
    // 20 heads.
    model.addLinearSumEqual(new IntVar[] {r, p}, 20);
    // 56 legs.
    model.addScalProdEqual(new IntVar[] {r, p}, new long[] {4, 2}, 56);

    // Creates a solver and solves the model.
    CpSolver solver = new CpSolver();
    CpSolverStatus status = solver.solve(model);

    if (status == CpSolverStatus.FEASIBLE) {
      System.out.println(solver.value(r) + " rabbits, and " + solver.value(p) + " pheasants");
    }
  }
}
