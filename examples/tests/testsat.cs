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

  static IntegerVariableProto NewIntegerVariable(long lb, long ub) {
    IntegerVariableProto var = new IntegerVariableProto();
    var.Domain.Add(lb);
    var.Domain.Add(ub);
    return var;
  }

  static ConstraintProto NewLinear2(int v1, int v2,
                                    long c1, long c2,
                                    long lb, long ub) {
    LinearConstraintProto linear = new LinearConstraintProto();
    linear.Vars.Add(v1);
    linear.Vars.Add(v2);
    linear.Coeffs.Add(c1);
    linear.Coeffs.Add(c2);
    linear.Domain.Add(lb);
    linear.Domain.Add(ub);
    ConstraintProto ct = new ConstraintProto();
    ct.Linear = linear;
    return ct;
  }

  static ConstraintProto NewLinear3(int v1, int v2, int v3,
                                    long c1, long c2, long c3,
                                    long lb, long ub) {
    LinearConstraintProto linear = new LinearConstraintProto();
    linear.Vars.Add(v1);
    linear.Vars.Add(v2);
    linear.Vars.Add(v3);
    linear.Coeffs.Add(c1);
    linear.Coeffs.Add(c2);
    linear.Coeffs.Add(c3);
    linear.Domain.Add(lb);
    linear.Domain.Add(ub);
    ConstraintProto ct = new ConstraintProto();
    ct.Linear = linear;
    return ct;
  }

  static CpObjectiveProto NewMinimize1(int v1, long c1) {
    CpObjectiveProto obj = new CpObjectiveProto();
    obj.Vars.Add(v1);
    obj.Coeffs.Add(c1);
    return obj;
  }

  static CpObjectiveProto NewMaximize1(int v1, long c1) {
    CpObjectiveProto obj = new CpObjectiveProto();
    obj.Vars.Add(-v1 - 1);
    obj.Coeffs.Add(c1);
    obj.ScalingFactor = -1;
    return obj;
  }

  static CpObjectiveProto NewMaximize2(int v1, int v2, long c1, long c2) {
    CpObjectiveProto obj = new CpObjectiveProto();
    obj.Vars.Add(-v1 - 1);
    obj.Vars.Add(-v2 - 1);
    obj.Coeffs.Add(c1);
    obj.Coeffs.Add(c2);
    obj.ScalingFactor = -1;
    return obj;
  }


  static void TestSimpleLinearModel() {
    CpModelProto model = new CpModelProto();
    model.Variables.Add(NewIntegerVariable(-10, 10));
    model.Variables.Add(NewIntegerVariable(-10, 10));
    model.Variables.Add(NewIntegerVariable(-1000000, 1000000));
    model.Constraints.Add(NewLinear2(0, 1 , 1, 1, -1000000, 100000));
    model.Constraints.Add(NewLinear3(0, 1, 2, 1, 2, -1, 0, 100000));
    model.Objective = NewMaximize1(2, 1);

    CpSolverResponse response = SatHelper.Solve(model);

    Console.WriteLine("model = " + model.ToString());
    Console.WriteLine("response = " + response.ToString());
  }

  static void TestSimpleLinearModel2() {
    CpModelProto model = new CpModelProto();
    model.Variables.Add(NewIntegerVariable(-10, 10));
    model.Variables.Add(NewIntegerVariable(-10, 10));
    model.Constraints.Add(NewLinear2(0, 1 , 1, 1, -1000000, 100000));
    model.Objective = NewMaximize2(0, 1, 1, -2);

    CpSolverResponse response = SatHelper.Solve(model);

    Console.WriteLine("model = " + model.ToString());
    Console.WriteLine("response = " + response.ToString());
  }

  static void Main() {
    TestSimpleLinearModel();
    TestSimpleLinearModel2();
    if (error_count_ != 0) {
      Console.WriteLine("Found " + error_count_ + " errors.");
      Environment.Exit(1);
    }
  }
}
