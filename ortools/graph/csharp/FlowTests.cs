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
using Xunit;
using Google.OrTools.Graph;

namespace Google.OrTools.Tests
{
public class FlowTest
{
    [Fact]
    public void testMinCostFlow()
    {
        int numSources = 4;
        int numTargets = 4;
        int[,] costs = { { 90, 75, 75, 80 }, { 35, 85, 55, 65 }, { 125, 95, 90, 105 }, { 45, 110, 95, 115 } };
        int expectedCost = 275;
        MinCostFlow minCostFlow = new MinCostFlow();
        Assert.NotNull(minCostFlow);
        for (int source = 0; source < numSources; ++source)
        {
            for (int target = 0; target < numTargets; ++target)
            {
                minCostFlow.AddArcWithCapacityAndUnitCost(source, numSources + target, 1, costs[source, target]);
            }
        }

        for (int source = 0; source < numSources; ++source)
        {
            minCostFlow.SetNodeSupply(source, 1);
        }
        for (int target = 0; target < numTargets; ++target)
        {
            minCostFlow.SetNodeSupply(numSources + target, -1);
        }
        MinCostFlowBase.Status solveStatus = minCostFlow.Solve();
        Assert.Equal(solveStatus, MinCostFlow.Status.OPTIMAL);
        long totalFlowCost = minCostFlow.OptimalCost();
        Assert.Equal(expectedCost, totalFlowCost);
    }

    [Fact]
    public void testMaxFlow()
    {
        int numNodes = 6;
        int numArcs = 9;
        int[] tails = { 0, 0, 0, 0, 1, 2, 3, 3, 4 };
        int[] heads = { 1, 2, 3, 4, 3, 4, 4, 5, 5 };
        int[] capacities = { 5, 8, 2, 0, 4, 5, 6, 0, 4 };
        int[] expectedFlows = { 4, 4, 2, 0, 4, 4, 0, 6, 4 };
        int expectedTotalFlow = 10;
        MaxFlow maxFlow = new MaxFlow();
        Assert.NotNull(maxFlow);
        for (int i = 0; i < numArcs; ++i)
        {
            maxFlow.AddArcWithCapacity(tails[i], heads[i], capacities[i]);
        }
        maxFlow.SetArcCapacity(7, 6);
        MaxFlow.Status solveStatus = maxFlow.Solve(/*source=*/0, /*sink=*/numNodes - 1);
        Assert.Equal(solveStatus, MaxFlow.Status.OPTIMAL);

        long totalFlow = maxFlow.OptimalFlow();
        Assert.Equal(expectedTotalFlow, totalFlow);

        Assert.Equal(numArcs, maxFlow.NumArcs());
        for (int i = 0; i < numArcs; ++i)
        {
            Assert.Equal(maxFlow.Flow(i), expectedFlows[i]);
        }
    }
}
} // namespace Google.OrTools.Tests
