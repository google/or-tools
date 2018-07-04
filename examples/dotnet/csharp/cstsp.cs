// Copyright 2010-2017 Google
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

using System;
using System.Collections.Generic;
using Google.OrTools.ConstraintSolver;

class Tsp
{
  class RandomManhattan : NodeEvaluator2 {
    public RandomManhattan(int size, int seed)
    {
      this.xs_ = new int[size];
      this.ys_ = new int[size];
      Random generator = new Random(seed);
      for (int i = 0; i < size; ++i)
      {
        xs_[i] = generator.Next(1000);
        ys_[i] = generator.Next(1000);
      }
    }

    public override long Run(int first_index, int second_index) {
      return Math.Abs(xs_[first_index] - xs_[second_index]) +
          Math.Abs(ys_[first_index] - ys_[second_index]);
    }

    private int[] xs_;
    private int[] ys_;
  };

  class ConstantCallback : NodeEvaluator2 {
    public override long Run(int first_index, int second_index) {
      return 1;
    }
  };

  static void Solve(int size, int forbidden, int seed)
  {
    RoutingModel routing = new RoutingModel(size, 1, 0);

    // Setting the cost function.
    // Put a permanent callback to the distance accessor here. The callback
    // has the following signature: ResultCallback2<int64, int64, int64>.
    // The two arguments are the from and to node inidices.
    RandomManhattan distances = new RandomManhattan(size, seed);
    routing.SetCost(distances);

    // Forbid node connections (randomly).
    Random randomizer = new Random();
    long forbidden_connections = 0;
    while (forbidden_connections < forbidden) {
      long from = randomizer.Next(size - 1);
      long to = randomizer.Next(size - 1) + 1;
      if (routing.NextVar(from).Contains(to)) {
        Console.WriteLine("Forbidding connection {0} -> {1}", from, to);
        routing.NextVar(from).RemoveValue(to);
        ++forbidden_connections;
      }
    }

    // Add dummy dimension to test API.
    routing.AddDimension(new ConstantCallback(),
                         size + 1,
                         size + 1,
                         true,
                         "dummy");

    // Solve, returns a solution if any (owned by RoutingModel).
    RoutingSearchParameters search_parameters =
        RoutingModel.DefaultSearchParameters();
    // Setting first solution heuristic (cheapest addition).
    search_parameters.FirstSolutionStrategy =
        FirstSolutionStrategy.Types.Value.PathCheapestArc;

    Assignment solution = routing.SolveWithParameters(search_parameters);
    Console.WriteLine("Status = {0}", routing.Status());
    if (solution != null) {
      // Solution cost.
      Console.WriteLine("Cost = {0}", solution.ObjectiveValue());
      // Inspect solution.
      // Only one route here; otherwise iterate from 0 to routing.vehicles() - 1
      int route_number = 0;
      for (long node = routing.Start(route_number);
           !routing.IsEnd(node);
           node = solution.Value(routing.NextVar(node))) {
        Console.Write("{0} -> ", node);
      }
      Console.WriteLine("0");
    }
  }

  public static void Main(String[] args)
  {
    int size = 10;
    if (args.Length > 0) {
      size = Convert.ToInt32(args[0]);
    }
    int forbidden = 0;
    if (args.Length > 0) {
      forbidden = Convert.ToInt32(args[1]);
    }
    int seed = 0;
    if (args.Length > 2) {
      seed = Convert.ToInt32(args[2]);
    }

    Solve(size, forbidden, seed);
  }
}
