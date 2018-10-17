//
// Copyright 2012 Google
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
import java.io.*;
import java.util.*;
import java.text.*;

import com.google.ortools.constraintsolver.Assignment;
import com.google.ortools.constraintsolver.LongToLong;
import com.google.ortools.constraintsolver.LongLongToLong;
import com.google.ortools.constraintsolver.RoutingIndexManager;
import com.google.ortools.constraintsolver.RoutingModel;
import com.google.ortools.constraintsolver.FirstSolutionStrategy;
import com.google.ortools.constraintsolver.RoutingSearchParameters;
import com.google.ortools.constraintsolver.main;

class Tsp {
  static {
    System.loadLibrary("jniortools");
  }

  static class RandomManhattan extends LongLongToLong {
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

    @Override
    public long run(long firstIndex, long secondIndex) {
      int firstNode = indexManager.indexToNode((int) firstIndex);
      int secondNode = indexManager.indexToNode((int) secondIndex);
      return Math.abs(xs[firstNode] - xs[secondNode]) +
          Math.abs(ys[firstNode] - ys[secondNode]);
    }

    private int[] xs;
    private int[] ys;
    private RoutingIndexManager indexManager;
  }

  static class ConstantCallback extends LongToLong {
    @Override
    public long run(long index) {
      return 1;
    }
  }

  static void solve(int size, int forbidden, int seed)
  {
    RoutingIndexManager manager = new RoutingIndexManager(size, 1, 0);
    RoutingModel routing = new RoutingModel(manager);

    // Setting the cost function.
    // Put a permanent callback to the distance accessor here. The callback
    // has the following signature: ResultCallback2<int64, int64, int64>.
    // The two arguments are the from and to node inidices.
    LongLongToLong distances = new RandomManhattan(manager, size, seed);
    routing.setArcCostEvaluatorOfAllVehicles(
        routing.registerTransitCallback(distances));

    // Forbid node connections (randomly).
    Random randomizer = new Random();
    long forbidden_connections = 0;
    while (forbidden_connections < forbidden) {
      long from = randomizer.nextInt(size - 1);
      long to = randomizer.nextInt(size - 1) + 1;
      if (routing.nextVar(from).contains(to)) {
        System.out.println("Forbidding connection " + from + " -> " + to);
        routing.nextVar(from).removeValue(to);
        ++forbidden_connections;
      }
    }

    // Add dummy dimension to test API.
    routing.addDimension(
        routing.registerUnaryTransitCallback(new ConstantCallback()),
        size + 1,
        size + 1,
        true,
        "dummy");

    // Solve, returns a solution if any (owned by RoutingModel).
    RoutingSearchParameters search_parameters =
        RoutingSearchParameters.newBuilder()
        .mergeFrom(main.defaultRoutingSearchParameters())
        .setFirstSolutionStrategy(FirstSolutionStrategy.Value.PATH_CHEAPEST_ARC)
        .build();

    Assignment solution = routing.solveWithParameters(search_parameters);
    if (solution != null) {
      // Solution cost.
      System.out.println("Cost = " + solution.objectiveValue());
      // Inspect solution.
      // Only one route here; otherwise iterate from 0 to routing.vehicles() - 1
      int route_number = 0;
      for (long node = routing.start(route_number);
           !routing.isEnd(node);
           node = solution.value(routing.nextVar(node))) {
        System.out.print("" + node + " -> ");
      }
      System.out.println("0");
    }
  }

  public static void main(String[] args) throws Exception {
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
