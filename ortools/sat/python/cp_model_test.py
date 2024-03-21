#!/usr/bin/env python3
# Copyright 2010-2024 Google LLC
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

"""Tests for ortools.sat.python.cp_model."""

from absl.testing import absltest
import pandas as pd

from ortools.sat.python import cp_model


class SolutionCounter(cp_model.CpSolverSolutionCallback):
    """Count solutions."""

    def __init__(self):
        cp_model.CpSolverSolutionCallback.__init__(self)
        self.__solution_count = 0

    def on_solution_callback(self):
        self.__solution_count += 1

    @property
    def solution_count(self):
        return self.__solution_count


class SolutionSum(cp_model.CpSolverSolutionCallback):
    """Record the sum of variables in the solution."""

    def __init__(self, variables):
        cp_model.CpSolverSolutionCallback.__init__(self)
        self.__sum = 0
        self.__vars = variables

    def on_solution_callback(self):
        self.__sum = sum(self.value(x) for x in self.__vars)

    @property
    def sum(self):
        return self.__sum


class SolutionObjective(cp_model.CpSolverSolutionCallback):
    """Record the objective value of the solution."""

    def __init__(self):
        cp_model.CpSolverSolutionCallback.__init__(self)
        self.__obj = 0

    def on_solution_callback(self):
        self.__obj = self.objective_value

    @property
    def obj(self):
        return self.__obj


class RecordSolution(cp_model.CpSolverSolutionCallback):
    """Record the objective value of the solution."""

    def __init__(
        self,
        int_vars: list[cp_model.VariableT],
        bool_vars: list[cp_model.LiteralT],
    ):
        cp_model.CpSolverSolutionCallback.__init__(self)
        self.__int_vars = int_vars
        self.__bool_vars = bool_vars
        self.__int_var_values: list[int] = []
        self.__bool_var_values: list[bool] = []

    def on_solution_callback(self):
        for int_var in self.__int_vars:
            self.__int_var_values.append(self.value(int_var))
        for bool_var in self.__bool_vars:
            self.__bool_var_values.append(self.boolean_value(bool_var))

    @property
    def int_var_values(self):
        return self.__int_var_values

    @property
    def bool_var_values(self):
        return self.__bool_var_values


class LogToString:
    """Record log in a string."""

    def __init__(self):
        self.__log = ""

    def new_message(self, message: str):
        self.__log += message
        self.__log += "\n"

    @property
    def log(self):
        return self.__log


class CpModelTest(absltest.TestCase):

    def testCreateIntegerVariable(self):
        print("testCreateIntegerVariable")
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

    def testNegation(self):
        print("testNegation")
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

    def testEqualityOverload(self):
        print("testEqualityOverload")
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        y = model.new_int_var(0, 5, "y")
        self.assertEqual(x, x)
        self.assertNotEqual(x, y)

    def testLinear(self):
        print("testLinear")
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        y = model.new_int_var(-10, 10, "y")
        model.add_linear_constraint(x + 2 * y, 0, 10)
        model.minimize(y)
        solver = cp_model.CpSolver()
        self.assertEqual(cp_model.OPTIMAL, solver.solve(model))
        self.assertEqual(10, solver.value(x))
        self.assertEqual(-5, solver.value(y))

    def testLinearConstraint(self):
        print("testLinear")
        model = cp_model.CpModel()
        model.add_linear_constraint(5, 0, 10)
        model.add_linear_constraint(-1, 0, 10)
        self.assertLen(model.proto.constraints, 2)
        self.assertTrue(model.proto.constraints[0].HasField("bool_and"))
        self.assertEmpty(model.proto.constraints[0].bool_and.literals)
        self.assertTrue(model.proto.constraints[1].HasField("bool_or"))
        self.assertEmpty(model.proto.constraints[1].bool_or.literals)

    def testLinearNonEqual(self):
        print("testLinearNonEqual")
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        y = model.new_int_var(-10, 10, "y")
        ct = model.add(-x + y != 3).proto
        self.assertLen(ct.linear.domain, 4)
        self.assertEqual(cp_model.INT_MIN, ct.linear.domain[0])
        self.assertEqual(2, ct.linear.domain[1])
        self.assertEqual(4, ct.linear.domain[2])
        self.assertEqual(cp_model.INT_MAX, ct.linear.domain[3])

    def testEq(self):
        print("testEq")
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        ct = model.add(x == 2).proto
        self.assertLen(ct.linear.vars, 1)
        self.assertLen(ct.linear.coeffs, 1)
        self.assertLen(ct.linear.domain, 2)
        self.assertEqual(2, ct.linear.domain[0])
        self.assertEqual(2, ct.linear.domain[1])

    def testGe(self):
        print("testGe")
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        ct = model.add(x >= 2).proto
        self.assertLen(ct.linear.vars, 1)
        self.assertLen(ct.linear.coeffs, 1)
        self.assertLen(ct.linear.domain, 2)
        self.assertEqual(2, ct.linear.domain[0])
        self.assertEqual(cp_model.INT_MAX, ct.linear.domain[1])

    def testGt(self):
        print("testGt")
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        ct = model.add(x > 2).proto
        self.assertLen(ct.linear.vars, 1)
        self.assertLen(ct.linear.coeffs, 1)
        self.assertLen(ct.linear.domain, 2)
        self.assertEqual(3, ct.linear.domain[0])
        self.assertEqual(cp_model.INT_MAX, ct.linear.domain[1])

    def testLe(self):
        print("testLe")
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        ct = model.add(x <= 2).proto
        self.assertLen(ct.linear.vars, 1)
        self.assertLen(ct.linear.coeffs, 1)
        self.assertLen(ct.linear.domain, 2)
        self.assertEqual(cp_model.INT_MIN, ct.linear.domain[0])
        self.assertEqual(2, ct.linear.domain[1])

    def testLt(self):
        print("testLt")
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        ct = model.add(x < 2).proto
        self.assertLen(ct.linear.vars, 1)
        self.assertLen(ct.linear.coeffs, 1)
        self.assertLen(ct.linear.domain, 2)
        self.assertEqual(cp_model.INT_MIN, ct.linear.domain[0])
        self.assertEqual(1, ct.linear.domain[1])

    def testEqVar(self):
        print("testEqVar")
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

    def testGeVar(self):
        print("testGeVar")
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

    def testGtVar(self):
        print("testGeVar")
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

    def testLeVar(self):
        print("testLeVar")
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

    def testLtVar(self):
        print("testLtVar")
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

    def testSimplification1(self):
        print("testSimplification1")
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        prod = (x * 2) * 2
        self.assertEqual(x, prod.expression())
        self.assertEqual(4, prod.coefficient())

    def testSimplification2(self):
        print("testSimplification2")
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        prod = 2 * (x * 2)
        self.assertEqual(x, prod.expression())
        self.assertEqual(4, prod.coefficient())

    def testSimplification3(self):
        print("testSimplification3")
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        prod = (2 * x) * 2
        self.assertEqual(x, prod.expression())
        self.assertEqual(4, prod.coefficient())

    def testSimplification4(self):
        print("testSimplification4")
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        prod = 2 * (2 * x)
        self.assertEqual(x, prod.expression())
        self.assertEqual(4, prod.coefficient())

    def testLinearNonEqualWithConstant(self):
        print("testLinearNonEqualWithConstant")
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

    def testLinearWithEnforcement(self):
        print("testLinearWithEnforcement")
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

    def testConstraintWithName(self):
        print("testConstraintWithName")
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        y = model.new_int_var(-10, 10, "y")
        ct = model.add_linear_constraint(x + 2 * y, 0, 10).with_name("test_constraint")
        self.assertEqual("test_constraint", ct.name)

    def testNaturalApiMinimize(self):
        print("testNaturalApiMinimize")
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

    def testNaturalApiMaximizeFloat(self):
        print("testNaturalApiMaximizeFloat")
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

    def testNaturalApiMaximizeComplex(self):
        print("testNaturalApiMaximizeFloat")
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

    def testNaturalApiMaximize(self):
        print("testNaturalApiMaximize")
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

    def testMinimizeConstant(self):
        print("testMinimizeConstant")
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        model.add(x >= -1)
        model.minimize(10)
        solver = cp_model.CpSolver()
        self.assertEqual("OPTIMAL", solver.status_name(solver.solve(model)))
        self.assertEqual(10, solver.objective_value)

    def testMaximizeConstant(self):
        print("testMinimizeConstant")
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        model.add(x >= -1)
        model.maximize(5)
        solver = cp_model.CpSolver()
        self.assertEqual("OPTIMAL", solver.status_name(solver.solve(model)))
        self.assertEqual(5, solver.objective_value)

    def testAddTrue(self):
        print("testAddTrue")
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        model.add(3 >= -1)
        model.minimize(x)
        solver = cp_model.CpSolver()
        self.assertEqual("OPTIMAL", solver.status_name(solver.solve(model)))
        self.assertEqual(-10, solver.value(x))

    def testAddFalse(self):
        print("testAddFalse")
        model = cp_model.CpModel()
        x = model.new_int_var(-10, 10, "x")
        model.add(3 <= -1)
        model.minimize(x)
        solver = cp_model.CpSolver()
        self.assertEqual("INFEASIBLE", solver.status_name(solver.solve(model)))

    def testSum(self):
        print("testSum")
        model = cp_model.CpModel()
        x = [model.new_int_var(0, 2, "x%i" % i) for i in range(100)]
        model.add(sum(x) <= 1)
        model.maximize(x[99])
        solver = cp_model.CpSolver()
        self.assertEqual(cp_model.OPTIMAL, solver.solve(model))
        self.assertEqual(1.0, solver.objective_value)
        for i in range(100):
            self.assertEqual(solver.value(x[i]), 1 if i == 99 else 0)

    def testSumWithApi(self):
        print("testSumWithApi")
        model = cp_model.CpModel()
        x = [model.new_int_var(0, 2, "x%i" % i) for i in range(100)]
        model.add(cp_model.LinearExpr.sum(x) <= 1)
        model.maximize(x[99])
        solver = cp_model.CpSolver()
        self.assertEqual(cp_model.OPTIMAL, solver.solve(model))
        self.assertEqual(1.0, solver.objective_value)
        for i in range(100):
            self.assertEqual(solver.value(x[i]), 1 if i == 99 else 0)

    def testWeightedSum(self):
        print("testWeightedSum")
        model = cp_model.CpModel()
        x = [model.new_int_var(0, 2, "x%i" % i) for i in range(100)]
        c = [2] * 100
        model.add(cp_model.LinearExpr.weighted_sum(x, c) <= 3)
        model.maximize(x[99])
        solver = cp_model.CpSolver()
        self.assertEqual(cp_model.OPTIMAL, solver.solve(model))
        self.assertEqual(1.0, solver.objective_value)
        for i in range(100):
            self.assertEqual(solver.value(x[i]), 1 if i == 99 else 0)

    def testAllDifferent(self):
        print("testAllDifferent")
        model = cp_model.CpModel()
        x = [model.new_int_var(0, 4, "x%i" % i) for i in range(5)]
        model.add_all_different(x)
        self.assertLen(model.proto.variables, 5)
        self.assertLen(model.proto.constraints, 1)
        self.assertLen(model.proto.constraints[0].all_diff.exprs, 5)

    def testAllDifferentGen(self):
        print("testAllDifferentGen")
        model = cp_model.CpModel()
        model.add_all_different(model.new_int_var(0, 4, "x%i" % i) for i in range(5))
        self.assertLen(model.proto.variables, 5)
        self.assertLen(model.proto.constraints, 1)
        self.assertLen(model.proto.constraints[0].all_diff.exprs, 5)

    def testAllDifferentList(self):
        print("testAllDifferentList")
        model = cp_model.CpModel()
        x = [model.new_int_var(0, 4, "x%i" % i) for i in range(5)]
        model.add_all_different(x[0], x[1], x[2], x[3], x[4])
        self.assertLen(model.proto.variables, 5)
        self.assertLen(model.proto.constraints, 1)
        self.assertLen(model.proto.constraints[0].all_diff.exprs, 5)

    def testElement(self):
        print("testElement")
        model = cp_model.CpModel()
        x = [model.new_int_var(0, 4, "x%i" % i) for i in range(5)]
        model.add_element(x[0], [x[1], 2, 4, x[2]], x[4])
        self.assertLen(model.proto.variables, 7)
        self.assertLen(model.proto.constraints, 1)
        self.assertLen(model.proto.constraints[0].element.vars, 4)
        self.assertEqual(0, model.proto.constraints[0].element.index)
        self.assertEqual(4, model.proto.constraints[0].element.target)
        self.assertRaises(ValueError, model.add_element, x[0], [], x[4])

    def testFixedElement(self):
        print("testFixedElement")
        model = cp_model.CpModel()
        x = [model.new_int_var(0, 4, "x%i" % i) for i in range(4)]
        model.add_element(1, [x[0], 2, 4, x[2]], x[3])
        self.assertLen(model.proto.variables, 4)
        self.assertLen(model.proto.constraints, 1)
        self.assertLen(model.proto.constraints[0].linear.vars, 1)
        self.assertEqual(x[3].index, model.proto.constraints[0].linear.vars[0])
        self.assertEqual(1, model.proto.constraints[0].linear.coeffs[0])
        self.assertEqual([2, 2], model.proto.constraints[0].linear.domain)

    def testCircuit(self):
        print("testCircuit")
        model = cp_model.CpModel()
        x = [model.new_bool_var(f"x{i}") for i in range(5)]
        model.add_circuit((i, i + 1, x[i]) for i in range(5))
        self.assertLen(model.proto.variables, 5)
        self.assertLen(model.proto.constraints, 1)
        self.assertLen(model.proto.constraints[0].circuit.heads, 5)
        self.assertLen(model.proto.constraints[0].circuit.tails, 5)
        self.assertLen(model.proto.constraints[0].circuit.literals, 5)
        self.assertRaises(ValueError, model.add_circuit, [])

    def testMultipleCircuit(self):
        print("testMultipleCircuit")
        model = cp_model.CpModel()
        x = [model.new_bool_var(f"x{i}") for i in range(5)]
        model.add_multiple_circuit((i, i + 1, x[i]) for i in range(5))
        self.assertLen(model.proto.variables, 5)
        self.assertLen(model.proto.constraints, 1)
        self.assertLen(model.proto.constraints[0].routes.heads, 5)
        self.assertLen(model.proto.constraints[0].routes.tails, 5)
        self.assertLen(model.proto.constraints[0].routes.literals, 5)
        self.assertRaises(ValueError, model.add_multiple_circuit, [])

    def testAllowedAssignments(self):
        print("testAllowedAssignments")
        model = cp_model.CpModel()
        x = [model.new_int_var(0, 4, "x%i" % i) for i in range(5)]
        model.add_allowed_assignments(
            x, [(0, 1, 2, 3, 4), (4, 3, 2, 1, 1), (0, 0, 0, 0, 0)]
        )
        self.assertLen(model.proto.variables, 5)
        self.assertLen(model.proto.constraints, 1)
        self.assertLen(model.proto.constraints[0].table.vars, 5)
        self.assertLen(model.proto.constraints[0].table.values, 15)
        self.assertRaises(
            TypeError,
            model.add_allowed_assignments,
            x,
            [(0, 1, 2, 3, 4), (4, 3, 2, 1, 1), (0, 0, 0, 0)],
        )
        self.assertRaises(
            ValueError,
            model.add_allowed_assignments,
            [],
            [(0, 1, 2, 3, 4), (4, 3, 2, 1, 1), (0, 0, 0, 0)],
        )

    def testForbiddenAssignments(self):
        print("testForbiddenAssignments")
        model = cp_model.CpModel()
        x = [model.new_int_var(0, 4, "x%i" % i) for i in range(5)]
        model.add_forbidden_assignments(
            x, [(0, 1, 2, 3, 4), (4, 3, 2, 1, 1), (0, 0, 0, 0, 0)]
        )
        self.assertLen(model.proto.variables, 5)
        self.assertLen(model.proto.constraints, 1)
        self.assertLen(model.proto.constraints[0].table.vars, 5)
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

    def testAutomaton(self):
        print("testAutomaton")
        model = cp_model.CpModel()
        x = [model.new_int_var(0, 4, "x%i" % i) for i in range(5)]
        model.add_automaton(x, 0, [2, 3], [(0, 0, 0), (0, 1, 1), (1, 2, 2), (2, 3, 3)])
        self.assertLen(model.proto.variables, 5)
        self.assertLen(model.proto.constraints, 1)
        self.assertLen(model.proto.constraints[0].automaton.vars, 5)
        self.assertLen(model.proto.constraints[0].automaton.transition_tail, 4)
        self.assertLen(model.proto.constraints[0].automaton.transition_head, 4)
        self.assertLen(model.proto.constraints[0].automaton.transition_label, 4)
        self.assertLen(model.proto.constraints[0].automaton.final_states, 2)
        self.assertEqual(0, model.proto.constraints[0].automaton.starting_state)
        self.assertRaises(
            TypeError,
            model.add_automaton,
            x,
            0,
            [2, 3],
            [(0, 0, 0), (0, 1, 1), (2, 2), (2, 3, 3)],
        )
        self.assertRaises(
            ValueError,
            model.add_automaton,
            [],
            0,
            [2, 3],
            [(0, 0, 0), (0, 1, 1), (2, 3, 3)],
        )
        self.assertRaises(
            ValueError,
            model.add_automaton,
            x,
            0,
            [],
            [(0, 0, 0), (0, 1, 1), (2, 3, 3)],
        )
        self.assertRaises(ValueError, model.add_automaton, x, 0, [2, 3], [])

    def testInverse(self):
        print("testInverse")
        model = cp_model.CpModel()
        x = [model.new_int_var(0, 4, "x%i" % i) for i in range(5)]
        y = [model.new_int_var(0, 4, "y%i" % i) for i in range(5)]
        model.add_inverse(x, y)
        self.assertLen(model.proto.variables, 10)
        self.assertLen(model.proto.constraints, 1)
        self.assertLen(model.proto.constraints[0].inverse.f_direct, 5)
        self.assertLen(model.proto.constraints[0].inverse.f_inverse, 5)

    def testMaxEquality(self):
        print("testMaxEquality")
        model = cp_model.CpModel()
        x = model.new_int_var(0, 4, "x")
        y = [model.new_int_var(0, 4, "y%i" % i) for i in range(5)]
        model.add_max_equality(x, y)
        self.assertLen(model.proto.variables, 6)
        self.assertLen(model.proto.constraints, 1)
        self.assertLen(model.proto.constraints[0].lin_max.exprs, 5)
        self.assertEqual(0, model.proto.constraints[0].lin_max.target.vars[0])
        self.assertEqual(1, model.proto.constraints[0].lin_max.target.coeffs[0])

    def testMinEquality(self):
        print("testMinEquality")
        model = cp_model.CpModel()
        x = model.new_int_var(0, 4, "x")
        y = [model.new_int_var(0, 4, "y%i" % i) for i in range(5)]
        model.add_min_equality(x, y)
        self.assertLen(model.proto.variables, 6)
        self.assertLen(model.proto.constraints[0].lin_max.exprs, 5)
        self.assertEqual(0, model.proto.constraints[0].lin_max.target.vars[0])
        self.assertEqual(-1, model.proto.constraints[0].lin_max.target.coeffs[0])

    def testMinEqualityList(self):
        print("testMinEqualityList")
        model = cp_model.CpModel()
        x = model.new_int_var(0, 4, "x")
        y = [model.new_int_var(0, 4, "y%i" % i) for i in range(5)]
        model.add_min_equality(x, [y[0], y[2], y[1], y[3]])
        self.assertLen(model.proto.variables, 6)
        self.assertLen(model.proto.constraints[0].lin_max.exprs, 4)
        self.assertEqual(0, model.proto.constraints[0].lin_max.target.vars[0])
        self.assertEqual(-1, model.proto.constraints[0].lin_max.target.coeffs[0])

    def testMinEqualityTuple(self):
        print("testMinEqualityTuple")
        model = cp_model.CpModel()
        x = model.new_int_var(0, 4, "x")
        y = [model.new_int_var(0, 4, "y%i" % i) for i in range(5)]
        model.add_min_equality(x, (y[0], y[2], y[1], y[3]))
        self.assertLen(model.proto.variables, 6)
        self.assertLen(model.proto.constraints[0].lin_max.exprs, 4)
        self.assertEqual(0, model.proto.constraints[0].lin_max.target.vars[0])
        self.assertEqual(-1, model.proto.constraints[0].lin_max.target.coeffs[0])

    def testMinEqualityGenerator(self):
        print("testMinEqualityGenerator")
        model = cp_model.CpModel()
        x = model.new_int_var(0, 4, "x")
        y = [model.new_int_var(0, 4, "y%i" % i) for i in range(5)]
        model.add_min_equality(x, (z for z in y))
        self.assertLen(model.proto.variables, 6)
        self.assertLen(model.proto.constraints[0].lin_max.exprs, 5)
        self.assertEqual(0, model.proto.constraints[0].lin_max.target.vars[0])
        self.assertEqual(-1, model.proto.constraints[0].lin_max.target.coeffs[0])

    def testMinEqualityWithConstant(self):
        print("testMinEqualityWithConstant")
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

    def testAbs(self):
        print("testAbs")
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

    def testDivision(self):
        print("testDivision")
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

    def testModulo(self):
        print("testModulo")
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

    def testMultiplicationEquality(self):
        print("testMultiplicationEquality")
        model = cp_model.CpModel()
        x = model.new_int_var(0, 4, "x")
        y = [model.new_int_var(0, 4, "y%i" % i) for i in range(5)]
        model.add_multiplication_equality(x, y)
        self.assertLen(model.proto.variables, 6)
        self.assertLen(model.proto.constraints, 1)
        self.assertLen(model.proto.constraints[0].int_prod.exprs, 5)
        self.assertEqual(0, model.proto.constraints[0].int_prod.target.vars[0])

    def testImplication(self):
        print("testImplication")
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

    def testBoolOr(self):
        print("testBoolOr")
        model = cp_model.CpModel()
        x = [model.new_bool_var("x%i" % i) for i in range(5)]
        model.add_bool_or(x)
        self.assertLen(model.proto.variables, 5)
        self.assertLen(model.proto.constraints, 1)
        self.assertLen(model.proto.constraints[0].bool_or.literals, 5)
        model.add_bool_or([x[0], x[1], False])
        self.assertLen(model.proto.variables, 6)
        self.assertRaises(TypeError, model.add_bool_or, [x[2], 2])
        y = model.new_int_var(0, 4, "y")
        self.assertRaises(TypeError, model.add_bool_or, [y, False])

    def testBoolOrListOrGet(self):
        print("testBoolOrListOrGet")
        model = cp_model.CpModel()
        x = [model.new_bool_var("x%i" % i) for i in range(5)]
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

    def testAtLeastOne(self):
        print("testAtLeastOne")
        model = cp_model.CpModel()
        x = [model.new_bool_var("x%i" % i) for i in range(5)]
        model.add_at_least_one(x)
        self.assertLen(model.proto.variables, 5)
        self.assertLen(model.proto.constraints, 1)
        self.assertLen(model.proto.constraints[0].bool_or.literals, 5)
        model.add_at_least_one([x[0], x[1], False])
        self.assertLen(model.proto.variables, 6)
        self.assertRaises(TypeError, model.add_at_least_one, [x[2], 2])
        y = model.new_int_var(0, 4, "y")
        self.assertRaises(TypeError, model.add_at_least_one, [y, False])

    def testAtMostOne(self):
        print("testAtMostOne")
        model = cp_model.CpModel()
        x = [model.new_bool_var("x%i" % i) for i in range(5)]
        model.add_at_most_one(x)
        self.assertLen(model.proto.variables, 5)
        self.assertLen(model.proto.constraints, 1)
        self.assertLen(model.proto.constraints[0].at_most_one.literals, 5)
        model.add_at_most_one([x[0], x[1], False])
        self.assertLen(model.proto.variables, 6)
        self.assertRaises(TypeError, model.add_at_most_one, [x[2], 2])
        y = model.new_int_var(0, 4, "y")
        self.assertRaises(TypeError, model.add_at_most_one, [y, False])

    def testExactlyOne(self):
        print("testExactlyOne")
        model = cp_model.CpModel()
        x = [model.new_bool_var("x%i" % i) for i in range(5)]
        model.add_exactly_one(x)
        self.assertLen(model.proto.variables, 5)
        self.assertLen(model.proto.constraints, 1)
        self.assertLen(model.proto.constraints[0].exactly_one.literals, 5)
        model.add_exactly_one([x[0], x[1], False])
        self.assertLen(model.proto.variables, 6)
        self.assertRaises(TypeError, model.add_exactly_one, [x[2], 2])
        y = model.new_int_var(0, 4, "y")
        self.assertRaises(TypeError, model.add_exactly_one, [y, False])

    def testBoolAnd(self):
        print("testBoolAnd")
        model = cp_model.CpModel()
        x = [model.new_bool_var("x%i" % i) for i in range(5)]
        model.add_bool_and(x)
        self.assertLen(model.proto.variables, 5)
        self.assertLen(model.proto.constraints, 1)
        self.assertLen(model.proto.constraints[0].bool_and.literals, 5)
        model.add_bool_and([x[1], x[2].negated(), True])
        self.assertEqual(1, model.proto.constraints[1].bool_and.literals[0])
        self.assertEqual(-3, model.proto.constraints[1].bool_and.literals[1])
        self.assertEqual(5, model.proto.constraints[1].bool_and.literals[2])

    def testBoolXOr(self):
        print("testBoolXOr")
        model = cp_model.CpModel()
        x = [model.new_bool_var("x%i" % i) for i in range(5)]
        model.add_bool_xor(x)
        self.assertLen(model.proto.variables, 5)
        self.assertLen(model.proto.constraints, 1)
        self.assertLen(model.proto.constraints[0].bool_xor.literals, 5)

    def testMapDomain(self):
        print("testMapDomain")
        model = cp_model.CpModel()
        x = [model.new_bool_var("x%i" % i) for i in range(5)]
        y = model.new_int_var(0, 10, "y")
        model.add_map_domain(y, x, 2)
        self.assertLen(model.proto.variables, 6)
        self.assertLen(model.proto.constraints, 10)

    def testInterval(self):
        print("testInterval")
        model = cp_model.CpModel()
        x = model.new_int_var(0, 4, "x")
        y = model.new_int_var(0, 3, "y")
        i = model.new_interval_var(x, 3, y, "i")
        self.assertEqual(1, i.index)

        j = model.new_fixed_size_interval_var(x, 2, "j")
        self.assertEqual(2, j.index)
        start_expr = j.start_expr()
        size_expr = j.size_expr()
        end_expr = j.end_expr()
        self.assertEqual(x.index, start_expr.index)
        self.assertEqual(size_expr, 2)
        self.assertEqual(str(end_expr), "(x + 2)")

    def testOptionalInterval(self):
        print("testOptionalInterval")
        model = cp_model.CpModel()
        b = model.new_bool_var("b")
        x = model.new_int_var(0, 4, "x")
        y = model.new_int_var(0, 3, "y")
        i = model.new_optional_interval_var(x, 3, y, b, "i")
        j = model.new_optional_interval_var(x, y, 10, b, "j")
        k = model.new_optional_interval_var(x, -y, 10, b, "k")
        l = model.new_optional_interval_var(x, 10, -y, b, "l")
        self.assertEqual(1, i.index)
        self.assertEqual(3, j.index)
        self.assertEqual(5, k.index)
        self.assertEqual(7, l.index)
        self.assertRaises(TypeError, model.new_optional_interval_var, 1, 2, 3, x, "x")
        self.assertRaises(
            TypeError, model.new_optional_interval_var, b + x, 2, 3, b, "x"
        )
        self.assertRaises(
            AttributeError, model.new_optional_interval_var, 1, 2, 3, b + 1, "x"
        )

    def testNoOverlap(self):
        print("testNoOverlap")
        model = cp_model.CpModel()
        x = model.new_int_var(0, 4, "x")
        y = model.new_int_var(0, 3, "y")
        z = model.new_int_var(0, 3, "y")
        i = model.new_interval_var(x, 3, y, "i")
        j = model.new_interval_var(x, 5, z, "j")
        ct = model.add_no_overlap([i, j])
        self.assertEqual(4, ct.index)
        self.assertLen(ct.proto.no_overlap.intervals, 2)
        self.assertEqual(1, ct.proto.no_overlap.intervals[0])
        self.assertEqual(3, ct.proto.no_overlap.intervals[1])

    def testNoOverlap2D(self):
        print("testNoOverlap2D")
        model = cp_model.CpModel()
        x = model.new_int_var(0, 4, "x")
        y = model.new_int_var(0, 3, "y")
        z = model.new_int_var(0, 3, "y")
        i = model.new_interval_var(x, 3, y, "i")
        j = model.new_interval_var(x, 5, z, "j")
        ct = model.add_no_overlap_2d([i, j], [j, i])
        self.assertEqual(4, ct.index)
        self.assertLen(ct.proto.no_overlap_2d.x_intervals, 2)
        self.assertEqual(1, ct.proto.no_overlap_2d.x_intervals[0])
        self.assertEqual(3, ct.proto.no_overlap_2d.x_intervals[1])
        self.assertLen(ct.proto.no_overlap_2d.y_intervals, 2)
        self.assertEqual(3, ct.proto.no_overlap_2d.y_intervals[0])
        self.assertEqual(1, ct.proto.no_overlap_2d.y_intervals[1])

    def testCumulative(self):
        print("testCumulative")
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
        self.assertEqual(20, ct.index)
        self.assertLen(ct.proto.cumulative.intervals, 10)
        self.assertRaises(TypeError, model.add_cumulative, [intervals[0], 3], [2, 3], 3)

    def testGetOrMakeIndexFromConstant(self):
        print("testGetOrMakeIndexFromConstant")
        model = cp_model.CpModel()
        self.assertEqual(0, model.get_or_make_index_from_constant(3))
        self.assertEqual(0, model.get_or_make_index_from_constant(3))
        self.assertEqual(1, model.get_or_make_index_from_constant(5))
        model_var = model.proto.variables[0]
        self.assertLen(model_var.domain, 2)
        self.assertEqual(3, model_var.domain[0])
        self.assertEqual(3, model_var.domain[1])

    def testStr(self):
        print("testStr")
        model = cp_model.CpModel()
        x = model.new_int_var(0, 4, "x")
        self.assertEqual(str(x == 2), "x == 2")
        self.assertEqual(str(x >= 2), "x >= 2")
        self.assertEqual(str(x <= 2), "x <= 2")
        self.assertEqual(str(x > 2), "x >= 3")
        self.assertEqual(str(x < 2), "x <= 1")
        self.assertEqual(str(x != 2), "x != 2")
        self.assertEqual(str(x * 3), "(3 * x)")
        self.assertEqual(str(-x), "-x")
        self.assertEqual(str(x + 3), "(x + 3)")
        self.assertEqual(str(x <= cp_model.INT_MAX), "True (unbounded expr x)")
        self.assertEqual(str(x != 9223372036854775807), "x <= 9223372036854775806")
        self.assertEqual(str(x != -9223372036854775808), "x >= -9223372036854775807")
        y = model.new_int_var(0, 4, "y")
        self.assertEqual(
            str(cp_model.LinearExpr.weighted_sum([x, y + 1, 2], [1, -2, 3])),
            "x - 2 * (y + 1) + 6",
        )
        self.assertEqual(str(cp_model.LinearExpr.term(x, 3)), "(3 * x)")
        self.assertEqual(str(x != y), "(x + -y) != 0")
        self.assertEqual(
            "0 <= x <= 10", str(cp_model.BoundedLinearExpression(x, [0, 10]))
        )
        print(str(model))
        b = model.new_bool_var("b")
        self.assertEqual(str(cp_model.LinearExpr.term(b.negated(), 3)), "(3 * not(b))")

        i = model.new_interval_var(x, 2, y, "i")
        self.assertEqual(str(i), "i")

    def testRepr(self):
        print("testRepr")
        model = cp_model.CpModel()
        x = model.new_int_var(0, 4, "x")
        y = model.new_int_var(0, 3, "y")
        z = model.new_int_var(0, 3, "z")
        self.assertEqual(repr(x), "x(0..4)")
        self.assertEqual(repr(x * 2), "ProductCst(x(0..4), 2)")
        self.assertEqual(repr(x + y), "sum(x(0..4), y(0..3))")
        self.assertEqual(
            repr(cp_model.LinearExpr.sum([x, y, z])),
            "SumArray(x(0..4), y(0..3), z(0..3), 0)",
        )
        self.assertEqual(
            repr(cp_model.LinearExpr.weighted_sum([x, y, 2], [1, 2, 3])),
            "weighted_sum([x(0..4), y(0..3)], [1, 2], 6)",
        )
        i = model.new_interval_var(x, 2, y, "i")
        self.assertEqual(repr(i), "i(start = x, size = 2, end = y)")
        b = model.new_bool_var("b")
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

    def testDisplayBounds(self):
        print("testDisplayBounds")
        self.assertEqual("10..20", cp_model.display_bounds([10, 20]))
        self.assertEqual("10", cp_model.display_bounds([10, 10]))
        self.assertEqual("10..15, 20..30", cp_model.display_bounds([10, 15, 20, 30]))

    def testShortName(self):
        print("testShortName")
        model = cp_model.CpModel()
        model.proto.variables.add(domain=[5, 10])
        self.assertEqual("[5..10]", cp_model.short_name(model.proto, 0))

    def testIntegerExpressionErrors(self):
        print("testIntegerExpressionErrors")
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

    def testModelErrors(self):
        print("testModelErrors")
        model = cp_model.CpModel()
        self.assertRaises(TypeError, model.add, "dummy")
        self.assertRaises(TypeError, model.get_or_make_index, "dummy")
        self.assertRaises(TypeError, model.minimize, "dummy")

    def testSolverErrors(self):
        print("testSolverErrors")
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

    def testHasObjectiveMinimize(self):
        print("testHasObjectiveMinimizs")
        model = cp_model.CpModel()
        x = model.new_int_var(0, 1, "x")
        y = model.new_int_var(-10, 10, "y")
        model.add_linear_constraint(x + 2 * y, 0, 10)
        self.assertFalse(model.has_objective())
        model.minimize(y)
        self.assertTrue(model.has_objective())

    def testHasObjectiveMaximize(self):
        print("testHasObjectiveMaximizs")
        model = cp_model.CpModel()
        x = model.new_int_var(0, 1, "x")
        y = model.new_int_var(-10, 10, "y")
        model.add_linear_constraint(x + 2 * y, 0, 10)
        self.assertFalse(model.has_objective())
        model.maximize(y)
        self.assertTrue(model.has_objective())

    def testSearchForAllSolutions(self):
        print("testSearchForAllSolutions")
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
        model.minimize(x)

    def testSolveWithSolutionCallback(self):
        print("testSolveWithSolutionCallback")
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

    def testValue(self):
        print("testValue")
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

    def testBooleanValue(self):
        print("testBooleanValue")
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

    def testUnsupportedOperators(self):
        print("testUnsupportedOperators")
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

    def testIsLiteralTrueFalse(self):
        print("testIsLiteralTrueFalse")
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

    def testSolveMinimizeWithSolutionCallback(self):
        print("testSolveMinimizeWithSolutionCallback")
        model = cp_model.CpModel()
        x = model.new_int_var(0, 5, "x")
        y = model.new_int_var(0, 5, "y")
        model.add_linear_constraint(x + y, 6, 6)
        model.maximize(x + 2 * y)

        solver = cp_model.CpSolver()
        solution_obj = SolutionObjective()
        status = solver.solve(model, solution_obj)
        self.assertEqual(cp_model.OPTIMAL, status)
        print("obj = ", solution_obj.obj)
        self.assertEqual(11, solution_obj.obj)

    def testSolutionValue(self):
        print("testSolutionValue")
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

    def testSolutionHinting(self):
        print("testSolutionHinting")
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

    def testStats(self):
        print("testStats")
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

    def testSearchStrategy(self):
        print("testSearchStrategy")
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

    def testModelAndResponseStats(self):
        print("testStats")
        model = cp_model.CpModel()
        x = model.new_int_var(0, 5, "x")
        y = model.new_int_var(0, 5, "y")
        model.add_linear_constraint(x + y, 6, 6)
        model.maximize(x + 2 * y)
        self.assertTrue(model.model_stats())

        solver = cp_model.CpSolver()
        solver.solve(model)
        self.assertTrue(solver.response_stats())

    def testValidateModel(self):
        print("testValidateModel")
        model = cp_model.CpModel()
        x = model.new_int_var(0, 5, "x")
        y = model.new_int_var(0, 5, "y")
        model.add_linear_constraint(x + y, 6, 6)
        model.maximize(x + 2 * y)
        self.assertFalse(model.validate())

    def testValidateModelWithOverflow(self):
        print("testValidateModel")
        model = cp_model.CpModel()
        x = model.new_int_var(0, cp_model.INT_MAX, "x")
        y = model.new_int_var(0, 10, "y")
        model.add_linear_constraint(x + y, 6, cp_model.INT_MAX)
        model.maximize(x + 2 * y)
        self.assertTrue(model.validate())

    def testCopyModel(self):
        print("testCopyModel")
        model = cp_model.CpModel()
        b = model.new_bool_var("b")
        x = model.new_int_var(0, 4, "x")
        y = model.new_int_var(0, 3, "y")
        i = model.new_optional_interval_var(x, 12, y, b, "i")
        lin = model.add(x + y <= 10)

        new_model = model.clone()
        copy_b = new_model.get_bool_var_from_proto_index(b.index)
        copy_x = new_model.get_int_var_from_proto_index(x.index)
        copy_y = new_model.get_int_var_from_proto_index(y.index)
        copy_i = new_model.get_interval_var_from_proto_index(i.index)

        self.assertEqual(b.index, copy_b.index)
        self.assertEqual(x.index, copy_x.index)
        self.assertEqual(y.index, copy_y.index)
        self.assertEqual(i.index, copy_i.index)

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

    def testCustomLog(self):
        print("testCustomLog")
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

    def testIssue2762(self):
        print("testIssue2762")
        model = cp_model.CpModel()

        x = [model.new_bool_var("a"), model.new_bool_var("b")]
        with self.assertRaises(NotImplementedError):
            model.add((x[0] != 0) or (x[1] != 0))

    def testModelError(self):
        print("TestModelError")
        model = cp_model.CpModel()
        x = [model.new_int_var(0, -2, "x%i" % i) for i in range(100)]
        model.add(sum(x) <= 1)
        solver = cp_model.CpSolver()
        solver.parameters.log_search_progress = True
        self.assertEqual(cp_model.MODEL_INVALID, solver.solve(model))
        self.assertEqual(solver.solution_info(), 'var #0 has no domain(): name: "x0"')

    def testIntVarSeries(self):
        print("testIntVarSeries")
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

    def testBoolVarSeries(self):
        print("testBoolVarSeries")
        df = pd.DataFrame([1, -1, 1], columns=["coeffs"])
        model = cp_model.CpModel()
        x = model.new_bool_var_series(name="x", index=df.index)
        model.minimize(df.coeffs.dot(x))
        solver = cp_model.CpSolver()
        self.assertEqual(cp_model.OPTIMAL, solver.solve(model))
        solution = solver.boolean_values(x)
        self.assertTrue((solution.values == [False, True, False]).all())

    def testFixedSizeIntervalVarSeries(self):
        print("testFixedSizeIntervalVarSeries")
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

    def testIntervalVarSeries(self):
        print("testIntervalVarSeries")
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
        self.assertLen(model.proto.constraints, 19)


if __name__ == "__main__":
    absltest.main()
