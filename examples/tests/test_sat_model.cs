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


  static void TestSimpleLinearModel() {
    CpModel model = new CpModel();
    IntVar v1 = model.NewIntVar(-10, 10, "v1");
    IntVar v2 = model.NewIntVar(-10, 10, "v2");
    IntVar v3 = model.NewIntVar(-100000, 100000, "v3");

    model.Maximize(v3);
//    model.Constraints.Add(NewLinear2(0, 1 , 1, 1, -1000000, 100000));
//    model.Constraints.Add(NewLinear3(0, 1, 2, 1, 2, -1, 0, 100000));

    CpSolver solver = new CpSolver();
    CpSolverStatus status = solver.Solve(model);

    Console.WriteLine("model = " + model.Model.ToString());
    Console.WriteLine("response = " + solver.Response.ToString());
  }

  // static void TestSimpleLinearModel2() {
  //   CpModelProto model = new CpModelProto();
  //   model.Variables.Add(NewIntegerVariable(-10, 10));
  //   model.Variables.Add(NewIntegerVariable(-10, 10));
  //   model.Constraints.Add(NewLinear2(0, 1 , 1, 1, -1000000, 100000));
  //   model.Objective = NewMaximize2(0, 1, 1, -2);

  //   CpSolverResponse response = SatHelper.Solve(model);

  //   Console.WriteLine("model = " + model.ToString());
  //   Console.WriteLine("response = " + response.ToString());
  // }

  static void Main() {
    TestSimpleLinearModel();
//    TestSimpleLinearModel2();
    if (error_count_ != 0) {
      Console.WriteLine("Found " + error_count_ + " errors.");
      Environment.Exit(1);
    }
  }
}
