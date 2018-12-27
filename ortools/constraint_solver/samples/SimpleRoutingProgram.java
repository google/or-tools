// Copyright 2018 Google LLC
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
import java.util.logging.Logger;
import com.google.ortools.constraintsolver.Assignment;
import com.google.ortools.constraintsolver.FirstSolutionStrategy;
import com.google.ortools.constraintsolver.LongLongToLong;
import com.google.ortools.constraintsolver.RoutingIndexManager;
import com.google.ortools.constraintsolver.RoutingModel;
import com.google.ortools.constraintsolver.RoutingSearchParameters;
import com.google.ortools.constraintsolver.main;
// [END import]

class SimpleRoutingProgram {
  static {
    System.loadLibrary("jniortools");
  }

  private static Logger logger =
    Logger.getLogger(SimpleRoutingProgram.class.getName());

  static void SimpleRoutingProgram() {
    // Instantiate the data problem.
    // [START data]
    final int num_location = 5;
    final int num_vehicles = 1;
    final int depot = 0;
    // [END data]

    // Create Routing Index Manager
    // [START index_manager]
    RoutingIndexManager manager = new RoutingIndexManager(num_location, num_vehicles, depot);
    // [END index_manager]

    // Create Routing Model.
    // [START routing_model]
    RoutingModel routing = new RoutingModel(manager);
    // [END routing_model]

    // Define cost of each arc.
    // [START arc_cost]
    routing.setArcCostEvaluatorOfAllVehicles(
        routing.registerTransitCallback(
          new LongLongToLong() {
            @Override
            public long run(long from_node, long to_node) {
              return 1;
            }
          }));
    // [END arc_cost]

    // Setting first solution heuristic.
    // [START parameters]
    RoutingSearchParameters search_parameters =
        RoutingSearchParameters.newBuilder()
            .mergeFrom(main.defaultRoutingSearchParameters())
            .setFirstSolutionStrategy(FirstSolutionStrategy.Value.PATH_CHEAPEST_ARC)
            .build();
    // [END parameters]

    // Solve the problem.
    // [START solve]
    Assignment solution = routing.solveWithParameters(search_parameters);
    // [END solve]

    // Print solution on console.
    // [START print_solution]
    logger.info("Objective: " + solution.objectiveValue());
    // Inspect solution.
    long index = routing.start(0);
    logger.info("Route for Vehicle 0:");
    long route_distance = 0;
    String route = "";
    while (routing.isEnd(index) == false) {
      route += manager.indexToNode(index) + " -> ";
      long previous_index = index;
      index = solution.value(routing.nextVar(index));
      route_distance += routing.getArcCostForVehicle(previous_index, index, 0);
    }
    route += manager.indexToNode(index);
    logger.info(route);
    logger.info("Distance of the route: " + route_distance + "m");
    // [END print_solution]
  }

  public static void main(String[] args) throws Exception {
    SimpleRoutingProgram();
  }
}
// [END program]
