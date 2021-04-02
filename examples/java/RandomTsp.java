// Copyright 2010-2021 Google LLC
//
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
import com.google.ortools.constraintsolver.RoutingIndexManager;
import com.google.ortools.constraintsolver.RoutingModel;
import com.google.ortools.constraintsolver.RoutingSearchParameters;
import com.google.ortools.constraintsolver.main;
// import java.io.*;
// import java.text.*;
// import java.util.*;
import java.util.Random;
import java.util.function.LongBinaryOperator;
import java.util.function.LongUnaryOperator;
import java.util.logging.Logger;

public class RandomTsp {
  private static Logger logger = Logger.getLogger(RandomTsp.class.getName());

  static class RandomManhattan implements LongBinaryOperator {
    public RandomManhattan(RoutingIndexManager manager, int size, int seed) {
      this.xs = new int[size];
      this.ys = new int[size];
      this.indexManager = manager;
      Random generator = new Random(seed);
      for (int i = 0; i < size; ++i) {
        xs[i] = generator.nextInt(1000);
        ys[i] = generator.nextInt(1000);
      }
    }

    public long applyAsLong(long firstIndex, long secondIndex) {
      int firstNode = indexManager.indexToNode(firstIndex);
      int secondNode = indexManager.indexToNode(secondIndex);
      return Math.abs(xs[firstNode] - xs[secondNode]) + Math.abs(ys[firstNode] - ys[secondNode]);
    }

    private int[] xs;
    private int[] ys;
    private RoutingIndexManager indexManager;
  }

  static class ConstantCallback implements LongUnaryOperator {
    public long applyAsLong(long index) {
      return 1;
    }
  }

  static void solve(int size, int forbidden, int seed) {
    RoutingIndexManager manager = new RoutingIndexManager(size, 1, 0);
    RoutingModel routing = new RoutingModel(manager);

    // Setting the cost function.
    // Put a permanent callback to the distance accessor here. The callback
    // has the following signature: ResultCallback2<int64_t, int64_t, int64_t>.
    // The two arguments are the from and to node inidices.
    LongBinaryOperator distances = new RandomManhattan(manager, size, seed);
    routing.setArcCostEvaluatorOfAllVehicles(routing.registerTransitCallback(distances));

    // Forbid node connections (randomly).
    Random randomizer = new Random();
    long forbidden_connections = 0;
    while (forbidden_connections < forbidden) {
      long from = randomizer.nextInt(size - 1);
      long to = randomizer.nextInt(size - 1) + 1;
      if (routing.nextVar(from).contains(to)) {
        logger.info("Forbidding connection " + from + " -> " + to);
        routing.nextVar(from).removeValue(to);
        ++forbidden_connections;
      }
    }

    // Add dummy dimension to test API.
    routing.addDimension(routing.registerUnaryTransitCallback(new ConstantCallback()), size + 1,
        size + 1, true, "dummy");

    // Solve, returns a solution if any (owned by RoutingModel).
    RoutingSearchParameters search_parameters =
        RoutingSearchParameters.newBuilder()
            .mergeFrom(main.defaultRoutingSearchParameters())
            .setFirstSolutionStrategy(FirstSolutionStrategy.Value.PATH_CHEAPEST_ARC)
            .build();

    Assignment solution = routing.solveWithParameters(search_parameters);
    if (solution != null) {
      // Solution cost.
      logger.info("Objective : " + solution.objectiveValue());
      // Inspect solution.
      // Only one route here; otherwise iterate from 0 to routing.vehicles() - 1
      int route_number = 0;
      String route = "";
      for (long node = routing.start(route_number); !routing.isEnd(node);
           node = solution.value(routing.nextVar(node))) {
        route += "" + node + " -> ";
      }
      logger.info(route + "0");
    }
  }

  public static void main(String[] args) throws Exception {
    Loader.loadNativeLibraries();
    int size = 10;
    if (args.length > 0) {
      size = Integer.parseInt(args[0]);
    }

    int forbidden = 0;
    if (args.length > 1) {
      forbidden = Integer.parseInt(args[1]);
    }

    int seed = 0;
    if (args.length > 2) {
      seed = Integer.parseInt(args[2]);
    }

    solve(size, forbidden, seed);
  }
}
