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

public class RabbitsAndPheasants {

  static {
    System.loadLibrary("jniortools");
  }

  static void RabbitsAndPheasants() {
    // Creates the model.
    CpModel model = new CpModel();
    // Creates the variables.
    IntVar r = model.newIntVar(0, 100, "r");
    IntVar p = model.newIntVar(0, 100, "p");
    // 20 heads.
    model.addLinearSum(new IntVar[] {r, p}, 20, 20);
    // 56 legs.
    model.addScalProd(new IntVar[] {r, p}, new long[] {4, 2}, 56, 56);

    // Creates a solver and solves the model.
    CpSolver solver = new CpSolver();
    CpSolverStatus status = solver.solve(model);

    if (status == CpSolverStatus.FEASIBLE)
    {
      System.out.println(solver.value(r) + " rabbits, and " +
                         solver.value(p) + " pheasants");
    }
  }

  public static void main(String[] args) throws Exception {
    RabbitsAndPheasants();
  }
}
