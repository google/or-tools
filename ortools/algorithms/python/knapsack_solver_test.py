#!/usr/bin/env python3
# Copyright 2010-2025 Google LLC
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""knapsack_solver unittest file."""

from absl.testing import absltest
from ortools.algorithms.python import knapsack_solver


class PyWrapAlgorithmsKnapsackSolverTest(absltest.TestCase):

    def RealSolve(self, profits, weights, capacities, solver_type, use_reduction):
        solver = knapsack_solver.KnapsackSolver(solver_type, "solver")
        solver.set_use_reduction(use_reduction)
        solver.init(profits, weights, capacities)
        profit = solver.solve()

        return profit

    def SolveKnapsackProblemUsingSpecificSolver(
        self, profits, weights, capacities, solver_type
    ):
        result_when_reduction = self.RealSolve(
            profits, weights, capacities, solver_type, True
        )
        result_when_no_reduction = self.RealSolve(
            profits, weights, capacities, solver_type, False
        )

        if result_when_reduction == result_when_no_reduction:
            return result_when_reduction
        else:
            return self._invalid_solution

    def SolveKnapsackProblem(self, profits, weights, capacities):
        self._invalid_solution = -1
        max_number_of_items_for_brute_force = 15
        max_number_of_items_for_divide_and_conquer = 32
        max_number_of_items_for_64_items_solver = 64
        number_of_items = len(profits)
        # This test is ran as size = 'small. To be fast enough, the dynamic
        # programming solver should be limited to instances with capacities smaller
        # than 10^6.
        max_capacity_for_dynamic_programming_solver = 1000000
        generic_profit = self.SolveKnapsackProblemUsingSpecificSolver(
            profits,
            weights,
            capacities,
            knapsack_solver.SolverType.KNAPSACK_MULTIDIMENSION_BRANCH_AND_BOUND_SOLVER,
        )

        if generic_profit == self._invalid_solution:
            return self._invalid_solution

        # Disabled due to ASAN raising a runtime error:
        # outside the range of representable values of type 'int'
        #    cbc_profit = self.SolveKnapsackProblemUsingSpecificSolver(
        #        profits,
        #        weights,
        #        capacities,
        #        knapsack_solver.SolverType.
        #        KNAPSACK_MULTIDIMENSION_CBC_MIP_SOLVER)
        #    if cbc_profit != generic_profit:
        #      return self._invalid_solution

        try:
            scip_profit = self.SolveKnapsackProblemUsingSpecificSolver(
                profits,
                weights,
                capacities,
                knapsack_solver.SolverType.KNAPSACK_MULTIDIMENSION_SCIP_MIP_SOLVER,
            )
            if scip_profit != generic_profit:
                return self._invalid_solution
        except AttributeError:
            print("SCIP support not compiled in")

        if len(weights) > 1:
            return generic_profit

        if number_of_items <= max_number_of_items_for_brute_force:
            brute_force_profit = self.SolveKnapsackProblemUsingSpecificSolver(
                profits,
                weights,
                capacities,
                knapsack_solver.SolverType.KNAPSACK_BRUTE_FORCE_SOLVER,
            )
            if brute_force_profit != generic_profit:
                return self._invalid_solution

        if number_of_items <= max_number_of_items_for_64_items_solver:
            items64_profit = self.SolveKnapsackProblemUsingSpecificSolver(
                profits,
                weights,
                capacities,
                knapsack_solver.SolverType.KNAPSACK_64ITEMS_SOLVER,
            )
            if items64_profit != generic_profit:
                return self._invalid_solution

        if capacities[0] <= max_capacity_for_dynamic_programming_solver:
            dynamic_programming_profit = self.SolveKnapsackProblemUsingSpecificSolver(
                profits,
                weights,
                capacities,
                knapsack_solver.SolverType.KNAPSACK_DYNAMIC_PROGRAMMING_SOLVER,
            )
            if dynamic_programming_profit != generic_profit:
                return self._invalid_solution

        if number_of_items <= max_number_of_items_for_divide_and_conquer:
            divide_and_conquer_profit = self.SolveKnapsackProblemUsingSpecificSolver(
                profits,
                weights,
                capacities,
                knapsack_solver.SolverType.KNAPSACK_DIVIDE_AND_CONQUER_SOLVER,
            )
            if divide_and_conquer_profit != generic_profit:
                return self._invalid_solution

        return generic_profit

    def testSolveOneDimension(self):
        profits = [1, 2, 3, 4, 5, 6, 7, 8, 9]
        weights = [[1, 2, 3, 4, 5, 6, 7, 8, 9]]
        capacities = [34]
        optimal_profit = 34
        profit = self.SolveKnapsackProblem(profits, weights, capacities)
        self.assertEqual(optimal_profit, profit)

    def testSolveTwoDimensions(self):
        profits = [1, 2, 3, 4, 5, 6, 7, 8, 9]
        weights = [[1, 2, 3, 4, 5, 6, 7, 8, 9], [1, 1, 1, 1, 1, 1, 1, 1, 1]]
        capacities = [34, 4]
        optimal_profit = 30
        profit = self.SolveKnapsackProblem(profits, weights, capacities)
        self.assertEqual(optimal_profit, profit)

    def testSolveBigOneDimension(self):
        profits = [
            360,
            83,
            59,
            130,
            431,
            67,
            230,
            52,
            93,
            125,
            670,
            892,
            600,
            38,
            48,
            147,
            78,
            256,
            63,
            17,
            120,
            164,
            432,
            35,
            92,
            110,
            22,
            42,
            50,
            323,
            514,
            28,
            87,
            73,
            78,
            15,
            26,
            78,
            210,
            36,
            85,
            189,
            274,
            43,
            33,
            10,
            19,
            389,
            276,
            312,
        ]
        weights = [
            [
                7,
                0,
                30,
                22,
                80,
                94,
                11,
                81,
                70,
                64,
                59,
                18,
                0,
                36,
                3,
                8,
                15,
                42,
                9,
                0,
                42,
                47,
                52,
                32,
                26,
                48,
                55,
                6,
                29,
                84,
                2,
                4,
                18,
                56,
                7,
                29,
                93,
                44,
                71,
                3,
                86,
                66,
                31,
                65,
                0,
                79,
                20,
                65,
                52,
                13,
            ]
        ]
        capacities = [850]
        optimal_profit = 7534
        profit = self.SolveKnapsackProblem(profits, weights, capacities)
        self.assertEqual(optimal_profit, profit)


if __name__ == "__main__":
    absltest.main()
