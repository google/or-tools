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

import com.google.ortools.linearsolver.MPConstraint;
import com.google.ortools.linearsolver.MPObjective;
import com.google.ortools.linearsolver.MPSolver;
import com.google.ortools.linearsolver.MPVariable;


public class TestLp {
  static { System.loadLibrary("jniortools"); }

  static void testSameConstraintName() {
    MPSolver solver = new MPSolver(
        "My_solver_name",
        MPSolver.OptimizationProblemType.CBC_MIXED_INTEGER_PROGRAMMING);
    boolean success = true;
    solver.makeConstraint("my_const_name");
    try {
      solver.makeConstraint("my_const_name");
    } catch(Throwable e) {
      System.out.println(e);
      success = false;
    }
    System.out.println("Success = " + success);
  }

  public static void main(String[] args) throws Exception {
    testSameConstraintName();
  }
}
