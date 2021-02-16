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
package com.google.ortools;

import static java.lang.Math.abs;

import com.google.ortools.Loader;
import com.google.ortools.constraintsolver.Assignment;
import com.google.ortools.constraintsolver.FirstSolutionStrategy;
import com.google.ortools.constraintsolver.RoutingIndexManager;
import com.google.ortools.constraintsolver.RoutingModel;
import com.google.ortools.constraintsolver.RoutingSearchParameters;
import com.google.ortools.constraintsolver.main;
import com.google.ortools.constraintsolver.IntBoolPair;
import java.util.logging.Logger;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.ValueSource;

/** Tests the Routing java interface. */
public class RoutingSolverTest {
  private static final Logger logger = Logger.getLogger(RoutingSolverTest.class.getName());

  @Test
  public void testRoutingTransitMatrix() {
    Loader.loadNativeLibraries();
    logger.info("testRoutingTransitMatrix...");
    // Create Routing Index Manager
    RoutingIndexManager manager =
        new RoutingIndexManager(5 /*location*/, 1 /*vehicle*/, 0 /*depot*/);
    // Create Routing Model.
    RoutingModel routing = new RoutingModel(manager);
    // Define cost of each arc.
    int transitCallbackIndex;
    long[][] matrix = {
      {1, 1, 1, 1, 1},
      {1, 1, 1, 1, 1},
      {1, 1, 1, 1, 1},
      {1, 1, 1, 1, 1},
      {1, 1, 1, 1, 1},
    };
    transitCallbackIndex = routing.registerTransitMatrix(matrix);
    routing.setArcCostEvaluatorOfAllVehicles(transitCallbackIndex);
    // Setting first solution heuristic.
    RoutingSearchParameters searchParameters =
        main.defaultRoutingSearchParameters()
            .toBuilder()
            .setFirstSolutionStrategy(FirstSolutionStrategy.Value.PATH_CHEAPEST_ARC)
            .build();
    // Solve the problem.
    Assignment solution = routing.solveWithParameters(searchParameters);
    if (null == solution)
      throw new AssertionError("null == solution");
    if (5 != solution.objectiveValue())
      throw new AssertionError("5 != objective");
    logger.info("testRoutingTransitMatrix...DONE");
  }

  @ParameterizedTest
  @ValueSource(booleans = {false, true})
  public void testRoutingTransitCallback(boolean enableGC) {
    Loader.loadNativeLibraries();
    logger.info("testRoutingTransitCallback (enable gc:" + enableGC + ")...");
    // Create Routing Index Manager
    RoutingIndexManager manager =
        new RoutingIndexManager(5 /*location*/, 1 /*vehicle*/, 0 /*depot*/);
    // Create Routing Model.
    RoutingModel routing = new RoutingModel(manager);
    // Define cost of each arc.
    int transitCallbackIndex;
    if (true) {
      transitCallbackIndex = routing.registerTransitCallback((long fromIndex, long toIndex) -> {
        // Convert from routing variable Index to user NodeIndex.
        int fromNode = manager.indexToNode(fromIndex);
        int toNode = manager.indexToNode(toIndex);
        return abs(toNode - fromNode);
      });
    }
    if (enableGC) {
      System.gc();
    }
    routing.setArcCostEvaluatorOfAllVehicles(transitCallbackIndex);
    // Setting first solution heuristic.
    RoutingSearchParameters searchParameters =
        main.defaultRoutingSearchParameters()
            .toBuilder()
            .setFirstSolutionStrategy(FirstSolutionStrategy.Value.PATH_CHEAPEST_ARC)
            .build();
    // Solve the problem.
    Assignment solution = routing.solveWithParameters(searchParameters);
    if (null == solution)
      throw new AssertionError("null == solution");
    if (8 != solution.objectiveValue())
      throw new AssertionError("8 != objective");
    logger.info("testRoutingTransitCallback (enable gc:" + enableGC + ")...DONE");
  }

  @Test
  public void testRoutingMatrixDimension() {
    Loader.loadNativeLibraries();
    logger.info("testRoutingMatrixDimension...");
    // Create Routing Index Manager
    RoutingIndexManager manager =
        new RoutingIndexManager(5 /*location*/, 1 /*vehicle*/, 0 /*depot*/);
    // Create Routing Model.
    RoutingModel routing = new RoutingModel(manager);
    // Define cost of each arc.
    int transitCallbackIndex;
    long[][] matrix = {
      {1, 1, 1, 1, 1},
      {1, 1, 1, 1, 1},
      {1, 1, 1, 1, 1},
      {1, 1, 1, 1, 1},
      {1, 1, 1, 1, 1},
    };
    IntBoolPair result = routing.addMatrixDimension(
        matrix,
        /*capacity=*/10,
        /*fix_start_cumul_to_zero=*/true,
        "Dimension");
    routing.setArcCostEvaluatorOfAllVehicles(result.getFirst());
    // Setting first solution heuristic.
    RoutingSearchParameters searchParameters =
        main.defaultRoutingSearchParameters()
            .toBuilder()
            .setFirstSolutionStrategy(FirstSolutionStrategy.Value.PATH_CHEAPEST_ARC)
            .build();
    // Solve the problem.
    Assignment solution = routing.solveWithParameters(searchParameters);
    if (null == solution)
      throw new AssertionError("null == solution");
    if (5 != solution.objectiveValue())
      throw new AssertionError("5 != objective");
    logger.info("testRoutingMatrixDimension...DONE");
  }

  @Test
  public void testRoutingUnaryTransitVector() {
    Loader.loadNativeLibraries();
    logger.info("testRoutingUnaryTransitVector...");
    // Create Routing Index Manager
    RoutingIndexManager manager =
        new RoutingIndexManager(5 /*location*/, 1 /*vehicle*/, 0 /*depot*/);
    // Create Routing Model.
    RoutingModel routing = new RoutingModel(manager);
    // Define cost of each arc.
    int transitCallbackIndex;
    long[] vector = {1, 1, 1, 1, 1};
    transitCallbackIndex = routing.registerUnaryTransitVector(vector);
    routing.setArcCostEvaluatorOfAllVehicles(transitCallbackIndex);
    // Setting first solution heuristic.
    RoutingSearchParameters searchParameters =
        main.defaultRoutingSearchParameters()
            .toBuilder()
            .setFirstSolutionStrategy(FirstSolutionStrategy.Value.PATH_CHEAPEST_ARC)
            .build();
    // Solve the problem.
    Assignment solution = routing.solveWithParameters(searchParameters);
    if (null == solution)
      throw new AssertionError("null == solution");
    if (5 != solution.objectiveValue())
      throw new AssertionError("5 != objective");
    logger.info("testRoutingUnaryTransitVector...DONE");
  }

  @ParameterizedTest
  @ValueSource(booleans = {false, true})
  public void testRoutingUnaryTransitCallback(boolean enableGC) {
    Loader.loadNativeLibraries();
    logger.info("testRoutingUnaryTransitCallback (enable gc:" + enableGC + ")...");
    // Create Routing Index Manager
    RoutingIndexManager manager =
        new RoutingIndexManager(5 /*location*/, 1 /*vehicle*/, 0 /*depot*/);
    // Create Routing Model.
    RoutingModel routing = new RoutingModel(manager);
    // Define cost of each arc.
    int transitCallbackIndex;
    if (true) {
      transitCallbackIndex = routing.registerUnaryTransitCallback((long fromIndex) -> {
        // Convert from routing variable Index to user NodeIndex.
        int fromNode = manager.indexToNode(fromIndex);
        return abs(fromNode);
      });
    }
    if (enableGC) {
      System.gc();
    }
    routing.setArcCostEvaluatorOfAllVehicles(transitCallbackIndex);
    // Setting first solution heuristic.
    RoutingSearchParameters searchParameters =
        main.defaultRoutingSearchParameters()
            .toBuilder()
            .setFirstSolutionStrategy(FirstSolutionStrategy.Value.PATH_CHEAPEST_ARC)
            .build();
    // Solve the problem.
    Assignment solution = routing.solveWithParameters(searchParameters);
    if (null == solution)
      throw new AssertionError("null == solution");
    if (10 != solution.objectiveValue())
      throw new AssertionError("10 != objective");
    logger.info("testRoutingUnaryTransitCallback (enable gc:" + enableGC + ")...DONE");
  }

  @Test
  public void testRoutingVectorDimension() {
    Loader.loadNativeLibraries();
    logger.info("testRoutingVectorDimension...");
    // Create Routing Index Manager
    RoutingIndexManager manager =
        new RoutingIndexManager(5 /*location*/, 1 /*vehicle*/, 0 /*depot*/);
    // Create Routing Model.
    RoutingModel routing = new RoutingModel(manager);
    // Define cost of each arc.
    int transitCallbackIndex;
    long[] vector = {1, 1, 1, 1, 1};
    IntBoolPair result = routing.addVectorDimension(
        vector,
        /*capacity=*/10,
        /*fix_start_cumul_to_zero=*/true,
        "Dimension");
    routing.setArcCostEvaluatorOfAllVehicles(result.getFirst());
    // Setting first solution heuristic.
    RoutingSearchParameters searchParameters =
        main.defaultRoutingSearchParameters()
            .toBuilder()
            .setFirstSolutionStrategy(FirstSolutionStrategy.Value.PATH_CHEAPEST_ARC)
            .build();
    // Solve the problem.
    Assignment solution = routing.solveWithParameters(searchParameters);
    if (null == solution)
      throw new AssertionError("null == solution");
    if (5 != solution.objectiveValue())
      throw new AssertionError("5 != objective");
    logger.info("testRoutingMatrixDimension...DONE");
  }
}
