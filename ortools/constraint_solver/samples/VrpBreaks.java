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
import com.google.ortools.Loader;
import com.google.ortools.constraintsolver.Assignment;
import com.google.ortools.constraintsolver.AssignmentIntervalContainer;
import com.google.ortools.constraintsolver.FirstSolutionStrategy;
import com.google.ortools.constraintsolver.IntVar;
import com.google.ortools.constraintsolver.IntervalVar;
import com.google.ortools.constraintsolver.IntervalVarElement;
import com.google.ortools.constraintsolver.RoutingDimension;
import com.google.ortools.constraintsolver.RoutingIndexManager;
import com.google.ortools.constraintsolver.RoutingModel;
import com.google.ortools.constraintsolver.RoutingSearchParameters;
import com.google.ortools.constraintsolver.Solver;
import com.google.ortools.constraintsolver.main;
import java.util.logging.Logger;
// [END import]

/** Minimal VRP with breaks. */
public final class VrpBreaks {
  private static final Logger logger = Logger.getLogger(VrpBreaks.class.getName());

  // [START data_model]
  static class DataModel {
    public final long[][] timeMatrix = {
        {0, 27, 38, 34, 29, 13, 25, 9, 15, 9, 26, 25, 19, 17, 23, 38, 33},
        {27, 0, 34, 15, 9, 25, 36, 17, 34, 37, 54, 29, 24, 33, 50, 43, 60},
        {38, 34, 0, 49, 43, 25, 13, 40, 23, 37, 20, 63, 58, 56, 39, 77, 37},
        {34, 15, 49, 0, 5, 32, 43, 25, 42, 44, 61, 25, 31, 41, 58, 28, 67},
        {29, 9, 43, 5, 0, 26, 38, 19, 36, 38, 55, 20, 25, 35, 52, 33, 62},
        {13, 25, 25, 32, 26, 0, 11, 15, 9, 12, 29, 38, 33, 31, 25, 52, 35},
        {25, 36, 13, 43, 38, 11, 0, 26, 9, 23, 17, 50, 44, 42, 25, 63, 24},
        {9, 17, 40, 25, 19, 15, 26, 0, 17, 19, 36, 23, 17, 16, 33, 37, 42},
        {15, 34, 23, 42, 36, 9, 9, 17, 0, 13, 19, 40, 34, 33, 16, 54, 25},
        {9, 37, 37, 44, 38, 12, 23, 19, 13, 0, 17, 26, 21, 19, 13, 40, 23},
        {26, 54, 20, 61, 55, 29, 17, 36, 19, 17, 0, 43, 38, 36, 19, 57, 17},
        {25, 29, 63, 25, 20, 38, 50, 23, 40, 26, 43, 0, 5, 15, 32, 13, 42},
        {19, 24, 58, 31, 25, 33, 44, 17, 34, 21, 38, 5, 0, 9, 26, 19, 36},
        {17, 33, 56, 41, 35, 31, 42, 16, 33, 19, 36, 15, 9, 0, 17, 21, 26},
        {23, 50, 39, 58, 52, 25, 25, 33, 16, 13, 19, 32, 26, 17, 0, 38, 9},
        {38, 43, 77, 28, 33, 52, 63, 37, 54, 40, 57, 13, 19, 21, 38, 0, 39},
        {33, 60, 37, 67, 62, 35, 24, 42, 25, 23, 17, 42, 36, 26, 9, 39, 0},
    };
    public final long[] serviceTime = {
        0,
        15,
        15,
        15,
        15,
        15,
        15,
        15,
        15,
        15,
        15,
        15,
        15,
        15,
        15,
        15,
        15,
    };
    public final int vehicleNumber = 4;
    public final int depot = 0;
  }
  // [END data_model]

  // [START solution_printer]
  /// @brief Print the solution.
  static void printSolution(
      RoutingModel routing, RoutingIndexManager manager, Assignment solution) {
    logger.info("Objective: " + solution.objectiveValue());

    logger.info("Breaks:");
    AssignmentIntervalContainer intervals = solution.intervalVarContainer();
    for (int i = 0; i < intervals.size(); ++i) {
      IntervalVarElement breakInterval = intervals.element(i);
      if (breakInterval.performedValue() == 1) {
        logger.info(breakInterval.var().name() + " " + breakInterval);
      } else {
        logger.info(breakInterval.var().name() + ": Unperformed");
      }
    }

    long totalTime = 0;
    RoutingDimension timeDimension = routing.getMutableDimension("Time");
    for (int i = 0; i < manager.getNumberOfVehicles(); ++i) {
      logger.info("Route for Vehicle " + i + ":");
      long index = routing.start(i);
      String route = "";
      while (!routing.isEnd(index)) {
        IntVar timeVar = timeDimension.cumulVar(index);
        route += manager.indexToNode(index) + " Time(" + solution.value(timeVar) + ") -> ";
        index = solution.value(routing.nextVar(index));
      }
      IntVar timeVar = timeDimension.cumulVar(index);
      route += manager.indexToNode(index) + " Time(" + solution.value(timeVar) + ")";
      logger.info(route);
      logger.info("Time of the route: " + solution.value(timeVar) + "min");
      totalTime += solution.value(timeVar);
    }
    logger.info("Total time of all roues: " + totalTime + "min");
  }
  // [END solution_printer]

  public static void main(String[] args) {
    Loader.loadNativeLibraries();
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
          return data.timeMatrix[fromNode][toNode] + data.serviceTime[fromNode];
        });
    // [END transit_callback]

    // Define cost of each arc.
    // [START arc_cost]
    routing.setArcCostEvaluatorOfAllVehicles(transitCallbackIndex);
    // [END arc_cost]

    // Add Time constraint.
    // [START time_constraint]
    routing.addDimension(transitCallbackIndex, 10, 180,
        true, // start cumul to zero
        "Time");
    RoutingDimension timeDimension = routing.getMutableDimension("Time");
    timeDimension.setGlobalSpanCostCoefficient(10);
    // [END time_constraint]

    // Add Breaks
    long[] serviceTimes = new long[(int) routing.size()];
    for (int index = 0; index < routing.size(); index++) {
      int node = manager.indexToNode(index);
      serviceTimes[index] = data.serviceTime[node];
    }

    Solver solver = routing.solver();
    for (int vehicle = 0; vehicle < manager.getNumberOfVehicles(); ++vehicle) {
      IntervalVar[] breakIntervals = new IntervalVar[1];
      IntervalVar breakInterval = solver.makeFixedDurationIntervalVar(50, // start min
          60, // start max
          10, // duration: 10min
          false, // optional: no
          "Break for vehicle " + vehicle);
      breakIntervals[0] = breakInterval;

      timeDimension.setBreakIntervalsOfVehicle(breakIntervals, vehicle, serviceTimes);
    }

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
    if (solution != null) {
      printSolution(routing, manager, solution);
    } else {
      logger.info("Solution not found.");
    }
    // [END print_solution]
  }

  private VrpBreaks() {}
}
// [END program]
