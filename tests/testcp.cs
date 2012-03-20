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
using Google.OrTools.ConstraintSolver;

public class CsTestCpOperator
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

  static void TestConstructors()
  {
    Solver solver = new Solver("test");
    IntVar x = solver.MakeIntVar(0, 10, "x");
    Constraint c1 = x == 2;
    Console.WriteLine(c1.ToString());
    Constraint c2 = x >= 2;
    Console.WriteLine(c2.ToString());
    Constraint c3 = x > 2;
    Console.WriteLine(c3.ToString());
    Constraint c4 = x <= 2;
    Console.WriteLine(c4.ToString());
    Constraint c5 = x < 2;
    Console.WriteLine(c5.ToString());
    Constraint c6 = x != 2;
    Console.WriteLine(c6.ToString());
  }

  static void TestOperatorWithExpr()
  {
    Solver solver = new Solver("test");
    IntVar x = solver.MakeIntVar(0, 10, "x");
    IntVar y = solver.MakeIntVar(0, 10, "y");
    Constraint c1 = x == 2;
    IntExpr e2 = c1 + 1;
    Console.WriteLine(e2.ToString());
    IntExpr e3 = c1.Var() + y;
    Console.WriteLine(e3.ToString());
    IntExpr e4 = (x == 3) + 1;
    Console.WriteLine(e4.ToString());
    Constraint c5 = (x == 3) <= (y == 2);
    Console.WriteLine(c5.ToString());
  }

  static void Main()
  {
    TestConstructors();
    TestOperatorWithExpr();
  }
}
