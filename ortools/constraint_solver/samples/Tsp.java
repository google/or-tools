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
package com.google.ortools.constraintsolver.samples;
// [START import]
import static java.lang.Math.abs;

import com.google.ortools.Loader;
import com.google.ortools.constraintsolver.Assignment;
import com.google.ortools.constraintsolver.FirstSolutionStrategy;
import com.google.ortools.constraintsolver.RoutingIndexManager;
import com.google.ortools.constraintsolver.RoutingModel;
import com.google.ortools.constraintsolver.RoutingSearchParameters;
import com.google.ortools.constraintsolver.main;
import java.util.function.LongBinaryOperator;
import java.util.logging.Logger;
// [END import]

/** Minimal TSP.*/
public class Tsp {
  private static final Logger logger = Logger.getLogger(Tsp.class.getName());

  // [START data_model]
  static class DataModel {
    public final int[][] locations = {
        {4, 4},
        {2, 0},
        {8, 0},
        {0, 1},
        {1, 1},
        {5, 2},
        {7, 2},
        {3, 3},
        {6, 3},
        {5, 5},
        {8, 5},
        {1, 6},
        {2, 6},
        {3, 7},
        {6, 7},
        {0, 8},
        {7, 8},
    };
    public final int vehicleNumber = 1;
    public final int depot = 0;
    public DataModel() {
      // Convert locations in meters using a city block dimension of 114m x 80m.
      for (int[] element : locations) {
        element[0] *= 114;
        element[1] *= 80;
      }
    }
  }
  // [END data_model]

  // [START manhattan_distance]
  /// @brief Manhattan distance implemented as a callback.
  /// @details It uses an array of positions and computes
  /// the Manhattan distance between the two positions of
  /// two different indices.
  static class ManhattanDistance implements LongBinaryOperator {
    public ManhattanDistance(DataModel data, RoutingIndexManager manager) {
      // precompute distance between location to have distance callback in O(1)
      distanceMatrix = new long[data.locations.length][data.locations.length];
      indexManager = manager;
      for (int fromNode = 0; fromNode < data.locations.length; ++fromNode) {
        for (int toNode = 0; toNode < data.locations.length; ++toNode) {
          if (fromNode == toNode) {
            distanceMatrix[fromNode][toNode] = 0;
          } else {
            distanceMatrix[fromNode][toNode] =
                (long) abs(data.locations[toNode][0] - data.locations[fromNode][0])
                + (long) abs(data.locations[toNode][1] - data.locations[fromNode][1]);
          }
        }
      }
    }
    @Override
    public long applyAsLong(long fromIndex, long toIndex) {
      // Convert from routing variable Index to distance matrix NodeIndex.
      int fromNode = indexManager.indexToNode(fromIndex);
      int toNode = indexManager.indexToNode(toIndex);
      return distanceMatrix[fromNode][toNode];
    }
    private final long[][] distanceMatrix;
    private final RoutingIndexManager indexManager;
  }
  // [END manhattan_distance]

  // [START solution_printer]
  /// @brief Print the solution.
  static void printSolution(
      DataModel data, RoutingModel routing, RoutingIndexManager manager, Assignment solution) {
    // Solution cost.
    logger.info("Objective : " + solution.objectiveValue());
    // Inspect solution.
    logger.info("Route for Vehicle 0:");
    long routeDistance = 0;
    String route = "";
    long index = routing.start(0);
    while (!routing.isEnd(index)) {
      route += manager.indexToNode(index) + " -> ";
      long previousIndex = index;
      index = solution.value(routing.nextVar(index));
      routeDistance += routing.getArcCostForVehicle(previousIndex, index, 0);
    }
    route += manager.indexToNode(routing.end(0));
    logger.info(route);
    logger.info("Distance of the route: " + routeDistance + "m");
  }
  // [END solution_printer]

  public static void main(String[] args) throws Exception {
    Loader.loadNativeLibraries();
    // Instantiate the data problem.
    // [START data]
    final DataModel data = new DataModel();
    // [END data]

    // Create Routing Index Manager
    // [START index_manager]
    RoutingIndexManager manager =
        new RoutingIndexManager(data.locations.length, data.vehicleNumber, data.depot);
    // [END index_manager]

    // Create Routing Model.
    // [START routing_model]
    RoutingModel routing = new RoutingModel(manager);
    // [END routing_model]

    // Create and register a transit callback.
    // [START transit_callback]
    final int transitCallbackIndex =
        routing.registerTransitCallback(new ManhattanDistance(data, manager));
    // [END transit_callback]

    // Define cost of each arc.
    // [START arc_cost]
    routing.setArcCostEvaluatorOfAllVehicles(transitCallbackIndex);
    // [END arc_cost]

    // Setting first solution heuristic.
    // [START parameters]
    RoutingSearchParameters searchParameters =
        main.defaultRoutingSearchParameters()
            .toBuilder()
            .setFirstSolutionStrategy(FirstSolutionStrategy.Value.PATH_CHEAPEST_ARC)
            .build();
    // [END parameters]

    // Solve the problem.
    // [START solve]
    Assignment solution = routing.solveWithParameters(searchParameters);
    // [END solve]

    // Print solution on console.
    // [START print_solution]
    printSolution(data, routing, manager, solution);
    // [END print_solution]
  }
}
// [END program]
