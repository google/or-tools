// Copyright 2010-2022 Google LLC
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
///   Minimal Vrp with drop nodes.
/// </summary>
public class VrpDropNodes
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
        // [START demands_capacities]
        public long[] Demands = { 0, 1, 1, 3, 6, 3, 6, 8, 8, 1, 2, 1, 2, 6, 6, 8, 8 };
        public long[] VehicleCapacities = { 15, 15, 15, 15 };
        // [END demands_capacities]
        public int VehicleNumber = 4;
        public int Depot = 0;
    };
    // [END data_model]

    // [START solution_printer]
    /// <summary>
    ///   Print the solution.
    /// </summary>
    static void PrintSolution(in DataModel data, in RoutingModel routing, in RoutingIndexManager manager,
                              in Assignment solution)
    {
        Console.WriteLine($"Objective {solution.ObjectiveValue()}:");

        // Inspect solution.
        // Display dropped nodes.
        string droppedNodes = "Dropped nodes:";
        for (int index = 0; index < routing.Size(); ++index)
        {
            if (routing.IsStart(index) || routing.IsEnd(index))
            {
                continue;
            }
            if (solution.Value(routing.NextVar(index)) == index)
            {
                droppedNodes += " " + manager.IndexToNode(index);
            }
        }
        Console.WriteLine("{0}", droppedNodes);
        // Inspect solution.
        long totalDistance = 0;
        long totalLoad = 0;
        for (int i = 0; i < data.VehicleNumber; ++i)
        {
            Console.WriteLine("Route for Vehicle {0}:", i);
            long routeDistance = 0;
            long routeLoad = 0;
            var index = routing.Start(i);
            while (routing.IsEnd(index) == false)
            {
                long nodeIndex = manager.IndexToNode(index);
                routeLoad += data.Demands[nodeIndex];
                Console.Write("{0} Load({1}) -> ", nodeIndex, routeLoad);
                var previousIndex = index;
                index = solution.Value(routing.NextVar(index));
                routeDistance += routing.GetArcCostForVehicle(previousIndex, index, 0);
            }
            Console.WriteLine("{0}", manager.IndexToNode((int)index));
            Console.WriteLine("Distance of the route: {0}m", routeDistance);
            totalDistance += routeDistance;
            totalLoad += routeLoad;
        }
        Console.WriteLine("Total Distance of all routes: {0}m", totalDistance);
        Console.WriteLine("Total Load of all routes: {0}m", totalLoad);
    }
    // [END solution_printer]

    public static void Main(String[] args)
    {
        // Instantiate the data problem.
        // [START data]
        DataModel data = new DataModel();
        // [END data]

        // Create Routing Index Manager
        // [START index_manager]
        RoutingIndexManager manager =
            new RoutingIndexManager(data.DistanceMatrix.GetLength(0), data.VehicleNumber, data.Depot);
        // [END index_manager]

        // Create Routing Model.
        // [START routing_model]
        RoutingModel routing = new RoutingModel(manager);
        // [END routing_model]

        // Create and register a transit callback.
        // [START transit_callback]
        int transitCallbackIndex = routing.RegisterTransitCallback((long fromIndex, long toIndex) =>
                                                                   {
                                                                       // Convert from routing variable Index to
                                                                       // distance matrix NodeIndex.
                                                                       var fromNode = manager.IndexToNode(fromIndex);
                                                                       var toNode = manager.IndexToNode(toIndex);
                                                                       return data.DistanceMatrix[fromNode, toNode];
                                                                   });
        // [END transit_callback]

        // Define cost of each arc.
        // [START arc_cost]
        routing.SetArcCostEvaluatorOfAllVehicles(transitCallbackIndex);
        // [END arc_cost]

        // Add Capacity constraint.
        // [START capacity_constraint]
        int demandCallbackIndex = routing.RegisterUnaryTransitCallback((long fromIndex) =>
                                                                       {
                                                                           // Convert from routing variable Index to
                                                                           // demand NodeIndex.
                                                                           var fromNode =
                                                                               manager.IndexToNode(fromIndex);
                                                                           return data.Demands[fromNode];
                                                                       });
        routing.AddDimensionWithVehicleCapacity(demandCallbackIndex, 0, // null capacity slack
                                                data.VehicleCapacities, // vehicle maximum capacities
                                                true,                   // start cumul to zero
                                                "Capacity");
        // Allow to drop nodes.
        long penalty = 1000;
        for (int i = 1; i < data.DistanceMatrix.GetLength(0); ++i)
        {
            routing.AddDisjunction(new long[] { manager.NodeToIndex(i) }, penalty);
        }
        // [END capacity_constraint]

        // Setting first solution heuristic.
        // [START parameters]
        RoutingSearchParameters searchParameters =
            operations_research_constraint_solver.DefaultRoutingSearchParameters();
        searchParameters.FirstSolutionStrategy = FirstSolutionStrategy.Types.Value.PathCheapestArc;
        searchParameters.LocalSearchMetaheuristic = LocalSearchMetaheuristic.Types.Value.GuidedLocalSearch;
        searchParameters.TimeLimit = new Duration { Seconds = 1 };
        // [END parameters]

        // Solve the problem.
        // [START solve]
        Assignment solution = routing.SolveWithParameters(searchParameters);
        // [END solve]

        // Print solution on console.
        // [START print_solution]
        PrintSolution(data, routing, manager, solution);
        // [END print_solution]
    }
}
// [END program]
