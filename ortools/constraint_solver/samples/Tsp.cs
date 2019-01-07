// Copyright 2018 Google
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
using System.Collections.Generic;
using Google.OrTools.ConstraintSolver;

/// <summary>
///   This is a sample using the routing library .Net wrapper to solve a TSP
///   problem.
///   A description of the problem can be found here:
///   http://en.wikipedia.org/wiki/Travelling_salesman_problem.
/// </summary>
public class TSP {
  class DataProblem {
    private int[,] locations_;

    // Constructor:
    public DataProblem() {
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
  };


  /// <summary>
  ///   Manhattan distance implemented as a callback. It uses an array of
  ///   positions and computes the Manhattan distance between the two
  ///   positions of two different indices.
  /// </summary>
  class ManhattanDistance : LongLongToLong {
    private int[,] distances_;
    private RoutingIndexManager manager_;

    public ManhattanDistance(in DataProblem data,
                             in RoutingIndexManager manager) {
      // precompute distance between location to have distance callback in O(1)
      int locationNumber = data.GetLocations().GetLength(0);
      distances_ = new int[locationNumber, locationNumber];
      manager_ = manager;
      for (int fromNode = 0; fromNode < locationNumber; fromNode++) {
        for (int toNode = 0; toNode < locationNumber; toNode++) {
          if (fromNode == toNode)
            distances_[fromNode, toNode] = 0;
          else
            distances_[fromNode, toNode] =
              Math.Abs(data.GetLocations()[toNode, 0] -
                       data.GetLocations()[fromNode, 0]) +
              Math.Abs(data.GetLocations()[toNode, 1] -
                       data.GetLocations()[fromNode, 1]);
        }
      }
    }

    /// <summary>
    ///   Returns the manhattan distance between the two nodes
    /// </summary>
    public override long Run(long FromIndex, long ToIndex) {
      int FromNode = manager_.IndexToNode(FromIndex);
      int ToNode = manager_.IndexToNode(ToIndex);
      return distances_[FromNode, ToNode];
    }
  };

  /// <summary>
  ///   Print the solution
  /// </summary>
  static void PrintSolution(
      in DataProblem data,
      in RoutingModel routing,
      in RoutingIndexManager manager,
      in Assignment solution) {
    Console.WriteLine("Objective: {0}", solution.ObjectiveValue());
    // Inspect solution.
    var index = routing.Start(0);
    Console.WriteLine("Route for Vehicle 0:");
    long distance = 0;
    while (routing.IsEnd(index) == false) {
      Console.Write("{0} -> ", manager.IndexToNode((int)index));
      var previousIndex = index;
      index = solution.Value(routing.NextVar(index));
      distance += routing.GetArcCostForVehicle(previousIndex, index, 0);
    }
    Console.WriteLine("{0}", manager.IndexToNode((int)index));
    Console.WriteLine("Distance of the route: {0}m", distance);
  }

  /// <summary>
  ///   Solves the current routing problem.
  /// </summary>
  static void Solve() {
    // Instantiate the data problem.
    DataProblem data = new DataProblem();

    // Create Routing Model
    RoutingIndexManager manager = new RoutingIndexManager(
        data.GetLocations().GetLength(0),
        data.GetVehicleNumber(),
        data.GetDepot());
    RoutingModel routing = new RoutingModel(manager);

    // Define weight of each edge
    LongLongToLong distanceEvaluator = new ManhattanDistance(data, manager);
    //protect callbacks from the GC
    GC.KeepAlive(distanceEvaluator);
    routing.SetArcCostEvaluatorOfAllVehicles(
        routing.RegisterTransitCallback(distanceEvaluator));

    // Setting first solution heuristic (cheapest addition).
    RoutingSearchParameters searchParameters =
        operations_research_constraint_solver.DefaultRoutingSearchParameters();
    searchParameters.FirstSolutionStrategy =
        FirstSolutionStrategy.Types.Value.PathCheapestArc;

    Assignment solution = routing.SolveWithParameters(searchParameters);
    PrintSolution(data, routing, manager, solution);
  }

  public static void Main(String[] args) {
    Solve();
  }
}
