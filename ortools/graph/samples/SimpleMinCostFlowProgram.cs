// Copyright 2010-2018 Google LLC
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

// """From Bradley, Hax, and Magnanti, 'Applied Mathematical Programming', figure 8.1."""
// [START program]
using System;
using Google.OrTools.Graph;

public class SimpleMinCostFlowProgram
{
  static void Main()
  {
    // [START data]
    // Define four parallel arrays: sources, destinations, capacities, and unit costs
    // between each pair. For instance, the arc from node 0 to node 1 has a
    // capacity of 15.
    // Problem taken From Taha's 'Introduction to Operations Research',
    // example 6.4-2.
    int[] startNodes = {0, 0, 1, 1, 1, 2, 2, 3, 4};
    int[] endNodes = {1, 2, 2, 3, 4, 3, 4, 4, 2};
    int[] capacities = {15, 8, 20, 4, 10, 15, 4, 20, 5};
    int[] unitCosts = {4, 4, 2, 2, 6, 1, 3, 2, 3};

    // Define an array of supplies at each node.
    int[] supplies = {20, 0, 0, -5, -15};
    // [END data]

    // [START constraints]
    // Instantiate a SimpleMinCostFlow solver.
    MinCostFlow minCostFlow = new MinCostFlow();

    // Add each arc.
    for (int i = 0; i < startNodes.Length; ++i)
    {
      int arc = minCostFlow.AddArcWithCapacityAndUnitCost(startNodes[i], endNodes[i],
                                           capacities[i], unitCosts[i]);
      if (arc != i) throw new Exception("Internal error");
    }

     // Add node supplies.
    for (int i = 0; i < supplies.Length; ++i)
    {
      minCostFlow.SetNodeSupply(i, supplies[i]);
    }

    // [END constraints]

    // [START solve]
    // Find the min cost flow.
    MinCostFlow.Status solveStatus = minCostFlow.Solve();
    // [END solve]

    // [START print_solution]
    if (solveStatus == MinCostFlow.Status.OPTIMAL)
    {
      Console.WriteLine("Minimum cost: " + minCostFlow.OptimalCost());
      Console.WriteLine("");
      Console.WriteLine(" Edge   Flow / Capacity  Cost");
      for (int i = 0; i < minCostFlow.NumArcs(); ++i)
      {
        long cost = minCostFlow.Flow(i) * minCostFlow.UnitCost(i);
        Console.WriteLine(minCostFlow.Tail(i) + " -> " +
                          minCostFlow.Head(i) + "  " +
                          string.Format("{0,3}",  minCostFlow.Flow(i)) + "  / " +
                          string.Format ("{0,3}", minCostFlow.Capacity(i)) + "       " +
                          string.Format ("{0,3}", cost));
      }
    }
    else
    {
      Console.WriteLine("Solving the min cost flow problem failed. Solver status: " +
                        solveStatus);
    }
    // [END print_solution]
  }
}
// [END program]
