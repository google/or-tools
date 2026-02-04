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
public class RoutingIndexManagerTest
{
    [Fact]
    public void TestSingleDepotManager()
    {
        int numNodes = 10;
        int numVehicles = 5;
        int depotIndex = 1;
        IndexManager manager = new IndexManager(numNodes, numVehicles, depotIndex);
        Assert.Equal(numNodes, manager.GetNumberOfNodes());
        Assert.Equal(numVehicles, manager.GetNumberOfVehicles());
        Assert.Equal(numNodes + 2 * numVehicles - 1, manager.GetNumberOfIndices());
        Assert.Equal(1, manager.GetNumberOfUniqueDepots());

        long[] expectedStarts = { 1, 10, 11, 12, 13 };
        long[] expectedEnds = { 14, 15, 16, 17, 18 };
        for (int i = 0; i < numVehicles; i++)
        {
            Assert.Equal(expectedStarts[i], manager.GetStartIndex(i));
            Assert.Equal(expectedEnds[i], manager.GetEndIndex(i));
        }

        for (int i = 0; i < manager.GetNumberOfIndices(); i++)
        {
            if (i >= numNodes) // start or end nodes
            {
                Assert.Equal(depotIndex, manager.IndexToNode(i));
            }
            else
            {
                Assert.Equal(i, manager.IndexToNode(i));
            }
        }

        long[] inputIndices = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18 };
        int[] expectedNodes = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
        Assert.Equal(expectedNodes, manager.IndicesToNodes(inputIndices));

        for (int i = 0; i < manager.GetNumberOfNodes(); i++)
        {
            Assert.Equal(i, manager.NodeToIndex(i));
        }

        int[] inputNodes = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
        long[] expectedIndicesFromNodes = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
        Assert.Equal(expectedIndicesFromNodes, manager.NodesToIndices(inputNodes));
    }

    [Fact]
    public void TestMultipleDepotManager()
    {
        int numNodes = 10;
        int numVehicles = 5;
        int[] starts = { 0, 3, 9, 2, 2 };
        int[] ends = { 0, 9, 3, 2, 1 };
        IndexManager manager = new IndexManager(numNodes, numVehicles, starts, ends);
        Assert.Equal(numNodes, manager.GetNumberOfNodes());
        Assert.Equal(numVehicles, manager.GetNumberOfVehicles());
        Assert.Equal(numNodes + 2 * numVehicles - 5, manager.GetNumberOfIndices());
        Assert.Equal(5, manager.GetNumberOfUniqueDepots());

        long[] expectedStarts = { 0, 2, 8, 1, 9 };
        long[] expectedEnds = { 10, 11, 12, 13, 14 };
        for (int i = 0; i < numVehicles; i++)
        {
            Assert.Equal(expectedStarts[i], manager.GetStartIndex(i));
            Assert.Equal(expectedEnds[i], manager.GetEndIndex(i));
        }

        int[] expectedNodeIndices = { 0, 2, 3, 4, 5, 6, 7, 8, 9, 2, 0, 9, 3, 2, 1 };
        for (int i = 0; i < manager.GetNumberOfIndices(); i++)
        {
            Assert.Equal(expectedNodeIndices[i], manager.IndexToNode(i));
        }

        long[] allIndices = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14 };
        Assert.Equal(expectedNodeIndices, manager.IndicesToNodes(allIndices));

        long unassigned = IndexManager.kUnassigned;
        long[] expectedIndices = { 0, unassigned, 1, 2, 3, 4, 5, 6, 7, 8 };
        for (int i = 0; i < manager.GetNumberOfNodes(); i++)
        {
            Assert.Equal(expectedIndices[i], manager.NodeToIndex(i));
        }

        int[] inputNodes = { 0, 2, 3, 4, 5, 6, 7, 8, 9 };
        long[] expectedIndicesFromNodes = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };
        Assert.Equal(expectedIndicesFromNodes, manager.NodesToIndices(inputNodes));
    }
}
} // namespace Google.OrTools.Tests
