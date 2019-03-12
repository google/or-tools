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
import com.google.ortools.constraintsolver.IntVar;
import com.google.ortools.constraintsolver.IntervalVar;
import com.google.ortools.constraintsolver.RoutingDimension;
import com.google.ortools.constraintsolver.RoutingIndexManager;
import com.google.ortools.constraintsolver.RoutingModel;
import com.google.ortools.constraintsolver.RoutingSearchParameters;
import com.google.ortools.constraintsolver.Solver;
import com.google.ortools.constraintsolver.main;
import java.util.Arrays;
import java.util.logging.Logger;
// [END import]

/** Minimal VRP with Resource Constraints.*/
public class VrpResources {
  static {
    System.loadLibrary("jniortools");
  }

  private static final Logger logger = Logger.getLogger(VrpResources.class.getName());

  // [START data_model]
  static class DataModel {
    public final long[][] timeMatrix = {
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
    public final long[][] timeWindows = {
        {0, 5},  // depot
        {7, 12}, // 1
        {10, 15}, // 2
        {5, 14}, // 3
        {5, 13}, // 4
        {0, 5}, // 5
        {5, 10}, // 6
        {0, 10}, // 7
        {5, 10}, // 8
        {0, 5}, // 9
        {10, 16}, // 10
        {10, 15}, // 11
        {0, 5}, // 12
        {5, 10}, // 13
        {7, 12}, // 14
        {10, 15}, // 15
        {5, 15}, // 16
    };
    public final int vehicleNumber = 4;
    // [START resources_data]
    public final int vehicleLoadTime = 5;
    public final int vehicleUnloadTime = 5;
    public final int depotCapacity = 2;
    // [END resources_data]
    public final int depot = 0;
  }
  // [END data_model]

  // [START solution_printer]
  /// @brief Print the solution.
  static void printSolution(
      DataModel data, RoutingModel routing, RoutingIndexManager manager, Assignment solution) {
    logger.info("Objective : " + solution.objectiveValue());
    RoutingDimension timeDimension = routing.getMutableDimension("Time");
    long totalTime = 0;
    for (int i = 0; i < data.vehicleNumber; ++i) {
      long index = routing.start(i);
      logger.info("Route for Vehicle " + i + ":");
      String route = "";
      while (!routing.isEnd(index)) {
        IntVar timeVar = timeDimension.cumulVar(index);
        IntVar slackVar = timeDimension.slackVar(index);
        route += manager.indexToNode(index) + " Time(" + solution.min(timeVar) + ","
            + solution.max(timeVar) + ") Slack(" + solution.min(slackVar) + ","
            + solution.max(slackVar) + ") -> ";
        long previousIndex = index;
        index = solution.value(routing.nextVar(index));
      }
      IntVar timeVar = timeDimension.cumulVar(index);
      route += manager.indexToNode(index) + " Time(" + solution.min(timeVar) + ","
          + solution.max(timeVar) + ")";
      logger.info(route);
      logger.info("Time of the route: " + solution.min(timeVar) + "min");
      totalTime += solution.min(timeVar);
    }
    logger.info("Total time of all routes: " + totalTime + "min");
  }
  // [END solution_printer]

  public static void main(String[] args) throws Exception {
    // Instantiate the data problem.
    // [START data]
    final DataModel data = new DataModel();
    // [END data]

    // Create Routing Index Manager
    // [START index_manager]
    RoutingIndexManager manager =
        new RoutingIndexManager(data.timeMatrix.length, data.vehicleNumber, data.depot);
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
          return data.timeMatrix[fromNode][toNode];
        });
    // [END transit_callback]

    // Define cost of each arc.
    // [START arc_cost]
    routing.setArcCostEvaluatorOfAllVehicles(transitCallbackIndex);
    // [END arc_cost]

    // Add Time constraint.
    // [START time_constraint]
    routing.addDimension(transitCallbackIndex, // transit callback
        30, // allow waiting time
        30, // vehicle maximum capacities
        false, // start cumul to zero
        "Time");
    RoutingDimension timeDimension = routing.getMutableDimension("Time");
    // Add time window constraints for each location except depot
    // and 'copy' the slack var in the solution object (aka Assignment) to print it
    for (int i = 1; i < data.timeWindows.length; ++i) {
      long index = manager.nodeToIndex(i);
      timeDimension.cumulVar(index).setRange(data.timeWindows[i][0], data.timeWindows[i][1]);
      routing.addToAssignment(timeDimension.slackVar(index));
    }
    // Add time window constraints for each vehicle start node
    // and 'copy' the slack var in the solution object (aka Assignment) to print
    // it
    for (int i = 0; i < data.vehicleNumber; ++i) {
      long index = routing.start(i);
      timeDimension.cumulVar(index).setRange(data.timeWindows[0][0], data.timeWindows[0][1]);
      routing.addToAssignment(timeDimension.slackVar(index));
    }
    // [END time_constraint]

    // Add resource constraints at the depot.
    // [START depot_load_time]
    Solver solver = routing.solver();
    IntervalVar[] intervals = new IntervalVar[ data.vehicleNumber * 2 ];
    for (int i = 0; i < data.vehicleNumber; ++i) {
      // Add load duration at start of routes
      intervals[2*i] = solver.makeFixedDurationIntervalVar(
            timeDimension.cumulVar(routing.start(i)), data.vehicleLoadTime,
            "depot_interval");
      // Add unload duration at end of routes.
      intervals[2*i+1] = solver.makeFixedDurationIntervalVar(
            timeDimension.cumulVar(routing.end(i)), data.vehicleUnloadTime,
            "depot_interval");
    }
    // [END depot_load_time]

    // [START depot_capacity]
    long[] depot_usage = new long[ intervals.length ];
    Arrays.fill(depot_usage, 1);
    solver.addConstraint(solver.makeCumulative(intervals, depot_usage,
          data.depotCapacity, "depot"));
    // [END depot_capacity]

    // Instantiate route start and end times to produce feasible times.
    // [START depot_start_end_times]
    for (int i = 0; i < data.vehicleNumber; ++i) {
      routing.addVariableMinimizedByFinalizer(
          timeDimension.cumulVar(routing.start(i)));
      routing.addVariableMinimizedByFinalizer(
          timeDimension.cumulVar(routing.end(i)));
    }
    // [END depot_start_end_times]

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
