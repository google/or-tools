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

package com.google.ortools.algorithms;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertNotNull;

import com.google.ortools.Loader;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

/** Test the Knapsack solver java interface. */
public final class KnapsackSolverTest {
  @BeforeEach
  public void setUp() {
    Loader.loadNativeLibraries();
  }

  private long runKnapsackSolver(final KnapsackSolver.SolverType solverType, final long[] profits,
      final long[][] weights, final long[] capacities) {
    final KnapsackSolver solver = new KnapsackSolver(solverType, "test");
    assertNotNull(solver);
    solver.init(profits, weights, capacities);

    return solver.solve();
  }

  private void solveKnapsackProblem(final long[] profits, final long[][] weights,
      final long[] capacities, final long optimalProfit) {
    final int maxNumberOfItemsForBruteForce = 20;
    final int maxNumberOfItemsForDivideAndConquer = 32;
    final int maxNumberOfItemsFor64ItemsSolver = 64;

    {
      final long profit = runKnapsackSolver(
          KnapsackSolver.SolverType.KNAPSACK_MULTIDIMENSION_BRANCH_AND_BOUND_SOLVER, profits,
          weights, capacities);
      assertEquals(optimalProfit, profit);
    }

    // Other solvers don't support multidimension models.
    if (weights.length > 1) {
      return;
    }

    final int numOfItems = profits.length;

    if (numOfItems <= maxNumberOfItemsForBruteForce) {
      final long profit = runKnapsackSolver(
          KnapsackSolver.SolverType.KNAPSACK_BRUTE_FORCE_SOLVER, profits, weights, capacities);
      assertEquals(optimalProfit, profit);
    }

    if (numOfItems <= maxNumberOfItemsForDivideAndConquer) {
      final long profit =
          runKnapsackSolver(KnapsackSolver.SolverType.KNAPSACK_DIVIDE_AND_CONQUER_SOLVER, profits,
              weights, capacities);
      assertEquals(optimalProfit, profit);
    }

    {
      final long profit =
          runKnapsackSolver(KnapsackSolver.SolverType.KNAPSACK_DYNAMIC_PROGRAMMING_SOLVER, profits,
              weights, capacities);
      assertEquals(optimalProfit, profit);
    }

    if (numOfItems <= maxNumberOfItemsFor64ItemsSolver) {
      final long profit = runKnapsackSolver(
          KnapsackSolver.SolverType.KNAPSACK_64ITEMS_SOLVER, profits, weights, capacities);
      assertEquals(optimalProfit, profit);
    }
  }

  @Test
  public void testSolveOneDimension() {
    final long[] profits = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    final long[][] weights = {{1, 2, 3, 4, 5, 6, 7, 8, 9}};
    final long[] capacities = {34};
    final long optimalProfit = 34;
    solveKnapsackProblem(profits, weights, capacities, optimalProfit);
  }

  @Test
  public void testSolveTwoDimensions() {
    final long[] profits = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    final long[][] weights = {{1, 2, 3, 4, 5, 6, 7, 8, 9}, {1, 1, 1, 1, 1, 1, 1, 1, 1}};
    final long[] capacities = {34, 4};
    final long optimalProfit = 30;
    solveKnapsackProblem(profits, weights, capacities, optimalProfit);
  }

  @Test
  public void testSolveBigOneDimension() {
    final long[] profits = {360, 83, 59, 130, 431, 67, 230, 52, 93, 125, 670, 892, 600, 38, 48, 147,
        78, 256, 63, 17, 120, 164, 432, 35, 92, 110, 22, 42, 50, 323, 514, 28, 87, 73, 78, 15, 26,
        78, 210, 36, 85, 189, 274, 43, 33, 10, 19, 389, 276, 312};
    final long[][] weights = {{7, 0, 30, 22, 80, 94, 11, 81, 70, 64, 59, 18, 0, 36, 3, 8, 15, 42, 9,
        0, 42, 47, 52, 32, 26, 48, 55, 6, 29, 84, 2, 4, 18, 56, 7, 29, 93, 44, 71, 3, 86, 66, 31,
        65, 0, 79, 20, 65, 52, 13}};
    final long[] capacities = {850};
    final long optimalProfit = 7534;
    solveKnapsackProblem(profits, weights, capacities, optimalProfit);
  }
}
