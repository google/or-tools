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
// [END import]

/// <summary>
///   Minimal TSP using time matrix.
/// </summary>
public class VrpBreaks
{
    // [START data_model]
    class DataModel
    {
        public long[,] TimeMatrix = {
            { 0, 27, 38, 34, 29, 13, 25, 9, 15, 9, 26, 25, 19, 17, 23, 38, 33 },
            { 27, 0, 34, 15, 9, 25, 36, 17, 34, 37, 54, 29, 24, 33, 50, 43, 60 },
            { 38, 34, 0, 49, 43, 25, 13, 40, 23, 37, 20, 63, 58, 56, 39, 77, 37 },
            { 34, 15, 49, 0, 5, 32, 43, 25, 42, 44, 61, 25, 31, 41, 58, 28, 67 },
            { 29, 9, 43, 5, 0, 26, 38, 19, 36, 38, 55, 20, 25, 35, 52, 33, 62 },
            { 13, 25, 25, 32, 26, 0, 11, 15, 9, 12, 29, 38, 33, 31, 25, 52, 35 },
            { 25, 36, 13, 43, 38, 11, 0, 26, 9, 23, 17, 50, 44, 42, 25, 63, 24 },
            { 9, 17, 40, 25, 19, 15, 26, 0, 17, 19, 36, 23, 17, 16, 33, 37, 42 },
            { 15, 34, 23, 42, 36, 9, 9, 17, 0, 13, 19, 40, 34, 33, 16, 54, 25 },
            { 9, 37, 37, 44, 38, 12, 23, 19, 13, 0, 17, 26, 21, 19, 13, 40, 23 },
            { 26, 54, 20, 61, 55, 29, 17, 36, 19, 17, 0, 43, 38, 36, 19, 57, 17 },
            { 25, 29, 63, 25, 20, 38, 50, 23, 40, 26, 43, 0, 5, 15, 32, 13, 42 },
            { 19, 24, 58, 31, 25, 33, 44, 17, 34, 21, 38, 5, 0, 9, 26, 19, 36 },
            { 17, 33, 56, 41, 35, 31, 42, 16, 33, 19, 36, 15, 9, 0, 17, 21, 26 },
            { 23, 50, 39, 58, 52, 25, 25, 33, 16, 13, 19, 32, 26, 17, 0, 38, 9 },
            { 38, 43, 77, 28, 33, 52, 63, 37, 54, 40, 57, 13, 19, 21, 38, 0, 39 },
            { 33, 60, 37, 67, 62, 35, 24, 42, 25, 23, 17, 42, 36, 26, 9, 39, 0 },
        };
        public long[] ServiceTime = {
            0, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
        };
        public int VehicleNumber = 4;
        public int Depot = 0;
    };
    // [END data_model]

    // [START solution_printer]
    /// <summary>
    ///   Print the solution.
    /// </summary>
    static void PrintSolution(in RoutingModel routing, in RoutingIndexManager manager, in Assignment solution)
    {
        Console.WriteLine($"Objective {solution.ObjectiveValue()}:");

        // Inspect solution.
        Console.WriteLine("Breaks:");
        AssignmentIntervalContainer intervals = solution.IntervalVarContainer();
        for (int i = 0; i < intervals.Size(); ++i)
        {
            IntervalVarElement breakInterval = intervals.Element(i);
            if (breakInterval.PerformedValue() == 1)
            {
                Console.WriteLine($"{breakInterval.Var().Name()} " + breakInterval.ToString());
            }
            else
            {
                Console.WriteLine($"{breakInterval.Var().Name()}: Unperformed");
            }
        }

        RoutingDimension timeDimension = routing.GetMutableDimension("Time");
        long totalTime = 0;
        for (int i = 0; i < manager.GetNumberOfVehicles(); ++i)
        {
            Console.WriteLine($"Route for Vehicle {i}:");
            var index = routing.Start(i);
            while (routing.IsEnd(index) == false)
            {
                IntVar timeVar = timeDimension.CumulVar(index);
                Console.Write($"{manager.IndexToNode((int)index)} Time({solution.Value(timeVar)}) -> ");
                index = solution.Value(routing.NextVar(index));
            }
            IntVar endTimeVar = timeDimension.CumulVar(index);
            Console.WriteLine($"{manager.IndexToNode((int)index)} Time({solution.Value(endTimeVar)})");
            Console.WriteLine($"Time of the route: {solution.Value(endTimeVar)}min");
            totalTime += solution.Value(endTimeVar);
        }
        Console.WriteLine($"Total time of all routes: {totalTime}min");
        Console.WriteLine($"Problem solved in {routing.solver().WallTime()}ms");
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
            new RoutingIndexManager(data.TimeMatrix.GetLength(0), data.VehicleNumber, data.Depot);

        // [END index_manager]

        // Create Routing Model.
        // [START routing_model]
        RoutingModel routing = new RoutingModel(manager);
        // [END routing_model]

        // Create and register a transit callback.
        // [START transit_callback]
        int transitCallbackIndex = routing.RegisterTransitCallback((long fromIndex, long toIndex) => {
            // Convert from routing variable Index to time matrix NodeIndex.
            var fromNode = manager.IndexToNode(fromIndex);
            var toNode = manager.IndexToNode(toIndex);
            return data.TimeMatrix[fromNode, toNode] + data.ServiceTime[fromNode];
        });
        // [END transit_callback]

        // Define cost of each arc.
        // [START arc_cost]
        routing.SetArcCostEvaluatorOfAllVehicles(transitCallbackIndex);
        // [END arc_cost]

        // Add Time constraint.
        // [START time_constraint]
        routing.AddDimension(transitCallbackIndex, 10, 180,
                             true, // start cumul to zero
                             "Time");
        RoutingDimension timeDimension = routing.GetMutableDimension("Time");
        timeDimension.SetGlobalSpanCostCoefficient(10);
        // [END time_constraint]

        // Add Breaks
        long[] serviceTimes = new long[routing.Size()];
        for (int index = 0; index < routing.Size(); index++)
        {
            int node = manager.IndexToNode(index);
            serviceTimes[index] = data.ServiceTime[node];
        }

        Solver solver = routing.solver();
        for (int vehicle = 0; vehicle < manager.GetNumberOfVehicles(); ++vehicle)
        {
            List<IntervalVar> breakIntervals = new List<IntervalVar>();
            IntervalVar break_interval = solver.MakeFixedDurationIntervalVar(50,    // start min
                                                                             60,    // start max
                                                                             10,    // duration: 10min
                                                                             false, // optional: no
                                                                             "Break for vehicle " + vehicle);
            breakIntervals.Add(break_interval);

            timeDimension.SetBreakIntervalsOfVehicle(breakIntervals.ToArray(), vehicle, serviceTimes);
        }

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
        if (solution != null)
        {
            PrintSolution(routing, manager, solution);
        }
        else
        {
            Console.WriteLine("Solution not found.");
        }
        // [END print_solution]
    }
}
// [END program]
