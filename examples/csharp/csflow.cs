// Copyright 2010-2017 Google
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
using Google.OrTools.Graph;

public class CsFlow
{
  private static void SolveMaxFlow()
  {
    Console.WriteLine("Max Flow Problem");
    int numNodes = 6;
    int numArcs = 9;
    int[] tails = {0, 0, 0, 0, 1, 2, 3, 3, 4};
    int[] heads = {1, 2, 3, 4, 3, 4, 4, 5, 5};
    int[] capacities = {5, 8, 5, 3, 4, 5, 6, 6, 4};
    int[] expectedFlows = {4, 4, 2, 0, 4, 4, 0, 6, 4};
    int expectedTotalFlow = 10;
    MaxFlow maxFlow = new MaxFlow();
    for (int i = 0; i < numArcs; ++i)
    {
      int arc = maxFlow.AddArcWithCapacity(tails[i], heads[i], capacities[i]);
      if (arc != i) throw new Exception("Internal error");
    }
    int source = 0;
    int sink = numNodes - 1;
    Console.WriteLine("Solving max flow with " + numNodes + " nodes, and " +
                      numArcs + " arcs, source=" + source + ", sink=" + sink);
    int solveStatus = maxFlow.Solve(source, sink);
    if (solveStatus == MaxFlow.OPTIMAL)
    {
      long totalFlow = maxFlow.OptimalFlow();
      Console.WriteLine("total computed flow " + totalFlow +
                        ", expected = " + expectedTotalFlow);
      for (int i = 0; i < numArcs; ++i)
      {
        Console.WriteLine("Arc " + i + " (" + maxFlow.Head(i) + " -> " +
                          maxFlow.Tail(i) + "), capacity = " +
                          maxFlow.Capacity(i) + ") computed = " +
                          maxFlow.Flow(i) + ", expected = " + expectedFlows[i]);
      }
    }
    else
    {
      Console.WriteLine("Solving the max flow problem failed. Solver status: " +
                        solveStatus);
    }
  }

  private static void SolveMinCostFlow()
  {
    Console.WriteLine("Min Cost Flow Problem");
    int numSources = 4;
    int numTargets = 4;
    int[,] costs = { {90, 75, 75, 80},
                     {35, 85, 55, 65},
                     {125, 95, 90, 105},
                     {45, 110, 95, 115} };
    int expectedCost = 275;
    MinCostFlow minCostFlow = new MinCostFlow();
    for (int source = 0; source < numSources; ++source)
    {
      for (int target = 0; target < numTargets; ++target) {
        minCostFlow.AddArcWithCapacityAndUnitCost(
            source, /*target=*/numSources + target, /*capacity=*/1,
            /*flow unit cost=*/costs[source, target]);
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
    Console.WriteLine("Solving min cost flow with " + numSources +
                      " sources, and " + numTargets + " targets.");
    int solveStatus = minCostFlow.Solve();
    if (solveStatus == MinCostFlow.OPTIMAL)
    {
      Console.WriteLine("total computed flow cost = " +
                        minCostFlow.OptimalCost() +
                        ", expected = " + expectedCost);
    }
    else
    {
      Console.WriteLine("Solving the min cost flow problem failed." +
                        " Solver status: " + solveStatus);
    }
  }

  static void Main()
  {
    SolveMaxFlow();
    SolveMinCostFlow();
  }
}
