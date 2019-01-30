using System;
using Xunit;
using Google.OrTools.Sat;

namespace Google.OrTools.Tests {
  public class SatSolverTest {
    static IntegerVariableProto NewIntegerVariable(long lb, long ub) {
      IntegerVariableProto var = new IntegerVariableProto();
      var.Domain.Add(lb);
      var.Domain.Add(ub);
      return var;
    }

    static ConstraintProto NewLinear2(
        int v1, int v2,
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

    static ConstraintProto NewLinear3(
        int v1, int v2, int v3,
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

    // CpModelProto
    [Fact]
      public void SimpleLinearModelProto() {
        CpModelProto model = new CpModelProto();
        model.Variables.Add(NewIntegerVariable(-10, 10));
        model.Variables.Add(NewIntegerVariable(-10, 10));
        model.Variables.Add(NewIntegerVariable(-1000000, 1000000));
        model.Constraints.Add(NewLinear2(0, 1 , 1, 1, -1000000, 100000));
        model.Constraints.Add(NewLinear3(0, 1, 2, 1, 2, -1, 0, 100000));
        model.Objective = NewMaximize1(2, 1);
        //Console.WriteLine("model = " + model.ToString());

        CpSolverResponse response = SatHelper.Solve(model);
        Assert.Equal(CpSolverStatus.Optimal, response.Status);
        Assert.Equal(30, response.ObjectiveValue);
        Assert.Equal(new long[] {10, 10, 30}, response.Solution);
        //Console.WriteLine("response = " + response.ToString());
      }

    [Fact]
      public void SimpleLinearModelProto2() {
        CpModelProto model = new CpModelProto();
        model.Variables.Add(NewIntegerVariable(-10, 10));
        model.Variables.Add(NewIntegerVariable(-10, 10));
        model.Constraints.Add(NewLinear2(0, 1 , 1, 1, -1000000, 100000));
        model.Objective = NewMaximize2(0, 1, 1, -2);
        //Console.WriteLine("model = " + model.ToString());

        CpSolverResponse response = SatHelper.Solve(model);
        Assert.Equal(CpSolverStatus.Optimal, response.Status);
        Assert.Equal(30, response.ObjectiveValue);
        Assert.Equal(new long[] {10, -10}, response.Solution);
        //Console.WriteLine("response = " + response.ToString());
      }

    // CpModel
    [Fact]
      public void SimpleLinearModel() {
        CpModel model = new CpModel();
        IntVar v1 = model.NewIntVar(-10, 10, "v1");
        IntVar v2 = model.NewIntVar(-10, 10, "v2");
        IntVar v3 = model.NewIntVar(-100000, 100000, "v3");
        model.AddLinearConstraint(new[] {v1, v2}, new[] {1, 1}, -1000000, 100000);
        model.AddLinearConstraint(new[] {v1, v2, v3}, new[] {1, 2, -1}, 0, 100000);
        model.Maximize(v3);
        //Console.WriteLine("model = " + model.Model.ToString());

        CpSolver solver = new CpSolver();
        CpSolverStatus status = solver.Solve(model);
        Assert.Equal(CpSolverStatus.Optimal, status);

        CpSolverResponse response = solver.Response;
        Assert.Equal(30, response.ObjectiveValue);
        Assert.Equal(new long[] {10, 10, 30}, response.Solution);
        //Console.WriteLine("response = " + reponse.ToString());
      }

    [Fact]
      public void SimpleLinearModel2() {
        CpModel model = new CpModel();
        IntVar v1 = model.NewIntVar(-10, 10, "v1");
        IntVar v2 = model.NewIntVar(-10, 10, "v2");
        model.AddLinearConstraint(new[] {v1, v2}, new[] {1, 1}, -1000000, 100000);
        model.Maximize(v1 - 2 * v2);
        //Console.WriteLine("model = " + model.Model.ToString());

        CpSolver solver = new CpSolver();
        CpSolverStatus status = solver.Solve(model);
        Assert.Equal(CpSolverStatus.Optimal, status);

        CpSolverResponse response = solver.Response;
        Assert.Equal(30, response.ObjectiveValue);
        Assert.Equal(new long[] {10, -10}, response.Solution);
        //Console.WriteLine("response = " + reponse.ToString());
      }

    [Fact]
      public void SimpleLinearModel3() {
        CpModel model = new CpModel();
        IntVar v1 = model.NewIntVar(-10, 10, "v1");
        IntVar v2 = model.NewIntVar(-10, 10, "v2");
        model.Add(-100000 <= v1 + 2 * v2 <= 100000);
        model.Minimize(v1 - 2 * v2);
        //Console.WriteLine("model = " + model.Model.ToString());

        CpSolver solver = new CpSolver();
        CpSolverStatus status = solver.Solve(model);
        Assert.Equal(CpSolverStatus.Optimal, status);

        CpSolverResponse response = solver.Response;
        Assert.Equal(-10, solver.Value(v1));
        Assert.Equal(10, solver.Value(v2));
        Assert.Equal(new long[] {-10, 10}, response.Solution);
        Assert.Equal(-30, solver.Value(v1 - 2 * v2));
        Assert.Equal(-30, response.ObjectiveValue);
        //Console.WriteLine("response = " + reponse.ToString());
      }

    [Fact]
      public void NegativeIntVar() {
        CpModel model = new CpModel();
        IntVar boolvar = model.NewBoolVar("boolvar");
        IntVar x = model.NewIntVar(0,10, "x");
        IntVar delta = model.NewIntVar(-5, 5,"delta");
        IntVar squaredDelta = model.NewIntVar(0, 25,"squaredDelta");
        model.Add(x == boolvar * 4);
        model.Add(delta == x - 5 );
        model.AddProdEquality(squaredDelta, new IntVar[] {delta, delta});
        model.Minimize(squaredDelta);
        //Console.WriteLine("model = " + model.Model.ToString());

        CpSolver solver = new CpSolver();
        CpSolverStatus status = solver.Solve(model);
        Assert.Equal(CpSolverStatus.Optimal, status);

        CpSolverResponse response = solver.Response;
        Assert.Equal(1, solver.Value(boolvar));
        Assert.Equal(4, solver.Value(x));
        Assert.Equal(-1, solver.Value(delta));
        Assert.Equal(1, solver.Value(squaredDelta));
        Assert.Equal(new long[] {1, 4, -1, 1}, response.Solution);
        Assert.Equal(1, response.ObjectiveValue);
        //Console.WriteLine("response = " + reponse.ToString());
      }

    [Fact]
      public void NegativeSquareVar() {
        CpModel model = new CpModel();
        IntVar boolvar = model.NewBoolVar("boolvar");
        IntVar x = model.NewIntVar(0,10, "x");
        IntVar delta = model.NewIntVar(-5, 5,"delta");
        IntVar squaredDelta = model.NewIntVar(0, 25,"squaredDelta");
        model.Add(x == 4).OnlyEnforceIf(boolvar);
        model.Add(x == 0).OnlyEnforceIf(boolvar.Not());
        model.Add(delta == x - 5 );
        long[,] tuples = { {-5, 25}, {-4, 16}, {-3, 9}, {-2, 4}, {-1, 1}, {0, 0},
          {1, 1}, {2, 4}, {3, 9}, {4, 16}, {5, 25} };
        model.AddAllowedAssignments(new IntVar[] {delta, squaredDelta}, tuples);
        model.Minimize(squaredDelta);
        //Console.WriteLine("model = " + model.Model.ToString());

        CpSolver solver = new CpSolver();
        CpSolverStatus status = solver.Solve(model);
        Assert.Equal(CpSolverStatus.Optimal, status);

        CpSolverResponse response = solver.Response;
        Assert.Equal(1, solver.Value(boolvar));
        Assert.Equal(4, solver.Value(x));
        Assert.Equal(-1, solver.Value(delta));
        Assert.Equal(1, solver.Value(squaredDelta));
        Assert.Equal(new long[] {1, 4, -1, 1}, response.Solution);
        Assert.Equal(1, response.ObjectiveValue);
        //Console.WriteLine("response = " + reponse.ToString());
      }

    [Fact]
      public void Division() {
        CpModel model = new CpModel();
        IntVar v1 = model.NewIntVar(0, 10, "v1");
        IntVar v2 = model.NewIntVar(1, 10, "v2");
        model.AddDivisionEquality(3, v1, v2);
        //Console.WriteLine(model.Model);

        CpSolver solver = new CpSolver();
        CpSolverStatus status = solver.Solve(model);
        Assert.Equal(CpSolverStatus.Feasible, status);

        CpSolverResponse response = solver.Response;
        Assert.Equal(3, solver.Value(v1));
        Assert.Equal(1, solver.Value(v2));
        Assert.Equal(new long[] {3, 1, 3}, response.Solution);
        Assert.Equal(0, response.ObjectiveValue);
        //Console.WriteLine("response = " + reponse.ToString());
      }

    [Fact]
      public void Modulo() {
        CpModel model = new CpModel();
        IntVar v1 = model.NewIntVar(1, 10, "v1");
        IntVar v2 = model.NewIntVar(1, 10, "v2");
        model.AddModuloEquality(3, v1, v2);
        //Console.WriteLine(model.Model);

        CpSolver solver = new CpSolver();
        CpSolverStatus status = solver.Solve(model);
        Assert.Equal(CpSolverStatus.Feasible, status);

        CpSolverResponse response = solver.Response;
        Assert.Equal(3, solver.Value(v1));
        Assert.Equal(4, solver.Value(v2));
        Assert.Equal(new long[] {3, 4, 3}, response.Solution);
        Assert.Equal(0, response.ObjectiveValue);
        //Console.WriteLine("response = " + reponse.ToString());
      }
  }
} // namespace Google.OrTools.Tests

