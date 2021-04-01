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
using Google.OrTools.ConstraintSolver;
// [END import]

/// <summary>
///   This is a sample using the routing library .Net wrapper.
/// </summary>
public class SimpleRoutingProgram
{
    public static void Main(String[] args)
    {
        // Instantiate the data problem.
        // [START data]
        const int numLocation = 5;
        const int numVehicles = 1;
        const int depot = 0;
        // [END data]

        // Create Routing Index Manager
        // [START index_manager]
        RoutingIndexManager manager = new RoutingIndexManager(numLocation, numVehicles, depot);
        // [END index_manager]

        // Create Routing Model.
        // [START routing_model]
        RoutingModel routing = new RoutingModel(manager);
        // [END routing_model]

        // Create and register a transit callback.
        // [START transit_callback]
        int transitCallbackIndex = routing.RegisterTransitCallback((long fromIndex, long toIndex) => {
            // Convert from routing variable Index to distance matrix NodeIndex.
            var fromNode = manager.IndexToNode(fromIndex);
            var toNode = manager.IndexToNode(toIndex);
            return Math.Abs(toNode - fromNode);
        });
        // [END transit_callback]

        // Define cost of each arc.
        // [START arc_cost]
        routing.SetArcCostEvaluatorOfAllVehicles(transitCallbackIndex);
        // [END arc_cost]

        // Setting first solution heuristic.
        // [START parameters]
        RoutingSearchParameters searchParameters =
            operations_research_constraint_solver.DefaultRoutingSearchParameters();
        searchParameters.FirstSolutionStrategy = FirstSolutionStrategy.Types.Value.PathCheapestArc;
        // [END parameters]

        // Solve the problem.
        // [START solve]
        Assignment solution = routing.SolveWithParameters(searchParameters);
        // [END solve]

        // Print solution on console.
        // [START print_solution]
        Console.WriteLine("Objective: {0}", solution.ObjectiveValue());
        // Inspect solution.
        long index = routing.Start(0);
        Console.WriteLine("Route for Vehicle 0:");
        long route_distance = 0;
        while (routing.IsEnd(index) == false)
        {
            Console.Write("{0} -> ", manager.IndexToNode((int)index));
            long previousIndex = index;
            index = solution.Value(routing.NextVar(index));
            route_distance += routing.GetArcCostForVehicle(previousIndex, index, 0);
        }
        Console.WriteLine("{0}", manager.IndexToNode(index));
        Console.WriteLine("Distance of the route: {0}m", route_distance);
        // [END print_solution]
    }
}
// [END program]
