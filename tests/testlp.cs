// Copyright 2010-2012 Google
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
  static void Check(bool test, String message)
  {
    if (!test)
    {
      Console.WriteLine("Error: " + message);
    }
  }

  static void CheckEquality(double v1, double v2, String message)
  {
    if (v1 != v2)
    {
      Console.WriteLine("Error: " + v1 + " != " + v2 + " " + message);
    }
  }

  static void TestVarOperator()
  {
    Console.WriteLine("Running TestVarOperator");
    MPSolver solver = new MPSolver("TestVarOperator",
                                   MPSolver.CLP_LINEAR_PROGRAMMING);
    MPVariable x = solver.MakeNumVar(0.0, 100.0, "x");
    MPConstraint ct1 = solver.Add(x >= 1);
    MPConstraint ct2 = solver.Add(x <= 1);
    MPConstraint ct3 = solver.Add(x == 1);
    MPConstraint ct4 = solver.Add(1 >= x);
    MPConstraint ct5 = solver.Add(1 <= x);
    MPConstraint ct6 = solver.Add(1 == x);
    CheckEquality(ct1.GetCoefficient(x), 1.0, "test1");
    CheckEquality(ct2.GetCoefficient(x), 1.0, "test2");
    CheckEquality(ct3.GetCoefficient(x), 1.0, "test3");
    CheckEquality(ct4.GetCoefficient(x), 1.0, "test4");
    CheckEquality(ct5.GetCoefficient(x), 1.0, "test5");
    CheckEquality(ct6.GetCoefficient(x), 1.0, "test6");
    CheckEquality(ct1.Lb(), 1.0, "test7");
    CheckEquality(ct1.Ub(), double.PositiveInfinity, "test8");
    CheckEquality(ct2.Lb(), double.NegativeInfinity, "test9");
    CheckEquality(ct2.Ub(), 1.0, "test10");
    CheckEquality(ct3.Lb(), 1.0, "test11");
    CheckEquality(ct3.Ub(), 1.0, "test12");
    CheckEquality(ct4.Lb(), double.NegativeInfinity, "test13");
    CheckEquality(ct4.Ub(), 1.0, "test14");
    CheckEquality(ct5.Lb(), 1.0, "test15");
    CheckEquality(ct5.Ub(), double.PositiveInfinity, "test16");
    CheckEquality(ct6.Lb(), 1.0, "test17");
    CheckEquality(ct6.Ub(), 1.0, "test18");
  }

  static void TestVarAddition()
  {
    Console.WriteLine("Running TestVarAddition");
    MPSolver solver = new MPSolver("TestVarAddition",
                                   MPSolver.CLP_LINEAR_PROGRAMMING);
    MPVariable x = solver.MakeNumVar(0.0, 100.0, "x");
    MPVariable y = solver.MakeNumVar(0.0, 100.0, "y");
    MPConstraint ct1 = solver.Add(x + y == 1);
    CheckEquality(ct1.GetCoefficient(x), 1.0, "test1");
    CheckEquality(ct1.GetCoefficient(y), 1.0, "test2");
    MPConstraint ct2 = solver.Add(x + x == 1);
    CheckEquality(ct2.GetCoefficient(x), 2.0, "test3");
    MPConstraint ct3 = solver.Add(x + (y + x) == 1);
    CheckEquality(ct3.GetCoefficient(x), 2.0, "test4");
    CheckEquality(ct3.GetCoefficient(y), 1.0, "test5");
    MPConstraint ct4 = solver.Add(x + (y + x + 3) == 1);
    CheckEquality(ct4.GetCoefficient(x), 2.0, "test4");
    CheckEquality(ct4.GetCoefficient(y), 1.0, "test5");
    CheckEquality(ct4.Lb(), -2.0, "test6");
    CheckEquality(ct4.Ub(), -2.0, "test7");
  }

  static void TestVarMultiplication()
  {
    Console.WriteLine("Running TestVarMultiplication");
    MPSolver solver = new MPSolver("TestVarMultiplication",
                                   MPSolver.CLP_LINEAR_PROGRAMMING);
    MPVariable x = solver.MakeNumVar(0.0, 100.0, "x");
    MPVariable y = solver.MakeNumVar(0.0, 100.0, "y");
    MPConstraint ct1 = solver.Add(3 * x == 1);
    CheckEquality(ct1.GetCoefficient(x), 3.0, "test1");
    MPConstraint ct2 = solver.Add(x * 3 == 1);
    CheckEquality(ct2.GetCoefficient(x), 3.0, "test2");
    MPConstraint ct3 = solver.Add(x + (2 * y + 3 * x) == 1);
    CheckEquality(ct3.GetCoefficient(x), 4.0, "test3");
    CheckEquality(ct3.GetCoefficient(y), 2.0, "test4");
    MPConstraint ct4 = solver.Add(x + 5 * (y + x + 3) == 1);
    CheckEquality(ct4.GetCoefficient(x), 6.0, "test5");
    CheckEquality(ct4.GetCoefficient(y), 5.0, "test6");
    CheckEquality(ct4.Lb(), -14.0, "test7");
    CheckEquality(ct4.Ub(), -14.0, "test8");
    MPConstraint ct5 = solver.Add(x + (2 * y + x + 3) * 3 == 1);
    CheckEquality(ct5.GetCoefficient(x), 4.0, "test9");
    CheckEquality(ct5.GetCoefficient(y), 6.0, "test10");
    CheckEquality(ct5.Lb(), -8.0, "test11");
    CheckEquality(ct5.Ub(), -8.0, "test12");
  }

  static void TestBinaryOperations()
  {
    Console.WriteLine("Running TestBinaryOperations");
    MPSolver solver = new MPSolver("TestBinaryOperations",
                                   MPSolver.CLP_LINEAR_PROGRAMMING);
    MPVariable x = solver.MakeNumVar(0.0, 100.0, "x");
    MPVariable y = solver.MakeNumVar(0.0, 100.0, "y");
    MPConstraint ct1 = solver.Add(x == y);
    CheckEquality(ct1.GetCoefficient(x), 1.0, "test1");
    CheckEquality(ct1.GetCoefficient(y), -1.0, "test2");
    MPConstraint ct2 = solver.Add(x == 3 * y + 5);
    CheckEquality(ct2.GetCoefficient(x), 1.0, "test3");
    CheckEquality(ct2.GetCoefficient(y), -3.0, "test4");
    CheckEquality(ct2.Lb(), 5.0, "test5");
    CheckEquality(ct2.Ub(), 5.0, "test6");
    MPConstraint ct3 = solver.Add(2 * x - 9 == y);
    CheckEquality(ct3.GetCoefficient(x), 2.0, "test7");
    CheckEquality(ct3.GetCoefficient(y), -1.0, "test8");
    CheckEquality(ct3.Lb(), 9.0, "test9");
    CheckEquality(ct3.Ub(), 9.0, "test10");
    Check(x == x, "test11");
    Check(!(x == y), "test12");
    Check(!(x != x), "test13");
    Check((x != y), "test14");

  }

  static void Main()
  {
    TestVarOperator();
    TestVarAddition();
    TestVarMultiplication();
    TestBinaryOperations();
  }
}
