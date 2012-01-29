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
    CheckEquality(ct2.GetCoefficient(x), 1.0, "test1");
    CheckEquality(ct3.GetCoefficient(x), 1.0, "test1");
    CheckEquality(ct4.GetCoefficient(x), 1.0, "test1");
    CheckEquality(ct5.GetCoefficient(x), 1.0, "test1");
    CheckEquality(ct6.GetCoefficient(x), 1.0, "test1");
  }

  static void Main()
  {
    TestVarOperator();
  }
}