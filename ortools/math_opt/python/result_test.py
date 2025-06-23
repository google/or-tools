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

import datetime
import math

from absl.testing import absltest
from ortools.pdlp import solve_log_pb2
from ortools.gscip import gscip_pb2
from ortools.math_opt import result_pb2
from ortools.math_opt import solution_pb2
from ortools.math_opt import sparse_containers_pb2
from ortools.math_opt.python import model
from ortools.math_opt.python import result
from ortools.math_opt.python import solution
from ortools.math_opt.python.testing import compare_proto
from ortools.math_opt.solvers import osqp_pb2


class TerminationTest(compare_proto.MathOptProtoAssertions, absltest.TestCase):

  def test_termination_unspecified(self) -> None:
    termination_proto = result_pb2.TerminationProto(
        reason=result_pb2.TERMINATION_REASON_UNSPECIFIED
    )
    with self.assertRaisesRegex(ValueError, "Termination.*UNSPECIFIED"):
      result.parse_termination(termination_proto)

  def test_termination_limit_but_not_limit_reason(self) -> None:
    termination_proto = result_pb2.TerminationProto(
        reason=result_pb2.TERMINATION_REASON_OPTIMAL,
        limit=result_pb2.LIMIT_OTHER,
    )
    with self.assertRaisesRegex(
        ValueError, "Termination limit.*FEASIBLE or NO_SOLUTION_FOUND"
    ):
      result.parse_termination(termination_proto)

  def test_termination_limit_reason_but_no_limit(self) -> None:
    termination_proto = result_pb2.TerminationProto(
        reason=result_pb2.TERMINATION_REASON_NO_SOLUTION_FOUND,
        limit=result_pb2.LIMIT_UNSPECIFIED,
    )
    with self.assertRaisesRegex(
        ValueError, "Termination limit.*FEASIBLE or NO_SOLUTION_FOUND"
    ):
      result.parse_termination(termination_proto)

  def test_termination_ok_proto_round_trip(self) -> None:
    termination = result.Termination(
        reason=result.TerminationReason.NO_SOLUTION_FOUND,
        limit=result.Limit.OTHER,
        detail="detail",
        problem_status=result.ProblemStatus(
            primal_status=result.FeasibilityStatus.FEASIBLE,
            dual_status=result.FeasibilityStatus.INFEASIBLE,
            primal_or_dual_infeasible=False,
        ),
        objective_bounds=result.ObjectiveBounds(primal_bound=10, dual_bound=20),
    )

    termination_proto = result_pb2.TerminationProto(
        reason=result_pb2.TERMINATION_REASON_NO_SOLUTION_FOUND,
        limit=result_pb2.LIMIT_OTHER,
        detail="detail",
        problem_status=result_pb2.ProblemStatusProto(
            primal_status=result_pb2.FEASIBILITY_STATUS_FEASIBLE,
            dual_status=result_pb2.FEASIBILITY_STATUS_INFEASIBLE,
            primal_or_dual_infeasible=False,
        ),
        objective_bounds=result_pb2.ObjectiveBoundsProto(
            primal_bound=10, dual_bound=20
        ),
    )

    # Test proto-> Termination
    self.assertEqual(result.parse_termination(termination_proto), termination)

    # Test Termination -> proto
    self.assert_protos_equiv(termination.to_proto(), termination_proto)


class ParseProblemStatus(
    compare_proto.MathOptProtoAssertions, absltest.TestCase
):

  def test_problem_status_round_trip(self) -> None:
    problem_status = result.ProblemStatus(
        primal_status=result.FeasibilityStatus.FEASIBLE,
        dual_status=result.FeasibilityStatus.INFEASIBLE,
        primal_or_dual_infeasible=False,
    )
    problem_status_proto = problem_status.to_proto()
    expected_proto = result_pb2.ProblemStatusProto(
        primal_status=result_pb2.FEASIBILITY_STATUS_FEASIBLE,
        dual_status=result_pb2.FEASIBILITY_STATUS_INFEASIBLE,
        primal_or_dual_infeasible=False,
    )
    self.assert_protos_equiv(expected_proto, problem_status_proto)
    round_trip_status = result.parse_problem_status(problem_status_proto)
    self.assertEqual(problem_status, round_trip_status)

  def test_problem_status_unspecified_primal_status(self) -> None:
    proto = result_pb2.ProblemStatusProto(
        primal_status=result_pb2.FEASIBILITY_STATUS_UNSPECIFIED,
        dual_status=result_pb2.FEASIBILITY_STATUS_INFEASIBLE,
        primal_or_dual_infeasible=False,
    )
    with self.assertRaisesRegex(
        ValueError, "Primal feasibility status.*UNSPECIFIED"
    ):
      result.parse_problem_status(proto)

  def test_problem_status_unspecified_dual_status(self) -> None:
    proto = result_pb2.ProblemStatusProto(
        primal_status=result_pb2.FEASIBILITY_STATUS_INFEASIBLE,
        dual_status=result_pb2.FEASIBILITY_STATUS_UNSPECIFIED,
        primal_or_dual_infeasible=False,
    )
    with self.assertRaisesRegex(
        ValueError, "Dual feasibility status.*UNSPECIFIED"
    ):
      result.parse_problem_status(proto)


class ParseObjectiveBounds(
    compare_proto.MathOptProtoAssertions, absltest.TestCase
):

  def test_objective_bounds_round_trip(self) -> None:
    objective_bounds = result.ObjectiveBounds(primal_bound=10, dual_bound=20)
    objective_bounds_proto = objective_bounds.to_proto()
    expected_proto = result_pb2.ObjectiveBoundsProto(
        primal_bound=10, dual_bound=20
    )
    self.assert_protos_equiv(expected_proto, objective_bounds_proto)
    round_trip_objective_bounds = result.parse_objective_bounds(
        objective_bounds_proto
    )
    self.assertEqual(objective_bounds, round_trip_objective_bounds)


class ParseSolveStats(compare_proto.MathOptProtoAssertions, absltest.TestCase):

  def test_problem_status_round_trip(self) -> None:
    solve_stats = result.SolveStats(
        solve_time=datetime.timedelta(seconds=10),
        simplex_iterations=10,
        barrier_iterations=20,
        first_order_iterations=30,
        node_count=40,
    )
    solve_stats_proto = solve_stats.to_proto()
    expected_proto = result_pb2.SolveStatsProto()
    expected_proto.solve_time.seconds = 10
    expected_proto.simplex_iterations = 10
    expected_proto.barrier_iterations = 20
    expected_proto.first_order_iterations = 30
    expected_proto.node_count = 40
    self.assert_protos_equiv(expected_proto, solve_stats_proto)
    round_trip_solve_stats = result.parse_solve_stats(solve_stats_proto)
    self.assertEqual(solve_stats, round_trip_solve_stats)


class SolveResultAuxiliaryFunctionsTest(absltest.TestCase):

  def test_solve_time(self) -> None:
    res = result.SolveResult(
        solve_stats=result.SolveStats(solve_time=datetime.timedelta(seconds=10))
    )
    self.assertEqual(res.solve_time(), datetime.timedelta(seconds=10))

  def test_best_objective_bound(self) -> None:
    res = result.SolveResult(
        termination=result.Termination(
            objective_bounds=result.ObjectiveBounds(dual_bound=10.0)
        )
    )
    self.assertEqual(res.best_objective_bound(), 10.0)

  def test_primal_solution_has_feasible(self) -> None:
    mod = model.Model(name="test_model")
    x = mod.add_binary_variable(name="x")
    y = mod.add_binary_variable(name="y")
    other_mod = model.Model(name="other_test_model")
    other_x = other_mod.add_binary_variable(name="other_x")
    res = result.SolveResult()
    res.solutions.append(
        solution.Solution(
            primal_solution=solution.PrimalSolution(
                variable_values={x: 2.0, y: 1.0},
                objective_value=3.0,
                feasibility_status=solution.SolutionStatus.FEASIBLE,
            )
        )
    )
    self.assertTrue(res.has_primal_feasible_solution())
    self.assertEqual(res.objective_value(), 3.0)
    self.assertDictEqual(res.variable_values(), {x: 2.0, y: 1.0})
    self.assertEqual(res.variable_values()[x], 2.0)
    self.assertEqual(res.variable_values([]), [])
    self.assertEqual(res.variable_values([y, x]), [1.0, 2.0])
    self.assertEqual(res.variable_values(y), 1.0)
    with self.assertRaisesRegex(KeyError, ".*other_x"):
      res.variable_values(other_x)
    with self.assertRaisesRegex(KeyError, ".*string"):
      res.variable_values([y, "string"])
    with self.assertRaisesRegex(TypeError, ".*int"):
      res.variable_values(20)  # pytype: disable=wrong-arg-types

  def test_primal_solution_no_feasible(self) -> None:
    mod = model.Model(name="test_model")
    x = mod.add_binary_variable(name="x")
    res = result.SolveResult()
    res.solutions.append(
        solution.Solution(
            primal_solution=solution.PrimalSolution(
                variable_values={
                    x: 2.0,
                },
                objective_value=3.0,
                feasibility_status=solution.SolutionStatus.UNDETERMINED,
            )
        )
    )
    self.assertFalse(res.has_primal_feasible_solution())
    with self.assertRaisesRegex(ValueError, "No primal feasible.*"):
      res.objective_value()
    with self.assertRaisesRegex(ValueError, "No primal feasible.*"):
      res.variable_values()

  def test_primal_solution_no_primal(self) -> None:
    mod = model.Model(name="test_model")
    x = mod.add_binary_variable(name="x")
    c = mod.add_linear_constraint(lb=0.0, ub=1.0, name="c")
    res = result.SolveResult()
    res.solutions.append(
        solution.Solution(
            dual_solution=solution.DualSolution(
                dual_values={c: 3.0},
                reduced_costs={x: 1.0},
                objective_value=2.0,
                feasibility_status=solution.SolutionStatus.FEASIBLE,
            )
        )
    )
    self.assertFalse(res.has_primal_feasible_solution())
    with self.assertRaisesRegex(ValueError, "No primal feasible.*"):
      res.objective_value()
    with self.assertRaisesRegex(ValueError, "No primal feasible.*"):
      res.variable_values()

  def test_primal_solution_no_solution(self) -> None:
    res = result.SolveResult()
    self.assertFalse(res.has_primal_feasible_solution())
    with self.assertRaisesRegex(ValueError, "No primal feasible.*"):
      res.objective_value()
    with self.assertRaisesRegex(ValueError, "No primal feasible.*"):
      res.variable_values()

  def test_dual_solution_has_feasible(self) -> None:
    mod = model.Model(name="test_model")
    x = mod.add_binary_variable(name="x")
    y = mod.add_binary_variable(name="y")
    c = mod.add_linear_constraint(lb=0.0, ub=1.0, name="c")
    d = mod.add_linear_constraint(lb=0.0, ub=1.0, name="d")
    other_mod = model.Model(name="other_test_model")
    other_x = other_mod.add_binary_variable(name="other_x")
    other_c = mod.add_linear_constraint(lb=0.0, ub=1.0, name="other_c")
    res = result.SolveResult()
    res.solutions.append(
        solution.Solution(
            dual_solution=solution.DualSolution(
                dual_values={c: 3.0, d: 4.0},
                reduced_costs={x: 1.0, y: -2.0},
                objective_value=2.0,
                feasibility_status=solution.SolutionStatus.FEASIBLE,
            )
        )
    )
    self.assertTrue(res.has_dual_feasible_solution())
    # Reduced costs.
    self.assertDictEqual(res.reduced_costs(), {x: 1.0, y: -2.0})
    self.assertEqual(res.reduced_costs()[x], 1.0)
    self.assertEqual(res.reduced_costs([]), [])
    self.assertEqual(res.reduced_costs([y, x]), [-2.0, 1.0])
    self.assertEqual(res.reduced_costs(y), -2.0)
    with self.assertRaisesRegex(KeyError, ".*other_x"):
      res.reduced_costs(other_x)
    with self.assertRaisesRegex(KeyError, ".*string"):
      res.reduced_costs([y, "string"])
    with self.assertRaisesRegex(TypeError, ".*int"):
      res.reduced_costs(20)  # pytype: disable=wrong-arg-types
    # Dual values.
    self.assertDictEqual(res.dual_values(), {c: 3.0, d: 4.0})
    self.assertEqual(res.dual_values()[c], 3.0)
    self.assertEqual(res.dual_values([]), [])
    self.assertEqual(res.dual_values([d, c]), [4.0, 3.0])
    self.assertEqual(res.dual_values(c), 3.0)
    with self.assertRaisesRegex(KeyError, ".*other_c"):
      res.dual_values(other_c)
    with self.assertRaisesRegex(KeyError, ".*string"):
      res.dual_values([d, "string"])
    with self.assertRaisesRegex(TypeError, ".*int"):
      res.dual_values(20)  # pytype: disable=wrong-arg-types

  def test_dual_solution_no_feasible(self) -> None:
    mod = model.Model(name="test_model")
    x = mod.add_binary_variable(name="x")
    c = mod.add_linear_constraint(lb=0.0, ub=1.0, name="c")
    res = result.SolveResult()
    res.solutions.append(
        solution.Solution(
            dual_solution=solution.DualSolution(
                dual_values={c: 3.0},
                reduced_costs={
                    x: 1.0,
                },
                objective_value=2.0,
                feasibility_status=solution.SolutionStatus.UNDETERMINED,
            )
        )
    )
    self.assertFalse(res.has_dual_feasible_solution())
    with self.assertRaisesRegex(ValueError, "Best solution.*dual feasible.*"):
      res.reduced_costs()
    with self.assertRaisesRegex(ValueError, "Best solution.*dual feasible.*"):
      res.dual_values()

  def test_dual_solution_no_dual_in_best_solution(self) -> None:
    mod = model.Model(name="test_model")
    x = mod.add_binary_variable(name="x")
    c = mod.add_linear_constraint(lb=0.0, ub=1.0, name="c")
    res = result.SolveResult()
    res.solutions.append(
        solution.Solution(
            primal_solution=solution.PrimalSolution(
                variable_values={
                    x: 2.0,
                },
                objective_value=3.0,
                feasibility_status=solution.SolutionStatus.FEASIBLE,
            )
        )
    )
    res.solutions.append(
        solution.Solution(
            dual_solution=solution.DualSolution(
                dual_values={c: 3.0},
                reduced_costs={
                    x: 1.0,
                },
                objective_value=2.0,
                feasibility_status=solution.SolutionStatus.FEASIBLE,
            )
        )
    )
    self.assertFalse(res.has_dual_feasible_solution())
    with self.assertRaisesRegex(ValueError, "Best solution.*dual feasible.*"):
      res.reduced_costs()
    with self.assertRaisesRegex(ValueError, "Best solution.*dual feasible.*"):
      res.dual_values()

  def test_dual_solution_no_solution(self) -> None:
    res = result.SolveResult()
    self.assertFalse(res.has_dual_feasible_solution())
    with self.assertRaisesRegex(ValueError, "Best solution.*dual feasible.*"):
      res.reduced_costs()
    with self.assertRaisesRegex(ValueError, "Best solution.*dual feasible.*"):
      res.dual_values()

  def test_primal_ray_has_ray(self) -> None:
    mod = model.Model(name="test_model")
    x = mod.add_binary_variable(name="x")
    y = mod.add_binary_variable(name="y")
    other_mod = model.Model(name="other_test_model")
    other_x = other_mod.add_binary_variable(name="other_x")
    res = result.SolveResult()
    res.primal_rays.append(solution.PrimalRay(variable_values={x: 2.0, y: 1.0}))
    self.assertTrue(res.has_ray())
    self.assertDictEqual(res.ray_variable_values(), {x: 2.0, y: 1.0})
    self.assertEqual(res.ray_variable_values()[x], 2.0)
    self.assertEqual(res.ray_variable_values([]), [])
    self.assertEqual(res.ray_variable_values([y, x]), [1.0, 2.0])
    self.assertEqual(res.ray_variable_values(y), 1.0)
    with self.assertRaisesRegex(KeyError, ".*other_x"):
      res.ray_variable_values(other_x)
    with self.assertRaisesRegex(KeyError, ".*string"):
      res.ray_variable_values([y, "string"])
    with self.assertRaisesRegex(TypeError, ".*int"):
      res.ray_variable_values(20)  # pytype: disable=wrong-arg-types

  def test_primal_ray_no_ray(self) -> None:
    res = result.SolveResult()
    self.assertFalse(res.has_ray())
    with self.assertRaisesRegex(ValueError, ".*primal ray.*"):
      res.ray_variable_values()

  def test_dual_ray_has_ray(self) -> None:
    mod = model.Model(name="test_model")
    x = mod.add_binary_variable(name="x")
    y = mod.add_binary_variable(name="y")
    c = mod.add_linear_constraint(lb=0.0, ub=1.0, name="c")
    d = mod.add_linear_constraint(lb=0.0, ub=1.0, name="c")
    other_mod = model.Model(name="other_test_model")
    other_x = other_mod.add_binary_variable(name="other_x")
    other_c = mod.add_linear_constraint(lb=0.0, ub=1.0, name="other_c")
    res = result.SolveResult()
    res.dual_rays.append(
        solution.DualRay(
            dual_values={c: 3.0, d: 4.0}, reduced_costs={x: 1.0, y: -2.0}
        )
    )
    self.assertTrue(res.has_dual_ray())
    self.assertDictEqual(res.ray_reduced_costs(), {x: 1.0, y: -2.0})
    # Reduced costs.
    self.assertEqual(res.ray_reduced_costs()[x], 1.0)
    self.assertEqual(res.ray_reduced_costs([]), [])
    self.assertEqual(res.ray_reduced_costs([y, x]), [-2.0, 1.0])
    self.assertEqual(res.ray_reduced_costs(y), -2.0)
    with self.assertRaisesRegex(KeyError, ".*other_x"):
      res.ray_reduced_costs(other_x)
    with self.assertRaisesRegex(KeyError, ".*string"):
      res.ray_reduced_costs([y, "string"])
    with self.assertRaisesRegex(TypeError, ".*int"):
      res.ray_reduced_costs(20)  # pytype: disable=wrong-arg-types
    # Dual values.
    self.assertDictEqual(res.ray_dual_values(), {c: 3.0, d: 4.0})
    self.assertEqual(res.ray_dual_values()[c], 3.0)
    self.assertEqual(res.ray_dual_values([]), [])
    self.assertEqual(res.ray_dual_values([d, c]), [4.0, 3.0])
    self.assertEqual(res.ray_dual_values(c), 3.0)
    with self.assertRaisesRegex(KeyError, ".*other_c"):
      res.ray_dual_values(other_c)
    with self.assertRaisesRegex(KeyError, ".*string"):
      res.ray_dual_values([d, "string"])
    with self.assertRaisesRegex(TypeError, ".*int"):
      res.ray_dual_values(20)  # pytype: disable=wrong-arg-types

  def test_dual_ray_no_ray(self) -> None:
    res = result.SolveResult()
    self.assertFalse(res.has_dual_ray())
    with self.assertRaisesRegex(ValueError, ".*dual ray.*"):
      res.ray_dual_values()
    with self.assertRaisesRegex(ValueError, ".*dual ray.*"):
      res.ray_reduced_costs()

  def test_basis_has_basis(self) -> None:
    mod = model.Model(name="test_model")
    x = mod.add_binary_variable(name="x")
    y = mod.add_binary_variable(name="y")
    c = mod.add_linear_constraint(lb=0.0, ub=1.0, name="c")
    d = mod.add_linear_constraint(lb=0.0, ub=1.0, name="d")
    other_mod = model.Model(name="other_test_model")
    other_x = other_mod.add_binary_variable(name="other_x")
    other_c = other_mod.add_linear_constraint(name="other_c")
    res = result.SolveResult()
    res.solutions.append(
        solution.Solution(
            basis=solution.Basis(
                variable_status={
                    x: solution.BasisStatus.AT_LOWER_BOUND,
                    y: solution.BasisStatus.AT_UPPER_BOUND,
                },
                constraint_status={
                    c: solution.BasisStatus.BASIC,
                    d: solution.BasisStatus.FIXED_VALUE,
                },
            )
        )
    )
    self.assertTrue(res.has_basis())
    # Variable status
    self.assertDictEqual(
        res.variable_status(),
        {
            x: solution.BasisStatus.AT_LOWER_BOUND,
            y: solution.BasisStatus.AT_UPPER_BOUND,
        },
    )
    self.assertEqual(
        res.variable_status()[x], solution.BasisStatus.AT_LOWER_BOUND
    )
    self.assertEqual(res.variable_status([]), [])
    self.assertEqual(
        res.variable_status([y, x]),
        [
            solution.BasisStatus.AT_UPPER_BOUND,
            solution.BasisStatus.AT_LOWER_BOUND,
        ],
    )
    self.assertEqual(
        res.variable_status(y), solution.BasisStatus.AT_UPPER_BOUND
    )
    with self.assertRaisesRegex(KeyError, ".*other_x"):
      res.variable_status(other_x)
    with self.assertRaisesRegex(KeyError, ".*string"):
      res.variable_status([y, "string"])
    with self.assertRaisesRegex(TypeError, ".*int"):
      res.variable_status(20)  # pytype: disable=wrong-arg-types
    # Constraint status
    self.assertDictEqual(
        res.constraint_status(),
        {c: solution.BasisStatus.BASIC, d: solution.BasisStatus.FIXED_VALUE},
    )
    self.assertEqual(res.constraint_status()[c], solution.BasisStatus.BASIC)
    self.assertEqual(res.constraint_status([]), [])
    self.assertEqual(
        res.constraint_status([d, c]),
        [solution.BasisStatus.FIXED_VALUE, solution.BasisStatus.BASIC],
    )
    self.assertEqual(res.constraint_status(c), solution.BasisStatus.BASIC)
    with self.assertRaisesRegex(KeyError, ".*other_c"):
      res.constraint_status(other_c)
    with self.assertRaisesRegex(KeyError, ".*string"):
      res.constraint_status([d, "string"])
    with self.assertRaisesRegex(TypeError, ".*int"):
      res.constraint_status(20)  # pytype: disable=wrong-arg-types

  def test_basis_no_basis_in_best_solution(self) -> None:
    mod = model.Model(name="test_model")
    x = mod.add_binary_variable(name="x")
    y = mod.add_binary_variable(name="y")
    c = mod.add_linear_constraint(lb=0.0, ub=1.0, name="c")
    res = result.SolveResult()
    res.solutions.append(
        solution.Solution(
            primal_solution=solution.PrimalSolution(
                variable_values={x: 2.0, y: 1.0},
                objective_value=3.0,
                feasibility_status=solution.SolutionStatus.FEASIBLE,
            )
        )
    )
    res.solutions.append(
        solution.Solution(
            basis=solution.Basis(
                variable_status={
                    x: solution.BasisStatus.AT_LOWER_BOUND,
                    y: solution.BasisStatus.AT_UPPER_BOUND,
                },
                constraint_status={c: solution.BasisStatus.BASIC},
            )
        )
    )
    self.assertFalse(res.has_basis())
    with self.assertRaisesRegex(ValueError, "Best solution.*basis.*"):
      res.variable_status()
    with self.assertRaisesRegex(ValueError, "Best solution.*basis.*"):
      res.constraint_status()

  def test_basis_no_solution(self) -> None:
    res = result.SolveResult()
    self.assertFalse(res.has_basis())
    with self.assertRaisesRegex(ValueError, "Best solution.*basis.*"):
      res.variable_status()
    with self.assertRaisesRegex(ValueError, "Best solution.*basis.*"):
      res.constraint_status()

  def test_bounded(self) -> None:
    res = result.SolveResult(
        termination=result.Termination(
            reason=result.TerminationReason.NO_SOLUTION_FOUND,
            problem_status=result.ProblemStatus(
                primal_status=result.FeasibilityStatus.FEASIBLE,
                dual_status=result.FeasibilityStatus.FEASIBLE,
                primal_or_dual_infeasible=False,
            ),
            objective_bounds=result.ObjectiveBounds(
                primal_bound=math.inf,
                dual_bound=-math.inf,
            ),
        ),
    )
    self.assertTrue(res.bounded())

  def test_not_bounded_primal_infeasible(self) -> None:
    res = result.SolveResult(
        termination=result.Termination(
            reason=result.TerminationReason.NO_SOLUTION_FOUND,
            problem_status=result.ProblemStatus(
                primal_status=result.FeasibilityStatus.INFEASIBLE,
                dual_status=result.FeasibilityStatus.FEASIBLE,
                primal_or_dual_infeasible=False,
            ),
            objective_bounds=result.ObjectiveBounds(
                primal_bound=math.inf,
                dual_bound=-math.inf,
            ),
        ),
    )
    self.assertFalse(res.bounded())

  def test_not_bounded_dual_infeasible(self) -> None:
    res = result.SolveResult(
        termination=result.Termination(
            reason=result.TerminationReason.NO_SOLUTION_FOUND,
            problem_status=result.ProblemStatus(
                primal_status=result.FeasibilityStatus.FEASIBLE,
                dual_status=result.FeasibilityStatus.INFEASIBLE,
                primal_or_dual_infeasible=False,
            ),
            objective_bounds=result.ObjectiveBounds(
                primal_bound=math.inf,
                dual_bound=-math.inf,
            ),
        ),
    )
    self.assertFalse(res.bounded())


def _make_undetermined_result_proto() -> result_pb2.SolveResultProto:
  proto = result_pb2.SolveResultProto(
      termination=result_pb2.TerminationProto(
          reason=result_pb2.TERMINATION_REASON_NO_SOLUTION_FOUND,
          limit=result_pb2.LIMIT_TIME,
          problem_status=result_pb2.ProblemStatusProto(
              primal_status=result_pb2.FEASIBILITY_STATUS_UNDETERMINED,
              dual_status=result_pb2.FEASIBILITY_STATUS_UNDETERMINED,
              primal_or_dual_infeasible=False,
          ),
          objective_bounds=result_pb2.ObjectiveBoundsProto(
              primal_bound=math.inf,
              dual_bound=-math.inf,
          ),
      )
  )
  proto.solve_stats.solve_time.FromTimedelta(datetime.timedelta(minutes=2))
  return proto


def _make_undetermined_solve_result() -> result.SolveResult:
  return result.SolveResult(
      termination=result.Termination(
          reason=result.TerminationReason.NO_SOLUTION_FOUND,
          limit=result.Limit.TIME,
          problem_status=result.ProblemStatus(
              primal_status=result.FeasibilityStatus.UNDETERMINED,
              dual_status=result.FeasibilityStatus.UNDETERMINED,
          ),
          objective_bounds=result.ObjectiveBounds(
              primal_bound=math.inf, dual_bound=-math.inf
          ),
      ),
      solve_stats=result.SolveStats(solve_time=datetime.timedelta(minutes=2)),
  )


class SolveResultTest(compare_proto.MathOptProtoAssertions, absltest.TestCase):

  def test_solve_result_gscip_output(self) -> None:
    mod = model.Model(name="test_model")
    res = _make_undetermined_solve_result()
    res.gscip_specific_output = gscip_pb2.GScipOutput(
        status_detail="gscip_detail"
    )

    proto = _make_undetermined_result_proto()
    proto.gscip_output.status_detail = "gscip_detail"

    # proto -> result
    actual_res = result.parse_solve_result(proto, mod)
    self.assertIsNotNone(actual_res.gscip_specific_output)
    assert actual_res.gscip_specific_output is not None
    self.assertEqual(
        "gscip_detail", actual_res.gscip_specific_output.status_detail
    )
    self.assertIsNone(actual_res.pdlp_specific_output)
    self.assertIsNone(actual_res.osqp_specific_output)

    # result -> proto
    self.assert_protos_equiv(res.to_proto(), proto)

  def test_solve_result_osqp_output(self) -> None:
    mod = model.Model(name="test_model")
    res = _make_undetermined_solve_result()
    res.osqp_specific_output = osqp_pb2.OsqpOutput(
        initialized_underlying_solver=True
    )

    proto = _make_undetermined_result_proto()
    proto.osqp_output.initialized_underlying_solver = True

    # proto -> result
    actual_res = result.parse_solve_result(proto, mod)
    self.assertIsNotNone(actual_res.osqp_specific_output)
    assert actual_res.osqp_specific_output is not None
    self.assertTrue(
        actual_res.osqp_specific_output.initialized_underlying_solver
    )
    self.assertIsNone(actual_res.pdlp_specific_output)
    self.assertIsNone(actual_res.gscip_specific_output)

    # result -> proto
    self.assert_protos_equiv(res.to_proto(), proto)

  def test_solve_result_pdlp_output(self) -> None:
    mod = model.Model(name="test_model")
    res = _make_undetermined_solve_result()
    res.pdlp_specific_output = result_pb2.SolveResultProto.PdlpOutput(
        convergence_information=solve_log_pb2.ConvergenceInformation(
            primal_objective=1.0
        )
    )

    proto = _make_undetermined_result_proto()
    proto.pdlp_output.convergence_information.primal_objective = 1.0

    # proto -> result
    actual_res = result.parse_solve_result(proto, mod)
    self.assertIsNotNone(actual_res.pdlp_specific_output)
    assert actual_res.pdlp_specific_output is not None
    self.assertEqual(
        actual_res.pdlp_specific_output.convergence_information.primal_objective,
        1.0,
    )
    self.assertIsNone(actual_res.osqp_specific_output)
    self.assertIsNone(actual_res.gscip_specific_output)

    # result -> proto
    self.assert_protos_equiv(res.to_proto(), proto)

  def test_multiple_solver_specific_outputs_error(self) -> None:
    res = _make_undetermined_solve_result()
    res.gscip_specific_output = gscip_pb2.GScipOutput(
        status_detail="gscip_detail"
    )
    res.osqp_specific_output = osqp_pb2.OsqpOutput(
        initialized_underlying_solver=False
    )
    with self.assertRaisesRegex(ValueError, "solver specific output"):
      res.to_proto()

  def test_solve_result_from_proto_missing_bounds_in_termination(
      self,
  ) -> None:
    mod = model.Model(name="test_model")
    proto = result_pb2.SolveResultProto(
        termination=result_pb2.TerminationProto(
            reason=result_pb2.TERMINATION_REASON_INFEASIBLE,
            detail="",
            problem_status=result_pb2.ProblemStatusProto(
                primal_status=result_pb2.FEASIBILITY_STATUS_INFEASIBLE,
                dual_status=result_pb2.FEASIBILITY_STATUS_INFEASIBLE,
                primal_or_dual_infeasible=False,
            ),
        ),
        solve_stats=result_pb2.SolveStatsProto(
            best_primal_bound=10.0,
            best_dual_bound=20.0,
        ),
    )
    res = result.parse_solve_result(proto, mod)
    self.assertEqual(10.0, res.termination.objective_bounds.primal_bound)
    self.assertEqual(20.0, res.termination.objective_bounds.dual_bound)
    self.assertFalse(res.termination.problem_status.primal_or_dual_infeasible)

  def test_solve_result_from_proto_missing_status_in_termination(
      self,
  ) -> None:
    mod = model.Model(name="test_model")
    proto = result_pb2.SolveResultProto(
        termination=result_pb2.TerminationProto(
            reason=result_pb2.TERMINATION_REASON_INFEASIBLE,
            detail="",
            objective_bounds=result_pb2.ObjectiveBoundsProto(
                primal_bound=10.0, dual_bound=20.0
            ),
        ),
        solve_stats=result_pb2.SolveStatsProto(
            problem_status=result_pb2.ProblemStatusProto(
                primal_status=result_pb2.FEASIBILITY_STATUS_INFEASIBLE,
                dual_status=result_pb2.FEASIBILITY_STATUS_FEASIBLE,
                primal_or_dual_infeasible=False,
            ),
        ),
    )
    res = result.parse_solve_result(proto, mod)
    self.assertEqual(
        result.FeasibilityStatus.INFEASIBLE,
        res.termination.problem_status.primal_status,
    )
    self.assertEqual(
        result.FeasibilityStatus.FEASIBLE,
        res.termination.problem_status.dual_status,
    )

  def test_solve_result_from_proto_double_infeasible_multiple_rays(
      self,
  ) -> None:
    mod = model.Model(name="test_model")
    x = mod.add_binary_variable(name="x")
    y = mod.add_binary_variable(name="y")
    c = mod.add_linear_constraint(lb=0.0, ub=1.0, name="c")

    proto = result_pb2.SolveResultProto(
        termination=result_pb2.TerminationProto(
            reason=result_pb2.TERMINATION_REASON_INFEASIBLE,
            detail="",
            problem_status=result_pb2.ProblemStatusProto(
                primal_status=result_pb2.FEASIBILITY_STATUS_INFEASIBLE,
                dual_status=result_pb2.FEASIBILITY_STATUS_INFEASIBLE,
                primal_or_dual_infeasible=False,
            ),
            objective_bounds=result_pb2.ObjectiveBoundsProto(
                primal_bound=math.inf, dual_bound=-math.inf
            ),
        ),
        primal_rays=[
            solution_pb2.PrimalRayProto(
                variable_values=sparse_containers_pb2.SparseDoubleVectorProto(
                    ids=[0, 1], values=[2.0, 1.0]
                )
            ),
            solution_pb2.PrimalRayProto(
                variable_values=sparse_containers_pb2.SparseDoubleVectorProto(
                    ids=[0, 1], values=[3.0, 2.0]
                )
            ),
        ],
        dual_rays=[
            solution_pb2.DualRayProto(
                dual_values=sparse_containers_pb2.SparseDoubleVectorProto(
                    ids=[0], values=[4.0]
                ),
                reduced_costs=sparse_containers_pb2.SparseDoubleVectorProto(
                    ids=[0, 1], values=[10.0, 11.0]
                ),
            ),
            solution_pb2.DualRayProto(
                dual_values=sparse_containers_pb2.SparseDoubleVectorProto(
                    ids=[0], values=[5.0]
                ),
                reduced_costs=sparse_containers_pb2.SparseDoubleVectorProto(
                    ids=[0, 1], values=[11.0, 12.0]
                ),
            ),
        ],
    )
    proto.solve_stats.node_count = 10
    proto.solve_stats.problem_status.primal_status = (
        result_pb2.FEASIBILITY_STATUS_INFEASIBLE
    )
    proto.solve_stats.problem_status.dual_status = (
        result_pb2.FEASIBILITY_STATUS_INFEASIBLE
    )
    proto.solve_stats.problem_status.primal_or_dual_infeasible = False
    proto.solve_stats.best_primal_bound = math.inf
    proto.solve_stats.best_dual_bound = -math.inf
    res = result.parse_solve_result(proto, mod)

    self.assertEqual(
        result.TerminationReason.INFEASIBLE, res.termination.reason
    )
    self.assertEqual("", res.termination.detail)
    self.assertIsNone(res.termination.limit)
    self.assertEmpty(res.solutions)
    self.assertLen(res.primal_rays, 2)
    self.assertLen(res.dual_rays, 2)
    self.assertDictEqual({x: 2.0, y: 1.0}, res.primal_rays[0].variable_values)
    self.assertDictEqual({x: 10.0, y: 11.0}, res.dual_rays[0].reduced_costs)
    self.assertDictEqual({c: 4.0}, res.dual_rays[0].dual_values)
    self.assertDictEqual({x: 3.0, y: 2.0}, res.primal_rays[1].variable_values)
    self.assertDictEqual({x: 11.0, y: 12.0}, res.dual_rays[1].reduced_costs)
    self.assertDictEqual({c: 5.0}, res.dual_rays[1].dual_values)

    # solve_stats
    self.assertEqual(10, res.solve_stats.node_count)
    self.assertEqual(
        result.FeasibilityStatus.INFEASIBLE,
        res.termination.problem_status.primal_status,
    )
    self.assertEqual(
        result.FeasibilityStatus.INFEASIBLE,
        res.termination.problem_status.dual_status,
    )
    self.assertFalse(res.termination.problem_status.primal_or_dual_infeasible)
    self.assertEqual(math.inf, res.termination.objective_bounds.primal_bound)
    self.assertEqual(-math.inf, res.termination.objective_bounds.dual_bound)
    self.assertIsNone(res.gscip_specific_output)

  def test_solve_result_from_feasible_multiple_solutions(self) -> None:
    mod = model.Model(name="test_model")
    x = mod.add_binary_variable(name="x")
    y = mod.add_binary_variable(name="y")
    c = mod.add_linear_constraint(lb=0.0, ub=1.0, name="c")

    proto = result_pb2.SolveResultProto(
        termination=result_pb2.TerminationProto(
            reason=result_pb2.TERMINATION_REASON_OPTIMAL,
            problem_status=result_pb2.ProblemStatusProto(
                primal_status=result_pb2.FEASIBILITY_STATUS_FEASIBLE,
                dual_status=result_pb2.FEASIBILITY_STATUS_FEASIBLE,
            ),
            objective_bounds=result_pb2.ObjectiveBoundsProto(
                primal_bound=10.0, dual_bound=20.0
            ),
        ),
        solutions=[
            solution_pb2.SolutionProto(
                primal_solution=solution_pb2.PrimalSolutionProto(
                    objective_value=2.0,
                    variable_values=sparse_containers_pb2.SparseDoubleVectorProto(
                        ids=[0, 1], values=[2.0, 1.0]
                    ),
                    feasibility_status=solution_pb2.SOLUTION_STATUS_FEASIBLE,
                ),
                dual_solution=solution_pb2.DualSolutionProto(
                    objective_value=2.0,
                    dual_values=sparse_containers_pb2.SparseDoubleVectorProto(
                        ids=[0], values=[4.0]
                    ),
                    reduced_costs=sparse_containers_pb2.SparseDoubleVectorProto(
                        ids=[0, 1], values=[10.0, 11.0]
                    ),
                    feasibility_status=solution_pb2.SOLUTION_STATUS_FEASIBLE,
                ),
                basis=solution_pb2.BasisProto(
                    constraint_status=solution_pb2.SparseBasisStatusVector(
                        ids=[0],
                        values=[solution_pb2.BASIS_STATUS_AT_UPPER_BOUND],
                    ),
                    variable_status=solution_pb2.SparseBasisStatusVector(
                        ids=[0, 1],
                        values=[
                            solution_pb2.BASIS_STATUS_BASIC,
                            solution_pb2.BASIS_STATUS_AT_LOWER_BOUND,
                        ],
                    ),
                    basic_dual_feasibility=solution_pb2.SOLUTION_STATUS_FEASIBLE,
                ),
            ),
            solution_pb2.SolutionProto(
                primal_solution=solution_pb2.PrimalSolutionProto(
                    objective_value=3.0,
                    variable_values=sparse_containers_pb2.SparseDoubleVectorProto(
                        ids=[0, 1], values=[3.0, 2.0]
                    ),
                    feasibility_status=solution_pb2.SOLUTION_STATUS_INFEASIBLE,
                )
            ),
            solution_pb2.SolutionProto(
                dual_solution=solution_pb2.DualSolutionProto(
                    objective_value=3.0,
                    dual_values=sparse_containers_pb2.SparseDoubleVectorProto(
                        ids=[0], values=[5.0]
                    ),
                    reduced_costs=sparse_containers_pb2.SparseDoubleVectorProto(
                        ids=[0, 1], values=[11.0, 12.0]
                    ),
                    feasibility_status=solution_pb2.SOLUTION_STATUS_INFEASIBLE,
                )
            ),
            solution_pb2.SolutionProto(
                basis=solution_pb2.BasisProto(
                    constraint_status=solution_pb2.SparseBasisStatusVector(
                        ids=[0], values=[solution_pb2.BASIS_STATUS_BASIC]
                    ),
                    variable_status=solution_pb2.SparseBasisStatusVector(
                        ids=[0, 1],
                        values=[
                            solution_pb2.BASIS_STATUS_AT_LOWER_BOUND,
                            solution_pb2.BASIS_STATUS_AT_UPPER_BOUND,
                        ],
                    ),
                    basic_dual_feasibility=solution_pb2.SOLUTION_STATUS_INFEASIBLE,
                )
            ),
        ],
    )

    proto.solve_stats.node_count = 10
    proto.solve_stats.problem_status.primal_status = (
        result_pb2.FEASIBILITY_STATUS_FEASIBLE
    )
    proto.solve_stats.problem_status.dual_status = (
        result_pb2.FEASIBILITY_STATUS_FEASIBLE
    )
    proto.solve_stats.problem_status.primal_or_dual_infeasible = False
    proto.solve_stats.best_primal_bound = 10
    proto.solve_stats.best_dual_bound = 10
    res = result.parse_solve_result(proto, mod)

    self.assertEqual(result.TerminationReason.OPTIMAL, res.termination.reason)
    self.assertEqual("", res.termination.detail)
    self.assertIsNone(res.termination.limit)
    self.assertLen(res.solutions, 4)
    self.assertEmpty(res.primal_rays)
    self.assertEmpty(res.dual_rays)

    # Solution 0
    assert (
        res.solutions[0].primal_solution is not None
        and res.solutions[0].dual_solution is not None
        and res.solutions[0].basis is not None
    )
    self.assertEqual(2.0, res.solutions[0].primal_solution.objective_value)
    self.assertDictEqual(
        {x: 2.0, y: 1.0}, res.solutions[0].primal_solution.variable_values
    )
    self.assertEqual(
        solution.SolutionStatus.FEASIBLE,
        res.solutions[0].primal_solution.feasibility_status,
    )
    self.assertEqual(2.0, res.solutions[0].dual_solution.objective_value)
    self.assertDictEqual(
        {x: 10.0, y: 11.0}, res.solutions[0].dual_solution.reduced_costs
    )
    self.assertDictEqual({c: 4.0}, res.solutions[0].dual_solution.dual_values)
    self.assertEqual(
        solution.SolutionStatus.FEASIBLE,
        res.solutions[0].dual_solution.feasibility_status,
    )
    self.assertDictEqual(
        {x: solution.BasisStatus.BASIC, y: solution.BasisStatus.AT_LOWER_BOUND},
        res.solutions[0].basis.variable_status,
    )
    self.assertDictEqual(
        {c: solution.BasisStatus.AT_UPPER_BOUND},
        res.solutions[0].basis.constraint_status,
    )
    self.assertEqual(
        solution.SolutionStatus.FEASIBLE,
        res.solutions[0].basis.basic_dual_feasibility,
    )

    # Solution 1
    assert res.solutions[1].primal_solution is not None
    self.assertEqual(3.0, res.solutions[1].primal_solution.objective_value)
    self.assertDictEqual(
        {x: 3.0, y: 2.0}, res.solutions[1].primal_solution.variable_values
    )
    self.assertEqual(
        solution.SolutionStatus.INFEASIBLE,
        res.solutions[1].primal_solution.feasibility_status,
    )
    self.assertIsNone(res.solutions[1].dual_solution)
    self.assertIsNone(res.solutions[1].basis)

    # Solution 2
    assert res.solutions[2].dual_solution is not None
    self.assertIsNone(res.solutions[2].primal_solution)
    self.assertEqual(3.0, res.solutions[2].dual_solution.objective_value)
    self.assertDictEqual(
        {x: 11.0, y: 12.0}, res.solutions[2].dual_solution.reduced_costs
    )
    self.assertDictEqual({c: 5.0}, res.solutions[2].dual_solution.dual_values)
    self.assertEqual(
        solution.SolutionStatus.INFEASIBLE,
        res.solutions[2].dual_solution.feasibility_status,
    )
    self.assertIsNone(res.solutions[2].basis)

    # Solution 3
    assert res.solutions[3].basis is not None
    self.assertIsNone(res.solutions[3].primal_solution)
    self.assertIsNone(res.solutions[3].dual_solution)
    self.assertDictEqual(
        {
            x: solution.BasisStatus.AT_LOWER_BOUND,
            y: solution.BasisStatus.AT_UPPER_BOUND,
        },
        res.solutions[3].basis.variable_status,
    )
    self.assertDictEqual(
        {c: solution.BasisStatus.BASIC},
        res.solutions[3].basis.constraint_status,
    )
    self.assertEqual(
        solution.SolutionStatus.INFEASIBLE,
        res.solutions[3].basis.basic_dual_feasibility,
    )

    # solve_stats
    self.assertEqual(10, res.solve_stats.node_count)
    self.assertEqual(
        result.FeasibilityStatus.FEASIBLE,
        res.termination.problem_status.primal_status,
    )
    self.assertEqual(
        result.FeasibilityStatus.FEASIBLE,
        res.termination.problem_status.dual_status,
    )
    self.assertFalse(res.termination.problem_status.primal_or_dual_infeasible)
    self.assertEqual(10, res.termination.objective_bounds.primal_bound)
    self.assertEqual(20, res.termination.objective_bounds.dual_bound)
    self.assertIsNone(res.gscip_specific_output)

  def test_to_proto_round_trip(self) -> None:
    mod = model.Model(name="test_model")
    x = mod.add_binary_variable(name="x")
    c = mod.add_linear_constraint(lb=0.0, ub=1.0, name="c")

    s = solution.Solution(
        primal_solution=solution.PrimalSolution(
            variable_values={x: 1.0},
            objective_value=2.0,
            feasibility_status=solution.SolutionStatus.FEASIBLE,
        )
    )
    r = result.SolveResult(
        termination=result.Termination(
            reason=result.TerminationReason.FEASIBLE,
            limit=result.Limit.TIME,
            problem_status=result.ProblemStatus(
                primal_status=result.FeasibilityStatus.FEASIBLE,
                dual_status=result.FeasibilityStatus.UNDETERMINED,
            ),
        ),
        solve_stats=result.SolveStats(
            node_count=3, solve_time=datetime.timedelta(seconds=4)
        ),
        solutions=[s],
        primal_rays=[solution.PrimalRay(variable_values={x: 4.0})],
        dual_rays=[
            solution.DualRay(reduced_costs={x: 5.0}, dual_values={c: 6.0})
        ],
    )

    s_proto = solution_pb2.SolutionProto(
        primal_solution=solution_pb2.PrimalSolutionProto(
            objective_value=2.0,
            feasibility_status=solution_pb2.SOLUTION_STATUS_FEASIBLE,
            variable_values=sparse_containers_pb2.SparseDoubleVectorProto(
                ids=[0], values=[1.0]
            ),
        )
    )
    r_proto = result_pb2.SolveResultProto(
        termination=result_pb2.TerminationProto(
            reason=result_pb2.TERMINATION_REASON_FEASIBLE,
            limit=result_pb2.LIMIT_TIME,
            problem_status=result_pb2.ProblemStatusProto(
                primal_status=result_pb2.FEASIBILITY_STATUS_FEASIBLE,
                dual_status=result_pb2.FEASIBILITY_STATUS_UNDETERMINED,
            ),
        ),
        solve_stats=result_pb2.SolveStatsProto(node_count=3),
        solutions=[s_proto],
        primal_rays=[
            solution_pb2.PrimalRayProto(
                variable_values=sparse_containers_pb2.SparseDoubleVectorProto(
                    ids=[0], values=[4.0]
                )
            )
        ],
        dual_rays=[
            solution_pb2.DualRayProto(
                reduced_costs=sparse_containers_pb2.SparseDoubleVectorProto(
                    ids=[0], values=[5.0]
                ),
                dual_values=sparse_containers_pb2.SparseDoubleVectorProto(
                    ids=[0], values=[6.0]
                ),
            )
        ],
    )
    r_proto.solve_stats.solve_time.FromTimedelta(datetime.timedelta(seconds=4))

    self.assert_protos_equiv(r.to_proto(), r_proto)
    self.assertEqual(result.parse_solve_result(r_proto, mod), r)

  def test_solution_validation(self) -> None:
    mod = model.Model(name="test_model")
    proto = result_pb2.SolveResultProto(
        termination=result_pb2.TerminationProto(
            reason=result_pb2.TERMINATION_REASON_OPTIMAL,
            problem_status=result_pb2.ProblemStatusProto(
                primal_status=result_pb2.FEASIBILITY_STATUS_FEASIBLE,
                dual_status=result_pb2.FEASIBILITY_STATUS_FEASIBLE,
            ),
        ),
        solutions=[
            solution_pb2.SolutionProto(
                primal_solution=solution_pb2.PrimalSolutionProto(
                    variable_values=sparse_containers_pb2.SparseDoubleVectorProto(
                        ids=[2], values=[4.0]
                    ),
                    feasibility_status=solution_pb2.SOLUTION_STATUS_FEASIBLE,
                )
            )
        ],
    )
    res = result.parse_solve_result(proto, mod, validate=False)
    bad_var = mod.get_variable(2, validate=False)
    self.assertLen(res.solutions, 1)
    # TODO: b/215588365 - make a local variable so pytype is happy
    primal = res.solutions[0].primal_solution
    self.assertIsNotNone(primal)
    self.assertDictEqual(primal.variable_values, {bad_var: 4.0})
    with self.assertRaises(KeyError):
      result.parse_solve_result(proto, mod, validate=True)

  def test_primal_ray_validation(self) -> None:
    mod = model.Model(name="test_model")
    proto = result_pb2.SolveResultProto(
        termination=result_pb2.TerminationProto(
            reason=result_pb2.TERMINATION_REASON_UNBOUNDED,
            problem_status=result_pb2.ProblemStatusProto(
                primal_status=result_pb2.FEASIBILITY_STATUS_FEASIBLE,
                dual_status=result_pb2.FEASIBILITY_STATUS_INFEASIBLE,
            ),
        ),
        primal_rays=[
            solution_pb2.PrimalRayProto(
                variable_values=sparse_containers_pb2.SparseDoubleVectorProto(
                    ids=[2], values=[4.0]
                )
            )
        ],
    )
    res = result.parse_solve_result(proto, mod, validate=False)
    bad_var = mod.get_variable(2, validate=False)
    self.assertLen(res.primal_rays, 1)
    self.assertDictEqual(res.primal_rays[0].variable_values, {bad_var: 4.0})
    with self.assertRaises(KeyError):
      result.parse_solve_result(proto, mod, validate=True)

  def test_dual_ray_validation(self) -> None:
    mod = model.Model(name="test_model")
    proto = result_pb2.SolveResultProto(
        termination=result_pb2.TerminationProto(
            reason=result_pb2.TERMINATION_REASON_INFEASIBLE,
            problem_status=result_pb2.ProblemStatusProto(
                primal_status=result_pb2.FEASIBILITY_STATUS_INFEASIBLE,
                dual_status=result_pb2.FEASIBILITY_STATUS_FEASIBLE,
            ),
        ),
        dual_rays=[
            solution_pb2.DualRayProto(
                reduced_costs=sparse_containers_pb2.SparseDoubleVectorProto(
                    ids=[2], values=[4.0]
                )
            )
        ],
    )
    res = result.parse_solve_result(proto, mod, validate=False)
    bad_var = mod.get_variable(2, validate=False)
    self.assertLen(res.dual_rays, 1)
    self.assertDictEqual(res.dual_rays[0].reduced_costs, {bad_var: 4.0})
    with self.assertRaises(KeyError):
      result.parse_solve_result(proto, mod, validate=True)


if __name__ == "__main__":
  absltest.main()
