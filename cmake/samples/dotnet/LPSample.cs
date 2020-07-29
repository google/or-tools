using System;
using Xunit;

using Google.OrTools.LinearSolver;

namespace Google.OrTools.Tests {
  public class LinearSolverTest {
    [Theory]
    [InlineData(false)]
    [InlineData(true)]
    public void SolverTest(bool callGC) {
      Solver solver = new Solver(
        "Solver",
        Solver.OptimizationProblemType.CLP_LINEAR_PROGRAMMING);

      if (callGC) {
        GC.Collect();
      }
    }
  }
} // namespace Google.Sample.Tests
