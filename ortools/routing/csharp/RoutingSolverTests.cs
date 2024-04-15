// Copyright 2010-2024 Google LLC
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
using System.Linq;
using Xunit;
using Google.OrTools.ConstraintSolver;
using Google.OrTools.Routing;

namespace Google.OrTools.Tests
{
public class RoutingSolverTest
{
    [Theory]
    [InlineData(false)]
    [InlineData(true)]
    public void SimpleLambdaCallback(bool callGC)
    {
        // Create Routing Index Manager
        RoutingIndexManager manager = new RoutingIndexManager(5 /*locations*/, 1 /*vehicle*/, 0 /*depot*/);
        Assert.NotNull(manager);
        // Create Routing Model.
        RoutingModel routing = new RoutingModel(manager);
        Assert.NotNull(routing);
        // Create a distance callback.
        int transitCallbackIndex = routing.RegisterTransitCallback((long fromIndex, long toIndex) =>
                                                                   {
                                                                       // Convert from routing variable Index to
                                                                       // distance matrix NodeIndex.
                                                                       var fromNode = manager.IndexToNode(fromIndex);
                                                                       var toNode = manager.IndexToNode(toIndex);
                                                                       return Math.Abs(toNode - fromNode);
                                                                   });
        // Define cost of each arc.
        routing.SetArcCostEvaluatorOfAllVehicles(transitCallbackIndex);
        if (callGC)
        {
            GC.Collect();
        }
        // Setting first solution heuristic.
        RoutingSearchParameters searchParameters = RoutingGlobals.DefaultRoutingSearchParameters();
        searchParameters.FirstSolutionStrategy = FirstSolutionStrategy.Types.Value.PathCheapestArc;
        Assignment solution = routing.SolveWithParameters(searchParameters);
        // 0 --(+1)-> 1 --(+1)-> 2 --(+1)-> 3 --(+1)-> 4 --(+4)-> 0 := +8
        Assert.Equal(8, solution.ObjectiveValue());
    }

    [Fact]
    public void TestTransitMatrix()
    {
        // Create Routing Index Manager
        RoutingIndexManager manager = new RoutingIndexManager(5 /*locations*/, 1 /*vehicle*/, 0 /*depot*/);
        Assert.NotNull(manager);
        // Create Routing Model.
        RoutingModel routing = new RoutingModel(manager);
        Assert.NotNull(routing);
        // Create a distance callback.
        long[][] matrix = new long[][] {
            new long[] { 1, 1, 1, 1, 1 }, new long[] { 1, 1, 1, 1, 1 }, new long[] { 1, 1, 1, 1, 1 },
            new long[] { 1, 1, 1, 1, 1 }, new long[] { 1, 1, 1, 1, 1 },
        };
        int transitCallbackIndex = routing.RegisterTransitMatrix(matrix);
        // Define cost of each arc.
        routing.SetArcCostEvaluatorOfAllVehicles(transitCallbackIndex);
        // Setting first solution heuristic.
        RoutingSearchParameters searchParameters = RoutingGlobals.DefaultRoutingSearchParameters();
        searchParameters.FirstSolutionStrategy = FirstSolutionStrategy.Types.Value.PathCheapestArc;
        Assignment solution = routing.SolveWithParameters(searchParameters);
        // 0 --(+1)-> 1 --(+1)-> 2 --(+1)-> 3 --(+1)-> 4 --(+1)-> 0 := +5
        Assert.Equal(5, solution.ObjectiveValue());
    }

    [Fact]
    public void TestTransitCallback()
    {
        // Create Routing Index Manager
        RoutingIndexManager manager = new RoutingIndexManager(5 /*locations*/, 1 /*vehicle*/, 0 /*depot*/);
        Assert.NotNull(manager);
        // Create Routing Model.
        RoutingModel routing = new RoutingModel(manager);
        Assert.NotNull(routing);
        // Create a distance callback.
        int transitCallbackIndex = routing.RegisterTransitCallback((long fromIndex, long toIndex) =>
                                                                   {
                                                                       // Convert from routing variable Index to
                                                                       // distance matrix NodeIndex.
                                                                       var fromNode = manager.IndexToNode(fromIndex);
                                                                       var toNode = manager.IndexToNode(toIndex);
                                                                       return Math.Abs(toNode - fromNode);
                                                                   });
        // Define cost of each arc.
        routing.SetArcCostEvaluatorOfAllVehicles(transitCallbackIndex);
        // Setting first solution heuristic.
        RoutingSearchParameters searchParameters = RoutingGlobals.DefaultRoutingSearchParameters();
        searchParameters.FirstSolutionStrategy = FirstSolutionStrategy.Types.Value.PathCheapestArc;
        Assignment solution = routing.SolveWithParameters(searchParameters);
        Assert.Equal(8, solution.ObjectiveValue());
    }

    [Fact]
    public void TestMatrixDimension()
    {
        // Create Routing Index Manager
        RoutingIndexManager manager = new RoutingIndexManager(5 /*locations*/, 1 /*vehicle*/, 0 /*depot*/);
        Assert.NotNull(manager);
        // Create Routing Model.
        RoutingModel routing = new RoutingModel(manager);
        Assert.NotNull(routing);
        // Create a distance callback.
        long[][] matrix = new long[][] {
            new long[] { 1, 1, 1, 1, 1 }, new long[] { 1, 1, 1, 1, 1 }, new long[] { 1, 1, 1, 1, 1 },
            new long[] { 1, 1, 1, 1, 1 }, new long[] { 1, 1, 1, 1, 1 },
        };
        IntBoolPair result = routing.AddMatrixDimension(matrix,
                                                        /*capacity=*/10,
                                                        /*fix_start_cumul_to_zero=*/true, "Dimension");
        // Define cost of each arc.
        routing.SetArcCostEvaluatorOfAllVehicles(result.first);
        // Setting first solution heuristic.
        RoutingSearchParameters searchParameters = RoutingGlobals.DefaultRoutingSearchParameters();
        searchParameters.FirstSolutionStrategy = FirstSolutionStrategy.Types.Value.PathCheapestArc;
        Assignment solution = routing.SolveWithParameters(searchParameters);
        // 0 --(+1)-> 1 --(+1)-> 2 --(+1)-> 3 --(+1)-> 4 --(+1)-> 0 := +5
        Assert.Equal(5, solution.ObjectiveValue());
    }

    [Fact]
    public void TestUnaryTransitVector()
    {
        // Create Routing Index Manager
        RoutingIndexManager manager = new RoutingIndexManager(5 /*locations*/, 1 /*vehicle*/, 0 /*depot*/);
        Assert.NotNull(manager);
        // Create Routing Model.
        RoutingModel routing = new RoutingModel(manager);
        Assert.NotNull(routing);
        // Create a distance callback.
        long[] vector = { 1, 1, 1, 1, 1 };
        int transitCallbackIndex = routing.RegisterUnaryTransitVector(vector);
        // Define cost of each arc.
        routing.SetArcCostEvaluatorOfAllVehicles(transitCallbackIndex);
        // Setting first solution heuristic.
        RoutingSearchParameters searchParameters = RoutingGlobals.DefaultRoutingSearchParameters();
        searchParameters.FirstSolutionStrategy = FirstSolutionStrategy.Types.Value.PathCheapestArc;
        Assignment solution = routing.SolveWithParameters(searchParameters);
        // 0 --(+1)-> 1 --(+1)-> 2 --(+1)-> 3 --(+1)-> 4 --(+1)-> 0 := +5
        Assert.Equal(5, solution.ObjectiveValue());
    }

    [Fact]
    public void TestUnaryTransitCallback()
    {
        // Create Routing Index Manager
        RoutingIndexManager manager = new RoutingIndexManager(5 /*locations*/, 1 /*vehicle*/, 0 /*depot*/);
        Assert.NotNull(manager);
        // Create Routing Model.
        RoutingModel routing = new RoutingModel(manager);
        Assert.NotNull(routing);
        // Create a distance callback.
        int transitCallbackIndex = routing.RegisterUnaryTransitCallback((long fromIndex) =>
                                                                        {
                                                                            // Convert from routing variable Index to
                                                                            // distance matrix NodeIndex.
                                                                            var fromNode =
                                                                                manager.IndexToNode(fromIndex);
                                                                            return fromNode + 1;
                                                                        });
        // Define cost of each arc.
        routing.SetArcCostEvaluatorOfAllVehicles(transitCallbackIndex);
        // Setting first solution heuristic.
        RoutingSearchParameters searchParameters = RoutingGlobals.DefaultRoutingSearchParameters();
        searchParameters.FirstSolutionStrategy = FirstSolutionStrategy.Types.Value.PathCheapestArc;
        Assignment solution = routing.SolveWithParameters(searchParameters);
        // 0 --(+1)-> 1 --(+2)-> 2 --(+3)-> 3 --(+4)-> 4 --(+5)-> 0 := +15
        Assert.Equal(15, solution.ObjectiveValue());
    }

    [Fact]
    public void TestVectorDimension()
    {
        // Create Routing Index Manager
        RoutingIndexManager manager = new RoutingIndexManager(5 /*locations*/, 1 /*vehicle*/, 0 /*depot*/);
        Assert.NotNull(manager);
        // Create Routing Model.
        RoutingModel routing = new RoutingModel(manager);
        Assert.NotNull(routing);
        // Create a distance callback.
        long[] vector = new long[] { 1, 1, 1, 1, 1 };
        IntBoolPair result = routing.AddVectorDimension(vector,
                                                        /*capacity=*/10,
                                                        /*fix_start_cumul_to_zero=*/true, "Dimension");
        // Define cost of each arc.
        routing.SetArcCostEvaluatorOfAllVehicles(result.first);
        // Setting first solution heuristic.
        RoutingSearchParameters searchParameters = RoutingGlobals.DefaultRoutingSearchParameters();
        searchParameters.FirstSolutionStrategy = FirstSolutionStrategy.Types.Value.PathCheapestArc;
        Assignment solution = routing.SolveWithParameters(searchParameters);
        // 0 --(+1)-> 1 --(+1)-> 2 --(+1)-> 3 --(+1)-> 4 --(+1)-> 0 := +5
        Assert.Equal(5, solution.ObjectiveValue());
    }
}

public class BoundCostTest
{
    [Fact]
    public void TestCtor()
    {
        // Create Routing Index Manager
        BoundCost boundCost = new BoundCost();
        Assert.NotNull(boundCost);
        Assert.Equal(0, boundCost.bound);
        Assert.Equal(0, boundCost.cost);

        boundCost = new BoundCost(97 /*bound*/, 101 /*cost*/);
        Assert.NotNull(boundCost);
        Assert.Equal(97, boundCost.bound);
        Assert.Equal(101, boundCost.cost);
    }
}

public class RoutingDimensionTest
{
    [Fact]
    public void TestCtor()
    {
        // Create Routing Index Manager
        RoutingIndexManager manager = new RoutingIndexManager(31 /*locations*/, 7 /*vehicle*/, 3 /*depot*/);
        Assert.NotNull(manager);
        // Create Routing Model.
        RoutingModel routing = new RoutingModel(manager);
        Assert.NotNull(routing);
        // Create a distance callback.
        int transitIndex = routing.RegisterTransitCallback((long fromIndex, long toIndex) =>
                                                           {
                                                               // Convert from routing variable Index to
                                                               // distance matrix NodeIndex.
                                                               var fromNode = manager.IndexToNode(fromIndex);
                                                               var toNode = manager.IndexToNode(toIndex);
                                                               return Math.Abs(toNode - fromNode);
                                                           });
        Assert.True(routing.AddDimension(transitIndex, 100, 100, true, "Dimension"));
        RoutingDimension dimension = routing.GetDimensionOrDie("Dimension");
    }

    [Fact]
    public void TestSoftSpanUpperBound()
    {
        // Create Routing Index Manager
        RoutingIndexManager manager = new RoutingIndexManager(31 /*locations*/, 7 /*vehicle*/, 3 /*depot*/);
        Assert.NotNull(manager);
        // Create Routing Model.
        RoutingModel routing = new RoutingModel(manager);
        Assert.NotNull(routing);
        // Create a distance callback.
        int transitIndex = routing.RegisterTransitCallback((long fromIndex, long toIndex) =>
                                                           {
                                                               // Convert from routing variable Index to
                                                               // distance matrix NodeIndex.
                                                               var fromNode = manager.IndexToNode(fromIndex);
                                                               var toNode = manager.IndexToNode(toIndex);
                                                               return Math.Abs(toNode - fromNode);
                                                           });
        Assert.True(routing.AddDimension(transitIndex, 100, 100, true, "Dimension"));
        RoutingDimension dimension = routing.GetDimensionOrDie("Dimension");

        BoundCost boundCost = new BoundCost(/*bound=*/97, /*cost=*/43);
        Assert.NotNull(boundCost);
        Assert.False(dimension.HasSoftSpanUpperBounds());
        foreach (int v in Enumerable.Range(0, manager.GetNumberOfVehicles()).ToArray())
        {
            dimension.SetSoftSpanUpperBoundForVehicle(boundCost, v);
            BoundCost bc = dimension.GetSoftSpanUpperBoundForVehicle(v);
            Assert.NotNull(bc);
            Assert.Equal(97, bc.bound);
            Assert.Equal(43, bc.cost);
        }
        Assert.True(dimension.HasSoftSpanUpperBounds());
    }

    [Fact]
    public void TestQuadraticCostSoftSpanUpperBound()
    {
        // Create Routing Index Manager
        RoutingIndexManager manager = new RoutingIndexManager(31 /*locations*/, 7 /*vehicle*/, 3 /*depot*/);
        Assert.NotNull(manager);
        // Create Routing Model.
        RoutingModel routing = new RoutingModel(manager);
        Assert.NotNull(routing);
        // Create a distance callback.
        int transitIndex = routing.RegisterTransitCallback((long fromIndex, long toIndex) =>
                                                           {
                                                               // Convert from routing variable Index to
                                                               // distance matrix NodeIndex.
                                                               var fromNode = manager.IndexToNode(fromIndex);
                                                               var toNode = manager.IndexToNode(toIndex);
                                                               return Math.Abs(toNode - fromNode);
                                                           });
        Assert.True(routing.AddDimension(transitIndex, 100, 100, true, "Dimension"));
        RoutingDimension dimension = routing.GetDimensionOrDie("Dimension");

        BoundCost boundCost = new BoundCost(/*bound=*/97, /*cost=*/43);
        Assert.NotNull(boundCost);
        Assert.False(dimension.HasQuadraticCostSoftSpanUpperBounds());
        foreach (int v in Enumerable.Range(0, manager.GetNumberOfVehicles()).ToArray())
        {
            dimension.SetQuadraticCostSoftSpanUpperBoundForVehicle(boundCost, v);
            BoundCost bc = dimension.GetQuadraticCostSoftSpanUpperBoundForVehicle(v);
            Assert.NotNull(bc);
            Assert.Equal(97, bc.bound);
            Assert.Equal(43, bc.cost);
        }
        Assert.True(dimension.HasQuadraticCostSoftSpanUpperBounds());
    }
}

// TODO(user): Add tests for Routing[Cost|Vehicle|Resource]ClassIndex

} // namespace Google.OrTools.Tests
