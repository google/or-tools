using System;
using Xunit;

using Google.OrTools.LinearSolver;

namespace Google.OrTools.Tests {
  public class LinearProgramming {
    [Fact]
      public void TestVarOperator() {
        Solver solver = new Solver("TestVarOperator", Solver.CLP_LINEAR_PROGRAMMING);
        Variable x = solver.MakeNumVar(0.0, 100.0, "x");
        Constraint ct1 = solver.Add(x >= 1);
        Constraint ct2 = solver.Add(x <= 1);
        Constraint ct3 = solver.Add(x == 1);
        Constraint ct4 = solver.Add(1 >= x);
        Constraint ct5 = solver.Add(1 <= x);
        Constraint ct6 = solver.Add(1 == x);

        Assert.Equal(1.0, ct1.GetCoefficient(x));
        Assert.Equal(1.0, ct2.GetCoefficient(x));
        Assert.Equal(1.0, ct3.GetCoefficient(x));
        Assert.Equal(1.0, ct4.GetCoefficient(x));
        Assert.Equal(1.0, ct5.GetCoefficient(x));
        Assert.Equal(1.0, ct6.GetCoefficient(x));
        Assert.Equal(1.0, ct1.Lb());
        Assert.Equal(ct1.Ub(), double.PositiveInfinity);
        Assert.Equal(ct2.Lb(), double.NegativeInfinity);
        Assert.Equal(1.0, ct2.Ub());
        Assert.Equal(1.0, ct3.Lb());
        Assert.Equal(1.0, ct3.Ub());
        Assert.Equal(ct4.Lb(), double.NegativeInfinity);
        Assert.Equal(1.0, ct4.Ub());
        Assert.Equal(1.0, ct5.Lb());
        Assert.Equal(ct5.Ub(), double.PositiveInfinity);
        Assert.Equal(1.0, ct6.Lb());
        Assert.Equal(1.0, ct6.Ub());
      }

    [Fact]
      public void TestVarAddition() {
        Solver solver = new Solver("TestVarAddition", Solver.CLP_LINEAR_PROGRAMMING);
        Variable x = solver.MakeNumVar(0.0, 100.0, "x");
        Variable y = solver.MakeNumVar(0.0, 100.0, "y");

        Constraint ct1 = solver.Add(x + y == 1);
        Assert.Equal(1.0, ct1.GetCoefficient(x));
        Assert.Equal(1.0, ct1.GetCoefficient(y));

        Constraint ct2 = solver.Add(x + x == 1);
        Assert.Equal(2.0, ct2.GetCoefficient(x));

        Constraint ct3 = solver.Add(x + (y + x) == 1);
        Assert.Equal(2.0, ct3.GetCoefficient(x));
        Assert.Equal(1.0, ct3.GetCoefficient(y));

        Constraint ct4 = solver.Add(x + (y + x + 3) == 1);
        Assert.Equal(2.0, ct4.GetCoefficient(x));
        Assert.Equal(1.0, ct4.GetCoefficient(y));
        Assert.Equal(-2.0, ct4.Lb());
        Assert.Equal(-2.0, ct4.Ub());
      }

    [Fact]
      public void TestVarMultiplication() {
        Solver solver = new Solver("TestVarMultiplication", Solver.CLP_LINEAR_PROGRAMMING);
        Variable x = solver.MakeNumVar(0.0, 100.0, "x");
        Variable y = solver.MakeNumVar(0.0, 100.0, "y");

        Constraint ct1 = solver.Add(3 * x == 1);
        Assert.Equal(3.0, ct1.GetCoefficient(x));

        Constraint ct2 = solver.Add(x * 3 == 1);
        Assert.Equal(3.0, ct2.GetCoefficient(x));

        Constraint ct3 = solver.Add(x + (2 * y + 3 * x) == 1);
        Assert.Equal(4.0, ct3.GetCoefficient(x));
        Assert.Equal(2.0, ct3.GetCoefficient(y));

        Constraint ct4 = solver.Add(x + 5 * (y + x + 3) == 1);
        Assert.Equal(6.0, ct4.GetCoefficient(x));
        Assert.Equal(5.0, ct4.GetCoefficient(y));
        Assert.Equal(-14.0, ct4.Lb());
        Assert.Equal(-14.0, ct4.Ub());

        Constraint ct5 = solver.Add(x + (2 * y + x + 3) * 3 == 1);
        Assert.Equal(4.0, ct5.GetCoefficient(x));
        Assert.Equal(6.0, ct5.GetCoefficient(y));
        Assert.Equal(-8.0, ct5.Lb());
        Assert.Equal(-8.0, ct5.Ub());
      }

    [Fact]
      public void TestBinaryOperations() {
        Solver solver = new Solver("TestBinaryOperations", Solver.CLP_LINEAR_PROGRAMMING);
        Variable x = solver.MakeNumVar(0.0, 100.0, "x");
        Variable y = solver.MakeNumVar(0.0, 100.0, "y");

        Constraint ct1 = solver.Add(x == y);
        Assert.Equal(1.0, ct1.GetCoefficient(x));
        Assert.Equal(-1.0, ct1.GetCoefficient(y));

        Constraint ct2 = solver.Add(x == 3 * y + 5);
        Assert.Equal(1.0, ct2.GetCoefficient(x));
        Assert.Equal(-3.0, ct2.GetCoefficient(y));
        Assert.Equal(5.0, ct2.Lb());
        Assert.Equal(5.0, ct2.Ub());

        Constraint ct3 = solver.Add(2 * x - 9 == y);
        Assert.Equal(2.0, ct3.GetCoefficient(x));
        Assert.Equal(-1.0, ct3.GetCoefficient(y));
        Assert.Equal(9.0, ct3.Lb());
        Assert.Equal(9.0, ct3.Ub());

        Assert.True(x == x, "test11");
        Assert.True(!(x == y), "test12");
        Assert.True(!(x != x), "test13");
        Assert.True((x != y), "test14");
      }

    [Fact]
      public void TestInequalities() {
        Solver solver = new Solver("TestInequalities", Solver.CLP_LINEAR_PROGRAMMING);
        Variable x = solver.MakeNumVar(0.0, 100.0, "x");
        Variable y = solver.MakeNumVar(0.0, 100.0, "y");

        Constraint ct1 = solver.Add(2 * (x + 3) + 5 * (y + x - 1) >= 3);
        Assert.Equal(7.0, ct1.GetCoefficient(x));
        Assert.Equal(5.0, ct1.GetCoefficient(y));
        Assert.Equal(2.0, ct1.Lb());
        Assert.Equal(double.PositiveInfinity, ct1.Ub());

        Constraint ct2 = solver.Add(2 * (x + 3) + 5 * (y + x - 1) <= 3);
        Assert.Equal(7.0, ct2.GetCoefficient(x));
        Assert.Equal(5.0, ct2.GetCoefficient(y));
        Assert.Equal(double.NegativeInfinity, ct2.Lb());
        Assert.Equal(2.0, ct2.Ub());

        Constraint ct3 = solver.Add(2 * (x + 3) + 5 * (y + x - 1) >= 3 - x - y);
        Assert.Equal(8.0, ct3.GetCoefficient(x));
        Assert.Equal(6.0, ct3.GetCoefficient(y));
        Assert.Equal(2.0, ct3.Lb());
        Assert.Equal(double.PositiveInfinity, ct3.Ub());

        Constraint ct4 = solver.Add(2 * (x + 3) + 5 * (y + x - 1) <= -x - y + 3);
        Assert.Equal(8.0, ct4.GetCoefficient(x));
        Assert.Equal(6.0, ct4.GetCoefficient(y));
        Assert.Equal(double.NegativeInfinity, ct4.Lb());
        Assert.Equal(2.0, ct4.Ub());
      }

    [Fact]
      public void TestSumArray() {
        Solver solver = new Solver("TestSumArray", Solver.CLP_LINEAR_PROGRAMMING);

        Variable[] x = solver.MakeBoolVarArray(10, "x");
        Constraint ct1 = solver.Add(x.Sum() == 3);
        Assert.Equal(1.0, ct1.GetCoefficient(x[0]));

        Constraint ct2 = solver.Add(-2 * x.Sum() == 3);
        Assert.Equal(-2.0, ct2.GetCoefficient(x[0]));

        LinearExpr[] array = new LinearExpr[] { x[0] + 2.0, x[0] + 3, x[0] + 4 };
        Constraint ct3 = solver.Add(array.Sum() == 1);
        Assert.Equal(3.0, ct3.GetCoefficient(x[0]));
        Assert.Equal(-8.0, ct3.Lb());
        Assert.Equal(-8.0, ct3.Ub());
      }

    [Fact]
      public void TestObjective() {

        Solver solver = new Solver("TestObjective", Solver.CLP_LINEAR_PROGRAMMING);

        Variable x = solver.MakeNumVar(0.0, 100.0, "x");
        Variable y = solver.MakeNumVar(0.0, 100.0, "y");

        solver.Maximize(x);
        Assert.Equal(0.0, solver.Objective().Offset());
        Assert.Equal(1.0, solver.Objective().GetCoefficient(x));
        Assert.True(solver.Objective().Maximization());

        solver.Minimize(-x - 2 * y + 3);
        Assert.Equal(3.0, solver.Objective().Offset());
        Assert.Equal(-1.0, solver.Objective().GetCoefficient(x));
        Assert.Equal(-2.0, solver.Objective().GetCoefficient(y));
        Assert.True(solver.Objective().Minimization());
      }
  }
}
