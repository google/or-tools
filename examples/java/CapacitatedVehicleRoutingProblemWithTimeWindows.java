// Copyright 2010-2025 Google LLC
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

package com.google.ortools.java;

import com.google.ortools.Loader;
import com.google.ortools.constraintsolver.Assignment;
import com.google.ortools.constraintsolver.FirstSolutionStrategy;
import com.google.ortools.constraintsolver.IntVar;
import com.google.ortools.constraintsolver.RoutingDimension;
import com.google.ortools.constraintsolver.RoutingIndexManager;
import com.google.ortools.constraintsolver.RoutingModel;
import com.google.ortools.constraintsolver.RoutingSearchParameters;
import com.google.ortools.constraintsolver.RoutingSearchStatus;
import com.google.ortools.constraintsolver.main;
import java.util.ArrayList;
import java.util.List;
import java.util.Random;
import java.util.function.LongBinaryOperator;
import java.util.function.LongUnaryOperator;
import java.util.logging.Logger;

/**
 * Sample showing how to model and solve a capacitated vehicle routing problem with time windows
 * using the swig-wrapped version of the vehicle routing library in
 * //ortools/constraint_solver.
 */
public class CapacitatedVehicleRoutingProblemWithTimeWindows {
  private static final Logger logger =
      Logger.getLogger(CapacitatedVehicleRoutingProblemWithTimeWindows.class.getName());

  // A pair class
  static class Pair<K, V> {
    final K first;
    final V second;

    public static <K, V> Pair<K, V> of(K element0, V element1) {
      return new Pair<K, V>(element0, element1);
    }

    public Pair(K element0, V element1) {
      this.first = element0;
      this.second = element1;
    }
  }

  static class DataModel {
    // Locations representing either an order location or a vehicle route
    // start/end.
    public final List<Pair<Integer, Integer>> locations = new ArrayList<>();

    // Quantity to be picked up for each order.
    public final List<Integer> orderDemands = new ArrayList<>();
    // Time window in which each order must be performed.
    public final List<Pair<Integer, Integer>> orderTimeWindows = new ArrayList<>();
    // Penalty cost "paid" for dropping an order.
    public final List<Integer> orderPenalties = new ArrayList<>();

    public final int numberOfVehicles = 20;

    // Capacity of the vehicles.
    public final int vehicleCapacity = 50;
    public final int numberOfOrders = 100;
    // Latest time at which each vehicle must end its tour.
    public final List<Integer> vehicleEndTime = new ArrayList<>();
    // Cost per unit of distance of each vehicle.
    public final List<Integer> vehicleCostCoefficients = new ArrayList<>();
    // Vehicle start and end indices. They have to be implemented as int[] due
    // to the available SWIG-ed interface.
    public int[] vehicleStarts;
    public int[] vehicleEnds;

    // Random number generator to produce data.
    private final Random randomGenerator = new Random(0xBEEF);

    /**
     * Creates order data. Location of the order is random, as well as its demand (quantity), time
     * window and penalty.
     *
     * @param xMax maximum x coordinate in which orders are located.
     * @param yMax maximum y coordinate in which orders are located.
     * @param demandMax maximum quantity of a demand.
     * @param timeWindowMax maximum starting time of the order time window.
     * @param timeWindowWidth duration of the order time window.
     * @param penaltyMin minimum pernalty cost if order is dropped.
     * @param penaltyMax maximum pernalty cost if order is dropped.
     */
    private void buildOrders(int xMax, int yMax, int demandMax, int timeWindowMax,
        int timeWindowWidth, int penaltyMin, int penaltyMax) {
      for (int order = 0; order < numberOfOrders; ++order) {
        locations.add(
            Pair.of(randomGenerator.nextInt(xMax + 1), randomGenerator.nextInt(yMax + 1)));
        orderDemands.add(randomGenerator.nextInt(demandMax + 1));
        int timeWindowStart = randomGenerator.nextInt(timeWindowMax + 1);
        orderTimeWindows.add(Pair.of(timeWindowStart, timeWindowStart + timeWindowWidth));
        orderPenalties.add(randomGenerator.nextInt(penaltyMax - penaltyMin + 1) + penaltyMin);
      }
    }

    /**
     * Creates fleet data. Vehicle starting and ending locations are random, as well as vehicle
     * costs per distance unit.
     *
     * @param xMax maximum x coordinate in which orders are located.
     * @param yMax maximum y coordinate in which orders are located.
     * @param endTime latest end time of a tour of a vehicle.
     * @param costCoefficientMax maximum cost per distance unit of a vehicle (minimum is 1),
     */
    private void buildFleet(int xMax, int yMax, int endTime, int costCoefficientMax) {
      vehicleStarts = new int[numberOfVehicles];
      vehicleEnds = new int[numberOfVehicles];
      for (int vehicle = 0; vehicle < numberOfVehicles; ++vehicle) {
        vehicleStarts[vehicle] = locations.size();
        locations.add(
            Pair.of(randomGenerator.nextInt(xMax + 1), randomGenerator.nextInt(yMax + 1)));
        vehicleEnds[vehicle] = locations.size();
        locations.add(
            Pair.of(randomGenerator.nextInt(xMax + 1), randomGenerator.nextInt(yMax + 1)));
        vehicleEndTime.add(endTime);
        vehicleCostCoefficients.add(randomGenerator.nextInt(costCoefficientMax) + 1);
      }
    }

    public DataModel() {
      final int xMax = 20;
      final int yMax = 20;
      final int demandMax = 3;
      final int timeWindowMax = 24 * 60;
      final int timeWindowWidth = 4 * 60;
      final int penaltyMin = 50;
      final int penaltyMax = 100;
      final int endTime = 24 * 60;
      final int costCoefficientMax = 3;
      buildOrders(xMax, yMax, demandMax, timeWindowMax, timeWindowWidth, penaltyMin, penaltyMax);
      buildFleet(xMax, yMax, endTime, costCoefficientMax);
    }
  } // DataModel

  /**
   * Creates a Manhattan Distance evaluator with 'costCoefficient'.
   *
   * @param data Data Model.
   * @param manager Node Index Manager.
   * @param costCoefficient The coefficient to apply to the evaluator.
   */
  private static LongBinaryOperator buildManhattanCallback(
      DataModel data, RoutingIndexManager manager, int costCoefficient) {
    return new LongBinaryOperator() {
      @Override
      public long applyAsLong(long fromIndex, long toIndex) {
        try {
          int fromNode = manager.indexToNode(fromIndex);
          int toNode = manager.indexToNode(toIndex);
          Pair<Integer, Integer> firstLocation = data.locations.get(fromNode);
          Pair<Integer, Integer> secondLocation = data.locations.get(toNode);
          return (long) costCoefficient
              * (Math.abs(firstLocation.first - secondLocation.first)
                  + Math.abs(firstLocation.second - secondLocation.second));
        } catch (Throwable throwed) {
          logger.warning(throwed.getMessage());
          return 0;
        }
      }
    };
  }

  // Print the solution.
  static void printSolution(
      DataModel data, RoutingModel model, RoutingIndexManager manager, Assignment solution) {
    RoutingSearchStatus.Value status = model.status();
    logger.info("Status: " + status);
    if (status != RoutingSearchStatus.Value.ROUTING_OPTIMAL
        && status != RoutingSearchStatus.Value.ROUTING_SUCCESS) {
      logger.warning("No solution found!");
      return;
    }

    // Solution cost.
    logger.info("Objective : " + solution.objectiveValue());
    // Dropped orders
    String dropped = "";
    for (int order = 0; order < data.numberOfOrders; ++order) {
      if (solution.value(model.nextVar(order)) == order) {
        dropped += " " + order;
      }
    }
    if (dropped.length() > 0) {
      logger.info("Dropped orders:" + dropped);
    }

    // Routes
    for (int vehicle = 0; vehicle < data.numberOfVehicles; ++vehicle) {
      if (!model.isVehicleUsed(solution, vehicle)) {
        continue;
      }
      long index = model.start(vehicle);
      logger.info("Route for Vehicle " + vehicle + ":");
      String route = "";
      RoutingDimension capacityDimension = model.getMutableDimension("capacity");
      RoutingDimension timeDimension = model.getMutableDimension("time");
      while (!model.isEnd(index)) {
        IntVar load = capacityDimension.cumulVar(index);
        IntVar time = timeDimension.cumulVar(index);
        long nodeIndex = manager.indexToNode(index);
        route += nodeIndex + " Load(" + solution.value(load) + ")";
        route += " Time(" + solution.min(time) + ", " + solution.max(time) + ") -> ";
        index = solution.value(model.nextVar(index));
      }
      IntVar load = capacityDimension.cumulVar(index);
      IntVar time = timeDimension.cumulVar(index);
      long nodeIndex = manager.indexToNode(index);
      route += nodeIndex + " Load(" + solution.value(load) + ")";
      route += " Time(" + solution.min(time) + ", " + solution.max(time) + ")";
      logger.info(route);
    }
  }

  public static void main(String[] args) {
    Loader.loadNativeLibraries();

    // Instantiate the data problem.
    final DataModel data = new DataModel();

    // Create Routing Index Manager
    RoutingIndexManager manager = new RoutingIndexManager(
        data.locations.size(), data.numberOfVehicles, data.vehicleStarts, data.vehicleEnds);
    RoutingModel model = new RoutingModel(manager);

    // Setting up dimensions
    final int bigNumber = 100000;
    final LongBinaryOperator callback = buildManhattanCallback(data, manager, 1);
    boolean unused = model.addDimension(
        model.registerTransitCallback(callback), bigNumber, bigNumber, false, "time");
    RoutingDimension timeDimension = model.getMutableDimension("time");

    LongUnaryOperator demandCallback = new LongUnaryOperator() {
      @Override
      public long applyAsLong(long fromIndex) {
        try {
          int fromNode = manager.indexToNode(fromIndex);
          if (fromNode < data.numberOfOrders) {
            return data.orderDemands.get(fromNode);
          }
          return 0;
        } catch (Throwable throwed) {
          logger.warning(throwed.getMessage());
          return 0;
        }
      }
    };
    unused = model.addDimension(model.registerUnaryTransitCallback(demandCallback), 0,
        data.vehicleCapacity, true, "capacity");

    // Setting up vehicles
    LongBinaryOperator[] callbacks = new LongBinaryOperator[data.numberOfVehicles];
    for (int vehicle = 0; vehicle < data.numberOfVehicles; ++vehicle) {
      final int costCoefficient = data.vehicleCostCoefficients.get(vehicle);
      callbacks[vehicle] = buildManhattanCallback(data, manager, costCoefficient);
      final int vehicleCost = model.registerTransitCallback(callbacks[vehicle]);
      model.setArcCostEvaluatorOfVehicle(vehicleCost, vehicle);
      timeDimension.cumulVar(model.end(vehicle)).setMax(data.vehicleEndTime.get(vehicle));
    }

    // Setting up orders
    for (int order = 0; order < data.numberOfOrders; ++order) {
      timeDimension.cumulVar(order).setRange(
          data.orderTimeWindows.get(order).first, data.orderTimeWindows.get(order).second);
      long[] orderIndices = {manager.nodeToIndex(order)};
      int unusedNested = model.addDisjunction(orderIndices, data.orderPenalties.get(order));
    }

    // Solving
    RoutingSearchParameters parameters =
        main.defaultRoutingSearchParameters()
            .toBuilder()
            .setFirstSolutionStrategy(FirstSolutionStrategy.Value.ALL_UNPERFORMED)
            .build();

    Assignment solution = model.solveWithParameters(parameters);

    // Print solution on console.
    printSolution(data, model, manager, solution);
  }

  private CapacitatedVehicleRoutingProblemWithTimeWindows() {}
}
