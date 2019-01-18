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
using System.Collections.Generic;
using Google.OrTools.ConstraintSolver;
// [END import]

/// <summary>
///   Minimal TSP.
///   A description of the problem can be found here:
///   http://en.wikipedia.org/wiki/Travelling_salesman_problem.
/// </summary>
public class Tsp {
  // [START data_model]
  class DataModel {
    // Constructor:
    public DataModel() {
      locations_ = new int[,] {
        {4, 4},
          {2, 0}, {8, 0},
          {0, 1}, {1, 1},
          {5, 2}, {7, 2},
          {3, 3}, {6, 3},
          {5, 5}, {8, 5},
          {1, 6}, {2, 6},
          {3, 7}, {6, 7},
          {0, 8}, {7, 8}
      };
      // Convert locations in meters using a city block dimension of 114m x 80m.
      for (int i=0; i < locations_.GetLength(0); i++) {
        locations_[i, 0] *= 114;
        locations_[i, 1] *= 80;
      }
    }
    public ref readonly int[,] GetLocations() { return ref locations_;}
    public int GetVehicleNumber() { return 1;}
    public int GetDepot() { return 0;}

    private int[,] locations_;
  };
  // [END data_model]

  // [START manhattan_distance]
  /// <summary>
  ///   Manhattan distance implemented as a callback. It uses an array of
  ///   positions and computes the Manhattan distance between the two
  ///   positions of two different indices.
  /// </summary>
  class ManhattanDistance {
    public ManhattanDistance(
        in DataModel data,
        in RoutingIndexManager manager) {
      // precompute distance between location to have distance callback in O(1)
      int locationNumber = data.GetLocations().GetLength(0);
      distancesMatrix_ = new long[locationNumber, locationNumber];
      indexManager_ = manager;
      for (int fromNode = 0; fromNode < locationNumber; fromNode++) {
        for (int toNode = 0; toNode < locationNumber; toNode++) {
          if (fromNode == toNode)
            distancesMatrix_[fromNode, toNode] = 0;
          else
            distancesMatrix_[fromNode, toNode] =
              Math.Abs(data.GetLocations()[toNode, 0] - data.GetLocations()[fromNode, 0]) +
              Math.Abs(data.GetLocations()[toNode, 1] - data.GetLocations()[fromNode, 1]);
        }
      }
    }

    /// <summary>
    ///   Returns the manhattan distance between the two nodes
    /// </summary>
    public long call(long FromIndex, long ToIndex) {
      // Convert from routing variable Index to distance matrix NodeIndex.
      int FromNode = indexManager_.IndexToNode(FromIndex);
      int ToNode = indexManager_.IndexToNode(ToIndex);
      return distancesMatrix_[FromNode, ToNode];
    }
    private long[,] distancesMatrix_;
    private RoutingIndexManager indexManager_;
  };
  // [END manhattan_distance]

  // [START solution_printer]
  /// <summary>
  ///   Print the solution.
  /// </summary>
  static void PrintSolution(
      in RoutingModel routing,
      in RoutingIndexManager manager,
      in Assignment solution) {
    Console.WriteLine("Objective: {0}", solution.ObjectiveValue());
    // Inspect solution.
    Console.WriteLine("Route for Vehicle 0:");
    long routeDistance = 0;
    var index = routing.Start(0);
    while (routing.IsEnd(index) == false) {
      Console.Write("{0} -> ", manager.IndexToNode((int)index));
      var previousIndex = index;
      index = solution.Value(routing.NextVar(index));
      routeDistance += routing.GetArcCostForVehicle(previousIndex, index, 0);
    }
    Console.WriteLine("{0}", manager.IndexToNode((int)index));
    Console.WriteLine("Distance of the route: {0}m", routeDistance);
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
        data.GetLocations().GetLength(0),
        data.GetVehicleNumber(),
        data.GetDepot());
    // [END index_manager]

    // Create Routing Model.
    // [START routing_model]
    RoutingModel routing = new RoutingModel(manager);
    // [END routing_model]

    // Create a distance callback.
    // [START distance_callback]
    var distanceCallback = new ManhattanDistance(data, manager);
    int transitCallbackIndex = routing.RegisterTransitCallback(distanceCallback.call);
    // [END distance_callback]

    // Define cost of each arc.
    // [START arc_cost]
    routing.SetArcCostEvaluatorOfAllVehicles(transitCallbackIndex);
    // [END arc_cost]

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
    PrintSolution(routing, manager, solution);
    // [END print_solution]
  }
}
// [END program]
