using System;
using Xunit;

using Google.OrTools.ConstraintSolver;

namespace Google.OrTools.Tests {
  public class ConstraintSolverTest {
    [Theory]
    [InlineData(false)]
    [InlineData(true)]
    public void SolverTest(bool callGC) {
      Solver solver = new Solver("Solver");
      IntVar x = solver.MakeIntVar(3, 7, "x");

      if (callGC) {
        GC.Collect();
      }

      Assert.Equal(3, x.Min());
      Assert.Equal(7, x.Max());
      Assert.Equal("x(3..7)", x.ToString());
    }
  }
} // namespace Google.Sample.Tests
