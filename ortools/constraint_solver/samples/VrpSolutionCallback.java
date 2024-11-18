// Copyright 2010-2024 Google LLC
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
import com.google.ortools.Loader;
import com.google.ortools.constraintsolver.Assignment;
import com.google.ortools.constraintsolver.FirstSolutionStrategy;
import com.google.ortools.constraintsolver.LocalSearchMetaheuristic;
import com.google.ortools.constraintsolver.RoutingDimension;
import com.google.ortools.constraintsolver.RoutingIndexManager;
import com.google.ortools.constraintsolver.RoutingModel;
import com.google.ortools.constraintsolver.RoutingSearchParameters;
import com.google.ortools.constraintsolver.main;
import com.google.protobuf.Duration;
import java.util.logging.Logger;
// [END import]

/** Minimal VRP. */
public final class VrpSolutionCallback {
  private static final Logger logger = Logger.getLogger(VrpSolutionCallback.class.getName());

  // [START data_model]
  static class DataModel {
    public final long[][] distanceMatrix = {
        {0, 548, 776, 696, 582, 274, 502, 194, 308, 194, 536, 502, 388, 354, 468, 776, 662},
        {548, 0, 684, 308, 194, 502, 730, 354, 696, 742, 1084, 594, 480, 674, 1016, 868, 1210},
        {776, 684, 0, 992, 878, 502, 274, 810, 468, 742, 400, 1278, 1164, 1130, 788, 1552, 754},
        {696, 308, 992, 0, 114, 650, 878, 502, 844, 890, 1232, 514, 628, 822, 1164, 560, 1358},
        {582, 194, 878, 114, 0, 536, 764, 388, 730, 776, 1118, 400, 514, 708, 1050, 674, 1244},
        {274, 502, 502, 650, 536, 0, 228, 308, 194, 240, 582, 776, 662, 628, 514, 1050, 708},
        {502, 730, 274, 878, 764, 228, 0, 536, 194, 468, 354, 1004, 890, 856, 514, 1278, 480},
        {194, 354, 810, 502, 388, 308, 536, 0, 342, 388, 730, 468, 354, 320, 662, 742, 856},
        {308, 696, 468, 844, 730, 194, 194, 342, 0, 274, 388, 810, 696, 662, 320, 1084, 514},
        {194, 742, 742, 890, 776, 240, 468, 388, 274, 0, 342, 536, 422, 388, 274, 810, 468},
        {536, 1084, 400, 1232, 1118, 582, 354, 730, 388, 342, 0, 878, 764, 730, 388, 1152, 354},
        {502, 594, 1278, 514, 400, 776, 1004, 468, 810, 536, 878, 0, 114, 308, 650, 274, 844},
        {388, 480, 1164, 628, 514, 662, 890, 354, 696, 422, 764, 114, 0, 194, 536, 388, 730},
        {354, 674, 1130, 822, 708, 628, 856, 320, 662, 388, 730, 308, 194, 0, 342, 422, 536},
        {468, 1016, 788, 1164, 1050, 514, 514, 662, 320, 274, 388, 650, 536, 342, 0, 764, 194},
        {776, 868, 1552, 560, 674, 1050, 1278, 742, 1084, 810, 1152, 274, 388, 422, 764, 0, 798},
        {662, 1210, 754, 1358, 1244, 708, 480, 856, 514, 468, 354, 844, 730, 536, 194, 798, 0},
    };
    public final int vehicleNumber = 4;
    public final int depot = 0;
  }
  // [END data_model]

  // [START solution_callback_printer]
  /// @brief Print the solution.
  static void printSolution(RoutingIndexManager routingManager, RoutingModel routingModel) {
    // Solution cost.
    logger.info("################");
    logger.info("Solution objective : " + routingModel.costVar().value());
    // Inspect solution.
    long totalDistance = 0;
    for (int i = 0; i < routingManager.getNumberOfVehicles(); ++i) {
      logger.info("Route for Vehicle " + i + ":");
      long routeDistance = 0;
      long index = routingModel.start(i);
      String route = "";
      while (!routingModel.isEnd(index)) {
        route += routingManager.indexToNode(index) + " -> ";
        long previousIndex = index;
        index = routingModel.nextVar(index).value();
        routeDistance += routingModel.getArcCostForVehicle(previousIndex, index, i);
      }
      logger.info(route + routingManager.indexToNode(index));
      logger.info("Distance of the route: " + routeDistance + "m");
      totalDistance += routeDistance;
    }
    logger.info("Total distance of all routes: " + totalDistance + "m");
  }
  // [END solution_callback_printer]

  // [START solution_callback]
  static class SolutionCallback implements Runnable {
    public final long[] objectives;
    private final RoutingIndexManager routingManager;
    private final RoutingModel routingModel;
    private final int maxSolution;
    private int counter;

    public SolutionCallback(
        final RoutingIndexManager manager, final RoutingModel routing, int limit) {
      routingManager = manager;
      routingModel = routing;
      maxSolution = limit;
      counter = 0;
      objectives = new long[maxSolution];
    }

    @Override
    public void run() {
      long objective = routingModel.costVar().value();
      if (counter == 0 || objective < objectives[counter - 1]) {
        objectives[counter] = objective;
        printSolution(routingManager, routingModel);
        counter++;
      }
      if (counter >= maxSolution) {
        routingModel.solver().finishCurrentSearch();
      }
    }
  };

  // [END solution_callback]

  public static void main(String[] args) {
    Loader.loadNativeLibraries();
    // Instantiate the data problem.
    // [START data]
    final DataModel data = new DataModel();
    // [END data]

    // Create Routing Index Manager
    // [START index_manager]
    final RoutingIndexManager routingManager =
        new RoutingIndexManager(data.distanceMatrix.length, data.vehicleNumber, data.depot);
    // [END index_manager]

    // Create Routing Model.
    // [START routing_model]
    final RoutingModel routingModel = new RoutingModel(routingManager);
    // [END routing_model]

    // Create and register a transit callback.
    // [START transit_callback]
    final int transitCallbackIndex =
        routingModel.registerTransitCallback((long fromIndex, long toIndex) -> {
          // Convert from routing variable Index to user NodeIndex.
          int fromNode = routingManager.indexToNode(fromIndex);
          int toNode = routingManager.indexToNode(toIndex);
          return data.distanceMatrix[fromNode][toNode];
        });
    // [END transit_callback]

    // Define cost of each arc.
    // [START arc_cost]
    routingModel.setArcCostEvaluatorOfAllVehicles(transitCallbackIndex);
    // [END arc_cost]

    // Add Distance constraint.
    // [START distance_constraint]
    boolean unused = routingModel.addDimension(transitCallbackIndex,
        0, // no slack
        3000, // vehicle maximum travel distance
        true, // start cumul to zero
        "Distance");
    RoutingDimension distanceDimension = routingModel.getMutableDimension("Distance");
    distanceDimension.setGlobalSpanCostCoefficient(100);
    // [END distance_constraint]

    // Attach a solution callback.
    // [START attach_callback]
    final SolutionCallback solutionCallback =
        new SolutionCallback(routingManager, routingModel, 15);
    routingModel.addAtSolutionCallback(solutionCallback);
    // [END attach_callback]

    // Setting first solution heuristic.
    // [START parameters]
    RoutingSearchParameters searchParameters =
        main.defaultRoutingSearchParameters()
            .toBuilder()
            .setFirstSolutionStrategy(FirstSolutionStrategy.Value.PATH_CHEAPEST_ARC)
            .setLocalSearchMetaheuristic(LocalSearchMetaheuristic.Value.GUIDED_LOCAL_SEARCH)
            .setTimeLimit(Duration.newBuilder().setSeconds(5).build())
            .build();
    // [END parameters]

    // Solve the problem.
    // [START solve]
    Assignment solution = routingModel.solveWithParameters(searchParameters);
    // [END solve]

    // Print solution on console.
    // [START print_solution]
    if (solution != null) {
      logger.info(
          "Best objective: " + solutionCallback.objectives[solutionCallback.objectives.length - 1]);
    } else {
      logger.info("No solution found !");
    }
    // [END print_solution]
  }

  private VrpSolutionCallback() {}
}
// [END program]
