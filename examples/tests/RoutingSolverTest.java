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

package com.google.ortools.constraintsolver;

import static com.google.common.truth.Truth.assertThat;
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertNotNull;
import static org.junit.jupiter.api.Assertions.assertTrue;

import com.google.auto.value.AutoValue;
import com.google.ortools.Loader;
import com.google.ortools.constraintsolver.Assignment;
import com.google.ortools.constraintsolver.FirstSolutionStrategy;
import com.google.ortools.constraintsolver.RoutingIndexManager;
import com.google.ortools.constraintsolver.RoutingModel;
import com.google.ortools.constraintsolver.RoutingModelParameters;
import com.google.ortools.constraintsolver.RoutingSearchParameters;
import com.google.protobuf.Duration;
import java.util.ArrayList;
import java.util.function.LongBinaryOperator;
import java.util.function.LongUnaryOperator;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

/** Tests the Routing java interface. */
public final class RoutingSolverTest {
  @AutoValue
  abstract static class Location {
    static Location create(Integer latitude, Integer longitude) {
      return new AutoValue_RoutingSolverTest_Location(latitude, longitude);
    }
    abstract Integer latitude();
    abstract Integer longitude();
  }
  private ArrayList<Location> coordinates;

  @BeforeEach
  public void setUp() {
    Loader.loadNativeLibraries();
    coordinates = new ArrayList<>();
    coordinates.add(Location.create(0, 0));
    coordinates.add(Location.create(-1, 0));
    coordinates.add(Location.create(-1, 2));
    coordinates.add(Location.create(2, 1));
    coordinates.add(Location.create(1, 0));
  }

  public LongBinaryOperator createManhattanCostCallback(RoutingIndexManager manager) {
    return (long i, long j) -> {
      final int firstIndex = manager.indexToNode(i);
      final int secondIndex = manager.indexToNode(j);
      final Location firstCoordinate = coordinates.get(firstIndex);
      final Location secondCoordinate = coordinates.get(secondIndex);
      return (long) Math.abs(firstCoordinate.latitude() - secondCoordinate.latitude())
          + Math.abs(firstCoordinate.longitude() - secondCoordinate.longitude());
    };
  }

  public LongUnaryOperator createUnaryCostCallback(RoutingIndexManager manager) {
    return (long fromIndex) -> {
      final int fromNode = manager.indexToNode(fromIndex);
      final Location firstCoordinate = coordinates.get(fromNode);
      return (long) Math.abs(firstCoordinate.latitude()) + Math.abs(firstCoordinate.longitude());
    };
  }

  public LongBinaryOperator createReturnOneCallback() {
    return (long i, long j) -> 1;
  }

  @Test
  public void testRoutingModel_checkGlobalRefGuard() {
    for (int i = 0; i < 500; ++i) {
      final RoutingIndexManager manager = new RoutingIndexManager(coordinates.size(), 1, 0);
      assertNotNull(manager);
      final RoutingModel model = new RoutingModel(manager);
      assertNotNull(model);
      LongBinaryOperator transit = (long fromIndex, long toIndex) -> {
        final int fromNode = manager.indexToNode(fromIndex);
        final int toNode = manager.indexToNode(toIndex);
        return (long) Math.abs(toNode - fromNode);
      };
      model.registerTransitCallback(transit);
      System.gc(); // model should keep alive the callback
    }
  }

  @Test
  public void testRoutingIndexManager() {
    final RoutingIndexManager manager = new RoutingIndexManager(42, 3, 7);
    assertNotNull(manager);
    assertEquals(42, manager.getNumberOfNodes());
    assertEquals(3, manager.getNumberOfVehicles());
    assertEquals(42 + 3 * 2 - 1, manager.getNumberOfIndices());
    for (int i = 0; i < manager.getNumberOfVehicles(); ++i) {
      assertEquals(7, manager.indexToNode(manager.getStartIndex(i)));
      assertEquals(7, manager.indexToNode(manager.getEndIndex(i)));
    }
  }

  @Test
  public void testRoutingIndexManager_multiDepotSame() {
    final RoutingIndexManager manager =
        new RoutingIndexManager(42, 3, new int[] {7, 7, 7}, new int[] {7, 7, 7});
    assertNotNull(manager);
    assertEquals(42, manager.getNumberOfNodes());
    assertEquals(3, manager.getNumberOfVehicles());
    assertEquals(42 + 3 * 2 - 1, manager.getNumberOfIndices());
    for (int i = 0; i < manager.getNumberOfVehicles(); ++i) {
      assertEquals(7, manager.indexToNode(manager.getStartIndex(i)));
      assertEquals(7, manager.indexToNode(manager.getEndIndex(i)));
    }
  }

  @Test
  public void testRoutingIndexManager_multiDepotAllDifferent() {
    final RoutingIndexManager manager =
        new RoutingIndexManager(42, 3, new int[] {1, 2, 3}, new int[] {4, 5, 6});
    assertNotNull(manager);
    assertEquals(42, manager.getNumberOfNodes());
    assertEquals(3, manager.getNumberOfVehicles());
    assertEquals(42, manager.getNumberOfIndices());
    for (int i = 0; i < manager.getNumberOfVehicles(); ++i) {
      assertEquals(i + 1, manager.indexToNode(manager.getStartIndex(i)));
      assertEquals(i + 4, manager.indexToNode(manager.getEndIndex(i)));
    }
  }

  @Test
  public void testRoutingModel() {
    final RoutingIndexManager manager =
        new RoutingIndexManager(42, 3, new int[] {1, 2, 3}, new int[] {4, 5, 6});
    assertNotNull(manager);
    final RoutingModel model = new RoutingModel(manager);
    assertNotNull(model);
    for (int i = 0; i < manager.getNumberOfVehicles(); ++i) {
      assertEquals(i + 1, manager.indexToNode(model.start(i)));
      assertEquals(i + 4, manager.indexToNode(model.end(i)));
    }
  }

  @Test
  public void testRoutingModelParameters() {
    final RoutingModelParameters parameters = main.defaultRoutingModelParameters();
    final RoutingIndexManager manager = new RoutingIndexManager(coordinates.size(), 1, 0);
    final RoutingModel model = new RoutingModel(manager, parameters);
    assertEquals(1, model.vehicles());
  }

  @Test
  public void testRoutingModel_solveWithParameters() {
    final RoutingIndexManager manager = new RoutingIndexManager(coordinates.size(), 1, 0);
    final RoutingModel model = new RoutingModel(manager);
    final RoutingSearchParameters parameters = main.defaultRoutingSearchParameters();
    final LongBinaryOperator callback = createManhattanCostCallback(manager);
    final int cost = model.registerTransitCallback(callback);
    System.gc();
    model.setArcCostEvaluatorOfAllVehicles(cost);

    assertEquals(RoutingModel.ROUTING_NOT_SOLVED, model.status());
    Assignment solution = model.solveWithParameters(parameters);
    assertEquals(10, solution.objectiveValue());
    solution = model.solveFromAssignmentWithParameters(solution, parameters);
    assertEquals(10, solution.objectiveValue());
  }

  @Test
  public void testRoutingModel_costsAndSolve() {
    final RoutingIndexManager manager = new RoutingIndexManager(coordinates.size(), 1, 0);
    final RoutingModel model = new RoutingModel(manager);
    assertEquals(5, model.nodes());
    final LongBinaryOperator callback = createManhattanCostCallback(manager);
    final int cost = model.registerTransitCallback(callback);
    System.gc();
    model.setArcCostEvaluatorOfAllVehicles(cost);

    assertEquals(RoutingModel.ROUTING_NOT_SOLVED, model.status());
    final Assignment solution = model.solve(null);
    assertEquals(RoutingModel.ROUTING_SUCCESS, model.status());
    assertNotNull(solution);
    assertEquals(10, solution.objectiveValue());
  }

  @Test
  public void testRoutingModel_matrixTransitOwnership() {
    final RoutingIndexManager manager = new RoutingIndexManager(coordinates.size(), 1, 0);
    final RoutingModel model = new RoutingModel(manager);
    assertEquals(5, model.nodes());
    final long[][] matrix = {
        {0, 1, 3, 3, 1},
        {1, 0, 2, 4, 2},
        {3, 2, 0, 4, 4},
        {3, 4, 4, 0, 2},
        {1, 2, 4, 2, 0},
    };
    final int cost = model.registerTransitMatrix(matrix);
    System.gc(); // model should keep alive the callback
    model.setArcCostEvaluatorOfAllVehicles(cost);

    assertEquals(RoutingModel.ROUTING_NOT_SOLVED, model.status());
    final Assignment solution = model.solve(null);
    assertEquals(RoutingModel.ROUTING_SUCCESS, model.status());
    assertNotNull(solution);
    assertEquals(10, solution.objectiveValue());
  }

  @Test
  public void testRoutingModel_transitCallbackOwnership() {
    final RoutingIndexManager manager = new RoutingIndexManager(coordinates.size(), 1, 0);
    final RoutingModel model = new RoutingModel(manager);
    assertEquals(5, model.nodes());
    final int cost = model.registerTransitCallback(createManhattanCostCallback(manager));
    System.gc(); // model should keep alive the callback
    model.setArcCostEvaluatorOfAllVehicles(cost);

    assertEquals(RoutingModel.ROUTING_NOT_SOLVED, model.status());
    final Assignment solution = model.solve(null);
    assertEquals(RoutingModel.ROUTING_SUCCESS, model.status());
    assertNotNull(solution);
    assertEquals(10, solution.objectiveValue());
  }

  @Test
  public void testRoutingModel_lambdaTransitCallbackOwnership() {
    final RoutingIndexManager manager = new RoutingIndexManager(coordinates.size(), 1, 0);
    final RoutingModel model = new RoutingModel(manager);
    assertEquals(5, model.nodes());
    final int cost = model.registerTransitCallback(
      (long fromIndex, long toIndex) -> {
        final int fromNode = manager.indexToNode(fromIndex);
        final int toNode = manager.indexToNode(toIndex);
        return (long) Math.abs(toNode - fromNode);
      });
    System.gc(); // model should keep alive the callback
    model.setArcCostEvaluatorOfAllVehicles(cost);

    assertEquals(RoutingModel.ROUTING_NOT_SOLVED, model.status());
    final Assignment solution = model.solve(null);
    assertEquals(RoutingModel.ROUTING_SUCCESS, model.status());
    assertNotNull(solution);
    assertEquals(8, solution.objectiveValue());
  }

  @Test
  public void testRoutingModel_unaryTransitVectorOwnership() {
    final RoutingIndexManager manager = new RoutingIndexManager(10, 1, 0);
    final RoutingModel model = new RoutingModel(manager);
    assertEquals(10, model.nodes());
    final long[] vector = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    final int cost = model.registerUnaryTransitVector(vector);
    System.gc(); // model should keep alive the callback
    model.setArcCostEvaluatorOfAllVehicles(cost);

    assertEquals(RoutingModel.ROUTING_NOT_SOLVED, model.status());
    final Assignment solution = model.solve(null);
    assertEquals(RoutingModel.ROUTING_SUCCESS, model.status());
    assertNotNull(solution);
    assertEquals(45, solution.objectiveValue());
  }

  @Test
  public void testRoutingModel_unaryTransitCallbackOwnership() {
    final RoutingIndexManager manager = new RoutingIndexManager(coordinates.size(), 1, 0);
    final RoutingModel model = new RoutingModel(manager);
    assertEquals(5, model.nodes());
    final int cost = model.registerUnaryTransitCallback(createUnaryCostCallback(manager));
    System.gc(); // model should keep alive the callback
    model.setArcCostEvaluatorOfAllVehicles(cost);

    assertEquals(RoutingModel.ROUTING_NOT_SOLVED, model.status());
    Assignment solution = model.solve(null);
    assertEquals(RoutingModel.ROUTING_SUCCESS, model.status());
    assertNotNull(solution);
    assertEquals(8, solution.objectiveValue());
  }

  @Test
  public void testRoutingModel_lambdaUnaryTransitCallbackOwnership() {
    final RoutingIndexManager manager = new RoutingIndexManager(coordinates.size(), 1, 0);
    final RoutingModel model = new RoutingModel(manager);
    assertEquals(5, model.nodes());
    final int cost = model.registerUnaryTransitCallback(
      (long fromIndex) -> {
        final int fromNode = manager.indexToNode(fromIndex);
        return (long) Math.abs(fromNode);
      });
    System.gc(); // model should keep alive the callback
    model.setArcCostEvaluatorOfAllVehicles(cost);

    assertEquals(RoutingModel.ROUTING_NOT_SOLVED, model.status());
    Assignment solution = model.solve(null);
    assertEquals(RoutingModel.ROUTING_SUCCESS, model.status());
    assertNotNull(solution);
    assertEquals(10, solution.objectiveValue());
  }

  @Test
  public void testRoutingModel_routesToAssignment() {
    final int vehicles = coordinates.size() - 1;
    final RoutingIndexManager manager = new RoutingIndexManager(coordinates.size(), vehicles, 0);
    final RoutingModel model = new RoutingModel(manager);
    model.closeModel();
    long[][] routes = new long[vehicles][];
    for (int i = 0; i < vehicles; ++i) {
      // Each route has a single node
      routes[i] = new long[1];
      routes[i][0] = manager.nodeToIndex(i + 1);
    }
    Assignment assignment = new Assignment(model.solver());
    model.routesToAssignment(routes, false, true, assignment);
    for (int i = 0; i < vehicles; ++i) {
      assertEquals(assignment.value(model.nextVar(model.start(i))), i + 1);
      assertEquals(assignment.value(model.nextVar(i + 1)), model.end(i));
    }
  }

  @Test
  public void testRoutingModel_addDisjunction() {
    final RoutingIndexManager manager = new RoutingIndexManager(coordinates.size(), 1, 0);
    final RoutingModel model = new RoutingModel(manager);
    final LongBinaryOperator callback = createManhattanCostCallback(manager);
    final int cost = model.registerTransitCallback(callback);
    model.setArcCostEvaluatorOfAllVehicles(cost);

    int[] a = new int[2];
    a[0] = 2;
    a[1] = 3;
    int[] b = new int[1];
    b[0] = 1;
    int[] c = new int[1];
    c[0] = 4;
    model.addDisjunction(manager.nodesToIndices(a));
    model.addDisjunction(manager.nodesToIndices(b));
    model.addDisjunction(manager.nodesToIndices(c));

    Assignment solution = model.solve(null);
    assertEquals(8, solution.objectiveValue());
  }

  @Test
  public void testRoutingModel_addConstantDimension() {
    final RoutingIndexManager manager = new RoutingIndexManager(10, 1, 0);
    final RoutingModel model = new RoutingModel(manager);
    assertEquals(10, model.nodes());
    final IntBoolPair pair = model.addConstantDimension(1,
        /*capacity=*/100,
        /*fix_start_cumul_to_zero=*/true, "Dimension");
    assertEquals(1, pair.getFirst());
    assertTrue(pair.getSecond());
    RoutingDimension dimension = model.getMutableDimension("Dimension");
    dimension.setSpanCostCoefficientForAllVehicles(2);

    RoutingSearchParameters searchParameters =
        RoutingSearchParameters.newBuilder()
            .mergeFrom(main.defaultRoutingSearchParameters())
            .setFirstSolutionStrategy(FirstSolutionStrategy.Value.PATH_CHEAPEST_ARC)
            .setTimeLimit(Duration.newBuilder().setSeconds(10))
            .build();

    assertEquals(RoutingModel.ROUTING_NOT_SOLVED, model.status());
    final Assignment solution = model.solveWithParameters(searchParameters);
    assertEquals(RoutingModel.ROUTING_SUCCESS, model.status());
    assertNotNull(solution);
    assertEquals(20, solution.objectiveValue());
  }

  @Test
  public void testRoutingModel_addVectorDimension() {
    final RoutingIndexManager manager = new RoutingIndexManager(10, 1, 0);
    final RoutingModel model = new RoutingModel(manager);
    assertEquals(10, model.nodes());
    final long[] vector = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    final IntBoolPair pair = model.addVectorDimension(vector,
        /*capacity=*/100,
        /*fix_start_cumul_to_zero=*/true, "Dimension");
    assertEquals(1, pair.getFirst());
    assertTrue(pair.getSecond());
    System.gc(); // model should keep alive the callback
    model.setArcCostEvaluatorOfAllVehicles(pair.getFirst());

    RoutingSearchParameters searchParameters =
        RoutingSearchParameters.newBuilder()
            .mergeFrom(main.defaultRoutingSearchParameters())
            .setFirstSolutionStrategy(FirstSolutionStrategy.Value.PATH_CHEAPEST_ARC)
            .setTimeLimit(Duration.newBuilder().setSeconds(10))
            .build();

    assertEquals(RoutingModel.ROUTING_NOT_SOLVED, model.status());
    final Assignment solution = model.solveWithParameters(searchParameters);
    assertEquals(RoutingModel.ROUTING_SUCCESS, model.status());
    assertNotNull(solution);
    assertEquals(45, solution.objectiveValue());
  }

  @Test
  public void testRoutingModel_addMatrixDimension() {
    final RoutingIndexManager manager = new RoutingIndexManager(5, 1, 0);
    final RoutingModel model = new RoutingModel(manager);
    assertEquals(5, model.nodes());
    final long[][] matrix = {
        {0, 1, 3, 3, 1},
        {1, 0, 2, 4, 2},
        {3, 2, 0, 4, 4},
        {3, 4, 4, 0, 2},
        {1, 2, 4, 2, 0},
    };
    final IntBoolPair pair = model.addMatrixDimension(matrix,
        /*capacity=*/100,
        /*fix_start_cumul_to_zero=*/true, "Dimension");
    assertEquals(1, pair.getFirst());
    assertTrue(pair.getSecond());
    System.gc(); // model should keep alive the callback
    model.setArcCostEvaluatorOfAllVehicles(pair.getFirst());

    final RoutingSearchParameters searchParameters =
        RoutingSearchParameters.newBuilder()
            .mergeFrom(main.defaultRoutingSearchParameters())
            .setFirstSolutionStrategy(FirstSolutionStrategy.Value.PATH_CHEAPEST_ARC)
            .setTimeLimit(Duration.newBuilder().setSeconds(10))
            .build();

    assertEquals(RoutingModel.ROUTING_NOT_SOLVED, model.status());
    final Assignment solution = model.solveWithParameters(searchParameters);
    assertEquals(RoutingModel.ROUTING_SUCCESS, model.status());
    assertNotNull(solution);
    assertEquals(10, solution.objectiveValue());
  }

  @Test
  public void testRoutingModel_addDimension() {
    final RoutingIndexManager manager = new RoutingIndexManager(coordinates.size(), 1, 0);
    final RoutingModel model = new RoutingModel(manager);
    final LongBinaryOperator manhattanCostCallback = createManhattanCostCallback(manager);
    final int cost = model.registerTransitCallback(manhattanCostCallback);
    model.setArcCostEvaluatorOfAllVehicles(cost);

    final LongBinaryOperator transit = (long firstIndex, long secondIndex) -> {
      int firstNode = manager.indexToNode(firstIndex);
      int secondNode = manager.indexToNode(secondIndex);
      if (firstNode >= coordinates.size()) {
        firstNode = 0;
      }
      if (secondNode >= coordinates.size()) {
        secondNode = 0;
      }
      long distanceTime = manhattanCostCallback.applyAsLong(firstIndex, secondIndex) * 10;
      long visitingTime = 1;
      return distanceTime + visitingTime;
    };
    assertTrue(
        model.addDimension(model.registerTransitCallback(transit), 1000, 1000, true, "time"));
    RoutingDimension dimension = model.getMutableDimension("time");

    for (int i = 1; i < coordinates.size(); i++) {
      int[] a = new int[1];
      a[0] = i;
      model.addDisjunction(manager.nodesToIndices(a), 10);

      dimension.cumulVar(i).setMin(0);
      dimension.cumulVar(i).setMax(40);
    }

    RoutingSearchParameters searchParameters =
        RoutingSearchParameters.newBuilder()
            .mergeFrom(main.defaultRoutingSearchParameters())
            .setFirstSolutionStrategy(FirstSolutionStrategy.Value.PATH_CHEAPEST_ARC)
            .setTimeLimit(Duration.newBuilder().setSeconds(10))
            .build();

    assertEquals(RoutingModel.ROUTING_NOT_SOLVED, model.status());
    Assignment solution = model.solveWithParameters(searchParameters);
    assertEquals(RoutingModel.ROUTING_SUCCESS, model.status());
    assertNotNull(solution);
    solution = model.solve(solution);
    assertNotNull(solution);
    assertTrue(solution.objectiveValue() >= 24 && solution.objectiveValue() <= 30);
    for (long i = 1; i < coordinates.size(); i++) {
      assertTrue(
          solution.min(dimension.cumulVar(i)) >= 0 && solution.max(dimension.cumulVar(i)) <= 40);
    }
  }

  @Test
  public void testRoutingModel_dimensionVehicleSpanCost() {
    final RoutingIndexManager manager = new RoutingIndexManager(2, 1, 0);
    final RoutingModel model = new RoutingModel(manager);
    final LongBinaryOperator callback = createReturnOneCallback();
    assertTrue(
        model.addDimension(model.registerTransitCallback(callback), 1000, 1000, true, "time"));
    RoutingDimension dimension = model.getMutableDimension("time");
    dimension.cumulVar(1).setMin(10);
    dimension.setSpanCostCoefficientForAllVehicles(2);
    assertEquals(2, dimension.getSpanCostCoefficientForVehicle(0));

    assertEquals(RoutingModel.ROUTING_NOT_SOLVED, model.status());
    final Assignment solution = model.solve(null);
    assertEquals(RoutingModel.ROUTING_SUCCESS, model.status());
    assertNotNull(solution);
    assertEquals(2 * (10 + 1), solution.objectiveValue());
  }

  @Test
  public void testRoutingModel_dimensionGlobalSpanCost() {
    final RoutingIndexManager manager = new RoutingIndexManager(3, 2, 0);
    final RoutingModel model = new RoutingModel(manager);
    final LongBinaryOperator callback = createReturnOneCallback();
    assertTrue(
        model.addDimension(model.registerTransitCallback(callback), 1000, 1000, false, "time"));
    RoutingDimension timeDimension = model.getMutableDimension("time");
    timeDimension.cumulVar(1).setMin(10);
    model.vehicleVar(1).setValue(0);
    timeDimension.cumulVar(2).setMax(2);
    model.vehicleVar(2).setValue(1);
    timeDimension.setGlobalSpanCostCoefficient(2);
    assertEquals(2, timeDimension.getGlobalSpanCostCoefficient());

    assertEquals(RoutingModel.ROUTING_NOT_SOLVED, model.status());
    final Assignment solution = model.solve(null);
    assertEquals(RoutingModel.ROUTING_SUCCESS, model.status());
    assertNotNull(solution);
    assertEquals(2 * (11 - 1), solution.objectiveValue());
  }

  @Test
  public void testRoutingModel_cumulVarSoftUpperBound() {
    final RoutingIndexManager manager = new RoutingIndexManager(2, 1, 0);
    final RoutingModel model = new RoutingModel(manager);
    final LongBinaryOperator callback = createReturnOneCallback();
    assertTrue(
        model.addDimension(model.registerTransitCallback(callback), 1000, 1000, false, "time"));
    RoutingDimension dimension = model.getMutableDimension("time");
    assertEquals(1000, dimension.getCumulVarSoftUpperBound(1));
    assertEquals(0, dimension.getCumulVarSoftUpperBoundCoefficient(1));
    dimension.setCumulVarSoftUpperBound(1, 5, 1);
    assertEquals(5, dimension.getCumulVarSoftUpperBound(1));
    assertEquals(1, dimension.getCumulVarSoftUpperBoundCoefficient(1));
  }

  @Test
  public void testRoutingModel_addDimensionWithVehicleCapacity() {
    final RoutingIndexManager manager = new RoutingIndexManager(1, 3, 0);
    final RoutingModel model = new RoutingModel(manager);
    final LongBinaryOperator callback = createReturnOneCallback();
    final long[] capacity = {5, 6, 7};
    model.addDimensionWithVehicleCapacity(
        model.registerTransitCallback(callback), 1000, capacity, false, "dim");
    RoutingDimension dimension = model.getMutableDimension("dim");

    assertEquals(RoutingModel.ROUTING_NOT_SOLVED, model.status());
    final Assignment solution = model.solve(null);
    assertEquals(RoutingModel.ROUTING_SUCCESS, model.status());
    assertNotNull(solution);
    for (int vehicle = 0; vehicle < 3; ++vehicle) {
      assertEquals(vehicle + 4, solution.max(dimension.cumulVar(model.start(vehicle))));
      assertEquals(vehicle + 5, solution.max(dimension.cumulVar(model.end(vehicle))));
    }
  }

  @Test
  public void testRoutingModel_addDimensionWithVehicleTransits() {
    final RoutingIndexManager manager = new RoutingIndexManager(1, 3, 0);
    final RoutingModel model = new RoutingModel(manager);
    final LongBinaryOperator[] callbacks = new LongBinaryOperator[3];
    int[] transits = new int[3];
    for (int i = 0; i < 3; ++i) {
      final int value = i + 1;
      callbacks[i] = (long firstIndex, long secondIndex) -> value;
      transits[i] = model.registerTransitCallback(callbacks[i]);
    }
    long capacity = 5;
    model.addDimensionWithVehicleTransits(transits, 1000, capacity, false, "dim");
    RoutingDimension dimension = model.getMutableDimension("dim");

    assertEquals(RoutingModel.ROUTING_NOT_SOLVED, model.status());
    final Assignment solution = model.solve(null);
    assertEquals(RoutingModel.ROUTING_SUCCESS, model.status());
    assertNotNull(solution);
    for (int vehicle = 0; vehicle < 3; ++vehicle) {
      assertEquals(
          capacity - (vehicle + 1), solution.max(dimension.cumulVar(model.start(vehicle))));
      assertEquals(capacity, solution.max(dimension.cumulVar(model.end(vehicle))));
    }
  }

  @Test
  public void testRoutingModel_addDimensionWithVehicleTransitAndCapacity() {
    final RoutingIndexManager manager = new RoutingIndexManager(1, 3, 0);
    final RoutingModel model = new RoutingModel(manager);
    final LongBinaryOperator[] callbacks = new LongBinaryOperator[3];
    final int[] transits = new int[3];
    for (int i = 0; i < 3; ++i) {
      final int value = i + 1;
      callbacks[i] = (long firstIndex, long secondIndex) -> value;
      transits[i] = model.registerTransitCallback(callbacks[i]);
    }
    long[] capacity = new long[3];
    for (int i = 0; i < 3; ++i) {
      capacity[i] = i + 5L;
    }
    model.addDimensionWithVehicleTransitAndCapacity(transits, 1000, capacity, false, "dim");
    final RoutingDimension dimension = model.getMutableDimension("dim");

    assertEquals(RoutingModel.ROUTING_NOT_SOLVED, model.status());
    final Assignment solution = model.solve(null);
    assertEquals(RoutingModel.ROUTING_SUCCESS, model.status());
    assertNotNull(solution);
    for (int vehicle = 0; vehicle < 3; ++vehicle) {
      assertEquals(4, solution.max(dimension.cumulVar(model.start(vehicle))));
      assertEquals(vehicle + 5, solution.max(dimension.cumulVar(model.end(vehicle))));
    }
  }

  @Test
  public void testRoutingModel_intVarVectorGetter() {
    final RoutingIndexManager manager = new RoutingIndexManager(coordinates.size(), 1, 0);
    final RoutingModel model = new RoutingModel(manager);
    final LongBinaryOperator manhattanCostCallback = createManhattanCostCallback(manager);
    final int cost = model.registerTransitCallback(manhattanCostCallback);
    model.setArcCostEvaluatorOfAllVehicles(cost);

    final LongBinaryOperator transit = (long firstIndex, long secondIndex) -> {
      int firstNode = manager.indexToNode(firstIndex);
      int secondNode = manager.indexToNode(secondIndex);
      if (firstNode >= coordinates.size()) {
        firstNode = 0;
      }
      if (secondNode >= coordinates.size()) {
        secondNode = 0;
      }
      long distanceTime = manhattanCostCallback.applyAsLong(firstIndex, secondIndex) * 10;
      long visitingTime = 1;
      return distanceTime + visitingTime;
    };
    assertTrue(
        model.addDimension(model.registerTransitCallback(transit), 1000, 1000, true, "time"));
    RoutingDimension timeDimension = model.getMutableDimension("time");
    IntVar[] cumuls = timeDimension.cumuls();
    assertThat(cumuls).isNotEmpty();
    IntVar[] transits = timeDimension.transits();
    assertThat(transits).isNotEmpty();
    IntVar[] slacks = timeDimension.slacks();
    assertThat(slacks).isNotEmpty();
    IntVar[] nexts = model.nexts();
    assertThat(nexts).isNotEmpty();
    IntVar[] vehicleVars = model.vehicleVars();
    assertThat(vehicleVars).isNotEmpty();
  }
}
