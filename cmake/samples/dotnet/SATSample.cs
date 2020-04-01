using System;
using Xunit;

using Google.OrTools.Sat;

namespace Google.OrTools.Tests {
  public class SatSolverTest {
    [Theory]
    [InlineData(false)]
    [InlineData(true)]
    public void SolverTest(bool callGC) {
      CpModel model = new CpModel();

      int num_vals = 3;
      IntVar x = model.NewIntVar(0, num_vals - 1, "x");
      IntVar y = model.NewIntVar(0, num_vals - 1, "y");
      IntVar z = model.NewIntVar(0, num_vals - 1, "z");

      model.Add(x != y);

      CpSolver solver = new CpSolver();
      if (callGC) {
        GC.Collect();
      }
      CpSolverStatus status = solver.Solve(model);
    }
  }
} // namespace Google.Sample.Tests
