using System;
using Xunit;

using Google.OrTools.ConstraintSolver;

namespace Google.OrTools.Tests {
  public class RoutingSolverTest {
    [Theory]
    [InlineData(false)]
    [InlineData(true)]
    public void SolverTest(bool callGC) {
      // Create Routing Index Manager
      RoutingIndexManager manager = new RoutingIndexManager(
          5/*locations*/, 1/*vehicle*/, 0/*depot*/);
      // Create Routing Model.
      RoutingModel routing = new RoutingModel(manager);
      // Create a distance callback.
      int transitCallbackIndex = routing.RegisterTransitCallback(
          (long fromIndex, long toIndex) => {
          // Convert from routing variable Index to distance matrix NodeIndex.
          var fromNode = manager.IndexToNode(fromIndex);
          var toNode = manager.IndexToNode(toIndex);
          return Math.Abs(toNode - fromNode);
          });
      // Define cost of each arc.
      routing.SetArcCostEvaluatorOfAllVehicles(transitCallbackIndex);
      if (callGC) {
        GC.Collect();
      }
      // Setting first solution heuristic.
      RoutingSearchParameters searchParameters =
        operations_research_constraint_solver.DefaultRoutingSearchParameters();
      searchParameters.FirstSolutionStrategy =
        FirstSolutionStrategy.Types.Value.PathCheapestArc;
      Assignment solution = routing.SolveWithParameters(searchParameters);
      // 0 --(+1)-> 1 --(+1)-> 2 --(+1)-> 3 --(+1)-> 4 --(+4)-> 0 := +8
      Assert.Equal(8, solution.ObjectiveValue());
    }
  }
} // namespace Google.Sample.Tests
