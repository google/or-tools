// Copyright 2010-2021 Google LLC
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

// [START program]
// [START import]
using System;
using Google.OrTools.Graph;
// [END import]

public class AssignmentMinFlow
{
    static void Main()
    {
        // [START solver]
        // Instantiate a SimpleMinCostFlow solver.
        MinCostFlow minCostFlow = new MinCostFlow();
        // [END solver]

        // [START data]
        // Define four parallel arrays: sources, destinations, capacities, and unit costs
        // between each pair.
        int[] startNodes = { 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 6, 7, 8 };
        int[] endNodes = { 1, 2, 3, 4, 5, 6, 7, 8, 5, 6, 7, 8, 5, 6, 7, 8, 5, 6, 7, 8, 9, 9, 9, 9 };
        int[] capacities = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
        int[] unitCosts = { 0,   0,  0,  0,   90, 76,  75, 70,  35, 85, 55, 65,
                            125, 95, 90, 105, 45, 110, 95, 115, 0,  0,  0,  0 };

        int source = 0;
        int sink = 9;
        int tasks = 4;
        // Define an array of supplies at each node.
        int[] supplies = { tasks, 0, 0, 0, 0, 0, 0, 0, 0, -tasks };
        // [END data]

        // [START constraints]
        // Add each arc.
        for (int i = 0; i < startNodes.Length; ++i)
        {
            int arc =
                minCostFlow.AddArcWithCapacityAndUnitCost(startNodes[i], endNodes[i], capacities[i], unitCosts[i]);
            if (arc != i)
                throw new Exception("Internal error");
        }

        // Add node supplies.
        for (int i = 0; i < supplies.Length; ++i)
        {
            minCostFlow.SetNodeSupply(i, supplies[i]);
        }
        // [END constraints]

        // [START solve]
        // Find the min cost flow.
        MinCostFlow.Status status = minCostFlow.Solve();
        // [END solve]

        // [START print_solution]
        if (status == MinCostFlow.Status.OPTIMAL)
        {
            Console.WriteLine("Total cost: " + minCostFlow.OptimalCost());
            Console.WriteLine("");
            for (int i = 0; i < minCostFlow.NumArcs(); ++i)
            {
                // Can ignore arcs leading out of source or into sink.
                if (minCostFlow.Tail(i) != source && minCostFlow.Head(i) != sink)
                {
                    // Arcs in the solution have a flow value of 1. Their start and end nodes
                    // give an assignment of worker to task.
                    if (minCostFlow.Flow(i) > 0)
                    {
                        Console.WriteLine("Worker " + minCostFlow.Tail(i) + " assigned to task " + minCostFlow.Head(i) +
                                          " Cost: " + minCostFlow.UnitCost(i));
                    }
                }
            }
        }
        else
        {
            Console.WriteLine("Solving the min cost flow problem failed.");
            Console.WriteLine("Solver status: " + status);
        }
        // [END print_solution]
    }
}
// [END program]
