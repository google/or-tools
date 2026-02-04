// Copyright 2010-2025 Google LLC
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
        IndexManager manager = new IndexManager(31 /*locations*/, 7 /*vehicle*/, 3 /*depot*/);
        Assert.NotNull(manager);
        // Create Routing Model.
        Model routing = new Model(manager);
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
        Dimension dimension = routing.GetDimensionOrDie("Dimension");
    }

    [Fact]
    public void TestSoftSpanUpperBound()
    {
        // Create Routing Index Manager
        IndexManager manager = new IndexManager(31 /*locations*/, 7 /*vehicle*/, 3 /*depot*/);
        Assert.NotNull(manager);
        // Create Routing Model.
        Model routing = new Model(manager);
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
        Dimension dimension = routing.GetDimensionOrDie("Dimension");

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
        IndexManager manager = new IndexManager(31 /*locations*/, 7 /*vehicle*/, 3 /*depot*/);
        Assert.NotNull(manager);
        // Create Routing Model.
        Model routing = new Model(manager);
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
        Dimension dimension = routing.GetDimensionOrDie("Dimension");

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
} // namespace Google.OrTools.Tests
