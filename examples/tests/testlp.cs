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
