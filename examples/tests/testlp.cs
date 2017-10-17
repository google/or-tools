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

using System;
using Google.OrTools.LinearSolver;

public class CsTestLp
{

  static int error_count = 0;

  static void Check(bool test, String message)
  {
    if (!test)
    {
      Console.WriteLine("Error: " + message);
      error_count++;
    }
  }

  static void CheckDoubleEq(double v1, double v2, String message)
  {
    if (v1 != v2)
    {
      Console.WriteLine("Error: " + v1 + " != " + v2 + " " + message);
      error_count++;
    }
  }

  static void TestVarOperator()
  {
    Console.WriteLine("Running TestVarOperator");
    Solver solver = new Solver("TestVarOperator",
                               Solver.CLP_LINEAR_PROGRAMMING);
    Variable x = solver.MakeNumVar(0.0, 100.0, "x");
    Constraint ct1 = solver.Add(x >= 1);
    Constraint ct2 = solver.Add(x <= 1);
    Constraint ct3 = solver.Add(x == 1);
    Constraint ct4 = solver.Add(1 >= x);
    Constraint ct5 = solver.Add(1 <= x);
    Constraint ct6 = solver.Add(1 == x);
    CheckDoubleEq(ct1.GetCoefficient(x), 1.0, "test1");
    CheckDoubleEq(ct2.GetCoefficient(x), 1.0, "test2");
    CheckDoubleEq(ct3.GetCoefficient(x), 1.0, "test3");
    CheckDoubleEq(ct4.GetCoefficient(x), 1.0, "test4");
    CheckDoubleEq(ct5.GetCoefficient(x), 1.0, "test5");
    CheckDoubleEq(ct6.GetCoefficient(x), 1.0, "test6");
    CheckDoubleEq(ct1.Lb(), 1.0, "test7");
    CheckDoubleEq(ct1.Ub(), double.PositiveInfinity, "test8");
    CheckDoubleEq(ct2.Lb(), double.NegativeInfinity, "test9");
    CheckDoubleEq(ct2.Ub(), 1.0, "test10");
    CheckDoubleEq(ct3.Lb(), 1.0, "test11");
    CheckDoubleEq(ct3.Ub(), 1.0, "test12");
    CheckDoubleEq(ct4.Lb(), double.NegativeInfinity, "test13");
    CheckDoubleEq(ct4.Ub(), 1.0, "test14");
    CheckDoubleEq(ct5.Lb(), 1.0, "test15");
    CheckDoubleEq(ct5.Ub(), double.PositiveInfinity, "test16");
    CheckDoubleEq(ct6.Lb(), 1.0, "test17");
    CheckDoubleEq(ct6.Ub(), 1.0, "test18");
  }

  static void TestVarAddition()
  {
    Console.WriteLine("Running TestVarAddition");
    Solver solver = new Solver("TestVarAddition",
                               Solver.CLP_LINEAR_PROGRAMMING);
    Variable x = solver.MakeNumVar(0.0, 100.0, "x");
    Variable y = solver.MakeNumVar(0.0, 100.0, "y");
    Constraint ct1 = solver.Add(x + y == 1);
    CheckDoubleEq(ct1.GetCoefficient(x), 1.0, "test1");
    CheckDoubleEq(ct1.GetCoefficient(y), 1.0, "test2");
    Constraint ct2 = solver.Add(x + x == 1);
    CheckDoubleEq(ct2.GetCoefficient(x), 2.0, "test3");
    Constraint ct3 = solver.Add(x + (y + x) == 1);
    CheckDoubleEq(ct3.GetCoefficient(x), 2.0, "test4");
    CheckDoubleEq(ct3.GetCoefficient(y), 1.0, "test5");
    Constraint ct4 = solver.Add(x + (y + x + 3) == 1);
    CheckDoubleEq(ct4.GetCoefficient(x), 2.0, "test4");
    CheckDoubleEq(ct4.GetCoefficient(y), 1.0, "test5");
    CheckDoubleEq(ct4.Lb(), -2.0, "test6");
    CheckDoubleEq(ct4.Ub(), -2.0, "test7");
  }

  static void TestVarMultiplication()
  {
    Console.WriteLine("Running TestVarMultiplication");
    Solver solver = new Solver("TestVarMultiplication",
                                   Solver.CLP_LINEAR_PROGRAMMING);
    Variable x = solver.MakeNumVar(0.0, 100.0, "x");
    Variable y = solver.MakeNumVar(0.0, 100.0, "y");
    Constraint ct1 = solver.Add(3 * x == 1);
    CheckDoubleEq(ct1.GetCoefficient(x), 3.0, "test1");
    Constraint ct2 = solver.Add(x * 3 == 1);
    CheckDoubleEq(ct2.GetCoefficient(x), 3.0, "test2");
    Constraint ct3 = solver.Add(x + (2 * y + 3 * x) == 1);
    CheckDoubleEq(ct3.GetCoefficient(x), 4.0, "test3");
    CheckDoubleEq(ct3.GetCoefficient(y), 2.0, "test4");
    Constraint ct4 = solver.Add(x + 5 * (y + x + 3) == 1);
    CheckDoubleEq(ct4.GetCoefficient(x), 6.0, "test5");
    CheckDoubleEq(ct4.GetCoefficient(y), 5.0, "test6");
    CheckDoubleEq(ct4.Lb(), -14.0, "test7");
    CheckDoubleEq(ct4.Ub(), -14.0, "test8");
    Constraint ct5 = solver.Add(x + (2 * y + x + 3) * 3 == 1);
    CheckDoubleEq(ct5.GetCoefficient(x), 4.0, "test9");
    CheckDoubleEq(ct5.GetCoefficient(y), 6.0, "test10");
    CheckDoubleEq(ct5.Lb(), -8.0, "test11");
    CheckDoubleEq(ct5.Ub(), -8.0, "test12");
  }

  static void TestBinaryOperations()
  {
    Console.WriteLine("Running TestBinaryOperations");
    Solver solver = new Solver("TestBinaryOperations",
                               Solver.CLP_LINEAR_PROGRAMMING);
    Variable x = solver.MakeNumVar(0.0, 100.0, "x");
    Variable y = solver.MakeNumVar(0.0, 100.0, "y");
    Constraint ct1 = solver.Add(x == y);
    CheckDoubleEq(ct1.GetCoefficient(x), 1.0, "test1");
    CheckDoubleEq(ct1.GetCoefficient(y), -1.0, "test2");
    Constraint ct2 = solver.Add(x == 3 * y + 5);
    CheckDoubleEq(ct2.GetCoefficient(x), 1.0, "test3");
    CheckDoubleEq(ct2.GetCoefficient(y), -3.0, "test4");
    CheckDoubleEq(ct2.Lb(), 5.0, "test5");
    CheckDoubleEq(ct2.Ub(), 5.0, "test6");
    Constraint ct3 = solver.Add(2 * x - 9 == y);
    CheckDoubleEq(ct3.GetCoefficient(x), 2.0, "test7");
    CheckDoubleEq(ct3.GetCoefficient(y), -1.0, "test8");
    CheckDoubleEq(ct3.Lb(), 9.0, "test9");
    CheckDoubleEq(ct3.Ub(), 9.0, "test10");
    Check(x == x, "test11");
    Check(!(x == y), "test12");
    Check(!(x != x), "test13");
    Check((x != y), "test14");
  }

  static void TestInequalities()
  {
    Console.WriteLine("Running TestInequalities");
    Solver solver = new Solver("TestInequalities",
                               Solver.CLP_LINEAR_PROGRAMMING);
    Variable x = solver.MakeNumVar(0.0, 100.0, "x");
    Variable y = solver.MakeNumVar(0.0, 100.0, "y");
    Constraint ct1 = solver.Add(2 * (x + 3) + 5 * (y + x -1) >= 3);
    CheckDoubleEq(ct1.GetCoefficient(x), 7.0, "test1");
    CheckDoubleEq(ct1.GetCoefficient(y), 5.0, "test2");
    CheckDoubleEq(ct1.Lb(), 2.0, "test3");
    CheckDoubleEq(ct1.Ub(), double.PositiveInfinity, "test4");
    Constraint ct2 = solver.Add(2 * (x + 3) + 5 * (y + x -1) <= 3);
    CheckDoubleEq(ct2.GetCoefficient(x), 7.0, "test5");
    CheckDoubleEq(ct2.GetCoefficient(y), 5.0, "test6");
    CheckDoubleEq(ct2.Lb(), double.NegativeInfinity, "test7");
    CheckDoubleEq(ct2.Ub(), 2.0, "test8");
    Constraint ct3 = solver.Add(2 * (x + 3) + 5 * (y + x -1) >= 3 - x - y);
    CheckDoubleEq(ct3.GetCoefficient(x), 8.0, "test9");
    CheckDoubleEq(ct3.GetCoefficient(y), 6.0, "test10");
    CheckDoubleEq(ct3.Lb(), 2.0, "test11");
    CheckDoubleEq(ct3.Ub(), double.PositiveInfinity, "test12");
    Constraint ct4 = solver.Add(2 * (x + 3) + 5 * (y + x -1) <= -x - y + 3);
    CheckDoubleEq(ct4.GetCoefficient(x), 8.0, "test13");
    CheckDoubleEq(ct4.GetCoefficient(y), 6.0, "test14");
    CheckDoubleEq(ct4.Lb(), double.NegativeInfinity, "test15");
    CheckDoubleEq(ct4.Ub(), 2.0, "test16");
  }

  static void TestSumArray()
  {
    Console.WriteLine("Running TestSumArray");
    Solver solver = new Solver("TestSumArray", Solver.CLP_LINEAR_PROGRAMMING);
    Variable[] x = solver.MakeBoolVarArray(10, "x");
    Constraint ct1 = solver.Add(x.Sum() == 3);
    CheckDoubleEq(ct1.GetCoefficient(x[0]), 1.0, "test1");
    Constraint ct2 = solver.Add(-2 * x.Sum() == 3);
    CheckDoubleEq(ct2.GetCoefficient(x[0]), -2.0, "test2");
    LinearExpr[] array = new LinearExpr[] { x[0]+ 2.0, x[0] + 3, x[0] + 4 };
    Constraint ct3 = solver.Add(array.Sum() == 1);
    CheckDoubleEq(ct3.GetCoefficient(x[0]), 3.0, "test3");
    CheckDoubleEq(ct3.Lb(), -8.0, "test4");
    CheckDoubleEq(ct3.Ub(), -8.0, "test5");
  }

  static void TestObjective()
  {
    Console.WriteLine("Running TestObjective");
    Solver solver = new Solver("TestObjective", Solver.CLP_LINEAR_PROGRAMMING);
    Variable x = solver.MakeNumVar(0.0, 100.0, "x");
    Variable y = solver.MakeNumVar(0.0, 100.0, "y");
    solver.Maximize(x);
    CheckDoubleEq(0.0, solver.Objective().Offset(), "test1");
    CheckDoubleEq(1.0, solver.Objective().GetCoefficient(x), "test2");
    Check(solver.Objective().Maximization(), "test3");
    solver.Minimize(-x - 2 * y + 3);
    CheckDoubleEq(3.0, solver.Objective().Offset(), "test4");
    CheckDoubleEq(-1.0, solver.Objective().GetCoefficient(x), "test5");
    CheckDoubleEq(-2.0, solver.Objective().GetCoefficient(y), "test6");
    Check(solver.Objective().Minimization(), "test7");
  }

  static void Main()
  {
    TestVarOperator();
    TestVarAddition();
    TestVarMultiplication();
    TestBinaryOperations();
    TestInequalities();
    TestSumArray();
    TestObjective();
    if (error_count != 0) {
      Console.WriteLine("Found " + error_count + " errors.");
      Environment.Exit(1);
    }
  }
}
