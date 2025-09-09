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

// [START program]
// [START import]
using System;
using System.Collections.Generic;
using Google.OrTools.ConstraintSolver;
using Google.OrTools.Routing;
using Google.Protobuf.WellKnownTypes; // Duration
// [END import]

/// <summary>
///   Minimal Vehicle Routing Problem (VRP).
///   The search stop after improving the solution 15 times or after 5 seconds.
/// </summary>
public class VrpSolutionCallback
{
    // [START data_model]
    class DataModel
    {
        public long[,] DistanceMatrix = {
            { 0, 548, 776, 696, 582, 274, 502, 194, 308, 194, 536, 502, 388, 354, 468, 776, 662 },
            { 548, 0, 684, 308, 194, 502, 730, 354, 696, 742, 1084, 594, 480, 674, 1016, 868, 1210 },
            { 776, 684, 0, 992, 878, 502, 274, 810, 468, 742, 400, 1278, 1164, 1130, 788, 1552, 754 },
            { 696, 308, 992, 0, 114, 650, 878, 502, 844, 890, 1232, 514, 628, 822, 1164, 560, 1358 },
            { 582, 194, 878, 114, 0, 536, 764, 388, 730, 776, 1118, 400, 514, 708, 1050, 674, 1244 },
            { 274, 502, 502, 650, 536, 0, 228, 308, 194, 240, 582, 776, 662, 628, 514, 1050, 708 },
            { 502, 730, 274, 878, 764, 228, 0, 536, 194, 468, 354, 1004, 890, 856, 514, 1278, 480 },
            { 194, 354, 810, 502, 388, 308, 536, 0, 342, 388, 730, 468, 354, 320, 662, 742, 856 },
            { 308, 696, 468, 844, 730, 194, 194, 342, 0, 274, 388, 810, 696, 662, 320, 1084, 514 },
            { 194, 742, 742, 890, 776, 240, 468, 388, 274, 0, 342, 536, 422, 388, 274, 810, 468 },
            { 536, 1084, 400, 1232, 1118, 582, 354, 730, 388, 342, 0, 878, 764, 730, 388, 1152, 354 },
            { 502, 594, 1278, 514, 400, 776, 1004, 468, 810, 536, 878, 0, 114, 308, 650, 274, 844 },
            { 388, 480, 1164, 628, 514, 662, 890, 354, 696, 422, 764, 114, 0, 194, 536, 388, 730 },
            { 354, 674, 1130, 822, 708, 628, 856, 320, 662, 388, 730, 308, 194, 0, 342, 422, 536 },
            { 468, 1016, 788, 1164, 1050, 514, 514, 662, 320, 274, 388, 650, 536, 342, 0, 764, 194 },
            { 776, 868, 1552, 560, 674, 1050, 1278, 742, 1084, 810, 1152, 274, 388, 422, 764, 0, 798 },
            { 662, 1210, 754, 1358, 1244, 708, 480, 856, 514, 468, 354, 844, 730, 536, 194, 798, 0 }
        };
        public int VehicleNumber = 4;
        public int Depot = 0;
    };
    // [END data_model]

    // [START solution_callback_printer]
    /// <summary>
    ///   Print the solution.
    /// </summary>
    static void printSolution(ref RoutingIndexManager routingManager, ref RoutingModel routingModel)
    {
        Console.WriteLine($"Solution objective: {routingModel.CostVar().Value()}:");
        // Inspect solution.
        long totalDistance = 0;
        for (int i = 0; i < routingManager.GetNumberOfVehicles(); ++i)
        {
            Console.WriteLine("################");
            Console.WriteLine($"Route for Vehicle {i}:");
            long routeDistance = 0;
            long index = routingModel.Start(i);
            if (routingModel.IsEnd(routingModel.NextVar(index).Value()))
            {
                continue;
            }
            while (routingModel.IsEnd(index) == false)
            {
                Console.Write($" {routingManager.IndexToNode(index)} ->");
                long previousIndex = index;
                index = routingModel.NextVar(index).Value();
                routeDistance += routingModel.GetArcCostForVehicle(previousIndex, index, 0);
            }
            Console.WriteLine($" {routingManager.IndexToNode(index)}");
            Console.WriteLine($"Distance of the route: {routeDistance}m");
            totalDistance += routeDistance;
        }
        Console.WriteLine($"Total Distance of all routes: {totalDistance}m");
    }
    // [END solution_callback_printer]

    // [START solution_callback]
    class SolutionCallback
    {
        public long[] objectives;
        private RoutingIndexManager routingManager;
        private RoutingModel routingModel;
        private long maxSolution;
        private long counter;

        public SolutionCallback(ref RoutingIndexManager manager, ref RoutingModel routing, in long limit)
        {
            routingManager = manager;
            routingModel = routing;
            ;
            maxSolution = limit;
            counter = 0;
            objectives = new long[maxSolution];
        }

        public void Run()
        {
            long objective = routingModel.CostVar().Value();
            if (counter == 0 || objective < objectives[counter - 1])
            {
                objectives[counter] = objective;
                printSolution(ref routingManager, ref routingModel);
                counter++;
            }
            if (counter >= maxSolution)
            {
                routingModel.solver().FinishCurrentSearch();
            }
        }
    };
    // [END solution_callback]

    public static void Main(String[] args)
    {
        // Instantiate the data problem.
        // [START data]
        DataModel data = new DataModel();
        // [END data]

        // Create Routing Index Manager
        // [START index_manager]
        RoutingIndexManager routingManager =
            new RoutingIndexManager(data.DistanceMatrix.GetLength(0), data.VehicleNumber, data.Depot);
        // [END index_manager]

        // Create Routing Model.
        // [START routing_model]
        RoutingModel routingModel = new RoutingModel(routingManager);
        // [END routing_model]

        // Create and register a transit callback.
        // [START transit_callback]
        int transitCallbackIndex =
            routingModel.RegisterTransitCallback((long fromIndex, long toIndex) =>
                                                 {
                                                     // Convert from routing variable Index to
                                                     // distance matrix NodeIndex.
                                                     var fromNode = routingManager.IndexToNode(fromIndex);
                                                     var toNode = routingManager.IndexToNode(toIndex);
                                                     return data.DistanceMatrix[fromNode, toNode];
                                                 });
        // [END transit_callback]

        // Define cost of each arc.
        // [START arc_cost]
        routingModel.SetArcCostEvaluatorOfAllVehicles(transitCallbackIndex);
        // [END arc_cost]

        // Add Distance constraint.
        // [START distance_constraint]
        routingModel.AddDimension(transitCallbackIndex,
                                  0,    // no slack
                                  3000, // vehicle maximum travel distance
                                  true, // start cumul to zero
                                  "Distance");
        RoutingDimension distanceDimension = routingModel.GetMutableDimension("Distance");
        distanceDimension.SetGlobalSpanCostCoefficient(100);
        // [END distance_constraint]

        // Attach a solution callback.
        // [START attach_callback]
        SolutionCallback solutionCallback =
            new SolutionCallback(ref routingManager, ref routingModel, /*max_solution=*/15);
        routingModel.AddAtSolutionCallback(solutionCallback.Run);
        // [END attach_callback]

        // Setting first solution heuristic.
        // [START parameters]
        RoutingSearchParameters searchParameters = RoutingGlobals.DefaultRoutingSearchParameters();
        searchParameters.FirstSolutionStrategy = FirstSolutionStrategy.Types.Value.PathCheapestArc;
        searchParameters.LocalSearchMetaheuristic = LocalSearchMetaheuristic.Types.Value.GuidedLocalSearch;
        searchParameters.TimeLimit = new Duration { Seconds = 5 };
        // [END parameters]

        // Solve the problem.
        // [START solve]
        Assignment solution = routingModel.SolveWithParameters(searchParameters);
        // [END solve]

        // Print solution on console.
        // [START print_solution]
        if (solution != null)
        {
            Console.WriteLine($"Best objective: {solutionCallback.objectives[^1]}");
        }
        else
        {
            Console.WriteLine("No solution found !");
        }
        // [END print_solution]
    }
}
// [END program]
