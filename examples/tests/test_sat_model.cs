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

using System;
using Google.OrTools.Sat;

public class CsTestCpOperator
{
  // TODO(user): Add proper tests.

  static int error_count_ = 0;

  static void Check(bool test, String message)
  {
    if (!test)
    {
      Console.WriteLine("Error: " + message);
      error_count_++;
    }
  }

  static void CheckLongEq(long v1, long v2, String message)
  {
    if (v1 != v2)
    {
      Console.WriteLine("Error: " + v1 + " != " + v2 + " " + message);
      error_count_++;
    }
  }

  static void CheckDoubleEq(double v1, double v2, String message)
  {
    if (v1 != v2)
    {
      Console.WriteLine("Error: " + v1 + " != " + v2 + " " + message);
      error_count_++;
    }
  }

  static void TestSimpleLinearModel() {
    Console.WriteLine("TestSimpleLinearModel");
    CpModel model = new CpModel();
    IntVar v1 = model.NewIntVar(-10, 10, "v1");
    IntVar v2 = model.NewIntVar(-10, 10, "v2");
    IntVar v3 = model.NewIntVar(-100000, 100000, "v3");
    model.AddLinearConstraint(new[] {v1, v2}, new[] {1, 1}, -1000000, 100000);
    model.AddLinearConstraint(new[] {v1, v2, v3}, new[] {1, 2, -1}, 0, 100000);

    model.Maximize(v3);

    CpSolver solver = new CpSolver();
    CpSolverStatus status = solver.Solve(model);
    Check(status == CpSolverStatus.Optimal, "Wrong status after solve");
    Console.WriteLine("Status = " + status);
    Console.WriteLine("model = " + model.Model.ToString());
    Console.WriteLine("response = " + solver.Response.ToString());
  }

  static void TestSimpleLinearModel2() {
    Console.WriteLine("TestSimpleLinearModel2");
    CpModel model = new CpModel();
    IntVar v1 = model.NewIntVar(-10, 10, "v1");
    IntVar v2 = model.NewIntVar(-10, 10, "v2");
    model.AddLinearConstraint(new[] {v1, v2}, new[] {1, 1}, -1000000, 100000);
    model.Maximize(v1 - 2 * v2);

    CpSolver solver = new CpSolver();
    CpSolverStatus status = solver.Solve(model);
    Check(status == CpSolverStatus.Optimal, "Wrong status after solve");
    CheckDoubleEq(30.0, solver.ObjectiveValue, "Wrong solution value");
    Console.WriteLine("response = " + solver.Response.ToString());
  }

  static void TestSimpleLinearModel3() {
    Console.WriteLine("TestSimpleLinearModel3");
    CpModel model = new CpModel();
    IntVar v1 = model.NewIntVar(-10, 10, "v1");
    IntVar v2 = model.NewIntVar(-10, 10, "v2");
    model.Add(-100000 <= v1 + 2 * v2 <= 100000);
    model.Minimize(v1 - 2 * v2);

    CpSolver solver = new CpSolver();
    CpSolverStatus status = solver.Solve(model);
    Check(status == CpSolverStatus.Optimal, "Wrong status after solve");
    CheckDoubleEq(-30.0, solver.ObjectiveValue, "Wrong solution value");
    CheckLongEq(-10, solver.Value(v1), "Wrong value");
    CheckLongEq(10, solver.Value(v2), "Wrong value");
    CheckLongEq(-30, solver.Value(v1 - 2 * v2), "Wrong value");
  }

  static void TestDivision() {
    Console.WriteLine("TestDivision");
    CpModel model = new CpModel();
    IntVar v1 = model.NewIntVar(0, 10, "v1");
    IntVar v2 = model.NewIntVar(1, 10, "v2");
    model.AddDivisionEquality(3, v1, v2);

    Console.WriteLine(model.Model);

    CpSolver solver = new CpSolver();
    CpSolverStatus status = solver.Solve(model);
    Check(status == CpSolverStatus.Feasible, "Wrong status after solve");
    Console.WriteLine("v1 = {0}", solver.Value(v1));
    Console.WriteLine("v2 = {0}", solver.Value(v2));
  }

  static void TestModulo() {
    Console.WriteLine("TestModulo");
    CpModel model = new CpModel();
    IntVar v1 = model.NewIntVar(1, 10, "v1");
    IntVar v2 = model.NewIntVar(1, 10, "v2");
    model.AddModuloEquality(3, v1, v2);

    Console.WriteLine(model.Model);

    // CpSolver solver = new CpSolver();
    // CpSolverStatus status = solver.Solve(model);
    // Check(status == CpSolverStatus.ModelSat, "Wrong status after solve");
    // Console.WriteLine("v1 = {0}", solver.Value(v1));
    // Console.WriteLine("v2 = {0}", solver.Value(v2));
  }


  static void Main() {
    TestSimpleLinearModel();
    TestSimpleLinearModel2();
    TestSimpleLinearModel3();
    TestDivision();
    TestModulo();
    if (error_count_ != 0) {
      Console.WriteLine("Found " + error_count_ + " errors.");
      Environment.Exit(1);
    }
  }
}
