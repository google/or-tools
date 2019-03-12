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

// [START program]
// [START import]
using System;
using System.Linq;
using System.Collections.Generic;
using Google.OrTools.ConstraintSolver;
// [END import]

/// <summary>
///   Vehicles Routing Problem (VRP) with Resource Constraints.
/// </summary>
public class VrpResources {
  // [START data_model]
  class DataModel {
    public long[,] TimeMatrix = {
      {0, 6, 9, 8, 7, 3, 6, 2, 3, 2, 6, 6, 4, 4, 5, 9, 7},
      {6, 0, 8, 3, 2, 6, 8, 4, 8, 8, 13, 7, 5, 8, 12, 10, 14},
      {9, 8, 0, 11, 10, 6, 3, 9, 5, 8, 4, 15, 14, 13, 9, 18, 9},
      {8, 3, 11, 0, 1, 7, 10, 6, 10, 10, 14, 6, 7, 9, 14, 6, 16},
      {7, 2, 10, 1, 0, 6, 9, 4, 8, 9, 13, 4, 6, 8, 12, 8, 14},
      {3, 6, 6, 7, 6, 0, 2, 3, 2, 2, 7, 9, 7, 7, 6, 12, 8},
      {6, 8, 3, 10, 9, 2, 0, 6, 2, 5, 4, 12, 10, 10, 6, 15, 5},
      {2, 4, 9, 6, 4, 3, 6, 0, 4, 4, 8, 5, 4, 3, 7, 8, 10},
      {3, 8, 5, 10, 8, 2, 2, 4, 0, 3, 4, 9, 8, 7, 3, 13, 6},
      {2, 8, 8, 10, 9, 2, 5, 4, 3, 0, 4, 6, 5, 4, 3, 9, 5},
      {6, 13, 4, 14, 13, 7, 4, 8, 4, 4, 0, 10, 9, 8, 4, 13, 4},
      {6, 7, 15, 6, 4, 9, 12, 5, 9, 6, 10, 0, 1, 3, 7, 3, 10},
      {4, 5, 14, 7, 6, 7, 10, 4, 8, 5, 9, 1, 0, 2, 6, 4, 8},
      {4, 8, 13, 9, 8, 7, 10, 3, 7, 4, 8, 3, 2, 0, 4, 5, 6},
      {5, 12, 9, 14, 12, 6, 6, 7, 3, 3, 4, 7, 6, 4, 0, 9, 2},
      {9, 10, 18, 6, 8, 12, 15, 8, 13, 9, 13, 3, 4, 5, 9, 0, 9},
      {7, 14, 9, 16, 14, 8, 5, 10, 6, 5, 4, 10, 8, 6, 2, 9, 0},
    };
    public long[,] TimeWindows = {
      {0, 5},    // depot
      {7, 12},   // 1
      {10, 15},  // 2
      {5, 14},   // 3
      {5, 13},   // 4
      {0, 5},    // 5
      {5, 10},   // 6
      {0, 10},   // 7
      {5, 10},   // 8
      {0, 5},    // 9
      {10, 16},  // 10
      {10, 15},  // 11
      {0, 5},    // 12
      {5, 10},   // 13
      {7, 12},   // 14
      {10, 15},  // 15
      {5, 15},   // 16
    };
    public int VehicleNumber = 4;
    // [START resources_data]
    public int VehicleLoadTime = 5;
    public int VehicleUnloadTime = 5;
    public int DepotCapacity = 2;
    // [END resources_data]
    public int Depot = 0;
  };
  // [END data_model]

  // [START solution_printer]
  /// <summary>
  ///   Print the solution.
  /// </summary>
  static void PrintSolution(
      in DataModel data,
      in RoutingModel routing,
      in RoutingIndexManager manager,
      in Assignment solution) {
    Console.WriteLine("Objective: {0}", solution.ObjectiveValue());
    RoutingDimension timeDimension = routing.GetMutableDimension("Time");
    // Inspect solution.
    long totalTime = 0;
    for (int i = 0; i < data.VehicleNumber; ++i) {
      Console.WriteLine("Route for Vehicle {0}:", i);
      var index = routing.Start(i);
      while (routing.IsEnd(index) == false) {
        var timeVar = timeDimension.CumulVar(index);
        var slackVar = timeDimension.SlackVar(index);
        Console.Write("{0} Time({1},{2}) Slack({3},{4}) -> ",
            manager.IndexToNode(index),
            solution.Min(timeVar),
            solution.Max(timeVar),
            solution.Min(slackVar),
            solution.Max(slackVar));
        var previousIndex = index;
        index = solution.Value(routing.NextVar(index));
      }
      var endTimeVar = timeDimension.CumulVar(index);
      Console.WriteLine("{0} Time({1},{2})",
          manager.IndexToNode(index),
          solution.Min(endTimeVar),
          solution.Max(endTimeVar));
      Console.WriteLine("Time of the route: {0}min", solution.Min(endTimeVar));
      totalTime += solution.Min(endTimeVar);
    }
    Console.WriteLine("Total time of all routes: {0}min", totalTime);
  }
  // [END solution_printer]

  public static void Main(String[] args) {
    // Instantiate the data problem.
    // [START data]
    DataModel data = new DataModel();
    // [END data]

    // Create Routing Index Manager
    // [START index_manager]
    RoutingIndexManager manager = new RoutingIndexManager(
        data.TimeMatrix.GetLength(0),
        data.VehicleNumber,
        data.Depot);
    // [END index_manager]

    // Create Routing Model.
    // [START routing_model]
    RoutingModel routing = new RoutingModel(manager);
    // [END routing_model]

    // Create and register a transit callback.
    // [START transit_callback]
    int transitCallbackIndex = routing.RegisterTransitCallback(
        (long fromIndex, long toIndex) => {
        // Convert from routing variable Index to distance matrix NodeIndex.
        var fromNode = manager.IndexToNode(fromIndex);
        var toNode = manager.IndexToNode(toIndex);
        return data.TimeMatrix[fromNode, toNode]; }
        );
    // [END transit_callback]

    // Define cost of each arc.
    // [START arc_cost]
    routing.SetArcCostEvaluatorOfAllVehicles(transitCallbackIndex);
    // [END arc_cost]

    // Add Distance constraint.
    // [START time_constraint]
    routing.AddDimension(
        transitCallbackIndex, // transit callback
        30, // allow waiting time
        30, // vehicle maximum capacities
        false,  // start cumul to zero
        "Time");
    RoutingDimension timeDimension = routing.GetMutableDimension("Time");
    // Add time window constraints for each location except depot
    // and 'copy' the slack var in the solution object (aka Assignment) to print it
    for (int i = 1; i < data.TimeWindows.GetLength(0); ++i) {
      long index = manager.NodeToIndex(i);
      timeDimension.CumulVar(index).SetRange(
          data.TimeWindows[i, 0],
          data.TimeWindows[i, 1]);
      routing.AddToAssignment(timeDimension.SlackVar(index));
    }
    // Add time window constraints for each vehicle start node
    // and 'copy' the slack var in the solution object (aka Assignment) to print
    // it
    for (int i = 0; i < data.VehicleNumber; ++i) {
      long index = routing.Start(i);
      timeDimension.CumulVar(index).SetRange(
          data.TimeWindows[0, 0],
          data.TimeWindows[0, 1]);
      routing.AddToAssignment(timeDimension.SlackVar(index));
    }
    // [END time_constraint]

    // Add resource constraints at the depot.
    // [START depot_load_time]
    Solver solver = routing.solver();
    IntervalVar[] intervals = new IntervalVar[ data.VehicleNumber * 2 ];
    for (int i = 0; i < data.VehicleNumber; ++i) {
      // Add load duration at start of routes
      intervals[2*i] = solver.MakeFixedDurationIntervalVar(
            timeDimension.CumulVar(routing.Start(i)), data.VehicleLoadTime,
            "depot_interval");
      // Add unload duration at end of routes.
      intervals[2*i+1] = solver.MakeFixedDurationIntervalVar(
            timeDimension.CumulVar(routing.End(i)), data.VehicleUnloadTime,
            "depot_interval");
    }
    // [END depot_load_time]

    // [START depot_capacity]
    long[] depot_usage = Enumerable.Repeat<long>(1, intervals.Length).ToArray();
    solver.Add(solver.MakeCumulative(intervals, depot_usage,
          data.DepotCapacity, "depot"));
    // [END depot_capacity]

    // Instantiate route start and end times to produce feasible times.
    // [START depot_start_end_times]
    for (int i = 0; i < data.VehicleNumber; ++i) {
      routing.AddVariableMinimizedByFinalizer(
          timeDimension.CumulVar(routing.Start(i)));
      routing.AddVariableMinimizedByFinalizer(
          timeDimension.CumulVar(routing.End(i)));
    }
    // [END depot_start_end_times]

    // Setting first solution heuristic.
    // [START parameters]
    RoutingSearchParameters searchParameters =
      operations_research_constraint_solver.DefaultRoutingSearchParameters();
    searchParameters.FirstSolutionStrategy =
      FirstSolutionStrategy.Types.Value.PathCheapestArc;
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
