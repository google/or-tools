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

/** Minimal VRP.*/
public class VrpWithTimeLimit {
  static {
    System.loadLibrary("jniortools");
  }

  private static final Logger logger = Logger.getLogger(VrpWithTimeLimit.class.getName());

  // [START solution_printer]
  /// @brief Print the solution.
  static void printSolution(RoutingIndexManager manager, RoutingModel routing, Assignment solution) {
    // Inspect solution.
    long maxRouteDistance = 0;
    for (int i = 0; i < manager.getNumberOfVehicles(); ++i) {
      long index = routing.start(i);
      logger.info("Route for Vehicle " + i + ":");
      long routeDistance = 0;
      String route = "";
      while (!routing.isEnd(index)) {
        route += manager.indexToNode(index) + " -> ";
        long previousIndex = index;
        index = solution.value(routing.nextVar(index));
        routeDistance += routing.getArcCostForVehicle(previousIndex, index, i);
      }
      logger.info(route + manager.indexToNode(index));
      logger.info("Distance of the route: " + routeDistance + "m");
      maxRouteDistance = Math.max(routeDistance, maxRouteDistance);
    }
    logger.info("Maximum of the route distances: " + maxRouteDistance + "m");
  }
  // [END solution_printer]

  public static void main(String[] args) throws Exception {
    // Instantiate the data problem.
    // [START data]
    final int locationNumber = 20;
    final int vehicleNumber = 5;
    final int depot = 0;
    // [END data]

    // Create Routing Index Manager
    // [START index_manager]
    RoutingIndexManager manager =
        new RoutingIndexManager(locationNumber, vehicleNumber, depot);
    // [END index_manager]

    // Create Routing Model.
    // [START routing_model]
    RoutingModel routing = new RoutingModel(manager);
    // [END routing_model]

    // Create and register a transit callback.
    // [START transit_callback]
    final int transitCallbackIndex =
        routing.registerTransitCallback((long fromIndex, long toIndex) -> {
          // Convert from routing variable Index to user NodeIndex.
          int fromNode = manager.indexToNode(fromIndex);
          int toNode = manager.indexToNode(toIndex);
          return 1;
        });
    // [END transit_callback]

    // Define cost of each arc.
    // [START arc_cost]
    routing.setArcCostEvaluatorOfAllVehicles(transitCallbackIndex);
    // [END arc_cost]

    // Add Distance constraint.
    // [START distance_constraint]
    routing.addDimension(
        transitCallbackIndex,
        /*slack=*/0,
        /*horizon=*/3000,
        /*start_cumul_to_zero=*/true,
        "Distance");
    RoutingDimension distanceDimension = routing.getMutableDimension("Distance");
    distanceDimension.setGlobalSpanCostCoefficient(100);
    // [END distance_constraint]

    // Setting first solution heuristic.
    // [START parameters]
    RoutingSearchParameters searchParameters =
        main.defaultRoutingSearchParameters()
            .toBuilder()
            .setFirstSolutionStrategy(FirstSolutionStrategy.Value.PATH_CHEAPEST_ARC)
            .setLocalSearchMetaheuristic(LocalSearchMetaheuristic.Value.GUIDED_LOCAL_SEARCH)
            .setLogSearch(true)
            .setTimeLimit(Duration.newBuilder().setSeconds(10).build())
            .build();
    // [END parameters]

    // Solve the problem.
    // [START solve]
    Assignment solution = routing.solveWithParameters(searchParameters);
    // [END solve]

    // Print solution on console.
    // [START print_solution]
    printSolution(manager, routing, solution);
    // [END print_solution]
  }
}
// [END program]
