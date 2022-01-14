// Copyright 2010-2021 Google LLC
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
using System.Collections.Generic;
using Xunit;
using Google.OrTools.Sat;

namespace Google.OrTools.Tests
{
public class SatSolverTest
{
    private const int LargeCount = 100000;

    static IntegerVariableProto NewIntegerVariable(long lb, long ub)
    {
        IntegerVariableProto var = new IntegerVariableProto();
        var.Domain.Add(lb);
        var.Domain.Add(ub);
        return var;
    }

    static ConstraintProto NewLinear2(int v1, int v2, long c1, long c2, long lb, long ub)
    {
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

    static ConstraintProto NewLinear3(int v1, int v2, int v3, long c1, long c2, long c3, long lb, long ub)
    {
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

    static CpObjectiveProto NewMinimize1(int v1, long c1)
    {
        CpObjectiveProto obj = new CpObjectiveProto();
        obj.Vars.Add(v1);
        obj.Coeffs.Add(c1);
        return obj;
    }

    static CpObjectiveProto NewMaximize1(int v1, long c1)
    {
        CpObjectiveProto obj = new CpObjectiveProto();
        obj.Vars.Add(-v1 - 1);
        obj.Coeffs.Add(c1);
        obj.ScalingFactor = -1;
        return obj;
    }

    static CpObjectiveProto NewMaximize2(int v1, int v2, long c1, long c2)
    {
        CpObjectiveProto obj = new CpObjectiveProto();
        obj.Vars.Add(-v1 - 1);
        obj.Vars.Add(-v2 - 1);
        obj.Coeffs.Add(c1);
        obj.Coeffs.Add(c2);
        obj.ScalingFactor = -1;
        return obj;
    }

    // CpModelProto
    [Fact]
    public void SimpleLinearModelProto()
    {
        CpModelProto model = new CpModelProto();
        model.Variables.Add(NewIntegerVariable(-10, 10));
        model.Variables.Add(NewIntegerVariable(-10, 10));
        model.Variables.Add(NewIntegerVariable(-1000000, 1000000));
        model.Constraints.Add(NewLinear2(0, 1, 1, 1, -1000000, 100000));
        model.Constraints.Add(NewLinear3(0, 1, 2, 1, 2, -1, 0, 100000));
        model.Objective = NewMaximize1(2, 1);
        // Console.WriteLine("model = " + model.ToString());
        SolveWrapper solve_wrapper = new SolveWrapper();
        CpSolverResponse response = solve_wrapper.Solve(model);
        Assert.Equal(CpSolverStatus.Optimal, response.Status);
        Assert.Equal(30, response.ObjectiveValue);
        Assert.Equal(new long[] { 10, 10, 30 }, response.Solution);
        // Console.WriteLine("response = " + response.ToString());
    }

    [Fact]
    public void SimpleLinearModelProto2()
    {
        CpModelProto model = new CpModelProto();
        model.Variables.Add(NewIntegerVariable(-10, 10));
        model.Variables.Add(NewIntegerVariable(-10, 10));
        model.Constraints.Add(NewLinear2(0, 1, 1, 1, -1000000, 100000));
        model.Objective = NewMaximize2(0, 1, 1, -2);
        // Console.WriteLine("model = " + model.ToString());

        SolveWrapper solve_wrapper = new SolveWrapper();
        CpSolverResponse response = solve_wrapper.Solve(model);
        Assert.Equal(CpSolverStatus.Optimal, response.Status);
        Assert.Equal(30, response.ObjectiveValue);
        Assert.Equal(new long[] { 10, -10 }, response.Solution);
        // Console.WriteLine("response = " + response.ToString());
    }

    // CpModel
    [Fact]
    public void SimpleLinearModel()
    {
        CpModel model = new CpModel();
        IntVar v1 = model.NewIntVar(-10, 10, "v1");
        IntVar v2 = model.NewIntVar(-10, 10, "v2");
        IntVar v3 = model.NewIntVar(-100000, 100000, "v3");
        model.AddLinearConstraint(v1 + v2, -1000000, 100000);
        model.AddLinearConstraint(v1 + 2 * v2 - v3, 0, 100000);
        model.Maximize(v3);
        Assert.Equal(v1.Domain.FlattenedIntervals(), new long[] { -10, 10 });
        // Console.WriteLine("model = " + model.Model.ToString());

        CpSolver solver = new CpSolver();
        CpSolverStatus status = solver.Solve(model);
        Assert.Equal(CpSolverStatus.Optimal, status);

        CpSolverResponse response = solver.Response;
        Assert.Equal(30, response.ObjectiveValue);
        Assert.Equal(new long[] { 10, 10, 30 }, response.Solution);
        // Console.WriteLine("response = " + response.ToString());
    }

    [Fact]
    public void SimpleLinearModel2()
    {
        CpModel model = new CpModel();
        IntVar v1 = model.NewIntVar(-10, 10, "v1");
        IntVar v2 = model.NewIntVar(-10, 10, "v2");
        model.AddLinearConstraint(v1 + v2, -1000000, 100000);
        model.Maximize(v1 - 2 * v2);
        // Console.WriteLine("model = " + model.Model.ToString());

        CpSolver solver = new CpSolver();
        CpSolverStatus status = solver.Solve(model);
        Assert.Equal(CpSolverStatus.Optimal, status);

        CpSolverResponse response = solver.Response;
        Assert.Equal(30, response.ObjectiveValue);
        Assert.Equal(new long[] { 10, -10 }, response.Solution);
        // Console.WriteLine("response = " + response.ToString());
    }

    [Fact]
    public void SimpleLinearModel3()
    {
        CpModel model = new CpModel();
        IntVar v1 = model.NewIntVar(-10, 10, "v1");
        IntVar v2 = model.NewIntVar(-10, 10, "v2");
        model.Add(-100000 <= v1 + 2 * v2 <= 100000);
        model.Minimize(v1 - 2 * v2);
        // Console.WriteLine("model = " + model.Model.ToString());

        CpSolver solver = new CpSolver();
        CpSolverStatus status = solver.Solve(model);
        Assert.Equal(CpSolverStatus.Optimal, status);

        CpSolverResponse response = solver.Response;
        Assert.Equal(-10, solver.Value(v1));
        Assert.Equal(10, solver.Value(v2));
        Assert.Equal(new long[] { -10, 10 }, response.Solution);
        Assert.Equal(-30, solver.Value(v1 - 2 * v2));
        Assert.Equal(-30, response.ObjectiveValue);
        // Console.WriteLine("response = " + response.ToString());
    }

    [Fact]
    public void NegativeIntVar()
    {
        CpModel model = new CpModel();
        IntVar boolvar = model.NewBoolVar("boolvar");
        IntVar x = model.NewIntVar(0, 10, "x");
        IntVar delta = model.NewIntVar(-5, 5, "delta");
        IntVar squaredDelta = model.NewIntVar(0, 25, "squaredDelta");
        model.Add(x == boolvar * 4);
        model.Add(delta == x - 5);
        model.AddMultiplicationEquality(squaredDelta, new IntVar[] { delta, delta });
        model.Minimize(squaredDelta);
        // Console.WriteLine("model = " + model.Model.ToString());

        CpSolver solver = new CpSolver();
        CpSolverStatus status = solver.Solve(model);
        CpSolverResponse response = solver.Response;
        Console.WriteLine("response = " + response.ToString());

        Assert.Equal(CpSolverStatus.Optimal, status);

        Assert.Equal(1, solver.Value(boolvar));
        Assert.Equal(4, solver.Value(x));
        Assert.Equal(-1, solver.Value(delta));
        Assert.Equal(1, solver.Value(squaredDelta));
        Assert.Equal(new long[] { 1, 4, -1, 1 }, response.Solution);
        Assert.Equal(1.0, response.ObjectiveValue, 5);
    }

    [Fact]
    public void NegativeSquareVar()
    {
        CpModel model = new CpModel();
        BoolVar boolvar = model.NewBoolVar("boolvar");
        IntVar x = model.NewIntVar(0, 10, "x");
        IntVar delta = model.NewIntVar(-5, 5, "delta");
        IntVar squaredDelta = model.NewIntVar(0, 25, "squaredDelta");
        model.Add(x == 4).OnlyEnforceIf(boolvar);
        model.Add(x == 0).OnlyEnforceIf(boolvar.Not());
        model.Add(delta == x - 5);
        long[,] tuples = { { -5, 25 }, { -4, 16 }, { -3, 9 }, { -2, 4 }, { -1, 1 }, { 0, 0 },
                           { 1, 1 },   { 2, 4 },   { 3, 9 },  { 4, 16 }, { 5, 25 } };
        model.AddAllowedAssignments(new IntVar[] { delta, squaredDelta }).AddTuples(tuples);
        model.Minimize(squaredDelta);

        CpSolver solver = new CpSolver();
        CpSolverStatus status = solver.Solve(model);

        CpSolverResponse response = solver.Response;
        Assert.Equal(1, solver.Value(boolvar));
        Assert.Equal(4, solver.Value(x));
        Assert.Equal(-1, solver.Value(delta));
        Assert.Equal(1, solver.Value(squaredDelta));
        Assert.Equal(new long[] { 1, 4, -1, 1 }, response.Solution);
        Assert.Equal(1.0, response.ObjectiveValue, 6);
    }

    [Fact]
    public void Division()
    {
        CpModel model = new CpModel();
        IntVar v1 = model.NewIntVar(0, 10, "v1");
        IntVar v2 = model.NewIntVar(1, 10, "v2");
        model.AddDivisionEquality(3, v1, v2);
        // Console.WriteLine(model.Model);

        CpSolver solver = new CpSolver();
        CpSolverStatus status = solver.Solve(model);
        Assert.Equal(CpSolverStatus.Optimal, status);

        CpSolverResponse response = solver.Response;
        Assert.Equal(3, solver.Value(v1));
        Assert.Equal(1, solver.Value(v2));
        Assert.Equal(new long[] { 3, 1 }, response.Solution);
        Assert.Equal(0, response.ObjectiveValue);
        // Console.WriteLine("response = " + response.ToString());
    }

    [Fact]
    public void Modulo()
    {
        CpModel model = new CpModel();
        IntVar v1 = model.NewIntVar(1, 10, "v1");
        IntVar v2 = model.NewIntVar(1, 10, "v2");
        model.AddModuloEquality(3, v1, v2);
        // Console.WriteLine(model.Model);

        CpSolver solver = new CpSolver();
        CpSolverStatus status = solver.Solve(model);
        Assert.Equal(CpSolverStatus.Optimal, status);

        CpSolverResponse response = solver.Response;
        Assert.Equal(3, solver.Value(v1));
        Assert.Equal(4, solver.Value(v2));
        Assert.Equal(new long[] { 3, 4 }, response.Solution);
        Assert.Equal(0, response.ObjectiveValue);
        // Console.WriteLine("response = " + response.ToString());
    }

    [Fact]
    public void LargeWeightedSumLong()
    {
        CpModel model = new CpModel();
        model.Model.Variables.Capacity = LargeCount;
        List<IntVar> vars = new List<IntVar>(LargeCount);
        List<long> coeffs = new List<long>(LargeCount);

        for (int i = 0; i < LargeCount; ++i)
        {
            vars.Add(model.NewBoolVar(""));
            coeffs.Add(i + 1);
        }

        var watch = System.Diagnostics.Stopwatch.StartNew();
        model.Minimize(LinearExpr.WeightedSum(vars, coeffs));
        watch.Stop();
        var elapsedMs = watch.ElapsedMilliseconds;
        Console.WriteLine($"Long: Elapsed time {elapsedMs}");
    }

    [Fact]
    public void LargeWeightedSumInt()
    {
        CpModel model = new CpModel();
        model.Model.Variables.Capacity = LargeCount;
        List<IntVar> vars = new List<IntVar>(LargeCount);
        List<int> coeffs = new List<int>(LargeCount);

        for (int i = 0; i < LargeCount; ++i)
        {
            vars.Add(model.NewBoolVar(""));
            coeffs.Add(i);
        }

        var watch = System.Diagnostics.Stopwatch.StartNew();
        model.Minimize(LinearExpr.WeightedSum(vars, coeffs));
        watch.Stop();
        var elapsedMs = watch.ElapsedMilliseconds;
        Console.WriteLine($"Int: Elapsed time {elapsedMs}");
    }

    [Fact]
    public void LargeWeightedSumExpr()
    {
        CpModel model = new CpModel();
        model.Model.Variables.Capacity = LargeCount;
        List<LinearExpr> exprs = new List<LinearExpr>(LargeCount);

        for (int i = 0; i < LargeCount; ++i)
        {
            exprs.Add(model.NewBoolVar("") * i);
        }

        var watch = System.Diagnostics.Stopwatch.StartNew();
        model.Minimize(LinearExpr.Sum(exprs));
        watch.Stop();
        var elapsedMs = watch.ElapsedMilliseconds;
        Console.WriteLine($"Exprs: Elapsed time {elapsedMs}");
    }

    [Fact]
    public void LargeWeightedSumBuilder()
    {
        CpModel model = new CpModel();
        model.Model.Variables.Capacity = LargeCount;
        List<IntVar> vars = new List<IntVar>(LargeCount);
        List<long> coeffs = new List<long>(LargeCount);

        for (int i = 0; i < LargeCount; ++i)
        {
            vars.Add(model.NewBoolVar(""));
            coeffs.Add(i + 1);
        }

        var watch = System.Diagnostics.Stopwatch.StartNew();
        LinearExprBuilder obj = LinearExpr.NewBuilder();
        for (int i = 0; i < LargeCount; ++i)
        {
            obj.AddTerm(vars[i], coeffs[i]);
        }
        model.Minimize(obj);
        watch.Stop();
        var elapsedMs = watch.ElapsedMilliseconds;
        Console.WriteLine($"Proto: Elapsed time {elapsedMs}");
    }

    [Fact]
    public void LinearExprStaticCompileTest()
    {
        Console.WriteLine("LinearExprStaticCompileTest");
        CpModel model = new CpModel();
        IntVar v1 = model.NewIntVar(-10, 10, "v1");
        IntVar v2 = model.NewIntVar(-10, 10, "v2");
        BoolVar b1 = model.NewBoolVar("b1");
        BoolVar b2 = model.NewBoolVar("b2");
        long[] c1 = new long[] { 2L, 4L };
        int[] c2 = new int[] { 2, 4 };
        LinearExpr e1 = LinearExpr.Sum(new IntVar[] { v1, v2 });
        Console.WriteLine(e1.ToString());
        LinearExpr e2 = LinearExpr.Sum(new ILiteral[] { b1, b2 });
        Console.WriteLine(e2.ToString());
        LinearExpr e3 = LinearExpr.Sum(new BoolVar[] { b1, b2 });
        Console.WriteLine(e3.ToString());
        LinearExpr e4 = LinearExpr.WeightedSum(new IntVar[] { v1, v2 }, c1);
        Console.WriteLine(e4.ToString());
        LinearExpr e5 = LinearExpr.WeightedSum(new ILiteral[] { b1, b2 }, c1);
        Console.WriteLine(e5.ToString());
        LinearExpr e6 = LinearExpr.WeightedSum(new BoolVar[] { b1, b2 }, c1);
        Console.WriteLine(e6.ToString());
        LinearExpr e7 = LinearExpr.WeightedSum(new IntVar[] { v1, v2 }, c2);
        Console.WriteLine(e7.ToString());
        LinearExpr e8 = LinearExpr.WeightedSum(new ILiteral[] { b1, b2 }, c2);
        Console.WriteLine(e8.ToString());
        LinearExpr e9 = LinearExpr.WeightedSum(new BoolVar[] { b1, b2 }, c2);
        Console.WriteLine(e9.ToString());
    }

    [Fact]
    public void LinearExprBuilderCompileTest()
    {
        Console.WriteLine("LinearExprBuilderCompileTest");
        CpModel model = new CpModel();
        IntVar v1 = model.NewIntVar(-10, 10, "v1");
        IntVar v2 = model.NewIntVar(-10, 10, "v2");
        BoolVar b1 = model.NewBoolVar("b1");
        BoolVar b2 = model.NewBoolVar("b2");
        long[] c1 = new long[] { 2L, 4L };
        int[] c2 = new int[] { 2, 4 };
        LinearExpr e1 = LinearExpr.NewBuilder().AddSum(new IntVar[] { v1, v2 });
        Console.WriteLine(e1.ToString());
        LinearExpr e2 = LinearExpr.NewBuilder().AddSum(new ILiteral[] { b1, b2 });
        Console.WriteLine(e2.ToString());
        LinearExpr e3 = LinearExpr.NewBuilder().AddSum(new BoolVar[] { b1, b2 });
        Console.WriteLine(e3.ToString());
        LinearExpr e4 = LinearExpr.NewBuilder().AddWeightedSum(new IntVar[] { v1, v2 }, c1);
        Console.WriteLine(e4.ToString());
        LinearExpr e5 = LinearExpr.NewBuilder().AddWeightedSum(new ILiteral[] { b1, b2 }, c1);
        Console.WriteLine(e5.ToString());
        LinearExpr e6 = LinearExpr.NewBuilder().AddWeightedSum(new BoolVar[] { b1, b2 }, c1);
        Console.WriteLine(e6.ToString());
        LinearExpr e7 = LinearExpr.NewBuilder().AddWeightedSum(new IntVar[] { v1, v2 }, c2);
        Console.WriteLine(e7.ToString());
        LinearExpr e8 = LinearExpr.NewBuilder().AddWeightedSum(new ILiteral[] { b1, b2 }, c2);
        Console.WriteLine(e8.ToString());
        LinearExpr e9 = LinearExpr.NewBuilder().AddWeightedSum(new BoolVar[] { b1, b2 }, c2);
        Console.WriteLine(e9.ToString());
        LinearExpr e10 = LinearExpr.NewBuilder().Add(v1);
        Console.WriteLine(e10.ToString());
        LinearExpr e11 = LinearExpr.NewBuilder().Add(b1);
        Console.WriteLine(e11.ToString());
        LinearExpr e12 = LinearExpr.NewBuilder().Add(b1.Not());
        Console.WriteLine(e12.ToString());
        LinearExpr e13 = LinearExpr.NewBuilder().AddTerm(v1, -1);
        Console.WriteLine(e13.ToString());
        LinearExpr e14 = LinearExpr.NewBuilder().AddTerm(b1, -1);
        Console.WriteLine(e14.ToString());
        LinearExpr e15 = LinearExpr.NewBuilder().AddTerm(b1.Not(), -2);
        Console.WriteLine(e15.ToString());
    }

    [Fact]
    public void LinearExprIntVarOperatorTest()
    {
        Console.WriteLine("LinearExprIntVarOperatorTest");
        CpModel model = new CpModel();
        IntVar v = model.NewIntVar(-10, 10, "v");
        LinearExpr e = v * 2;
        Console.WriteLine(e);
        e = 2 * v;
        Console.WriteLine(e);
        e = v + 2;
        Console.WriteLine(e);
        e = 2 + v;
        Console.WriteLine(e);
        e = v;
        Console.WriteLine(e);
        e = -v;
        Console.WriteLine(e);
        e = 1 - v;
        Console.WriteLine(e);
        e = v - 1;
        Console.WriteLine(e);
    }

    [Fact]
    public void LinearExprBoolVarOperatorTest()
    {
        Console.WriteLine("LinearExprBoolVarOperatorTest");
        CpModel model = new CpModel();
        BoolVar v = model.NewBoolVar("v");
        LinearExpr e = v * 2;
        Console.WriteLine(e);
        e = 2 * v;
        Console.WriteLine(e);
        e = v + 2;
        Console.WriteLine(e);
        e = 2 + v;
        Console.WriteLine(e);
        e = v;
        Console.WriteLine(e);
        e = -v;
        Console.WriteLine(e);
        e = 1 - v;
        Console.WriteLine(e);
        e = v - 1;
        Console.WriteLine(e);
    }

    [Fact]
    public void LinearExprNotBoolVarOperatorTest()
    {
        Console.WriteLine("LinearExprBoolVarNotOperatorTest");
        CpModel model = new CpModel();
        ILiteral v = model.NewBoolVar("v");
        LinearExpr e = v.NotAsExpr() * 2;
        Console.WriteLine(e);
        e = 2 * v.NotAsExpr();
        Console.WriteLine(e);
        e = v.NotAsExpr() + 2;
        Console.WriteLine(e);
        e = 2 + v.NotAsExpr();
        Console.WriteLine(e);
        e = v.NotAsExpr();
        Console.WriteLine(e);
        e = -v.NotAsExpr();
        Console.WriteLine(e);
        e = 1 - v.NotAsExpr();
        Console.WriteLine(e);
        e = v.NotAsExpr() - 1;
        Console.WriteLine(e);
    }
    [Fact]
    public void ExportModel()
    {
        CpModel model = new CpModel();
        IntVar v1 = model.NewIntVar(-10, 10, "v1");
        IntVar v2 = model.NewIntVar(-10, 10, "v2");
        model.Add(-100000 <= v1 + 2 * v2 <= 100000);
        model.Minimize(v1 - 2 * v2);
        Assert.True(model.ExportToFile("test_model_dotnet.pbtxt"));
        Console.WriteLine("Model written to file");
    }

    [Fact]
    public void SolveFromString()
    {
        string model_str = @"
            {
              ""variables"": [
                { ""name"": ""C"", ""domain"": [ ""1"", ""9"" ] },
                { ""name"": ""P"", ""domain"": [ ""0"", ""9"" ] },
                { ""name"": ""I"", ""domain"": [ ""1"", ""9"" ] },
                { ""name"": ""S"", ""domain"": [ ""0"", ""9"" ] },
                { ""name"": ""F"", ""domain"": [ ""1"", ""9"" ] },
                { ""name"": ""U"", ""domain"": [ ""0"", ""9"" ] },
                { ""name"": ""N"", ""domain"": [ ""0"", ""9"" ] },
                { ""name"": ""T"", ""domain"": [ ""1"", ""9"" ] },
                { ""name"": ""R"", ""domain"": [ ""0"", ""9"" ] },
                { ""name"": ""E"", ""domain"": [ ""0"", ""9"" ] }
              ],
              ""constraints"": [
                { ""allDiff"": { ""exprs"": [
                    { ""vars"": [""0""], ""coeffs"": [""1""] },
                    { ""vars"": [""1""], ""coeffs"": [""1""] },
                    { ""vars"": [""2""], ""coeffs"": [""1""] },
                    { ""vars"": [""3""], ""coeffs"": [""1""] },
                    { ""vars"": [""4""], ""coeffs"": [""1""] },
                    { ""vars"": [""5""], ""coeffs"": [""1""] },
                    { ""vars"": [""6""], ""coeffs"": [""1""] },
                    { ""vars"": [""7""], ""coeffs"": [""1""] },
                    { ""vars"": [""8""], ""coeffs"": [""1""] },
                    { ""vars"": [""9""], ""coeffs"": [""1""] } ] } },
                { ""linear"": { ""vars"": [ 6, 5, 9, 4, 3, 7, 8, 2, 0, 1 ], ""coeffs"": [ ""1"", ""0"", ""-1"", ""100"", ""1"", ""-1000"", ""-100"", ""10"", ""10"", ""1"" ], ""domain"": [ ""0"", ""0"" ] } }
              ]
            }";
        CpModelProto model = Google.Protobuf.JsonParser.Default.Parse<CpModelProto>(model_str);
        SolveWrapper solve_wrapper = new SolveWrapper();
        CpSolverResponse response = solve_wrapper.Solve(model);
        Console.WriteLine(response);
    }

    [Fact]
    public void CaptureLog()
    {
        Console.WriteLine("CaptureLog test");
        CpModel model = new CpModel();
        IntVar v1 = model.NewIntVar(-10, 10, "v1");
        IntVar v2 = model.NewIntVar(-10, 10, "v2");
        IntVar v3 = model.NewIntVar(-100000, 100000, "v3");
        model.AddLinearConstraint(v1 + v2, -1000000, 100000);
        model.AddLinearConstraint(v1 + 2 * v2 - v3, 0, 100000);
        model.Maximize(v3);
        Assert.Equal(v1.Domain.FlattenedIntervals(), new long[] { -10, 10 });
        // Console.WriteLine("model = " + model.Model.ToString());

        CpSolver solver = new CpSolver();
        solver.StringParameters = "log_search_progress:true log_to_stdout:false";
        string log = "";
        solver.SetLogCallback(message => log += message + "\n");
        solver.Solve(model);
        Assert.NotEmpty(log);
        Assert.Contains("OPTIMAL", log);
    }

    [Fact]
    public void TestInterval()
    {
        Console.WriteLine("TestInterval test");
        CpModel model = new CpModel();
        IntVar v = model.NewIntVar(-10, 10, "v");
        IntervalVar i = model.NewFixedSizeIntervalVar(v, 3, "i");
        Assert.Equal("v", i.StartExpr().ToString());
        Assert.Equal("3", i.SizeExpr().ToString());
        Assert.Equal("v + 3", i.EndExpr().ToString());
    }
}
} // namespace Google.OrTools.Tests
