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

import copy
import itertools
import sys
import time

from absl.testing import absltest
import numpy as np
import pandas as pd

from ortools.sat import cp_model_pb2
from ortools.sat.python import cp_model
from ortools.sat.python import cp_model_helper as cmh


class SolutionCounter(cp_model.CpSolverSolutionCallback):
    """Count solutions."""

    def __init__(self) -> None:
        cp_model.CpSolverSolutionCallback.__init__(self)
        self.__solution_count = 0

    def on_solution_callback(self) -> None:
        self.__solution_count += 1

    @property
    def solution_count(self) -> int:
        return self.__solution_count


class SolutionSum(cp_model.CpSolverSolutionCallback):
    """Record the sum of variables in the solution."""

    def __init__(self, variables: list[cp_model.IntVar]) -> None:
        cp_model.CpSolverSolutionCallback.__init__(self)
        self.__sum: int = 0
        self.__vars = variables

    def on_solution_callback(self) -> None:
        self.__sum = sum(self.value(x) for x in self.__vars)

    @property
    def sum(self) -> int:
        return self.__sum


class SolutionFloatValue(cp_model.CpSolverSolutionCallback):
    """Record the evaluation of a float expression in the solution."""

    def __init__(self, expr: cp_model.LinearExpr) -> None:
        cp_model.CpSolverSolutionCallback.__init__(self)
        self.__expr: cp_model.LinearExpr = expr
        self.__value: float = 0.0

    def on_solution_callback(self) -> None:
        self.__value = self.float_value(self.__expr)

    @property
    def value(self) -> float:
        return self.__value


class SolutionObjective(cp_model.CpSolverSolutionCallback):
    """Record the objective value of the solution."""

    def __init__(self) -> None:
        cp_model.CpSolverSolutionCallback.__init__(self)
        self.__obj: float = 0

    def on_solution_callback(self) -> None:
        self.__obj = self.objective_value

    @property
    def obj(self) -> float:
        return self.__obj


class RecordSolution(cp_model.CpSolverSolutionCallback):
    """Record the objective value of the solution."""

    def __init__(
        self,
        int_vars: list[cp_model.VariableT],
        bool_vars: list[cp_model.LiteralT],
    ) -> None:
        cp_model.CpSolverSolutionCallback.__init__(self)
        self.__int_vars = int_vars
        self.__bool_vars = bool_vars
        self.__int_var_values: list[int] = []
        self.__bool_var_values: list[bool] = []

    def on_solution_callback(self) -> None:
        for int_var in self.__int_vars:
            self.__int_var_values.append(self.value(int_var))
        for bool_var in self.__bool_vars:
            self.__bool_var_values.append(self.boolean_value(bool_var))

    @property
    def int_var_values(self) -> list[int]:
        return self.__int_var_values

    @property
    def bool_var_values(self) -> list[bool]:
        return self.__bool_var_values


class TimeRecorder(cp_model.CpSolverSolutionCallback):

    def __init__(self) -> None:
        super().__init__()
        self.__last_time: float = 0.0

    def on_solution_callback(self) -> None:
        self.__last_time = time.time()

    @property
    def last_time(self) -> float:
        return self.__last_time


class RaiseException(cp_model.CpSolverSolutionCallback):

    def __init__(self, msg: str) -> None:
        super().__init__()
        self.__msg = msg

    def on_solution_callback(self) -> None:
        raise ValueError(self.__msg)


class LogToString:
    """Record log in a string."""

    def __init__(self) -> None:
        self.__log = ""

    def new_message(self, message: str) -> None:
        self.__log += message
        self.__log += "\n"

    @property
    def log(self) -> str:
        return self.__log


class BestBoundCallback:

    def __init__(self) -> None:
        self.best_bound: float = 0.0

    def new_best_bound(self, bb: float) -> None:
        self.best_bound = bb


class BestBoundTimeCallback:

    def __init__(self) -> None:
        self.__last_time: float = 0.0

    def new_best_bound(self, unused_bb: float):
        self.__last_time = time.time()

    @property
    def last_time(self) -> float:
        return self.__last_time


class CpModelTest(absltest.TestCase):

    def tearDown(self) -> None:
        super().tearDown()
        sys.stdout.flush()

    def test_create_integer_variable(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        self.assertEqual("x", str(x))
        self.assertEqual("x(-10..10)", repr(x))
        y = model.new_int_var_from_domain(
            cp_model.Domain.from_intervals([[2, 4], [7]]), "y"
        )
        self.assertEqual("y", str(y))
        self.assertEqual("y(2..4, 7)", repr(y))
        z = model.new_int_var_from_domain(
            cp_model.Domain.from_values([2, 3, 4, 7]), "z"
        )
        self.assertEqual("z", str(z))
        self.assertEqual("z(2..4, 7)", repr(z))
        t = model.new_int_var_from_domain(
            cp_model.Domain.from_flat_intervals([2, 4, 7, 7]), "t"
        )
        self.assertEqual("t", str(t))
        self.assertEqual("t(2..4, 7)", repr(t))
        cst = model.new_constant(5)
        self.assertEqual("5", str(cst))

    def test_hash_int_var(self) -> None:
        model = cp_model.CpModel()
        var_a = model.new_int_var(0, 2, "a")
        variables = set()
        variables.add(var_a)

    def test_literal(self) -> None:
        model = cp_model.CpModel()
        x = model.new_bool_var("x")
        self.assertEqual("x", str(x))
        self.assertEqual("not(x)", str(~x))
        self.assertEqual("not(x)", str(x.negated()))
        self.assertEqual(x.negated().negated(), x)
        self.assertEqual(x.negated().negated().index, x.index)
        y = model.new_int_var(0, 1, "y")
        self.assertEqual("y", str(y))
        self.assertEqual("not(y)", str(~y))
        zero = model.new_constant(0)
        self.assertEqual("0", str(zero))
        self.assertEqual("not(0)", str(~zero))
        one = model.new_constant(1)
        self.assertEqual("1", str(one))
        self.assertEqual("not(1)", str(~one))
        z = model.new_int_var(0, 2, "z")
        self.assertRaises(TypeError, z.negated)
        self.assertRaises(TypeError, z.__invert__)

    def test_negation(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        b = model.new_bool_var("b")
        nb = b.negated()
        self.assertEqual(b.negated(), nb)
        self.assertEqual(~b, nb)
        self.assertEqual(b.negated().negated(), b)
        self.assertEqual(~(~b), b)
        self.assertEqual(nb.index, -b.index - 1)
        self.assertRaises(TypeError, x.negated)

    def test_issue_4654(self) -> None:
        model = cp_model.CpModel()
        x = model.NewIntVar(0, 1, "x")
        y = model.NewIntVar(0, 2, "y")
        z = model.NewIntVar(0, 3, "z")
        expr = x - y - 2 * z
        self.assertEqual(str(expr), "(x + (-y) + (-(2 * z)))")

    def test_equality_overload(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        y = model.new_int_var(0, 5, "y")
        self.assertEqual(x, x)
        self.assertNotEqual(x, y)

    def test_linear(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        y = model.new_int_var(-10, 10, "y")
        model.add_linear_constraint(x + 2 * y, 0, 10)
        model.minimize(y)
        solver = cp_model.CpSolver()
        self.assertEqual(cp_model.OPTIMAL, solver.solve(model))
        self.assertEqual(10, solver.value(x))
        self.assertEqual(-5, solver.value(y))

    def test_none_argument(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        y = model.new_int_var(-10, 10, "y")
        model.add_linear_constraint(x + 2 * y, 0, 10)
        model.minimize(y)
        solver = cp_model.CpSolver()
        self.assertEqual(cp_model.OPTIMAL, solver.solve(model))
        self.assertRaises(TypeError, solver.value, None)
        self.assertRaises(TypeError, solver.float_value, None)
        self.assertRaises(TypeError, solver.boolean_value, None)

    def test_linear_constraint(self) -> None:
        model = cp_model.CpModel()
        model.add_linear_constraint(5, 0, 10)
        model.add_linear_constraint(-1, 0, 10)
        self.assertLen(model.proto.constraints, 2)
        self.assertTrue(model.proto.constraints[0].HasField("bool_and"))
        self.assertEmpty(model.proto.constraints[0].bool_and.literals)
        self.assertTrue(model.proto.constraints[1].HasField("bool_or"))
        self.assertEmpty(model.proto.constraints[1].bool_or.literals)

    def test_linear_non_equal(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        y = model.new_int_var(-10, 10, "y")
        ct = model.add(-x + y != 3).proto
        self.assertLen(ct.linear.domain, 4)
        self.assertEqual(cp_model.INT_MIN, ct.linear.domain[0])
        self.assertEqual(2, ct.linear.domain[1])
        self.assertEqual(4, ct.linear.domain[2])
        self.assertEqual(cp_model.INT_MAX, ct.linear.domain[3])

    def test_eq(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        ct = model.add(x == 2).proto
        self.assertLen(ct.linear.vars, 1)
        self.assertLen(ct.linear.coeffs, 1)
        self.assertLen(ct.linear.domain, 2)
        self.assertEqual(2, ct.linear.domain[0])
        self.assertEqual(2, ct.linear.domain[1])

    def testGe(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        ct = model.add(x >= 2).proto
        self.assertLen(ct.linear.vars, 1)
        self.assertLen(ct.linear.coeffs, 1)
        self.assertLen(ct.linear.domain, 2)
        self.assertEqual(2, ct.linear.domain[0])
        self.assertEqual(cp_model.INT_MAX, ct.linear.domain[1])

    def test_gt(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        ct = model.add(x > 2).proto
        self.assertLen(ct.linear.vars, 1)
        self.assertLen(ct.linear.coeffs, 1)
        self.assertLen(ct.linear.domain, 2)
        self.assertEqual(3, ct.linear.domain[0])
        self.assertEqual(cp_model.INT_MAX, ct.linear.domain[1])

    def test_le(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        ct = model.add(x <= 2).proto
        self.assertLen(ct.linear.vars, 1)
        self.assertLen(ct.linear.coeffs, 1)
        self.assertLen(ct.linear.domain, 2)
        self.assertEqual(cp_model.INT_MIN, ct.linear.domain[0])
        self.assertEqual(2, ct.linear.domain[1])

    def test_lt(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        ct = model.add(x < 2).proto
        self.assertLen(ct.linear.vars, 1)
        self.assertLen(ct.linear.coeffs, 1)
        self.assertLen(ct.linear.domain, 2)
        self.assertEqual(cp_model.INT_MIN, ct.linear.domain[0])
        self.assertEqual(1, ct.linear.domain[1])

    def test_eq_var(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        y = model.new_int_var(-10, 10, "y")
        ct = model.add(x == y + 2).proto
        self.assertLen(ct.linear.vars, 2)
        self.assertEqual(1, ct.linear.vars[0] + ct.linear.vars[1])
        self.assertLen(ct.linear.coeffs, 2)
        self.assertEqual(0, ct.linear.coeffs[0] + ct.linear.coeffs[1])
        self.assertLen(ct.linear.domain, 2)
        self.assertEqual(2, ct.linear.domain[0])
        self.assertEqual(2, ct.linear.domain[1])

    def test_ge_var(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        y = model.new_int_var(-10, 10, "y")
        ct = model.add(x >= 1 - y).proto
        self.assertLen(ct.linear.vars, 2)
        self.assertEqual(1, ct.linear.vars[0] + ct.linear.vars[1])
        self.assertLen(ct.linear.coeffs, 2)
        self.assertEqual(1, ct.linear.coeffs[0])
        self.assertEqual(1, ct.linear.coeffs[1])
        self.assertLen(ct.linear.domain, 2)
        self.assertEqual(1, ct.linear.domain[0])
        self.assertEqual(cp_model.INT_MAX, ct.linear.domain[1])

    def test_gt_var(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        y = model.new_int_var(-10, 10, "y")
        ct = model.add(x > 1 - y).proto
        self.assertLen(ct.linear.vars, 2)
        self.assertEqual(1, ct.linear.vars[0] + ct.linear.vars[1])
        self.assertLen(ct.linear.coeffs, 2)
        self.assertEqual(1, ct.linear.coeffs[0])
        self.assertEqual(1, ct.linear.coeffs[1])
        self.assertLen(ct.linear.domain, 2)
        self.assertEqual(2, ct.linear.domain[0])
        self.assertEqual(cp_model.INT_MAX, ct.linear.domain[1])

    def test_le_var(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        y = model.new_int_var(-10, 10, "y")
        ct = model.add(x <= 1 - y).proto
        self.assertLen(ct.linear.vars, 2)
        self.assertEqual(1, ct.linear.vars[0] + ct.linear.vars[1])
        self.assertLen(ct.linear.coeffs, 2)
        self.assertEqual(1, ct.linear.coeffs[0])
        self.assertEqual(1, ct.linear.coeffs[1])
        self.assertLen(ct.linear.domain, 2)
        self.assertEqual(cp_model.INT_MIN, ct.linear.domain[0])
        self.assertEqual(1, ct.linear.domain[1])

    def test_lt_var(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        y = model.new_int_var(-10, 10, "y")
        ct = model.add(x < 1 - y).proto
        self.assertLen(ct.linear.vars, 2)
        self.assertEqual(1, ct.linear.vars[0] + ct.linear.vars[1])
        self.assertLen(ct.linear.coeffs, 2)
        self.assertEqual(1, ct.linear.coeffs[0])
        self.assertEqual(1, ct.linear.coeffs[1])
        self.assertLen(ct.linear.domain, 2)
        self.assertEqual(cp_model.INT_MIN, ct.linear.domain[0])
        self.assertEqual(0, ct.linear.domain[1])

    def test_linear_non_equal_with_constant(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        y = model.new_int_var(-10, 10, "y")
        ct = model.add(x + y + 5 != 3).proto
        self.assertLen(ct.linear.domain, 4)
        # Checks that saturated arithmetics worked.
        self.assertEqual(cp_model.INT_MIN, ct.linear.domain[0])
        self.assertEqual(-3, ct.linear.domain[1])
        self.assertEqual(-1, ct.linear.domain[2])
        self.assertEqual(cp_model.INT_MAX, ct.linear.domain[3])

    def test_linear_with_enforcement(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        y = model.new_int_var(-10, 10, "y")
        b = model.new_bool_var("b")
        model.add_linear_constraint(x + 2 * y, 0, 10).only_enforce_if(b.negated())
        model.minimize(y)
        self.assertLen(model.proto.constraints, 1)
        self.assertEqual(-3, model.proto.constraints[0].enforcement_literal[0])
        c = model.new_bool_var("c")
        model.add_linear_constraint(x + 4 * y, 0, 10).only_enforce_if([b, c])
        self.assertLen(model.proto.constraints, 2)
        self.assertEqual(2, model.proto.constraints[1].enforcement_literal[0])
        self.assertEqual(3, model.proto.constraints[1].enforcement_literal[1])
        model.add_linear_constraint(x + 5 * y, 0, 10).only_enforce_if(c.negated(), b)
        self.assertLen(model.proto.constraints, 3)
        self.assertEqual(-4, model.proto.constraints[2].enforcement_literal[0])
        self.assertEqual(2, model.proto.constraints[2].enforcement_literal[1])

    def test_names(self) -> None:
        model = cp_model.CpModel()
        model.name = "test_model"
        x = model.new_int_var(-10, 10, "x")
        y = model.new_int_var(-10, 10, "y")
        ct = model.add_linear_constraint(x + 2 * y, 0, 10).with_name("test_constraint")
        self.assertEqual(model.name, "test_model")
        self.assertEqual(x.name, "x")
        self.assertEqual("test_constraint", ct.name)
        model.remove_all_names()
        self.assertEmpty(model.name)
        self.assertEmpty(x.name)
        self.assertEmpty(ct.name)

    def test_natural_api_minimize(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        y = model.new_int_var(-10, 10, "y")
        model.add(x * 2 - 1 * y == 1)
        model.minimize(x * 1 - 2 * y + 3)
        solver = cp_model.CpSolver()
        self.assertEqual("OPTIMAL", solver.status_name(solver.solve(model)))
        self.assertEqual(5, solver.value(x))
        self.assertEqual(15, solver.value(x * 3))
        self.assertEqual(6, solver.value(1 + x))
        self.assertEqual(-10.0, solver.objective_value)

    def test_natural_api_maximize_float(self) -> None:
        model = cp_model.CpModel()
        x = model.new_bool_var("x")
        y = model.new_int_var(0, 10, "y")
        model.maximize(x.negated() * 3.5 + x.negated() - y + 2 * y + 1.6)
        solver = cp_model.CpSolver()
        self.assertEqual("OPTIMAL", solver.status_name(solver.solve(model)))
        self.assertFalse(solver.boolean_value(x))
        self.assertTrue(solver.boolean_value(x.negated()))
        self.assertEqual(-10, solver.value(-y))
        self.assertEqual(16.1, solver.objective_value)

    def test_natural_api_maximize_complex(self) -> None:
        model = cp_model.CpModel()
        x1 = model.new_bool_var("x1")
        x2 = model.new_bool_var("x1")
        x3 = model.new_bool_var("x1")
        x4 = model.new_bool_var("x1")
        model.maximize(
            cp_model.LinearExpr.sum([x1, x2])
            + cp_model.LinearExpr.weighted_sum([x3, x4.negated()], [2, 4])
        )
        solver = cp_model.CpSolver()
        self.assertEqual("OPTIMAL", solver.status_name(solver.solve(model)))
        self.assertEqual(5, solver.value(3 + 2 * x1))
        self.assertEqual(3, solver.value(x1 + x2 + x3))
        self.assertEqual(1, solver.value(cp_model.LinearExpr.sum([x1, x2, x3, 0, -2])))
        self.assertEqual(
            7,
            solver.value(
                cp_model.LinearExpr.weighted_sum([x1, x2, x4, 3], [2, 2, 2, 1])
            ),
        )
        self.assertEqual(5, solver.value(5 * x4.negated()))
        self.assertEqual(8, solver.objective_value)

    def test_natural_api_maximize(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        y = model.new_int_var(-10, 10, "y")
        model.add(2 * x - y == 1)
        model.maximize(x - 2 * y + 3)
        solver = cp_model.CpSolver()
        self.assertEqual("OPTIMAL", solver.status_name(solver.solve(model)))
        self.assertEqual(-4, solver.value(x))
        self.assertEqual(-9, solver.value(y))
        self.assertEqual(17, solver.objective_value)

    def test_minimize_constant(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        model.add(x >= -1)
        model.minimize(10)
        solver = cp_model.CpSolver()
        self.assertEqual("OPTIMAL", solver.status_name(solver.solve(model)))
        self.assertEqual(10, solver.objective_value)

    def test_maximize_constant(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        model.add(x >= -1)
        model.maximize(5)
        solver = cp_model.CpSolver()
        self.assertEqual("OPTIMAL", solver.status_name(solver.solve(model)))
        self.assertEqual(5, solver.objective_value)

    def test_add_true(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        model.add(3 >= -1)
        model.minimize(x)
        solver = cp_model.CpSolver()
        self.assertEqual("OPTIMAL", solver.status_name(solver.solve(model)))
        self.assertEqual(-10, solver.value(x))

    def test_add_false(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        model.add(3 <= -1)
        model.minimize(x)
        solver = cp_model.CpSolver()
        self.assertEqual("INFEASIBLE", solver.status_name(solver.solve(model)))

    def test_sum(self) -> None:
        model = cp_model.CpModel()
        x = [model.new_int_var(0, 2, f"x{i}") for i in range(100)]
        model.add(sum(x) <= 1)
        model.maximize(x[99])
        solver = cp_model.CpSolver()
        self.assertEqual(cp_model.OPTIMAL, solver.solve(model))
        self.assertEqual(1.0, solver.objective_value)
        for i in range(100):
            self.assertEqual(solver.value(x[i]), 1 if i == 99 else 0)

    def test_sum_parsing(self) -> None:
        model = cp_model.CpModel()
        x = [model.new_int_var(0, 2, f"x{i}") for i in range(5)]
        s1 = cp_model.LinearExpr.sum(x)
        self.assertTrue(s1.is_integer())
        flat_s1 = cp_model.FlatIntExpr(s1)
        self.assertLen(flat_s1.vars, 5)
        self.assertEqual(0, flat_s1.offset)

        s2 = cp_model.LinearExpr.sum(x[0], x[2], x[4])
        self.assertTrue(s2.is_integer())
        flat_s2 = cp_model.FlatIntExpr(s2)
        self.assertLen(flat_s2.vars, 3)
        self.assertEqual(0, flat_s2.offset)

        s3 = cp_model.LinearExpr.sum(x[0], x[2], 2, x[4], -4)
        self.assertTrue(s3.is_integer())
        flat_s3 = cp_model.FlatIntExpr(s3)
        self.assertLen(flat_s3.vars, 3)
        self.assertEqual(-2, flat_s3.offset)

        s4 = cp_model.LinearExpr.sum(x[0], x[2], 2.5)
        self.assertFalse(s4.is_integer())
        flat_s4 = cp_model.FlatFloatExpr(s4)
        self.assertLen(flat_s4.vars, 2)
        self.assertEqual(2.5, flat_s4.offset)

        s5 = cp_model.LinearExpr.sum(x[0], x[2], 2, 1.5)
        self.assertFalse(s5.is_integer())
        flat_s5 = cp_model.FlatFloatExpr(s5)
        self.assertLen(flat_s5.vars, 2)
        self.assertEqual(3.5, flat_s5.offset)
        self.assertEqual(str(s5), "(x0 + x2 + 3.5)")

        s5b = cp_model.LinearExpr.sum(x[0], x[2], 2, -2.5)
        self.assertFalse(s5b.is_integer())
        self.assertEqual(str(s5b), "(x0 + x2 - 0.5)")
        flat_s5b = cp_model.FlatFloatExpr(s5b)
        self.assertLen(flat_s5b.vars, 2)
        self.assertEqual(-0.5, flat_s5b.offset)

        s6 = cp_model.LinearExpr.sum(x[0], x[2], np.int8(-1), np.int64(-4))
        self.assertTrue(s6.is_integer())
        flat_s6 = cp_model.FlatIntExpr(s6)
        self.assertLen(flat_s6.vars, 2)
        self.assertEqual(-5, flat_s6.offset)

        s7 = cp_model.LinearExpr.sum(x[0], x[2], np.float64(2.0), np.float32(1.5))
        self.assertFalse(s7.is_integer())
        flat_s7 = cp_model.FlatFloatExpr(s7)
        self.assertLen(flat_s7.vars, 2)
        self.assertEqual(3.5, flat_s7.offset)

        s8 = cp_model.LinearExpr.sum(x[0], 3)
        self.assertTrue(s8.is_integer())
        self.assertIsInstance(s8, cmh.IntAffine)
        self.assertEqual(s8.expression, x[0])
        self.assertEqual(s8.coefficient, 1)
        self.assertEqual(s8.offset, 3)

        s9 = cp_model.LinearExpr.sum(x[0], -2.1)
        self.assertFalse(s9.is_integer())
        self.assertIsInstance(s9, cmh.FloatAffine)
        self.assertEqual(s9.expression, x[0])
        self.assertEqual(s9.coefficient, 1.0)
        self.assertEqual(s9.offset, -2.1)
        self.assertEqual(str(s9), "(x0 - 2.1)")

        s10 = cp_model.LinearExpr.sum(x[0], 1, -1)
        self.assertTrue(s10.is_integer())
        self.assertIsInstance(s10, cp_model.IntVar)
        self.assertEqual(s10, x[0])

        s11 = cp_model.LinearExpr.sum(x[0])
        self.assertTrue(s11.is_integer())
        self.assertIsInstance(s11, cp_model.IntVar)
        self.assertEqual(s11, x[0])

        s12 = cp_model.LinearExpr.sum(x[0], -x[2], -3)
        self.assertEqual(str(s12), "(x0 + (-x2) - 3)")
        self.assertEqual(
            repr(s12),
            "SumArray(x0(0..2), IntAffine(expr=x2(0..2), coeff=-1, offset=0),"
            " int_offset=-3)",
        )
        flat_int_s12 = cp_model.FlatIntExpr(s12)
        self.assertEqual(str(flat_int_s12), "(x0 - x2 - 3)")
        self.assertEqual(
            repr(flat_int_s12),
            "FlatIntExpr([x0(0..2), x2(0..2)], [1, -1], -3)",
        )
        flat_float_s12 = cp_model.FlatFloatExpr(s12)
        self.assertEqual(str(flat_float_s12), "(x0 - x2 - 3)")
        self.assertEqual(
            repr(flat_float_s12),
            "FlatFloatExpr([x0(0..2), x2(0..2)], [1, -1], -3)",
        )

        s13 = cp_model.LinearExpr.sum(2)
        self.assertEqual(str(s13), "2")
        self.assertEqual(repr(s13), "IntConstant(2)")

        s14 = cp_model.LinearExpr.sum(2.5)
        self.assertEqual(str(s14), "2.5")
        self.assertEqual(repr(s14), "FloatConstant(2.5)")

        class FakeNpDTypeA:

            def __init__(self):
                self.dtype = 2
                pass

            def __str__(self):
                return "FakeNpDTypeA"

        class FakeNpDTypeB:

            def __init__(self):
                self.is_integer = False
                pass

            def __str__(self):
                return "FakeNpDTypeB"

        with self.assertRaises(TypeError):
            cp_model.LinearExpr.sum(x[0], x[2], "foo")

        with self.assertRaises(TypeError):
            cp_model.LinearExpr.sum(x[0], x[2], FakeNpDTypeA())

        with self.assertRaises(TypeError):
            cp_model.LinearExpr.sum(x[0], x[2], FakeNpDTypeB())

    def test_weighted_sum_parsing(self) -> None:
        model = cp_model.CpModel()
        x = [model.new_int_var(0, 2, f"x{i}") for i in range(5)]
        c = [1, -2, 2, 3, 0.0]
        float_c = [1, -1.0, 2, 3, 0.0]

        s1 = cp_model.LinearExpr.weighted_sum(x, c)
        self.assertTrue(s1.is_integer())
        flat_s1 = cp_model.FlatIntExpr(s1)
        self.assertLen(flat_s1.vars, 4)
        self.assertEqual(0, flat_s1.offset)

        s2 = cp_model.LinearExpr.weighted_sum(x, float_c)
        self.assertFalse(s2.is_integer())
        flat_s2 = cp_model.FlatFloatExpr(s2)
        self.assertLen(flat_s2.vars, 4)
        self.assertEqual(0, flat_s2.offset)

        s3 = cp_model.LinearExpr.weighted_sum(x + [2], c + [-1])
        self.assertTrue(s3.is_integer())
        flat_s3 = cp_model.FlatIntExpr(s3)
        self.assertLen(flat_s3.vars, 4)
        self.assertEqual(-2, flat_s3.offset)

        s4 = cp_model.LinearExpr.weighted_sum(x + [2], float_c + [-1.0])
        self.assertFalse(s4.is_integer())
        flat_s4 = cp_model.FlatFloatExpr(s4)
        self.assertLen(flat_s4.vars, 4)
        self.assertEqual(-2, flat_s4.offset)

        s5 = cp_model.LinearExpr.weighted_sum(x + [np.int16(2)], c + [-1])
        self.assertTrue(s5.is_integer())
        flat_s5 = cp_model.FlatIntExpr(s5)
        self.assertLen(flat_s5.vars, 4)
        self.assertEqual(-2, flat_s5.offset)

        s6 = cp_model.LinearExpr.weighted_sum([2], [1])
        self.assertEqual(repr(s6), "IntConstant(2)")

        s7 = cp_model.LinearExpr.weighted_sum([2], [1.25])
        self.assertEqual(repr(s7), "FloatConstant(2.5)")

    def test_sum_with_api(self) -> None:
        model = cp_model.CpModel()
        x = [model.new_int_var(0, 2, f"x{i}") for i in range(100)]
        self.assertEqual(cp_model.LinearExpr.sum([x[0]]), x[0])
        self.assertEqual(cp_model.LinearExpr.sum([x[0], 0]), x[0])
        self.assertEqual(cp_model.LinearExpr.sum([x[0], 0.0]), x[0])
        self.assertEqual(
            repr(cp_model.LinearExpr.sum([x[0], 2])),
            repr(cp_model.LinearExpr.affine(x[0], 1, 2)),
        )
        model.add(cp_model.LinearExpr.sum(x) <= 1)
        model.maximize(x[99])
        solver = cp_model.CpSolver()
        self.assertEqual(cp_model.OPTIMAL, solver.solve(model))
        self.assertEqual(1.0, solver.objective_value)
        for i in range(100):
            self.assertEqual(solver.value(x[i]), 1 if i == 99 else 0)

    def test_weighted_sum(self) -> None:
        model = cp_model.CpModel()
        x = [model.new_int_var(0, 2, f"x{i}") for i in range(100)]
        c = [2] * 100
        model.add(cp_model.LinearExpr.weighted_sum(x, c) <= 3)
        model.maximize(x[99])
        solver = cp_model.CpSolver()
        self.assertEqual(cp_model.OPTIMAL, solver.solve(model))
        self.assertEqual(1.0, solver.objective_value)
        for i in range(100):
            self.assertEqual(solver.value(x[i]), 1 if i == 99 else 0)

        with self.assertRaises(ValueError):
            cp_model.LinearExpr.weighted_sum([x[0]], [1, 2])
        with self.assertRaises(ValueError):
            cp_model.LinearExpr.weighted_sum([x[0]], [1.1, 2.2])
        with self.assertRaises(ValueError):
            cp_model.LinearExpr.weighted_sum([x[0], 3, 5], [1, 2])
        with self.assertRaises(ValueError):
            cp_model.LinearExpr.weighted_sum([x[0], 2.2, 3], [1.1, 2.2])
        with self.assertRaises(ValueError):
            cp_model.LinearExpr.WeightedSum([x[0]], [1, 2])
        with self.assertRaises(ValueError):
            cp_model.LinearExpr.WeightedSum([x[0]], [1.1, 2.2])

    def test_all_different(self) -> None:
        model = cp_model.CpModel()
        x = [model.new_int_var(0, 4, f"x{i}") for i in range(5)]
        model.add_all_different(x)
        self.assertLen(model.proto.variables, 5)
        self.assertLen(model.proto.constraints, 1)
        self.assertLen(model.proto.constraints[0].all_diff.exprs, 5)

    def test_all_different_gen(self) -> None:
        model = cp_model.CpModel()
        model.add_all_different(model.new_int_var(0, 4, f"x{i}") for i in range(5))
        self.assertLen(model.proto.variables, 5)
        self.assertLen(model.proto.constraints, 1)
        self.assertLen(model.proto.constraints[0].all_diff.exprs, 5)

    def test_all_different_list(self) -> None:
        model = cp_model.CpModel()
        x = [model.new_int_var(0, 4, f"x{i}") for i in range(5)]
        model.add_all_different(x[0], x[1], x[2], x[3], x[4])
        self.assertLen(model.proto.variables, 5)
        self.assertLen(model.proto.constraints, 1)
        self.assertLen(model.proto.constraints[0].all_diff.exprs, 5)

    def test_element(self) -> None:
        model = cp_model.CpModel()
        x = [model.new_int_var(0, 4, f"x{i}") for i in range(5)]
        model.add_element(x[0], [x[1], 2, 4, x[2]], x[4])
        self.assertLen(model.proto.variables, 5)
        self.assertLen(model.proto.constraints, 1)
        self.assertLen(model.proto.constraints[0].element.exprs, 4)
        self.assertEqual(0, model.proto.constraints[0].element.linear_index.vars[0])
        self.assertEqual(4, model.proto.constraints[0].element.linear_target.vars[0])
        with self.assertRaises(ValueError):
            model.add_element(x[0], [], x[4])

    def test_fixed_element(self) -> None:
        model = cp_model.CpModel()
        x = [model.new_int_var(0, 4, f"x{i}") for i in range(4)]
        model.add_element(1, [x[0], 2, 4, x[2]], x[3])
        self.assertLen(model.proto.variables, 4)
        self.assertLen(model.proto.constraints, 1)
        self.assertLen(model.proto.constraints[0].linear.vars, 1)
        self.assertEqual(x[3].index, model.proto.constraints[0].linear.vars[0])
        self.assertEqual(1, model.proto.constraints[0].linear.coeffs[0])
        self.assertEqual([2, 2], model.proto.constraints[0].linear.domain)

    def test_affine_element(self) -> None:
        model = cp_model.CpModel()
        x = [model.new_int_var(0, 4, f"x{i}") for i in range(5)]
        model.add_element(x[0] + 1, [2 * x[1] - 2, 2, 4, x[2]], x[4] - 1)
        self.assertLen(model.proto.variables, 5)
        self.assertLen(model.proto.constraints, 1)
        self.assertLen(model.proto.constraints[0].element.exprs, 4)
        self.assertEqual(0, model.proto.constraints[0].element.linear_index.vars[0])
        self.assertEqual(1, model.proto.constraints[0].element.linear_index.coeffs[0])
        self.assertEqual(1, model.proto.constraints[0].element.linear_index.offset)

        self.assertEqual(4, model.proto.constraints[0].element.linear_target.vars[0])
        self.assertEqual(1, model.proto.constraints[0].element.linear_target.coeffs[0])
        self.assertEqual(-1, model.proto.constraints[0].element.linear_target.offset)
        self.assertEqual(4, model.proto.constraints[0].element.linear_target.vars[0])
        expr0 = model.proto.constraints[0].element.exprs[0]
        self.assertEqual(1, expr0.vars[0])
        self.assertEqual(2, expr0.coeffs[0])
        self.assertEqual(-2, expr0.offset)

    def testCircuit(self) -> None:
        model = cp_model.CpModel()
        x = [model.new_bool_var(f"x{i}") for i in range(5)]
        arcs: list[tuple[int, int, cp_model.LiteralT]] = [
            (i, i + 1, x[i]) for i in range(5)
        ]
        model.add_circuit(arcs)
        self.assertLen(model.proto.variables, 5)
        self.assertLen(model.proto.constraints, 1)
        self.assertLen(model.proto.constraints[0].circuit.heads, 5)
        self.assertLen(model.proto.constraints[0].circuit.tails, 5)
        self.assertLen(model.proto.constraints[0].circuit.literals, 5)
        with self.assertRaises(ValueError):
            model.add_circuit([])

    def test_multiple_circuit(self) -> None:
        model = cp_model.CpModel()
        x = [model.new_bool_var(f"x{i}") for i in range(5)]
        arcs: list[tuple[int, int, cp_model.LiteralT]] = [
            (i, i + 1, x[i]) for i in range(5)
        ]
        model.add_multiple_circuit(arcs)
        self.assertLen(model.proto.variables, 5)
        self.assertLen(model.proto.constraints, 1)
        self.assertLen(model.proto.constraints[0].routes.heads, 5)
        self.assertLen(model.proto.constraints[0].routes.tails, 5)
        self.assertLen(model.proto.constraints[0].routes.literals, 5)
        with self.assertRaises(ValueError):
            model.add_multiple_circuit([])

    def test_allowed_assignments(self) -> None:
        model = cp_model.CpModel()
        x = [model.new_int_var(0, 4, f"x{i}") for i in range(5)]
        model.add_allowed_assignments(
            x, [(0, 1, 2, 3, 4), (4, 3, 2, 1, 1), (0, 0, 0, 0, 0)]
        )
        self.assertLen(model.proto.variables, 5)
        self.assertLen(model.proto.constraints, 1)
        self.assertLen(model.proto.constraints[0].table.exprs, 5)
        self.assertLen(model.proto.constraints[0].table.values, 15)
        with self.assertRaises(TypeError):
            model.add_allowed_assignments(
                x,
                [(0, 1, 2, 3, 4), (4, 3, 2, 1, 1), (0, 0, 0, 0)],
            )
        with self.assertRaises(ValueError):
            model.add_allowed_assignments(
                [],
                [(0, 1, 2, 3, 4), (4, 3, 2, 1, 1), (0, 0, 0, 0)],
            )

    def test_forbidden_assignments(self) -> None:
        model = cp_model.CpModel()
        x = [model.new_int_var(0, 4, f"x{i}") for i in range(5)]
        model.add_forbidden_assignments(
            x, [(0, 1, 2, 3, 4), (4, 3, 2, 1, 1), (0, 0, 0, 0, 0)]
        )
        self.assertLen(model.proto.variables, 5)
        self.assertLen(model.proto.constraints, 1)
        self.assertLen(model.proto.constraints[0].table.exprs, 5)
        self.assertLen(model.proto.constraints[0].table.values, 15)
        self.assertTrue(model.proto.constraints[0].table.negated)
        self.assertRaises(
            TypeError,
            model.add_forbidden_assignments,
            x,
            [(0, 1, 2, 3, 4), (4, 3, 2, 1, 1), (0, 0, 0, 0)],
        )
        self.assertRaises(
            ValueError,
            model.add_forbidden_assignments,
            [],
            [(0, 1, 2, 3, 4), (4, 3, 2, 1, 1), (0, 0, 0, 0)],
        )

    def test_automaton(self) -> None:
        model = cp_model.CpModel()
        x = [model.new_int_var(0, 4, f"x{i}") for i in range(5)]
        model.add_automaton(x, 0, [2, 3], [(0, 0, 0), (0, 1, 1), (1, 2, 2), (2, 3, 3)])
        self.assertLen(model.proto.variables, 5)
        self.assertLen(model.proto.constraints, 1)
        self.assertLen(model.proto.constraints[0].automaton.exprs, 5)
        self.assertLen(model.proto.constraints[0].automaton.transition_tail, 4)
        self.assertLen(model.proto.constraints[0].automaton.transition_head, 4)
        self.assertLen(model.proto.constraints[0].automaton.transition_label, 4)
        self.assertLen(model.proto.constraints[0].automaton.final_states, 2)
        self.assertEqual(0, model.proto.constraints[0].automaton.starting_state)
        with self.assertRaises(TypeError):
            model.add_automaton(
                x,
                0,
                [2, 3],
                [(0, 0, 0), (0, 1, 1), (2, 2), (2, 3, 3)],
            )
        with self.assertRaises(ValueError):
            model.add_automaton(
                [],
                0,
                [2, 3],
                [(0, 0, 0), (0, 1, 1), (2, 3, 3)],
            )
        with self.assertRaises(ValueError):
            model.add_automaton(
                x,
                0,
                [],
                [(0, 0, 0), (0, 1, 1), (2, 3, 3)],
            )
        with self.assertRaises(ValueError):
            model.add_automaton(x, 0, [2, 3], [])

    def test_inverse(self) -> None:
        model = cp_model.CpModel()
        x = [model.new_int_var(0, 4, f"x{i}") for i in range(5)]
        y = [model.new_int_var(0, 4, f"y{i}") for i in range(5)]
        model.add_inverse(x, y)
        self.assertLen(model.proto.variables, 10)
        self.assertLen(model.proto.constraints, 1)
        self.assertLen(model.proto.constraints[0].inverse.f_direct, 5)
        self.assertLen(model.proto.constraints[0].inverse.f_inverse, 5)

    def test_max_equality(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(0, 4, "x")
        y = [model.new_int_var(0, 4, f"y{i}") for i in range(5)]
        model.add_max_equality(x, y)
        self.assertLen(model.proto.variables, 6)
        self.assertLen(model.proto.constraints, 1)
        self.assertLen(model.proto.constraints[0].lin_max.exprs, 5)
        self.assertEqual(0, model.proto.constraints[0].lin_max.target.vars[0])
        self.assertEqual(1, model.proto.constraints[0].lin_max.target.coeffs[0])

    def test_min_equality(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(0, 4, "x")
        y = [model.new_int_var(0, 4, f"y{i}") for i in range(5)]
        model.add_min_equality(x, y)
        self.assertLen(model.proto.variables, 6)
        self.assertLen(model.proto.constraints[0].lin_max.exprs, 5)
        self.assertEqual(0, model.proto.constraints[0].lin_max.target.vars[0])
        self.assertEqual(-1, model.proto.constraints[0].lin_max.target.coeffs[0])

    def test_min_equality_list(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(0, 4, "x")
        y = [model.new_int_var(0, 4, f"y{i}") for i in range(5)]
        model.add_min_equality(x, [y[0], y[2], y[1], y[3]])
        self.assertLen(model.proto.variables, 6)
        self.assertLen(model.proto.constraints[0].lin_max.exprs, 4)
        self.assertEqual(0, model.proto.constraints[0].lin_max.target.vars[0])
        self.assertEqual(-1, model.proto.constraints[0].lin_max.target.coeffs[0])

    def test_min_equality_tuple(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(0, 4, "x")
        y = [model.new_int_var(0, 4, f"y{i}") for i in range(5)]
        model.add_min_equality(x, (y[0], y[2], y[1], y[3]))
        self.assertLen(model.proto.variables, 6)
        self.assertLen(model.proto.constraints[0].lin_max.exprs, 4)
        self.assertEqual(0, model.proto.constraints[0].lin_max.target.vars[0])
        self.assertEqual(-1, model.proto.constraints[0].lin_max.target.coeffs[0])

    def test_min_equality_generator(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(0, 4, "x")
        y = [model.new_int_var(0, 4, f"y{i}") for i in range(5)]
        model.add_min_equality(x, (z for z in y))
        self.assertLen(model.proto.variables, 6)
        self.assertLen(model.proto.constraints[0].lin_max.exprs, 5)
        self.assertEqual(0, model.proto.constraints[0].lin_max.target.vars[0])
        self.assertEqual(-1, model.proto.constraints[0].lin_max.target.coeffs[0])

    def test_min_equality_with_constant(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(0, 4, "x")
        y = model.new_int_var(0, 4, "y")
        model.add_min_equality(x, [y, 3])
        self.assertLen(model.proto.variables, 2)
        self.assertLen(model.proto.constraints, 1)
        lin_max = model.proto.constraints[0].lin_max
        self.assertLen(lin_max.exprs, 2)
        self.assertLen(lin_max.exprs[0].vars, 1)
        self.assertEqual(1, lin_max.exprs[0].vars[0])
        self.assertEqual(-1, lin_max.exprs[0].coeffs[0])
        self.assertEqual(0, lin_max.exprs[0].offset)
        self.assertEmpty(lin_max.exprs[1].vars)
        self.assertEqual(-3, lin_max.exprs[1].offset)

    def test_abs(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(0, 4, "x")
        y = model.new_int_var(-5, 5, "y")
        model.add_abs_equality(x, y)
        self.assertLen(model.proto.variables, 2)
        self.assertLen(model.proto.constraints, 1)
        self.assertLen(model.proto.constraints[0].lin_max.exprs, 2)
        self.assertEqual(1, model.proto.constraints[0].lin_max.exprs[0].vars[0])
        self.assertEqual(1, model.proto.constraints[0].lin_max.exprs[0].coeffs[0])
        self.assertEqual(1, model.proto.constraints[0].lin_max.exprs[1].vars[0])
        self.assertEqual(-1, model.proto.constraints[0].lin_max.exprs[1].coeffs[0])
        passed = False
        error_msg = None
        try:
            abs(x)
        except NotImplementedError as e:
            error_msg = str(e)
            passed = True
        self.assertEqual(
            "calling abs() on a linear expression is not supported, "
            "please use CpModel.add_abs_equality",
            error_msg,
        )
        self.assertTrue(passed)

    def test_issue4568(self) -> None:
        model = cp_model.CpModel()
        target = 11
        value = model.new_int_var(0, 10, "")
        defect = model.new_int_var(0, cp_model.INT32_MAX, "")
        model.add_abs_equality(defect, value - target)
        model.minimize(defect)

        solver = cp_model.CpSolver()
        status = solver.Solve(model)
        self.assertEqual(status, cp_model.OPTIMAL)
        self.assertEqual(solver.objective_value, 1.0)

    def test_division(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(0, 10, "x")
        y = model.new_int_var(0, 50, "y")
        model.add_division_equality(x, y, 6)
        self.assertLen(model.proto.variables, 2)
        self.assertLen(model.proto.constraints, 1)
        self.assertLen(model.proto.constraints[0].int_div.exprs, 2)
        self.assertEqual(model.proto.constraints[0].int_div.exprs[0].vars[0], 1)
        self.assertEqual(model.proto.constraints[0].int_div.exprs[0].coeffs[0], 1)
        self.assertEmpty(model.proto.constraints[0].int_div.exprs[1].vars)
        self.assertEqual(model.proto.constraints[0].int_div.exprs[1].offset, 6)
        passed = False
        error_msg = None
        try:
            x / 3
        except NotImplementedError as e:
            error_msg = str(e)
            passed = True
        self.assertEqual(
            "calling // on a linear expression is not supported, "
            "please use CpModel.add_division_equality",
            error_msg,
        )
        self.assertTrue(passed)

    def testModulo(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(0, 10, "x")
        y = model.new_int_var(0, 50, "y")
        model.add_modulo_equality(x, y, 6)
        self.assertLen(model.proto.variables, 2)
        self.assertLen(model.proto.constraints, 1)
        self.assertLen(model.proto.constraints[0].int_mod.exprs, 2)
        self.assertEqual(model.proto.constraints[0].int_mod.exprs[0].vars[0], 1)
        self.assertEqual(model.proto.constraints[0].int_mod.exprs[0].coeffs[0], 1)
        self.assertEmpty(model.proto.constraints[0].int_mod.exprs[1].vars)
        self.assertEqual(model.proto.constraints[0].int_mod.exprs[1].offset, 6)
        passed = False
        error_msg = None
        try:
            x % 3
        except NotImplementedError as e:
            error_msg = str(e)
            passed = True
        self.assertEqual(
            "calling %% on a linear expression is not supported, "
            "please use CpModel.add_modulo_equality",
            error_msg,
        )
        self.assertTrue(passed)

    def test_multiplication_equality(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(0, 4, "x")
        y = [model.new_int_var(0, 4, f"y{i}") for i in range(5)]
        model.add_multiplication_equality(x, y)
        self.assertLen(model.proto.variables, 6)
        self.assertLen(model.proto.constraints, 1)
        self.assertLen(model.proto.constraints[0].int_prod.exprs, 5)
        self.assertEqual(0, model.proto.constraints[0].int_prod.target.vars[0])

    def test_implication(self) -> None:
        model = cp_model.CpModel()
        x = model.new_bool_var("x")
        y = model.new_bool_var("y")
        model.add_implication(x, y)
        self.assertLen(model.proto.variables, 2)
        self.assertLen(model.proto.constraints, 1)
        self.assertLen(model.proto.constraints[0].bool_or.literals, 1)
        self.assertLen(model.proto.constraints[0].enforcement_literal, 1)
        self.assertEqual(x.index, model.proto.constraints[0].enforcement_literal[0])
        self.assertEqual(y.index, model.proto.constraints[0].bool_or.literals[0])

    def test_bool_or(self) -> None:
        model = cp_model.CpModel()
        x = [model.new_bool_var(f"x{i}") for i in range(5)]
        model.add_bool_or(x)
        self.assertLen(model.proto.variables, 5)
        self.assertLen(model.proto.constraints, 1)
        self.assertLen(model.proto.constraints[0].bool_or.literals, 5)
        model.add_bool_or([x[0], x[1], False])
        self.assertLen(model.proto.variables, 6)
        with self.assertRaises(TypeError):
            model.add_bool_or([x[2], 2])
        y = model.new_int_var(0, 4, "y")
        with self.assertRaises(TypeError):
            model.add_bool_or([y, False])

    def test_bool_or_list_or_get(self) -> None:
        model = cp_model.CpModel()
        x = [model.new_bool_var(f"x{i}") for i in range(5)]
        model.add_bool_or(x)
        model.add_bool_or(True, x[0], x[2])
        model.add_bool_or(False, x[0])
        model.add_bool_or(x[i] for i in [0, 2, 3, 4])
        self.assertLen(model.proto.variables, 7)
        self.assertLen(model.proto.constraints, 4)
        self.assertLen(model.proto.constraints[0].bool_or.literals, 5)
        self.assertLen(model.proto.constraints[1].bool_or.literals, 3)
        self.assertLen(model.proto.constraints[2].bool_or.literals, 2)
        self.assertLen(model.proto.constraints[3].bool_or.literals, 4)

    def test_at_least_one(self) -> None:
        model = cp_model.CpModel()
        x = [model.new_bool_var(f"x{i}") for i in range(5)]
        model.add_at_least_one(x)
        self.assertLen(model.proto.variables, 5)
        self.assertLen(model.proto.constraints, 1)
        self.assertLen(model.proto.constraints[0].bool_or.literals, 5)
        model.add_at_least_one([x[0], x[1], False])
        self.assertLen(model.proto.variables, 6)
        self.assertRaises(TypeError, model.add_at_least_one, [x[2], 2])
        y = model.new_int_var(0, 4, "y")
        self.assertRaises(TypeError, model.add_at_least_one, [y, False])

    def test_at_most_one(self) -> None:
        model = cp_model.CpModel()
        x = [model.new_bool_var(f"x{i}") for i in range(5)]
        model.add_at_most_one(x)
        self.assertLen(model.proto.variables, 5)
        self.assertLen(model.proto.constraints, 1)
        self.assertLen(model.proto.constraints[0].at_most_one.literals, 5)
        model.add_at_most_one([x[0], x[1], False])
        self.assertLen(model.proto.variables, 6)
        self.assertRaises(TypeError, model.add_at_most_one, [x[2], 2])
        y = model.new_int_var(0, 4, "y")
        self.assertRaises(TypeError, model.add_at_most_one, [y, False])

    def test_exactly_one(self) -> None:
        model = cp_model.CpModel()
        x = [model.new_bool_var(f"x{i}") for i in range(5)]
        model.add_exactly_one(x)
        self.assertLen(model.proto.variables, 5)
        self.assertLen(model.proto.constraints, 1)
        self.assertLen(model.proto.constraints[0].exactly_one.literals, 5)
        model.add_exactly_one([x[0], x[1], False])
        self.assertLen(model.proto.variables, 6)
        self.assertRaises(TypeError, model.add_exactly_one, [x[2], 2])
        y = model.new_int_var(0, 4, "y")
        self.assertRaises(TypeError, model.add_exactly_one, [y, False])

    def test_bool_and(self) -> None:
        model = cp_model.CpModel()
        x = [model.new_bool_var(f"x{i}") for i in range(5)]
        model.add_bool_and(x)
        self.assertLen(model.proto.variables, 5)
        self.assertLen(model.proto.constraints, 1)
        self.assertLen(model.proto.constraints[0].bool_and.literals, 5)
        model.add_bool_and([x[1], x[2].negated(), True])
        self.assertEqual(1, model.proto.constraints[1].bool_and.literals[0])
        self.assertEqual(-3, model.proto.constraints[1].bool_and.literals[1])
        self.assertEqual(5, model.proto.constraints[1].bool_and.literals[2])

    def test_bool_x_or(self) -> None:
        model = cp_model.CpModel()
        x = [model.new_bool_var(f"x{i}") for i in range(5)]
        model.add_bool_xor(x)
        self.assertLen(model.proto.variables, 5)
        self.assertLen(model.proto.constraints, 1)
        self.assertLen(model.proto.constraints[0].bool_xor.literals, 5)

    def test_map_domain(self) -> None:
        model = cp_model.CpModel()
        x = [model.new_bool_var(f"x{i}") for i in range(5)]
        y = model.new_int_var(0, 10, "y")
        model.add_map_domain(y, x, 2)
        self.assertLen(model.proto.variables, 6)
        self.assertLen(model.proto.constraints, 10)

    def test_interval(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(0, 4, "x")
        y = model.new_int_var(0, 3, "y")
        i = model.new_interval_var(x, 3, y, "i")
        self.assertEqual(0, i.index)

        j = model.new_fixed_size_interval_var(x, 2, "j")
        self.assertEqual(1, j.index)
        start_expr = j.start_expr()
        size_expr = j.size_expr()
        end_expr = j.end_expr()
        self.assertEqual(x.index, start_expr.index)
        self.assertEqual(size_expr, 2)
        self.assertEqual(str(end_expr), "(x + 2)")

    def test_rebuild_from_linear_expression_proto(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(0, 4, "x")
        y = model.new_int_var(0, 1, "y")
        z = model.new_int_var(0, 5, "z")
        i = model.new_interval_var(x, y, z, "i")
        self.assertEqual(i.start_expr(), x)
        self.assertEqual(i.size_expr(), y)
        self.assertEqual(i.end_expr(), z)
        self.assertEqual(~i.size_expr(), ~y)
        self.assertRaises(TypeError, i.start_expr().negated)

        proto = cp_model_pb2.LinearExpressionProto()
        proto.vars.append(x.index)
        proto.coeffs.append(1)
        proto.vars.append(y.index)
        proto.coeffs.append(2)
        expr1 = model.rebuild_from_linear_expression_proto(proto)
        canonical_expr1 = cmh.FlatIntExpr(expr1)
        self.assertEqual(canonical_expr1.vars[0], x)
        self.assertEqual(canonical_expr1.vars[1], y)
        self.assertEqual(canonical_expr1.coeffs[0], 1)
        self.assertEqual(canonical_expr1.coeffs[1], 2)
        self.assertEqual(canonical_expr1.offset, 0)
        self.assertEqual(~canonical_expr1.vars[1], ~y)
        self.assertRaises(TypeError, canonical_expr1.vars[0].negated)

        proto.offset = 2
        expr2 = model.rebuild_from_linear_expression_proto(proto)
        canonical_expr2 = cmh.FlatIntExpr(expr2)
        self.assertEqual(canonical_expr2.vars[0], x)
        self.assertEqual(canonical_expr2.vars[1], y)
        self.assertEqual(canonical_expr2.coeffs[0], 1)
        self.assertEqual(canonical_expr2.coeffs[1], 2)
        self.assertEqual(canonical_expr2.offset, 2)

    def test_absent_interval(self) -> None:
        model = cp_model.CpModel()
        i = model.new_optional_interval_var(1, 0, 1, False, "")
        self.assertEqual(0, i.index)

    def test_optional_interval(self) -> None:
        model = cp_model.CpModel()
        b = model.new_bool_var("b")
        x = model.new_int_var(0, 4, "x")
        y = model.new_int_var(0, 3, "y")
        i = model.new_optional_interval_var(x, 3, y, b, "i")
        j = model.new_optional_interval_var(x, y, 10, b, "j")
        k = model.new_optional_interval_var(x, -y, 10, b, "k")
        l = model.new_optional_interval_var(x, 10, -y, b, "l")
        self.assertEqual(0, i.index)
        self.assertEqual(1, j.index)
        self.assertEqual(2, k.index)
        self.assertEqual(3, l.index)
        with self.assertRaises(TypeError):
            model.new_optional_interval_var(1, 2, 3, x, "x")
        with self.assertRaises(TypeError):
            model.new_optional_interval_var(b + x, 2, 3, b, "x")
        with self.assertRaises(TypeError):
            model.new_optional_interval_var(1, 2, 3, b + 1, "x")

    def test_no_overlap(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(0, 4, "x")
        y = model.new_int_var(0, 3, "y")
        z = model.new_int_var(0, 3, "y")
        i = model.new_interval_var(x, 3, y, "i")
        j = model.new_interval_var(x, 5, z, "j")
        ct = model.add_no_overlap([i, j])
        self.assertEqual(2, ct.index)
        self.assertLen(ct.proto.no_overlap.intervals, 2)
        self.assertEqual(0, ct.proto.no_overlap.intervals[0])
        self.assertEqual(1, ct.proto.no_overlap.intervals[1])

    def test_no_overlap2d(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(0, 4, "x")
        y = model.new_int_var(0, 3, "y")
        z = model.new_int_var(0, 3, "y")
        i = model.new_interval_var(x, 3, y, "i")
        j = model.new_interval_var(x, 5, z, "j")
        ct = model.add_no_overlap_2d([i, j], [j, i])
        self.assertEqual(2, ct.index)
        self.assertLen(ct.proto.no_overlap_2d.x_intervals, 2)
        self.assertEqual(0, ct.proto.no_overlap_2d.x_intervals[0])
        self.assertEqual(1, ct.proto.no_overlap_2d.x_intervals[1])
        self.assertLen(ct.proto.no_overlap_2d.y_intervals, 2)
        self.assertEqual(1, ct.proto.no_overlap_2d.y_intervals[0])
        self.assertEqual(0, ct.proto.no_overlap_2d.y_intervals[1])

    def test_cumulative(self) -> None:
        model = cp_model.CpModel()
        intervals = [
            model.new_interval_var(
                model.new_int_var(0, 10, f"s_{i}"),
                5,
                model.new_int_var(5, 15, f"e_{i}"),
                f"interval[{i}]",
            )
            for i in range(10)
        ]
        demands = [1, 3, 5, 2, 4, 5, 3, 4, 2, 3]
        capacity = 4
        ct = model.add_cumulative(intervals, demands, capacity)
        self.assertEqual(10, ct.index)
        self.assertLen(ct.proto.cumulative.intervals, 10)
        with self.assertRaises(TypeError):
            model.add_cumulative([intervals[0], 3], [2, 3], 3)

    def test_get_or_make_index_from_constant(self) -> None:
        model = cp_model.CpModel()
        self.assertEqual(0, model.get_or_make_index_from_constant(3))
        self.assertEqual(0, model.get_or_make_index_from_constant(3))
        self.assertEqual(1, model.get_or_make_index_from_constant(5))
        model_var = model.proto.variables[0]
        self.assertLen(model_var.domain, 2)
        self.assertEqual(3, model_var.domain[0])
        self.assertEqual(3, model_var.domain[1])

    def test_str(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(0, 4, "x")
        self.assertEqual(str(x == 2), "x == 2")
        self.assertEqual(str(x >= 2), "x >= 2")
        self.assertEqual(str(x <= 2), "x <= 2")
        self.assertEqual(str(x > 2), "x >= 3")
        self.assertEqual(str(x < 2), "x <= 1")
        self.assertEqual(str(x != 2), "x != 2")
        self.assertEqual(str(x * 3), "(3 * x)")
        self.assertEqual(str(-x), "(-x)")
        self.assertEqual(str(x + 3), "(x + 3)")
        self.assertEqual(str(x <= cp_model.INT_MAX), "True (unbounded expr x)")
        self.assertEqual(str(x != 9223372036854775807), "x <= 9223372036854775806")
        self.assertEqual(str(x != -9223372036854775808), "x >= -9223372036854775807")
        y = model.new_int_var(0, 4, "y")
        self.assertEqual(
            str(cp_model.LinearExpr.weighted_sum([x, y + 1, 2], [1, -2, 3])),
            "(x - 2 * (y + 1) + 6)",
        )
        self.assertEqual(str(cp_model.LinearExpr.term(x, 3)), "(3 * x)")
        self.assertEqual(str(x != y), "(x - y) != 0")
        self.assertEqual(
            "0 <= x <= 10",
            str(cp_model.BoundedLinearExpression(x, cp_model.Domain(0, 10))),
        )
        e1 = 2 * cp_model.LinearExpr.sum([x, y])
        flat_e1 = cmh.FlatIntExpr(e1)
        self.assertEqual(str(e1), "(2 * (x + y))")
        self.assertEqual(flat_e1.vars, [x, y])
        self.assertEqual(flat_e1.coeffs, [2, 2])
        self.assertEqual(flat_e1.offset, 0)
        repeat_flat_e1 = cmh.FlatIntExpr(flat_e1 + 3)
        self.assertEqual(repeat_flat_e1.vars, [x, y])
        self.assertEqual(repeat_flat_e1.coeffs, [2, 2])
        self.assertEqual(repeat_flat_e1.offset, 3)
        float_flat_e1 = cmh.FlatFloatExpr(flat_e1)
        self.assertEqual(float_flat_e1.vars, [x, y])
        self.assertEqual(float_flat_e1.coeffs, [2.0, 2.0])
        self.assertEqual(float_flat_e1.offset, 0.0)
        repeat_float_flat_e1 = cmh.FlatFloatExpr(float_flat_e1 - 2.5)
        self.assertEqual(repeat_float_flat_e1.vars, [x, y])
        self.assertEqual(repeat_float_flat_e1.coeffs, [2.0, 2.0])
        self.assertEqual(repeat_float_flat_e1.offset, -2.5)

        b = model.new_bool_var("b")
        self.assertEqual(str(cp_model.LinearExpr.term(b.negated(), 3)), "(3 * not(b))")

        i = model.new_interval_var(x, 2, y, "i")
        self.assertEqual(str(i), "i")

    def test_repr(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(0, 4, "x")
        y = model.new_int_var(0, 3, "y")
        z = model.new_int_var(0, 3, "z")
        self.assertEqual(repr(x), "x(0..4)")
        self.assertEqual(repr(x + 0), "x(0..4)")
        self.assertEqual(repr(x + 0.0), "x(0..4)")
        self.assertEqual(repr(x - 0), "x(0..4)")
        self.assertEqual(repr(x - 0.0), "x(0..4)")
        self.assertEqual(repr(x * 1), "x(0..4)")
        self.assertEqual(repr(x * 1.0), "x(0..4)")
        self.assertEqual(repr(x * 0), "IntConstant(0)")
        self.assertEqual(repr(x * 0.0), "IntConstant(0)")
        self.assertEqual(repr(x * 2), "IntAffine(expr=x(0..4), coeff=2, offset=0)")
        self.assertEqual(
            repr(x + 1.5), "FloatAffine(expr=x(0..4), coeff=1, offset=1.5)"
        )
        self.assertEqual(repr(x + y), "SumArray(x(0..4), y(0..3))")
        self.assertEqual(
            repr(cp_model.LinearExpr.sum([x, y, z])),
            "SumArray(x(0..4), y(0..3), z(0..3))",
        )
        self.assertEqual(
            repr(cp_model.LinearExpr.weighted_sum([x, y, 2], [1, 2, 3])),
            "IntWeightedSum([x(0..4), y(0..3)], [1, 2], 6)",
        )
        i = model.new_interval_var(x, 2, y, "i")
        self.assertEqual(repr(i), "i(start = x, size = 2, end = y)")
        b = model.new_bool_var("b")
        self.assertEqual(repr(b), "b(0..1)")
        self.assertEqual(repr(~b), "NotBooleanVariable(index=3)")
        x1 = model.new_int_var(0, 4, "x1")
        y1 = model.new_int_var(0, 3, "y1")
        j = model.new_optional_interval_var(x1, 2, y1, b, "j")
        self.assertEqual(repr(j), "j(start = x1, size = 2, end = y1, is_present = b)")
        x2 = model.new_int_var(0, 4, "x2")
        y2 = model.new_int_var(0, 3, "y2")
        k = model.new_optional_interval_var(x2, 2, y2, b.negated(), "k")
        self.assertEqual(
            repr(k), "k(start = x2, size = 2, end = y2, is_present = not(b))"
        )

    def testDisplayBounds(self) -> None:
        self.assertEqual("10..20", cp_model.display_bounds([10, 20]))
        self.assertEqual("10", cp_model.display_bounds([10, 10]))
        self.assertEqual("10..15, 20..30", cp_model.display_bounds([10, 15, 20, 30]))

    def test_short_name(self) -> None:
        model = cp_model.CpModel()
        model.proto.variables.add(domain=[5, 10])
        self.assertEqual("[5..10]", cp_model.short_name(model.proto, 0))

    def test_integer_expression_errors(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(0, 1, "x")
        y = model.new_int_var(0, 3, "y")
        self.assertRaises(TypeError, x.__mul__, y)
        self.assertRaises(NotImplementedError, x.__div__, y)
        self.assertRaises(NotImplementedError, x.__truediv__, y)
        self.assertRaises(NotImplementedError, x.__mod__, y)
        self.assertRaises(NotImplementedError, x.__pow__, y)
        self.assertRaises(NotImplementedError, x.__lshift__, y)
        self.assertRaises(NotImplementedError, x.__rshift__, y)
        self.assertRaises(NotImplementedError, x.__and__, y)
        self.assertRaises(NotImplementedError, x.__or__, y)
        self.assertRaises(NotImplementedError, x.__xor__, y)
        self.assertRaises(ArithmeticError, x.__lt__, cp_model.INT_MIN)
        self.assertRaises(ArithmeticError, x.__gt__, cp_model.INT_MAX)
        self.assertRaises(TypeError, x.__add__, "dummy")
        self.assertRaises(TypeError, x.__mul__, "dummy")

    def test_model_errors(self) -> None:
        model = cp_model.CpModel()
        self.assertRaises(TypeError, model.add, "dummy")
        self.assertRaises(TypeError, model.get_or_make_index, "dummy")
        self.assertRaises(TypeError, model.minimize, "dummy")

    def test_solver_errors(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(0, 1, "x")
        y = model.new_int_var(-10, 10, "y")
        model.add_linear_constraint(x + 2 * y, 0, 10)
        model.minimize(y)
        solver = cp_model.CpSolver()
        self.assertRaises(RuntimeError, solver.value, x)
        solver.solve(model)
        self.assertRaises(TypeError, solver.value, "not_a_variable")
        self.assertRaises(TypeError, model.add_bool_or, [x, y])

    def test_has_objective_minimize(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(0, 1, "x")
        y = model.new_int_var(-10, 10, "y")
        model.add_linear_constraint(x + 2 * y, 0, 10)
        self.assertFalse(model.has_objective())
        model.minimize(y)
        self.assertTrue(model.has_objective())

    def test_has_objective_maximize(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(0, 1, "x")
        y = model.new_int_var(-10, 10, "y")
        model.add_linear_constraint(x + 2 * y, 0, 10)
        self.assertFalse(model.has_objective())
        model.maximize(y)
        self.assertTrue(model.has_objective())

    def test_search_for_all_solutions(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(0, 5, "x")
        y = model.new_int_var(0, 5, "y")
        model.add_linear_constraint(x + y, 6, 6)

        solver = cp_model.CpSolver()
        solver.parameters.enumerate_all_solutions = True
        solution_counter = SolutionCounter()
        status = solver.solve(model, solution_counter)
        self.assertEqual(cp_model.OPTIMAL, status)
        self.assertEqual(5, solution_counter.solution_count)

    def test_solve_with_solution_callback(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(0, 5, "x")
        y = model.new_int_var(0, 5, "y")
        model.add_linear_constraint(x + y, 6, 6)

        solver = cp_model.CpSolver()
        solution_sum = SolutionSum([x, y])
        self.assertRaises(RuntimeError, solution_sum.value, x)
        status = solver.solve(model, solution_sum)
        self.assertEqual(cp_model.OPTIMAL, status)
        self.assertEqual(6, solution_sum.sum)

    def test_solve_with_float_value_in_callback(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(0, 5, "x")
        y = model.new_int_var(0, 5, "y")
        model.add_linear_constraint(x + y, 6, 6)

        solver = cp_model.CpSolver()
        solution_float_value = SolutionFloatValue((x + y) * 0.5)
        status = solver.solve(model, solution_float_value)
        self.assertEqual(cp_model.OPTIMAL, status)
        self.assertEqual(3.0, solution_float_value.value)

    def test_best_bound_callback(self) -> None:
        model = cp_model.CpModel()
        x0 = model.new_bool_var("x0")
        x1 = model.new_bool_var("x1")
        x2 = model.new_bool_var("x2")
        x3 = model.new_bool_var("x3")
        model.add_bool_or(x0, x1, x2, x3)
        model.minimize(3 * x0 + 2 * x1 + 4 * x2 + 5 * x3 + 0.6)

        solver = cp_model.CpSolver()
        best_bound_callback = BestBoundCallback()
        solver.best_bound_callback = best_bound_callback.new_best_bound
        solver.parameters.num_workers = 1
        solver.parameters.linearization_level = 2
        status = solver.solve(model)
        self.assertEqual(cp_model.OPTIMAL, status)
        self.assertEqual(2.6, best_bound_callback.best_bound)

    def test_value(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(0, 10, "x")
        y = model.new_int_var(0, 10, "y")
        model.add(x + 2 * y == 29)
        solver = cp_model.CpSolver()
        status = solver.solve(model)
        self.assertEqual(cp_model.OPTIMAL, status)
        self.assertEqual(solver.value(x), 9)
        self.assertEqual(solver.value(y), 10)
        self.assertEqual(solver.value(2), 2)

    def test_float_value(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(0, 10, "x")
        y = model.new_int_var(0, 10, "y")
        model.add(x + 2 * y == 29)
        solver = cp_model.CpSolver()
        status = solver.solve(model)
        self.assertEqual(cp_model.OPTIMAL, status)
        self.assertEqual(solver.float_value(x * 1.5 + 0.25), 13.75)
        self.assertEqual(solver.float_value(2.25), 2.25)

    def test_boolean_value(self) -> None:
        model = cp_model.CpModel()
        x = model.new_bool_var("x")
        y = model.new_bool_var("y")
        z = model.new_bool_var("z")
        model.add_bool_or([x, z.negated()])
        model.add_bool_or([x, z])
        model.add_bool_or([x.negated(), y.negated()])
        solver = cp_model.CpSolver()
        status = solver.solve(model)
        self.assertEqual(cp_model.OPTIMAL, status)
        self.assertEqual(solver.boolean_value(x), True)
        self.assertEqual(solver.value(x), 1 - solver.value(x.negated()))
        self.assertEqual(solver.value(y), 1 - solver.value(y.negated()))
        self.assertEqual(solver.value(z), 1 - solver.value(z.negated()))
        self.assertEqual(solver.boolean_value(y), False)
        self.assertEqual(solver.boolean_value(True), True)
        self.assertEqual(solver.boolean_value(False), False)
        self.assertEqual(solver.boolean_value(2), True)
        self.assertEqual(solver.boolean_value(0), False)

    def test_unsupported_operators(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(0, 10, "x")
        y = model.new_int_var(0, 10, "y")
        z = model.new_int_var(0, 10, "z")

        with self.assertRaises(NotImplementedError):
            model.add(x == min(y, z))
        with self.assertRaises(NotImplementedError):
            if x > y:
                print("passed1")
        with self.assertRaises(NotImplementedError):
            if x == 2:
                print("passed2")

    def test_is_literal_true_false(self) -> None:
        model = cp_model.CpModel()
        x = model.new_constant(0)
        self.assertFalse(cp_model.object_is_a_true_literal(x))
        self.assertTrue(cp_model.object_is_a_false_literal(x))
        self.assertTrue(cp_model.object_is_a_true_literal(x.negated()))
        self.assertFalse(cp_model.object_is_a_false_literal(x.negated()))
        self.assertTrue(cp_model.object_is_a_true_literal(True))
        self.assertTrue(cp_model.object_is_a_false_literal(False))
        self.assertFalse(cp_model.object_is_a_true_literal(False))
        self.assertFalse(cp_model.object_is_a_false_literal(True))

    def test_solve_minimize_with_solution_callback(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(0, 5, "x")
        y = model.new_int_var(0, 5, "y")
        model.add_linear_constraint(x + y, 6, 6)
        model.maximize(x + 2 * y)

        solver = cp_model.CpSolver()
        solution_obj = SolutionObjective()
        status = solver.solve(model, solution_obj)
        self.assertEqual(cp_model.OPTIMAL, status)
        self.assertEqual(11, solution_obj.obj)

    def test_solution_value(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(0, 5, "x")
        b = model.new_bool_var("b")
        model.add_decision_strategy(
            [x], cp_model.CHOOSE_MIN_DOMAIN_SIZE, cp_model.SELECT_MAX_VALUE
        )
        model.add_decision_strategy(
            [b], cp_model.CHOOSE_MIN_DOMAIN_SIZE, cp_model.SELECT_MIN_VALUE
        )
        solver = cp_model.CpSolver()
        solver.parameters.keep_all_feasible_solutions_in_presolve = True
        solver.parameters.num_workers = 1
        solution_recorder = RecordSolution([3, x, 1 - x], [1, False, ~b])
        status = solver.solve(model, solution_recorder)
        self.assertEqual(cp_model.OPTIMAL, status)
        self.assertEqual([3, 5, -4], solution_recorder.int_var_values)
        self.assertEqual([True, False, True], solution_recorder.bool_var_values)

    def test_solution_hinting(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(0, 5, "x")
        y = model.new_int_var(0, 5, "y")
        model.add_linear_constraint(x + y, 6, 6)
        model.add_hint(x, 2)
        model.add_hint(y, 4)
        solver = cp_model.CpSolver()
        solver.parameters.cp_model_presolve = False
        status = solver.solve(model)
        self.assertEqual(cp_model.OPTIMAL, status)
        self.assertEqual(2, solver.value(x))
        self.assertEqual(4, solver.value(y))

    def test_solution_hinting_with_booleans(self) -> None:
        model = cp_model.CpModel()
        x = model.new_bool_var("x")
        y = model.new_bool_var("y")
        model.add_linear_constraint(x + y, 1, 1)
        model.add_hint(x, True)
        model.add_hint(~y, True)
        solver = cp_model.CpSolver()
        solver.parameters.cp_model_presolve = False
        status = solver.solve(model)
        self.assertEqual(cp_model.OPTIMAL, status)
        self.assertTrue(solver.boolean_value(x))
        self.assertFalse(solver.boolean_value(y))

    def test_stats(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(0, 5, "x")
        y = model.new_int_var(0, 5, "y")
        model.add_linear_constraint(x + y, 4, 6)
        model.add_linear_constraint(2 * x + y, 0, 10)
        model.maximize(x + 2 * y)

        solver = cp_model.CpSolver()
        status = solver.solve(model)
        self.assertEqual(cp_model.OPTIMAL, status)
        self.assertEqual(solver.num_booleans, 0)
        self.assertEqual(solver.num_conflicts, 0)
        self.assertEqual(solver.num_branches, 0)
        self.assertGreater(solver.wall_time, 0.0)

    def test_search_strategy(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(0, 5, "x")
        y = model.new_int_var(0, 5, "y")
        z = model.new_bool_var("z")
        model.add_decision_strategy(
            [y, x, z.negated()],
            cp_model.CHOOSE_MIN_DOMAIN_SIZE,
            cp_model.SELECT_MAX_VALUE,
        )
        self.assertLen(model.proto.search_strategy, 1)
        strategy = model.proto.search_strategy[0]
        self.assertLen(strategy.exprs, 3)
        self.assertEqual(y.index, strategy.exprs[0].vars[0])
        self.assertEqual(1, strategy.exprs[0].coeffs[0])
        self.assertEqual(x.index, strategy.exprs[1].vars[0])
        self.assertEqual(1, strategy.exprs[1].coeffs[0])
        self.assertEqual(z.index, strategy.exprs[2].vars[0])
        self.assertEqual(-1, strategy.exprs[2].coeffs[0])
        self.assertEqual(1, strategy.exprs[2].offset)
        self.assertEqual(
            cp_model.CHOOSE_MIN_DOMAIN_SIZE, strategy.variable_selection_strategy
        )
        self.assertEqual(cp_model.SELECT_MAX_VALUE, strategy.domain_reduction_strategy)

    def test_model_and_response_stats(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(0, 5, "x")
        y = model.new_int_var(0, 5, "y")
        model.add_linear_constraint(x + y, 6, 6)
        model.maximize(x + 2 * y)
        self.assertTrue(model.model_stats())

        solver = cp_model.CpSolver()
        solver.solve(model)
        self.assertTrue(solver.response_stats())

    def test_validate_model(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(0, 5, "x")
        y = model.new_int_var(0, 5, "y")
        model.add_linear_constraint(x + y, 6, 6)
        model.maximize(x + 2 * y)
        self.assertFalse(model.validate())

    def test_validate_model_with_overflow(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(0, cp_model.INT_MAX, "x")
        y = model.new_int_var(0, 10, "y")
        model.add_linear_constraint(x + y, 6, cp_model.INT_MAX)
        model.maximize(x + 2 * y)
        self.assertTrue(model.validate())

    def test_copy_model(self) -> None:
        model = cp_model.CpModel()
        b = model.new_bool_var("b")
        x = model.new_int_var(0, 4, "x")
        y = model.new_int_var(0, 3, "y")
        i = model.new_optional_interval_var(x, 12, y, b, "i")
        lin = model.add(x + y <= 10)

        new_model = model.clone()
        clone_b = new_model.get_bool_var_from_proto_index(b.index)
        clone_x = new_model.get_int_var_from_proto_index(x.index)
        clone_y = new_model.get_int_var_from_proto_index(y.index)
        clone_i = new_model.get_interval_var_from_proto_index(i.index)

        self.assertEqual(b.index, clone_b.index)
        self.assertEqual(x.index, clone_x.index)
        self.assertEqual(y.index, clone_y.index)
        self.assertEqual(i.index, clone_i.index)

        solo_copy_b = copy.copy(b)
        self.assertEqual(b.index, solo_copy_b.index)
        self.assertEqual(b.is_boolean, solo_copy_b.is_boolean)
        self.assertIs(solo_copy_b.model_proto, b.model_proto)
        solo_copy_x = copy.copy(x)
        self.assertEqual(x.index, solo_copy_x.index)
        self.assertEqual(x.is_boolean, solo_copy_x.is_boolean)
        self.assertIs(solo_copy_x.model_proto, x.model_proto)
        solo_copy_i = copy.copy(i)
        self.assertEqual(i.index, solo_copy_i.index)
        self.assertIs(solo_copy_i.model_proto, i.model_proto)

        model_copy = copy.copy(model)
        copy_b = model_copy.get_bool_var_from_proto_index(b.index)
        copy_x = model_copy.get_int_var_from_proto_index(x.index)
        copy_y = model_copy.get_int_var_from_proto_index(y.index)
        copy_i = model_copy.get_interval_var_from_proto_index(i.index)

        self.assertEqual(b.index, copy_b.index)
        self.assertEqual(x.index, copy_x.index)
        self.assertEqual(y.index, copy_y.index)
        self.assertEqual(i.index, copy_i.index)
        self.assertEqual(b.is_boolean, copy_b.is_boolean)
        self.assertEqual(x.is_boolean, copy_x.is_boolean)
        self.assertEqual(y.is_boolean, copy_y.is_boolean)
        self.assertIs(copy_b.model_proto, b.model_proto)
        self.assertIs(copy_x.model_proto, x.model_proto)
        self.assertIs(copy_i.model_proto, i.model_proto)

        model_deepcopy = copy.deepcopy(model)
        deepcopy_b = model_deepcopy.get_bool_var_from_proto_index(b.index)
        deepcopy_x = model_deepcopy.get_int_var_from_proto_index(x.index)
        deepcopy_y = model_deepcopy.get_int_var_from_proto_index(y.index)
        deepcopy_i = model_deepcopy.get_interval_var_from_proto_index(i.index)

        self.assertEqual(b.index, deepcopy_b.index)
        self.assertEqual(x.index, deepcopy_x.index)
        self.assertEqual(y.index, deepcopy_y.index)
        self.assertEqual(i.index, deepcopy_i.index)
        self.assertEqual(b.is_boolean, deepcopy_b.is_boolean)
        self.assertEqual(x.is_boolean, deepcopy_x.is_boolean)
        self.assertEqual(y.is_boolean, deepcopy_y.is_boolean)
        self.assertIsNot(deepcopy_b.model_proto, b.model_proto)
        self.assertIsNot(deepcopy_x.model_proto, x.model_proto)
        self.assertIsNot(deepcopy_y.model_proto, y.model_proto)
        self.assertIsNot(deepcopy_i.model_proto, i.model_proto)
        self.assertIs(deepcopy_b.model_proto, deepcopy_x.model_proto)
        self.assertIs(deepcopy_b.model_proto, deepcopy_y.model_proto)
        self.assertIs(deepcopy_b.model_proto, deepcopy_i.model_proto)

        with self.assertRaises(ValueError):
            new_model.get_bool_var_from_proto_index(-1)

        with self.assertRaises(ValueError):
            new_model.get_int_var_from_proto_index(-1)

        with self.assertRaises(ValueError):
            new_model.get_interval_var_from_proto_index(-1)

        with self.assertRaises(ValueError):
            new_model.get_bool_var_from_proto_index(x.index)

        with self.assertRaises(ValueError):
            new_model.get_interval_var_from_proto_index(lin.index)

        interval_ct = new_model.proto.constraints[copy_i.index].interval
        self.assertEqual(12, interval_ct.size.offset)

        class Composite:

            def __init__(self, model: cp_model.CpModel, var: cp_model.IntVar):
                self.model = model
                self.var = var

        c = Composite(model, x)
        copy_c = copy.copy(c)
        self.assertIs(copy_c.model, c.model)
        self.assertIs(copy_c.var, c.var)

        deepcopy_c = copy.deepcopy(c)
        self.assertIsNot(deepcopy_c.model, c.model)
        self.assertIsNot(deepcopy_c.var, c.var)
        self.assertIs(deepcopy_c.model.proto, deepcopy_c.var.model_proto)
        self.assertIs(
            deepcopy_c.var,
            deepcopy_c.model.get_int_var_from_proto_index(x.index),
        )

    def test_custom_log(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        y = model.new_int_var(-10, 10, "y")
        model.add_linear_constraint(x + 2 * y, 0, 10)
        model.minimize(y)
        solver = cp_model.CpSolver()
        solver.parameters.log_search_progress = True
        solver.parameters.log_to_stdout = False
        log_callback = LogToString()
        solver.log_callback = log_callback.new_message

        self.assertEqual(cp_model.OPTIMAL, solver.solve(model))
        self.assertEqual(10, solver.value(x))
        self.assertEqual(-5, solver.value(y))

        self.assertRegex(log_callback.log, ".*log_to_stdout.*")

    def test_issue2762(self) -> None:
        model = cp_model.CpModel()

        x = [model.new_bool_var("a"), model.new_bool_var("b")]
        with self.assertRaises(NotImplementedError):
            model.add((x[0] != 0) or (x[1] != 0))

    def test_model_error(self) -> None:
        model = cp_model.CpModel()
        x = [model.new_int_var(0, -2, f"x{i}") for i in range(100)]
        model.add(sum(x) <= 1)
        solver = cp_model.CpSolver()
        solver.parameters.log_search_progress = True
        self.assertEqual(cp_model.MODEL_INVALID, solver.solve(model))
        self.assertEqual(solver.solution_info(), 'var #0 has no domain(): name: "x0"')

    def test_int_var_series(self) -> None:
        df = pd.DataFrame([1, -1, 1], columns=["coeffs"])
        model = cp_model.CpModel()
        x = model.new_int_var_series(
            name="x", index=df.index, lower_bounds=0, upper_bounds=5
        )
        model.minimize(df.coeffs.dot(x))
        solver = cp_model.CpSolver()
        self.assertEqual(cp_model.OPTIMAL, solver.solve(model))
        solution = solver.values(x)
        self.assertTrue((solution.values == [0, 5, 0]).all())
        self.assertRaises(TypeError, x.apply, lambda x: ~x)
        y = model.new_int_var_series(
            name="y", index=df.index, lower_bounds=-1, upper_bounds=1
        )
        self.assertRaises(TypeError, y.apply, lambda x: ~x)
        z = model.new_int_var_series(
            name="y", index=df.index, lower_bounds=0, upper_bounds=1
        )
        _ = z.apply(lambda x: ~x)

    def test_bool_var_series(self) -> None:
        df = pd.DataFrame([1, -1, 1], columns=["coeffs"])
        model = cp_model.CpModel()
        x = model.new_bool_var_series(name="x", index=df.index)
        _ = x.apply(lambda x: ~x)
        y = model.new_int_var_series(
            name="y", index=df.index, lower_bounds=0, upper_bounds=1
        )
        _ = y.apply(lambda x: ~x)
        model.minimize(df.coeffs.dot(x))
        solver = cp_model.CpSolver()
        self.assertEqual(cp_model.OPTIMAL, solver.solve(model))
        solution = solver.boolean_values(x)
        self.assertTrue((solution.values == [False, True, False]).all())

    def test_fixed_size_interval_var_series(self) -> None:
        df = pd.DataFrame([2, 4, 6], columns=["size"])
        model = cp_model.CpModel()
        starts = model.new_int_var_series(
            name="starts", index=df.index, lower_bounds=0, upper_bounds=5
        )
        presences = model.new_bool_var_series(name="rresences", index=df.index)
        fixed_size_intervals = model.new_fixed_size_interval_var_series(
            name="fixed_size_intervals",
            index=df.index,
            starts=starts,
            sizes=df.size,
        )
        opt_fixed_size_intervals = model.new_optional_fixed_size_interval_var_series(
            name="fixed_size_intervals",
            index=df.index,
            starts=starts,
            sizes=df.size,
            are_present=presences,
        )
        model.add_no_overlap(
            fixed_size_intervals.to_list() + opt_fixed_size_intervals.to_list()
        )
        self.assertLen(model.proto.constraints, 7)

    def test_interval_var_series(self) -> None:
        df = pd.DataFrame([2, 4, 6], columns=["size"])
        model = cp_model.CpModel()
        starts = model.new_int_var_series(
            name="starts", index=df.index, lower_bounds=0, upper_bounds=5
        )
        sizes = model.new_int_var_series(
            name="sizes", index=df.index, lower_bounds=2, upper_bounds=4
        )
        ends = model.new_int_var_series(
            name="ends", index=df.index, lower_bounds=0, upper_bounds=10
        )
        presences = model.new_bool_var_series(name="presences", index=df.index)
        intervals = model.new_interval_var_series(
            name="fixed_size_intervals",
            index=df.index,
            starts=starts,
            sizes=sizes,
            ends=ends,
        )
        fixed_intervals = model.new_fixed_size_interval_var_series(
            name="fixed_size_intervals",
            index=df.index,
            starts=starts,
            sizes=3,
        )
        opt_intervals = model.new_optional_interval_var_series(
            name="fixed_size_intervals",
            index=df.index,
            starts=starts,
            sizes=sizes,
            ends=ends,
            are_present=presences,
        )
        absent_fixed_intervals = model.new_optional_fixed_size_interval_var_series(
            name="fixed_size_intervals",
            index=df.index,
            starts=starts,
            sizes=3,
            are_present=False,
        )
        model.add_no_overlap(
            intervals.to_list()
            + opt_intervals.to_list()
            + fixed_intervals.to_list()
            + absent_fixed_intervals.to_list()
        )
        self.assertLen(model.proto.constraints, 13)

    def test_compare_with_none(self) -> None:
        model = cp_model.CpModel()
        x = model.new_int_var(0, 10, "x")
        self.assertRaises(TypeError, x.__eq__, None)
        self.assertRaises(TypeError, x.__ne__, None)
        self.assertRaises(TypeError, x.__lt__, None)
        self.assertRaises(TypeError, x.__le__, None)
        self.assertRaises(TypeError, x.__gt__, None)
        self.assertRaises(TypeError, x.__ge__, None)

    def test_small_series(self) -> None:
        # OR-Tools issue #4525.
        model = cp_model.CpModel()
        x = model.new_bool_var("foo")
        y = model.new_bool_var("bar")
        z = model.new_bool_var("baz")

        s1 = pd.Series([x, y], index=[1, 2])
        self.assertEqual(str(s1.sum()), "(foo + bar)")
        s2 = pd.Series([1, -1], index=[1, 2])
        self.assertEqual(str(s1.dot(s2)), "(foo + (-bar))")

        s3 = pd.Series([x], index=[1])
        self.assertIs(s3.sum(), x)
        s4 = pd.Series([1], index=[1])
        self.assertIs(s3.dot(s4), x)

        s5 = pd.Series([x, y, z], index=[1, 2, 3])
        self.assertEqual(str(s5.sum()), "(foo + bar + baz)")
        s6 = pd.Series([1, -2, 1], index=[1, 2, 3])
        self.assertEqual(str(s5.dot(s6)), "(foo + (-2 * bar) + baz)")

    def test_issue4376_sat_model(self) -> None:
        letters: str = "BCFLMRT"

        def symbols_from_string(text: str) -> list[int]:
            return [letters.index(char) for char in text]

        def rotate_symbols(symbols: list[int], turns: int) -> list[int]:
            return symbols[turns:] + symbols[:turns]

        data = """FMRC
FTLB
MCBR
FRTM
FBTM
BRFM
BTRM
BCRM
RTCF
TFRC
CTRM
CBTM
TFBM
TCBM
CFTM
BLTR
RLFM
CFLM
CRML
FCLR
FBTR
TBRF
RBCF
RBCT
BCTF
TFCR
CBRT
FCBT
FRTB
RBCM
MTFC
MFTC
MBFC
RTBM
RBFM
TRFM"""

        tiles = [symbols_from_string(line) for line in data.splitlines()]

        model = cp_model.CpModel()

        # choices[i, x, y, r] is true iff we put tile i in cell (x,y) with
        # rotation r.
        choices = {}
        for i in range(len(tiles)):
            for x in range(6):
                for y in range(6):
                    for r in range(4):
                        choices[(i, x, y, r)] = model.new_bool_var(
                            f"tile_{i}_{x}_{y}_{r}"
                        )

        # corners[x, y, s] is true iff the corner at (x,y) contains symbol s.
        corners = {}
        for x in range(7):
            for y in range(7):
                for s in range(7):
                    corners[(x, y, s)] = model.new_bool_var(f"corner_{x}_{y}_{s}")

        # Placing a tile puts a symbol in each corner.
        for (i, x, y, r), choice in choices.items():
            symbols = rotate_symbols(tiles[i], r)
            model.add_implication(choice, corners[x, y, symbols[0]])
            model.add_implication(choice, corners[x, y + 1, symbols[1]])
            model.add_implication(choice, corners[x + 1, y + 1, symbols[2]])
            model.add_implication(choice, corners[x + 1, y, symbols[3]])

        # We must make exactly one choice for each tile.
        for i in range(len(tiles)):
            tmp_literals = []
            for x in range(6):
                for y in range(6):
                    for r in range(4):
                        tmp_literals.append(choices[(i, x, y, r)])
            model.add_exactly_one(tmp_literals)

        # We must make exactly one choice for each square.
        for x, y in itertools.product(range(6), range(6)):
            tmp_literals = []
            for i in range(len(tiles)):
                for r in range(4):
                    tmp_literals.append(choices[(i, x, y, r)])
            model.add_exactly_one(tmp_literals)

        # Each corner contains exactly one symbol.
        for x, y in itertools.product(range(7), range(7)):
            model.add_exactly_one(corners[x, y, s] for s in range(7))

        # Solve.
        solver = cp_model.CpSolver()
        solver.parameters.num_workers = 8
        solver.parameters.max_time_in_seconds = 20
        solver.parameters.log_search_progress = True
        solver.parameters.cp_model_presolve = False
        solver.parameters.symmetry_level = 0

        solution_callback = TimeRecorder()
        status = solver.Solve(model, solution_callback)
        if status == cp_model.OPTIMAL:
            self.assertLess(time.time(), solution_callback.last_time + 5.0)

    def test_issue4376_minimize_model(self) -> None:
        model = cp_model.CpModel()

        jobs = [
            [3, 3],  # [duration, width]
            [2, 5],
            [1, 3],
            [3, 7],
            [7, 3],
            [2, 2],
            [2, 2],
            [5, 5],
            [10, 2],
            [4, 3],
            [2, 6],
            [1, 2],
            [6, 8],
            [4, 5],
            [3, 7],
        ]

        max_width = 10

        horizon = sum(t[0] for t in jobs)

        intervals = []
        intervals0 = []
        intervals1 = []
        performed = []
        starts = []
        ends = []
        demands = []

        for i, job in enumerate(jobs):
            # Create main interval.
            start = model.new_int_var(0, horizon, f"start_{i}")
            duration, width = job
            end = model.new_int_var(0, horizon, f"end_{i}")
            interval = model.new_interval_var(start, duration, end, f"interval_{i}")
            starts.append(start)
            intervals.append(interval)
            ends.append(end)
            demands.append(width)

            # Create an optional copy of interval to be executed on machine 0.
            performed_on_m0 = model.new_bool_var(f"perform_{i}_on_m0")
            performed.append(performed_on_m0)
            start0 = model.new_int_var(0, horizon, f"start_{i}_on_m0")
            end0 = model.new_int_var(0, horizon, f"end_{i}_on_m0")
            interval0 = model.new_optional_interval_var(
                start0, duration, end0, performed_on_m0, f"interval_{i}_on_m0"
            )
            intervals0.append(interval0)

            # Create an optional copy of interval to be executed on machine 1.
            start1 = model.new_int_var(0, horizon, f"start_{i}_on_m1")
            end1 = model.new_int_var(0, horizon, f"end_{i}_on_m1")
            interval1 = model.new_optional_interval_var(
                start1,
                duration,
                end1,
                ~performed_on_m0,
                f"interval_{i}_on_m1",
            )
            intervals1.append(interval1)

            # We only propagate the constraint if the tasks is performed on the
            # machine.
            model.add(start0 == start).only_enforce_if(performed_on_m0)
            model.add(start1 == start).only_enforce_if(~performed_on_m0)

        # Width constraint (modeled as a cumulative)
        model.add_cumulative(intervals, demands, max_width)

        # Choose which machine to perform the jobs on.
        model.add_no_overlap(intervals0)
        model.add_no_overlap(intervals1)

        # Objective variable.
        makespan = model.new_int_var(0, horizon, "makespan")
        model.add_max_equality(makespan, ends)
        model.minimize(makespan)

        # Symmetry breaking.
        model.add(performed[0] == 0)

        # Solve.
        solver = cp_model.CpSolver()
        solver.parameters.num_workers = 8
        solver.parameters.max_time_in_seconds = 50
        solver.parameters.log_search_progress = True
        solution_callback = TimeRecorder()
        best_bound_callback = BestBoundTimeCallback()
        solver.best_bound_callback = best_bound_callback.new_best_bound
        status = solver.Solve(model, solution_callback)
        if status == cp_model.OPTIMAL:
            self.assertLess(
                time.time(),
                max(best_bound_callback.last_time, solution_callback.last_time) + 9.0,
            )

    def test_issue4434(self) -> None:
        model = cp_model.CpModel()
        i = model.NewIntVar(0, 10, "i")
        j = model.NewIntVar(0, 10, "j")

        # Causes a mypy error: Argument has incompatible type
        # "BoundedLinearExpression | bool"; expected "BoundedLinearExpression"
        expr_eq: cp_model.BoundedLinearExpression = i + j == 5
        expr_ne: cp_model.BoundedLinearExpression = i + j != 5

        # This works fine with other comparison operators
        expr_ge: cp_model.BoundedLinearExpression = i + j >= 5

        self.assertIsNotNone(expr_eq)
        self.assertIsNotNone(expr_ne)
        self.assertIsNotNone(expr_ge)

    def test_raise_python_exception_in_callback(self) -> None:
        model = cp_model.CpModel()

        jobs = [
            [3, 3],  # [duration, width]
            [2, 5],
            [1, 3],
            [3, 7],
            [7, 3],
            [2, 2],
            [2, 2],
            [5, 5],
            [10, 2],
            [4, 3],
            [2, 6],
            [1, 2],
            [6, 8],
            [4, 5],
            [3, 7],
        ]

        max_width = 10

        horizon = sum(t[0] for t in jobs)

        intervals = []
        intervals0 = []
        intervals1 = []
        performed = []
        starts = []
        ends = []
        demands = []

        for i, job in enumerate(jobs):
            # Create main interval.
            start = model.new_int_var(0, horizon, f"start_{i}")
            duration, width = job
            end = model.new_int_var(0, horizon, f"end_{i}")
            interval = model.new_interval_var(start, duration, end, f"interval_{i}")
            starts.append(start)
            intervals.append(interval)
            ends.append(end)
            demands.append(width)

            # Create an optional copy of interval to be executed on machine 0.
            performed_on_m0 = model.new_bool_var(f"perform_{i}_on_m0")
            performed.append(performed_on_m0)
            start0 = model.new_int_var(0, horizon, f"start_{i}_on_m0")
            end0 = model.new_int_var(0, horizon, f"end_{i}_on_m0")
            interval0 = model.new_optional_interval_var(
                start0, duration, end0, performed_on_m0, f"interval_{i}_on_m0"
            )
            intervals0.append(interval0)

            # Create an optional copy of interval to be executed on machine 1.
            start1 = model.new_int_var(0, horizon, f"start_{i}_on_m1")
            end1 = model.new_int_var(0, horizon, f"end_{i}_on_m1")
            interval1 = model.new_optional_interval_var(
                start1,
                duration,
                end1,
                ~performed_on_m0,
                f"interval_{i}_on_m1",
            )
            intervals1.append(interval1)

            # We only propagate the constraint if the tasks is performed on the
            # machine.
            model.add(start0 == start).only_enforce_if(performed_on_m0)
            model.add(start1 == start).only_enforce_if(~performed_on_m0)

        # Width constraint (modeled as a cumulative)
        model.add_cumulative(intervals, demands, max_width)

        # Choose which machine to perform the jobs on.
        model.add_no_overlap(intervals0)
        model.add_no_overlap(intervals1)

        # Objective variable.
        makespan = model.new_int_var(0, horizon, "makespan")
        model.add_max_equality(makespan, ends)
        model.minimize(makespan)

        # Symmetry breaking.
        model.add(performed[0] == 0)

        solver = cp_model.CpSolver()
        solver.parameters.log_search_progress = True
        solver.parameters.num_workers = 1
        msg: str = "this is my test message"
        callback = RaiseException(msg)

        with self.assertRaisesRegex(ValueError, msg):
            solver.solve(model, callback)

    def test_in_place_sum_modifications(self) -> None:
        model = cp_model.CpModel()
        x = [model.new_int_var(0, 10, f"x{i}") for i in range(5)]
        y = [model.new_int_var(0, 10, f"y{i}") for i in range(5)]
        e1 = sum(x)
        self.assertIsInstance(e1, cmh.SumArray)
        self.assertEqual(e1.int_offset, 0)
        self.assertEqual(e1.double_offset, 0)
        self.assertEqual(e1.num_exprs, 5)
        e1_str = str(e1)
        _ = e1 + y[0]
        _ = sum(y) + e1
        self.assertEqual(e1_str, str(e1))

        e2 = sum(x) - 2 - y[0] - 0.1
        e2_str = str(e2)
        self.assertIsInstance(e2, cmh.SumArray)
        self.assertEqual(e2.int_offset, -2)
        self.assertEqual(e2.double_offset, -0.1)
        self.assertEqual(e2.num_exprs, 6)
        _ = e2 + 2.5
        self.assertEqual(str(e2), e2_str)

        e3 = 1.2 + sum(x) + 0.3
        self.assertIsInstance(e3, cmh.SumArray)
        self.assertEqual(e3.int_offset, 0)
        self.assertEqual(e3.double_offset, 1.5)
        self.assertEqual(e3.num_exprs, 5)

    def test_large_sum(self) -> None:
        model = cp_model.CpModel()
        x = [model.new_int_var(0, 10, f"x{i}") for i in range(100000)]
        model.add(sum(x) == 10)

    def test_large_iadd(self):
        model = cp_model.CpModel()
        s = 0
        for _ in range(300000):
            s += model.new_bool_var("")
        model.add(s == 10)

    def test_large_isub(self):
        model = cp_model.CpModel()
        s = 0
        for _ in range(300000):
            s -= model.new_bool_var("")
        model.add(s == 10)

    def test_simplification1(self):
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        prod = (x * 2) * 2
        self.assertIsInstance(prod, cmh.IntAffine)
        self.assertEqual(x, prod.expression)
        self.assertEqual(4, prod.coefficient)
        self.assertEqual(0, prod.offset)

    def test_simplification2(self):
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        prod = 2 * (x * 2)
        self.assertIsInstance(prod, cmh.IntAffine)
        self.assertEqual(x, prod.expression)
        self.assertEqual(4, prod.coefficient)
        self.assertEqual(0, prod.offset)

    def test_simplification3(self):
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        prod = (2 * x) * 2
        self.assertIsInstance(prod, cmh.IntAffine)
        self.assertEqual(x, prod.expression)
        self.assertEqual(4, prod.coefficient)
        self.assertEqual(0, prod.offset)

    def test_simplification4(self):
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        prod = 2 * (2 * x)
        self.assertIsInstance(prod, cmh.IntAffine)
        self.assertEqual(x, prod.expression)
        self.assertEqual(4, prod.coefficient)
        self.assertEqual(0, prod.offset)

    def test_simplification5(self):
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        prod = 2 * (x + 1)
        self.assertIsInstance(prod, cmh.IntAffine)
        self.assertEqual(x, prod.expression)
        self.assertEqual(2, prod.coefficient)
        self.assertEqual(2, prod.offset)

    def test_simplification6(self):
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        prod = (x + 1) * 2
        self.assertIsInstance(prod, cmh.IntAffine)
        self.assertEqual(x, prod.expression)
        self.assertEqual(2, prod.coefficient)
        self.assertEqual(2, prod.offset)

    def test_simplification7(self):
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        prod = 2 * (x - 1)
        self.assertIsInstance(prod, cmh.IntAffine)
        self.assertEqual(x, prod.expression)
        self.assertEqual(2, prod.coefficient)
        self.assertEqual(-2, prod.offset)

    def test_simplification8(self):
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        prod = (x - 1) * 2
        self.assertIsInstance(prod, cmh.IntAffine)
        self.assertEqual(x, prod.expression)
        self.assertEqual(2, prod.coefficient)
        self.assertEqual(-2, prod.offset)

    def test_simplification9(self):
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        prod = 2 * (1 - x)
        self.assertIsInstance(prod, cmh.IntAffine)
        self.assertEqual(x, prod.expression)
        self.assertEqual(-2, prod.coefficient)
        self.assertEqual(2, prod.offset)

    def test_simplification10(self):
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        prod = (1 - x) * 2
        self.assertIsInstance(prod, cmh.IntAffine)
        self.assertEqual(x, prod.expression)
        self.assertEqual(-2, prod.coefficient)
        self.assertEqual(2, prod.offset)


if __name__ == "__main__":
    absltest.main()
