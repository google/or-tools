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
using System.Collections.Generic;
using Google.OrTools.ConstraintSolver;
using Google.Protobuf.WellKnownTypes; // Duration
// [END import]

/// <summary>
///   Minimal Vrp using a time limit.
/// </summary>
public class Vrp
{
    // [START solution_printer]
    /// <summary>
    ///   Print the solution.
    /// </summary>
    static void PrintSolution(in RoutingIndexManager manager, in RoutingModel routing, in Assignment solution)
    {
        Console.WriteLine($"Objective {solution.ObjectiveValue()}:");

        // Inspect solution.
        long maxRouteDistance = 0;
        for (int i = 0; i < manager.GetNumberOfVehicles(); ++i)
        {
            Console.WriteLine("Route for Vehicle {0}:", i);
            long routeDistance = 0;
            var index = routing.Start(i);
            while (routing.IsEnd(index) == false)
            {
                Console.Write("{0} -> ", manager.IndexToNode((int)index));
                var previousIndex = index;
                index = solution.Value(routing.NextVar(index));
                routeDistance += routing.GetArcCostForVehicle(previousIndex, index, 0);
            }
            Console.WriteLine("{0}", manager.IndexToNode((int)index));
            Console.WriteLine("Distance of the route: {0}m", routeDistance);
            maxRouteDistance = Math.Max(routeDistance, maxRouteDistance);
        }
        Console.WriteLine("Maximum distance of the routes: {0}m", maxRouteDistance);
    }
    // [END solution_printer]

    public static void Main(String[] args)
    {
        // Instantiate the data problem.
        // [START data]
        int locationNumber = 20;
        int vehicleNumber = 5;
        int depot = 0;
        // [END data]

        // Create Routing Index Manager
        // [START index_manager]
        RoutingIndexManager manager = new RoutingIndexManager(locationNumber, vehicleNumber, depot);

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
            return 1;
        });
        // [END transit_callback]

        // Define cost of each arc.
        // [START arc_cost]
        routing.SetArcCostEvaluatorOfAllVehicles(transitCallbackIndex);
        // [END arc_cost]

        // Add Distance constraint.
        // [START distance_constraint]
        routing.AddDimension(transitCallbackIndex,
                             /*slack=*/0,
                             /*horizon=*/3000,
                             /*start_cumul_to_zero=*/true, "Distance");
        RoutingDimension distanceDimension = routing.GetMutableDimension("Distance");
        distanceDimension.SetGlobalSpanCostCoefficient(100);
        // [END distance_constraint]

        // Setting first solution heuristic.
        // [START parameters]
        RoutingSearchParameters searchParameters =
            operations_research_constraint_solver.DefaultRoutingSearchParameters();
        searchParameters.FirstSolutionStrategy = FirstSolutionStrategy.Types.Value.PathCheapestArc;
        searchParameters.LocalSearchMetaheuristic = LocalSearchMetaheuristic.Types.Value.GuidedLocalSearch;
        searchParameters.LogSearch = true;
        searchParameters.TimeLimit = new Duration { Seconds = 5 };
        // [END parameters]

        // Solve the problem.
        // [START solve]
        Assignment solution = routing.SolveWithParameters(searchParameters);
        // [END solve]

        // Print solution on console.
        // [START print_solution]
        PrintSolution(manager, routing, solution);
        // [END print_solution]
    }
}
// [END program]
