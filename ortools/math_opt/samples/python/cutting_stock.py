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

"""Solve the cutting stock problem by column generation.

The Cutting Stock problem is as follows. You begin with unlimited boards, all
of the same length. You are also given a list of smaller pieces to cut out,
each with a length and a demanded quantity. You want to cut out all these
pieces using as few of your starting boards as possible.

E.g. you begin with boards that are 20 feet long, and you must cut out 3
pieces that are 6 feet long and 5 pieces that are 8 feet long. An optimal
solution is:
  [(6,), (8, 8) (8, 8), (6, 6, 8)]
(We cut a 6 foot piece from the first board, two 8 foot pieces from
the second board, and so on.)

This example approximately solves the problem with a column generation
heuristic. The leader problem is a set cover problem, and the worker is a
knapsack problem. We alternate between solving the LP relaxation of the
leader incrementally, and solving the worker to generate new a configuration
(a column) for the leader. When the worker can no longer find a column
improving the LP cost, we convert the leader problem to a MIP and solve
again. We now give precise statements of the leader and worker.

Problem data:
 * l_i: the length of each piece we need to cut out.
 * d_i: how many copies each piece we need.
 * L: the length of our initial boards.
 * q_ci: for configuration c, the quantity of piece i produced.

Leader problem variables:
 * x_c: how many copies of configuration c to produce.

Leader problem formulation:
  min         sum_c x_c
  s.t. sum_c q_ci * x_c = d_i for all i
                    x_c >= 0, integer for all c.

The worker problem is to generate new configurations for the leader problem
based on the dual variables of the demand constraints in the LP relaxation.
Worker problem data:
  * p_i: The "price" of piece i (dual value from leader's demand constraint)

Worker decision variables:
 * y_i: How many copies of piece i should be in the configuration.

Worker formulation
  max   sum_i p_i * y_i
  s.t.  sum_i l_i * y_i <= L
                    y_i >= 0, integer for all i

An optimal solution y* defines a new configuration c with q_ci = y_i* for all
i. If the solution has objective value <= 1, no further improvement on the LP
is possible. For additional background and proofs see:
  https://people.orie.cornell.edu/shmoys/or630/notes-06/lec16.pdf
or any other reference on the "Cutting Stock Problem".

Note: this problem is equivalent to symmetric bin packing:
  https://en.wikipedia.org/wiki/Bin_packing_problem#Formal_statement
but typically in bin packing it is not assumed that you should exploit having
multiple items of the same size.
"""
from collections.abc import Sequence
import dataclasses

from absl import app

from ortools.math_opt.python import mathopt


@dataclasses.dataclass
class CuttingStockInstance:
    """Data for a cutting stock instance.

    Attributes:
      piece_sizes: The size of each piece with non-zero demand. Must have the same
        length as piece_demands, and each size must be in [0, board_length].
      piece_demands: The demand for a given piece. Must have the same length as
        piece_sizes.
      board_length: The length of each board.
    """

    piece_sizes: list[int] = dataclasses.field(default_factory=list)
    piece_demands: list[int] = dataclasses.field(default_factory=list)
    board_length: int = 0


@dataclasses.dataclass
class Configuration:
    """Describes a size-configuration that can be cut out of a board.

    Attributes:
      pieces: The size of each piece in the configuration. Must have the same
        length as piece_demands, and the total sum of pieces (sum of piece sizes
        times quantity of pieces) must not exceed the board length of the
        associated cutting stock instance.
      quantity: The qualtity of pieces of a given size. Must have the same length
        as pieces.
    """

    pieces: list[int] = dataclasses.field(default_factory=list)
    quantity: list[int] = dataclasses.field(default_factory=list)


@dataclasses.dataclass
class CuttingStockSolution:
    """Describes a solution to a cutting stock problem.

    To be feasible, the demand for each piece type must be met by the produced
    configurations

    Attributes:
      configurations: The configurations used by the solution. Must have the same
        length as quantity.
      quantity: The number of each configuration in the solution. Must have the
        same length as configurations.
      objective_value: The objective value of the configuration, which is equal to
        sum(quantity).
    """

    configurations: list[Configuration] = dataclasses.field(default_factory=list)
    quantity: list[int] = dataclasses.field(default_factory=list)
    objective_value: int = 0


def best_configuration(
    piece_prices: list[float], piece_sizes: list[int], board_size: int
) -> tuple[Configuration, float]:
    """Solves the worker problem.

    Solves the problem on finding the configuration (with its objective value) to
    add the to model that will give the greatest improvement in the LP
    relaxation. This is equivalent to a knapsack problem.

    Args:
      piece_prices: The price for each piece with non-zero demand. Must have the
        same length as piece_sizes.
      piece_sizes: The size of each piece with non-zero demand. Must have the same
        length as piece_prices, and each size must be in [0, board_length].
      board_size: The length of each board.

    Returns:
      The best configuration and its cost.

    Raises:
      RuntimeError: On solve errors.
    """
    num_pieces = len(piece_sizes)
    assert len(piece_sizes) == num_pieces
    model = mathopt.Model(name="knapsack")
    pieces = [
        model.add_integer_variable(lb=0, name=f"item_{i}") for i in range(num_pieces)
    ]
    model.maximize(sum(piece_prices[i] * pieces[i] for i in range(num_pieces)))
    model.add_linear_constraint(
        sum(piece_sizes[i] * pieces[i] for i in range(num_pieces)) <= board_size
    )
    solve_result = mathopt.solve(model, mathopt.SolverType.CP_SAT)
    if solve_result.termination.reason != mathopt.TerminationReason.OPTIMAL:
        raise RuntimeError(
            "Failed to solve knapsack pricing problem to "
            f" optimality: {solve_result.termination}"
        )
    config = Configuration()
    for i in range(num_pieces):
        use = round(solve_result.variable_values()[pieces[i]])
        if use > 0:
            config.pieces.append(i)
            config.quantity.append(use)
    return config, solve_result.objective_value()


def solve_cutting_stock(instance: CuttingStockInstance) -> CuttingStockSolution:
    """Solves the full cutting stock problem by decomposition.

    Args:
      instance: A cutting stock instance.

    Returns:
      A solution to the cutting stock instance.

    Raises:
      RuntimeError: On solve errors.
    """
    model = mathopt.Model(name="cutting_stock")
    model.objective.is_maximize = False
    n = len(instance.piece_sizes)
    demands = instance.piece_demands
    demand_met = [
        model.add_linear_constraint(lb=demands[i], ub=demands[i]) for i in range(n)
    ]

    configs: list[tuple[Configuration, mathopt.Variable]] = []

    def add_config(config: Configuration) -> None:
        v = model.add_variable(lb=0.0)
        model.objective.set_linear_coefficient(v, 1)
        for item, use in zip(config.pieces, config.quantity):
            if use >= 1:
                demand_met[item].set_coefficient(v, use)
        configs.append((config, v))

    # To ensure the leader problem is always feasible, begin a configuration for
    # every item that has a single copy of the item.
    for i in range(n):
        add_config(Configuration(pieces=[i], quantity=[1]))

    solver = mathopt.IncrementalSolver(model, mathopt.SolverType.GLOP)

    pricing_round = 0
    while True:
        solve_result = solver.solve()
        if solve_result.termination.reason != mathopt.TerminationReason.OPTIMAL:
            raise RuntimeError(
                "Failed to solve leader LP problem to optimality at "
                f"iteration {pricing_round} termination: "
                f"{solve_result.termination}"
            )
        if not solve_result.has_dual_feasible_solution:
            # MathOpt does not require solvers to return a dual solution on optimal,
            # but most LP solvers always will, see go/mathopt-solver-contracts for
            # details.
            raise RuntimeError(
                "no dual solution was returned with optimal solution "
                f"at iteration {pricing_round}"
            )
        prices = [solve_result.dual_values()[d] for d in demand_met]
        config, value = best_configuration(
            prices, instance.piece_sizes, instance.board_length
        )
        if value < 1 + 1e-3:
            # The LP relaxation is solved, we can stop adding columns.
            break
        add_config(config)
        print(
            f"round: {pricing_round}, "
            f"lp objective: {solve_result.objective_value()}",
            flush=True,
        )
        pricing_round += 1
    print("Done adding columns, switching to MIP")
    for _, var in configs:
        var.integer = True

    solve_result = mathopt.solve(model, mathopt.SolverType.CP_SAT)
    if solve_result.termination.reason not in (
        mathopt.TerminationReason.OPTIMAL,
        mathopt.TerminationReason.FEASIBLE,
    ):
        raise RuntimeError(
            "Failed to solve final cutting stock MIP, "
            f"termination: {solve_result.termination}"
        )

    solution = CuttingStockSolution()
    for config, var in configs:
        use = round(solve_result.variable_values()[var])
        if use > 0:
            solution.configurations.append(config)
            solution.quantity.append(use)
            solution.objective_value += use
    return solution


def main(argv: Sequence[str]) -> None:
    del argv  # Unused.

    # Data from https://en.wikipedia.org/wiki/Cutting_stock_problem
    instance = CuttingStockInstance(
        board_length=5600,
        piece_sizes=[
            1380,
            1520,
            1560,
            1710,
            1820,
            1880,
            1930,
            2000,
            2050,
            2100,
            2140,
            2150,
            2200,
        ],
        piece_demands=[22, 25, 12, 14, 18, 18, 20, 10, 12, 14, 16, 18, 20],
    )
    solution = solve_cutting_stock(instance)
    print("Best known solution uses 73 rolls.")
    print(f"Total rolls used in actual solution found: {solution.objective_value}")


if __name__ == "__main__":
    app.run(main)
