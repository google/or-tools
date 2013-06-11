// Copyright 2010-2013 Google
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
    StarGraph graph = new StarGraph(numSources + numTargets,
                                    numSources * numTargets);
    MinCostFlow  minCostFlow = new MinCostFlow(graph);
    for (int source = 0; source < numSources; ++source)
    {
      for (int target = 0; target < numTargets; ++target) {
        int arc = graph.AddArc(source, numSources + target);
        minCostFlow.SetArcUnitCost(arc, costs[source, target]);
        minCostFlow.SetArcCapacity(arc, 1);
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
    if (minCostFlow.Solve())
    {
      long totalFlowCost = minCostFlow.GetOptimalCost();
      Console.WriteLine("total computed flow cost = " + totalFlowCost +
                        ", expected = " + expectedCost);
    }
    else
    {
      Console.WriteLine("No solution found");
    }
  }

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
    StarGraph graph = new StarGraph(numNodes, numArcs);
    MaxFlow maxFlow = new MaxFlow(graph, 0, numNodes - 1);
    for (int i = 0; i < numArcs; ++i)
    {
      int arc = graph.AddArc(tails[i], heads[i]);
      maxFlow.SetArcCapacity(arc, capacities[i]);
    }
    Console.WriteLine("Solving max flow with " + numNodes + " nodes, and " +
                      numArcs + " arcs, source = 0, sink = " + (numNodes - 1));
    if (maxFlow.Solve())
    {
      long totalFlow = maxFlow.GetOptimalFlow();
      Console.WriteLine("total computed flow " + totalFlow +
                        ", expected = " + expectedTotalFlow);
      for (int i = 0; i < numArcs; ++i)
      {
        Console.WriteLine("Arc " + i + " (" + heads[i] + " -> " + tails[i] +
                          ", capacity = " + capacities[i] + ") computed = " +
                          maxFlow.Flow(i) + ", expected = " + expectedFlows[i]);
      }
    }
    else
    {
      Console.WriteLine("No solution found");
    }
  }

  static void Main()
  {
    SolveMinCostFlow();
    SolveMaxFlow();
  }
}
